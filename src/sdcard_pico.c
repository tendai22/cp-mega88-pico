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
#include "hardware/clocks.h"
#include "hardware/spi.h"

//
// pico specific definition
//
#define SPI_FILL_CHAR 0xff
#define SPI_ID spi0

//
// SPI Pin Assign
//

// RX  GP00
// CS  GP01
// SCK GP02
// TX  GP03
#define P_DO 0
#define P_CS 1
#define P_CK 2
#define P_DI 3

static unsigned char
buffer[512];

static unsigned short
crc;

static unsigned long
cur_blk;

static unsigned char
ccs;

static void cs_select()
{
  asm volatile("nop \n nop \n nop");
  gpio_put(P_CS, 0);  // Active low
  asm volatile("nop \n nop \n nop");
}

static void cs_deselect()
{
  asm volatile("nop \n nop \n nop");
  gpio_put(P_CS, 1);  // Active low
  asm volatile("nop \n nop \n nop");
}

static uint8_t sdcard_spi_write(spi_inst_t *spi, const uint8_t value) {
    u_int8_t received = SPI_FILL_CHAR;
    spi_write_read_blocking(spi, &value, &received, 1);
    return received;
}

static unsigned long
sd_in
(void)
{
  uint8_t value = SPI_FILL_CHAR;
  uint8_t received;
  spi_write_read_blocking(SPI_ID, &value, &received, 1);
  return received;
}

static void
sd_out
(const unsigned char c)
{
  uint8_t dummy;
  spi_write_read_blocking(SPI_ID, &c, &dummy, 1);
}

static int
sd_wait_resp
(unsigned char value, int counter)
{
  unsigned char c;
  while (counter > 0 && (c = sd_in()) == value) {
    sleep_us(1);
    counter--;
  }
  if (counter <= 0) {
    printf("time out\n");
    return -1;
  }
  return c;

}

