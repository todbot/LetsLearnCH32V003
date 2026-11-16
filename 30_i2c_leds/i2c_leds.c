// i2c_leds.c -- I2C peripheral that sets WS2812 LED colors
// 14 Nov 2026 - @todbot / Tod Kurt
//
// - I2C SDA on PC1, SCL on PC2, needs pull-ups (Stemma QT hosts have them)
// - LED data on PC6
// - Register map is a subset of the full one in 60_i2c_touch_leds
// - Write [reg, data...] to set regs, write [reg] then read to get regs

#include "ch32fun.h"
#include <stdio.h>
#include "i2c_slave.h"

#define I2C_ADDR (0x54)
#define NUM_LEDS (8)

#define WS2812DMA_IMPLEMENTATION
#define WSGRB
#define WS2812B_ALLOW_INTERRUPT_NESTING
#include "ws2812b_dma_spi_led_driver.h"

#define WHO_AM_I_VAL (0xC3)
#define FW_VERSION_VAL (30)

enum {
    REG_WHO_AM_I   = 0x00,
    REG_FW_VERSION = 0x01,
    REG_CONFIG     = 0x04,
    REG_BRIGHTNESS = 0x05,
    REG_COMMAND    = 0x06,
    REG_LEDS       = 0x20,  // NUM_LEDS x R,G,B
    REG_NUMREGS    = REG_LEDS + NUM_LEDS*3,
};

#define CONFIG_AUTOSHOW (1<<1)
#define CMD_SHOW (0x01)

// written by I2C ISR, out-of-range accesses are dropped / read as 0 by lib
volatile uint8_t regs[REG_NUMREGS];
volatile bool leds_dirty = false;

uint32_t WS2812BLEDCallback( int ledno )
{
    volatile uint8_t* rgb = &regs[REG_LEDS + ledno*3];
    uint8_t bri = regs[REG_BRIGHTNESS];
    uint32_t r = (rgb[0] * bri) >> 8;
    uint32_t g = (rgb[1] * bri) >> 8;
    uint32_t b = (rgb[2] * bri) >> 8;
    return r | (g << 8) | (b << 16);
}

// called from I2C ISR on STOP, keep it short and no printf
void on_i2c_write( uint8_t reg, uint8_t len )
{
    // lib also calls this after a read, or a reg-pointer-only write
    if( !i2c_slave_state.writing ) { return; }

    if( regs[REG_COMMAND] == CMD_SHOW ) {
        regs[REG_COMMAND] = 0;
        leds_dirty = true;
    }
    if( (regs[REG_CONFIG] & CONFIG_AUTOSHOW) && reg + len > REG_LEDS ) {
        leds_dirty = true;
    }
}

int main()
{
    SystemInit();
    funGpioInitAll();
    WS2812BDMAInit();

    regs[REG_WHO_AM_I] = WHO_AM_I_VAL;
    regs[REG_FW_VERSION] = FW_VERSION_VAL;
    regs[REG_CONFIG] = CONFIG_AUTOSHOW;
    regs[REG_BRIGHTNESS] = 64;

    // I2C pins are "alternate function": peripheral drives them, not GPIO regs
    // open-drain since I2C only pulls low
    funPinMode( PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SDA
    funPinMode( PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SCL
    SetupI2CSlave( I2C_ADDR, regs, sizeof(regs), on_i2c_write, NULL, false );

    leds_dirty = true;  // show initial (all off)

    while(1) {
        if( leds_dirty && !WS2812BLEDInUse ) {
            leds_dirty = false;
            WS2812BDMAStart( NUM_LEDS );
        }
    }
}
