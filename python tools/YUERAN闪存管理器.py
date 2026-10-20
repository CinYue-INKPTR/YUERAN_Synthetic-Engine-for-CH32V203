# -*- coding: utf-8 -*-
"""YUERAN 闪存管理器（W25QXX）

通过 230400 串口（8N1，普通模式）管理设备上的 W25QXX 闪存。

设备端协议（与固件 YUERAN 工程对齐）：
    串口数据包：0xAB + 包长度(1) + 指令数(1) + 指令数据
        包长度 = 3 + 指令数据长度（包含 3 字节包头），指令数固定为 1
    指令数据：
    0F 0F 0F                                    退出闪存模式
    "INFO"                                      返回 2 字节扇区数（高字节在前）
    'R' + 字节地址(3) + 长度(2) + 校验(1)        返回指定长度数据（不回完成标记）
    'W' + 字节地址(3) + 长度(1) + 校验(1) + 数据 写入数据（<=128 字节）
    'D' + 扇区地址(2) + 扇区数(2) + 校验(1)     擦除扇区
    "CCC"                                       擦除整颗闪存
单片机校验通过后先回发包头 4 字节（0xAB、包长度、指令数、指令首字节），
执行完成后返回 3 字节完成标记 0xAB 0xCD 0xEF（R 指令不回完成标记）。
所有地址与数值均高位在前；R 的校验为前 6 个字节之和的低 8 位，
W 和 D 的校验为前 5 个字节之和的低 8 位。

索引表：
    位于扇区 0、1（共 8192 字节），每 16 字节一份。
    表头：0x00 + 15 个 0xFF；文件索引：名称(10) + 扇区数(2) + 起始扇区(2)
    + X 尺寸(1) + Y 占页数(1；bit7 为旧版字库驱动标志，仅用于兼容显示)。
    寻表规则与固件 W25Qxx_GetIndex 一致：16 字节步长扫描，记录最大的 0x00
    地址；扇区 0 内遇到 0xFF 直接跳到扇区 1，扇区 1 内遇到 0xFF 结束扫描。
    表的更新方式为“在旧表后面抄写新表”，当新表头与旧表头不在同一扇区时，
    新表写完后擦除旧表所在扇区，因此地址最大的表头就是有效表头。
"""

import math
import os
import queue
import sys
import time
import traceback
from dataclasses import dataclass

import serial
from serial.tools import list_ports

from PyQt5 import QtCore, QtGui, QtWidgets

# ============================== 常量 ==============================
BAUD = 230400
PACKET_HEAD = 0xAB                                  # 数据包识别头
PACKET_CMD_COUNT = 1                                # 每个数据包固定 1 条指令
ECHO_LEN = 4                                        # 单片机确认回显：包头 3 字节 + 指令首字节

SECTOR_SIZE = 4096
PAGE_SIZE = 256
INDEX_SECTOR_COUNT = 2
INDEX_TOTAL = INDEX_SECTOR_COUNT * SECTOR_SIZE      # 8192
SLOT = 16                                           # 索引单位长度
DATA_FIRST_SECTOR = INDEX_SECTOR_COUNT              # 数据区从扇区 2 开始
MAX_FILES = 64
READ_CHUNK = 128                                    # 单条 R 指令最多 128 字节
WRITE_CHUNK = 128                                   # 单条 W 指令最大数据长度 128 字节

DONE_MARK = b"\xab\xcd\xef"                          # 固件 BkSt_DONE
HEADER_UNIT = b"\x00" + b"\xff" * (SLOT - 1)

NAME_MIN, NAME_MAX = 1, 10
X_MIN, X_MAX = 1, 128
Y_MIN, Y_MAX = 1, 8
DRIVER_FLAG = 0x80

# 进度估算用的“等效字节”权重（读 1 字节记 1，写 1 字节约 1.45）
COST_WRITE = 1.45
CHIP_ERASE_EST_SEC = 45.0

COLOR_OK = "#1b5e20"
COLOR_ERR = "#b71c1c"
COLOR_DRIVER = "#1565c0"


# ============================== 工具函数 ==============================
def fmt_size(n):
    if n >= 1024 * 1024:
        return "%.2f MB" % (n / 1048576.0)
    if n >= 1024:
        return "%.1f KB" % (n / 1024.0)
    return "%d B" % n


def fmt_addr(a):
    return "0x%06X" % a


def fmt_duration(sec):
    if sec is None or sec < 0:
        return "--"
    sec = int(round(sec))
    if sec < 60:
        return "%d 秒" % sec
    m, s = divmod(sec, 60)
    if m < 60:
        return "%d 分 %d 秒" % (m, s)
    h, m = divmod(m, 60)
    return "%d 小时 %d 分" % (h, m)


def fmt_hex(data, limit=16):
    text = " ".join("%02X" % b for b in data[:limit])
    if len(data) > limit:
        text += " ..."
    return text


# ============================== 异常 ==============================
class ProtocolError(Exception):
    pass


