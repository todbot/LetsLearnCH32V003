# touchwheelqwiic.py -- CircuitPython/MicroPython driver for 60_touchwheel
# 14 Nov 2026 - @todbot / Tod Kurt
import struct

_WHO_AM_I, _TOUCH_STATUS, _TOUCH_EVENTS = 0x00, 0x02, 0x03
_CONFIG, _BRIGHTNESS, _COMMAND, _WHEEL_POS = 0x04, 0x05, 0x06, 0x07
_TOUCH_RAW, _TOUCH_BASELINE, _TOUCH_THRESH, _LEDS = 0x08, 0x10, 0x18, 0x20
_CMD_SHOW, _CMD_RECALIBRATE = 0x01, 0x02
_WHO_AM_I_VAL = 0xC3

CONFIG_INT_ENABLE = 0x01
CONFIG_AUTOSHOW = 0x02
CONFIG_FEEDBACK = 0x04
CONFIG_STATUS_LED = 0x08

class Touchwheel:
    def __init__(self, i2c, addr=0x54, num_pads=4, num_leds=8):
        self.i2c, self.addr = i2c, addr
        self.num_pads, self.num_leds = num_pads, num_leds
        self._buf1 = bytearray(1)
        if self._read(_WHO_AM_I, 1)[0] != _WHO_AM_I_VAL:
            raise RuntimeError("touchwheel not found at 0x%02x" % addr)

    def _read(self, reg, n):
        buf = self._buf1 if n == 1 else bytearray(n)
        while not self.i2c.try_lock(): pass
        try:
            self.i2c.writeto_then_readfrom(self.addr, bytes([reg]), buf)
        finally:
            self.i2c.unlock()
        return buf

    def _write(self, reg, data):
        while not self.i2c.try_lock(): pass
        try:
            self.i2c.writeto(self.addr, bytes([reg]) + bytes(data))
        finally:
            self.i2c.unlock()

    def position(self):
        """0 = not touched, else 1-255 around the wheel"""
        return self._read(_WHEEL_POS, 1)[0]

    def touches(self):
        """bitmask, bit n = pad n touched"""
        return self._read(_TOUCH_STATUS, 1)[0]

    def events(self, clear=True):
        """bitmask of pads pressed since last clear"""
        ev = self._read(_TOUCH_EVENTS, 1)[0]
        if clear and ev:
            self._write(_TOUCH_EVENTS, [ev])
        return ev

    def raws(self):
        return struct.unpack("<%dH" % self.num_pads, self._read(_TOUCH_RAW, self.num_pads*2))

    def baselines(self):
        return struct.unpack("<%dH" % self.num_pads, self._read(_TOUCH_BASELINE, self.num_pads*2))

    def set_thresholds(self, threshs):
        self._write(_TOUCH_THRESH, struct.pack("<%dH" % len(threshs), *threshs))

    def recalibrate(self):
        self._write(_COMMAND, [_CMD_RECALIBRATE])

    @property
    def config(self):
        return self._read(_CONFIG, 1)[0]

    @config.setter
    def config(self, val):
        self._write(_CONFIG, [val])

    @property
    def brightness(self):
        return self._read(_BRIGHTNESS, 1)[0]

    @brightness.setter
    def brightness(self, val):
        self._write(_BRIGHTNESS, [val])

    def rgb(self, r, g, b):
        """set all LEDs to one color"""
        self._write(_LEDS, bytes((r, g, b)) * self.num_leds)

    def rgbs(self, colors):
        """set LEDs from list of (r,g,b)"""
        self._write(_LEDS, bytes(c for rgb in colors for c in rgb))

    def status_led(self, on):
        """turn the PD6 status LED on or off"""
        cfg = self.config
        self.config = (cfg | CONFIG_STATUS_LED) if on else (cfg & ~CONFIG_STATUS_LED)

    def show(self):
        """only needed if CONFIG_AUTOSHOW is off"""
        self._write(_COMMAND, [_CMD_SHOW])
