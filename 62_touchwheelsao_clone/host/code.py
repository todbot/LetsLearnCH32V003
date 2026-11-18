"""TouchwheelSAO demo: rainbow spin, touch angle, and a scroll counter

From github.com/todbot/TouchwheelSAO examples/circuitpython, run against
62_touchwheelsao_clone. Copy touchwheel.py to CIRCUITPY too.
"""

import time
import board
import busio

from touchwheel import Touchwheel, color_wheel

# QT Py RP2040's STEMMA port
i2c = busio.I2C(board.SCL1, board.SDA1, frequency=400_000)
wheel = Touchwheel(i2c)

scroll = 0
was_touched = False
while True:
    angle = wheel.angle()
    scroll += wheel.delta()
    touched = angle is not None

    if touched != was_touched:
        was_touched = touched
        wheel.status_led(touched)
        if not touched:
            print("released, scroll:", scroll)

    if touched:
        # The wheel lights itself to match the touch, so leave the LEDs be
        print("angle:", angle, "scroll:", scroll)
    else:
        hue = int(time.monotonic() * 1000) // 6
        wheel.rgbs([color_wheel(hue + n * 85) for n in range(3)])

    if not wheel.present:
        print("no touchwheel found")
        time.sleep(1)
    time.sleep(0.04)
