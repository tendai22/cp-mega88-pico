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

//#include "sdcard.h"
#include "spi.h"

#include <stdio.h>
#include "pico/stdlib.h"

//
// pico specific definition
//
#define SPI_FILL_CHAR 0xff

#define USE_SOFTSPI
// SPI Pin Assign

// RX  GP00
// CS  GP01
// SCK GP02
// TX  GP03
#define P_DO 0
#define P_CS 1
#define P_CK 2
#define P_DI 3

#if defined(USE_SOFTSPI)
#include "hardware/clocks.h"
#include "hardware/sync.h"
//
//  software spi, using gpio
//

#define M_DO (1<<P_DO)
#define M_CS (1<<P_CS)
#define M_CK (1<<P_CK)
#define M_DI (1<<P_DI)

long int
measured_time;


static void spi_xfer(unsigned char out_reg, unsigned char *receivep);

void
do_spi_init
(void)
{
  /*
   * CK/DI/CS: out, low
   * DO: in
   */
  //printf("do_spi_init: start\n");
  //sleep_ms(100);
  // Port Settings
  gpio_init(P_DO);
  gpio_set_dir(P_DO, GPIO_IN);

  gpio_init(P_DI);
  gpio_set_dir(P_DI, GPIO_OUT);
  gpio_put(P_DI, 1);

  gpio_init(P_CK);
  gpio_set_dir(P_CK, GPIO_OUT);
  gpio_put(P_CK, 0);

  //unsigned char in, out = 0xff;
  //for (int i = 0; i < 20; ++i)
  //  spi_xfer(out, &in);

  // CSn
  gpio_init(P_CS); // CSn
  gpio_put(P_CS, 1);
  gpio_set_dir(P_CS, GPIO_OUT);
}

void
reset_clk
(void)
{
  cs_deselect();
  gpio_put(P_CK, 0);
}

static inline void
wait_clk
(void)
{
// so far, asm nops do not work well. isb seems to help stability
// of clock pulse width.
//  asm volatile("isb\n");
//  asm volatile("nop \n nop \n nop");
//  asm volatile("nop \n nop \n nop");
//  asm volatile("nop \n nop \n nop");
//  asm volatile("nop \n nop \n nop\n nop\n nop \n");
//  asm volatile("isb\n");
  sleep_us(1);    // 1 clock == 2 wait_clks, 2us or 500kHz
}

static void
spi_xfer
(unsigned char out_reg, unsigned char *receivep)
{
  unsigned char in_reg = 0;
  for (int i = 0; i < 8; ++i) {
    gpio_put(P_CK, 0);
    gpio_put(P_DI, (out_reg & 0x80) ? 1 : 0);
    wait_clk();
    if (gpio_get(P_DO)) in_reg |= 1;
    gpio_put(P_CK, 1);
    wait_clk();
    if (i == 7) break;
    out_reg <<= 1;
    in_reg <<= 1;
  }
  gpio_put(P_CK, 0);
  //gpio_put(P_DI, 1);
  if (receivep)
    *receivep = in_reg;
}

int
sd_wait_resp
(unsigned char value, long int counter, int bitnum)
{
  unsigned char c;
  long int ncounter = counter;
  long int nstart = ncounter;
  if (bitnum == 8) {
    while (ncounter > 0 && (c = sd_in()) == value) {
      ncounter--;
    }
    if (ncounter <= 0) {
      return -1;
    }
#if defined(SDCARD_DEBUG)
    measured_time = nstart - ncounter;
#endif
    return c;
  }
  ncounter *= 8;  // the following loop period is counter * 8clocks.
  gpio_put(P_DI, 1);  // out 1 on DI
  while (ncounter > 0) {
    gpio_put(P_CK, 0);
    wait_clk();
    if (!gpio_get(P_DO) != !value) break;
    gpio_put(P_CK, 1);
    wait_clk();
    ncounter--;
  }
  if (ncounter <= 0) {
    return -1;
  }
#if defined(SDCARD_DEBUG)
  measured_time = nstart - ncounter;
#endif
  spi_xfer(0xff, &c);
  return c;

}

unsigned long
sd_in
(void)
{
  uint8_t value = SPI_FILL_CHAR;
  uint8_t received;
  spi_xfer(value, &received);
  return received;
}

void
sd_out
(const unsigned char c)
{
  uint8_t dummy;
  spi_xfer(c, &dummy);
}

void cs_select()
{
//  asm volatile("nop \n nop \n nop");
//  sd_out(0xff);
//  asm volatile("nop \n nop \n nop");
  gpio_put(P_CS, 0);  // Active low
}

void cs_deselect()
{
//  asm volatile("nop \n nop \n nop");
  gpio_put(P_CS, 1);  // Active low
  //sd_out(0xff);
//  asm volatile("nop \n nop \n nop");
}

#else //defined(USE_SOFTSPI)
#include "hardware/spi.h"
//
// use hardware_spi in pico-sdk
//
#define SPI_ID spi0

void
do_spi_init
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
}

void
reset_clk
(void)
{
  cs_deselect();
  gpio_put(P_CK, 0);
}

int
sd_wait_resp
(unsigned char value, long int counter, int bitnum)
{
  unsigned char c;
  long int ncounter = counter;
  long int nstart = ncounter;
  while (ncounter > 0 && (c = sd_in()) == value) {
    ncounter--;
  }
  if (ncounter <= 0) {
    return -1;
  }
#if defined(SDCARD_DEBUG)
    measured_time = nstart - ncounter;
#endif
  return c;
}

unsigned long
sd_in
(void)
{
  uint8_t value = SPI_FILL_CHAR;
  uint8_t received;
  spi_write_read_blocking(SPI_ID, &value, &received, 1);
  return received;
}

void
sd_out
(const unsigned char c)
{
  uint8_t dummy;
  spi_write_read_blocking(SPI_ID, &c, &dummy, 1);
}

void cs_select()
{
  asm volatile("nop \n nop \n nop");
//  sd_out(0xff);
  asm volatile("nop \n nop \n nop");
  gpio_put(P_CS, 0);  // Active low
}

void cs_deselect()
{
  asm volatile("nop \n nop \n nop");
  gpio_put(P_CS, 1);  // Active low
  sd_out(0xff);
  asm volatile("nop \n nop \n nop");
}

#endif //define(USE_SOFT_SPI)