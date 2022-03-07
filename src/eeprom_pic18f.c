/*
 * Copyright (c) 2022, Norihiro Kumagai <tendai22plus@gmail.com>
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

#include "eeprom.h"

#include <XC.h>

//
// address range: 0-255
//

void
my_eeprom_write
(unsigned short addr, unsigned char data)
{
    unsigned char GIEBitValue = INTCON0bits.GIE;
    NVMADR = addr;
    NVMDATL = data;
    NVMCON1bits.CMD = 0x03;
    INTCON0bits.GIE = 0;
    NVMLOCK = 0x55;
    NVMLOCK = 0xaa;
    NVMCON0bits.GO = 1;
    while (NVMCON0bits.GO);
//    if (NVMCON1bits.WRERR) {
        // write fault recovery
//    }
    INTCON0bits.GIE = GIEBitValue;
    NVMCON1bits.CMD = 0;
}

unsigned char
my_eeprom_read
(unsigned short addr)
{
    NVMADR = addr;
    NVMCON1bits.CMD = 0;
    NVMCON0bits.GO = 1;
    while (NVMCON0bits.GO);
    return NVMDATL;
}
