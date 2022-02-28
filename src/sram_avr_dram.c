/*
 * Copyright (c) 2016, Takashi TOYOSHIMA <toyoshim@gmail.com>
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

#include "hardware_config.h"
#include "sram.h"
#include "debug.h"
#include <avr/io.h>

// PB0: /OE - output
// PB1: /RAS - output
// PB2: /CASL - output
// PB3: /WE - output
// PD*: Address / Data - in/out

#define P_OE DDB0
#define P_RAS DDB1
#define P_CAS DDB2
#define P_WE DDB3
#define P_PORT PORTD
#define P_DDR DDRD
#define P_IN PIND

static void
rascas
(unsigned short addr)
{
  P_PORT = ((addr)&0xff);
  PORTB &= ~_BV(P_RAS);
  P_PORT = ((addr>>8)&0xff);
  PORTB &= ~_BV(P_CAS);
}

static unsigned long ref_coutner = 0;

unsigned char
sram_read
(unsigned short addr)
{
  unsigned char data;
  P_DDR = 0xff; // output
  P_PORT = ((addr)&0xff);
  PORTB &= ~_BV(P_RAS);
  P_PORT = ((addr>>8)&0xff);
  PORTB &= ~_BV(P_CAS);
  P_PORT = 0xf0;  // some magic
  asm volatile("nop");
  P_DDR = 0;    // input
  PORTB &= ~_BV(P_OE);
  asm volatile("nop");
  asm volatile("nop");
  data = P_IN;
  PORTB |= (_BV(P_RAS) | _BV(P_CAS) | _BV(P_OE));
  return data;
}

void
sram_write
(unsigned short addr, unsigned char data)
{
  P_DDR = 0xff; // output
  P_PORT = ((addr)&0xff);
  PORTB &= ~_BV(P_RAS);
  P_PORT = ((addr>>8)&0xff);
  PORTB &= ~_BV(P_CAS);
  P_PORT = data;
  PORTB &= ~_BV(P_WE);
  PORTB |= _BV(P_WE);
  PORTB |= (_BV(P_RAS) | _BV(P_CAS) | _BV(P_WE));
}

void
sram_init
(void)
{
  DDRB  |=  (_BV(P_OE) | _BV(P_RAS) | _BV(P_CAS) | _BV(P_WE));  // output
  PORTB |=  (_BV(P_OE) | _BV(P_RAS) | _BV(P_CAS) | _BV(P_WE));  // all high
  P_DDR = 0xff;   // output
  P_PORT = 0;
//  MCUCR |= (1<<4);  // Pull-up disable
}

void
dram_refresh
(void)
{
  static unsigned long n = 0;
  if (n++ < 10000) return;
  n = 0;
  // CAS-before-RAS refresh
  for (int i = 0; i < 512; ++i) {
    PORTB &= ~_BV(P_CAS);
    asm volatile("nop");
    PORTB &= ~_BV(P_RAS);
    asm volatile("nop");
    PORTB |= _BV(P_CAS);
    asm volatile("nop");
    PORTB |= _BV(P_RAS);
    asm volatile("nop");
  }
}