static int
sd_response_in
(int counter)
{
  return sd_wait_resp(0xff, counter);
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


//
// SPI Command Debug Dump
//
#ifdef SDCARD_CMD_DUMP
static int cmd_flag = 1;
#else
static int cmd_flag = 0;
#endif

static int
sd_cmd
(char cmd, char arg0, char arg1, char arg2, char arg3, char crc, unsigned long *resp)
{
  int c;
  unsigned long rc;
  if (cmd_flag) printf("[%02X %02X %02X %02X %02X (%02X)]->", cmd, arg0, arg1, arg2, arg3, crc);
  cs_select();
  sd_out(cmd);
  sd_out(arg0);
  sd_out(arg1);
  sd_out(arg2);
  sd_out(arg3);
  sd_out(crc);
  if ((c = sd_response_in(20)) < 0) return -1;
  // Read remaining response bites.
  if (0x48 == cmd) {
    if (cmd_flag) printf("%02X:", c);
    // Even if R1 return value, we still need to read four bytes, no special reason
    // is known.
    rc = sd_four_in();
    cs_deselect();
    if (cmd_flag) printf("%X\n", rc);
    if (resp) *resp = rc;
    return (c & 0x04) ? -2: c;  // R7
  }
  if (0x7a == cmd) {
    if (cmd_flag) printf("%02X:", c);
    rc = sd_four_in();
    cs_deselect();
    if (cmd_flag) printf("%X\n", rc);
    if (resp) *resp = rc;
    return c;  // R3
  }
  sd_four_in();
  if (cmd_flag) printf("%02X\n", c);
    //else printf(".");
  return c;  // R1
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
  spi_init(spi0, 2000 * 1000);  // 2Mbit/sec
  gpio_set_function(P_DO, GPIO_FUNC_SPI); // RX
  gpio_set_function(P_CK, GPIO_FUNC_SPI); // SCK
  gpio_set_function(P_DI, GPIO_FUNC_SPI); // TX
  // CSn
  gpio_init(P_CS); // CSn
  gpio_set_dir(P_CS, GPIO_OUT);
  gpio_put(P_CS, 1);
  cur_blk = 0;
}

int
sdcard_open
(void)
{
  uint32_t dummy;

  gpio_put(P_DI, 1);
  // reset with 80 clocks
  for (int i = 0; i < 10; ++i) {
    sd_out(0xff);
  }
  unsigned long rc;
  // cmd0 - GO_IDLE_STATE (response R1)
  rc = sd_cmd(0x40, 0x00, 0x00, 0x00, 0x00, 0x95, &dummy);
  if (rc < 0 ||1 != rc) {
    cs_deselect();
    return -1;
  }
  // cmd8 - SEND_IF_COND (response R7)
  rc = sd_cmd(0x48, 0x00, 0x00, 0x01, 0x0aa, 0x87, &dummy);
  if (rc < 0) {
    cs_deselect();
    return -1;
  }
  int type;
  if ((rc & 0x04) == 0x04) {  // CMD8 error (illegal command)
    // it means SD Version 1 (legacy card)
    type = 1;   // SD1
    printf("SD Ver.1\n");
    // reset to IDLE State again
    rc = sd_cmd(0x40, 0x00, 0x00, 0x00, 0x00, 0x95, &dummy);
    if (rc < 0 || 1 != rc) {
      cs_deselect();
      printf("re IDLE STATE fail\n");
      return -1;
    }
    // go to ACMD41 initialize
  } else if ((dummy & 0x00000fff) != 0x1aa) {
    cs_deselect();
    printf("ERR\n");
    goto error;
  } else {
    cs_deselect();
    printf("SD Ver.2\n");
    // 0x1aa
    type = 2; // SD2
  }
  int arg = (type == 2) ? 0X40 : 0;
  int count = 0;
  do {
    // cmd55 - APP_CMD (response R1)
    sd_cmd(0x77, 0x00, 0x00, 0x00, 0x00, 0x01, &dummy);
    // acmd41 - SD_SEND_OP_COND (response R1)
    rc = sd_cmd(0x69, arg, 0x00, 0x00, 0x00, 0x77/*0x77*/, &dummy);
    sleep_ms(100);
    if (count++ >= 50) goto error;
  } while (rc != 0);
  ccs = (rc & 0x40000000) ? 1 : 0; // ccs bit high means SDHC card

  if (type == 2) {
    // SD TYPE 2
    // cmd58 - READ_OCR (response R3)
    rc = sd_cmd(0x7a, 0x00, 0x00, 0x00, 0x00, 0xff, &dummy);//0xfd);
    ccs = (dummy & 0x40000000) ? 1 : 0; // ccs bit high means SDHC card
    if ((dummy & 0xC0000000) == 0xC0000000) {
      type = 3; // SDHC
      printf ("SDHC%s\n", (ccs ? " High Capacity" : ""));
    } else {
      printf("SD Ver.2+\n");
      rc = sd_cmd(0x50, 0x00, 0x00, 0x02, 0x00, 0x00, &dummy);
      // force 512 byte/block
    }
  }
  gpio_clr_mask(1<<P_CK);
  cs_deselect();
  return 0;
error:
  gpio_set_mask(1<<P_CS);
  cs_deselect();
  return -1;
}

int
sdcard_fetch
(unsigned long blk_addr)
{
  int c;
  uint32_t dummy, crc;

  cur_blk = blk_addr;
  if (0 != ccs) blk_addr >>= 9; // SDHC cards use block addresses
  // cmd17
  unsigned long rc =
    sd_cmd(0x51, blk_addr >> 24, blk_addr >> 16, blk_addr >> 8, blk_addr, 0x00, &dummy);
  if (0 != rc) return -1;
  // Data token 0xFE
  if ((c = sd_response_in(60)) < 0 || c != 0xfe) return -2;
  for (int i = 0; i < 512; i++) {
    buffer[i] = sd_in();
#if SDCARD_FETCH_DUMP
    if ((i % 16) == 0)
      printf("%03X ", i);
    printf("%02X ", buffer[i]);
    if ((i % 16) == 15)
      printf("\n");
#endif //SDCARD_FETCH_DUMP
  }
  crc = sd_in() << 8;
  crc |= sd_in();
  // XXX: rc check

  gpio_clr_mask(1<<P_CK);
  return 0;
}

int
sdcard_store
(unsigned long blk_addr)
{
  uint32_t dummy;
  int c;
  unsigned long rc;

  if (0 != ccs) blk_addr >>= 9; // SDHC cards use block addresses
  // cmd24
  rc = sd_cmd(0x58, blk_addr >> 24, blk_addr >> 16, blk_addr >> 8, blk_addr, 0x00, &dummy);
  if (0 != rc) return -1;
  sd_out(0xff); // blank 1B
  sd_out(0xfe); // Data Token
  int i;
  for (i = 0; i < 512; i++) sd_out(buffer[i]); // Data Block
  sd_out(0xff); // CRC dummy 1/2
  sd_out(0xff); // CRC dummy 2/2
  // read data response
  if ((c = sd_response_in(10)) < 0) return -3;
  // skip busy "0" bits on DO
  if (sd_wait_resp(0, 1000) < 0) {
    printf("DR: timeout\n");
    return -3;
  }
  int d = c & 0xe;
  if (d != 0x01) {
    printf("DR: bad Data Response %02X\n", (c & 0x1f));
    return -3;
  } else if ((d = (c & 0x11)) != 0x05) {
    printf("DR: data rejected %02X\n", (c & 0x1f));
  }
  // successfully written
  gpio_clr_mask(1<<P_CK);
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
