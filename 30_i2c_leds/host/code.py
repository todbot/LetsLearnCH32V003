# code.py -- CircuitPython test for 30_i2c_leds
# 14 Nov 2026 - @todbot / Tod Kurt
import time, board, busio

ADDR = 0x54
REG_WHO_AM_I, REG_CONFIG, REG_BRIGHTNESS, REG_COMMAND, REG_LEDS = 0x00, 0x04, 0x05, 0x06, 0x20
CONFIG_AUTOSHOW = 1 << 1
CMD_SHOW = 0x01
NUM_LEDS = 8

i2c = busio.I2C(board.SCL, board.SDA, frequency=100_000)
while not i2c.try_lock(): pass
print("scan:", [hex(a) for a in i2c.scan()])

def read_regs(reg, n):
    buf = bytearray(n)
    i2c.writeto_then_readfrom(ADDR, bytes([reg]), buf)
    return buf

def write_regs(reg, data):
    i2c.writeto(ADDR, bytes([reg]) + bytes(data))

print("who_am_i: %02x" % read_regs(REG_WHO_AM_I, 1)[0])
# TouchwheelSAO bug was only first byte of multi-byte reads correct
print("regs 0-7:", ["%02x" % b for b in read_regs(0x00, 8)])

# past end of reg map, should be ignored
write_regs(REG_LEDS + NUM_LEDS*3, [1, 2, 3, 4])
print("still alive: %02x" % read_regs(REG_WHO_AM_I, 1)[0])

write_regs(REG_BRIGHTNESS, [32])

# autoshow off: LED writes only land in regs until CMD_SHOW
config = read_regs(REG_CONFIG, 1)[0]
write_regs(REG_CONFIG, [config & ~CONFIG_AUTOSHOW])
print("config: %02x (want %02x)" % (read_regs(REG_CONFIG, 1)[0], config & ~CONFIG_AUTOSHOW))
write_regs(REG_LEDS, bytes((255,0,255)) * NUM_LEDS)
print("wrote magenta, LEDs should NOT change for 2 secs")
time.sleep(2)
write_regs(REG_COMMAND, [CMD_SHOW])
print("sent CMD_SHOW, LEDs should be magenta now")
print("command: %02x (want 00, self-clears)" % read_regs(REG_COMMAND, 1)[0])
time.sleep(2)
write_regs(REG_CONFIG, [config])

colors = [(255,0,0), (0,255,0), (0,0,255), (255,255,255)]
i = 0
while True:
    c = colors[i % len(colors)]
    write_regs(REG_LEDS, bytes(c) * NUM_LEDS)  # autoshow is on by default
    i += 1
    time.sleep(0.5)
