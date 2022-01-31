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

#include "sdcard.h"

#include <stdio.h>
#include "pico/stdlib.h"
//#include "hardware/clocks.h"
//#include "hardware/spi.h"
//#include "hardware/sync.h"
#include "spi.h"

//
// pico specific definition
//
#define SPI_FILL_CHAR 0xff

static unsigned char
buffer[512];

static unsigned short
crc;

static unsigned long
cur_sec = -1;

static unsigned char
ccs;

static int
debug_flag = 1;
//
// SPI Command Debug Dump
//
static int
cmd_flag = 1;

#define SDCARD_MEASURE_TIME 1

#if defined (SDCARD_MEASURE_TIME)
static long
rd_time;

static long
wr_time;
#endif //defined(SDCARD_MEASURE_TIME)

void
sdcard_init
(void)
{
  do_spi_init();
}

static int
sd_response_in
(int counter)
{
  return sd_wait_resp(0xff, counter, 1);
}

static int
sd_response_byte
(int counter)
{
  return sd_wait_resp(0xff, counter, 8);
}

static unsigned long
sd_four_in
(void)
{
  unsigned long rc = 0;
  rc |= sd_in() << 24;
  rc |= sd_in() << 16;
  rc |= sd_in() << 8;
  rc |= sd_in();
  return rc;
}


// Most of the cards seems to need some pre-cs_select clocks.
// Two of my cards do not work in case pre-cs-select clocks are provided.
// So, preclock are provided af first. If initialization fails (in ACMD41),
// turn off this tweak and restart initialization.
static unsigned char
preclock_tweak = 1;

static unsigned char
traditional_initialize = 1;

static int
sd_cmd
(char cmd, char arg0, char arg1, char arg2, char arg3, char crc, unsigned long *resp)
{
  int c = 0x01;
  unsigned long rc;
  
  if (cmd_flag) printf("[%02X %02X %02X %02X %02X (%02X)]->", cmd, arg0, arg1, arg2, arg3, crc);
  if (preclock_tweak) {
    sd_out(0xff);  // sync with this 8 clks before CS down.
  }
  cs_select();
  sd_out(cmd);
  sd_out(arg0);
  sd_out(arg1);
  sd_out(arg2);
  sd_out(arg3);
  sd_out(crc);
  c = sd_response_in(40);   // 40 * 8 clocks
  if (c < 0) return -1;
  // Read remaining response bites.
  if (0x48 == cmd || 0x7a == cmd) {
    if (cmd_flag) printf("%02X:", c);
    // Even if R1 return value, we still need to read four bytes, no special reason
    // is known.
    rc = sd_four_in();
    if (cmd_flag) printf("%X\n", rc);
    if (resp) *resp = rc;
    return c;  // R1, R7 returns on *resp
  }
  if (cmd_flag) printf("%02X\n", c);
    //else printf(".");
  return c;  // R1
}

void
reset_80clks
(void)
{
  for (int i = 0; i < 10; ++i) {
    sd_out(0xff);
  }
  reset_clk();
}

static int
sd_initCMD
(char type, char arg)
{
  int rc;
  unsigned long dummy;
  if (1 == type) {
    rc = sd_cmd(0x41, 0x00, 0x00, 0x00, 0x00, 0x00, &dummy);
  } else if (2 == type) {
    // cmd55 - APP_CMD (response R1)
    sd_cmd(0x77, 0x00, 0x00, 0x00, 0x00, 0x01, &dummy);
    cs_deselect();
    // acmd41 - SD_SEND_OP_COND (response R1)
    rc = sd_cmd(0x69, arg, 0x00, 0x00, 0x00, 0x77/*0x77*/, &dummy);
  }
  cs_deselect();
  return rc;
}

