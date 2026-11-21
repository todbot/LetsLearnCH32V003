// 45_touch_ws2812.c -- read captouch pads, light LED n while pad n is touched
// 15 Nov 2026 - @todbot / Tod Kurt
//
// 40_touch_printf plus 20_ws2812. LED data on PC6.
// Same printf line as 40 on "make monitor", but only when touches change.

#include "ch32fun.h"
#include <stdio.h>

#define NUM_PADS (4)
#define TOUCH_WHEEL (0)
// {port, pin, adc_chan}
#define TOUCH_PINS { {GPIOD,2,3}, {GPIOD,3,4}, {GPIOD,4,7}, {GPIOC,4,2} }
#include "touch.h"

#define NUM_LEDS (8)
#define TOUCH_UPDATE_MS (2)

#define WS2812DMA_IMPLEMENTATION
#define DMALEDS (32)  // driver's default 16 drops the last 2 of 8 LEDs (and 7-9, 15-17...)
#define WSGRB  // WS2812 wire order is G,R,B; callback still returns 0xBBGGRR
#include "ws2812b_dma_spi_led_driver.h"

uint8_t led_rgb[NUM_LEDS][3];
uint8_t brightness = 64;  // range 0-255, global scale at output

// called from DMA ISR as it fills its buffer, keep it short
uint32_t WS2812BLEDCallback( int ledno )
{
    uint32_t r = (led_rgb[ledno][0] * brightness) >> 8;
    uint32_t g = (led_rgb[ledno][1] * brightness) >> 8;
    uint32_t b = (led_rgb[ledno][2] * brightness) >> 8;
    return r | (g << 8) | (b << 16);
}

// hue 0-255 to r,g,b
void wheel( uint8_t pos, uint8_t* rgb )
{
    if( pos < 85 ) {
        rgb[0] = 255 - pos*3; rgb[1] = pos*3; rgb[2] = 0;
    } else if( pos < 170 ) {
        pos -= 85;
        rgb[0] = 0; rgb[1] = 255 - pos*3; rgb[2] = pos*3;
    } else {
        pos -= 170;
        rgb[0] = pos*3; rgb[1] = 0; rgb[2] = 255 - pos*3;
    }
}

// ch32fun's printf has no %b. pad 0 first, to line up with the columns
static const char* bits_str( uint8_t bits )
{
    static char s[NUM_PADS+1];
    for( int i = 0; i < NUM_PADS; i++ ) { s[i] = (bits & (1<<i)) ? '1' : '0'; }
    return s;
}

int main()
{
    SystemInit();
    // sets up SPI1 at 3 MHz (4 SPI bits per WS2812 bit), DMA1 Ch3, and PC6
    WS2812BDMAInit();
    touch_init();
    printf("touch_ws2812: %d pads\n", NUM_PADS);

    uint8_t last_bits = 0xFF;  // force first LED update
    uint32_t next_update = SysTick->CNT;
    while(1) {
        // touch_update() debounce & drift count calls, so pace it w/ SysTick, not loop speed.
        // signed diff handles SysTick->CNT wrapping
        if( (int32_t)(SysTick->CNT - next_update) < 0 ) { continue; }
        next_update = SysTick->CNT + Ticks_from_Ms( TOUCH_UPDATE_MS );

        // keep LED data edges and DMA ISRs out of touch readings
        while( WS2812BLEDInUse ) { }
        touch_update();

        uint8_t bits = touch_bits();
        if( bits == last_bits ) { continue; }
        last_bits = bits;

        for( int i = 0; i < NUM_LEDS; i++ ) {
            if( i < NUM_PADS && (bits & (1<<i)) ) { wheel( i * 256 / NUM_PADS, led_rgb[i] ); }
            else { led_rgb[i][0] = led_rgb[i][1] = led_rgb[i][2] = 0; }
        }
        WS2812BDMAStart( NUM_LEDS );

        // after LED start so printing doesn't delay the LEDs.
        // each line waits on the programmer to read it, so print only on changes
        for( int i = 0; i < NUM_PADS; i++ ) {
            printf("%5d %5d %4d | ", pads[i].raw, pads[i].baseline, touch_delta(i));
        }
#if TOUCH_WHEEL
        printf("wheel:%s %3d\n", bits_str( bits ), touch_wheel_pos());
#else
        printf("buttons:%s\n", bits_str( bits ));
#endif
    }
}
