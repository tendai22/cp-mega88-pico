/*
 * Copyright (c) 2010, Takashi TOYOSHIMA <toyoshim@gmail.com>
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

#include "fat.h"
#include "sdcard.h"

#include <stdio.h>

// offsets in MBR sector
#define OFF_FS_DESC 450
#define OFF_P1_1SCT 454

// offsets in BPB sector
#define OFF_FS_TYPE 54
#define OFF_B_P_SCT 11  // BPB_BytePerSec
#define OFF_SCT_P_C 13  // BPB_SectorPerClus
#define OFF_SCT_RSV 14  //
#define OFF_NUM_O_F 16
#define OFF_ROOTNUM 17
#define OFF_TOTSEC16 19
#define OFF_SCT_P_F 22
#define OFF_TOTSEC32 32

// FS TYPE
#define FS_FAT12 1  // FAT12(CHS/LBA <65536sectors)
#define FS_FAT16 4  // FAT16(CHS/LBA <65536sectors)
#define FS_FAT32 12 // FAT32(LBA)


static unsigned long sectors_per_cluster;
static unsigned long fat_first_sect;
static unsigned short reserved_sectors;

static unsigned long dir_sector;
static unsigned short dir_entries;
static unsigned short dir_offset;
static unsigned short dir_cluster;
static unsigned short dir_size;

static unsigned long top_of_cluster;
static unsigned long number_of_clusters;
static unsigned long offset_cluster = 2;
static unsigned long fat_first_cluster;

static unsigned long file_cluster;
static unsigned long file_offset;
static unsigned long file_size;

static unsigned short last_cluster = 0xffff;
static unsigned long last_offset = 0;
static unsigned char fs_desc = 0;

static unsigned short
read2
(unsigned short off)
{
  return (sdcard_read(off + 1) <<  8) | sdcard_read(off);
}

static unsigned long
read4
(unsigned short off)
{
  return ((unsigned long)read2(off + 2) << 16) | read2(off);
}

static int
fetch_cluster
(unsigned long cluster, unsigned long offset)
{
  if ((cluster == last_cluster) &&
      ((offset & 0xfffffe00) == (last_offset & 0xfffffe00))) return 0;
  printf("fc: cluster = %ld, offset = %ld\n", cluster, offset);
  last_cluster = cluster;
  last_offset = offset;
  unsigned long sector;
  if (0 == cluster) sector = dir_sector + (offset >> 9);
  else {
    while (offset > (sectors_per_cluster << 9)) {
      unsigned long pos =
          ((fat_first_sect + reserved_sectors) << 9) + (cluster << 1);
      if (sdcard_fetch(pos & 0xfffffe00) < 0) return -1;
      cluster = read2(pos & 0x000001ff);
      if ((cluster < 2) || (0xfff7 <= cluster)) return -2;
      offset -= (sectors_per_cluster << 9);
    }
//    unsigned long cluster_sect = dir_sector +
//        (unsigned long)(dir_size + cluster - 2) * sectors_per_cluster;
      unsigned long cluster_sect = top_of_cluster + (cluster - offset_cluster) * sectors_per_cluster;
    printf("cs: %ld = cluster_sect(%ld) + (cls(%ld) - 2) * sec/cls(%ld)\n", 
            cluster_sect, top_of_cluster, cluster, sectors_per_cluster);
    sector = cluster_sect + (offset >> 9);
  }
  return sdcard_fetch(sector << 9);
}

int
fat_init
(void)
{
  if (sdcard_fetch(0) < 0) return -1;
  fs_desc = sdcard_read(OFF_FS_DESC);
  printf("fs_desc: %02X\n", fs_desc);
  if ((0x55 != sdcard_read(510)) || (0xaa != sdcard_read(511))) {
    printf("bad boot sector\n");
    return -2;
  }
#if 0
  if ((4 != fs_desc) && (6 != fs_desc) && (0x0b != fs_desc)) {
    printf("unknown desc: %02X\n", fs_desc);
    return -80 - fs_desc;
  }
#endif
  // sector zero is BPB?
  if (0xEB == sdcard_read(0)) {
    // Now we have already gotten BPB
    fat_first_sect = 0;
  } else {
    // read BPB sector
    fat_first_sect = read4(OFF_P1_1SCT);
    printf("fat_first_sect: %lX (%ld)\n", fat_first_sect, fat_first_sect);

    if (sdcard_fetch(fat_first_sect << 9) < 0) {
      printf("cannot read fat_first_sect\n");
      return -3;
    }
  }
  // check bpb format?
  if ((0xEB != sdcard_read(0)) ||(0x55 != sdcard_read(510)) || (0xaa != sdcard_read(511))) {
    printf("bad fat_first_sect\n");
    return -4;
  }

  // get BPB params

  // number of sectors
  unsigned short bytes_per_sector;
  unsigned char number_of_fats;
  unsigned short rootnum;
  unsigned short sectors_per_fat;
  unsigned long rootdir_sectors;
  unsigned short fat_start_sector;
  unsigned long fat_size;
  bytes_per_sector = read2(OFF_B_P_SCT);   // 11 BytePerSec
  sectors_per_cluster = sdcard_read(OFF_SCT_P_C); // 13 SecPerClus
  reserved_sectors = read2(OFF_SCT_RSV);  // 14 RsvdSecCnt
  number_of_fats = sdcard_read(OFF_NUM_O_F); // 16 NumFATs
  rootnum = read2(OFF_ROOTNUM);            // 17 RootEntCnt
  sectors_per_fat = read2(OFF_SCT_P_F);    // 22 FATSz16
  fat_size = read2(OFF_SCT_P_F);          // 22 FATSz16
  if (0 == fat_size) {
    fat_size = read4(36); // FATSZ32
  }
  fat_start_sector = reserved_sectors;
#if 0
  //printf("fat_size: %lX (%ld)\n", fat_size, fat_size);
  unsigned char number_of_fats = sdcard_read(OFF_NUM_O_F);
  //printf("number_of_fats: %d\n", number_of_fats);
  unsigned short fat_sectors = fat_size * number_of_fats;
  //printf("fat_sectors: %d\n", fat_sectors);

  unsigned int rootdir_start_sector = fat_start_sector + fat_sectors;
  //printf("rootdir_start_sector: %X (%d)\n", rootdir_start_sector, rootdir_start_sector);
  unsigned short bytes_per_sector = read2(OFF_B_P_SCT);
  //printf("bytes_per_sector: %d\n", bytes_per_sector);
  unsigned long rootdir_sectors = (32 * read2(OFF_ROOTNUM) + bytes_per_sector - 1) / bytes_per_sector;
  //printf("rootdir_sectors: %ld\n", rootdir_sectors);

  unsigned long data_start_sector = rootdir_start_sector + rootdir_sectors;
  //printf("data_start_sector: %ld\n", data_start_sector);
  unsigned long totsec = read2(OFF_TOTSEC16);
  //printf("totsec16: %ld\n", totsec);
  if (0 == totsec) {
    totsec = read4(OFF_TOTSEC32);
  }
  printf("totsec: %ld\n", totsec);
  unsigned long data_sectors = totsec - data_start_sector;
  sectors_per_cluster = sdcard_read(OFF_SCT_P_C);
  //printf("sectors_per_cluster: %d\n", sectors_per_cluster);
  unsigned long number_of_clusters = data_sectors / sectors_per_cluster;
  //printf("data_sectors: %ld\n", data_sectors);
  printf("number_of_clusters: %ld\n", number_of_clusters);
  if (number_of_clusters <= 4085) {
    printf("FAT12\n");
  } else if (4086 <= number_of_clusters && number_of_clusters <= 65525) {
    printf("FAT16\n");
  } else if (65526 <= number_of_clusters) {
    printf("FAT32\n");
  } else {
    printf("FAT unbiguious\n");
  }

  printf("--------------\n");
#endif
  rootdir_sectors = (32 * rootnum + bytes_per_sector - 1) / bytes_per_sector;
  printf("11 BytePerSec: %d\n", bytes_per_sector);
  printf("13 SecPerClus: %d\n", sectors_per_cluster);
  printf("16 NumFATs:    %d\n", number_of_fats);
  printf("17 RootEntCnt: %d\n", rootnum);
  printf("22 FATSz16:    %d\n", sectors_per_fat);
  if (sectors_per_fat == 0) {
    // FAT32
    sectors_per_fat = read4(36);  // 36 FATSz32
    printf("32 FATSz32:    %d\n", sectors_per_fat);
  }
  dir_entries = rootnum;     // 17 or 36
  printf("dir_entries: %04X (%d)\n", dir_entries, dir_entries);

  unsigned short fat_sectors = fat_size * number_of_fats;
//  printf("sectors_per_fat: %04X (%d)\n", sectors_per_fat, sectors_per_fat);
//  printf("reserved_sectors: %04X (%d)\n", reserved_sectors, reserved_sectors);
//  printf("sectors_per_cluster: %02X (%d)\n", sectors_per_cluster, sectors_per_cluster);
  dir_sector =
      fat_first_sect + reserved_sectors + sectors_per_fat * number_of_fats;
  printf("dir_sector(%ld) = fat_first_sect(%ld) + resv(%ld) + (sec/fat(%ld)) * num_of_fat(%ld))\n", 
        dir_sector, fat_first_sect, reserved_sectors, sectors_per_fat, number_of_fats);
//  dir_size =
//     ((unsigned short)(dir_entries >> 4) + sectors_per_cluster - 1) /
//     sectors_per_cluster;
  dir_size = (dir_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
  printf("dir_size: %04X (%d)\n", dir_size, dir_size);

  top_of_cluster = fat_first_sect + reserved_sectors + fat_sectors + rootdir_sectors;
  printf("top_of_cluster: %lX (%ld)\n", top_of_cluster, top_of_cluster);
  // totsec: total number of sectors in the drive file
  unsigned long totsec = read2(OFF_TOTSEC16);
  if (0 == totsec) {
    totsec = read4(OFF_TOTSEC32);
  }
  printf("totsec: %ld\n", totsec);
  unsigned long data_capacity = totsec - top_of_cluster * sectors_per_cluster;
  data_capacity /= sectors_per_cluster;
  printf("data_capacity: %ld\n", data_capacity);
  if (data_capacity <= 4085) {
    fs_desc = FS_FAT12;
    printf("FAT12\n");
  } else if (4086 <= data_capacity && data_capacity <= 65525) {
    fs_desc = FS_FAT16;
    printf("FAT16\n");
  } else if (65526 <= data_capacity) {
    fs_desc = FS_FAT32;
    printf("FAT32\n");
    fat_first_cluster = read4(44);   // 44 RootClus
  } else {
    printf("FAT unbiguious\n");
  }

  if (512 != bytes_per_sector) {
    printf("bad bytes_per_sector\n");
    return -5;
  }
  dir_cluster = 0;
  fat_rewind();
  return fs_desc;
}

void
fat_rewind
(void)
{
  dir_offset = 0xffff;
}

int
fat_next
(void)
{
  for (dir_offset++; dir_offset < dir_entries; dir_offset++) {
    unsigned short off = dir_offset % 16;
    if (fetch_cluster(dir_cluster, ((unsigned long)dir_offset << 5)) < 0)
      return -2;
    unsigned char first_char = sdcard_read(off << 5);
    if (0 == first_char) continue;
    if (0 != (0x80 & first_char)) continue;
    if (0x0f == fat_attr()) continue;
    return dir_offset;
  }
  return -1;
}

int
fat_next32
(void)
{

}

void
fat_name
(char *namebuf)
{
  char *p = namebuf;
  unsigned short off = dir_offset % 16;
  int i;
  for (i = 0; i < (8 + 3); i++) {
    unsigned char c = sdcard_read((off << 5) + i);
    if (0x20 != c) {
      if (8 == i) *namebuf++ = '.';
      *namebuf++ = c;
    }
  }
  *namebuf = 0;
  //printf("[%s]\n", p);
}

char
fat_attr
(void)
{
  unsigned short off = dir_offset % 16;
  return sdcard_read((off << 5) + 11);
}

int
fat_chdir
(void)
{
  unsigned short off = dir_offset % 16;
  dir_cluster = read2((off << 5) + 26);
  fat_rewind();
  return 0;
}

unsigned long
fat_size
(void)
{
  unsigned short off = dir_offset % 16;
  return read4((off << 5) + 28);
}

int
fat_open
(void)
{
  unsigned short off = dir_offset % 16;
  unsigned long cluster = ((unsigned long)read2((off << 5) + 20)) << 16;
  file_cluster = cluster | read2((off << 5) + 26);
  printf("file_cluster: %lX (%ld)\n", file_cluster, file_cluster);
  file_size = fat_size();
  printf("file_size: %ld\n", file_size);
  return fat_seek(0);
}

int
fat_seek
(unsigned long pos)
{
  if (file_size <= pos) {
    pos = file_size - 1;
    return -1;
  }
  file_offset = pos;
  return 0;
}

int
fat_read
(void)
{
  return fetch_cluster(file_cluster, file_offset);
}