int
sdcard_open
(void)
{
  unsigned long dummy;
  int rc;
  char type; // 1: SD, 2: SDV2, 3:
  char arg; 
  char initialized = 0;
  char flip_count = 0;
  char err = 0;

  // reset with 80 clocks
  reset_80clks();
  // cmd0 - GO_IDLE_STATE (response R1)
restart:
  if (flip_count > 2) {
    return -4;  // unexpected error
  }
  rc = sd_cmd(0x40, 0x00, 0x00, 0x00, 0x00, 0x95, &dummy);
  cs_deselect();
  if (rc < 0) {
    printf("time out, no card?\n");
    return -1;  // -1: no card.
  }
  if (0x1 != rc) {
    printf("unexpected result (%lX)\n", rc);
    return -3;  // -3: unexpected result
  }
  // cmd8 - SEND_IF_COND (response R7)
  rc = sd_cmd(0x48, 0x00, 0x00, 0x01, 0x0aa, 0x87, &dummy);
  cs_deselect();
  if (rc < 0) {
    return -4;  // -3: unexpected result
  }
  if (rc & 0x04) {
    // CMD8 error (illegal command)
    // it means SD Version 1 (legacy card)
    type = 1;
    arg = 0;
  } else if (0x1aa == (dummy & 0x00000fff)) {
    type = 2;
    arg = 0X40;
    // SD Version 2
  } else {
    // R4 return is not 0x1aa
    return -4; // unexpected error
  }
  int count = 0;
  while(1) {
    rc = sd_initCMD(type, arg);  // CMD1 or ACMD41
    if (count++ >= 10) {
      printf("%s time out\n", type == 1 ? "CMD1" : "ACMD41");
      return -2;  // -2: initialize time out
    }
    if (rc & 0x04) {
      // Illegal Command bit, ... maybe pre-cmd clock wrong.
      preclock_tweak = !preclock_tweak;
      flip_count++;
      printf("toggle tweak flag, restart\n");
      goto restart;
    }
    if (0 == rc) break;
    sleep_ms(100);
  }
  if (1 == type) {
    if (0 != rc)
      return -6;  // unexpected error
  } else if (2 == type) {
    // SD TYPE 2
    // cmd58 - READ_OCR (response R3)
    rc = sd_cmd(0x7a, 0x00, 0x00, 0x00, 0x00, 0xff, &dummy);//0xfd);
    cs_deselect();
    if (0 != rc) {
      return -5; // unexpected error
    }
    ccs = (dummy & 0x40000000) ? 1 : 0; // ccs bit high means SDHC card
    if ((dummy & 0xC0000000) == 0xC0000000) {
      type = 3; // SDHC
    } else {
      //rc = sd_cmd(0x50, 0x00, 0x00, 0x02, 0x00, 0x00, &dummy);
      //cs_deselect();
      // force 512 byte/block
    }
  }
done:
  // print SDcard type
  if (3 == type) {
    printf ("SDHC%s\n", (ccs ? " High Capacity" : ""));
  } else if (2 == type) {
    printf ("SD Ver.2+\n");
  } else if (1 == type) {
    printf ("SD Ver.1\n");
  } else {
    printf ("SD unknown (%d)\n", type);
  }
  return 0;
}

