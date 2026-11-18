# code.py -- CircuitPython test for 70_seesaw, stock adafruit_seesaw only
# 14 Nov 2026 - @todbot / Tod Kurt
import time, board, busio
from adafruit_seesaw.seesaw import Seesaw
from adafruit_seesaw.neopixel import NeoPixel

NUM_LEDS = 8
TOUCH_PINS = (4, 5, 6, 7)  # Crickit's touch pins, firmware maps to pads 0-3

i2c = busio.I2C(board.SCL1, board.SDA1, frequency=400_000)
ss = Seesaw(i2c, 0x49)
print("hw_id: %02x  version: %08x  options: %08x" %
      (ss.chip_id, ss.get_version(), ss.get_options()))

pixels = NeoPixel(ss, 0, NUM_LEDS, brightness=0.3)  # pin num ignored
pixels.fill(0x000000)

# firmware's own touch detection isn't exposed via seesaw, so do it here
base = [ss.touch_read(p) for p in TOUCH_PINS]
THRESH = 500  # touches read ~3500 above idle on test pads
while True:
    vals = [ss.touch_read(p) for p in TOUCH_PINS]
    touched = [v - b > THRESH for v, b in zip(vals, base)]
    print(vals, touched)
    for i in range(NUM_LEDS):
        pad = i * len(TOUCH_PINS) // NUM_LEDS
        pixels[i] = 0x00ff40 if touched[pad] else 0x000000
    time.sleep(0.05)