# ============================== 串口协议层 ==============================
class SerialLink(object):
    """设备串口通讯（0xAB 数据包 + 4 字节确认回显 + AB CD EF 完成标记）"""

    def __init__(self):
        self.ser = None

    @property
    def is_open(self):
        return self.ser is not None and self.ser.is_open

    def open(self, port):
        self.close()
        self.ser = serial.Serial(
            port=port, baudrate=BAUD, bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE,
            timeout=0.05, write_timeout=10)
        time.sleep(0.05)
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()

    def close(self):
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None

    # ---------- 底层收发 ----------
    @staticmethod
    def _packet(cmd):
        """0xAB + 包长度(1) + 指令数(1) + 指令数据（包长度含 3 字节包头）"""
        cmd = bytes(cmd)
        total = 3 + len(cmd)
        if total > 0xFF:
            raise ProtocolError("数据包过长：%d 字节（上限 255）" % total)
        return bytes([PACKET_HEAD, total, PACKET_CMD_COUNT]) + cmd

    def _send(self, packet):
        if not self.is_open:
            raise ProtocolError("串口未打开")
        self.ser.reset_input_buffer()
        self.ser.write(packet)
        self.ser.flush()

    def _read_exact(self, n, timeout):
        """读取 n 字节；超时返回已收到的部分（不抛异常）"""
        end = time.monotonic() + timeout
        buf = bytearray()
        while len(buf) < n:
            if time.monotonic() >= end:
                break
            chunk = self.ser.read(n - len(buf))
            if chunk:
                buf.extend(chunk)
        return bytes(buf)

    def _read_echo(self, packet, timeout, what):
        """读取单片机回发的 4 字节确认（包头 3 字节 + 指令首字节）"""
        expected = packet[:ECHO_LEN]
        head = self._read_exact(ECHO_LEN, timeout)
        if not head:
            raise ProtocolError(
                "%s：未收到单片机的指令确认回显（期望 %s，实际 0 字节）。"
                "请检查设备是否处于闪存模式、串口是否接在 USART2、波特率是否为 %d"
                % (what, fmt_hex(expected), BAUD))
        if len(head) < ECHO_LEN:
            raise ProtocolError(
                "%s：指令确认回显不完整（期望 %d 字节 %s，实际 %d 字节 %s）"
                % (what, ECHO_LEN, fmt_hex(expected), len(head), fmt_hex(head)))
        if head != expected:
            raise ProtocolError(
                "%s：指令确认回显不匹配（期望 %s，实际 %s）。"
                "可能波特率不匹配、线路受干扰，或指令校验未通过"
                % (what, fmt_hex(expected), fmt_hex(head)))

    def _collect_done(self, timeout, progress_cb=None, what="应答"):
        """已收到确认回显后，继续读到完成标记 AB CD EF，返回标记前的数据"""
        buf = bytearray()
        end = time.monotonic() + timeout
        while True:
            idx = buf.find(DONE_MARK)
            if idx >= 0:
                return bytes(buf[:idx])
            if time.monotonic() >= end:
                raise ProtocolError(
                    "%s：已收到指令确认回显，但等待执行完成标记 AB CD EF 超时"
                    "（%.1f 秒，期间收到 %d 字节：%s）"
                    % (what, timeout, len(buf),
                       fmt_hex(bytes(buf), 32) if buf else "无"))
            chunk = self.ser.read(128)
            if chunk:
                buf.extend(chunk)
            elif progress_cb is not None:
                progress_cb()

    def _cmd_done(self, cmd, timeout, progress_cb=None, what="应答"):
        packet = self._packet(cmd)
        self._send(packet)
        self._read_echo(packet, min(2.0, timeout), what)
        return self._collect_done(timeout, progress_cb, what)

    # ---------- 设备指令 ----------
    def exit_flash_mode(self):
        self._cmd_done(b"\x0f\x0f\x0f", 6.0, what="退出闪存模式")

    def info(self):
        payload = self._cmd_done(b"INFO", 6.0, what="INFO")
        if len(payload) < 2:
            raise ProtocolError("INFO 应答不足 2 字节：%s" % fmt_hex(payload))
        return (payload[0] << 8) | payload[1]

    def read(self, addr, length):
        """读闪存，单条指令最多 256 字节；设备不回完成标记，按长度收满即结束"""
        if not 0 < length <= READ_CHUNK:
            raise ProtocolError("读长度非法：%d" % length)
        cmd = bytearray(b"R")
        cmd += addr.to_bytes(3, "big")
        cmd += length.to_bytes(2, "big")
        cmd.append(sum(cmd) & 0xFF)
        packet = self._packet(cmd)
        timeout = max(2.0, length * 10.0 / BAUD * 4.0 + 1.0)

        self._send(packet)
        self._read_echo(packet, min(2.0, timeout), "读取 %s" % fmt_addr(addr))
        data = self._read_exact(length, timeout)
        if len(data) < length:
            raise ProtocolError(
                "读取 %s：已收到指令确认回显，但数据接收超时（期望 %d 字节，实际 %d 字节）"
                % (fmt_addr(addr), length, len(data)))
        return data[:length]

    def write_page(self, addr, data, timeout=30.0):
        """写一包（<=128 字节，且不跨 256 字节页）"""
        if not 0 < len(data) <= WRITE_CHUNK:
            raise ProtocolError("写长度非法：%d（单包最多 %d 字节）"
                                % (len(data), WRITE_CHUNK))
        if (addr // PAGE_SIZE) != ((addr + len(data) - 1) // PAGE_SIZE):
            raise ProtocolError("写入跨越页边界：%s 长度 %d" % (fmt_addr(addr), len(data)))
        cmd = bytearray(b"W")
        cmd += addr.to_bytes(3, "big")
        cmd += len(data).to_bytes(1, "big")
        cmd.append(sum(cmd) & 0xFF)
        packet = self._packet(bytes(cmd) + bytes(data))
        self._send(packet)
        self._read_echo(packet, min(2.0, timeout), "写入 %s" % fmt_addr(addr))
        self._collect_done(timeout, what="写入 %s" % fmt_addr(addr))

    def erase_sectors(self, sector, count, progress_cb=None):
        """擦除扇区：'D' + 扇区地址(2) + 扇区数(2) + 前 5 字节校验和(1)

        单片机会先回发 4 字节包头确认，擦除完成后再返回 AB CD EF。
        """
        if count <= 0:
            return
        cmd = bytearray(b"D")
        cmd += int(sector).to_bytes(2, "big")
        cmd += int(count).to_bytes(2, "big")
        cmd.append(sum(cmd) & 0xFF)
        timeout = 15.0 + count * 0.7
        self._cmd_done(bytes(cmd), timeout, progress_cb,
                       what="擦除扇区 %d~%d" % (sector, sector + count - 1))

    def erase_chip(self, progress_cb=None):
        packet = self._packet(b"CCC")
        self._send(packet)
        self._read_echo(packet, 2.0, "擦除整颗闪存")
        self._collect_done(420.0, progress_cb, what="擦除整颗闪存")


# ============================== 索引表 ==============================
@dataclass
class FlashFile(object):
    name: str
    sectors: int
    start_sector: int
    xsize: int
    ysize: int

    @property
    def byte_addr(self):
        return self.start_sector * SECTOR_SIZE

    @property
    def byte_size(self):
        return self.sectors * SECTOR_SIZE

    @property
    def end_sector(self):
        return self.start_sector + self.sectors - 1

    @property
    def is_driver(self):
        return bool(self.ysize & DRIVER_FLAG)

    @property
    def y_pages(self):
        return self.ysize & ~DRIVER_FLAG

    @property
    def kind(self):
        return "驱动" if self.is_driver else "素材"

    def to_unit(self):
        raw = self.name.encode("ascii", "replace")[:NAME_MAX]
        raw = raw + b"\x00" * (NAME_MAX - len(raw))
        return (raw
                + (self.sectors & 0xFFFF).to_bytes(2, "big")
                + (self.start_sector & 0xFFFF).to_bytes(2, "big")
                + bytes([self.xsize & 0xFF, self.ysize & 0xFF]))

    @classmethod
    def from_unit(cls, unit):
        raw = unit[:NAME_MAX]
        end = 0
        for i, b in enumerate(raw):
            if 0x20 <= b <= 0x7E:
                end = i + 1
            else:
                break
        return cls(name=raw[:end].decode("ascii", "replace"),
                   sectors=int.from_bytes(unit[10:12], "big"),
                   start_sector=int.from_bytes(unit[12:14], "big"),
                   xsize=unit[14],
                   ysize=unit[15])


def parse_index(raw):
    """按寻表规则解析索引表，返回 (表头地址 or None, 文件列表)

    与固件 W25Qxx_GetIndex 的扫描方式一致：以 16 字节为步长扫描，记录
    读到 0x00 的最大地址作为有效表头；扇区 0 内遇到 0xFF 空段直接跳到
    扇区 1 继续找，扇区 1 内遇到 0xFF 空段结束扫描。
    从表头后一个单位开始读文件索引，最多 64 个，遇到 0xFF 结束，越过
    8192 时回绕到 0 继续读。
    """
    if len(raw) < INDEX_TOTAL:
        raise ProtocolError("索引区数据长度不足：%d" % len(raw))

    best = None
    addr = 0
    while addr < INDEX_TOTAL:
        b = raw[addr]
        if b == 0x00 and (best is None or addr > best):
            best = addr
        if b == 0xFF:
            if addr < SECTOR_SIZE:
                addr = SECTOR_SIZE          # 扇区 0 的空段：跳到扇区 1
                continue
            break                           # 扇区 1 的空段：结束扫描
        addr += SLOT

    if best is None:
        return None, []

    files = []
    pos = (best + SLOT) % INDEX_TOTAL
    while len(files) < MAX_FILES:
        if raw[pos] == 0xFF:
            break
        files.append(FlashFile.from_unit(raw[pos:pos + SLOT]))
        pos = (pos + SLOT) % INDEX_TOTAL
    return best, files


def find_free_sector(files, need, sector_total):
    """从扇区 2 起找第一个足够大的连续空洞"""
    if need <= 0:
        return DATA_FIRST_SECTOR
    occupied = sorted((f.start_sector, f.sectors) for f in files
                      if f.sectors > 0 and f.start_sector >= DATA_FIRST_SECTOR)
    cursor = DATA_FIRST_SECTOR
    for start, count in occupied:
        if start >= cursor and start - cursor >= need:
            return cursor
        cursor = max(cursor, start + count)
    if sector_total - cursor >= need:
        return cursor
    return None


def find_move_sector(files, target_file):
    """在目标文件之前找一块能容纳它的连续空洞（向前移动）"""
    need = target_file.sectors
    occupied = sorted((f.start_sector, f.sectors) for f in files
                      if f.sectors > 0 and f.start_sector >= DATA_FIRST_SECTOR)
    cursor = DATA_FIRST_SECTOR
    for start, count in occupied:
        if start >= target_file.start_sector:
            break
        if start - cursor >= need:
            return cursor
        cursor = max(cursor, start + count)
    if target_file.start_sector - cursor >= need:
        return cursor
    return None


def region_all_ff(buf, pos, length):
    for i in range(length):
        if buf[(pos + i) % INDEX_TOTAL] != 0xFF:
            return False
    return True


# ============================== 进度 ==============================
class Progress(object):
    """按阶段加权的进度：成本单位是“等效字节”，ETA 按实测速度推算"""

    def __init__(self, emit, stages):
        self.emit = emit
        self.stages = [[name, max(1e-6, float(cost))] for name, cost in stages]
        self.done = [0.0] * len(self.stages)
        self.cur = 0
        self.t0 = time.monotonic()
        self.last = 0.0
        self._update(force=True)

    @property
    def total(self):
        return sum(c for _, c in self.stages)

    @property
    def used(self):
        return sum(self.done)

    def set_stage_cost(self, i, cost):
        self.stages[i][1] = max(1e-6, float(cost))

    def enter(self, i):
        self.cur = i
        self._update(force=True)

    def add(self, i, amount):
        self.done[i] = min(self.stages[i][1], self.done[i] + amount)
        self._update()

    def frac(self, i, frac):
        self.done[i] = self.stages[i][1] * max(0.0, min(1.0, frac))
        self._update()

    def finish(self):
        for i in range(len(self.done)):
            self.done[i] = self.stages[i][1]
        self._update(force=True)

    def _update(self, force=False):
        now = time.monotonic()
        if not force and now - self.last < 0.1:
            return
        self.last = now
        used = self.used
        total = max(1e-9, self.total)
        pct = 100 if used >= total else int(used * 100.0 / total)
        if used >= total:
            eta = 0.0
        elif used > total * 0.005:
            eta = (now - self.t0) * (total - used) / used
        else:
            eta = -1.0
        self.emit(self.stages[self.cur][0], pct, eta)


class TaskContext(object):
    def __init__(self, log_signal, progress_signal):
        self._log = log_signal
        self._progress = progress_signal
        self.progress = None

    def log(self, text):
        self._log.emit("[%s] %s" % (time.strftime("%H:%M:%S"), text))

    def begin(self, stages):
        self.progress = Progress(self._progress.emit, stages)
        return self.progress


# ============================== 闪存操作层 ==============================
class FlashTool(object):
    def __init__(self, link):
        self.link = link
        self.sector_total = 0
        self.raw = b""
        self.header = None
        self.files = []

    # ---------- 基础读写 ----------
    def _require_open(self):
        if not self.link.is_open:
            raise ProtocolError("串口未打开")

    def _require_ready(self):
        self._require_open()
        if self.sector_total <= 0:
            raise ProtocolError("请先点击「刷新闪存信息」获取闪存状态")

    def _read_bytes(self, p, stage, addr, length):
        out = bytearray()
        done = 0
        while done < length:
            n = min(READ_CHUNK, length - done)
            out += self.link.read(addr + done, n)
            done += n
            p.add(stage, n)
        return bytes(out)

    def _write_bytes(self, p, stage, addr, data):
        off = 0
        while off < len(data):
            room = PAGE_SIZE - ((addr + off) % PAGE_SIZE)
            n = min(len(data) - off, room, WRITE_CHUNK)
            self.link.write_page(addr + off, data[off:off + n])
            off += n
            p.add(stage, n * COST_WRITE)

    def _write_ring(self, p, stage, addr, data):
        """向索引区写数据，超出 8192 的部分回绕到 0 继续"""
        first = min(len(data), INDEX_TOTAL - addr)
        self._write_bytes(p, stage, addr, data[:first])
        if first < len(data):
            self._write_bytes(p, stage, 0, data[first:])

    def _erase_sectors(self, ctx, p, stage, sector, count):
        ctx.log("擦除扇区 %d ~ %d（共 %d 个扇区）" % (sector, sector + count - 1, count))
        est = max(0.4, 0.35 * count)
        t0 = time.monotonic()

        def cb():
            p.frac(stage, min(0.98, (time.monotonic() - t0) / est))

        self.link.erase_sectors(sector, count, cb)
        p.frac(stage, 1.0)

    def _erase_chip(self, ctx, p, stage):
        ctx.log("发送 CCC 擦除整颗闪存，等待设备完成（大容量芯片可能需要较长时间）")
        t0 = time.monotonic()

        def cb():
            p.frac(stage, min(0.97, (time.monotonic() - t0) / CHIP_ERASE_EST_SEC))

        self.link.erase_chip(cb)
        p.frac(stage, 1.0)

    # ---------- 索引表 ----------
    @property
    def table_end(self):
        if self.header is None:
            return 0
        return (self.header + SLOT * (len(self.files) + 1)) % INDEX_TOTAL

    def _load_index(self, ctx, p, stage):
        """每次用到索引表都重新寻表，不使用历史值"""
        ctx.log("按规则重新寻表：读取索引区 0 ~ %d（%d 字节）"
                % (INDEX_TOTAL - 1, INDEX_TOTAL))
        self.raw = self._read_bytes(p, stage, 0, INDEX_TOTAL)
        self.header, self.files = parse_index(self.raw)

        if self.header is None:
            ctx.log("未找到有效索引表表头（空表）。如需使用请先「格式化芯片」")
        else:
            ctx.log("有效表头：%s（扇区 %d），文件索引 %d 个，表长 %d 字节"
                    % (fmt_addr(self.header), self.header // SECTOR_SIZE,
                       len(self.files), SLOT * (len(self.files) + 1)))
        for i, f in enumerate(self.files):
            if (f.sectors <= 0 or f.start_sector < DATA_FIRST_SECTOR
                    or f.start_sector + f.sectors > self.sector_total):
                ctx.log("警告：第 %d 项「%s」索引越界（起始扇区 %d，占用 %d），"
                        "可能是闪存中的残留数据" % (i, f.name, f.start_sector, f.sectors))
        return self.files

    def _file_at(self, row, name):
        if row < 0 or row >= len(self.files):
            raise ProtocolError("选中的文件已不存在（索引表可能已变化），请刷新后重试")
        f = self.files[row]
        if f.sectors <= 0:
            raise ProtocolError("第 %d 项「%s」的索引数据异常（占用扇区数为 0）"
                                % (row, f.name))
        if name is not None and f.name != name:
            raise ProtocolError("索引表已变化：第 %d 项现在是「%s」，请刷新后重试"
                                % (row, f.name))
        return f

    def _rewrite_table(self, ctx, p, write_stage, erase_stage, new_files):
        """把修改后的整张表抄写到旧表后面；换扇区时擦除旧表所在扇区"""
        if self.header is None:
            new_pos = 0
        else:
            new_pos = self.table_end

        table = HEADER_UNIT + b"".join(f.to_unit() for f in new_files)

        if self.header is None and not region_all_ff(self.raw, 0, len(table)):
            raise ProtocolError("索引区没有表头且扇区 0 不是空白，建议先「格式化芯片」")

        # 固件按 16 字节步长寻表：表头所在扇区里，表头之前不能出现 0xFF 空段；
        # 同时整张表必须写到已擦除的区域。出现这两种情况时把整张表搬到该扇区
        # 起始位置，并先擦除该扇区。
        force_sec = None
        if new_pos % SECTOR_SIZE:
            sec = new_pos // SECTOR_SIZE
            base = sec * SECTOR_SIZE
            ahead_blank = any(self.raw[(base + off) % INDEX_TOTAL] == 0xFF
                              for off in range(0, new_pos % SECTOR_SIZE, SLOT))
            target_dirty = any(self.raw[(new_pos + i) % INDEX_TOTAL] != 0xFF
                               for i in range(len(table)))
            if ahead_blank or target_dirty:
                ctx.log("表头前有空白单位或目标区域非空白，整张表改到扇区 %d 起始地址写入" % sec)
                new_pos = base
                force_sec = sec

        # 需要先擦除的扇区（目标表覆盖范围内出现非 0xFF 字节）
        erase_secs = set()
        for i in range(len(table)):
            a = (new_pos + i) % INDEX_TOTAL
            sec = a // SECTOR_SIZE
            if force_sec is not None and sec == force_sec:
                continue
            if self.raw[a] != 0xFF:
                erase_secs.add(sec)
        if force_sec is not None:
            erase_secs.add(force_sec)
        for sec in sorted(erase_secs):
            ctx.log("写入新表前擦除索引扇区 %d" % sec)
            self._erase_sectors(ctx, p, erase_stage, sec, 1)

        new_sec = new_pos // SECTOR_SIZE
        ctx.log("重写索引表：%d 个文件，%d 字节，新表头 %s（扇区 %d）"
                % (len(new_files), len(table), fmt_addr(new_pos), new_sec))

        p.set_stage_cost(write_stage, len(table) * COST_WRITE)
        p.enter(write_stage)
        self._write_ring(p, write_stage, new_pos, table)

        # 新表完全落在某一个扇区时，另一个扇区里的旧表就可以擦掉了
        end = new_pos + len(table) - 1
        if end < INDEX_TOTAL and new_pos // SECTOR_SIZE == end // SECTOR_SIZE:
            other = 1 - new_sec
            if (other not in erase_secs
                    and not region_all_ff(self.raw, other * SECTOR_SIZE, SECTOR_SIZE)):
                ctx.log("新表已完全落在扇区 %d，擦除旧索引扇区 %d" % (new_sec, other))
                self._erase_sectors(ctx, p, erase_stage, other, 1)
        return new_pos

    def _append_entry(self, ctx, p, stage, erase_stage, f):
        """新增文件索引：直接追加到有效表末尾，不重写表头"""
        if self.header is None:
            self._rewrite_table(ctx, p, stage, erase_stage, [f])
            return
        if len(self.files) >= MAX_FILES:
            raise ProtocolError("索引表已满（最多 %d 个文件）" % MAX_FILES)
        pos = self.table_end
        if not region_all_ff(self.raw, pos, SLOT):
            ctx.log("索引表尾部空间非空白，改为整表重写")
            self._rewrite_table(ctx, p, stage, erase_stage, self.files + [f])
            return
        ctx.log("追加文件索引「%s」到 %s（不新建表头）" % (f.name, fmt_addr(pos)))
        p.set_stage_cost(stage, SLOT * COST_WRITE)
        p.enter(stage)
        self._write_ring(p, stage, pos, f.to_unit())

    # ---------- 数据区 ----------
    def _write_file_data(self, ctx, p, stage, start_sector, data):
        total = len(data)
        ctx.log("写入数据区：扇区 %d ~ %d（%d 字节，每包最多 %d 字节）"
                % (start_sector,
                   start_sector + max(0, int(math.ceil(total / float(SECTOR_SIZE))) - 1),
                   total, WRITE_CHUNK))
        base = start_sector * SECTOR_SIZE
        off = 0
        chunks = int(math.ceil(total / float(WRITE_CHUNK))) if total else 0
        while off < total:
            n = min(WRITE_CHUNK, total - off)
            self.link.write_page(base + off, data[off:off + n])
            off += n
            p.add(stage, n * COST_WRITE)
            i = off // WRITE_CHUNK if off < total else chunks
            if chunks <= 16 or i % 32 == 0 or off >= total:
                ctx.log("  写入进度：%d / %d 包（%s / %s）"
                        % (i, chunks, fmt_size(min(off, total)), fmt_size(total)))

    def _copy_file_data(self, ctx, p, stage, f, dst_sector):
        total = f.byte_size
        src = f.byte_addr
        dst = dst_sector * SECTOR_SIZE
        ctx.log("复制数据：%s -> %s（%d 字节）" % (fmt_addr(src), fmt_addr(dst), total))
        off = 0
        chunks = int(math.ceil(total / float(READ_CHUNK))) if total else 0
        while off < total:
            n = min(READ_CHUNK, total - off)
            chunk = self.link.read(src + off, n)
            for sub in range(0, n, WRITE_CHUNK):
                self.link.write_page(dst + off + sub, chunk[sub:sub + WRITE_CHUNK])
            off += n
            p.add(stage, n * (1.0 + COST_WRITE))
            i = off // READ_CHUNK if off < total else chunks
            if chunks <= 16 or i % 32 == 0 or off >= total:
                ctx.log("  复制进度：%s / %s" % (fmt_size(min(off, total)), fmt_size(total)))

    def read_sector(self, sector):
        """读取一个扇区（供扇区查看使用）"""
        self._require_open()
        if sector < 0:
            raise ProtocolError("扇区号非法")
        if self.sector_total > 0 and sector >= self.sector_total:
            raise ProtocolError("扇区号超出范围（共 %d 个扇区）" % self.sector_total)
        out = bytearray()
        off = 0
        while off < SECTOR_SIZE:
            n = min(READ_CHUNK, SECTOR_SIZE - off)
            out += self.link.read(sector * SECTOR_SIZE + off, n)
            off += n
        return bytes(out)

    # ============================== 操作 ==============================
    def refresh(self, ctx):
        p = ctx.begin([("读取闪存信息", 1.0), ("读取索引区", float(INDEX_TOTAL))])
        p.enter(0)
        total = self.link.info()
        p.frac(0, 1.0)
        if total <= 0:
            raise ProtocolError("设备返回的扇区数为 0，无法识别闪存")
        self.sector_total = total
        ctx.log("闪存扇区数：%d，容量：%s" % (total, fmt_size(total * SECTOR_SIZE)))

        p.enter(1)
        self._load_index(ctx, p, 1)
        p.finish()
        return ("闪存信息已刷新：%d 个扇区（%s），文件 %d 个"
                % (total, fmt_size(total * SECTOR_SIZE), len(self.files)))

    def add_file(self, ctx, name, xsize, ysize, path):
        self._require_ready()
        if not os.path.isfile(path):
            raise ProtocolError("找不到文件：%s" % path)
        with open(path, "rb") as fp:
            data = fp.read()
        if not data:
            raise ProtocolError("文件内容为空：%s" % path)

        sectors = max(1, int(math.ceil(len(data) / float(SECTOR_SIZE))))
        ctx.log("添加文件：名称「%s」，大小 %s，需要 %d 个扇区"
                % (name, fmt_size(len(data)), sectors))

        p = ctx.begin([("重新寻表", float(INDEX_TOTAL)),
                       ("写入文件索引", SLOT * COST_WRITE),
                       ("擦除旧索引扇区", 1.0),
                       ("写入文件数据", len(data) * COST_WRITE),
                       ("刷新文件表", float(INDEX_TOTAL))])

        p.enter(0)
        self._load_index(ctx, p, 0)
        if len(self.files) >= MAX_FILES:
            raise ProtocolError("索引表已满（最多 %d 个文件）" % MAX_FILES)
        start = find_free_sector(self.files, sectors, self.sector_total)
        if start is None:
            raise ProtocolError("空间不足：需要 %d 个连续扇区，数据区没有足够大的空洞"
                                % sectors)
        if start + sectors > self.sector_total:
            raise ProtocolError("空间不足：文件超出闪存范围")

        f = FlashFile(name, sectors, start, xsize, ysize)
        ctx.log("分配数据区：扇区 %d ~ %d（字节地址 %s）"
                % (start, start + sectors - 1, fmt_addr(start * SECTOR_SIZE)))

        self._append_entry(ctx, p, 1, 2, f)
        p.enter(3)
        self._write_file_data(ctx, p, 3, start, data)
        p.enter(4)
        self._load_index(ctx, p, 4)
        p.finish()
        return "文件「%s」已添加：扇区 %d ~ %d，%s" % (name, start, start + sectors - 1,
                                                    fmt_size(len(data)))

    def modify_file(self, ctx, row, old_name, name, xsize, y_pages):
        self._require_ready()
        p = ctx.begin([("重新寻表", float(INDEX_TOTAL)),
                       ("写入新索引表", 1040 * COST_WRITE),
                       ("擦除旧索引扇区", 1.0),
                       ("刷新文件表", float(INDEX_TOTAL))])

        p.enter(0)
        self._load_index(ctx, p, 0)
        f = self._file_at(row, old_name)
        ysize = y_pages | (DRIVER_FLAG if f.is_driver else 0)
        new_f = FlashFile(name, f.sectors, f.start_sector, xsize, ysize)
        ctx.log("修改文件信息：第 %d 项「%s」 -> 名称「%s」，X=%d，Y=%d%s"
                % (row, f.name, name, xsize, y_pages, "（驱动）" if f.is_driver else ""))

        new_files = list(self.files)
        new_files[row] = new_f
        self._rewrite_table(ctx, p, 1, 2, new_files)

        p.enter(3)
        self._load_index(ctx, p, 3)
        p.finish()
        return "文件「%s」信息已更新" % name

    def delete_file(self, ctx, row, old_name):
        self._require_ready()
        p = ctx.begin([("重新寻表", float(INDEX_TOTAL)),
                       ("写入新索引表", 1040 * COST_WRITE),
                       ("擦除旧索引扇区", 1.0),
                       ("擦除文件数据区", 1.0),
                       ("刷新文件表", float(INDEX_TOTAL))])

        p.enter(0)
        self._load_index(ctx, p, 0)
        f = self._file_at(row, old_name)

        # 先改索引表：从索引中移除该项，并把整张新表写到旧表后面
        new_files = [x for i, x in enumerate(self.files) if i != row]
        self._rewrite_table(ctx, p, 1, 2, new_files)

        # 索引写完后才操作数据区
        p.enter(3)
        ctx.log("擦除文件数据区：「%s」（扇区 %d ~ %d）"
                % (f.name, f.start_sector, f.end_sector))
        self._erase_sectors(ctx, p, 3, f.start_sector, f.sectors)

        p.enter(4)
        self._load_index(ctx, p, 4)
        p.finish()
        return "文件「%s」已删除" % f.name

    def move_file(self, ctx, row, old_name):
        self._require_ready()
        p = ctx.begin([("重新寻表", float(INDEX_TOTAL)),
                       ("写入新索引表", 1040 * COST_WRITE),
                       ("擦除旧索引扇区", 1.0),
                       ("复制文件数据", 1.0),
                       ("擦除原数据区", 1.0),
                       ("刷新文件表", float(INDEX_TOTAL))])

        p.enter(0)
        self._load_index(ctx, p, 0)
        f = self._file_at(row, old_name)

        target = find_move_sector(self.files, f)
        if target is None:
            raise ProtocolError("文件「%s」前面没有足够的连续空闲扇区，无法向前移动"
                                % f.name)
        if target >= f.start_sector:
            raise ProtocolError("文件已经在最前面，无需移动")

        # 先改索引表：把起始扇区改成目标位置并写入新表
        new_files = list(self.files)
        new_files[row] = FlashFile(f.name, f.sectors, target, f.xsize, f.ysize)
        self._rewrite_table(ctx, p, 1, 2, new_files)

        # 索引写完后才复制/擦除数据区
        p.set_stage_cost(3, f.byte_size * (1.0 + COST_WRITE))
        p.enter(3)
        ctx.log("移动文件：「%s」从扇区 %d 前移到扇区 %d（%d 个扇区）"
                % (f.name, f.start_sector, target, f.sectors))
        self._copy_file_data(ctx, p, 3, f, target)

        p.enter(4)
        ctx.log("移动完成，擦除原数据区扇区 %d ~ %d"
                % (f.start_sector, f.end_sector))
        self._erase_sectors(ctx, p, 4, f.start_sector, f.sectors)

        p.enter(5)
        self._load_index(ctx, p, 5)
        p.finish()
        return "文件「%s」已移动到扇区 %d" % (f.name, target)

    def export_file(self, ctx, row, old_name, path):
        self._require_ready()
        p = ctx.begin([("重新寻表", float(INDEX_TOTAL)),
                       ("读取文件数据", 1.0)])

        p.enter(0)
        self._load_index(ctx, p, 0)
        f = self._file_at(row, old_name)

        p.set_stage_cost(1, float(f.byte_size))
        p.enter(1)
        ctx.log("导出文件：「%s」，扇区 %d ~ %d，读取 %d 字节"
                % (f.name, f.start_sector, f.end_sector, f.byte_size))
        data = self._read_bytes(p, 1, f.byte_addr, f.byte_size)

        with open(path, "wb") as fp:
            fp.write(data)
        p.finish()
        ctx.log("已保存到：%s" % path)
        return "文件「%s」已导出（%s）" % (f.name, fmt_size(len(data)))

    def format_chip(self, ctx):
        self._require_open()
        p = ctx.begin([("擦除整颗闪存", 1.0),
                       ("写入索引表头", 1.0),
                       ("刷新文件表", float(INDEX_TOTAL))])

        p.enter(0)
        self._erase_chip(ctx, p, 0)

        p.enter(1)
        ctx.log("在地址 0x000000 写入表头标志字节 0x00")
        self.link.write_page(0, b"\x00")
        p.frac(1, 1.0)

        total = self.link.info()
        if total > 0:
            self.sector_total = total
            ctx.log("格式化后闪存扇区数：%d（%s）"
                    % (total, fmt_size(total * SECTOR_SIZE)))

        p.enter(2)
        self._load_index(ctx, p, 2)
        p.finish()
        return "闪存已格式化，索引表头已建立在扇区 0 的 0x000000"

    def exit_flash_mode(self, ctx):
        self._require_open()
        p = ctx.begin([("退出闪存模式", 1.0)])
        p.enter(0)
        ctx.log("发送 0F 0F 0F 退出闪存模式")
        self.link.exit_flash_mode()
        p.frac(0, 1.0)
        p.finish()
        return "设备已返回渲染模式"


# ============================== 后台任务线程 ==============================
class Worker(QtCore.QThread):
    sig_log = QtCore.pyqtSignal(str)
    sig_progress = QtCore.pyqtSignal(str, int, float)
    sig_done = QtCore.pyqtSignal(bool, str)
    sig_busy = QtCore.pyqtSignal(bool)

    def __init__(self, parent=None):
        super(Worker, self).__init__(parent)
        self._queue = queue.Queue()
        self._running = True

    def submit(self, fn):
        self._queue.put(fn)

    def stop(self):
        self._running = False
        self._queue.put(None)

    def run(self):
        while self._running:
            fn = self._queue.get()
            if fn is None:
                break
            self.sig_busy.emit(True)
            ctx = TaskContext(self.sig_log, self.sig_progress)
            try:
                message = fn(ctx)
            except Exception as e:
                self.sig_log.emit("[%s] 异常：%s"
                                  % (time.strftime("%H:%M:%S"), traceback.format_exc().rstrip()))
                self.sig_done.emit(False, str(e))
            else:
                self.sig_done.emit(True, message or "操作完成")
            self.sig_busy.emit(False)


# ============================== 对话框 ==============================
def check_name(widget, text):
    if not (NAME_MIN <= len(text) <= NAME_MAX):
        QtWidgets.QMessageBox.warning(widget, "提示",
                                      "文件名称必须是 1 ~ %d 个 ASCII 字符" % NAME_MAX)
        return False
    if any(not (0x20 <= ord(ch) <= 0x7E) for ch in text):
        QtWidgets.QMessageBox.warning(widget, "提示", "文件名称只能使用可见 ASCII 字符")
        return False
    return True


class AddFileDialog(QtWidgets.QDialog):
    def __init__(self, tool, parent=None):
        super(AddFileDialog, self).__init__(parent)
        self.tool = tool
        self.setWindowTitle("添加文件")
        self.setMinimumWidth(500)

        form = QtWidgets.QFormLayout(self)

        self.name_edit = QtWidgets.QLineEdit()
        self.name_edit.setMaxLength(NAME_MAX)
        self.name_edit.setPlaceholderText("1 ~ 10 个 ASCII 字符")
        form.addRow("文件名称：", self.name_edit)

        self.x_spin = QtWidgets.QSpinBox()
        self.x_spin.setRange(X_MIN, X_MAX)
        self.x_spin.setValue(8)
        form.addRow("单元素 X 尺寸：", self.x_spin)

        self.y_spin = QtWidgets.QSpinBox()
        self.y_spin.setRange(Y_MIN, Y_MAX)
        self.y_spin.setValue(1)
        self.y_spin.setToolTip("1 页 = 8 像素")
        form.addRow("单元素 Y 占页数：", self.y_spin)

        row = QtWidgets.QHBoxLayout()
        self.path_edit = QtWidgets.QLineEdit()
        self.path_edit.setReadOnly(True)
        btn = QtWidgets.QPushButton("浏览...")
        btn.clicked.connect(self._browse)
        row.addWidget(self.path_edit)
        row.addWidget(btn)
        form.addRow("二进制文件：", row)

        self.info = QtWidgets.QLabel("-")
        self.info.setWordWrap(True)
        form.addRow("", self.info)

        btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok |
                                          QtWidgets.QDialogButtonBox.Cancel)
        btns.button(QtWidgets.QDialogButtonBox.Ok).setText("确定")
        btns.button(QtWidgets.QDialogButtonBox.Cancel).setText("取消")
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        form.addRow(btns)

    def _browse(self):
        path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "选择要写入闪存的二进制文件", "", "二进制文件 (*.bin);;所有文件 (*)")
        if path:
            self.path_edit.setText(path)
            self._update_info()

    def _update_info(self):
        path = self.path_edit.text()
        if not path or not os.path.isfile(path):
            self.info.setText("-")
            return
        size = os.path.getsize(path)
        need = max(1, int(math.ceil(size / float(SECTOR_SIZE))))
        start = find_free_sector(self.tool.files, need, self.tool.sector_total)
        if start is None:
            self.info.setText("<font color='%s'>文件 %s，需要 %d 个扇区，"
                              "当前没有足够的连续空间</font>" % (COLOR_ERR, fmt_size(size), need))
        else:
            self.info.setText("文件 %s，需要 <b>%d</b> 个扇区，预计写入扇区 "
                              "<b>%d ~ %d</b>（实际以操作时的索引表为准）"
                              % (fmt_size(size), need, start, start + need - 1))

    def values(self):
        return (self.name_edit.text(), self.x_spin.value(),
                self.y_spin.value(), self.path_edit.text())

    def accept(self):
        if not check_name(self, self.name_edit.text()):
            return
        path = self.path_edit.text()
        if not path or not os.path.isfile(path):
            QtWidgets.QMessageBox.warning(self, "提示", "请先选择二进制文件")
            return
        if os.path.getsize(path) == 0:
            QtWidgets.QMessageBox.warning(self, "提示", "文件内容为空")
            return
        need = max(1, int(math.ceil(os.path.getsize(path) / float(SECTOR_SIZE))))
        if find_free_sector(self.tool.files, need, self.tool.sector_total) is None:
            QtWidgets.QMessageBox.warning(self, "提示", "闪存没有足够的连续空间")
            return
        super(AddFileDialog, self).accept()


