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

#include "sdcard.h"

#include <stdio.h>
#include "pico/stdlib.h"
//#include "hardware/clocks.h"
//#include "hardware/pll.h"
//#include "hardware/clocks.h"
//#include "hardware/structs/pll.h"
//#include "hardware/structs/clocks.h"

// RX  GP00
// CS  GP01
// SCK GP02
// TX  GP03

#define PORT XXX
#define DDR  YYY
#define PIN  gpio_get_all()

# define P_CK (1<<2)
# define P_DI (1<<3)
# define P_DO (1<<0)
# define P_CS (1<<1)

# define PIN_HIGH(x) gpio_set_mask(x)
# define PIN_LOW(x)  gpio_clr_mask(x)
# define WAIT_NOP(x) do { int n = x; while(n>0){ asm volatile("nop"); n--; }}while(0)

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
    WAIT_NOP(60);
    PIN_HIGH(P_CK);
    WAIT_NOP(60);
  }
  WAIT_NOP(60);   // hold SCK high for a few us
  PIN_LOW(P_CK);
  WAIT_NOP(60);
}

static int
sd_busy
(char f)
{
  unsigned long timeout = 0xffff;
  uint32_t c;
  c = PIN;
  if ((f & P_DO) == (c & P_DO)) return 0;
  for (; 0 != timeout; timeout--) {
    PIN_HIGH(P_CK);
    WAIT_NOP(3);
    c = PIN;
    WAIT_NOP(57);
    PIN_LOW(P_CK);
    if ((f & P_DO) == (c & P_DO)) return 0;
    WAIT_NOP(60);
  }
  return -1;
}

static unsigned long
sd_in
(char n)
{
  int i;
  unsigned long rc = 0;
  WAIT_NOP(60);
  for (i = 0; i < n; i++) {
    uint32_t c;
    PIN_HIGH(P_CK);
    WAIT_NOP(3);
    c = PIN;
    WAIT_NOP(57);
    PIN_LOW(P_CK);
    rc <<= 1;
    if (c & P_DO) rc |= 1;
    WAIT_NOP(60);
  }
  return rc;
}

static unsigned long
sd_cmd
(char cmd, char arg0, char arg1, char arg2, char arg3, char crc)
{
  unsigned long rc;
  printf("[%02X %02X %02X %02X %02X (%02X)]->", cmd, arg0, arg1, arg2, arg3, crc);
  if (sd_busy(P_DO) < 0) {
    printf("FFFFFFFF\n");
    return 0xffffffff;
  }
  PIN_HIGH(P_DI);
  PIN_LOW(P_CS);

  WAIT_NOP(100);

  sd_out(cmd);
  sd_out(arg0);
  sd_out(arg1);
  sd_out(arg2);
  sd_out(arg3);
  sd_out(crc);
  PIN_HIGH(P_DI);
  for (int i = 0; ((rc = sd_in(8)) & 0x80) && i != 0xff; ++i)
    ;
  // Read remaining response bites.
  if (0x48 == cmd) {
    printf("%02X:", rc);
    if (rc & 0x04)
      return rc;
    rc = sd_in(32);
    printf("%X\n", rc);
    return rc;  // R7
  }
  if (0x7a == cmd) {
    printf("%02X:", rc);
    rc = sd_in(32);
    printf("%X\n", rc);
    return rc;  // R3
  }
  printf("%02X\n", rc);
    //else printf(".");
  return rc;  // R1
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
#if !defined(MCU_PICO)
  DDR |=  (P_CK | P_DI | P_CS);
  PORT &= ~(P_CK | P_DI | P_CS);

  DDR &= ~P_DO;
  PORT |= P_PU;
#else
  sleep_ms(1);
  gpio_init_mask(P_CS|P_DI|P_DO|P_CK);
  gpio_set_dir_in_masked(P_DO);
  gpio_set_dir_out_masked(P_CS|P_DI|P_CK);
  gpio_clr_mask(P_CS|P_DI|P_CK);
#endif //!defined(MCU_PICO)
  cur_blk = 0;
}

int
sdcard_open
(void)
{
  // initial clock
  PIN_HIGH(P_DI | P_CS);
  WAIT_NOP(60);
  int i;
  #if 0
  for (i = 0; i < 80; i++) {
    PIN_HIGH(P_CK);
    WAIT_NOP(60);
    PIN_LOW(P_CK);
    WAIT_NOP(60);
  }
  #else
  for (i = 0; i < 10; ++i) {
    sd_out(0xff);
  }
  #endif
  unsigned long rc;
  // cmd0 - GO_IDLE_STATE (response R1)
  rc = sd_cmd(0x40, 0x00, 0x00, 0x00, 0x00, 0x95);
  if (1 != rc) return -1;

  // cmd8 - SEND_IF_COND (response R7)
  rc = sd_cmd(0x48, 0x00, 0x00, 0x01, 0x0aa, 0x87);
  int type;
  if ((rc & 0x04) == 0x04) {  // CMD8 error
    type = 1;   // SD1
    printf("SD Ver.1\n");
  } else if ((rc & 0x00000fff) != 0x1aa) {
    printf("ERR\n");
    goto error;
  } else {
    printf("SD Ver.2\n");
    // 0x1aa
    type = 2; // SD2
  }
  int arg = (type == 2) ? 0X40 : 0;
  int count = 0;
  do {
    uint32_t rc1;
    // cmd55 - APP_CMD (response R1)
    rc1 = sd_cmd(0x77, 0x00, 0x00, 0x00, 0x00, 0x01);
    // acmd41 - SD_SEND_OP_COND (response R1)
    rc = sd_cmd(0x69, arg, 0x00, 0x00, 0x00, 0x77/*0x77*/);
    sleep_ms(100);
    if (count++ >= 50) goto error;
  } while (rc != 0);
  ccs = (rc & 0x40000000) ? 1 : 0; // ccs bit high means SDHC card

  if (type == 2) {
    // SD TYPE 2
    // cmd58 - READ_OCR (response R3)
    rc = sd_cmd(0x7a, 0x00, 0x00, 0x00, 0x00, 0xff);//0xfd);
    ccs = (rc & 0x40000000) ? 1 : 0; // ccs bit high means SDHC card
    if ((rc & 0xC0000000) == 0xC0000000) {
      type = 3; // SDHC
      printf ("SDHC\n");
    } else {
      printf("SD Ver.2+\n");
      rc = sd_cmd(0x50, 0x00, 0x00, 0x02, 0x00, 0x00);
      // force 512 byte/block
    }
  }
  PIN_LOW(P_CK);
  return 0;
error:
  PIN_HIGH(P_CS);
  return -1;
}

int
sdcard_fetch
(unsigned long blk_addr)
{
  cur_blk = blk_addr;
  if (0 != ccs) blk_addr >>= 9; // SDHC cards use block addresses
  // cmd17
  unsigned long rc =
    sd_cmd(0x51, blk_addr >> 24, blk_addr >> 16, blk_addr >> 8, blk_addr, 0x00);
  if (0 != rc) return -1;
  if (sd_busy(0) < 0) return -2;
  int i;
  for (i = 0; i < 512; i++) buffer[i] = sd_in(8);
  rc = sd_in(8);
  crc = (rc << 8) | sd_in(8);

  // XXX: rc check

  PIN_LOW(P_CK);
  return 0;
}

int
sdcard_store
(unsigned long blk_addr)
{
  if (0 != ccs) blk_addr >>= 9; // SDHC cards use block addresses
  // cmd24
  unsigned long rc =
    sd_cmd(0x58, blk_addr >> 24, blk_addr >> 16, blk_addr >> 8, blk_addr, 0x00);
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
