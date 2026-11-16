// ws2812.c -- drive several WS2812s with SPI+DMA, no CPU bit-banging
// 14 Nov 2026 - @todbot / Tod Kurt
//
// LED data on PC6 (fixed: it's SPI1 MOSI). 5V LEDs want a 74AHCT1G125 buffer.
// On boot shows red, green, blue for 1 sec each to check color order,
// then a rainbow chase.

#include "ch32fun.h"
#include <stdio.h>

#define NUM_LEDS (8)

#define WS2812DMA_IMPLEMENTATION
#define WSGRB  // WS2812 wire order is G,R,B; callback still returns 0xBBGGRR
#define WS2812B_ALLOW_INTERRUPT_NESTING  // let DMA ISR preempt others (I2C later)
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

void leds_fill( uint8_t r, uint8_t g, uint8_t b )
{
    for( int i = 0; i < NUM_LEDS; i++ ) {
        led_rgb[i][0] = r; led_rgb[i][1] = g; led_rgb[i][2] = b;
    }
}

void leds_show()
{
    while( WS2812BLEDInUse ) { }  // driver clears this when DMA is done
    WS2812BDMAStart( NUM_LEDS );
}

int main()
{
    SystemInit();
    // sets up SPI1 at 3 MHz (4 SPI bits per WS2812 bit), DMA1 Ch3, and PC6
    WS2812BDMAInit();
    
    printf("ws2812 test: red, green, blue 3 times\n");
    
    for(int i=0; i<3; i++) {
        leds_fill( 255, 0, 0 ); leds_show();
        Delay_Ms(300);
        leds_fill( 0, 255, 0 ); leds_show();
        Delay_Ms(300);
        leds_fill( 0, 0, 255 ); leds_show();
        Delay_Ms(300);
        leds_fill( 0, 0, 0 ); leds_show();
        Delay_Ms(500);
    }
    
    uint8_t hue = 0;
    while(1) {
        for( int i = 0; i < NUM_LEDS; i++ ) {
            wheel( hue + i * (256 / NUM_LEDS), led_rgb[i] );
        }
        hue++;
        leds_show();
        Delay_Ms( 10 );
    }
}
