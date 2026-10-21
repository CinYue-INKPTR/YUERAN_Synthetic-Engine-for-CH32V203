# -*- coding: utf-8 -*-
"""YUERAN 渲染指令基础指令构造器

按 YUERAN_CmdLoad.c 的解析规则，逐条构造渲染模式指令。
渲染口：USART1，115200 8N1。
渲染/闪存模式由模式引脚电平决定，不再通过串口指令切换。

数据包：0xAB + 包长度(1) + 指令数(1) + 指令数据
    包长度 = 3 + 指令数据总长度（含 3 字节包头）

需要校验的指令：末字节 = 该字节之前所有字节之和的低 8 位。
FrameClear / ScreenSet 不用校验和，而是要求指令的 3 个字节相同。

渲染模式没有逐条应答，单片机会持续通过 USART1 上报图层状态：
    1 字节 BackData_Head + 16 个图层 × 7 字节 + 1 字节性能占用（BCD）
    每个图层的 7 字节：Xpos、Ypos、Alpha、Block_Xsize、Block_Ysize、StreamAdd 高/低
"""

import json
import sys
import time
from datetime import datetime

import serial
from serial.tools import list_ports

from PyQt5 import QtCore, QtGui, QtWidgets

BAUD = 115200
PACKET_HEAD = 0xAB
E_LAYER_SIZE = 16
TEXT_MAX = 32

# 渲染管线标志位（PipelineFlag）
PIPELINE_ALPHA = 0x80

LAYER_BYTES = 7
TELEMETRY_FRAME_LEN = 1 + E_LAYER_SIZE * LAYER_BYTES + 1
TELEMETRY_PERIOD_S = 0.020
TELEMETRY_GAP_S = 0.005

COLOR_OK = "#1b5e20"
COLOR_ERR = "#b71c1c"

MIX_ITEMS = (
    ("0 硬覆盖", 0),
    ("1 或叠加", 1),
    ("2 与保留", 2),
    ("3 非擦除", 3),
    ("4 异或翻转", 4),
)

# X/Y 位置、尺寸、流地址的插值函数：D7~6 模式，D5~0 数值
XY_FCN_MODES = (
    ("0 无", 0),
    ("1 匀速", 1),
    ("2 减速", 2),
    ("3 加速", 3),
)

# Alpha 插值：D7~5 模式，D4 过渡方式，D3~0 数值
ALPHA_FCN_MODES = (
    ("0 均匀", 0),
    ("1 顶部", 1),
    ("2 底部", 2),
    ("3 左侧", 3),
    ("4 右侧", 4),
)
ALPHA_TRANSITIONS = (
    ("直达", 0),
    ("匀速过渡", 1),
)


# ============================== 工具函数 ==============================
def now_text():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def wrap_hex(data, per=16):
    if not data:
        return ""
    return "\n".join(" ".join("%02X" % b for b in data[i:i + per])
                     for i in range(0, len(data), per))


def short_hex(data, limit=18):
    text = " ".join("%02X" % b for b in data[:limit])
    if len(data) > limit:
        text += " …"
    return text


def ascii_to_fullwidth(text):
    """把 ASCII 空格和可见字符转换成对应的全角字符

    GB2312 模式下每个字符都按 2 字节处理，混入 1 字节的 ASCII 会破坏
    固件的字宽/寻址计算，所以这里统一转成全角。
    """
    out = []
    for ch in text:
        code = ord(ch)
        if code == 0x20:
            out.append("\u3000")          # 半角空格 -> 全角空格
        elif 0x21 <= code <= 0x7E:
            out.append(chr(code + 0xFEE0))  # ! ~ -> ！ ～
        else:
            out.append(ch)
    return "".join(out)


def _b(v):
    return int(v) & 0xFF


def checksum(data):
    """校验字节：data 所有字节之和的低 8 位"""
    return sum(data) & 0xFF


def build_packet(cmds):
    """把若干条指令封装成一个 0xAB 数据包"""
    cmds = [bytes(c) for c in cmds]
    if not cmds:
        raise ValueError("至少需要一条指令")
    body = b"".join(cmds)
    total = 3 + len(body)
    if total > 0xFF:
        raise ValueError("数据包过长：%d 字节（上限 255）" % total)
    return bytes([PACKET_HEAD, total, len(cmds)]) + body


# ============================== 指令构造 ==============================
def cmd_disable(layer):
    """ElementOperation / Type_Disable(0)"""
    body = bytes([_b(layer) & 0x0F, 0x00])
    return body + bytes([checksum(body)])


def cmd_disable_all():
    """DisableAllLayer(0xA0)：3 个 0xA0，失能全部图层"""
    return bytes([0xA0, 0xA0, 0xA0])


def cmd_low_power():
    """LowPower(0xD0)：3 个 0xD0，进入 STANDBY"""
    return bytes([0xD0, 0xD0, 0xD0])


def cmd_picture(layer, pipeline, item, mix, xpos, xfcn, ypos, yfcn,
                alpha, afcn, picture_add):
    """ElementOperation / Type_Picture(1)"""
    body = bytes([
        _b(layer) & 0x0F, 0x01, _b(pipeline), _b(item), _b(mix),
        _b(xpos), _b(xfcn), _b(ypos), _b(yfcn),
        _b(alpha), _b(afcn),
        _b(picture_add >> 8), _b(picture_add),
    ])
    return body + bytes([checksum(body)])


def cmd_char(layer, type_code, pipeline, item, mix, xpos, xfcn, ypos, yfcn,
             alpha, afcn, text_bytes):
    """ElementOperation / Type_ASCIIchr(2) 或 Type_GB2312chr(3)

    布局：0 指令|图层, 1 类型, 2 PipelineFlag, 3 flash项, 4 混合,
          5 X, 6 Xfcn, 7 Y, 8 Yfcn, 9 Alpha, 10 Alfafcn, 11 文本字节数,
          12 校验（前 12 字节之和）, 13.. 文本内容
    """
    text_bytes = bytes(text_bytes)
    body = bytes([
        _b(layer) & 0x0F, _b(type_code), _b(pipeline), _b(item), _b(mix),
        _b(xpos), _b(xfcn), _b(ypos), _b(yfcn),
        _b(alpha), _b(afcn), len(text_bytes),
    ])
    return body + bytes([checksum(body)]) + text_bytes


def cmd_block(layer, pipeline, mix, xpos, xfcn, ypos, yfcn, alpha, afcn,
              block_xsize, block_ysize, corners):
    """ElementOperation / Type_DrawBlock(4)"""
    body = bytes([
        _b(layer) & 0x0F, 0x04, _b(pipeline), _b(mix),
        _b(xpos), _b(xfcn), _b(ypos), _b(yfcn),
        _b(alpha), _b(afcn),
        _b(block_xsize), _b(block_ysize), _b(corners),
    ])
    return body + bytes([checksum(body)])


def cmd_layer_base(layer, pipeline, xpos_target, xfcn, ypos_target, yfcn,
                   alpha_target, afcn):
    """LayerBaseValue(0x10)"""
    body = bytes([
        0x10 | (_b(layer) & 0x0F), _b(pipeline),
        _b(xpos_target), _b(xfcn), _b(ypos_target), _b(yfcn),
        _b(alpha_target), _b(afcn),
    ])
    return body + bytes([checksum(body)])