int
sdcard_fetch_sec
(unsigned long sec_addr)
{
  int c;
  unsigned long dummy;
  if (cur_sec == sec_addr) return 0;
  if (debug_flag) printf("sfs: %lX (%ld) + %x\n", sec_addr, sec_addr, 0);
  cur_sec = sec_addr;
  if (0 == ccs) sec_addr <<= 9; // SDHC cards use block addresses
  // cmd17
  unsigned long rc =
    sd_cmd(0x51, sec_addr >> 24, sec_addr >> 16, sec_addr >> 8, sec_addr, 0x00, &dummy);
//  if (0xc1 == rc) {
//    rc = sd_cmd(0x51, blk_addr >> 24, blk_addr >> 16, blk_addr >> 8, blk_addr, 0x00, &dummy);
//  }
  if (0 != rc) {
    cs_deselect();
    return -1;
  }
  // Data token 0xFE
  if ((c = sd_response_byte(10 * 63)) < 0 || c != 0xfe) {  // 1ms == 63countes
    printf("response: %02X\n", c);
    cs_deselect();
    return -2;
  }
  for (int i = 0; i < 512; i++) {
    buffer[i] = sd_in();
  }
#if defined(SDCARD_MEASURE_TIME)
  rd_time = measured_time;
#endif //defined(SDCARD_DEBUG)
  if (debug_flag & 2) {
    for (int i = 0; i < 16; ++i) {
      char *top;
      if ((i % 16) == 0) {
        printf("%03X ", i);
        top = &buffer[i];
      }
      printf("%02X ", buffer[i]);
      if ((i % 16) == 15) {
        for (int j = 0; j < 16; ++j) {
          int c = top[j];
          printf("%c", (' ' <= c && c <= 0x7f) ? c : '.');
        }
        printf("\n");
      }
    }
  }
  crc = sd_in() << 8;
  crc |= sd_in();
  // XXX: rc check
  //gpio_clr_mask(1<<P_CK);
  cs_deselect();
#if defined(SDCARD_MEASURE_TIME)
  if (debug_flag) printf("rd_time: %ld usec\n", rd_time * (1000 / ONE_MS_COUNT));
#endif //defined(SDCARD_DEBUG)
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
(unsigned long sec_addr)
{
  uint32_t dummy;
  int c;
  unsigned long rc;

  if (debug_flag) printf("ss: %lX (%ld)\n", sec_addr, sec_addr);
  if (0 == ccs) sec_addr <<= 9; // SDHC cards use block addresses
  // cmd24
  //uint32_t ints = save_and_disable_interrupts();
  rc = sd_cmd(0x58, sec_addr >> 24, sec_addr >> 16, sec_addr >> 8, sec_addr, 0x00, &dummy);
  if (0 != rc) {
    cs_deselect();
    return -1;
  }
  sd_out(0xff); // blank 1B
  sd_out(0xfe); // Data Token
  int i;
  for (i = 0; i < 512; i++) sd_out(buffer[i]); // Data Block
  sd_out(0xff); // CRC dummy 1/2
  sd_out(0xff); // CRC dummy 2/2
  // read data response
//  if ((c = sd_response_byte(10000)) < 0) {   // 10 * 100us, or 1ms
    if ((c = sd_in()) < 0) {

    //restore_interrupts(ints);
    cs_deselect();
    printf("DR: response timeout\n");
    return -3;
  }
  // skip busy "0" bits on DO
  // 1 clock == 2us, 8 clocks == 16us, 62.5 counts == 1ms
  if (sd_wait_resp(0, ONE_MS_COUNT * 250, 8) < 0) {   // 2500 * 100us, or 250ms
    //restore_interrupts(ints);
    cs_deselect();
    printf("DR: timeout\n");
    return -3;
  }
#if defined(SDCARD_MEASURE_TIME)
  wr_time = measured_time;
#endif //defined(SDCARD_DEBUG)
  int d = c & 0x1f;
  if (d != 0x05) {
    //restore_interrupts(ints);
    if (debug_flag) {
      for (int i = 0; i < 512; i++) {
        if ((i % 16) == 0) {
          printf("%03X ", i);
        }
        printf("%02X ", buffer[i]);
        if ((i % 16) == 15) {
          printf("\n");
        }
      }
    }
    cs_deselect();
    printf("DR: bad Data Response %02X\n", (c & 0x1f));
    return -3;
  }
  // successfully written
  //gpio_clr_mask(1<<P_CK);
  cs_deselect();
  //restore_interrupts(ints);
#if defined(SDCARD_MEASURE_TIME)
  if (debug_flag) printf("wr_time: %ld usec\n", wr_time);
#endif //defined(SDCARD_DEBUG)
  return 0;
}

int
sdcard_store
(unsigned long blk_addr)
{
  return sdcard_store_sec(blk_addr >> 9);
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
  return sdcard_store_sec(cur_sec);
}

void *
sdcard_buffer
(void)
{
  return buffer;
}
