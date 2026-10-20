# -*- coding: utf-8 -*-
"""YUERAN 基础指令构造器

逐条构建设备端的基础指令并直接发送，不做文件表、扇区管理之类的高层封装。

基础指令（8N1，所有数值高位在前；界面可选波特率 460800 / 230400 / 115200）：
    串口数据包：0xAB + 包长度(1) + 指令数(1) + 指令数据
        包长度 = 3 + 指令数据长度（包含 3 字节包头），指令数固定为 1
    指令数据：
    0F 0F 0F                                      退出闪存模式
    "INFO"                                        读取 2 字节扇区数
    'R' + 字节地址(3) + 长度(2) + 校验(1)          读取 1~256 字节（设备不回完成标记）
    'W' + 字节地址(3) + 长度(1) + 校验(1) + 数据   写入 1~16 字节
    'D' + 扇区地址(2) + 扇区数(2) + 校验(1)        擦除扇区
    "CCC"                                         擦除整颗闪存

    R   校验和：前 6 个字节之和的低 8 位
    W/D 校验和：前 5 个字节之和的低 8 位
    单片机校验通过后先回发包头 4 字节（0xAB、包长度、指令数、指令首字节），
    执行完成后返回 3 字节 0xAB 0xCD 0xEF（R 指令不回完成标记）
"""

import sys
import time
import traceback
from datetime import datetime

import serial
from serial.tools import list_ports

from PyQt5 import QtCore, QtGui, QtWidgets

BAUD_OPTIONS = (460800, 230400, 115200)
BAUD = BAUD_OPTIONS[0]
PACKET_HEAD = 0xAB
PACKET_CMD_COUNT = 1
ECHO_LEN = 4
DONE_MARK = b"\xab\xcd\xef"
MAX_READ = 256
MAX_WRITE = 16

SECTOR_SIZE = 4096
INDEX_TOTAL = 8192
SLOT = 16
MAX_FILES = 64

COLOR_OK = "#1b5e20"
COLOR_ERR = "#b71c1c"


# ============================== 工具函数 ==============================
def now_text():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def fmt_size(n):
    if n >= 1024 * 1024:
        return "%.2f MB" % (n / 1048576.0)
    if n >= 1024:
        return "%.1f KB" % (n / 1024.0)
    return "%d B" % n


def wrap_hex(data, per=16):
    if not data:
        return ""
    return "\n".join(" ".join("%02X" % b for b in data[i:i + per])
                     for i in range(0, len(data), per))


def hex_dump(data):
    """带偏移和 ASCII 的十六进制转储"""
    lines = []
    for off in range(0, len(data), 16):
        row = data[off:off + 16]
        hexs = " ".join("%02X" % b for b in row)
        asci = "".join(chr(b) if 0x20 <= b <= 0x7E else "." for b in row)
        lines.append("%04X  %-47s  |%s|" % (off, hexs, asci))
    return "\n".join(lines)


def parse_hex_int(text, max_value, what):
    t = text.strip().lower()
    if t.startswith("0x"):
        t = t[2:]
    t = t.replace(" ", "").replace("_", "")
    if not t:
        raise ValueError("%s 不能为空" % what)
    if any(ch not in "0123456789abcdef" for ch in t):
        raise ValueError("%s 只能填写十六进制字符" % what)
    value = int(t, 16)
    if value > max_value:
        raise ValueError("%s 超出范围（最大 0x%X）" % (what, max_value))
    return value


def parse_hex_bytes(text):
    t = text.lower().replace("0x", "")
    for ch in " \t\r\n,;:|-_":
        t = t.replace(ch, "")
    if not t:
        return b""
    if any(ch not in "0123456789abcdef" for ch in t):
        raise ValueError("数据里含非十六进制字符")
    if len(t) % 2:
        raise ValueError("十六进制数据必须按字节成对填写")
    return bytes.fromhex(t)


# ============================== 帧构造 ==============================
def frame_exit():
    return b"\x0f\x0f\x0f"


def frame_info():
    return b"INFO"


def frame_read(addr, length):
    cmd = bytearray(b"R")
    cmd += addr.to_bytes(3, "big")
    cmd += length.to_bytes(2, "big")
    cmd.append(sum(cmd) & 0xFF)
    return bytes(cmd)


def frame_write(addr, data):
    cmd = bytearray(b"W")
    cmd += addr.to_bytes(3, "big")
    cmd += len(data).to_bytes(1, "big")
    cmd.append(sum(cmd) & 0xFF)
    return bytes(cmd) + bytes(data)


def frame_erase_sector(sector, count):
    cmd = bytearray(b"D")
    cmd += sector.to_bytes(2, "big")
    cmd += count.to_bytes(2, "big")
    cmd.append(sum(cmd) & 0xFF)
    return bytes(cmd)


def frame_erase_chip():
    return b"CCC"


def packet_encode(cmd):
    """0xAB + 包长度(1) + 指令数(1) + 指令数据（包长度含 3 字节包头）"""
    cmd = bytes(cmd)
    total = 3 + len(cmd)
    if total > 0xFF:
        raise ValueError("数据包过长：%d 字节（上限 255）" % total)
    return bytes([PACKET_HEAD, total, PACKET_CMD_COUNT]) + cmd