def cmd_stream_set(layer, stream_add, stream_fcn, attributes):
    """StreamSet(0x20)"""
    body = bytes([
        0x20 | (_b(layer) & 0x0F),
        _b(stream_add >> 8), _b(stream_add), _b(stream_fcn),
        _b(attributes >> 8), _b(attributes),
    ])
    return body + bytes([checksum(body)])


def cmd_stream_value(layer, stream_add_target, stream_fcn):
    """StreamValue(0x30)，流地址为 24 位"""
    body = bytes([
        0x30 | (_b(layer) & 0x0F),
        _b(stream_add_target >> 16), _b(stream_add_target >> 8),
        _b(stream_add_target), _b(stream_fcn),
    ])
    return body + bytes([checksum(body)])


def cmd_stream_pause(layer, pause_enable):
    """StreamPause(0x40)：固件里 1=暂停，0=运行"""
    body = bytes([0x40 | (_b(layer) & 0x0F),
                  1 if pause_enable else 0])
    return body + bytes([checksum(body)])


def cmd_block_value(layer, xsize_target, xsize_fcn, ysize_target, ysize_fcn):
    """BlockValue(0x50)"""
    body = bytes([
        0x50 | (_b(layer) & 0x0F),
        _b(xsize_target), _b(xsize_fcn), _b(ysize_target), _b(ysize_fcn),
    ])
    return body + bytes([checksum(body)])


def cmd_frame_clear(enable):
    """FrameClear(0xC0)：3 个字节必须相同，低 4 位非 0 表示置位"""
    v = 0xC0 | (1 if enable else 0)
    return bytes([v, v, v])


def cmd_screen_set(bits):
    """ScreenSet(0xE0)：3 个字节必须相同"""
    v = 0xE0 | (_b(bits) & 0x0F)
    return bytes([v, v, v])


# ============================== 串口层 ==============================
class ProtocolError(Exception):
    pass


class Link(object):
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

    def send(self, packet):
        if not self.is_open:
            raise ProtocolError("串口未打开")
        self.ser.write(packet)
        self.ser.flush()


class TelemetryParser(object):
    """单片机上报帧解析

    帧格式：BackData_Head(1) + 16 图层 × 7 字节 + 性能占用(1) = 114 字节
    每个图层：Xpos、Ypos、Alpha、Block_Xsize、Block_Ysize、StreamAdd 高、StreamAdd 低

    单片机每 20ms 发一帧，一帧约 9.9ms 发完，帧间有约 10ms 空闲。
    因此按接收间隔判断帧边界：超过 5ms 没有新数据，下一次收到的第一个字节
    就当作新帧的开头，然后按 114 字节定长切帧，最后一字节自然对齐到性能占用。
    """

    def __init__(self):
        self.buf = bytearray()
        self.last_rx = None

    def reset(self):
        self.buf = bytearray()
        self.last_rx = None

    @staticmethod
    def _valid_perf(byte):
        if byte == 0xFF:
            return True
        return (byte >> 4) <= 9 and (byte & 0x0F) <= 9

    def _valid_frame(self, frame):
        # 仅用于丢弃开串口时可能截到的半帧，不用它来寻找帧头
        return frame[0] in (0x00, 0xF0, 0xFF) and self._valid_perf(frame[-1])

    def feed(self, data, now=None):
        """喂入串口收到的字节，返回本次解析出的完整帧列表"""
        if now is None:
            now = time.monotonic()
        if self.last_rx is None or now - self.last_rx > TELEMETRY_GAP_S:
            # 空闲超过 5ms，认为上一帧已结束，本次数据从帧头开始
            self.buf = bytearray()
        self.last_rx = now
        self.buf.extend(data)

        frames = []
        while len(self.buf) >= TELEMETRY_FRAME_LEN:
            frame = bytes(self.buf[:TELEMETRY_FRAME_LEN])
            del self.buf[:TELEMETRY_FRAME_LEN]
            if self._valid_frame(frame):
                frames.append(frame)
        return frames

    @staticmethod
    def parse(frame):
        layers = []
        for i in range(E_LAYER_SIZE):
            base = 1 + i * LAYER_BYTES
            xpos = frame[base]
            ypos = frame[base + 1]
            layers.append({
                "x": xpos - 256 if xpos >= 128 else xpos,
                "y": ypos - 256 if ypos >= 128 else ypos,
                "alpha": frame[base + 2],
                "bx": frame[base + 3],
                "by": frame[base + 4],
                "stream": (frame[base + 5] << 8) | frame[base + 6],
            })
        perf = frame[-1]
        if perf == 0xFF:
            perf_value = 100
            perf_text = ">99%"
        else:
            hi, lo = perf >> 4, perf & 0x0F
            if hi <= 9 and lo <= 9:
                perf_value = hi * 10 + lo
                perf_text = "%d%%" % perf_value
            else:
                perf_value = -1
                perf_text = "--"
        return {"head": frame[0], "layers": layers,
                "perf_value": perf_value, "perf_text": perf_text}


# ============================== 控件辅助 ==============================
def spin(min_value, max_value, value, tip=None):
    w = QtWidgets.QSpinBox()
    w.setRange(min_value, max_value)
    w.setValue(value)
    w.setMinimumWidth(130)
    if tip:
        w.setToolTip(tip)
    return w


def layer_combo():
    c = QtWidgets.QComboBox()
    for i in range(E_LAYER_SIZE):
        c.addItem("图层 %d" % i, i)
    c.setMinimumWidth(130)
    return c


def mix_combo():
    c = QtWidgets.QComboBox()
    for text, value in MIX_ITEMS:
        c.addItem(text, value)
    c.setMinimumWidth(130)
    return c


def preview_edit():
    e = QtWidgets.QPlainTextEdit()
    e.setReadOnly(True)
    e.setMaximumHeight(86)
    e.setStyleSheet(
        "font-family: Consolas, 'Cascadia Mono', monospace; font-size: 12px;")
    return e


class FcnField(QtWidgets.QWidget):
    """插值函数输入：模式 + 数值；Alpha 额外带过渡方式

    普通插值（位置/尺寸/流地址）：D7~6 模式，D5~0 数值
    Alpha 插值：D7~5 模式，D4 过渡方式，D3~0 数值
    """

    def __init__(self, alpha=False, parent=None):
        super(FcnField, self).__init__(parent)
        self.alpha = alpha
        lay = QtWidgets.QHBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(6)

        self.mode = QtWidgets.QComboBox()
        for text, value in (ALPHA_FCN_MODES if alpha else XY_FCN_MODES):
            self.mode.addItem(text, value)
        self.mode.setMinimumWidth(100)
        lay.addWidget(self.mode)

        self.trans = None
        if alpha:
            self.trans = QtWidgets.QComboBox()
            for text, value in ALPHA_TRANSITIONS:
                self.trans.addItem(text, value)
            self.trans.setMinimumWidth(100)
            lay.addWidget(self.trans)

        self.value = QtWidgets.QSpinBox()
        self.value.setRange(0, 15 if alpha else 63)
        self.value.setPrefix("值 ")
        self.value.setMinimumWidth(80)
        lay.addWidget(self.value)

    def byte_value(self):
        if self.alpha:
            return (((self.mode.currentData() & 0x07) << 5)
                    | ((self.trans.currentData() & 0x01) << 4)
                    | (self.value.value() & 0x0F))
        return (((self.mode.currentData() & 0x03) << 6)
                | (self.value.value() & 0x3F))


