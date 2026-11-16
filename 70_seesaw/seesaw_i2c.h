// seesaw_i2c.h -- minimal I2C target for seesaw-style transactions
// 14 Nov 2026 - @todbot / Tod Kurt
// ISR structure from ch32fun examples/i2c_slave/i2c_slave.h by Renze Nicolai (MIT)
//
// Seesaw hosts write [base, func, data...], STOP, wait a few ms, then read.
// So: collect a whole write, hand it to seesaw_on_write() on STOP,
// which fills seesaw_tx[] for the next read.

#ifndef SEESAW_I2C_H
#define SEESAW_I2C_H

#include "ch32fun.h"

#define SEESAW_RX_MAX (2 + 32)  // seesaw neopixel lib sends 24 byte chunks
#define SEESAW_TX_MAX (8)

// you implement this, called from I2C ISR on STOP after a write
void seesaw_on_write( uint8_t* buf, uint8_t len );

volatile uint8_t seesaw_rx[SEESAW_RX_MAX];
volatile uint8_t seesaw_rx_len;
volatile uint8_t seesaw_tx[SEESAW_TX_MAX];
volatile uint8_t seesaw_tx_len;
volatile uint8_t seesaw_tx_pos;

void seesaw_i2c_init( uint8_t address )
{
    RCC->APB1PCENR |= RCC_APB1Periph_I2C1;
    // pulse reset so peripheral regs start clean
    RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
    RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

    // FREQ = peripheral clock in MHz (RM: 8-48), needed for bus timing even as target
    I2C1->CTLR2 = (FUNCONF_SYSTEM_CORE_CLOCK / 1000000) & I2C_CTLR2_FREQ;
    // interrupt on events (ADDR, STOPF), buffer (RXNE, TXE), and errors
    I2C1->CTLR2 |= I2C_CTLR2_ITBUFEN | I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITERREN;
    NVIC_EnableIRQ( I2C1_EV_IRQn );
    NVIC_EnableIRQ( I2C1_ER_IRQn );

    I2C1->OADDR1 = address << 1;  // 7-bit addr lives in bits 7:1
    I2C1->CTLR1 |= I2C_CTLR1_PE;  // enable peripheral
    I2C1->CTLR1 |= I2C_CTLR1_ACK; // ACK our address and incoming bytes
}

void I2C1_EV_IRQHandler(void) __attribute__((interrupt));
void I2C1_EV_IRQHandler(void)
{
    // reading STAR1 then STAR2 is what clears the ADDR flag
    uint16_t star1 = I2C1->STAR1;
    uint16_t star2 = I2C1->STAR2;

    if( star1 & I2C_STAR1_ADDR ) {
        if( star2 & I2C_STAR2_TRA ) { seesaw_tx_pos = 0; }  // host is reading
        else                        { seesaw_rx_len = 0; }  // host is writing
    }

    if( star1 & I2C_STAR1_RXNE ) {
        uint8_t b = I2C1->DATAR;  // must read DATAR even if dropping it
        if( seesaw_rx_len < SEESAW_RX_MAX ) { seesaw_rx[seesaw_rx_len++] = b; }
    }

    if( star1 & I2C_STAR1_TXE ) {
        I2C1->DATAR = (seesaw_tx_pos < seesaw_tx_len) ? seesaw_tx[seesaw_tx_pos++] : 0;
    }

    if( star1 & I2C_STAR1_STOPF ) {
        I2C1->CTLR1 |= 0;  // a write to CTLR1 (after STAR1 read) clears STOPF
        if( seesaw_rx_len ) {
            seesaw_on_write( (uint8_t*)seesaw_rx, seesaw_rx_len );
            seesaw_rx_len = 0;
        }
    }
}

void I2C1_ER_IRQHandler(void) __attribute__((interrupt));
void I2C1_ER_IRQHandler(void)
{
    // AF = host NACKed our last byte, normal at end of a read
    I2C1->STAR1 &= ~(I2C_STAR1_BERR | I2C_STAR1_ARLO | I2C_STAR1_AF | I2C_STAR1_OVR);
}

#endif
