// i2c_oled.c -- I2C peripheral that draws on a 128x64 SSD1306 OLED
// 20 Nov 2026 - @todbot / Tod Kurt
//
// - I2C SDA on PC1, SCL on PC2, needs pull-ups
// - OLED on bit-banged I2C (SDA PC3, SCL PC4), the only device on those pins
// - Host writes [REG_COMMAND, cmd, args...], polls STATUS until not busy,
//   then sends the next. Draw commands only change the framebuffer, CMD_SHOW
//   sends it to the OLED
// - Register and command map in ../README.md

#define SSD1306_128X64  // or SSD1306_128X32
#define SSD1306_I2C_BITBANG_SDA PC3
#define SSD1306_I2C_BITBANG_SCL PC4
// nops per bit-bang delay step. lib default of 1 clocks in MHz, past SSD1306's 400 kHz spec
#define I2CSPEEDBASE 10
// lib's init pulses a reset pin, but 4-pin OLED modules have none.
// pulsing SDA w/ SCL high instead is just a START + STOP
#define SSD1306_RST_PIN SSD1306_I2C_BITBANG_SDA

#include "ch32fun.h"
#include <stdio.h>
#include "i2c_slave.h"
#include "ssd1306_i2c_bitbang.h"
#include "ssd1306.h"

#define I2C_ADDR (0x55)  // not 0x54, so it can share a bus with the touchwheel
#define WHO_AM_I_VAL (0xC4)
#define FW_VERSION_VAL (90)
#define ARGS_LEN (40)

enum {
    REG_WHO_AM_I   = 0x00,
    REG_FW_VERSION = 0x01,
    REG_STATUS     = 0x02,  // bit0 = busy
    REG_CONFIG     = 0x04,
    REG_CONTRAST   = 0x05,
    REG_COMMAND    = 0x06,
    REG_ARGS       = 0x07,  // ARGS_LEN bytes
    REG_NUMREGS    = REG_ARGS + ARGS_LEN,
};

#define STATUS_BUSY     (1<<0)
#define CONFIG_AUTOSHOW (1<<1)  // show after every draw command

enum {
    CMD_SHOW       = 0x01,
    CMD_CLEAR      = 0x10,  // color
    CMD_PIXEL      = 0x11,  // x, y, color
    CMD_LINE       = 0x12,  // x0, y0, x1, y1, color
    CMD_RECT       = 0x13,  // x, y, w, h, color
    CMD_FILLRECT   = 0x14,  // x, y, w, h, color
    CMD_CIRCLE     = 0x15,  // x, y, r, color
    CMD_FILLCIRCLE = 0x16,  // x, y, r, color
    CMD_PRINT      = 0x17,  // x, y, size, color, text...
    CMD_INVERT     = 0x18,  // on, done by the OLED so no show needed
    CMD_RESET      = 0xA5,
};

volatile uint8_t regs[REG_NUMREGS];
volatile uint8_t cmd;  // nonzero while a command is pending
volatile bool contrast_dirty = false;
uint8_t args[ARGS_LEN + 1];  // +1 so PRINT text is always terminated

// called from I2C ISR on STOP, keep it short and no printf
void on_i2c_write( uint8_t reg, uint8_t len )
{
    if( !i2c_slave_state.writing ) { return; }  // also called after reads

    int end = reg + len;
    if( reg <= REG_CONTRAST && end > REG_CONTRAST ) { contrast_dirty = true; }
    if( reg <= REG_COMMAND && end > REG_COMMAND && regs[REG_COMMAND] && !cmd ) {
        // copy args out, so the host can't change them mid-draw.
        // zero the rest, else a short PRINT picks up an older, longer one's text
        int nargs = end - REG_ARGS;
        for( int i = 0; i < ARGS_LEN; i++ ) {
            args[i] = (i < nargs) ? regs[REG_ARGS + i] : 0;
        }
        cmd = regs[REG_COMMAND];
        regs[REG_STATUS] = STATUS_BUSY;
    }
    regs[REG_COMMAND] = 0;  // a command sent while busy is dropped
}

static void print_str( int x, int y, int size, int color, const char* s )
{
    if( size < 1 ) { size = 1; }
    if( size > 8 ) { size = 8; }
    for( ; *s && x < SSD1306_W; s++, x += 8*size ) {
        ssd1306_drawchar_sz( x, y, *s, color, size );
    }
}

static void run_cmd( uint8_t c )
{
    uint8_t* a = args;
    switch( c ) {
        case CMD_SHOW:       ssd1306_refresh(); return;
        case CMD_INVERT:     ssd1306_cmd( a[0] ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY ); return;
        case CMD_CLEAR:      ssd1306_setbuf( a[0] ); break;
        case CMD_PIXEL:      ssd1306_drawPixel( a[0], a[1], a[2] ); break;
        case CMD_LINE:       ssd1306_drawLine( a[0], a[1], a[2], a[3], a[4] ); break;
        case CMD_RECT:       ssd1306_drawRect( a[0], a[1], a[2], a[3], a[4] ); break;
        case CMD_FILLRECT:   ssd1306_fillRect( a[0], a[1], a[2], a[3], a[4] ); break;
        case CMD_CIRCLE:     ssd1306_drawCircle( a[0], a[1], a[2], a[3] ); break;
        case CMD_FILLCIRCLE: ssd1306_fillCircle( a[0], a[1], a[2], a[3] ); break;
        case CMD_PRINT:      print_str( a[0], a[1], a[2], a[3], (char*)&a[4] ); break;
        case CMD_RESET:      NVIC_SystemReset(); break;
        default: return;
    }
    if( regs[REG_CONFIG] & CONFIG_AUTOSHOW ) { ssd1306_refresh(); }
}

int main()
{
    SystemInit();
    Delay_Ms( 100 );  // OLED needs time to power up
    ssd1306_i2c_setup();
    if( ssd1306_init() ) {
        printf("no OLED at 0x%x\n", SSD1306_I2C_ADDR);
        while(1) { }
    }

    regs[REG_WHO_AM_I] = WHO_AM_I_VAL;
    regs[REG_FW_VERSION] = FW_VERSION_VAL;
    regs[REG_CONTRAST] = 0x8F;  // lib's init value

    print_str( 0, 0, 1, 1, "90_i2c_oled" );
    char str[12];
    sprintf( str, "addr 0x%x", I2C_ADDR );
    print_str( 0, 12, 1, 1, str );
    ssd1306_refresh();

    // alternate function open-drain: I2C peripheral drives the pins
    funPinMode( PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SDA
    funPinMode( PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD ); // SCL
    SetupI2CSlave( I2C_ADDR, regs, sizeof(regs), on_i2c_write, NULL, false );

    while(1) {
        if( contrast_dirty ) {
            contrast_dirty = false;
            ssd1306_cmd( SSD1306_SETCONTRAST );
            ssd1306_cmd( regs[REG_CONTRAST] );
        }
        if( cmd ) {
            run_cmd( cmd );
            __disable_irq();  // so ISR doesn't take a new cmd between these two
            regs[REG_STATUS] = 0;
            cmd = 0;
            __enable_irq();
        }
    }
}