class PipelineField(QtWidgets.QCheckBox):
    """渲染管线标志：目前只有 D7=Alpha 链路"""

    def __init__(self, parent=None):
        super(PipelineField, self).__init__("启用 Alpha 链路（0x80）", parent)
        self.setChecked(True)
        self.setToolTip("PipelineFlag 的 D7：勾选时该图层参与 Alpha 叠加，"
                        "取消勾选可跳过该层的 Alpha 处理")

    def byte_value(self):
        return PIPELINE_ALPHA if self.isChecked() else 0x00


# ============================== 指令页基类 ==============================
class CommandPage(QtWidgets.QWidget):
    title = ""
    hint = ""
    uses_checksum = True

    def __init__(self, main):
        super(CommandPage, self).__init__()
        self.main = main
        self.connected = False
        self._valid = False
        self._watched = []
        self._building = True

        lay = QtWidgets.QVBoxLayout(self)
        hint = QtWidgets.QLabel(self.hint)
        hint.setWordWrap(True)
        lay.addWidget(hint)

        self.form = QtWidgets.QFormLayout()
        lay.addLayout(self.form)
        self._build_fields()

        lay.addWidget(QtWidgets.QLabel("发送数据包："))
        self.preview = preview_edit()
        lay.addWidget(self.preview)

        self.info = QtWidgets.QLabel("-")
        self.info.setWordWrap(True)
        lay.addWidget(self.info)

        row = QtWidgets.QHBoxLayout()
        row.addStretch(1)
        self.btn_add = QtWidgets.QPushButton("加入自动化清单")
        row.addWidget(self.btn_add)
        self.btn_send = QtWidgets.QPushButton("发送该指令")
        row.addWidget(self.btn_send)
        lay.addLayout(row)
        lay.addStretch(1)

        self._building = False
        for widget in self._watched:
            if isinstance(widget, QtWidgets.QSpinBox):
                widget.valueChanged.connect(self.refresh_preview)
            elif isinstance(widget, QtWidgets.QComboBox):
                widget.currentIndexChanged.connect(self.refresh_preview)
            elif isinstance(widget, QtWidgets.QLineEdit):
                widget.textChanged.connect(self.refresh_preview)
            elif isinstance(widget, QtWidgets.QCheckBox):
                widget.toggled.connect(self.refresh_preview)

        self.btn_send.clicked.connect(self.send)
        self.btn_add.clicked.connect(self.add_to_automation)
        self.refresh_preview()

    def _build_fields(self):
        pass

    def build_command(self):
        raise NotImplementedError

    def add_field(self, label, widget):
        self.form.addRow(label, widget)
        return widget

    def watch(self, widget):
        self._watched.append(widget)
        return widget

    def refresh_preview(self):
        if self._building:
            return
        try:
            cmd = self.build_command()
            packet = build_packet([cmd])
        except Exception as e:
            self.preview.setPlainText("参数有误：%s" % e)
            self.info.setText("<font color='%s'>%s</font>" % (COLOR_ERR, e))
            self._valid = False
        else:
            self.preview.setPlainText(wrap_hex(packet))
            if self.uses_checksum:
                self.info.setText("数据包 %d 字节（指令 %d 字节，指令数 1，校验和 0x%02X）"
                                  % (len(packet), len(cmd), cmd[-1]))
            else:
                self.info.setText("数据包 %d 字节（指令 %d 字节，指令数 1，无累加校验）"
                                  % (len(packet), len(cmd)))
            self._valid = True
        self.btn_send.setEnabled(self.connected and self._valid)
        self.btn_add.setEnabled(self._valid)

    def set_connected(self, connected):
        self.connected = connected
        self.btn_send.setEnabled(connected and self._valid)

    def send(self):
        try:
            cmd = self.build_command()
            packet = build_packet([cmd])
        except Exception as e:
            QtWidgets.QMessageBox.warning(self, "参数有误", str(e))
            return
        self.main.send_packet(packet, self.title)

    def add_to_automation(self):
        try:
            cmd = self.build_command()
            packet = build_packet([cmd])
        except Exception as e:
            QtWidgets.QMessageBox.warning(self, "参数有误", str(e))
            return
        self.main.add_automation_item(self.title, packet)


# ============================== 各指令页 ==============================
class DisablePage(CommandPage):
    title = "失能图层"
    hint = ("ElementOperation / Type_Disable(0)：关闭指定图层，"
            "同时把该层的流地址、暂停位、流偏移清零。")

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))

    def build_command(self):
        return cmd_disable(self.layer.currentData())


class DisableAllPage(CommandPage):
    title = "全部失能图层"
    hint = ("DisableAllLayer(0xA0)：发送 3 个 0xA0，"
            "关闭全部 16 个图层，并清空所有图层的流状态。")
    uses_checksum = False

    def _build_fields(self):
        pass

    def build_command(self):
        return cmd_disable_all()


class PicturePage(CommandPage):
    title = "图片"
    hint = ("ElementOperation / Type_Picture(1)：设置图层为图片，"
            "并从 flash 索引项加载画面。X/Y 为有符号位置，Alpha=255 为完全不透明。")

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))
        self.pipeline = self.watch(self.add_field("渲染管线：", PipelineField()))
        self.item = self.watch(self.add_field(
            "Flash 项序号：", spin(0, 255, 0, "对应索引表里的第几项，0 开始")))
        self.mix = self.watch(self.add_field("混合模式：", mix_combo()))
        self.xpos = self.watch(self.add_field(
            "X 位置：", spin(-128, 127, 0, "有符号 int8")))
        self.xf = self.add_field("X 插值函数：", FcnField())
        self.watch(self.xf.mode)
        self.watch(self.xf.value)
        self.ypos = self.watch(self.add_field(
            "Y 位置：", spin(-128, 127, 0, "有符号 int8")))
        self.yf = self.add_field("Y 插值函数：", FcnField())
        self.watch(self.yf.mode)
        self.watch(self.yf.value)
        self.alpha = self.watch(self.add_field(
            "Alpha：", spin(0, 255, 255, "0 全透明，255 完全不透明")))
        self.af = self.add_field("Alpha 插值函数：", FcnField(alpha=True))
        self.watch(self.af.mode)
        self.watch(self.af.trans)
        self.watch(self.af.value)
        self.picture_add = self.watch(self.add_field(
            "图片帧偏移：", spin(0, 65535, 0, "以帧为单位，一帧 = Xsize × Ysize 字节")))

    def build_command(self):
        return cmd_picture(self.layer.currentData(), self.pipeline.byte_value(),
                           self.item.value(), self.mix.currentData(),
                           self.xpos.value(), self.xf.byte_value(),
                           self.ypos.value(), self.yf.byte_value(),
                           self.alpha.value(), self.af.byte_value(),
                           self.picture_add.value())


