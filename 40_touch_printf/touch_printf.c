// touch_printf.c -- read captouch pads, printf values for tuning
// 14 Nov 2026 - @todbot / Tod Kurt
//
// View with "make monitor". Each line per pad: raw baseline delta,
// then touched bits and wheel pos.
// Use it to pick TOUCH_ITERS and TOUCH_THRESH_DEFAULT in touch.h

#include "ch32fun.h"
#include <stdio.h>

#define NUM_PADS (4)
#define TOUCH_WHEEL (0)
// {port, pin, adc_chan}
#define TOUCH_PINS { {GPIOC,4,2}, {GPIOD,2,3}, {GPIOD,3,4}, {GPIOD,4,7} }
#include "touch.h"

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
    touch_init();
    printf("touch_printf: %d pads\n", NUM_PADS);
       
    while(1) {
        touch_update();
        for( int i = 0; i < NUM_PADS; i++ ) {
            printf("%5d %5d %4d | ", pads[i].raw, pads[i].baseline, touch_delta(i));
        }
#if TOUCH_WHEEL
        printf("wheel:%s %3d\n", bits_str( touch_bits() ), touch_wheel_pos());
#else
        printf("buttons:%s\n", bits_str( touch_bits() ));
#endif
        Delay_Ms( 20 );
    }
}
