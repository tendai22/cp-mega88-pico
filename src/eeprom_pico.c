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
#include <stdio.h>
#include "eeprom.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// We use Pico QSPI Flash as eeprom storage, the last sector(4kB) of its size(2MB)
// The start address of QSPI flash device is XPI_BASE. So the start address of
// eeprom is XIP_BASE + 0x200000 - 4096
#ifndef FLASH_SECTOR_SIZE
#warning FLASH_SECTOR_SIZE not set, assume 4096
#define FLASH_SECTOR_SIZE 4096
#endif
#ifndef FLASH_PAGE_SIZE
#warning FLASH_PAGE_SIZE not set, assume 256
#define FLASH_PAGE_SIZE 256
#endif

#ifndef PICO_FLASH_SIZE_BYTES
#warning PICO_FLASH_SIZE_BYTES not set, assuming 2M
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024);
#endif

// address for erase/program, base is zero
// for read operation(memcpy), base is XIP_BASE.
#define EEPROM_BASE (0 + PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define EEPROM_BASE_ADDR (XIP_BASE + EEPROM_BASE)

//
// The size of eeprom_load is EEPROM_SIZE, which is (usually) larger than that of 
// QSPI Flash SECTOR SIZE, so we need an internal buffer which should accomodate a
// sector data (4096bytes)
//

static uint8_t flash_sector_buffer[FLASH_SECTOR_SIZE];

int
eeprom_load
(void *image)
{
  // the top EEPROM_SIZE (512bytes) of the flash sector is used as eeprom
  // so we only copy the size of EEPROM_SIZE when we read it.
  memcpy(image, (unsigned char *)EEPROM_BASE_ADDR, EEPROM_SIZE);
  return EEPROM_SIZE;
}

void
eeprom_flush
(void *image)
{
  uint32_t ints = save_and_disable_interrupts();
  memcpy(flash_sector_buffer, (unsigned char *)EEPROM_BASE_ADDR, FLASH_SECTOR_SIZE);
  memcpy(flash_sector_buffer, image, EEPROM_SIZE);
  flash_range_erase (EEPROM_BASE, FLASH_SECTOR_SIZE);
  flash_range_program(EEPROM_BASE, flash_sector_buffer, FLASH_SECTOR_SIZE);
  restore_interrupts (ints);
}

