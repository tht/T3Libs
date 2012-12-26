/*
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@bug.st>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>

#include "w5200.h"

// W5200 controller instance
W5200Class W5200;

#define TX_RX_MAX_BUF_SIZE 2048
#define TX_BUF 0x1100
#define RX_BUF (TX_BUF + TX_RX_MAX_BUF_SIZE)

#define TXBUF_BASE 0x8000
#define RXBUF_BASE 0xC000

void W5200Class::init(void) {
    
    // configure SPI pins
    digitalWriteFast(SCK, LOW);
    digitalWriteFast(MOSI, LOW);
    digitalWriteFast(9, HIGH);
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(9, OUTPUT);
    
    // enables and configures SPI module
    SIM_SCGC6 |= SIM_SCGC6_SPI0;  // enable SPI clock
    SPI0_MCR = 0x80004000;
    SPCR |= _BV(MSTR);
    SPCR |= _BV(SPE);
    SPCR &= ~(_BV(DORD));   // MSBFIRST SPI
    
    // resetting module
    writeMR(1<<7);
    
    for (int i=0; i<MAX_SOCK_NUM; i++) {
        SBASE[i] = TXBUF_BASE + SSIZE * i;
        RBASE[i] = RXBUF_BASE + RSIZE * i;
    }
}

uint16_t W5200Class::getTXFreeSize(SOCKET s) {
    uint16_t val=0, val1=0;
    do {
        val1 = readSnTX_FSR(s);
        if (val1 != 0)
            val = readSnTX_FSR(s);
    }
    while (val != val1);
    return val;
}

uint16_t W5200Class::getRXReceivedSize(SOCKET s) {
    uint16_t val=0,val1=0;
    do {
        val1 = readSnRX_RSR(s);
        if (val1 != 0)
            val = readSnRX_RSR(s);
    }
    while (val != val1);
    return val;
}


void W5200Class::send_data_processing(SOCKET s, const uint8_t *data, uint16_t len) {
    // This is same as having no offset in a call to send_data_processing_offset
    send_data_processing_offset(s, 0, data, len);
}

void W5200Class::send_data_processing_offset(SOCKET s, uint16_t data_offset, const uint8_t *data, uint16_t len) {
    Serial.print("send_data_processing_offset(");
    Serial.print(s, DEC); Serial.print(", ");
    Serial.print(data_offset, HEX); Serial.print(", 0x...., ");
    Serial.print(len, DEC); Serial.println(");");
    
    uint16_t ptr = readSnTX_WR(s);
    ptr += data_offset;
    uint16_t offset = ptr & SMASK;
    uint16_t dstAddr = offset + SBASE[s];
    
    if (offset + len > SSIZE)
    {
        // Wrap around circular buffer
        uint16_t size = SSIZE - offset;
        write(dstAddr, data, size);
        write(SBASE[s], data + size, len - size);
    }
    else {
        write(dstAddr, data, len);
    }
    
    ptr += len;
    writeSnTX_WR(s, ptr);
}


void W5200Class::recv_data_processing(SOCKET s, uint8_t *data, uint16_t len, uint8_t peek) {
    uint16_t ptr;
    ptr = readSnRX_RD(s);
    read_data(s, ptr, data, len);
    if (!peek)
    {
        ptr += len;
        writeSnRX_RD(s, ptr);
    }
}

void W5200Class::read_data(SOCKET s, uint16_t src, volatile uint8_t *dst, uint16_t len) {
    uint16_t size;
    uint16_t src_mask;
    uint16_t src_ptr;
    
    src_mask = (uint16_t)src & RMASK;
    src_ptr = RBASE[s] + src_mask;
    
    if( (src_mask + len) > RSIZE )
    {
        size = RSIZE - src_mask;
        read(src_ptr, (uint8_t *)dst, size);
        dst += size;
        read(RBASE[s], (uint8_t *) dst, len - size);
    }
    else
        read(src_ptr, (uint8_t *) dst, len);
}


uint8_t W5200Class::write(uint16_t addr, uint8_t data) {
    return write(addr, &data, 1);
}

uint16_t W5200Class::write(uint16_t addr, const uint8_t *data, uint16_t data_len) {
    // 16MHz 8bit transfers on CTAR0
    SPI0_CTAR0 = 0xB8010000;
    digitalWriteFast(9, LOW);
    
    SPI0_PUSHR = (1<<26) | ((addr & 0xFF00) >> 8);
    SPI0_PUSHR =           ( addr & 0xFF);
    
    SPI0_PUSHR =           0x80 | ((data_len & 0x7F00) >> 8);
    SPI0_PUSHR =           ( data_len & 0xFF);
    
    for (int i=0; i<data_len; i++) {
        while ((0xF & (SPI0_SR >> 12)) == 4) ; // wait while TX fifo is full
        SPI0_PUSHR =        *(data+i);
    }
    
    // add 4 control bytes to data_len and shift to match SPI0_TCR
    uint32_t data_sent = data_len + 4;
    
    data_sent <<= 16;
    while (SPI0_TCR != data_sent) ; // loop until transfer is complete
    
    digitalWriteFast(9, HIGH);
    return data_len;
}


uint8_t W5200Class::read(uint16_t _addr) {
    uint8_t res;
    read(_addr, &res, 1);
    return res;
}

uint16_t W5200Class::read(uint16_t addr, uint8_t *data, uint16_t data_len) {
    // 16MHz 8bit transfers on CTAR0
    SPI0_CTAR0 = 0xB8010000;
    digitalWriteFast(9, LOW);
    
    SPI0_PUSHR = (1<<26) | ((addr & 0xFF00) >> 8);
    SPI0_PUSHR =           ( addr & 0xFF);
    
    SPI0_PUSHR =           0x00 | ((data_len & 0x7F00) >> 8);
    SPI0_PUSHR =           ( data_len & 0xFF);
    
    while ((0xF & (SPI0_SR >> 12)) != 0) ; // wait until all is sent
    bitSet(SPI0_MCR, 10);                  // clear RX FIFO
    
    while ((0xF & (SPI0_SR >> 4)) != 0)    // discard everything from RX FIFO
        uint8_t discard = SPI0_POPR;
    
    // preloading TX buffer
    uint8_t to_send = data_len;
    while ( (0xF & (SPI0_SR >> 12)) != 4 && to_send) {
        SPI0_PUSHR = 0;
        --to_send;
    }
    
    for (int i=0; i<data_len; i++) {
        while ((0xF & (SPI0_SR >> 4)) == 0) ; // wait until we get a byte
        data[i] = SPI0_POPR;
        
        if ( to_send) {
            SPI0_PUSHR = 0;
            --to_send;
        }
    }
    
    digitalWriteFast(9, HIGH);
    return data_len;
}


void W5200Class::execCmdSn(SOCKET s, SockCMD _cmd) {
    // Send command to socket
    writeSnCR(s, _cmd);
    // Wait for command to complete
    while (readSnCR(s))
        ;
}
