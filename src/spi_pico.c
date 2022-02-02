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
typedef unsigned long reg_t;

// SPI Pin Assign

// RX  GP00
// CS  GP01
// SCK GP02
// TX  GP03
#define P_DO 0
#define P_CS 1
#define P_CK 2
#define P_DI 3

#define USE_SOFTSPI

#if defined(USE_SOFTSPI)
#include "hardware/clocks.h"
#include "hardware/sync.h"

#else //defined(USE_SOFTSPI)
#include "hardware/spi.h"

#define SPI_ID spi0
#endif //defined(USE_SOFTSPI)

//
//  software spi, using gpio
//

#define M_DO (1<<P_DO)
#define M_CS (1<<P_CS)
#define M_CK (1<<P_CK)
#define M_DI (1<<P_DI)

long int
measured_time;

static unsigned char
spi_xfer
(unsigned char out_reg);

static inline void
PIN_LOW
(int pinno)
{
  gpio_put(pinno, 0);
}

static inline void
PIN_HIGH
(int pinno)
{
  gpio_put(pinno, 1);
}

static inline reg_t
PIN_DO
(void)
{
  return gpio_get_all() & (1<<P_DO);
}

void
do_spi_init
(void)
{
  /*
   * CK/DI/CS: out, low
   * DO: in
   */
  // gpio spi
  // Port Settings
#if defined(USE_SOFTSPI)
  gpio_init(P_DO);
  gpio_set_dir(P_DO, GPIO_IN);

  gpio_init(P_CS); // CSn
  gpio_init(P_CK);
  gpio_init(P_DI);
  
  PIN_HIGH(P_CS);
  PIN_LOW(P_CK);
  PIN_HIGH(P_DI);

  gpio_set_dir(P_DI, GPIO_OUT);
  gpio_set_dir(P_CK, GPIO_OUT);
  gpio_set_dir(P_CS, GPIO_OUT);
#else //defined(USE_SOFTSPI)
  // use hardware_spi
  // Port Settings
  spi_init(SPI_ID, (unsigned long)10000 * 1000);  // 2Mbit/sec
  gpio_set_function(P_DO, GPIO_FUNC_SPI); // RX
  gpio_set_function(P_CK, GPIO_FUNC_SPI); // SCK
  gpio_set_function(P_DI, GPIO_FUNC_SPI); // TX
  // CSn
  gpio_init(P_CS); // CSn
  gpio_set_dir(P_CS, GPIO_OUT);
  gpio_put(P_CS, 1);
#endif //defined(USE_SOFTSPI)
}

static inline void
wait_clk
(void)
{
// so far, asm nops do not work well. isb seems to help stability
// of clock pulse width.
//  asm volatile("nop \n nop \n nop");
//  asm volatile("nop \n nop \n nop");
//  88nop ... 800kHz (No18 okay, No17 no good,)
//  asm volatile("nop \n nop \n nop\n nop\n nop \n nop \n nop \n nop\n nop\n nop \n");
//  asm volatile("nop \n nop \n nop\n nop\n nop \n nop \n nop \n nop\n nop\n nop \n");
#if 1
  // three pairs, 3.3MHz ... card #18 do not work.
  //asm volatile("nop\n");
  //asm volatile("isb\n");
  // two pairs, 3.3MHz ... card #10 works, maybe all of others will be okay.
  asm volatile("nop\n");
  asm volatile("isb\n");
  // one pair  5.0MHz ... card #10 do not work.
  asm volatile("nop\n");
  asm volatile("isb\n");
#else
  sleep_us(1);    // 1 clock == 2 wait_clks, 2us or 500kHz
#endif
}

static unsigned char
spi_xfer
(unsigned char out_reg)
{
#if defined(USE_SOFTSPI)
  unsigned char in_reg = 0;
  for (int i = 0; i < 8; ++i) {
    PIN_LOW(P_CK);
    if (out_reg & 0x80) PIN_HIGH(P_DI);
    else PIN_LOW(P_DI);
    wait_clk();
    if (PIN_DO()) in_reg |= 1;
    PIN_HIGH(P_CK);
    wait_clk();
    if (i == 7) break;
    out_reg <<= 1;
    in_reg <<= 1;
  }
  PIN_LOW(P_CK);
  //gpio_put(P_DI, 1);
  return in_reg;
#else
  unsigned char in_reg;
  spi_write_read_blocking(SPI_ID, &out_reg, &in_reg, 1);
  return in_reg;
#endif //defined(USE_SOFTSPI)
}

//
// The following codes are machine/cpu independent codes
//

void
reset_clk
(void)
{
  cs_deselect();
  PIN_LOW(P_CK);
}

int
sd_wait_resp
(unsigned char value, long int counter, int bitnum)
{
  unsigned char c;
  reg_t mask = value ? (((reg_t)(-1)) & M_DO) : 0;
  reg_t x;
  long int ncounter = counter;
  long int nstart = ncounter;
#if defined(USE_SOFTSPI)
  if (bitnum != 8) {
    ncounter *= 8;  // the following loop period is counter * 8clocks.
    PIN_HIGH(P_DI);  // out 1 on DI
    while (ncounter > 0) {
      PIN_LOW(P_CK);
      wait_clk();
      if (PIN_DO() != mask) break;
      PIN_HIGH(P_CK);
      wait_clk();
      ncounter--;
    }
    if (ncounter <= 0) {
      return -1;
    }
#if defined(SDCARD_DEBUG)
    measured_time = nstart - ncounter;
#endif
    return spi_xfer(SPI_FILL_CHAR);
  }
#endif //defined(USE_SOFTSPI)
  while (ncounter > 0 && (c = sd_in()) == value)
    ncounter--;
  if (ncounter <= 0) return -1;
#if defined(SDCARD_DEBUG)
  measured_time = nstart - ncounter;
#endif
  return c;
}

unsigned long
sd_in
(void)
{
  return spi_xfer(SPI_FILL_CHAR);
}

void
sd_out
(const unsigned char c)
{
  spi_xfer(c);
}

void cs_select()
{
  PIN_LOW(P_CS);  // Active low
}

void cs_deselect()
{
  PIN_HIGH(P_CS);  // Active low
}

//
// use hardware_spi in pico-sdk
//
#define SPI_ID spi0

#if 0
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