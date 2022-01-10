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

#include "con.h"

#include <stdio.h>
#include "pico/stdlib.h"

#if defined(LIB_PICO_STDIO_UART)
//
// USE_UART macros
//
#define UART_ID uart_default
#define BAUD_RATE 115200
// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 16
#define UART_RX_PIN 17
#endif // defined(LIB_PICO_STDIO_UART)

static void
sleep
(void)
{
  sleep_us(100);   // 100us wait
}

void
con_init
(void)
{
#if defined(LIB_PICO_STDIO_UART)
  // stdio_uart mode
  stdio_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
#elif defined(LIB_PICO_STDIO_USB)
  // for stdio_usb mode
  stdio_init_all();
  sleep_ms(1000);
#endif //defined(USE_UART)
  while(getchar_timeout_us(100) != PICO_ERROR_TIMEOUT)
    ;
}

void
con_putchar
(unsigned char c)
{
  putchar_raw(c);
}

// for peeking where no peek functions we have.
static int unget_char = -1;

int
con_getchar
(void)
{
  int c;
  if ((c = unget_char) != -1) {
    unget_char = -1;
    return c;
  }
  return ((c = getchar_timeout_us(10)) != PICO_ERROR_TIMEOUT) ? c : -1;
}

int
con_peek
(void)
{
  int c;
  if (unget_char != -1)
    return 1;
  if ((c = getchar_timeout_us(10)) != PICO_ERROR_TIMEOUT) {
    unget_char = c;
    return 1;
  }
  return 0;
}
