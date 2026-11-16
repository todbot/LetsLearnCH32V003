// seesaw.c -- touchwheel that speaks Adafruit seesaw (STATUS, NEOPIXEL, TOUCH)
// 14 Nov 2026 - @todbot / Tod Kurt
//
// - I2C SDA on PC1, SCL on PC2, LED data on PC6
// - Works with stock CircuitPython adafruit_seesaw:
//     ss = Seesaw(i2c, 0x49)
//     ss.touch_read(4..7)  -> pads 0-3
//     NeoPixel(ss, 0, NUM_LEDS)  (pin number is ignored)
// - Seesaw multi-byte values are big-endian

#include "ch32fun.h"
#include <stdio.h>
#include "seesaw_i2c.h"

#define I2C_ADDR (0x49)  // seesaw default
#define NUM_LEDS (8)
#define BRIGHTNESS_MAX (128)  // power cap, host lib does its own brightness too

#define NUM_PADS (4)
#define TOUCH_WHEEL (1)
#if BOARD_TARGET  // set in Makefile, "make BOARD_TARGET=1" for SOP-16/QFN-20
#define TOUCH_PINS { {GPIOA,2,0}, {GPIOA,1,1}, {GPIOC,4,2}, {GPIOD,4,7} }
#else  // dev board: PA1/PA2 are crystal pins, so skip them
#define TOUCH_PINS { {GPIOC,4,2}, {GPIOD,2,3}, {GPIOD,3,4}, {GPIOD,4,7} }
#endif
#include "touch.h"

#define WS2812DMA_IMPLEMENTATION
// no color order define: callback's bits 23-0 go out MSB first, so the
// host's bytes (already in wire order) go out as sent
#define WS2812B_ALLOW_INTERRUPT_NESTING
#include "ws2812b_dma_spi_led_driver.h"

// lib only accepts certain HW_IDs, and only Crickit's pinmap has touch_pins
// (4-7), picked by PID in upper half of VERSION. So pretend: SAMD09 + Crickit
#define SEESAW_HW_ID (0x55)
#define SEESAW_PID (9999)
#define SEESAW_DATE (0x1A0E)  // free-form lower half of VERSION

enum {
    STATUS_BASE = 0x00,
    STATUS_HW_ID = 0x01, STATUS_VERSION = 0x02, STATUS_OPTIONS = 0x03,
    STATUS_SWRST = 0x7F,

    NEOPIXEL_BASE = 0x0E,
    NEOPIXEL_PIN = 0x01, NEOPIXEL_SPEED = 0x02, NEOPIXEL_BUF_LENGTH = 0x03,
    NEOPIXEL_BUF = 0x04, NEOPIXEL_SHOW = 0x05,

    TOUCH_BASE = 0x0F,
    TOUCH_CHANNEL_OFFSET = 0x10,
};

uint8_t led_buf[NUM_LEDS*3];
volatile uint16_t led_buf_len = sizeof(led_buf);
volatile bool leds_dirty = false;

uint32_t WS2812BLEDCallback( int ledno )
{
    uint8_t* p = &led_buf[ledno*3];
    return ((p[0] * BRIGHTNESS_MAX) >> 8) << 16 |
           ((p[1] * BRIGHTNESS_MAX) >> 8) << 8 |
           ((p[2] * BRIGHTNESS_MAX) >> 8);
}

static void tx_u32( uint32_t v )
{
    seesaw_tx[0] = v >> 24; seesaw_tx[1] = v >> 16;
    seesaw_tx[2] = v >> 8;  seesaw_tx[3] = v;
    seesaw_tx_len = 4;
}

static void tx_u16( uint16_t v )
{
    seesaw_tx[0] = v >> 8; seesaw_tx[1] = v;
    seesaw_tx_len = 2;
}

// called from I2C ISR on STOP, keep it short and no printf
void seesaw_on_write( uint8_t* buf, uint8_t len )
{
    if( len < 2 ) { return; }
    uint8_t base = buf[0], func = buf[1];
    uint8_t* data = buf + 2;
    int n = len - 2;
    seesaw_tx_len = 0;

    if( base == STATUS_BASE ) {
        if( func == STATUS_HW_ID )   { seesaw_tx[0] = SEESAW_HW_ID; seesaw_tx_len = 1; }
        if( func == STATUS_VERSION ) { tx_u32( ((uint32_t)SEESAW_PID << 16) | SEESAW_DATE ); }
        if( func == STATUS_OPTIONS ) {  // bitmask of modules we have
            tx_u32( (1 << STATUS_BASE) | (1 << NEOPIXEL_BASE) | (1 << TOUCH_BASE) );
        }
        if( func == STATUS_SWRST ) { NVIC_SystemReset(); }
    }
    else if( base == NEOPIXEL_BASE ) {
        // PIN and SPEED are accepted and ignored, we only have PC6 @ 800kHz
        if( func == NEOPIXEL_BUF_LENGTH && n >= 2 ) {
            uint16_t l = (data[0] << 8) | data[1];
            led_buf_len = (l < sizeof(led_buf)) ? l : sizeof(led_buf);
        }
        if( func == NEOPIXEL_BUF && n >= 2 ) {
            // host always sends full 24-byte chunks, so clamp to buffer length
            uint16_t off = (data[0] << 8) | data[1];
            for( int i = 2; i < n && off < led_buf_len; i++ ) {
                led_buf[off++] = data[i];
            }
        }
        if( func == NEOPIXEL_SHOW ) { leds_dirty = true; }
    }
    else if( base == TOUCH_BASE ) {
        int ch = func - TOUCH_CHANNEL_OFFSET;
        if( ch >= 0 && ch < NUM_PADS ) { tx_u16( pads[ch].filt ); }
    }
}

int main()
{
    SystemInit();
    funGpioInitAll();
    touch_init();
    WS2812BDMAInit();
    leds_dirty = true;  // show initial (all off)

    // alternate function open-drain: I2C peripheral drives the pins
    funPinMode( PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SDA
    funPinMode( PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SCL
    seesaw_i2c_init( I2C_ADDR );

    while(1) {
        // keep LED data edges and DMA ISRs out of touch readings
        while( WS2812BLEDInUse ) { }

        touch_update();

        if( leds_dirty ) {
            leds_dirty = false;
            WS2812BDMAStart( NUM_LEDS );
        }
        Delay_Ms( 10 );
    }
}
