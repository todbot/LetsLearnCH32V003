# code.py -- CircuitPython demo for 90_i2c_oled
# 20 Nov 2026 - @todbot / Tod Kurt
# copy i2c_oled.py to CIRCUITPY too
import time, board, busio
from i2c_oled import I2COLED

i2c = busio.I2C(board.SCL1, board.SDA1, frequency=400_000)
oled = I2COLED(i2c)
print("found i2c_oled, fw version", oled.version)

oled.clear()
oled.print(0, 0, "hello")
oled.print(0, 12, "big", size=2)
oled.rect(64, 0, 64, 32)
oled.fill_rect(68, 4, 20, 24)
oled.circle(108, 16, 12)
oled.line(0, 63, 127, 32)
oled.show()
time.sleep(2)

# bouncing ball, frame counter
BALL_R = 5  # radius, 1-30
x_min, x_max = BALL_R + 1, oled.width - 2 - BALL_R  # stay inside the border
y_min, y_max = BALL_R + 1, oled.height - 2 - BALL_R
x, y, dx, dy = oled.width // 2, oled.height // 2, 3, 2
frames, last = 0, time.monotonic()
fps = 0
while True:
    x, y = x + dx, y + dy
    # clamp too, so a big step can't push the ball past the edge
    if not x_min <= x <= x_max: dx, x = -dx, min(max(x, x_min), x_max)
    if not y_min <= y <= y_max: dy, y = -dy, min(max(y, y_min), y_max)
    oled.clear()
    oled.rect(0, 0, 128, 64)
    oled.fill_circle(x, y, BALL_R)
    oled.print(4, 4, "fps %d" % fps)
    oled.show()
    frames += 1
    now = time.monotonic()
    if now - last >= 1:
        fps, frames, last = frames, 0, now
        print("fps:", fps)
