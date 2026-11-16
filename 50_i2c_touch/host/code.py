# code.py -- CircuitPython test for 50_i2c_touch
# 14 Nov 2026 - @todbot / Tod Kurt
import time, struct, board, busio, digitalio

ADDR = 0x54
NUM_PADS = 4
REG_TOUCH_STATUS, REG_TOUCH_EVENTS, REG_COMMAND = 0x02, 0x03, 0x06
REG_WHEEL_POS, REG_TOUCH_RAW, REG_TOUCH_BASELINE = 0x07, 0x08, 0x10

i2c = busio.I2C(board.SCL, board.SDA, frequency=100_000)
while not i2c.try_lock(): pass
print("scan:", [hex(a) for a in i2c.scan()])

int_pin = None
if hasattr(board, "D2"):  # wire touchwheel INT (PC7) here, optional
    int_pin = digitalio.DigitalInOut(board.D2)
    int_pin.pull = digitalio.Pull.UP

def read_regs(reg, n):
    buf = bytearray(n)
    i2c.writeto_then_readfrom(ADDR, bytes([reg]), buf)
    return buf

# one read of status, events, ... raw, baseline so they're consistent
while True:
    buf = read_regs(REG_TOUCH_STATUS, REG_TOUCH_BASELINE + NUM_PADS*2 - REG_TOUCH_STATUS)
    status, events = buf[0], buf[1]
    pos = buf[REG_WHEEL_POS - REG_TOUCH_STATUS]
    raws = struct.unpack_from("<%dH" % NUM_PADS, buf, REG_TOUCH_RAW - REG_TOUCH_STATUS)
    bases = struct.unpack_from("<%dH" % NUM_PADS, buf, REG_TOUCH_BASELINE - REG_TOUCH_STATUS)
    deltas = [r - b for r, b in zip(raws, bases)]
    irq = "" if int_pin is None else ("INT" if not int_pin.value else "   ")
    print("status:%s events:%s pos:%3d deltas:%s %s" %
          ("{:04b}".format(status), "{:04b}".format(events), pos, deltas, irq))
    if events:
        i2c.writeto(ADDR, bytes([REG_TOUCH_EVENTS, events]))  # write 1s to clear
    time.sleep(0.1)