class TextPageBase(CommandPage):
    type_code = 2
    encoding = "ascii"
    fullwidth_ascii = False
    extra_hint = ""

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))
        self.pipeline = self.watch(self.add_field("渲染管线：", PipelineField()))
        self.item = self.watch(self.add_field(
            "Flash 项序号：", spin(0, 255, 0, "对应索引表里的第几项，0 开始")))
        self.mix = self.watch(self.add_field("混合模式：", mix_combo()))
        self.xpos = self.watch(self.add_field("X 位置：", spin(-128, 127, 0)))
        self.xf = self.add_field("X 插值函数：", FcnField())
        self.watch(self.xf.mode)
        self.watch(self.xf.value)
        self.ypos = self.watch(self.add_field("Y 位置：", spin(-128, 127, 0)))
        self.yf = self.add_field("Y 插值函数：", FcnField())
        self.watch(self.yf.mode)
        self.watch(self.yf.value)
        self.alpha = self.watch(self.add_field("Alpha：", spin(0, 255, 255)))
        self.af = self.add_field("Alpha 插值函数：", FcnField(alpha=True))
        self.watch(self.af.mode)
        self.watch(self.af.trans)
        self.watch(self.af.value)
        self.text = self.watch(self.add_field("文本内容：", QtWidgets.QLineEdit()))
        self.text.setPlaceholderText(self.extra_hint)
        self.text.setMaxLength(TEXT_MAX)

    def encode_text(self):
        raw = self.text.text()
        if self.fullwidth_ascii:
            raw = ascii_to_fullwidth(raw)
        try:
            data = raw.encode(self.encoding)
        except UnicodeEncodeError:
            raise ValueError("文本无法按 %s 编码" % self.encoding.upper())
        if not 1 <= len(data) <= TEXT_MAX:
            raise ValueError("文本编码后必须是 1~%d 字节（当前 %d 字节）"
                             % (TEXT_MAX, len(data)))
        if self.encoding == "gb2312" and len(data) % 2:
            raise ValueError("GB2312 文本编码后必须是偶数个字节")
        return data

    def build_command(self):
        return cmd_char(self.layer.currentData(), self.type_code,
                        self.pipeline.byte_value(),
                        self.item.value(), self.mix.currentData(),
                        self.xpos.value(), self.xf.byte_value(),
                        self.ypos.value(), self.yf.byte_value(),
                        self.alpha.value(), self.af.byte_value(),
                        self.encode_text())


class AsciiPage(TextPageBase):
    title = "ASCII 文字"
    type_code = 2
    encoding = "ascii"
    extra_hint = "1~32 个 ASCII 字符"
    hint = ("ElementOperation / Type_ASCIIchr(2)：按 ASCII 文字加载。"
            "文本字节数放在偏移 11，校验字节（前 12 字节之和）在偏移 12，"
            "文本内容从偏移 13 开始。")


class Gb2312Page(TextPageBase):
    title = "GB2312 文字"
    type_code = 3
    encoding = "gb2312"
    fullwidth_ascii = True
    extra_hint = "GB2312 中文；输入的 ASCII 会自动转为全角，转换后不超过 32 字节"
    hint = ("ElementOperation / Type_GB2312chr(3)：按 GB2312 文字加载，"
            "每个汉字 2 字节。输入的 ASCII 字符会自动转换为全角字符"
            "（A→Ａ、1→１、空格→全角空格），避免与双字节编码混用。")


class BlockPage(CommandPage):
    title = "矩形绘制"
    hint = ("ElementOperation / Type_DrawBlock(4)：设置图层为矩形。"
            "Block_Corners 的 D7=1 为斜角、0 为圆角，D6~0 为圆角/斜角值。")

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))
        self.pipeline = self.watch(self.add_field("渲染管线：", PipelineField()))
        self.mix = self.watch(self.add_field("混合模式：", mix_combo()))
        self.xpos = self.watch(self.add_field("X 位置：", spin(-128, 127, 0)))
        self.xf = self.add_field("X 插值函数：", FcnField())
        self.watch(self.xf.mode)
        self.watch(self.xf.value)
        self.ypos = self.watch(self.add_field("Y 位置：", spin(-128, 127, 0)))
        self.yf = self.add_field("Y 插值函数：", FcnField())
        self.watch(self.yf.mode)
        self.watch(self.yf.value)
        self.alpha = self.watch(self.add_field("Alpha：", spin(0, 255, 255)))
        self.af = self.add_field("Alpha 插值函数：", FcnField(alpha=True))
        self.watch(self.af.mode)
        self.watch(self.af.trans)
        self.watch(self.af.value)
        self.bx = self.watch(self.add_field("矩形 X 大小：", spin(0, 255, 16)))
        self.by = self.watch(self.add_field("矩形 Y 大小：", spin(0, 255, 8)))
        self.corners = self.watch(self.add_field(
            "圆角/斜角：", spin(0, 255, 0, "D7=1 斜角、0 圆角；D6~0 数值")))

    def build_command(self):
        return cmd_block(self.layer.currentData(), self.pipeline.byte_value(),
                         self.mix.currentData(),
                         self.xpos.value(), self.xf.byte_value(),
                         self.ypos.value(), self.yf.byte_value(),
                         self.alpha.value(), self.af.byte_value(),
                         self.bx.value(), self.by.value(), self.corners.value())


class LayerBasePage(CommandPage):
    title = "图层状态"
    hint = ("LayerBaseValue(0x10)：更新图层目标位置与 Alpha。"
            "X/Y 目标为有符号 int8，Alpha 目标 0~255。")

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))
        self.pipeline = self.watch(self.add_field("渲染管线：", PipelineField()))
        self.xt = self.watch(self.add_field("X 目标位置：", spin(-128, 127, 0)))
        self.xf = self.add_field("X 插值函数：", FcnField())
        self.watch(self.xf.mode)
        self.watch(self.xf.value)
        self.yt = self.watch(self.add_field("Y 目标位置：", spin(-128, 127, 0)))
        self.yf = self.add_field("Y 插值函数：", FcnField())
        self.watch(self.yf.mode)
        self.watch(self.yf.value)
        self.at = self.watch(self.add_field("Alpha 目标：", spin(0, 255, 255)))
        self.af = self.add_field("Alpha 插值函数：", FcnField(alpha=True))
        self.watch(self.af.mode)
        self.watch(self.af.trans)
        self.watch(self.af.value)

    def build_command(self):
        return cmd_layer_base(self.layer.currentData(), self.pipeline.byte_value(),
                              self.xt.value(), self.xf.byte_value(),
                              self.yt.value(), self.yf.byte_value(),
                              self.at.value(), self.af.byte_value())


class StreamSetPage(CommandPage):
    title = "流配置"
    hint = ("StreamSet(0x20)：设置流地址、流地址插值函数和流属性。"
            "属性 D15=方向，D14=循环，D13~7=步长，D6~0=冻结值。")

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))
        self.addr = self.watch(self.add_field(
            "流地址：", spin(0, 65535, 0, "同时也作为 StreamAdd_Target")))
        self.fcn = self.add_field("流地址插值函数：", FcnField())
        self.watch(self.fcn.mode)
        self.watch(self.fcn.value)
        self.direction = self.watch(self.add_field("方向 D15：", QtWidgets.QCheckBox("反向")))
        self.circulation = self.watch(self.add_field("循环 D14：", QtWidgets.QCheckBox("循环播放")))
        self.step = self.watch(self.add_field(
            "步长 D13~7：", spin(0, 127, 1, "每帧在流地址上增加的步长")))
        self.freeze = self.watch(self.add_field(
            "冻结值 D6~0：", spin(0, 127, 2, "同一帧保持多少个渲染周期")))

    def build_command(self):
        attrs = ((0x8000 if self.direction.isChecked() else 0)
                 | (0x4000 if self.circulation.isChecked() else 0)
                 | ((self.step.value() & 0x7F) << 7)
                 | (self.freeze.value() & 0x7F))
        return cmd_stream_set(self.layer.currentData(), self.addr.value(),
                              self.fcn.byte_value(), attrs)


