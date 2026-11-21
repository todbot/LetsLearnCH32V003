# i2c_oled.py -- CircuitPython driver for 90_i2c_oled
# 20 Nov 2026 - @todbot / Tod Kurt

_WHO_AM_I, _FW_VERSION, _STATUS = 0x00, 0x01, 0x02
_CONFIG, _CONTRAST, _COMMAND = 0x04, 0x05, 0x06
_WHO_AM_I_VAL = 0xC4
_STATUS_BUSY = 0x01
_ARGS_LEN = 40

CONFIG_AUTOSHOW = 0x02

_CMD_SHOW, _CMD_CLEAR, _CMD_PIXEL, _CMD_LINE = 0x01, 0x10, 0x11, 0x12
_CMD_RECT, _CMD_FILLRECT, _CMD_CIRCLE, _CMD_FILLCIRCLE = 0x13, 0x14, 0x15, 0x16
_CMD_PRINT, _CMD_RESET = 0x17, 0xA5

class I2COLED:
    width, height = 128, 64

    def __init__(self, i2c, addr=0x55):
        self.i2c, self.addr = i2c, addr
        self._buf1 = bytearray(1)
        if self._read(_WHO_AM_I) != _WHO_AM_I_VAL:
            raise RuntimeError("i2c_oled not found at 0x%02x" % addr)

    def _read(self, reg):
        while not self.i2c.try_lock(): pass
        try:
            self.i2c.writeto_then_readfrom(self.addr, bytes([reg]), self._buf1)
        finally:
            self.i2c.unlock()
        return self._buf1[0]

    def _write(self, reg, data):
        while not self.i2c.try_lock(): pass
        try:
            self.i2c.writeto(self.addr, bytes([reg]) + bytes(data))
        finally:
            self.i2c.unlock()

    def _cmd(self, cmd, *args):
        # firmware drops a command sent while it's still busy with the last one
        while self._read(_STATUS) & _STATUS_BUSY: pass
        self._write(_COMMAND, bytes([cmd]) + bytes(args))

    @property
    def version(self):
        return self._read(_FW_VERSION)

    @property
    def config(self):
        return self._read(_CONFIG)

    @config.setter
    def config(self, val):
        self._write(_CONFIG, [val])

    @property
    def contrast(self):
        return self._read(_CONTRAST)

    @contrast.setter
    def contrast(self, val):
        self._write(_CONTRAST, [val])

    def show(self):
        """send the framebuffer to the OLED"""
        self._cmd(_CMD_SHOW)

    def clear(self, color=0):
        self._cmd(_CMD_CLEAR, color)

    def pixel(self, x, y, color=1):
        self._cmd(_CMD_PIXEL, x, y, color)

    def line(self, x0, y0, x1, y1, color=1):
        self._cmd(_CMD_LINE, x0, y0, x1, y1, color)

    def rect(self, x, y, w, h, color=1):
        self._cmd(_CMD_RECT, x, y, w, h, color)

    def fill_rect(self, x, y, w, h, color=1):
        self._cmd(_CMD_FILLRECT, x, y, w, h, color)

    def circle(self, x, y, r, color=1):
        self._cmd(_CMD_CIRCLE, x, y, r, color)

    def fill_circle(self, x, y, r, color=1):
        self._cmd(_CMD_FILLCIRCLE, x, y, r, color)

    def print(self, x, y, text, size=1, color=1):
        """8x8 font, size scales it 1-8x. text over 36 chars is cut"""
        self._cmd(_CMD_PRINT, x, y, size, color, *text.encode()[:_ARGS_LEN - 4])

    def reset(self):
        self._cmd(_CMD_RESET)