class ModifyDialog(QtWidgets.QDialog):
    def __init__(self, f, parent=None):
        super(ModifyDialog, self).__init__(parent)
        self.f = f
        self.setWindowTitle("修改文件信息")
        self.setMinimumWidth(420)

        form = QtWidgets.QFormLayout(self)

        self.name_edit = QtWidgets.QLineEdit(f.name)
        self.name_edit.setMaxLength(NAME_MAX)
        form.addRow("文件名称：", self.name_edit)

        self.x_spin = QtWidgets.QSpinBox()
        self.x_spin.setRange(X_MIN, X_MAX)
        self.x_spin.setValue(max(X_MIN, min(X_MAX, f.xsize)))
        form.addRow("单元素 X 尺寸：", self.x_spin)

        self.y_spin = QtWidgets.QSpinBox()
        self.y_spin.setRange(Y_MIN, Y_MAX)
        self.y_spin.setValue(max(Y_MIN, min(Y_MAX, f.y_pages)))
        form.addRow("单元素 Y 占页数：", self.y_spin)

        form.addRow("类型：", QtWidgets.QLabel(
            "字库芯片驱动（Y 值保留 0x80 标志）" if f.is_driver else "素材文件"))
        form.addRow("占用扇区：", QtWidgets.QLabel(
            "%d ~ %d（共 %d 个）" % (f.start_sector, f.end_sector, f.sectors)))

        btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok |
                                          QtWidgets.QDialogButtonBox.Cancel)
        btns.button(QtWidgets.QDialogButtonBox.Ok).setText("确定")
        btns.button(QtWidgets.QDialogButtonBox.Cancel).setText("取消")
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        form.addRow(btns)

    def values(self):
        return self.name_edit.text(), self.x_spin.value(), self.y_spin.value()

    def accept(self):
        if not check_name(self, self.name_edit.text()):
            return
        super(ModifyDialog, self).accept()