class StreamValuePage(CommandPage):
    title = "流地址更新"
    hint = ("StreamValue(0x30)：更新流的 24 位目标地址，"
            "用于让流沿指定方向插值移动。")

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))
        self.addr = self.watch(self.add_field(
            "流目标地址(24 位)：", spin(0, 0xFFFFFF, 0)))
        self.fcn = self.add_field("流地址插值函数：", FcnField())
        self.watch(self.fcn.mode)
        self.watch(self.fcn.value)

    def build_command(self):
        return cmd_stream_value(self.layer.currentData(), self.addr.value(),
                                self.fcn.byte_value())


class StreamPausePage(CommandPage):
    title = "流暂停/启动"
    hint = ("StreamPause(0x40)：固件里写 1 会置暂停位 0x80（暂停），"
            "写 0 会清除暂停位（运行）。")

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))
        self.pause = self.watch(self.add_field("状态：", QtWidgets.QComboBox()))
        self.pause.addItem("运行（0）", 0)
        self.pause.addItem("暂停（1）", 1)
        self.pause.setMinimumWidth(130)

    def build_command(self):
        return cmd_stream_pause(self.layer.currentData(),
                                self.pause.currentData())


class BlockValuePage(CommandPage):
    title = "矩形尺寸"
    hint = "BlockValue(0x50)：更新矩形图层的目标 X/Y 尺寸及插值函数。"

    def _build_fields(self):
        self.layer = self.watch(self.add_field("图层：", layer_combo()))
        self.bx = self.watch(self.add_field("X 尺寸目标：", spin(0, 255, 16)))
        self.bxf = self.add_field("X 尺寸插值函数：", FcnField())
        self.watch(self.bxf.mode)
        self.watch(self.bxf.value)
        self.by = self.watch(self.add_field("Y 尺寸目标：", spin(0, 255, 8)))
        self.byf = self.add_field("Y 尺寸插值函数：", FcnField())
        self.watch(self.byf.mode)
        self.watch(self.byf.value)

    def build_command(self):
        return cmd_block_value(self.layer.currentData(), self.bx.value(),
                               self.bxf.byte_value(), self.by.value(),
                               self.byf.byte_value())


class FrameClearPage(CommandPage):
    title = "帧清除"
    uses_checksum = False
    hint = ("FrameClear(0xC0)：3 个字节必须相同。低 4 位非 0 表示每帧刷新前清除帧缓存，"
            "0 表示清除该标志。")

    def _build_fields(self):
        self.enable = self.watch(self.add_field("帧清除：", QtWidgets.QComboBox()))
        self.enable.addItem("不启用（C0 C0 C0）", False)
        self.enable.addItem("启用（C1 C1 C1）", True)
        self.enable.setMinimumWidth(180)

    def build_command(self):
        return cmd_frame_clear(self.enable.currentData())


class ScreenSetPage(CommandPage):
    title = "屏幕设置"
    uses_checksum = False
    hint = ("ScreenSet(0xE0)：3 个字节必须相同。"
            "D3=显示使能，D2=X 正常，D1=Y 正常，D0=颜色正常；"
            "对应位为 0 时固件会设为镜像/反色。")

    def _build_fields(self):
        self.show = self.watch(self.add_field("显示使能 D3：",
                                              QtWidgets.QCheckBox("显示")))
        self.xnormal = self.watch(self.add_field("X 正常 D2：",
                                                 QtWidgets.QCheckBox("不镜像")))
        self.ynormal = self.watch(self.add_field("Y 正常 D1：",
                                                 QtWidgets.QCheckBox("不镜像")))
        self.cnormal = self.watch(self.add_field("颜色正常 D0：",
                                                 QtWidgets.QCheckBox("不反色")))
        for w in (self.show, self.xnormal, self.ynormal, self.cnormal):
            w.setChecked(True)

    def build_command(self):
        bits = 0
        if self.show.isChecked():
            bits |= 0x08
        if self.xnormal.isChecked():
            bits |= 0x04
        if self.ynormal.isChecked():
            bits |= 0x02
        if self.cnormal.isChecked():
            bits |= 0x01
        return cmd_screen_set(bits)


class LowPowerPage(CommandPage):
    title = "低功耗"
    uses_checksum = False
    hint = ("LowPower(0xD0)：发送 3 个 0xD0，单片机进入 STANDBY。"
            "进入后渲染回传停止，需要给 PA0 一个上升沿（或复位）才能唤醒；"
            "唤醒后程序从头启动，图层配置会清空。")

    def _build_fields(self):
        pass

    def build_command(self):
        return cmd_low_power()

    def send(self):
        answer = QtWidgets.QMessageBox.question(
            self, "进入低功耗",
            "确定让设备进入 STANDBY 吗？\n"
            "进入后串口回传会停止，需要 PA0 上升沿或复位才能唤醒。",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No)
        if answer != QtWidgets.QMessageBox.Yes:
            return
        super(LowPowerPage, self).send()


PAGE_CLASSES = (
    DisablePage, DisableAllPage, PicturePage, AsciiPage, Gb2312Page, BlockPage,
    LayerBasePage, StreamSetPage, StreamValuePage, StreamPausePage,
    BlockValuePage, FrameClearPage, ScreenSetPage, LowPowerPage,
)


