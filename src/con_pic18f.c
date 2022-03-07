/*
 * Copyright (c) 2021, Norihiro KUMAGAI <tendai22plus@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the authors nor the names of its contributors may be
 *   used to endorse or promote products derived from this software with out
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUE
 * NTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "conx.h"

#include <xc.h>
#include <stdio.h>

#define _XTAL_FREQ 64000000L

static void
sleep
(void)
{
  __delay_us(100);   // 100us wait
}

// UART3 Transmit
void
con_putchar
(unsigned char c)
{
    while(!U3TXIF); // Tx interrupt flag not set
    U3TXB = c;
}
 
// UART3 Recive
int
con_getchar
(void)
{
    while(!U3RXIF); // Rx interrupt flag not set
    return U3RXB;
}

int
con_peek
(void)
{
    return U3RXIF ? 1 : 0;
}

int
getchar_timeout_us
(unsigned long micros)
{
    unsigned long count = (micros + 9) / 10;
    while (count-- > 0) {
        if (U3RXIF) {
            return U3RXB;
        }
        __delay_us(10);
    }
    return -1;
}

//
// printf workhorse function in XC8 compiler/library
//
void
putch
(unsigned char c)
{
    con_putchar(c);
}

char
getch
(void)
{
    return (char)con_getchar(); 
}

void
con_init
(void)
{
    // UART3 initialize
    U3CON0 |= (1<<7);   // BRGS = 0, 4 baud clocks per bit
//    U3BRG = 416; // 9600bps @ 64MHz
    U3BRG = 138;    // 115200bps @ 64MHz, BRG=0, 99%
    
    U3TXEN = 1; // Transmitter enable
    U3RXEN = 1; // Receiver enable
     
    RA6PPS = 0x26;  // RA6->UART3TX
    TRISA6 = 0; // TX set as output
    ANSELA6 = 0; // Disable analog function
 
    U3RXPPS = 0x07; // UART3RX->RA7
    TRISA7 = 1; // RX set as input
    ANSELA7 = 0; // Disable analog function
 
//    U3RXPPS = 0x05; // UART3RX->RA5
//    TRISA5 = 1; // RX set as input
//    ANSELA5 = 0; // Disable analog function
     
    U3ON = 1; // Serial port enable
     
//    printf("hello, world\r\n");
}
