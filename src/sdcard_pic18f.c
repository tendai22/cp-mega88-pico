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

#include "hardware_config.h"
#include "sdcard.h"

#include <XC.h>

// CS RB0
// DI/MOSI RB1
// DO/MISO RB2
// CK/SCK RB3

#define PIN_LOW(x) (x = 0)
#define PIN_HIGH(x) (x = 1)
#define P_CK LATB3
//#define P_PU _BV(PINC4)
#define P_DO LATB2
#define P_DI LATB1
#define P_CS LATB0

static unsigned char
buffer[512];

static unsigned short
crc;

static unsigned long
cur_blk;

static unsigned char
ccs;

static void
sd_out
(char c)
{
  int i;
  for (i = 7; i >= 0; i--) {
    PIN_LOW(P_CK);
    if (c & (1 << i)) PIN_HIGH(P_DI);
    else PIN_LOW(P_DI);
    PIN_HIGH(P_CK);
  }
  PIN_LOW(P_CK);
}

static int
sd_busy
(char f)
{
  unsigned long timeout = 0xffff;
  for (; 0 != timeout; timeout--) {
    char c;
    PIN_HIGH(P_CK);
    c = PORTB;
    PIN_LOW(P_CK);
    if ((f & (1<<2)) == (c & (1<<2))) return 0;
  }
  return -1;
}

static unsigned long
sd_in
(char n)
{
  int i;
  unsigned long rc = 0;
  for (i = 0; i < n; i++) {
    char c;
    PIN_HIGH(P_CK);
    c = PORTB;
    PIN_LOW(P_CK);
    rc <<= 1;
    if (c & (1<<2)) rc |= 1;
  }
  return rc;
}

static unsigned long
sd_cmd
(char cmd, char arg0, char arg1, char arg2, char arg3, char crc)
{
  unsigned char rc;
  PIN_HIGH(P_DI);
  PIN_LOW(P_CS);

  sd_out(cmd);
  sd_out(arg0);
  sd_out(arg1);
  sd_out(arg2);
  sd_out(arg3);
  sd_out(crc);
  PIN_HIGH(P_DI);
  if (sd_busy(0) < 0) return 0xffffffff;
  // The first response bit was already consumed by sd_busy.
  // Read remaining response bites.
  if (0x48 == cmd) return sd_in(39);  // R7
  if (0x7a == cmd) return sd_in(39);  // R3
  return sd_in(7);  // R1
}

void
sdcard_init
(void)
{
  /*
   * CK/DI/CS: out, low
   * DO: in
   */
  // Port Settings
  // CS RB0 - output high
  // DI/MOSI RB1 - output, high
  // DO/MISO RB2 - input
  // CK/SCK RB3 - output, low
  ANSELB0 = 0;  // disable analog function
  LATB0 = 1;    // CS initial value
  TRISB0 = 0;   // set as output

  ANSELB1 = 0;  // disable analog function
  LATB1 = 1;    // DI initial value
  TRISB1 = 0;   // set as output

  ANSELB2 = 0;  // disable analog function
//  LATB2 = 1;    // DO initial value
  TRISB2 = 1;   // set as input

  ANSELB3 = 0;  // disable analog function
  LATB3 = 0;    // CK initial value
  TRISB3 = 0;   // set as output

  cur_blk = 0;
}

int
sdcard_open
(void)
{
  // initial clock
  PIN_HIGH(P_DI);
  PIN_HIGH(P_CS);
  int i;
  for (i = 0; i < 80; i++) {
    PIN_HIGH(P_CK);
    PIN_LOW(P_CK);
  }
  unsigned long rc;
  // cmd0 - GO_IDLE_STATE (response R1)
  rc = sd_cmd(0x40, 0x00, 0x00, 0x00, 0x00, 0x95);
  if (1 != rc) return -1;

  // cmd8 - SEND_IF_COND (response R7)
  rc = sd_cmd(0x48, 0x00, 0x00, 0x01, 0x0aa, 0x87);
  if ((rc & 0x00000fff) != 0x1aa) {
    // SDC v2 initialization failed, try legacy command.
    // cmd1 - SEND_OP_COND (response R1)
    for (;;) {
      rc = sd_cmd(0x41, 0x00, 0x00, 0x00, 0x00, 0x00);
      if (0 == rc) break;
      if (0xffffffff == rc) return -2;
    }
    if (0 != rc) return -3;
    ccs = 0;
  } else {
    do {
      // cmd55 - APP_CMD (response R1)
      rc = sd_cmd(0x77, 0x00, 0x00, 0x00, 0x00, 0x01);
      // acmd41 - SD_SEND_OP_COND (response R1)
      rc = sd_cmd(0x69, 0x40, 0x00, 0x00, 0x00, 0x77);
    } while (0 != rc);  // no errors, no idle
    do {
      // cmd58 - READ_OCR (response R3)
      rc = sd_cmd(0x7a, 0x00, 0x00, 0x00, 0x00, 0xfd);
    } while (0 == (rc & 0x80000000));  // card powerup not completed
    ccs = (rc & 0x40000000) ? 1 : 0; // ccs bit high means SDHC card
  }
  PIN_LOW(P_CK);
  return 0;
}

int
sdcard_fetch_sec
(unsigned long blk_addr)
{
  cur_blk = blk_addr;
  if (0 == ccs) blk_addr <<= 9; // SD cards use byte-offset address
  // cmd17
  unsigned long rc =
    sd_cmd(0x51, (unsigned char)(blk_addr >> 24), (unsigned char)(blk_addr >> 16), (unsigned char)(blk_addr >> 8), (unsigned char)blk_addr, 0x00);
  if (0 != rc) return -1;
  if (sd_busy(0) < 0) return -2;
  int i;
  for (i = 0; i < 512; i++) buffer[i] = (unsigned char)sd_in(8);
  rc = sd_in(8);
  crc = (rc << 8) | (unsigned char)sd_in(8);

  // XXX: rc check

  PIN_LOW(P_CK);
  return 0;
}

int
sdcard_fetch
(unsigned long blk_addr)
{
  return sdcard_fetch_sec(blk_addr >> 9);
}

int
sdcard_store_sec
(unsigned long blk_addr)
{
  if (0 == ccs) blk_addr <<= 9; // SD cards accecpt byte address
  // cmd24
  unsigned long rc =
    sd_cmd(0x58, (unsigned char)(blk_addr >> 24), (unsigned char)(blk_addr >> 16), (unsigned char)(blk_addr >> 8), (unsigned char)blk_addr, 0x00);
  if (0 != rc) return -1;
  sd_out(0xff); // blank 1B
  sd_out(0xfe); // Data Token
  int i;
  for (i = 0; i < 512; i++) sd_out(buffer[i]); // Data Block
  sd_out(0xff); // CRC dummy 1/2
  sd_out(0xff); // CRC dummy 2/2
  if (sd_busy(0) < 0) return -3;
  rc = sd_in(4);
  if (sd_busy(~0) < 0) return -4;
  if (rc != 5) return rc ? -rc : -5;
  PIN_LOW(P_CK);

  return 0;
}

int
sdcard_store
(unsigned long blk_addr)
{
  return sdcard_store_sec(blk_addr >> 9); // pass sector address
}

unsigned short
sdcard_crc
(void)
{
  return crc;
}

int
sdcard_flush
(void)
{
  return sdcard_store(cur_blk);
}

void *
sdcard_buffer
(void)
{
  return buffer;
}
