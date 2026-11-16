// button.c -- read the dev board's button, toggle LED on each press
// 16 Nov 2026 - @todbot / Tod Kurt
//
// Dev board button connects PC0 to VDD, so it needs a pull-down and reads 1 when pressed.
// Wire LED + 330 ohm resistor from LED_PIN to GND

#include "ch32fun.h"
#include <stdio.h>

#define BUTTON_PIN PC0
#define LED_PIN PD6
#define DEBOUNCE_MS (10)

int main()
{
    SystemInit();
    funGpioInitAll();
    funPinMode( LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );
    // input w/ pull: the OUTDR bit picks pull-up (1) or pull-down (0)
    funPinMode( BUTTON_PIN, GPIO_CNF_IN_PUPD );
    funDigitalWrite( BUTTON_PIN, FUN_LOW );
    printf("button: press the board's button\n");

    bool led_on = false;
    int last = 0;
    int presses = 0;
    while(1) {
        int pressed = funDigitalRead( BUTTON_PIN );
        if( pressed != last ) {
            last = pressed;
            if( pressed ) {
                presses++;
                led_on = !led_on;
                funDigitalWrite( LED_PIN, led_on ? FUN_HIGH : FUN_LOW );
            }
            printf("%s %d\n", pressed ? "press" : "release", presses);
        }
        // contacts bounce for a few ms, so only look again after they settle
        Delay_Ms( DEBOUNCE_MS );
    }
}
