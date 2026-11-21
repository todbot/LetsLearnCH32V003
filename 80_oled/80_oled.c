// 80_oled.c -- draw text and graphics on a 128x64 SSD1306 I2C OLED
// 16 Nov 2026 - @todbot / Tod Kurt
//
// OLED on bit-banged I2C (SDA PC3, SCL PC4), leaving I2C1 (PC1/PC2) free for
// 90 to be an I2C peripheral. The OLED must be the only device on these pins.

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
#include "ssd1306_i2c_bitbang.h"
#include "ssd1306.h"

int main()
{
    SystemInit();
    Delay_Ms( 100 );  // OLED needs time to power up
    ssd1306_i2c_setup();
    if( ssd1306_init() ) {
        printf("no OLED at 0x%x\n", SSD1306_I2C_ADDR);
        while(1) { }
    }

    int count = 0;
    while(1) {
        ssd1306_setbuf( 0 );
        ssd1306_drawstr( 0, 0, "80_oled", 1 );
        char str[8];
        sprintf( str, "%d", count );
        ssd1306_drawstr_sz( 0, 12, str, 1, fontsize_16x16 );
        int x = count % SSD1306_W;
        ssd1306_fillRect( x, SSD1306_H - 4, 8, 4, 1 );
        ssd1306_fillRect( (x*2) % SSD1306_W, SSD1306_H - 10, 8, 4, 1 );

        uint32_t start = SysTick->CNT;
        ssd1306_refresh();
        if( count == 0 ) {
            printf("refresh: %d us\n", (int)((SysTick->CNT - start) / Ticks_from_Us(1)));
        }
        count++;
    }
}