# ============================== 主窗口 ==============================
class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super(MainWindow, self).__init__()
        self.link = Link()
        self.pages = []
        self.automation = []
        self.auto_running = False
        self.auto_index = 0
        self._auto_rebuilding = False
        self.auto_timer = QtCore.QTimer(self)
        self.auto_timer.setSingleShot(True)
        self.auto_timer.timeout.connect(self._run_auto_step)
        self.parser = TelemetryParser()
        self.tele_frames = 0
        self.tele_latest = None
        self.tele_last_view = 0.0
        self.tele_open_time = 0.0
        self.tele_timer = QtCore.QTimer(self)
        self.tele_timer.setInterval(2)
        self.tele_timer.timeout.connect(self.poll_telemetry)
        self.setWindowTitle("YUERAN 渲染指令基础指令构造器")
        self._apply_geometry()

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        root = QtWidgets.QVBoxLayout(central)
        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(8)

        bar = QtWidgets.QHBoxLayout()
        bar.addWidget(QtWidgets.QLabel("串口："))
        self.port_combo = QtWidgets.QComboBox()
        self.port_combo.setMinimumWidth(230)
        bar.addWidget(self.port_combo)
        self.btn_scan = QtWidgets.QPushButton("刷新串口列表")
        bar.addWidget(self.btn_scan)
        self.btn_port = QtWidgets.QPushButton("打开串口")
        bar.addWidget(self.btn_port)
        bar.addSpacing(14)
        self.lbl_conn = QtWidgets.QLabel("未连接")
        self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_ERR)
        bar.addWidget(self.lbl_conn)
        bar.addStretch(1)
        bar.addWidget(QtWidgets.QLabel("渲染 USART1 · 115200 8-N-1"))
        root.addLayout(bar)

        note = QtWidgets.QLabel(
            "说明：渲染模式没有逐条应答，单片机会持续通过 USART1 上报图层状态；"
            "带校验的指令末字节为前面所有字节之和的低 8 位，"
            "帧清除/屏幕设置则要求 3 个字节相同。")
        note.setWordWrap(True)
        note.setStyleSheet("color:#555;")
        root.addWidget(note)

        self.tabs = QtWidgets.QTabWidget()
        for cls in PAGE_CLASSES:
            page = cls(self)
            self.pages.append(page)
            scroll = QtWidgets.QScrollArea()
            scroll.setWidgetResizable(True)
            scroll.setFrameShape(QtWidgets.QFrame.NoFrame)
            scroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
            scroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
            scroll.setWidget(page)
            self.tabs.addTab(scroll, cls.title)

        self.auto_page = self._build_automation_page()
        self.tabs.addTab(self.auto_page, "自动化清单")

        tele_box = self._build_telemetry_box()
        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        splitter.addWidget(self.tabs)
        splitter.addWidget(tele_box)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        splitter.setSizes([760, 420])
        root.addWidget(splitter, 3)

        log_box = QtWidgets.QGroupBox("发送记录")
        ll = QtWidgets.QVBoxLayout(log_box)
        row = QtWidgets.QHBoxLayout()
        row.addStretch(1)
        self.btn_copy_log = QtWidgets.QPushButton("复制全部")
        self.btn_clear_log = QtWidgets.QPushButton("清空")
        row.addWidget(self.btn_copy_log)
        row.addWidget(self.btn_clear_log)
        ll.addLayout(row)
        self.log_edit = QtWidgets.QPlainTextEdit()
        self.log_edit.setReadOnly(True)
        self.log_edit.setMaximumBlockCount(20000)
        self.log_edit.setStyleSheet(
            "font-family: Consolas, 'Cascadia Mono', monospace; font-size: 12px;")
        ll.addWidget(self.log_edit)
        log_box.setMinimumHeight(150)
        root.addWidget(log_box, 1)

        self.btn_scan.clicked.connect(self.refresh_ports)
        self.btn_port.clicked.connect(self.toggle_port)
        self.btn_copy_log.clicked.connect(self.copy_log)
        self.btn_clear_log.clicked.connect(self.log_edit.clear)

        self.refresh_ports()
        self.update_buttons()
        self.append_log("YUERAN 渲染指令构造器已启动：选择串口并打开后即可发送指令")

    # ---------- 窗口 ----------
    def _apply_geometry(self):
        """初始大小取屏幕可用区域的 2/3，并居中显示"""
        screen = QtWidgets.QApplication.primaryScreen()
        geo = (screen.availableGeometry() if screen is not None
               else QtCore.QRect(0, 0, 1280, 800))
        w = min(geo.width(), max(640, int(geo.width() * 2 // 3)))
        h = min(geo.height(), max(480, int(geo.height() * 2 // 3)))
        self.setGeometry(geo.x() + (geo.width() - w) // 2,
                         geo.y() + (geo.height() - h) // 2, w, h)
        self.setMinimumSize(min(w, 640), min(h, 480))

    # ---------- 单片机回传 ----------
    def _build_telemetry_box(self):
        box = QtWidgets.QGroupBox("单片机回传（%d 图层）" % E_LAYER_SIZE)
        lay = QtWidgets.QVBoxLayout(box)

        self.telem_status = QtWidgets.QLabel("串口未打开")
        self.telem_status.setWordWrap(True)
        lay.addWidget(self.telem_status)

        self.tele_table = QtWidgets.QTableWidget(E_LAYER_SIZE, 7)
        self.tele_table.setHorizontalHeaderLabels(
            ["图层", "X", "Y", "Alpha", "块宽X", "块高Y", "流地址"])
        self.tele_table.verticalHeader().setVisible(False)
        self.tele_table.verticalHeader().setDefaultSectionSize(20)
        self.tele_table.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self.tele_table.setSelectionMode(QtWidgets.QAbstractItemView.NoSelection)
        self.tele_table.setFocusPolicy(QtCore.Qt.NoFocus)
        self.tele_table.horizontalHeader().setSectionResizeMode(
            QtWidgets.QHeaderView.Stretch)
        for row in range(E_LAYER_SIZE):
            for col in range(7):
                item = QtWidgets.QTableWidgetItem("")
                item.setTextAlignment(QtCore.Qt.AlignCenter)
                if col == 0:
                    item.setText(str(row))
                    item.setForeground(QtGui.QColor("#555555"))
                self.tele_table.setItem(row, col, item)
        self.tele_table.setMinimumHeight(320)
        lay.addWidget(self.tele_table, 1)
        return box

    def poll_telemetry(self):
        if not self.link.is_open:
            return
        try:
            now = time.monotonic()
            waiting = self.link.ser.in_waiting
            if waiting:
                frames = self.parser.feed(self.link.ser.read(waiting), now)
                if frames:
                    self.tele_frames += len(frames)
                    self.tele_latest = frames[-1]
            if self.tele_latest is not None:
                if now - self.tele_last_view >= 0.1:
                    self.tele_last_view = now
                    self.update_telemetry_view(self.tele_latest)
            elif now - self.tele_open_time > 1.0:
                self.telem_status.setText(
                    "未收到单片机回传：请确认设备处于渲染模式、"
                    "串口接在 USART1、波特率为 115200")
        except Exception as e:
            self.telem_status.setText("回传数据解析失败：%s" % e)

    def _set_tele_cell(self, row, col, text):
        item = self.tele_table.item(row, col)
        if item is not None and item.text() != text:
            item.setText(text)

    def update_telemetry_view(self, frame):
        data = TelemetryParser.parse(frame)
        head = data["head"]
        head_text = {0xFF: "正常", 0x00: "指令执行成功", 0xF0: "指令被拒绝"}
        self.telem_status.setText(
            "回传正常：帧数 %d，性能占用 %s，BackData_Head=0x%02X（%s）"
            % (self.tele_frames, data["perf_text"], head,
               head_text.get(head, "未知")))
        for i, layer in enumerate(data["layers"]):
            self._set_tele_cell(i, 1, str(layer["x"]))
            self._set_tele_cell(i, 2, str(layer["y"]))
            self._set_tele_cell(i, 3, str(layer["alpha"]))
            self._set_tele_cell(i, 4, str(layer["bx"]))
            self._set_tele_cell(i, 5, str(layer["by"]))
            self._set_tele_cell(i, 6, str(layer["stream"]))

    # ---------- 自动化清单 ----------
    def _build_automation_page(self):
        page = QtWidgets.QWidget()
        lay = QtWidgets.QVBoxLayout(page)
        hint = QtWidgets.QLabel(
            "在任意指令页点击“加入自动化清单”，即可把当前参数生成的指令按顺序加入。"
            "“延时”表示这条指令发送后等待多久再发下一条，双击表格中的延时即可修改。")
        hint.setWordWrap(True)
        lay.addWidget(hint)

        self.auto_table = QtWidgets.QTableWidget(0, 5)
        self.auto_table.setHorizontalHeaderLabels(
            ["序号", "指令", "长度", "延时(ms)", "数据包"])
        self.auto_table.verticalHeader().setVisible(False)
        self.auto_table.verticalHeader().setDefaultSectionSize(22)
        self.auto_table.setSelectionBehavior(
            QtWidgets.QAbstractItemView.SelectRows)
        self.auto_table.setSelectionMode(
            QtWidgets.QAbstractItemView.ExtendedSelection)
        self.auto_table.setEditTriggers(
            QtWidgets.QAbstractItemView.DoubleClicked
            | QtWidgets.QAbstractItemView.SelectedClicked
            | QtWidgets.QAbstractItemView.EditKeyPressed)
        header = self.auto_table.horizontalHeader()
        header.setSectionResizeMode(0, QtWidgets.QHeaderView.ResizeToContents)
        header.setSectionResizeMode(1, QtWidgets.QHeaderView.ResizeToContents)
        header.setSectionResizeMode(2, QtWidgets.QHeaderView.ResizeToContents)
        header.setSectionResizeMode(3, QtWidgets.QHeaderView.ResizeToContents)
        header.setSectionResizeMode(4, QtWidgets.QHeaderView.Stretch)
        self.auto_table.itemChanged.connect(self.on_auto_item_changed)
        lay.addWidget(self.auto_table, 1)

        self.auto_status = QtWidgets.QLabel("清单为空")
        self.auto_status.setWordWrap(True)
        lay.addWidget(self.auto_status)

        row = QtWidgets.QHBoxLayout()
        self.btn_auto_up = QtWidgets.QPushButton("上移")
        self.btn_auto_down = QtWidgets.QPushButton("下移")
        self.btn_auto_del = QtWidgets.QPushButton("删除选中")
        self.btn_auto_clear = QtWidgets.QPushButton("清空")
        row.addWidget(self.btn_auto_up)
        row.addWidget(self.btn_auto_down)
        row.addWidget(self.btn_auto_del)
        row.addWidget(self.btn_auto_clear)
        self.btn_auto_import = QtWidgets.QPushButton("导入清单")
        self.btn_auto_export = QtWidgets.QPushButton("导出清单")
        row.addWidget(self.btn_auto_import)
        row.addWidget(self.btn_auto_export)
        row.addStretch(1)
        self.btn_auto_run = QtWidgets.QPushButton("执行清单")
        self.btn_auto_stop = QtWidgets.QPushButton("停止")
        row.addWidget(self.btn_auto_run)
        row.addWidget(self.btn_auto_stop)
        lay.addLayout(row)

        self.btn_auto_up.clicked.connect(lambda: self.move_auto(-1))
        self.btn_auto_down.clicked.connect(lambda: self.move_auto(1))
        self.btn_auto_del.clicked.connect(self.remove_auto_selected)
        self.btn_auto_clear.clicked.connect(self.clear_automation)
        self.btn_auto_import.clicked.connect(self.import_automation)
        self.btn_auto_export.clicked.connect(self.export_automation)
        self.btn_auto_run.clicked.connect(self.start_automation)
        self.btn_auto_stop.clicked.connect(self.stop_automation)
        return page

    def add_automation_item(self, title, packet):
        self.automation.append({"title": title, "packet": packet, "delay": 100})
        self.rebuild_auto_table(len(self.automation) - 1)
        self.append_log("已加入自动化清单：%s（%d 字节，延时 100 ms）"
                        % (title, len(packet)))

    def rebuild_auto_table(self, select_row=None):
        self._auto_rebuilding = True
        self.auto_table.setRowCount(len(self.automation))
        for row, entry in enumerate(self.automation):
            values = (str(row + 1), entry["title"], str(len(entry["packet"])),
                      str(entry["delay"]), short_hex(entry["packet"]))
            for col, text in enumerate(values):
                cell = QtWidgets.QTableWidgetItem(text)
                cell.setTextAlignment(QtCore.Qt.AlignCenter)
                if col == 3:
                    cell.setToolTip("这条指令发送后等待多少毫秒再发下一条，双击修改")
                else:
                    cell.setFlags(cell.flags() & ~QtCore.Qt.ItemIsEditable)
                    if col == 4:
                        cell.setToolTip(wrap_hex(entry["packet"]))
                self.auto_table.setItem(row, col, cell)
        self._auto_rebuilding = False
        if select_row is not None and 0 <= select_row < len(self.automation):
            self.auto_table.selectRow(select_row)
        self.update_auto_status()
        self.update_auto_buttons()

    def on_auto_item_changed(self, item):
        if self._auto_rebuilding:
            return
        row, col = item.row(), item.column()
        if col != 3 or row < 0 or row >= len(self.automation):
            return
        try:
            value = int(item.text().strip())
        except ValueError:
            value = self.automation[row]["delay"]
        value = max(0, min(60000, value))
        self.automation[row]["delay"] = value
        self._auto_rebuilding = True
        item.setText(str(value))
        self._auto_rebuilding = False
        self.update_auto_status()

    def update_auto_status(self):
        if self.auto_running:
            return
        if not self.automation:
            self.auto_status.setText("清单为空：请在指令页点击“加入自动化清单”")
            return
        total = sum(entry["delay"] for entry in self.automation[:-1])
        self.auto_status.setText(
            "清单共 %d 条；发送完一条后按各自延时等待，指令间等待合计 %d ms"
            % (len(self.automation), total))

    def update_auto_buttons(self):
        running = self.auto_running
        has = bool(self.automation)
        self.btn_auto_run.setEnabled(self.link.is_open and has and not running)
        self.btn_auto_stop.setEnabled(running)
        self.btn_auto_up.setEnabled(has and not running)
        self.btn_auto_down.setEnabled(has and not running)
        self.btn_auto_del.setEnabled(has and not running)
        self.btn_auto_clear.setEnabled(has and not running)
        self.btn_auto_export.setEnabled(has and not running)
        self.btn_auto_import.setEnabled(not running)
        self.auto_table.setEnabled(not running)

    def auto_selected_rows(self):
        rows = sorted({index.row() for index in self.auto_table.selectedIndexes()})
        if not rows:
            row = self.auto_table.currentRow()
            if row >= 0:
                rows = [row]
        return rows

    def move_auto(self, delta):
        rows = self.auto_selected_rows()
        if len(rows) != 1:
            return
        row = rows[0]
        target = row + delta
        if target < 0 or target >= len(self.automation):
            return
        self.automation[row], self.automation[target] = \
            self.automation[target], self.automation[row]
        self.rebuild_auto_table(target)

    def remove_auto_selected(self):
        rows = self.auto_selected_rows()
        if not rows:
            return
        for row in reversed(rows):
            del self.automation[row]
        select = min(rows[0], len(self.automation) - 1)
        self.rebuild_auto_table(select if self.automation else None)

    def clear_automation(self):
        if not self.automation:
            return
        answer = QtWidgets.QMessageBox.question(
            self, "清空自动化清单",
            "确定要清空全部 %d 条指令吗？" % len(self.automation))
        if answer != QtWidgets.QMessageBox.Yes:
            return
        self.automation = []
        self.rebuild_auto_table()
        self.append_log("自动化清单已清空")

    def _sync_auto_delays(self):
        """把表格里已提交的延时写回清单"""
        for row in range(min(self.auto_table.rowCount(), len(self.automation))):
            cell = self.auto_table.item(row, 3)
            if cell is None:
                continue
            try:
                value = int(cell.text().strip())
            except ValueError:
                continue
            self.automation[row]["delay"] = max(0, min(60000, value))

    def _commit_auto_edit(self):
        self.auto_table.clearFocus()
        QtWidgets.QApplication.processEvents()
        self._sync_auto_delays()

    def export_automation(self):
        if not self.automation:
            QtWidgets.QMessageBox.information(self, "提示", "自动化清单为空")
            return
        self._commit_auto_edit()
        default = ("YUERAN渲染自动化清单_%s.json"
                   % datetime.now().strftime("%Y%m%d_%H%M%S"))
        path, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "导出自动化清单", default,
            "JSON 文件 (*.json);;所有文件 (*.*)")
        if not path:
            return
        if not path.lower().endswith(".json"):
            path += ".json"

        data = {
            "app": "YUERAN渲染指令构造器",
            "version": 1,
            "saved_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "items": [
                {
                    "title": entry["title"],
                    "length": len(entry["packet"]),
                    "delay": entry["delay"],
                    "packet": entry["packet"].hex().upper(),
                }
                for entry in self.automation
            ],
        }
        try:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "导出失败", str(e))
            return
        self.append_log("自动化清单已导出：%s（%d 条）"
                        % (path, len(self.automation)))

    def import_automation(self):
        path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "导入自动化清单", "",
            "JSON 文件 (*.json);;所有文件 (*.*)")
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8-sig") as f:
                data = json.load(f)
        except Exception as e:
            QtWidgets.QMessageBox.critical(
                self, "导入失败", "读取 JSON 失败：%s" % e)
            return

        items = data.get("items") if isinstance(data, dict) else data
        if not isinstance(items, list) or not items:
            QtWidgets.QMessageBox.critical(
                self, "导入失败", "文件里没有有效的 items 清单")
            return

        imported = []
        for index, raw in enumerate(items):
            try:
                title = str(raw.get("title", "指令"))
                delay = max(0, min(60000, int(raw.get("delay", 100))))
                hex_text = str(raw.get("packet", ""))
                hex_text = hex_text.replace(" ", "").replace("\n", "").replace("\r", "")
                packet = bytes.fromhex(hex_text)
            except Exception as e:
                QtWidgets.QMessageBox.critical(
                    self, "导入失败",
                    "第 %d 条数据格式有误：%s" % (index + 1, e))
                return
            if (len(packet) < 3 or packet[0] != PACKET_HEAD
                    or packet[1] != len(packet) or packet[2] < 1):
                QtWidgets.QMessageBox.critical(
                    self, "导入失败",
                    "第 %d 条 packet 不是有效的渲染数据包" % (index + 1))
                return
            imported.append({"title": title, "packet": packet, "delay": delay})

        if self.automation:
            box = QtWidgets.QMessageBox(self)
            box.setWindowTitle("导入自动化清单")
            box.setText("当前清单已有 %d 条，导入文件有 %d 条。"
                        % (len(self.automation), len(imported)))
            btn_replace = box.addButton(
                "替换当前清单", QtWidgets.QMessageBox.AcceptRole)
            btn_append = box.addButton(
                "追加到当前清单", QtWidgets.QMessageBox.ActionRole)
            box.addButton("取消", QtWidgets.QMessageBox.RejectRole)
            box.exec_()
            if box.clickedButton() == btn_replace:
                self.automation = imported
                action = "替换"
            elif box.clickedButton() == btn_append:
                self.automation.extend(imported)
                action = "追加"
            else:
                return
        else:
            self.automation = imported
            action = "导入"

        self.rebuild_auto_table(0)
        self.append_log("自动化清单已从 %s %s：共 %d 条"
                        % (path, action, len(imported)))

    def start_automation(self):
        if not self.link.is_open:
            QtWidgets.QMessageBox.warning(self, "提示", "请先打开串口")
            return
        if not self.automation:
            QtWidgets.QMessageBox.warning(self, "提示", "自动化清单为空")
            return
        self.auto_running = True
        self.auto_index = 0
        self.update_auto_buttons()
        self.auto_status.setText("开始执行…")
        self._run_auto_step()

    def _run_auto_step(self):
        if not self.auto_running:
            return
        if self.auto_index >= len(self.automation):
            self._finish_automation("执行完成")
            return
        entry = self.automation[self.auto_index]
        try:
            self.link.send(entry["packet"])
        except Exception as e:
            self._finish_automation("发送失败：%s" % e)
            return
        self.auto_index += 1
        self.append_log("[%s] [自动化 %d/%d] TX [%s] %s"
                        % (now_text(), self.auto_index, len(self.automation),
                           entry["title"], wrap_hex(entry["packet"])))
        if self.auto_index >= len(self.automation):
            self._finish_automation("执行完成")
            return
        delay = max(0, min(60000, entry["delay"]))
        self.auto_status.setText(
            "执行中：已发送 %d/%d（%s），等待 %d ms 后发送下一条"
            % (self.auto_index, len(self.automation), entry["title"], delay))
        self.auto_timer.start(delay)

    def _finish_automation(self, text):
        self.auto_running = False
        self.auto_timer.stop()
        self.auto_status.setText(text)
        self.append_log("[%s] 自动化：%s" % (now_text(), text))
        self.update_auto_buttons()

    def stop_automation(self):
        if not self.auto_running:
            return
        self._finish_automation("已停止（已发送 %d/%d）"
                                % (self.auto_index, len(self.automation)))

    # ---------- 串口 ----------
    def refresh_ports(self):
        self.port_combo.clear()
        ports = list(list_ports.comports())
        for p in ports:
            self.port_combo.addItem("%s  %s" % (p.device, p.description or ""),
                                    p.device)
        if not ports:
            self.port_combo.addItem("（未发现串口）", None)
        self.append_log("串口列表已刷新，发现 %d 个串口" % len(ports))
        self.update_buttons()

    def toggle_port(self):
        if self.link.is_open:
            self.stop_automation()
            self.link.close()
            self.tele_timer.stop()
            self.parser.reset()
            self.telem_status.setText("串口已关闭")
            self.append_log("串口已关闭")
        else:
            port = self.port_combo.currentData()
            if not port:
                QtWidgets.QMessageBox.warning(self, "提示", "没有可用的串口")
                return
            try:
                self.link.open(port)
            except Exception as e:
                QtWidgets.QMessageBox.critical(self, "打开串口失败", str(e))
                return
            self.parser.reset()
            self.tele_frames = 0
            self.tele_latest = None
            self.tele_last_view = 0.0
            self.tele_open_time = time.monotonic()
            self.telem_status.setText("等待单片机回传…")
            self.tele_timer.start()
            self.append_log("串口已打开：%s（115200 8-N-1，渲染模式）" % port)
        self.update_buttons()

    def update_buttons(self):
        connected = self.link.is_open
        self.port_combo.setEnabled(not connected)
        self.btn_scan.setEnabled(not connected)
        self.btn_port.setText("关闭串口" if connected else "打开串口")
        if connected:
            self.lbl_conn.setText("已连接：%s" % self.port_combo.currentData())
            self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_OK)
        else:
            self.lbl_conn.setText("未连接")
            self.lbl_conn.setStyleSheet("color:%s; font-weight:bold;" % COLOR_ERR)
        for page in self.pages:
            page.set_connected(connected)
        self.update_auto_buttons()

    # ---------- 发送与日志 ----------
    def send_packet(self, packet, title):
        if not self.link.is_open:
            QtWidgets.QMessageBox.warning(self, "提示", "请先打开串口")
            return
        try:
            self.link.send(packet)
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "发送失败", str(e))
            return
        self.append_log("[%s] TX [%s] %s" % (now_text(), title, wrap_hex(packet)))

    def append_log(self, text):
        self.log_edit.appendPlainText(text)
        bar = self.log_edit.verticalScrollBar()
        bar.setValue(bar.maximum())

    def copy_log(self):
        QtWidgets.QApplication.clipboard().setText(self.log_edit.toPlainText())
        self.append_log("发送记录已复制到剪贴板")


def main():
    QtCore.QCoreApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
    QtCore.QCoreApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("YUERAN渲染指令构造器")
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
