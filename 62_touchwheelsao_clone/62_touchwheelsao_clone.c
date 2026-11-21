// 62_touchwheelsao_clone.c -- 60_touchwheel with the TouchwheelSAO register map
// 18 Nov 2026 - @todbot / Tod Kurt
//
// - I2C SDA on PC1, SCL on PC2
// - LED data on PC6
// - INT on PC7, open-drain active-low, asserted while EVENTS != 0
// - status LED on PD6, set by LED_STATUS
// - register map in ../README.md, u16 regs are little-endian
// - regs 0-27 match TouchwheelSAO, so its host driver works as-is

#include "ch32fun.h"
#include <stdio.h>
#include "i2c_slave.h"

#define I2C_ADDR (0x54)
#define INT_PIN PC7
#define STATUS_LED_PIN PD6
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
/* #define TOUCH_PINS { {GPIOD,2,3}, {GPIOD,3,4}, {GPIOD,4,7}, {GPIOC,4,2} } */

#include "touch.h"

#define WS2812DMA_IMPLEMENTATION
#define DMALEDS (32)  // driver's default 16 drops the last 2 of 8 LEDs (and 7-9, 15-17...)
#define WSGRB
#define WS2812B_ALLOW_INTERRUPT_NESTING
#include "ws2812b_dma_spi_led_driver.h"

#define WHO_AM_I_VAL (0xC3)
#define FW_VERSION_VAL (62)
#define BRIGHTNESS_DEFAULT (32)  // 8 LEDs full white is ~480mA, keep it low

enum {
    // TouchwheelSAO-compatible
    REG_POSITION   = 0x00,  // 0 = no touch, 1-255
    REG_TOUCHES    = 0x01,  // bit n = pad n touched
    REG_RAW0       = 0x02,  // u16 x 3, pads 0-2
    REG_THRESH0    = 0x08,  // u16 x 3, pads 0-2, writable
    REG_LED_STATUS = 0x0E,
    REG_LED_RGB    = 0x0F,  // R,G,B, writing a channel sets it on all LEDs
    REG_LED0       = 0x12,  // LED0-2 R,G,B, mirrors first 9 bytes of REG_LEDS
    REG_VERSION    = 0x1B,
    // extensions
    REG_WHO_AM_I   = 0x1C,
    REG_EVENTS     = 0x1D,  // latched presses, write 1 to clear
    REG_CONFIG     = 0x1E,
    REG_BRIGHTNESS = 0x1F,
    REG_COMMAND    = 0x20,
    REG_RAW3       = 0x22,  // u16, optional pad 3
    REG_THRESH3    = 0x24,  // u16, optional pad 3, writable
    REG_BASELINE   = 0x28,  // u16 x 4
    REG_LEDS       = 0x40,  // NUM_LEDS x R,G,B
    REG_NUMREGS    = REG_LEDS + NUM_LEDS*3,
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

// SAO's map only has room for 3 pads
static int raw_reg( int i )    { return i < 3 ? REG_RAW0 + i*2 : REG_RAW3; }
static int thresh_reg( int i ) { return i < 3 ? REG_THRESH0 + i*2 : REG_THRESH3; }

static bool overlaps( int reg, int end, int lo, int n )
{
    return reg < lo + n && end > lo;
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

    int end = reg + len;
    if( overlaps( reg, end, REG_EVENTS, 1 ) ) {
        // lib wrote the host's byte straight into regs[], use it as clear mask.
        // if update_regs() lands between that byte and STOP, all events clear
        touch_events &= ~regs[REG_EVENTS];
        regs[REG_EVENTS] = touch_events;
    }
    if( overlaps( reg, end, REG_THRESH0, 6 ) || overlaps( reg, end, REG_THRESH3, 2 ) ) {
        thresh_written = true;
    }

    // fill-all, then LED0-2, then all LEDs, as if the bytes were applied in order
    bool leds_written = false;
    for( int c = 0; c < 3; c++ ) {
        if( !overlaps( reg, end, REG_LED_RGB + c, 1 ) ) { continue; }
        for( int n = 0; n < NUM_LEDS; n++ ) { regs[REG_LEDS + n*3 + c] = regs[REG_LED_RGB + c]; }
        leds_written = true;
    }
    for( int i = 0; i < 9; i++ ) {
        if( !overlaps( reg, end, REG_LED0 + i, 1 ) ) { continue; }
        regs[REG_LEDS + i] = regs[REG_LED0 + i];
        leds_written = true;
    }
    if( end > REG_LEDS ) { leds_written = true; }
    if( leds_written ) {
        for( int i = 0; i < 9; i++ ) { regs[REG_LED0 + i] = regs[REG_LEDS + i]; }
        if( regs[REG_CONFIG] & CONFIG_AUTOSHOW ) { leds_dirty = true; }
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
    static uint8_t last_pos = 0;
    uint8_t bits = touch_bits();
    uint8_t pos = touch_wheel_pos();

    // IRQs off so host never reads half of a u16 mid-update.
    // A multi-byte read can still span two updates, that's ok for touch data
    __disable_irq();
    touch_events |= bits & ~last_bits;  // new presses only
    regs[REG_EVENTS] = touch_events;
    regs[REG_TOUCHES] = bits;
    regs[REG_POSITION] = pos;
    for( int i = 0; i < NUM_PADS; i++ ) {
        put_u16( raw_reg(i), pads[i].filt );
        put_u16( REG_BASELINE + i*2, pads[i].baseline );
    }
    __enable_irq();
    last_bits = bits;

    funDigitalWrite( STATUS_LED_PIN, regs[REG_LED_STATUS] ? FUN_HIGH : FUN_LOW );

    bool int_on = touch_events && (regs[REG_CONFIG] & CONFIG_INT_ENABLE);
    // open-drain: low pulls INT down, high lets it float to host's pull-up
    funDigitalWrite( INT_PIN, int_on ? FUN_LOW : FUN_HIGH );

    // also clears on lift, else the last LED under the finger stays lit
    if( (regs[REG_CONFIG] & CONFIG_FEEDBACK) && (pos || last_pos) ) {
        __disable_irq();
        for( int i = 0; i < NUM_LEDS*3; i++ ) { regs[REG_LEDS + i] = 0; }
        if( pos ) {
            int led = (pos - 1) * NUM_LEDS / 255;
            wheel( pos, &regs[REG_LEDS + led*3] );
        }
        for( int i = 0; i < 9; i++ ) { regs[REG_LED0 + i] = regs[REG_LEDS + i]; }
        __enable_irq();
        leds_dirty = true;
    }
    last_pos = pos;
}

int main()
{
    SystemInit();
    funGpioInitAll();
    touch_init();
    WS2812BDMAInit();

    regs[REG_WHO_AM_I] = WHO_AM_I_VAL;
    regs[REG_VERSION] = FW_VERSION_VAL;
    // feedback on by default, like TouchwheelSAO
    regs[REG_CONFIG] = CONFIG_INT_ENABLE | CONFIG_AUTOSHOW | CONFIG_FEEDBACK;
    regs[REG_BRIGHTNESS] = BRIGHTNESS_DEFAULT;
    for( int i = 0; i < NUM_PADS; i++ ) {
        put_u16( thresh_reg(i), pads[i].thresh );
    }
    leds_dirty = true;  // show initial (all off)

    funPinMode( STATUS_LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );

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
                pads[i].thresh = get_u16( thresh_reg(i) );
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
