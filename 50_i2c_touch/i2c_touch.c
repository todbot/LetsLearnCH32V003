// i2c_touch.c -- I2C peripheral that reports captouch pads
// 14 Nov 2026 - @todbot / Tod Kurt
//
// - I2C SDA on PC1, SCL on PC2
// - INT on PC7, open-drain active-low, asserted while TOUCH_EVENTS != 0
// - status LED on PD6, set by CONFIG bit3
// - u16 regs are little-endian

#include "ch32fun.h"
#include <stdio.h>
#include "i2c_slave.h"

#define I2C_ADDR (0x54)
#define INT_PIN PC7
#define STATUS_LED_PIN PD6

// for 3-pad wheel
#define NUM_PADS (3)
#define TOUCH_WHEEL (1)
// {port, pin, adc_chan}
#define TOUCH_PINS { {GPIOD,2,3}, {GPIOD,3,4}, {GPIOD,4,7} }

// for 4-pad wheel
/* #define NUM_PADS (4) */
/* #define TOUCH_WHEEL (1) */
/* // {port, pin, adc_chan} */
/* #define TOUCH_PINS { {GPIOD,2,3}, {GPIOD,3,4}, {GPIOD,4,7}, {GPIOC,4,2} } */

#include "touch.h"

#define WHO_AM_I_VAL (0xC3)
#define FW_VERSION_VAL (50)

enum {
    REG_WHO_AM_I       = 0x00,
    REG_FW_VERSION     = 0x01,
    REG_TOUCH_STATUS   = 0x02,
    REG_TOUCH_EVENTS   = 0x03,  // latched presses, write 1 to clear
    REG_CONFIG         = 0x04,
    REG_COMMAND        = 0x06,
    REG_WHEEL_POS      = 0x07,  // 0 = no touch, 1-255
    REG_TOUCH_RAW      = 0x08,  // u16 x 4
    REG_TOUCH_BASELINE = 0x10,  // u16 x 4
    REG_TOUCH_THRESH   = 0x18,  // u16 x 4, writable
    REG_NUMREGS        = 0x20,
};

#define CONFIG_INT_ENABLE (1<<0)
#define CONFIG_STATUS_LED (1<<3)  // lights PD6 LED
#define CMD_RECALIBRATE (0x02)

volatile uint8_t regs[REG_NUMREGS];
volatile uint8_t touch_events;
volatile bool do_recalibrate = false;
volatile bool thresh_written = false;

static void put_u16( int reg, uint16_t v )
{
    regs[reg] = v & 0xff;
    regs[reg+1] = v >> 8;
}

static uint16_t get_u16( int reg )
{
    return regs[reg] | (regs[reg+1] << 8);
}

// called from I2C ISR on STOP, keep it short and no printf
void on_i2c_write( uint8_t reg, uint8_t len )
{
    if( !i2c_slave_state.writing ) { return; }  // also called after reads

    uint8_t end = reg + len;
    if( reg <= REG_TOUCH_EVENTS && end > REG_TOUCH_EVENTS ) {
        // lib wrote the host's byte straight into regs[], use it as clear mask.
        // if update_regs() lands between that byte and STOP, all events clear
        touch_events &= ~regs[REG_TOUCH_EVENTS];
        regs[REG_TOUCH_EVENTS] = touch_events;
    }
    if( regs[REG_COMMAND] == CMD_RECALIBRATE ) {
        regs[REG_COMMAND] = 0;
        do_recalibrate = true;
    }
    if( end > REG_TOUCH_THRESH && reg < REG_TOUCH_THRESH + NUM_PADS*2 ) {
        thresh_written = true;
    }
}

void update_regs()
{
    static uint8_t last_bits = 0;
    uint8_t bits = touch_bits();

    // IRQs off so host never reads half of a u16 mid-update.
    // A multi-byte read can still span two updates, that's ok for touch data
    __disable_irq();
    touch_events |= bits & ~last_bits;  // new presses only
    regs[REG_TOUCH_EVENTS] = touch_events;
    regs[REG_TOUCH_STATUS] = bits;
    regs[REG_WHEEL_POS] = touch_wheel_pos();
    for( int i = 0; i < NUM_PADS; i++ ) {
        put_u16( REG_TOUCH_RAW + i*2, pads[i].filt );
        put_u16( REG_TOUCH_BASELINE + i*2, pads[i].baseline );
    }
    __enable_irq();
    last_bits = bits;

    bool int_on = touch_events && (regs[REG_CONFIG] & CONFIG_INT_ENABLE);
    // open-drain: low pulls INT down, high lets it float to host's pull-up
    funDigitalWrite( INT_PIN, int_on ? FUN_LOW : FUN_HIGH );
    funDigitalWrite( STATUS_LED_PIN, (regs[REG_CONFIG] & CONFIG_STATUS_LED) ? FUN_HIGH : FUN_LOW );
}

int main()
{
    SystemInit();
    funGpioInitAll();
    touch_init();

    regs[REG_WHO_AM_I] = WHO_AM_I_VAL;
    regs[REG_FW_VERSION] = FW_VERSION_VAL;
    regs[REG_CONFIG] = CONFIG_INT_ENABLE;
    for( int i = 0; i < NUM_PADS; i++ ) {
        put_u16( REG_TOUCH_THRESH + i*2, pads[i].thresh );
    }

    funPinMode( INT_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_OD );
    funPinMode( STATUS_LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );
    funDigitalWrite( INT_PIN, FUN_HIGH );

    // alternate function open-drain: I2C peripheral drives the pins
    funPinMode( PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SDA
    funPinMode( PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SCL
    SetupI2CSlave( I2C_ADDR, regs, sizeof(regs), on_i2c_write, NULL, false );

    while(1) {
        touch_update();
        if( do_recalibrate ) {
            do_recalibrate = false;
            touch_recalibrate();
        }
        if( thresh_written ) {
            thresh_written = false;
            for( int i = 0; i < NUM_PADS; i++ ) {
                pads[i].thresh = get_u16( REG_TOUCH_THRESH + i*2 );
            }
        }
        update_regs();
        Delay_Ms( 10 );
    }
}