def parse_index(raw):
    """解析索引区原始数据，供扇区查看的“简要说明”使用

    返回 (表头地址 or None, [(名称, 扇区数, 起始扇区, Y字节), ...])
    """
    best = None
    for addr in range(0, min(len(raw), INDEX_TOTAL), SLOT):
        if raw[addr] == 0x00:
            best = addr
    if best is None:
        return None, []

    files = []
    pos = (best + SLOT) % INDEX_TOTAL
    while len(files) < MAX_FILES and raw[pos] != 0xFF:
        unit = raw[pos:pos + SLOT]
        name_bytes = unit[:10]
        end = 0
        for i, b in enumerate(name_bytes):
            if 0x20 <= b <= 0x7E:
                end = i + 1
            else:
                break
        name = name_bytes[:end].decode("ascii", "replace")
        files.append((name,
                      int.from_bytes(unit[10:12], "big"),
                      int.from_bytes(unit[12:14], "big"),
                      unit[15]))
        pos = (pos + SLOT) % INDEX_TOTAL
    return best, files


class ProtocolError(Exception):
    pass


# ============================== 串口层 ==============================
class Link(object):
    """只做“发一帧、收应答”这一件事，不做任何业务封装"""

    def __init__(self):
        self.ser = None
        self.baud = BAUD

    @property
    def is_open(self):
        return self.ser is not None and self.ser.is_open

    def open(self, port, baud=None):
        self.close()
        self.baud = int(baud or BAUD)
        self.ser = serial.Serial(
            port=port, baudrate=self.baud, bytesize=serial.EIGHTBITS,
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

    def _read_exact(self, n, timeout):
        end = time.monotonic() + timeout
        buf = bytearray()
        while len(buf) < n:
            if time.monotonic() >= end:
                break
            chunk = self.ser.read(n - len(buf))
            if chunk:
                buf.extend(chunk)
        return bytes(buf)

    def exchange(self, frame, data_len=0, need_done=True, timeout=5.0):
        """把指令封装成数据包发送并收应答。

        frame 是纯指令数据（不含 0xAB 包头）；
        need_done=True：收到包头确认后继续读到 AB CD EF；
        need_done=False：收到包头确认后按 data_len 收满数据（R 指令）。
        返回值：{"raw", "echo", "payload", "done", "packet"}
        """
        if not self.is_open:
            raise ProtocolError("串口未打开")
        packet = packet_encode(frame)
        expected = packet[:ECHO_LEN]

        self.ser.reset_input_buffer()
        self.ser.write(packet)
        self.ser.flush()

        head = self._read_exact(ECHO_LEN, min(2.0, timeout))
        if not head:
            raise ProtocolError(
                "未收到单片机的指令确认回显（期望 %s，实际 0 字节）。"
                "请确认设备处于闪存模式、串口接在 USART2、波特率为 %d"
                % (wrap_hex(expected).replace("\n", " "), self.baud))
        if len(head) < ECHO_LEN:
            raise ProtocolError(
                "指令确认回显不完整（期望 %d 字节 %s，实际 %d 字节 %s）"
                % (ECHO_LEN, wrap_hex(expected).replace("\n", " "),
                   len(head), wrap_hex(head).replace("\n", " ")))
        if head != expected:
            raise ProtocolError(
                "指令确认回显不匹配（期望 %s，实际 %s）。"
                "可能波特率不匹配、线路受干扰，或指令校验未通过"
                % (wrap_hex(expected).replace("\n", " "),
                   wrap_hex(head).replace("\n", " ")))

        if need_done:
            raw = bytearray()
            end = time.monotonic() + timeout
            while True:
                cut = raw.find(DONE_MARK)
                if cut >= 0:
                    return {"raw": head + bytes(raw), "echo": True,
                            "payload": bytes(raw[:cut]), "done": True,
                            "packet": packet}
                if time.monotonic() >= end:
                    raise ProtocolError(
                        "已收到指令确认回显，但等待执行完成标记 AB CD EF 超时"
                        "（%.1f 秒，期间收到 %d 字节：%s）"
                        % (timeout, len(raw),
                           wrap_hex(bytes(raw), 32) if raw else "无"))
                chunk = self.ser.read(128)
                if chunk:
                    raw.extend(chunk)

        data = self._read_exact(data_len, timeout)
        if len(data) < data_len:
            raise ProtocolError(
                "已收到指令确认回显，但数据接收超时（期望 %d 字节，实际 %d 字节）"
                % (data_len, len(data)))
        return {"raw": head + data, "echo": True,
                "payload": data[:data_len], "done": False, "packet": packet}


# ============================== 后台任务线程 ==============================
class Worker(QtCore.QThread):
    sig_log = QtCore.pyqtSignal(str)
    sig_busy = QtCore.pyqtSignal(bool)
    sig_done = QtCore.pyqtSignal(bool, str, object)

    def __init__(self, parent=None):
        super(Worker, self).__init__(parent)
        self._queue = []
        self._lock = QtCore.QMutex()
        self._wait = QtCore.QWaitCondition()
        self._running = True

    def submit(self, fn):
        self._lock.lock()
        self._queue.append(fn)
        self._wait.wakeAll()
        self._lock.unlock()

    def stop(self):
        self._running = False
        self._wait.wakeAll()

    def run(self):
        while self._running:
            self._lock.lock()
            while self._running and not self._queue:
                self._wait.wait(self._lock, 200)
            if not self._running:
                self._lock.unlock()
                break
            fn = self._queue.pop(0)
            self._lock.unlock()

            self.sig_busy.emit(True)
            try:
                result = fn(self.sig_log.emit)
            except Exception as e:
                self.sig_log.emit("[%s] 异常：%s"
                                  % (now_text(), traceback.format_exc().rstrip()))
                self.sig_done.emit(False, str(e), None)
            else:
                self.sig_done.emit(True, "", result)
            self.sig_busy.emit(False)


# ============================== 扇区查看 ==============================
class SectorDialog(QtWidgets.QDialog):
    """用基础 R 指令组合读取并查看一个扇区（与闪存管理器的扇区查看一致）"""

    def __init__(self, link, sector_total, parent=None):
        super(SectorDialog, self).__init__(parent)
        self.link = link
        self.sector_total = sector_total
        self.index_raw = None
        self.index_error = None
        self.header = None
        self.files = []

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
        self.spin.setRange(0, max(0, sector_total - 1))
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

        # 打开窗口时先用 R 指令读一次索引区，供“简要说明”判断文件归属
        self._load_index()

    # ---------- 基础指令组合 ----------
    def _read_bytes(self, addr, length):
        out = bytearray()
        off = 0
        while off < length:
            n = min(MAX_READ, length - off)
            res = self.link.exchange(frame_read(addr + off, n),
                                     data_len=n, need_done=False, timeout=5.0)
            out += res["payload"]
            off += n
        return bytes(out)

    def _load_index(self):
        try:
            self.index_raw = self._read_bytes(0, INDEX_TOTAL)
            self.header, self.files = parse_index(self.index_raw)
        except Exception as e:
            self.index_raw = None
            self.index_error = str(e)
            self.header, self.files = None, []

    def read_sector(self):
        sector = self.spin.value()
        if not self.link.is_open:
            QtWidgets.QMessageBox.warning(self, "提示", "串口未打开")
            return
        QtWidgets.QApplication.setOverrideCursor(QtCore.Qt.WaitCursor)
        try:
            data = self._read_bytes(sector * SECTOR_SIZE, SECTOR_SIZE)
        except Exception as e:
            QtWidgets.QApplication.restoreOverrideCursor()
            QtWidgets.QMessageBox.critical(self, "读取失败", str(e))
            return
        QtWidgets.QApplication.restoreOverrideCursor()
        self._render(sector, data)

    # ---------- 显示 ----------
    def _update_addr(self):
        sector = self.spin.value()
        base = sector * SECTOR_SIZE
        self.lbl_addr.setText("地址 0x%06X ~ 0x%06X" % (base, base + SECTOR_SIZE - 1))

    def _render(self, sector, data):
        self.lbl_desc.setText(self._describe(sector, data))
        self.view.setPlainText(self._dump_text(data))

    @staticmethod
    def _dump_text(data):
        """同一行依次显示：偏移、16 字节十六进制、ASCII 明文、该行 16 个字节的位图"""
        lines = []
        for off in range(0, len(data), SLOT):
            row = data[off:off + SLOT]
            hexs = " ".join("%02X" % b for b in row)
            asci = "".join(chr(b) if 0x20 <= b <= 0x7E else "." for b in row)
            bits = "".join(chr(0x2800 + b) for b in row)
            lines.append("%04X  %-47s  |%s|  %s" % (off, hexs, asci, bits))
        return "\n".join(lines)

    def _describe(self, sector, data):
        tail = ""
        if self.index_raw is None:
            tail = "（索引表读取失败：%s）" % self.index_error

        if sector < 2:
            if self.header is None:
                head = "未找到有效表头（空表）"
            else:
                head = ("有效表头 0x%06X（扇区 %d），文件 %d 个"
                        % (self.header, self.header // SECTOR_SIZE, len(self.files)))
            return ("扇区 %d：文件索引区。%s%s"
                    % (sector, head, tail))

        for i, (name, sectors, start, ysize) in enumerate(self.files):
            if start <= sector < start + sectors:
                pos = sector - start + 1
                role = ("字库芯片驱动配置" if (ysize & 0x80)
                        else "文件数据（第 %d / %d 扇区）" % (pos, sectors))
                return "扇区 %d：文件 #%d「%s」的%s%s" % (sector, i, name, role, tail)
        if all(b == 0xFF for b in data):
            return "扇区 %d：空白扇区（全 0xFF）%s" % (sector, tail)
        return "扇区 %d：没有被索引表引用的残留数据%s" % (sector, tail)


# ============================== 主窗口 ==============================
class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super(MainWindow, self).__init__()
        self.link = Link()
        self.worker = Worker(self)
        self.worker.sig_log.connect(self.append_log)
        self.worker.sig_busy.connect(self.on_busy)
        self.worker.sig_done.connect(self.on_done)
        self.worker.start()

        self.busy = False
        self.pending_kind = None
        self.ui_ready = False
        self.valid = {"exit": True, "info": True, "r": False,
                      "w": False, "d": False, "ccc": True}

        self.setWindowTitle("YUERAN 基础指令构造器")
        self._apply_geometry()
        self._build_ui()
        self.refresh_ports()
        self.ui_ready = True
        self.update_read_preview()
        self.update_write_preview()
        self.update_erase_preview()
        self.update_buttons()
        self.append_log("YUERAN 基础指令构造器已启动：选择串口并打开后，即可逐条发送基础指令")

    # ---------- 窗口 ----------
    def _apply_geometry(self):
        screen = QtWidgets.QApplication.primaryScreen()
        geo = (screen.availableGeometry() if screen is not None
               else QtCore.QRect(0, 0, 1280, 800))
        w = min(geo.width(), max(720, int(geo.width() * 0.75)))
        h = min(geo.height(), max(520, int(geo.height() * 0.75)))
        self.setGeometry(geo.x() + (geo.width() - w) // 2,
                         geo.y() + (geo.height() - h) // 2, w, h)
        self.setMinimumSize(min(w, 900), min(h, 600))

    # ---------- 界面 ----------
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
        bar.addSpacing(10)
        bar.addWidget(QtWidgets.QLabel("波特率："))
        self.baud_combo = QtWidgets.QComboBox()
        for b in BAUD_OPTIONS:
            self.baud_combo.addItem(str(b), b)
        self.baud_combo.setMinimumWidth(90)
        bar.addWidget(self.baud_combo)
        self.btn_scan = QtWidgets.QPushButton("刷新串口列表")
        bar.addWidget(self.btn_scan)
        self.btn_port = QtWidgets.QPushButton("打开串口")
        bar.addWidget(self.btn_port)
        bar.addSpacing(14)
        self.lbl_conn = QtWidgets.QLabel("未连接")
        self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_ERR)
        bar.addWidget(self.lbl_conn)
        bar.addSpacing(14)
        self.lbl_state = QtWidgets.QLabel("就绪")
        bar.addWidget(self.lbl_state)
        bar.addSpacing(14)
        self.btn_sector = QtWidgets.QPushButton("查看扇区")
        bar.addWidget(self.btn_sector)
        bar.addStretch(1)
        bar.addWidget(QtWidgets.QLabel("8-N-1  普通模式"))
        root.addLayout(bar)

        # 指令页
        self.tabs = QtWidgets.QTabWidget()
        self.tabs.addTab(self._build_exit_tab(), "0F 退出闪存模式")
        self.tabs.addTab(self._build_info_tab(), "INFO 闪存信息")
        self.tabs.addTab(self._build_read_tab(), "R 读闪存")
        self.tabs.addTab(self._build_write_tab(), "W 写闪存")
        self.tabs.addTab(self._build_erase_tab(), "D 擦除扇区")
        self.tabs.addTab(self._build_chip_tab(), "CCC 整片擦除")
        root.addWidget(self.tabs, 3)

        # 报文区
        log_box = QtWidgets.QGroupBox("收发报文")
        ll = QtWidgets.QVBoxLayout(log_box)
        lb = QtWidgets.QHBoxLayout()
        self.btn_copy_log = QtWidgets.QPushButton("复制全部")
        self.btn_clear_log = QtWidgets.QPushButton("清空")
        lb.addStretch(1)
        lb.addWidget(self.btn_copy_log)
        lb.addWidget(self.btn_clear_log)
        ll.addLayout(lb)
        self.log_edit = QtWidgets.QPlainTextEdit()
        self.log_edit.setReadOnly(True)
        self.log_edit.setMaximumBlockCount(30000)
        self.log_edit.setStyleSheet(
            "font-family: Consolas, 'Cascadia Mono', monospace; font-size: 12px;")
        self.log_edit.setTextInteractionFlags(
            QtCore.Qt.TextSelectableByMouse | QtCore.Qt.TextSelectableByKeyboard)
        ll.addWidget(self.log_edit)
        log_box.setMinimumHeight(150)
        root.addWidget(log_box, 1)

        self.btn_scan.clicked.connect(self.refresh_ports)
        self.btn_port.clicked.connect(self.toggle_port)
        self.btn_sector.clicked.connect(self.open_sector_view)
        self.btn_copy_log.clicked.connect(self.copy_log)
        self.btn_clear_log.clicked.connect(self.log_edit.clear)

    # ---------- 页面辅助 ----------
    @staticmethod
    def _preview_edit():
        ed = QtWidgets.QPlainTextEdit()
        ed.setReadOnly(True)
        ed.setFixedHeight(86)
        ed.setLineWrapMode(QtWidgets.QPlainTextEdit.NoWrap)
        ed.setStyleSheet(
            "font-family: Consolas, 'Cascadia Mono', monospace; font-size: 12px;"
            "background:#f7f9fc;")
        return ed

    @staticmethod
    def _info_label():
        lb = QtWidgets.QLabel("-")
        lb.setWordWrap(True)
        return lb

    @staticmethod
    def _timeout_spin(default):
        sp = QtWidgets.QSpinBox()
        sp.setRange(1, 1200)
        sp.setValue(default)
        sp.setSuffix(" 秒")
        sp.setMaximumWidth(110)
        return sp

    # ---------- 0F ----------
    def _build_exit_tab(self):
        w = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        hint = QtWidgets.QLabel("发送 0F 0F 0F，让设备退出闪存模式；设备会返回 DONE。")
        hint.setWordWrap(True)
        lay.addWidget(hint)

        lay.addWidget(QtWidgets.QLabel("发送数据包："))
        self.prev_exit = self._preview_edit()
        self.prev_exit.setPlainText(wrap_hex(packet_encode(frame_exit())))
        lay.addWidget(self.prev_exit)

        row = QtWidgets.QHBoxLayout()
        row.addWidget(QtWidgets.QLabel("应答超时："))
        self.to_exit = self._timeout_spin(5)
        row.addWidget(self.to_exit)
        row.addStretch(1)
        self.btn_exit = QtWidgets.QPushButton("发送 0F 0F 0F")
        row.addWidget(self.btn_exit)
        lay.addLayout(row)
        lay.addStretch(1)

        self.btn_exit.clicked.connect(self.send_exit)
        return w

    # ---------- INFO ----------
    def _build_info_tab(self):
        w = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        hint = QtWidgets.QLabel("发送 INFO，设备返回 2 字节扇区数（高字节在前），随后返回 DONE。")
        hint.setWordWrap(True)
        lay.addWidget(hint)

        lay.addWidget(QtWidgets.QLabel("发送数据包："))
        self.prev_info = self._preview_edit()
        self.prev_info.setPlainText(wrap_hex(packet_encode(frame_info())))
        lay.addWidget(self.prev_info)

        row = QtWidgets.QHBoxLayout()
        row.addWidget(QtWidgets.QLabel("应答超时："))
        self.to_info = self._timeout_spin(5)
        row.addWidget(self.to_info)
        row.addStretch(1)
        self.btn_info = QtWidgets.QPushButton("发送 INFO")
        row.addWidget(self.btn_info)
        lay.addLayout(row)

        self.info_result = QtWidgets.QLabel("扇区数：--")
        self.info_result.setStyleSheet("font-size:15px; font-weight:bold; color:%s;" % COLOR_OK)
        lay.addWidget(self.info_result)
        lay.addStretch(1)

        self.btn_info.clicked.connect(self.send_info)
        return w

    # ---------- R ----------
    def _build_read_tab(self):
        w = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        hint = QtWidgets.QLabel(
            "构造 'R' + 字节地址(3) + 长度(2) + 校验(1)，读取 1~256 字节。"
            "设备不回 DONE。")
        hint.setWordWrap(True)
        lay.addWidget(hint)

        form = QtWidgets.QFormLayout()
        self.r_addr = QtWidgets.QLineEdit("000000")
        self.r_addr.setPlaceholderText("十六进制，如 001000")
        self.r_addr.setMaximumWidth(190)
        form.addRow("字节地址：", self.r_addr)
        self.r_len = QtWidgets.QSpinBox()
        self.r_len.setRange(1, MAX_READ)
        self.r_len.setValue(256)
        form.addRow("长度（1~256）：", self.r_len)
        self.to_r = self._timeout_spin(5)
        form.addRow("应答超时：", self.to_r)
        lay.addLayout(form)

        lay.addWidget(QtWidgets.QLabel("发送数据包："))
        self.prev_r = self._preview_edit()
        lay.addWidget(self.prev_r)
        self.info_r = self._info_label()
        lay.addWidget(self.info_r)

        row = QtWidgets.QHBoxLayout()
        row.addStretch(1)
        self.btn_r = QtWidgets.QPushButton("发送 R 读取指令")
        row.addWidget(self.btn_r)
        lay.addLayout(row)

        lay.addWidget(QtWidgets.QLabel("应答数据："))
        self.r_out = QtWidgets.QPlainTextEdit()
        self.r_out.setReadOnly(True)
        self.r_out.setLineWrapMode(QtWidgets.QPlainTextEdit.NoWrap)
        self.r_out.setStyleSheet(
            "font-family: Consolas, 'Cascadia Mono', monospace; font-size: 12px;")
        lay.addWidget(self.r_out, 1)

        self.r_addr.textChanged.connect(self.update_read_preview)
        self.r_len.valueChanged.connect(self.update_read_preview)
        self.btn_r.clicked.connect(self.send_r)
        return w

    # ---------- W ----------
    def _build_write_tab(self):
        w = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        hint = QtWidgets.QLabel(
            "构造 'W' + 字节地址(3) + 长度(1) + 校验(1) + 数据，写入 1~16 字节。"
            "长度由下面的数据字节数自动得到，发送后等待 DONE。")
        hint.setWordWrap(True)
        lay.addWidget(hint)

        form = QtWidgets.QFormLayout()
        self.w_addr = QtWidgets.QLineEdit("000000")
        self.w_addr.setPlaceholderText("十六进制，如 002000")
        self.w_addr.setMaximumWidth(190)
        form.addRow("字节地址：", self.w_addr)
        self.w_count = QtWidgets.QLabel("数据长度：0 字节")
        form.addRow("", self.w_count)
        self.to_w = self._timeout_spin(30)
        form.addRow("应答超时：", self.to_w)
        lay.addLayout(form)

        lay.addWidget(QtWidgets.QLabel("写入数据（十六进制，可空格/换行分隔）："))
        self.w_data = QtWidgets.QPlainTextEdit()
        self.w_data.setPlaceholderText("例如：00 01 02 03 或 00010203")
        self.w_data.setStyleSheet(
            "font-family: Consolas, 'Cascadia Mono', monospace; font-size: 12px;")
        lay.addWidget(self.w_data, 1)

        lay.addWidget(QtWidgets.QLabel("发送数据包："))
        self.prev_w = self._preview_edit()
        lay.addWidget(self.prev_w)
        self.info_w = self._info_label()
        lay.addWidget(self.info_w)

        row = QtWidgets.QHBoxLayout()
        row.addStretch(1)
        self.btn_w = QtWidgets.QPushButton("发送 W 写入指令")
        row.addWidget(self.btn_w)
        lay.addLayout(row)

        self.w_addr.textChanged.connect(self.update_write_preview)
        self.w_data.textChanged.connect(self.update_write_preview)
        self.btn_w.clicked.connect(self.send_w)
        return w

    # ---------- D ----------
    def _build_erase_tab(self):
        w = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        hint = QtWidgets.QLabel(
            "构造 'D' + 扇区地址(2) + 扇区数(2) + 校验(1)，擦除指定扇区。\n"
            "设备会先原样回发这条指令表示收到且校验通过，擦除完成后再返回 DONE。")
        hint.setWordWrap(True)
        lay.addWidget(hint)

        form = QtWidgets.QFormLayout()
        self.d_sector = QtWidgets.QLineEdit("0000")
        self.d_sector.setPlaceholderText("十六进制，如 0010")
        self.d_sector.setMaximumWidth(190)
        form.addRow("扇区地址：", self.d_sector)
        self.d_count = QtWidgets.QSpinBox()
        self.d_count.setRange(1, 65535)
        self.d_count.setValue(1)
        form.addRow("扇区数：", self.d_count)
        self.to_d = self._timeout_spin(60)
        form.addRow("应答超时：", self.to_d)
        lay.addLayout(form)

        tip = QtWidgets.QLabel("提示：擦除时间随扇区数增加，超时建议不小于 15 + 0.7 × 扇区数。")
        tip.setStyleSheet("color:#555;")
        lay.addWidget(tip)

        lay.addWidget(QtWidgets.QLabel("发送数据包："))
        self.prev_d = self._preview_edit()
        lay.addWidget(self.prev_d)
        self.info_d = self._info_label()
        lay.addWidget(self.info_d)

        row = QtWidgets.QHBoxLayout()
        row.addStretch(1)
        self.btn_d = QtWidgets.QPushButton("发送 D 擦除指令")
        row.addWidget(self.btn_d)
        lay.addLayout(row)
        lay.addStretch(1)

        self.d_sector.textChanged.connect(self.update_erase_preview)
        self.d_count.valueChanged.connect(self.update_erase_preview)
        self.btn_d.clicked.connect(self.send_d)
        return w

    # ---------- CCC ----------
    def _build_chip_tab(self):
        w = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(w)
        warn = QtWidgets.QLabel("危险操作：CCC 会擦除整颗闪存，发送前会弹出确认。")
        warn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_ERR)
        lay.addWidget(warn)

        lay.addWidget(QtWidgets.QLabel("发送数据包："))
        self.prev_ccc = self._preview_edit()
        self.prev_ccc.setPlainText(wrap_hex(packet_encode(frame_erase_chip())))
        lay.addWidget(self.prev_ccc)

        row = QtWidgets.QHBoxLayout()
        row.addWidget(QtWidgets.QLabel("应答超时："))
        self.to_ccc = self._timeout_spin(420)
        row.addWidget(self.to_ccc)
        row.addStretch(1)
        self.btn_ccc = QtWidgets.QPushButton("发送 CCC 擦除整片")
        row.addWidget(self.btn_ccc)
        lay.addLayout(row)
        lay.addStretch(1)

        self.btn_ccc.clicked.connect(self.send_ccc)
        return w

    # ---------- 串口 ----------
    def refresh_ports(self):
        self.port_combo.clear()
        ports = list(list_ports.comports())
        for p in ports:
            self.port_combo.addItem("%s  %s" % (p.device, p.description or ""), p.device)
        if not ports:
            self.port_combo.addItem("（未发现串口）", None)
        self.append_log("串口列表已刷新，发现 %d 个串口" % len(ports))
        self.update_buttons()

    def toggle_port(self):
        if self.link.is_open:
            self.link.close()
            self.append_log("串口已关闭")
        else:
            port = self.port_combo.currentData()
            baud = self.baud_combo.currentData()
            if not port:
                QtWidgets.QMessageBox.warning(self, "提示", "没有可用的串口")
                return
            try:
                self.link.open(port, baud)
            except Exception as e:
                QtWidgets.QMessageBox.critical(self, "打开串口失败", str(e))
                return
            self.append_log("串口已打开：%s（%d 8-N-1）" % (port, baud))
        self.update_buttons()

    def open_sector_view(self):
        """用 INFO 取扇区总数，再在窗口里用 R 指令分批读取扇区"""
        if self.busy:
            return
        if not self.link.is_open:
            QtWidgets.QMessageBox.warning(self, "提示", "请先打开串口")
            return
        QtWidgets.QApplication.setOverrideCursor(QtCore.Qt.WaitCursor)
        try:
            res = self.link.exchange(frame_info(), need_done=True, timeout=5.0)
            payload = res["payload"]
            if len(payload) < 2:
                raise ProtocolError("INFO 应答不足 2 字节")
            total = (payload[0] << 8) | payload[1]
            if total <= 0:
                raise ProtocolError("设备返回的扇区数为 0")
            dlg = SectorDialog(self.link, total, self)
        except Exception as e:
            QtWidgets.QApplication.restoreOverrideCursor()
            QtWidgets.QMessageBox.critical(self, "无法查看扇区", str(e))
            return
        QtWidgets.QApplication.restoreOverrideCursor()
        self.append_log("查看扇区：闪存共 %d 个扇区，已读取索引区用于说明" % total)
        dlg.exec_()

    def update_buttons(self):
        if not self.ui_ready:
            return
        connected = self.link.is_open
        idle = not self.busy
        self.port_combo.setEnabled(idle and not connected)
        self.baud_combo.setEnabled(idle and not connected)
        self.btn_scan.setEnabled(idle and not connected)
        self.btn_port.setEnabled(idle)
        self.btn_port.setText("关闭串口" if connected else "打开串口")

        pairs = ((self.btn_exit, "exit"), (self.btn_info, "info"),
                 (self.btn_r, "r"), (self.btn_w, "w"),
                 (self.btn_d, "d"), (self.btn_ccc, "ccc"))
        for btn, key in pairs:
            btn.setEnabled(connected and idle and self.valid[key])
        self.btn_sector.setEnabled(connected and idle)

        if connected:
            self.lbl_conn.setText("已连接：%s @ %d" % (self.port_combo.currentData(),
                                                     self.link.baud))
            self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_OK)
        else:
            self.lbl_conn.setText("未连接")
            self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_ERR)
        self.lbl_state.setText("通讯中…" if self.busy else "就绪")

    # ---------- 预览 ----------
    def update_read_preview(self):
        try:
            addr = parse_hex_int(self.r_addr.text(), 0xFFFFFF, "字节地址")
            cmd = frame_read(addr, self.r_len.value())
            frame = packet_encode(cmd)
        except ValueError as e:
            self.prev_r.setPlainText("参数有误：%s" % e)
            self.info_r.setText("<font color='%s'>%s</font>" % (COLOR_ERR, e))
            self.valid["r"] = False
        else:
            self.prev_r.setPlainText(wrap_hex(frame))
            self.info_r.setText("数据包长 %d 字节（命令 %d 字节），命令校验和 0x%02X（前 6 字节之和）"
                                % (len(frame), len(cmd), cmd[-1]))
            self.valid["r"] = True
        self.update_buttons()

    def update_write_preview(self):
        try:
            addr = parse_hex_int(self.w_addr.text(), 0xFFFFFF, "字节地址")
            data = parse_hex_bytes(self.w_data.toPlainText())
            if not 1 <= len(data) <= MAX_WRITE:
                raise ValueError("数据长度必须为 1~%d 字节（当前 %d）"
                                 % (MAX_WRITE, len(data)))
            cmd = frame_write(addr, data)
            frame = packet_encode(cmd)
        except ValueError as e:
            self.prev_w.setPlainText("参数有误：%s" % e)
            self.info_w.setText("<font color='%s'>%s</font>" % (COLOR_ERR, e))
            self.w_count.setText("数据长度：--")
            self.valid["w"] = False
        else:
            self.prev_w.setPlainText(wrap_hex(frame))
            self.info_w.setText("数据 %d 字节，数据包长 %d 字节，命令校验和 0x%02X（前 5 字节之和）"
                                % (len(data), len(frame), cmd[5]))
            self.w_count.setText("数据长度：%d 字节（上限 %d）" % (len(data), MAX_WRITE))
            self.valid["w"] = True
        self.update_buttons()

    def update_erase_preview(self):
        try:
            sector = parse_hex_int(self.d_sector.text(), 0xFFFF, "扇区地址")
            cmd = frame_erase_sector(sector, self.d_count.value())
            frame = packet_encode(cmd)
        except ValueError as e:
            self.prev_d.setPlainText("参数有误：%s" % e)
            self.info_d.setText("<font color='%s'>%s</font>" % (COLOR_ERR, e))
            self.valid["d"] = False
        else:
            self.prev_d.setPlainText(wrap_hex(frame))
            self.info_d.setText("擦除扇区 %d ~ %d（共 %d 个），数据包长 %d 字节，"
                                "命令校验和 0x%02X（前 5 字节之和）"
                                % (sector, sector + self.d_count.value() - 1,
                                   self.d_count.value(), len(frame), cmd[-1]))
            self.valid["d"] = True
        self.update_buttons()

    # ---------- 发送 ----------
    def _start(self, kind, fn):
        if self.busy:
            return
        if not self.link.is_open:
            QtWidgets.QMessageBox.warning(self, "提示", "请先打开串口")
            return
        self.busy = True
        self.pending_kind = kind
        self.update_buttons()
        self.worker.submit(fn)

    def _task_exchange(self, log, frame, data_len=0, need_done=True, timeout=5.0):
        packet = packet_encode(frame)
        log("[%s] TX  %s" % (now_text(), " ".join("%02X" % b for b in packet[:16])
                             + (" …" if len(packet) > 16 else "")))
        for i in range(16, len(packet), 32):
            log("            " + " ".join("%02X" % b for b in packet[i:i + 32]))
        t0 = time.monotonic()
        result = self.link.exchange(frame, data_len=data_len,
                                    need_done=need_done, timeout=timeout)
        cost = time.monotonic() - t0

        raw = result["raw"]
        for i in range(0, len(raw), 32):
            prefix = "RX  " if i == 0 else "    "
            log("[%s] %s%s" % (now_text(), prefix,
                               " ".join("%02X" % b for b in raw[i:i + 32])))

        notes = []
        notes.append("指令确认回显一致" if result["echo"] else "未收到指令确认回显")
        if result["done"]:
            notes.append("收到完成标记 AB CD EF")
        if not need_done:
            notes.append("收到 %d 字节数据" % len(result["payload"]))
        log("            " + "，".join(notes) + "（用时 %.3f 秒）" % cost)
        return result

    def send_exit(self):
        frame = frame_exit()
        timeout = self.to_exit.value()
        self._start("exit", lambda log: self._task_exchange(
            log, frame, need_done=True, timeout=timeout))

    def send_info(self):
        frame = frame_info()
        timeout = self.to_info.value()
        self._start("info", lambda log: self._task_exchange(
            log, frame, need_done=True, timeout=timeout))

    def send_read(self):
        try:
            addr = parse_hex_int(self.r_addr.text(), 0xFFFFFF, "字节地址")
        except ValueError as e:
            QtWidgets.QMessageBox.warning(self, "参数有误", str(e))
            return
        length = self.r_len.value()
        frame = frame_read(addr, length)
        timeout = self.to_r.value()
        self.r_out.clear()
        self._start("r", lambda log: self._task_exchange(
            log, frame, data_len=length, need_done=False, timeout=timeout))

    # 兼容按钮信号命名
    def send_r(self):
        self.send_read()

    def send_write(self):
        try:
            addr = parse_hex_int(self.w_addr.text(), 0xFFFFFF, "字节地址")
            data = parse_hex_bytes(self.w_data.toPlainText())
            if not 1 <= len(data) <= MAX_WRITE:
                raise ValueError("数据长度必须为 1~%d 字节" % MAX_WRITE)
        except ValueError as e:
            QtWidgets.QMessageBox.warning(self, "参数有误", str(e))
            return
        frame = frame_write(addr, data)
        timeout = self.to_w.value()
        self._start("w", lambda log: self._task_exchange(
            log, frame, need_done=True, timeout=timeout))

    def send_w(self):
        self.send_write()

    def send_erase(self):
        try:
            sector = parse_hex_int(self.d_sector.text(), 0xFFFF, "扇区地址")
        except ValueError as e:
            QtWidgets.QMessageBox.warning(self, "参数有误", str(e))
            return
        count = self.d_count.value()
        frame = frame_erase_sector(sector, count)
        timeout = self.to_d.value()
        self._start("d", lambda log: self._task_exchange(
            log, frame, need_done=True, timeout=timeout))

    def send_d(self):
        self.send_erase()

    def send_erase_chip(self):
        ret = QtWidgets.QMessageBox.warning(
            self, "确认发送 CCC",
            "将向设备发送 CCC，擦除整颗闪存。\n此操作不可恢复，是否继续？",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No)
        if ret != QtWidgets.QMessageBox.Yes:
            return
        frame = frame_erase_chip()
        timeout = self.to_ccc.value()
        self._start("ccc", lambda log: self._task_exchange(
            log, frame, need_done=True, timeout=timeout))

    def send_ccc(self):
        self.send_erase_chip()

    # ---------- 任务结果 ----------
    def on_busy(self, busy):
        self.busy = busy
        self.update_buttons()

    def on_done(self, ok, message, result):
        self.busy = False
        kind = self.pending_kind
        self.pending_kind = None
        if not ok:
            self.append_log("[%s] 失败：%s" % (now_text(), message))
            QtWidgets.QMessageBox.critical(self, "操作失败", message)
        else:
            self.append_log("[%s] 完成" % now_text())
            if kind == "r" and isinstance(result, dict):
                payload = result.get("payload", b"")
                self.r_out.setPlainText(hex_dump(payload) if payload else "（没有收到数据）")
            elif kind == "info" and isinstance(result, dict):
                payload = result.get("payload", b"")
                if len(payload) >= 2:
                    sectors = (payload[0] << 8) | payload[1]
                    self.info_result.setText(
                        "扇区数：%d（按 4096 字节/扇区约 %s）"
                        % (sectors, fmt_size(sectors * 4096)))
                else:
                    self.info_result.setText("应答不足 2 字节：%s"
                                             % (wrap_hex(payload) or "无数据"))
                    self.append_log("INFO 应答不足 2 字节")
        self.update_buttons()

    # ---------- 报文 ----------
    def append_log(self, text):
        self.log_edit.appendPlainText(text)
        bar = self.log_edit.verticalScrollBar()
        bar.setValue(bar.maximum())

    def copy_log(self):
        QtWidgets.QApplication.clipboard().setText(self.log_edit.toPlainText())
        self.append_log("收发报文已复制到剪贴板")

    # ---------- 关闭 ----------
    def closeEvent(self, event):
        if self.busy:
            ret = QtWidgets.QMessageBox.question(
                self, "退出", "当前有指令正在等待应答，确定退出？",
                QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
                QtWidgets.QMessageBox.No)
            if ret != QtWidgets.QMessageBox.Yes:
                event.ignore()
                return
        self.worker.stop()
        if not self.worker.wait(1500):
            self.worker.terminate()
            self.worker.wait(500)
        self.link.close()
        event.accept()


def main():
    QtCore.QCoreApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
    QtCore.QCoreApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("YUERAN 基础指令构造器")
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
