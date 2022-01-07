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

#include <string.h>
#include <inttypes.h>
#include "eeprom.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// We use Pico QSPI Flash as eeprom storage, the last sector(4kB) of its size(2MB)
// The start address of QSPI flash device is XPI_BASE. So the start address of
// eeprom is XIP_BASE + 0x200000 - 4096
#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE 4096
#endif
#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 256
#endif
#define EEPROM_BASE ((uint32_t)XIP_BASE + 0x200000 - FLASH_SECTOR_SIZE)

int
eeprom_load
(void *image)
{
  memcpy(image, (unsigned char *)EEPROM_BASE, FLASH_SECTOR_SIZE);
  return FLASH_SECTOR_SIZE;
}

void
eeprom_flush
(void *image)
{
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase (EEPROM_BASE, FLASH_SECTOR_SIZE);
  flash_range_program(EEPROM_BASE, image, FLASH_SECTOR_SIZE);
  restore_interrupts (ints);
}

