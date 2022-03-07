/*
 * Copyright (c) 2021, Norihiro Kumagai <tendai22plus@gmail.com>
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

#include "sram.h"

#include <XC.h>

  // RA0: /OE - output
  // RA1: /RAS - output
  // RA2: /CASL - output
  // RA3: /OE
  // RD*: Address - out
  // RC*: Data - in/out


unsigned char
sram_read
(unsigned short addr)
{
  unsigned char data;
  LATD = addr >> 8;
  LATA1 = 0;    // /RAS = 0
  LATD = addr & 0xff;
  LATA2 = 0;    // /CASL = 0;
  LATA3 = 0;    // /OE = 0;
  data = PORTC;
  LATA |= ((1<<0)|(1<<1)|(1<<2)|(1<<3));
  return data;
}

void
sram_write
(unsigned short addr, unsigned char data)
{
  LATD = addr >> 8;
  LATA1 = 0;    // /RAS = 0
  LATD = addr & 0xff;
  LATA2 = 0;    // /CASL = 0;
  LATD = data;
  TRISD = 0;    // RD0-7 as output;
  LATA |= ((1<<0)|(1<<1)|(1<<2)|(1<<3));
}

void
sram_init
(void)
{
  // RA0: /OE - output
  // RA1: /RAS - output
  // RA2: /CASL - output
  // RA3: /OE
  // RD*: Address - out
  // RC*: Data - in/out

  ANSELA0 = 0;  // disable analog function
  LATA0 = 1;    // /WE initial value
  TRISA0 = 0;   // set as output

  ANSELA1 = 0;  // disable analog function
  LATA1 = 1;    // /WE initial value
  TRISA1 = 0;   // set as output

  ANSELA2 = 0;  // disable analog function
  LATA2 = 1;    // /WE initial value
  TRISA2 = 0;   // set as output

  ANSELA3 = 0;  // disable analog function
  LATA3 = 1;    // /WE initial value
  TRISA3 = 0;   // set as output

  ANSELD = 0; // disable analog function
  WPUD = 0;   // Weak-pullup disable
  TRISD = 0;  // Set as output
  LATD = 0;   // initial value for address bus

  ANSELC = 0; // disable analog function
  WPUC = 0;   // Weak-pullup disable
  TRISC = 0xff;  // Set as input

}
