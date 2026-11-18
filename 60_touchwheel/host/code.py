# code.py -- CircuitPython demo for 60_touchwheel
# 14 Nov 2026 - @todbot / Tod Kurt
# copy touchwheelqwiic.py to CIRCUITPY too
import time, board, busio
from touchwheelqwiic import Touchwheel, CONFIG_INT_ENABLE, CONFIG_AUTOSHOW

i2c = busio.I2C(board.SCL1, board.SDA1, frequency=400_000)
tw = Touchwheel(i2c, num_pads=3)
tw.brightness = 32
num_leds = tw.num_leds

def color_wheel(pos):
    pos = pos % 256
    if pos < 85: return (255 - pos*3, pos*3, 0)
    if pos < 170: pos -= 85; return (0, 255 - pos*3, pos*3)
    pos -= 170; return (pos*3, 0, 255 - pos*3)

# host-side feedback, firmware's CONFIG_FEEDBACK does similar on its own
tw.config = CONFIG_INT_ENABLE | CONFIG_AUTOSHOW
last_pos = 0
while True:
    pos = tw.position()
    if pos != last_pos:
        print("pos:", pos, "touches: {:04b}".format(tw.touches()), "events:", tw.events())
        if pos:
            led = (pos - 1) * num_leds // 255
            tw.rgbs([color_wheel(pos) if i == led else (0,0,0) for i in range(num_leds)])
        else:
            tw.rgb(0, 0, 0)
        last_pos = pos
    time.sleep(0.02)