class SectorDialog(QtWidgets.QDialog):
    """扇区查看：字节、明文、位图、简要说明（同一页面按行对应）"""

    def __init__(self, tool, parent=None):
        super(SectorDialog, self).__init__(parent)
        self.tool = tool
        self.setWindowTitle("扇区查看")
        screen = QtWidgets.QApplication.primaryScreen()
        geo = (screen.availableGeometry() if screen is not None
               else QtCore.QRect(0, 0, 1280, 800))
        w = min(geo.width(), max(720, int(geo.width() * 0.75)))
        h = min(geo.height(), max(480, int(geo.height() * 0.75)))
        self.setGeometry(geo.x() + (geo.width() - w) // 2,
                         geo.y() + (geo.height() - h) // 2, w, h)
        self.setMinimumSize(min(w, 860), min(h, 520))

        lay = QtWidgets.QVBoxLayout(self)

        bar = QtWidgets.QHBoxLayout()
        bar.addWidget(QtWidgets.QLabel("扇区号："))
        self.spin = QtWidgets.QSpinBox()
        self.spin.setRange(0, max(0, tool.sector_total - 1))
        self.spin.setMaximumWidth(110)
        bar.addWidget(self.spin)
        self.btn_read = QtWidgets.QPushButton("读取")
        bar.addWidget(self.btn_read)
        bar.addSpacing(12)
        self.lbl_addr = QtWidgets.QLabel("-")
        bar.addWidget(self.lbl_addr)
        bar.addStretch(1)
        lay.addLayout(bar)

        self.lbl_desc = QtWidgets.QLabel("点击「读取」获取扇区数据")
        self.lbl_desc.setWordWrap(True)
        self.lbl_desc.setStyleSheet("padding:6px; background:#f3f6fa; border:1px solid #d5dce6;")
        lay.addWidget(self.lbl_desc)

        legend = QtWidgets.QLabel(
            "每行：偏移 | 16 字节十六进制 | ASCII 明文 | 位图"
            "（1 个字符 = 1 字节的 8 个位，实心点 = 1），三者按行一一对应")
        legend.setStyleSheet("color:#555;")
        lay.addWidget(legend)

        self.view = QtWidgets.QPlainTextEdit()
        self.view.setReadOnly(True)
        self.view.setLineWrapMode(QtWidgets.QPlainTextEdit.NoWrap)
        self.view.setStyleSheet(
            "font-family: Consolas, 'Cascadia Mono', 'Segoe UI Symbol', monospace;"
            " font-size: 12px;")
        lay.addWidget(self.view, 1)

        btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Close)
        btns.button(QtWidgets.QDialogButtonBox.Close).setText("关闭")
        btns.rejected.connect(self.reject)
        lay.addWidget(btns)

        self.btn_read.clicked.connect(self.read_sector)
        self.spin.valueChanged.connect(self._update_addr)
        self._update_addr()

    def _update_addr(self):
        sector = self.spin.value()
        base = sector * SECTOR_SIZE
        self.lbl_addr.setText("地址 %s ~ %s" % (fmt_addr(base),
                                              fmt_addr(base + SECTOR_SIZE - 1)))

    def read_sector(self):
        sector = self.spin.value()
        if not self.tool.link.is_open:
            QtWidgets.QMessageBox.warning(self, "提示", "串口未打开")
            return
        QtWidgets.QApplication.setOverrideCursor(QtCore.Qt.WaitCursor)
        try:
            data = self.tool.read_sector(sector)
        except Exception as e:
            QtWidgets.QApplication.restoreOverrideCursor()
            QtWidgets.QMessageBox.critical(self, "读取失败", str(e))
            return
        QtWidgets.QApplication.restoreOverrideCursor()
        self._render(sector, data)

    def _render(self, sector, data):
        self.lbl_desc.setText(self._describe(sector, data))
        self.view.setPlainText(self._dump_text(data))

    @staticmethod
    def _dump_text(data):
        """同一行依次显示：偏移、16 字节十六进制、ASCII 明文、该行 16 个字节的位图

        位图用盲文字符绘制：U+2800 + 字节值，正好把一个字节的 8 个位映射到
        一个字符的 8 个点上，实心点表示 1，空心点表示 0。
        """
        lines = []
        for off in range(0, len(data), SLOT):
            row = data[off:off + SLOT]
            hexs = " ".join("%02X" % b for b in row)
            asci = "".join(chr(b) if 0x20 <= b <= 0x7E else "." for b in row)
            bits = "".join(chr(0x2800 + b) for b in row)
            lines.append("%04X  %-47s  |%s|  %s" % (off, hexs, asci, bits))
        return "\n".join(lines)

    def _describe(self, sector, data):
        tool = self.tool
        blank = all(b == 0xFF for b in data)
        if sector < INDEX_SECTOR_COUNT:
            if tool.header is None:
                head = "未找到有效表头（空表）"
            else:
                head = ("有效表头 %s（扇区 %d），文件 %d 个"
                        % (fmt_addr(tool.header), tool.header // SECTOR_SIZE,
                           len(tool.files)))
            return ("扇区 %d：文件索引区。%s。（说明基于最近一次刷新得到的索引表）"
                    % (sector, head))
        for i, f in enumerate(tool.files):
            if f.start_sector <= sector < f.start_sector + f.sectors:
                pos = sector - f.start_sector + 1
                role = ("字库芯片驱动配置" if f.is_driver
                        else "文件数据（第 %d / %d 扇区）" % (pos, f.sectors))
                return "扇区 %d：文件 #%d「%s」的%s" % (sector, i, f.name, role)
        if blank:
            return "扇区 %d：空白扇区（全 0xFF）" % sector
        return "扇区 %d：没有被索引表引用的残留数据" % sector


