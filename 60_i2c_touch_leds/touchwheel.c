// touchwheel.c -- I2C peripheral with captouch pads and WS2812 LEDs
// 14 Nov 2026 - @todbot / Tod Kurt
//
// - I2C SDA on PC1, SCL on PC2
// - LED data on PC6
// - INT on PC7, open-drain active-low, asserted while TOUCH_EVENTS != 0
// - register map in ../README.md, u16 regs are little-endian

#include "ch32fun.h"
#include <stdio.h>
#include "i2c_slave.h"

#define I2C_ADDR (0x54)
#define INT_PIN PC7
#define NUM_LEDS (8)

// for 3-pad wheel
#define NUM_PADS (3)
#define TOUCH_WHEEL (1)
// {port, pin, adc_chan}
#define TOUCH_PINS { {GPIOD,2,3}, {GPIOD,3,4}, {GPIOD,4,7} }

// for 4-pad wheel
/* #define NUM_PADS (4) */
/* #define TOUCH_WHEEL (1) */
/* // {port, pin, adc_chan} */
/* #define TOUCH_PINS { {GPIOC,4,2}, {GPIOD,2,3}, {GPIOD,3,4}, {GPIOD,4,7} } */

#include "touch.h"

#define WS2812DMA_IMPLEMENTATION
#define DMALEDS (32)  // driver's default 16 drops the last 2 of 8 LEDs (and 7-9, 15-17...)
#define WSGRB
#define WS2812B_ALLOW_INTERRUPT_NESTING
#include "ws2812b_dma_spi_led_driver.h"

#define WHO_AM_I_VAL (0xC3)
#define FW_VERSION_VAL (60)
#define BRIGHTNESS_DEFAULT (32)  // 8 LEDs full white is ~480mA, keep it low

enum {
    REG_WHO_AM_I       = 0x00,
    REG_FW_VERSION     = 0x01,
    REG_TOUCH_STATUS   = 0x02,
    REG_TOUCH_EVENTS   = 0x03,  // latched presses, write 1 to clear
    REG_CONFIG         = 0x04,
    REG_BRIGHTNESS     = 0x05,
    REG_COMMAND        = 0x06,
    REG_WHEEL_POS      = 0x07,  // 0 = no touch, 1-255
    REG_TOUCH_RAW      = 0x08,  // u16 x 4
    REG_TOUCH_BASELINE = 0x10,  // u16 x 4
    REG_TOUCH_THRESH   = 0x18,  // u16 x 4, writable
    REG_LEDS           = 0x20,  // NUM_LEDS x R,G,B
    REG_NUMREGS        = REG_LEDS + NUM_LEDS*3,
};

#define CONFIG_INT_ENABLE (1<<0)
#define CONFIG_AUTOSHOW   (1<<1)  // show LEDs after any write to LED regs
#define CONFIG_FEEDBACK   (1<<2)  // firmware lights LED under finger
#define CMD_SHOW        (0x01)
#define CMD_RECALIBRATE (0x02)
#define CMD_RESET       (0xA5)

volatile uint8_t regs[REG_NUMREGS];
volatile uint8_t touch_events;
volatile bool leds_dirty = false;
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

// hue 0-255 to r,g,b
void wheel( uint8_t pos, volatile uint8_t* rgb )
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

// called from DMA ISR as it fills its buffer, keep it short
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
    if( !i2c_slave_state.writing ) { return; }  // also called after reads

    uint8_t end = reg + len;
    if( reg <= REG_TOUCH_EVENTS && end > REG_TOUCH_EVENTS ) {
        // lib wrote the host's byte straight into regs[], use it as clear mask.
        // if update_regs() lands between that byte and STOP, all events clear
        touch_events &= ~regs[REG_TOUCH_EVENTS];
        regs[REG_TOUCH_EVENTS] = touch_events;
    }
    if( end > REG_TOUCH_THRESH && reg < REG_TOUCH_THRESH + NUM_PADS*2 ) {
        thresh_written = true;
    }
    if( (regs[REG_CONFIG] & CONFIG_AUTOSHOW) && end > REG_LEDS ) {
        leds_dirty = true;
    }
    switch( regs[REG_COMMAND] ) {
        case CMD_SHOW:        leds_dirty = true; break;
        case CMD_RECALIBRATE: do_recalibrate = true; break;
        case CMD_RESET:       NVIC_SystemReset(); break;
    }
    regs[REG_COMMAND] = 0;
}

void update_regs()
{
    static uint8_t last_bits = 0;
    uint8_t bits = touch_bits();
    uint8_t pos = touch_wheel_pos();

    // IRQs off so host never reads half of a u16 mid-update.
    // A multi-byte read can still span two updates, that's ok for touch data
    __disable_irq();
    touch_events |= bits & ~last_bits;  // new presses only
    regs[REG_TOUCH_EVENTS] = touch_events;
    regs[REG_TOUCH_STATUS] = bits;
    regs[REG_WHEEL_POS] = pos;
    for( int i = 0; i < NUM_PADS; i++ ) {
        put_u16( REG_TOUCH_RAW + i*2, pads[i].filt );
        put_u16( REG_TOUCH_BASELINE + i*2, pads[i].baseline );
    }
    __enable_irq();
    last_bits = bits;

    bool int_on = touch_events && (regs[REG_CONFIG] & CONFIG_INT_ENABLE);
    // open-drain: low pulls INT down, high lets it float to host's pull-up
    funDigitalWrite( INT_PIN, int_on ? FUN_LOW : FUN_HIGH );

    if( (regs[REG_CONFIG] & CONFIG_FEEDBACK) && pos ) {
        int led = (pos - 1) * NUM_LEDS / 255;
        for( int i = 0; i < NUM_LEDS*3; i++ ) { regs[REG_LEDS + i] = 0; }
        wheel( pos, &regs[REG_LEDS + led*3] );
        leds_dirty = true;
    }
}

int main()
{
    SystemInit();
    funGpioInitAll();
    touch_init();
    WS2812BDMAInit();

    regs[REG_WHO_AM_I] = WHO_AM_I_VAL;
    regs[REG_FW_VERSION] = FW_VERSION_VAL;
    regs[REG_CONFIG] = CONFIG_INT_ENABLE | CONFIG_AUTOSHOW;
    regs[REG_BRIGHTNESS] = BRIGHTNESS_DEFAULT;
    for( int i = 0; i < NUM_PADS; i++ ) {
        put_u16( REG_TOUCH_THRESH + i*2, pads[i].thresh );
    }
    leds_dirty = true;  // show initial (all off)

    funPinMode( INT_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_OD );
    funDigitalWrite( INT_PIN, FUN_HIGH );

    // alternate function open-drain: I2C peripheral drives the pins
    funPinMode( PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SDA
    funPinMode( PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SCL
    SetupI2CSlave( I2C_ADDR, regs, sizeof(regs), on_i2c_write, NULL, false );

    while(1) {
        // keep LED data edges and DMA ISRs out of touch readings
        while( WS2812BLEDInUse ) { }

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

        if( leds_dirty ) {
            leds_dirty = false;
            WS2812BDMAStart( NUM_LEDS );
        }
        Delay_Ms( 10 );
    }
}
