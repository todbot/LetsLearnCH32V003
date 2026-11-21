// 10_blink.c -- blink an LED and printf, to test toolchain + programmer
// 14 Nov 2026 - @todbot / Tod Kurt
//
// Wire LED + 330 ohm resistor from LED_PIN to GND

#include "ch32fun.h"
#include <stdio.h>

#define LED_PIN PD6

int main()
{
    // switch to internal 24 MHz HSI x2 PLL = 48 MHz, start SysTick for Delay_Ms()
    SystemInit();

    // GPIO ports must be clocked (RCC->APB2PCENR) before their regs do anything
    funGpioInitAll();

    // 4 bits per pin in GPIOx->CFGLR: 2 bits of speed (0 = input), 2 bits of config
    funPinMode( LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );

    int count = 0;
    while(1) {
        funDigitalWrite( LED_PIN, FUN_HIGH );
        Delay_Ms( 300 );
        funDigitalWrite( LED_PIN, FUN_LOW );
        Delay_Ms( 300 );
        // waits for the programmer to read it, or times out once (~120ms) if none
        printf("blink %d\n", count++);
    }
}