# ============================== 主窗口 ==============================
class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super(MainWindow, self).__init__()
        self.tool = FlashTool(SerialLink())
        self.worker = Worker(self)
        self.worker.sig_log.connect(self.append_log)
        self.worker.sig_progress.connect(self.on_progress)
        self.worker.sig_done.connect(self.on_task_done)
        self.worker.sig_busy.connect(self.on_busy)
        self.worker.start()

        self.busy = False
        self.task_t0 = 0.0

        self.setWindowTitle("YUERAN闪存管理器")
        self._apply_default_geometry()
        self._build_ui()
        self.refresh_ports()
        self.refresh_table()
        self.update_actions()
        self.append_log("YUERAN 闪存管理器已启动，请选择串口并打开")

    # ---------- 界面 ----------
    def _apply_default_geometry(self):
        """默认窗口为屏幕可用区域的 3/4，并居中显示"""
        screen = QtWidgets.QApplication.primaryScreen()
        geo = (screen.availableGeometry() if screen is not None
               else QtCore.QRect(0, 0, 1280, 800))
        w = min(geo.width(), max(640, int(geo.width() * 0.75)))
        h = min(geo.height(), max(480, int(geo.height() * 0.75)))
        self.setGeometry(geo.x() + (geo.width() - w) // 2,
                         geo.y() + (geo.height() - h) // 2, w, h)
        self.setMinimumSize(min(w, 900), min(h, 560))

    def _build_ui(self):
        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        root = QtWidgets.QVBoxLayout(central)
        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(8)

        # 串口栏
        bar = QtWidgets.QHBoxLayout()
        bar.addWidget(QtWidgets.QLabel("串口："))
        self.port_combo = QtWidgets.QComboBox()
        self.port_combo.setMinimumWidth(220)
        bar.addWidget(self.port_combo)
        self.btn_scan = QtWidgets.QPushButton("刷新串口列表")
        bar.addWidget(self.btn_scan)
        self.btn_port = QtWidgets.QPushButton("打开串口")
        bar.addWidget(self.btn_port)
        bar.addSpacing(16)
        self.lbl_conn = QtWidgets.QLabel("未连接")
        self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_ERR)
        bar.addWidget(self.lbl_conn)
        bar.addStretch(1)
        bar.addWidget(QtWidgets.QLabel("230400  8-N-1  普通模式"))
        root.addLayout(bar)

        mid = QtWidgets.QHBoxLayout()
        mid.setSpacing(8)
        root.addLayout(mid, 3)

        # 左侧控制区（宽度随窗口按比例伸缩）
        ctrl_box = QtWidgets.QGroupBox("控制区")
        ctrl_box.setMinimumWidth(190)
        ctrl_box.setMaximumWidth(340)
        ctrl = QtWidgets.QVBoxLayout(ctrl_box)
        ctrl.setSpacing(4)
        self.btn_refresh_info = self._ctrl_button(ctrl, "刷新闪存信息")
        ctrl.addSpacing(3)
        self.btn_add = self._ctrl_button(ctrl, "添加文件")
        self.btn_modify = self._ctrl_button(ctrl, "修改文件信息")
        self.btn_move = self._ctrl_button(ctrl, "移动文件")
        self.btn_export = self._ctrl_button(ctrl, "导出文件")
        self.btn_delete = self._ctrl_button(ctrl, "删除文件")
        ctrl.addSpacing(3)
        self.btn_format = self._ctrl_button(ctrl, "格式化芯片")
        self.btn_sector = self._ctrl_button(ctrl, "扇区查看")
        ctrl.addSpacing(3)
        self.btn_exit = self._ctrl_button(ctrl, "退出闪存模式")
        mid.addWidget(ctrl_box, 1)

        right = QtWidgets.QVBoxLayout()
        right.setSpacing(8)
        mid.addLayout(right, 4)

        # 文件列表
        file_box = QtWidgets.QGroupBox("文件列表")
        fl = QtWidgets.QVBoxLayout(file_box)
        self.table = QtWidgets.QTableWidget(0, 8)
        self.table.setHorizontalHeaderLabels(
            ["序号", "文件名", "类型", "X 尺寸", "Y 页数",
             "文件大小（扇区）", "起始扇区", "结束扇区"])
        self.table.verticalHeader().setVisible(False)
        self.table.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self.table.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self.table.setAlternatingRowColors(True)
        header = self.table.horizontalHeader()
        header.setSectionResizeMode(QtWidgets.QHeaderView.ResizeToContents)
        header.setStretchLastSection(True)
        fl.addWidget(self.table)
        right.addWidget(file_box, 1)

        # 内存状态 + 操作进度
        bottom = QtWidgets.QHBoxLayout()
        bottom.setSpacing(8)
        right.addLayout(bottom)

        status_box = QtWidgets.QGroupBox("闪存内存状态")
        sl = QtWidgets.QVBoxLayout(status_box)
        self.lbl_chip = QtWidgets.QLabel("扇区数：--    容量：--")
        self.lbl_chip.setWordWrap(True)
        sl.addWidget(self.lbl_chip)
        self.lbl_used = QtWidgets.QLabel("已用：0 / 0 扇区（0%）")
        self.lbl_used.setWordWrap(True)
        sl.addWidget(self.lbl_used)
        self.ram_bar = QtWidgets.QProgressBar()
        self.ram_bar.setRange(0, 100)
        self.ram_bar.setValue(0)
        self.ram_bar.setTextVisible(True)
        sl.addWidget(self.ram_bar)
        self.lbl_index = QtWidgets.QLabel("索引表：表头 --，文件 0 个")
        self.lbl_index.setWordWrap(True)
        sl.addWidget(self.lbl_index)
        bottom.addWidget(status_box, 1)

        prog_box = QtWidgets.QGroupBox("操作进度")
        pl = QtWidgets.QVBoxLayout(prog_box)
        self.lbl_stage = QtWidgets.QLabel("阶段：空闲")
        self.lbl_stage.setWordWrap(True)
        pl.addWidget(self.lbl_stage)
        self.prog_bar = QtWidgets.QProgressBar()
        self.prog_bar.setRange(0, 100)
        self.prog_bar.setValue(0)
        pl.addWidget(self.prog_bar)
        self.lbl_prog = QtWidgets.QLabel("进度：0%    预计所需：--")
        self.lbl_prog.setWordWrap(True)
        pl.addWidget(self.lbl_prog)
        bottom.addWidget(prog_box, 1)

        # 操作报文区
        log_box = QtWidgets.QGroupBox("操作报文区")
        ll = QtWidgets.QVBoxLayout(log_box)
        lbar = QtWidgets.QHBoxLayout()
        lbar.addStretch(1)
        self.btn_copy_log = QtWidgets.QPushButton("复制全部")
        self.btn_clear_log = QtWidgets.QPushButton("清空")
        lbar.addWidget(self.btn_copy_log)
        lbar.addWidget(self.btn_clear_log)
        ll.addLayout(lbar)
        self.log_edit = QtWidgets.QPlainTextEdit()
        self.log_edit.setReadOnly(True)
        self.log_edit.setMaximumBlockCount(20000)
        self.log_edit.setStyleSheet(
            "font-family: Consolas, 'Cascadia Mono', monospace; font-size: 12px;")
        self.log_edit.setTextInteractionFlags(
            QtCore.Qt.TextSelectableByMouse | QtCore.Qt.TextSelectableByKeyboard)
        ll.addWidget(self.log_edit)
        log_box.setMinimumHeight(150)
        root.addWidget(log_box, 1)

        # 信号
        self.btn_scan.clicked.connect(self.refresh_ports)
        self.btn_port.clicked.connect(self.toggle_port)
        self.btn_refresh_info.clicked.connect(self.op_refresh_info)
        self.btn_add.clicked.connect(self.op_add_file)
        self.btn_modify.clicked.connect(self.op_modify_file)
        self.btn_move.clicked.connect(self.op_move_file)
        self.btn_export.clicked.connect(self.op_export_file)
        self.btn_delete.clicked.connect(self.op_delete_file)
        self.btn_format.clicked.connect(self.op_format)
        self.btn_sector.clicked.connect(self.op_sector_view)
        self.btn_exit.clicked.connect(self.op_exit_flash)
        self.btn_copy_log.clicked.connect(self.copy_log)
        self.btn_clear_log.clicked.connect(self.log_edit.clear)
        self.table.itemSelectionChanged.connect(self.update_actions)

    @staticmethod
    def _ctrl_button(layout, text):
        btn = QtWidgets.QPushButton(text)
        btn.setMinimumHeight(24)
        btn.setMaximumHeight(46)
        btn.setSizePolicy(QtWidgets.QSizePolicy.Preferred,
                          QtWidgets.QSizePolicy.Expanding)
        btn.setStyleSheet("text-align:left; padding-left:10px;")
        layout.addWidget(btn)
        return btn

    # ---------- 串口 ----------
    def refresh_ports(self):
        self.port_combo.clear()
        ports = list(list_ports.comports())
        for p in ports:
            desc = p.description or ""
            self.port_combo.addItem("%s  %s" % (p.device, desc), p.device)
        if not ports:
            self.port_combo.addItem("（未发现串口）", None)
        self.append_log("串口列表已刷新，发现 %d 个串口" % len(ports))

    def toggle_port(self):
        if self.tool.link.is_open:
            self.tool.link.close()
            self.tool.sector_total = 0
            self.tool.raw = b""
            self.tool.header = None
            self.tool.files = []
            self.append_log("串口已关闭")
            self.refresh_table()
        else:
            port = self.port_combo.currentData()
            if not port:
                QtWidgets.QMessageBox.warning(self, "提示", "没有可用的串口")
                return
            try:
                self.tool.link.open(port)
            except Exception as e:
                QtWidgets.QMessageBox.critical(self, "打开串口失败", str(e))
                return
            self.append_log("串口已打开：%s（230400 8-N-1）" % port)
            self.tool.sector_total = 0
            self.tool.files = []
            self.refresh_table()
        self.update_actions()

    # ---------- 状态 ----------
    def update_actions(self):
        connected = self.tool.link.is_open
        ready = connected and self.tool.sector_total > 0 and not self.busy
        has_sel = self.table.currentRow() >= 0

        self.port_combo.setEnabled(not self.busy and not connected)
        self.btn_scan.setEnabled(not self.busy and not connected)
        self.btn_port.setEnabled(not self.busy)
        self.btn_port.setText("关闭串口" if connected else "打开串口")
        self.btn_refresh_info.setEnabled(connected and not self.busy)

        for btn in (self.btn_add, self.btn_format, self.btn_sector):
            btn.setEnabled(ready)
        for btn in (self.btn_modify, self.btn_move, self.btn_export, self.btn_delete):
            btn.setEnabled(ready and has_sel)
        self.btn_exit.setEnabled(connected and not self.busy)

        if connected:
            self.lbl_conn.setText("已连接：%s" % self.port_combo.currentData())
            self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_OK)
        else:
            self.lbl_conn.setText("未连接")
            self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_ERR)

    def refresh_table(self):
        files = self.tool.files
        row = self.table.currentRow()
        self.table.blockSignals(True)
        self.table.setRowCount(len(files))
        for i, f in enumerate(files):
            values = [str(i), f.name, f.kind, str(f.xsize), str(f.y_pages),
                      str(f.sectors), str(f.start_sector), str(f.end_sector)]
            for col, text in enumerate(values):
                item = QtWidgets.QTableWidgetItem(text)
                item.setTextAlignment(QtCore.Qt.AlignCenter if col != 1
                                      else QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
                if f.is_driver:
                    item.setForeground(QtGui.QColor(COLOR_DRIVER))
                self.table.setItem(i, col, item)
        self.table.blockSignals(False)
        if 0 <= row < len(files):
            self.table.selectRow(row)

        total = self.tool.sector_total
        used = sum(f.sectors for f in files if f.sectors > 0)
        if total > 0:
            pct = min(100.0, used * 100.0 / total)
            self.lbl_chip.setText("扇区数：%d    容量：%s"
                                  % (total, fmt_size(total * SECTOR_SIZE)))
            self.lbl_used.setText("已用：%d / %d 扇区（%.1f%%）"
                                  % (used, total, pct))
            self.ram_bar.setValue(int(round(pct)))
            self.ram_bar.setFormat("%.1f%%" % pct)
            if self.tool.header is None:
                self.lbl_index.setText("索引表：未找到表头，文件 0 个")
            else:
                self.lbl_index.setText(
                    "索引表：表头 %s（扇区 %d），文件 %d 个"
                    % (fmt_addr(self.tool.header), self.tool.header // SECTOR_SIZE,
                       len(files)))
        else:
            self.lbl_chip.setText("扇区数：--    容量：--")
            self.lbl_used.setText("已用：0 / 0 扇区（0%）")
            self.ram_bar.setValue(0)
            self.ram_bar.setFormat("0%")
            self.lbl_index.setText("索引表：表头 --，文件 0 个")
        self.update_actions()

    def append_log(self, text):
        self.log_edit.appendPlainText(text)
        bar = self.log_edit.verticalScrollBar()
        bar.setValue(bar.maximum())

    def copy_log(self):
        QtWidgets.QApplication.clipboard().setText(self.log_edit.toPlainText())
        self.append_log("操作报文已复制到剪贴板")

    # ---------- 任务 ----------
    def start_task(self, title, fn):
        if not self.tool.link.is_open:
            QtWidgets.QMessageBox.warning(self, "提示", "请先打开串口")
            return
        if self.busy:
            return
        self.busy = True
        self.task_t0 = time.monotonic()
        self.prog_bar.setValue(0)
        self.lbl_stage.setText("阶段：%s 准备中" % title)
        self.lbl_prog.setText("进度：0%    预计所需：--")
        self.update_actions()
        self.worker.submit(fn)

    def on_busy(self, busy):
        self.busy = busy
        self.update_actions()

    def on_progress(self, stage, pct, eta):
        self.prog_bar.setValue(pct)
        self.lbl_stage.setText("阶段：%s" % stage)
        elapsed = time.monotonic() - self.task_t0 if self.task_t0 else 0.0
        total = (elapsed + eta) if eta >= 0 else -1
        self.lbl_prog.setText("进度：%d%%    预计所需：%s    已用：%s"
                              % (pct, fmt_duration(total), fmt_duration(elapsed)))

    def on_task_done(self, ok, message):
        self.busy = False
        if ok:
            self.append_log("[完成] %s" % message)
        else:
            self.append_log("[失败] %s" % message)
            QtWidgets.QMessageBox.critical(self, "操作失败", message)
        self.refresh_table()
        self.update_actions()

    # ---------- 操作入口 ----------
    def op_refresh_info(self):
        self.start_task("刷新闪存信息", lambda ctx: self.tool.refresh(ctx))

    def op_add_file(self):
        dlg = AddFileDialog(self.tool, self)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        name, xsize, ysize, path = dlg.values()
        self.start_task("添加文件",
                        lambda ctx: self.tool.add_file(ctx, name, xsize, ysize, path))

    def op_modify_file(self):
        row, f = self._selected_file()
        if f is None:
            return
        dlg = ModifyDialog(f, self)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        name, xsize, y = dlg.values()
        self.start_task("修改文件信息",
                        lambda ctx: self.tool.modify_file(ctx, row, f.name,
                                                          name, xsize, y))

    def op_move_file(self):
        row, f = self._selected_file()
        if f is None:
            return
        target = find_move_sector(self.tool.files, f)
        if target is None:
            QtWidgets.QMessageBox.information(
                self, "无法移动",
                "文件「%s」前面没有足够大的连续空闲扇区，无法向前移动。" % f.name)
            return
        ret = QtWidgets.QMessageBox.question(
            self, "确认移动",
            "文件「%s」当前位于扇区 %d ~ %d。\n"
            "将把数据移动到扇区 %d ~ %d（向前整理）。\n\n是否继续？"
            % (f.name, f.start_sector, f.end_sector, target, target + f.sectors - 1),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No)
        if ret != QtWidgets.QMessageBox.Yes:
            return
        self.start_task("移动文件",
                        lambda ctx: self.tool.move_file(ctx, row, f.name))

    def op_export_file(self):
        row, f = self._selected_file()
        if f is None:
            return
        path, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "导出文件", f.name + ".bin", "二进制文件 (*.bin);;所有文件 (*)")
        if not path:
            return
        self.start_task("导出文件",
                        lambda ctx: self.tool.export_file(ctx, row, f.name, path))

    def op_delete_file(self):
        row, f = self._selected_file()
        if f is None:
            return
        ret = QtWidgets.QMessageBox.question(
            self, "确认删除",
            "将删除文件「%s」（扇区 %d ~ %d），并擦除其数据区。\n\n是否继续？"
            % (f.name, f.start_sector, f.end_sector),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No)
        if ret != QtWidgets.QMessageBox.Yes:
            return
        self.start_task("删除文件",
                        lambda ctx: self.tool.delete_file(ctx, row, f.name))

    def op_format(self):
        ret = QtWidgets.QMessageBox.warning(
            self, "确认格式化",
            "将擦除整颗闪存（包括全部文件索引和文件数据），\n"
            "并在扇区 0 的 0x000000 写入新的表头。\n\n"
            "此操作不可恢复，是否继续？",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No)
        if ret != QtWidgets.QMessageBox.Yes:
            return
        self.start_task("格式化芯片", lambda ctx: self.tool.format_chip(ctx))

    def op_sector_view(self):
        if not self.tool.link.is_open:
            QtWidgets.QMessageBox.warning(self, "提示", "请先打开串口")
            return
        if self.tool.sector_total <= 0:
            QtWidgets.QMessageBox.warning(self, "提示", "请先刷新闪存信息")
            return
        dlg = SectorDialog(self.tool, self)
        dlg.exec_()

    def op_exit_flash(self):
        self.start_task("退出闪存模式", lambda ctx: self.tool.exit_flash_mode(ctx))

    def _selected_file(self):
        row = self.table.currentRow()
        if row < 0:
            QtWidgets.QMessageBox.warning(self, "提示", "请先在文件列表中选择一个文件")
            return -1, None
        if row >= len(self.tool.files):
            QtWidgets.QMessageBox.warning(self, "提示", "文件列表已失效，请刷新闪存信息")
            return -1, None
        return row, self.tool.files[row]

    # ---------- 关闭 ----------
    def closeEvent(self, event):
        if self.busy:
            ret = QtWidgets.QMessageBox.question(
                self, "退出",
                "当前有操作正在进行，退出可能导致设备状态不确定。\n是否仍要退出？",
                QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
                QtWidgets.QMessageBox.No)
            if ret != QtWidgets.QMessageBox.Yes:
                event.ignore()
                return
        self.worker.stop()
        if not self.worker.wait(1500):
            self.worker.terminate()
            self.worker.wait(500)
        self.tool.link.close()
        event.accept()


def main():
    QtCore.QCoreApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
    QtCore.QCoreApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("YUERAN闪存管理器")
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
