/*
 * Copyright (c) 2022, Norihiro KUMAGAI <tendai22plus@gmail.com>
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
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "flash.h"
#include "xmodem.h"
#include "crc16.h"

//
// Use Pico embedded QSPI flash device as a disk drive
//
// Raspberry Pi has a QSPI flash device, which is mainly for storing
// Pico program.  Its capacity is 2Mbyte and it usually has amount of 
// unused space.  So, we can use one or several disk images in it.
//
// The size of a CP/Mega88 disk image is 256256bytes, or 1001 * 256 bytes.
// So we allocate the tail 1024 * 256 bytes of the QSPI flash device to
// the disk image.
// We has already allocated eeprom area to the last 4kB, which does not
// interfare the disk image, because of the eeprom area, 16 * 256 bytes
// fits within the last 23 * 256 unused area.
//
// Memory map is as follow;
//
// flash address
// (for erase/prog)
//        0x1C0000       +---------------------------+
//                       |     disk image            |
//                       |     (256256 bytes)        |
//                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                       |                           |
//                       |                           |
//        0x1FE900       +---------------------------+
//                       |     (unused area)         |
//                       |                           |
//        0x1FF000       +---------------------------+
//                       |     eeprom area (4kB)     |
//        0x200000       +---------------------------+
//
// for read address, add XIP_BASE to those above flash addresses
//

// QSPI access is based on the following concept.
// Read: read as if it is ordinal memory cell, so we usually use memcpy(2).
// Program: write operation, but a program operation can only turn bit 1 to bit 0.
//   Before program operation, all the bits in the target area should be 1.
//   By performing 'erase' operation before doing program, we can get such the bits.
// Erase: before actual Write operation, we need to erase a region which
//   includes the target write area.
//
// Unit lengths of program and erase are different.  Program size is a multiple of PAGE size,
// usually 256bytes.  Erase size is larger, usually a multiple of SECTOR size, or 4096bytes.
// To avoid breaking adjacent area by erase and program operations, we need to prefetch content of
// the corresponding sectors, and overwrite the target area, and write them back to QSPI flash.
// THis 'read-modify-erase-program' operation is a essential to implement CP/M 128byte unit
// write operations

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

#define DISK_SIZE (1001 * 256)
#define DISK_AREA_SIZE (1024 * 256)
#define DRIVEA_BASE (PICO_FLASH_SIZE_BYTES - DISK_AREA_SIZE)

// PICO_CONFIG: PICO_STDIO_USB_LOW_PRIORITY_IRQ, low priority (non hardware) IRQ number to claim for tud_task() background execution, default=31, advanced=true, group=pico_stdio_usb
#ifndef PICO_STDIO_USB_LOW_PRIORITY_IRQ
#define PICO_STDIO_USB_LOW_PRIORITY_IRQ 31
#endif
// address for erase/program operations, base is zero
// for read operation(memcpy), base is XIP_BASE.
#define DRIVEA_BASE_ADDR (XIP_BASE + DRIVEA_BASE)

static unsigned char
buffer[FLASH_SECTOR_SIZE];

static unsigned long
cur_blk;

static int
dirty_flag = 0;

static long
cur_sector = -1;  // sector number, specifying the content of 'buffer[]'

static int
cur_offset = -1;  // byte offset within 'buffer[]'

static int
cur_drive = -1;   // drive number

static int
debug_out = 0;

void
sdcard_init
(void)
{
  // No special initialization is needed for flash access
  cur_blk = 0;
  cur_drive = -1;
  cur_sector = -1;
  dirty_flag = 0;
}

int
sdcard_open
(void)
{
  return 0;
}

void
sdcard_buffer_dirty
(void)
{
  if (!dirty_flag) {
    if (debug_out) printf("d:\n");
  }
  dirty_flag = 1;
}

static void
flash_erase_and_program_sector
(uint32_t addr, unsigned char *src)
{
  uint32_t ints = 0;
  // disable all of the irqs and perform erase and program
//  for (uint32_t i = 0; i < 32; ++i) {
//    if (irq_is_enabled(i))
//      mask |= (1u << i);
//  }
//  irq_set_mask_enabled(mask, false);  // all irq disabled
//  flash_range_erase(addr, FLASH_SECTOR_SIZE);
//  flash_range_program(DRIVEA_BASE + (cur_sector << 12), buffer, FLASH_SECTOR_SIZE);
  ints = save_and_disable_interrupts();
  flash_range_erase(addr, FLASH_SECTOR_SIZE);
  flash_range_program(addr, src, FLASH_SECTOR_SIZE);
//  irq_set_mask_enabled(mask, true);
  restore_interrupts(ints);
}

static void
flush_buffer_if_necessary
(void)
{
  if (dirty_flag) {
    // flush current content to QSPI flash memory
    if (cur_sector == -1) {
      // invalid data
      // do nothing
      if (debug_out) printf("f: invalid\n");
    } else {
      // write it out
      if (debug_out) printf("f: %08X\n", cur_sector);
      flash_erase_and_program_sector(DRIVEA_BASE + (cur_sector << 12), buffer);
//      irq_set_enabled(PICO_STDIO_USB_LOW_PRIORITY_IRQ, false);
//      flash_range_erase(DRIVEA_BASE + (cur_sector << 12), FLASH_SECTOR_SIZE);
//      flash_range_program(DRIVEA_BASE + (cur_sector << 12), buffer, FLASH_SECTOR_SIZE);
//      irq_set_enabled(PICO_STDIO_USB_LOW_PRIORITY_IRQ, true);
      dirty_flag = 0;
    }
  }
}

static void
read_from_flash
(unsigned long sector)
{
  if (debug_out) printf("r: %08X\n", sector);
  unsigned char *src = (unsigned char *)(XIP_BASE) + DRIVEA_BASE + sector * FLASH_SECTOR_SIZE;
  memcpy(buffer, src, FLASH_SECTOR_SIZE);
  cur_sector = sector;
}

#if defined(USE_FLASH)
void
select_drive
(int drive)
{
  // always flush buffer
  if (debug_out) printf("sd: %d\n", drive);
  flush_buffer_if_necessary();
  if (cur_drive != drive) {
    // if another drive is selected, buffer[] becomes invalid
    cur_drive = drive;
    cur_sector = -1;
  }
}
#endif //defined(USE_FLASH)

#if defined(USE_FLASH)
void
flash_disk_write
(unsigned long pos, const unsigned char *src, int len)
{
  int n;
//  static uint32_t mask;

  while (len > 0) {
    unsigned long new_sector = pos / FLASH_SECTOR_SIZE;
    int new_offset = pos % FLASH_SECTOR_SIZE;
    if (dirty_flag && cur_sector >= 0 && cur_sector != new_sector) {
      flash_erase_and_program_sector(DRIVEA_BASE + (cur_sector << 12), buffer);
      dirty_flag = 0;
    }
    if (cur_sector != new_sector) {
      // read a sector to fill buffer[]
      memcpy(buffer, (unsigned char *)XIP_BASE + DRIVEA_BASE + (new_sector << 12), FLASH_SECTOR_SIZE);
      cur_sector = new_sector;
    }
    // overwtite buffer[] with src and len
    n = (FLASH_SECTOR_SIZE - new_offset) < len ? FLASH_SECTOR_SIZE - new_offset : len;
    memcpy(&buffer[new_offset], src, n);
    dirty_flag = 1;
    src += n;
    len -= n;
    pos += n;
  }
}

void
end_flash_write
(void)
{
  if (dirty_flag && cur_sector >= 0) {
      irq_set_enabled(PICO_STDIO_USB_LOW_PRIORITY_IRQ, false);
//      flash_range_erase(DRIVEA_BASE + (cur_sector << 12), FLASH_SECTOR_SIZE);
//      flash_range_program(DRIVEA_BASE + (cur_sector << 12), buffer, FLASH_SECTOR_SIZE);
      irq_set_enabled(PICO_STDIO_USB_LOW_PRIORITY_IRQ, true);
      dirty_flag = 0;
  }
}

char
flash_read
(unsigned long pos)
{
  return *((unsigned char *)XIP_BASE + DRIVEA_BASE + pos);
}

long
xmodem_receive_flash
(unsigned short *crc)
{
  long len = xmodemReceive(0, DISK_SIZE);
  *crc = crc16_ccitt((unsigned char *)XIP_BASE + DRIVEA_BASE, DISK_SIZE);
  return len;
}

long
xmodem_send_flash
(unsigned short *crc)
{
  long len = xmodemTransmit((unsigned char *)XIP_BASE + DRIVEA_BASE, DISK_SIZE);
  *crc = crc16_ccitt((unsigned char *)XIP_BASE + DRIVEA_BASE, DISK_SIZE);
  return len;
}
#endif //defined(USE_FLASH)

int
sdcard_fetch
(unsigned long blk_addr)
{
//  int c;
//  uint32_t dummy, crc;

  cur_blk = blk_addr;

  // read the specified sector
  unsigned long new_sector = blk_addr / FLASH_SECTOR_SIZE;
  int new_offset = blk_addr % FLASH_SECTOR_SIZE;
  if (debug_out) printf("F: %08X %05X:%03X\n", blk_addr, new_sector, new_offset);

  flush_buffer_if_necessary();

  // now no-dirty, read it
  if (cur_sector != new_sector) {
    read_from_flash(new_sector);
    cur_offset = new_offset;
  } else if (cur_offset != new_offset) {
    cur_offset = new_offset;
  }
  // copy it to the destination
  return 0;
}

int
sdcard_store
(unsigned long blk_addr)
{
//  uint32_t dummy;
//  int c;
//  unsigned long rc;
  unsigned long new_sector = blk_addr / FLASH_SECTOR_SIZE;
  int new_offset = blk_addr % FLASH_SECTOR_SIZE;

  if (debug_out) printf("S: %08X %05X:%03X\n", blk_addr, new_sector, new_offset);
  // read-modify-erase-program
  if (cur_sector != new_sector) {
    flush_buffer_if_necessary();
  }
  return 0;
}

unsigned short
sdcard_crc
(void)
{
  return 0xffff;  // dummy value
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
//  if (debug_out) printf("b: %03X\n", cur_offset);
  return &buffer[cur_offset];
}
