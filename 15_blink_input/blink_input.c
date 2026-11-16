// blink_input.c -- set LED blink time by typing in "make monitor"
// 15 Nov 2026 - @todbot / Tod Kurt
//
// Keys 1,2,...,9,0 pick 10 to 1000 ms, any other key picks a random time.
// Wire LED + 330 ohm resistor from LED_PIN to GND

#include "ch32fun.h"
#include <stdio.h>

#define LED_PIN PC0
#define BLINK_MIN_MS (10)
#define BLINK_MAX_MS (1000)

volatile int key = -1;  // -1 = no new key

// ch32fun calls this from inside printf() or poll_input() when the programmer
// has sent keys over SWIO. printf here would recurse, so hand off to main loop
void handle_debug_input( int numbytes, uint8_t* data )
{
    key = data[numbytes-1];
}

int main()
{
    SystemInit();
    funGpioInitAll();
    funPinMode( LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );
    printf("blink_input: press 1,2,...,9,0 or any key\n");

    int blink_ms = 300;
    bool led_on = false;
    uint32_t last = SysTick->CNT;
    while(1) {
        // no interrupt on input, the chip must check the debug mailbox
        poll_input();

        if( key >= 0 ) {
            int c = key;
            if( c >= '0' && c <= '9' ) {
                int n = (c == '0') ? 9 : c - '1';
                blink_ms = BLINK_MIN_MS + n * (BLINK_MAX_MS - BLINK_MIN_MS) / 9;
            } else {
                // keypress timing makes SysTick's count random enough
                blink_ms = BLINK_MIN_MS + SysTick->CNT % (BLINK_MAX_MS - BLINK_MIN_MS + 1);
            }
            printf("key 0x%x '%c' -> %d ms\n", c, c, blink_ms);
            // minichlink w/ ESP32 programmer resends a key until the chip prints,
            // so drop copies that arrived during the printf
            key = -1;
        }

        // SysTick->CNT is a free-running 32-bit up-counter, unsigned diff handles wrap
        if( SysTick->CNT - last >= Ticks_from_Ms( blink_ms ) ) {
            last = SysTick->CNT;
            led_on = !led_on;
            funDigitalWrite( LED_PIN, led_on ? FUN_HIGH : FUN_LOW );
        }
    }
}
