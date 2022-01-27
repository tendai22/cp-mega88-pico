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
#include <string.h>

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
#define FS_EXFAT 7  // HPFS/NTFS/exFAT(CHS/LBA)


static unsigned long sectors_per_cluster;
static unsigned long fat_first_sect;
static unsigned short reserved_sectors;
static unsigned short bytes_per_sector = 512;


static unsigned long top_of_dir;
static unsigned long dir_entries;
static unsigned long dir_size;
#if defined(USE_EXFAT)
#define MAX_CLUSTERS 0xffffffff
static unsigned long dir_offset;
static unsigned long dir_cluster;
#else
#define MAX_CLUSTERS 0xffff
static unsigned short dir_offset;
static unsigned short dir_cluster;
#endif //defined(USE_EXFAT)
static unsigned long alloc_cluster;

static unsigned long top_of_cluster;
static unsigned long number_of_clusters;
static unsigned long offset_cluster = 2;
static unsigned long rootdir_cluster;

static unsigned long file_cluster;
static unsigned long file_offset;
static unsigned long file_size;
static unsigned char file_attr;
static unsigned char file_gsflag = 0x01;

#if defined(USE_EXFAT)

#if !defined(MAX_NAMESTR)
#define MAX_NAMESTR 30
#endif //defined(USE_EXFAT)

static unsigned char exfat_attr;
static unsigned long exfat_cluster;
static unsigned long exfat_size;
static unsigned long exfat_filelen;
static unsigned char exfat_gsflag;
static unsigned char exfat_namelen;
static unsigned short exfat_namehash;
static unsigned char namestr[MAX_NAMESTR];
#endif //defined(USE_EXFAT)

static unsigned long last_cluster = 0xffff;
static unsigned long last_offset = 0;
static unsigned char fs_desc = 0;

static int fat_debug = 1;//7;

static int do_fat_next(void);

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
  if (fat_debug) printf("fc: %ld (%0X), %ld\n", cluster, cluster, offset);
  if ((cluster == last_cluster) &&
      ((offset & 0xfffffe00) == (last_offset & 0xfffffe00))) return 0;
  last_cluster = cluster;
  last_offset = offset;
  unsigned long sector;
  if (0 == cluster) sector = top_of_dir + (offset >> 9);
  else {
    unsigned long sec;
    while (offset > (sectors_per_cluster << 9)) {
      if (FS_FAT16 == fs_desc) {
        sec = fat_first_sect + reserved_sectors + ((cluster << 1) >> 9);
        if (sdcard_fetch_sec(sec) < 0) return -1;
        cluster = read2((cluster << 1) % 512);
        if ((cluster < 2) || (0xfff7 <= cluster)) return -2;
      } else if (FS_FAT32 == fs_desc || FS_EXFAT == fs_desc) {
        sec = fat_first_sect + reserved_sectors + ((cluster << 2) >> 9);
        //printf("[sec:%ld]", sec);
        if (sdcard_fetch_sec(sec) < 0) return -1;
        cluster = read4((cluster << 2) % 512);
        if ((cluster < 2) || (0x0ffffff7 <= cluster)) return -2;
      } else if (FS_FAT12 == fs_desc) {
        unsigned int off = ((cluster) >> 1) * 3 + (cluster & 1);
        sec = fat_first_sect + reserved_sectors + off / 512; 
        if (sdcard_fetch_sec(sec) < 0) return -1;
        unsigned short lo = sdcard_read(off & 0x1ff);
        unsigned short hi = sdcard_read((off + 1) & 0x1ff);
        if ((off & 0x1ff) == 0x1ff) {
          // go over the next sector
          if (sdcard_fetch_sec(sec + 1) < 0) return -1;
          hi = sdcard_read((off + 1) & 0x1ff);
        }
        if (cluster & 1) {    // odd number
          cluster = ((lo >> 4) & 0xf) |((hi << 4) & 0xff0);
        } else {              // even
          cluster = lo | ((hi << 8) & 0xf00);
        }
        if ((cluster < 2) || (0xff7 <= cluster)) return -2;
      }
      if (fat_debug) printf("[%03lX]", cluster);
      offset -= (sectors_per_cluster << 9);
    }
    unsigned long cluster_sect = top_of_cluster + (cluster - offset_cluster) * sectors_per_cluster;
    if (fat_debug) printf(" cluster_sect: %ld\n", cluster_sect);
    sector = cluster_sect + (offset >> 9);
  }
  return sdcard_fetch_sec(sector);
}

#if defined(USE_EXFAT)
static int
fat_init_exfat
(void)
{
  printf("exFAT: \n");
  // Now we have exFAT
  unsigned long partition_offset = read4(64); // beginning of this exFAT volume
  unsigned long temp = read4(68);   // high 8-byte, maybe zero
  if (0 != temp) {
    printf("exFAT partition_offset too large.\n");
    return -1;
  }
  //printf("partition_offset:  %ld\n", partition_offset);
  unsigned long totsec = read4(72);
  temp = read4(76);
  if (0 != temp) {
    printf("exFAT total_sectors too big.\n");
    return -2;
  }
  unsigned long cluster_count = read4(92);
  unsigned long fat_offset = read4(80);
  reserved_sectors = fat_offset;    // reserved_sectors is top_of_fat, or fat_offset;
  unsigned long fat_size = read4(84);
  unsigned long cluster_heap_offset = read4(88);
  unsigned char number_of_fats = sdcard_read(110);

  top_of_cluster = partition_offset + cluster_heap_offset;
  // sectors_per_cluster
  unsigned short bytes_per_sector;
  bytes_per_sector = (1 << sdcard_read(108));
  sectors_per_cluster = (1 << sdcard_read(109));
  //
  rootdir_cluster = read4(96);

  printf("total_sectors:  %ld (%lX)\n", totsec, totsec);
  printf("sec/cluster:    %ld\n", sectors_per_cluster);
  printf("# of clusters:  %ld (%lX)\n", cluster_count, cluster_count);
  printf("fat_first_sec:  %ld\n", partition_offset);
  printf("reserved_sectors: %ld\n", fat_offset);
  printf("fat_sectors:    %ld\n", fat_size * number_of_fats);
  printf("top_of_dir:     %ld\n", fat_offset + fat_size * number_of_fats);
  printf("rootdir_sectors: %ld\n", 0);
  printf("top_of_cluster: %ld (%lX)\n", cluster_heap_offset, cluster_heap_offset);

#if 0
  printf("BytePerSec: %d\n", bytes_per_sector);
  printf("SecPerClus: %d\n", sectors_per_cluster);
  printf("FATSz:      %d\n\n", fat_size);
  printf("fat_offset: %d\n", fat_offset);
  printf("fat_end:    %d\n", fat_offset + number_of_fats * fat_size);
  printf("top_of_cluster: %d\n\n", top_of_cluster);
  printf("allocation table size: %d\n", (cluster_count + 7)>>3);
#endif

  // scan root directory
  printf("\nrootdir_cluster: %ld\n", rootdir_cluster);
  dir_cluster = rootdir_cluster;
  dir_entries = 1000;   // not the right way, but an actial way
  fat_rewind();
  long off;
  int n = 100;
  //fat_debug = 1;
  while (n-- > 0) {
    if ((off = fat_next()) == -1) break;
    unsigned char entry_type = sdcard_read(off << 5);
    if (fat_debug) printf("off: %ld, typ: %02X\n", off, entry_type);
    if (entry_type == 0x81) break;
  }
  if (fat_debug & 4) {
    for (int i = 0; i < 32; ++i) {
      printf("%02X ", sdcard_read((off << 5) + i));
      if (i % 16 == 15) {
        printf("\n");
      }
    }
  }
  alloc_cluster = read4((off << 5) + 20);
  printf("alloc_cluster:   %d\n", alloc_cluster);


  // volume flags
  unsigned short volume_flags = read2(106);
  printf("volume_flag: %02X\n", volume_flags);
  fs_desc = FS_EXFAT;
  return fs_desc;
}
#endif //defined(USE_EXFAT)

int
fat_init
(void)
{
  if (sdcard_fetch(0) < 0) return -1;
  if ((0x55 != sdcard_read(510)) || (0xaa != sdcard_read(511))) {
    printf("bad boot sector\n");
    return -2;
  }
  // sector zero is BPB?
  if (0xEB == sdcard_read(0)) {
    // Now we have already gotten BPB
    fat_first_sect = 0;
  } else {
    // We have MBR, now.  Read BPB sector
    fat_first_sect = read4(OFF_P1_1SCT);
    if (sdcard_fetch(fat_first_sect << 9) < 0) {
      printf("cannot read fat_first_sect\n");
      return -3;
    }
  }
  // check bpb format?
  if ((0xEB != sdcard_read(0)) || (0x55 != sdcard_read(510)) || (0xaa != sdcard_read(511))) {
    printf("bad fat_first_sect\n");
    return -4;
  }
  // exFAT or legacy FAT?
  int exfat_flag = 1;
  // If it is exFAT, all of the following bytes are zeros
  for (int i = 11; i < 64; ++i) {
    if (sdcard_read(i) != 0) {
      if (fat_debug) printf("nonzero: %03X %02x\n", i, sdcard_read(i));
      exfat_flag = 0;
      break;
    }
  }
  // dump
  if (fat_debug) {
    printf("BPB sector\n");
    for (int i = 0; i < 512; ++i) {
      if (i % 16 == 0) {
        printf("%03X ", i);
      }
      printf("%02X ", sdcard_read(i));
      if (i % 16 == 15) {
        printf("\n");
      }
    }
  }

  // get BPB params
  #if defined (USE_EXFAT)
  if (0 != exfat_flag) {
    // exFAT
    return fat_init_exfat();
  }
  #endif //defined(USE_EXFAT)
  // FAT12,16,32
  // number of sectors
  unsigned short bytes_per_sector;
  unsigned char number_of_fats;
  //unsigned short sectors_per_fat;
  unsigned short fat_start_sector;
  unsigned long fat_size;
  unsigned long rootdir_sectors;
  unsigned long fat_sectors;
  bytes_per_sector = read2(OFF_B_P_SCT);   // 11 BytePerSec
  sectors_per_cluster = sdcard_read(OFF_SCT_P_C); // 13 SecPerClus
  reserved_sectors = read2(OFF_SCT_RSV);  // 14 RsvdSecCnt
  number_of_fats = sdcard_read(OFF_NUM_O_F); // 16 NumFATs
  dir_entries = read2(OFF_ROOTNUM);            // 17 RootEntCnt
  //sectors_per_fat = read2(OFF_SCT_P_F);    // 22 FATSz16
  fat_size = read2(OFF_SCT_P_F);          // 22 FATSz16
  if (0 == fat_size) {
    fat_size = read4(36); // FATSZ32
  }
  fat_start_sector = reserved_sectors;

  fat_sectors = fat_size * number_of_fats;
#if 0
  printf("11 BytePerSec: %d\n", bytes_per_sector);
  printf("13 SecPerClus: %d\n", sectors_per_cluster);
  printf("14 RsvdSecCnt: %d\n", reserved_sectors);
  printf("16 NumFATs:    %d\n", number_of_fats);
  printf("17 RootEntCnt: %d\n", dir_entries);
  printf("22 FATSz16:    %d\n", fat_size);

  printf("dir_entries: %04X (%d)\n", dir_entries, dir_entries);
  printf("dir_size: %04X (%d)\n", dir_size, dir_size);
#endif
  top_of_dir = fat_first_sect + reserved_sectors + fat_sectors;
  // totsec: total number of sectors in the drive file
  unsigned long totsec = read2(OFF_TOTSEC16);
  if (0 == totsec) {
    totsec = read4(OFF_TOTSEC32);
  }
  //printf("totsec: %ld\n", totsec);
  unsigned long number_of_clusters = totsec - top_of_cluster;
  number_of_clusters /= sectors_per_cluster;
  //printf("number_of_clusters: %ld\n", number_of_clusters);
  if (number_of_clusters <= 4085) {
    fs_desc = FS_FAT12;
    printf("FAT12\n");
  } else if (4086 <= number_of_clusters && number_of_clusters <= 65525) {
    fs_desc = FS_FAT16;
    printf("FAT16\n");
  } else if (65526 <= number_of_clusters) {
    fs_desc = FS_FAT32;
    printf("FAT32\n");
    rootdir_cluster = read4(44);   // 44 RootClus
    dir_entries = 65536;
    dir_size = (dir_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
  } else {
    printf("FAT unbiguious\n");
  }
  if (FS_FAT12 == fs_desc || FS_FAT16 == fs_desc)
    rootdir_sectors = (32 * dir_entries + bytes_per_sector - 1) / bytes_per_sector;
  else 
    rootdir_sectors = 0;   // In FAT32 and exFAT, we use a cluster chain as root directory entries.

  top_of_cluster = fat_first_sect + reserved_sectors + fat_sectors + rootdir_sectors;

  printf("total_sectors:  %ld (%X)\n", totsec, totsec);
  printf("sec/cluster:    %ld\n", sectors_per_cluster);
  printf("# of clusters:  %ld (%X)\n", number_of_clusters, number_of_clusters);
  printf("fat_first_sect: %ld\n", fat_first_sect);
  printf("reserved_sectors: %ld\n", reserved_sectors);
  printf("fat_sectors:    %ld\n", fat_sectors);
  printf("top_of_dir:     %ld\n", top_of_dir);
  printf("rootdir_sectors: %ld\n", rootdir_sectors);
  printf("top_of_cluster: %ld (%lX)\n", top_of_cluster, top_of_cluster);
  if (FS_FAT32 == fs_desc)
    printf("rootdir_cluster: %ld\n", rootdir_cluster);


  if (512 != bytes_per_sector) {
    printf("bad bytes_per_sector\n");
    return -5;
  }
  dir_cluster = 0;
  if (FS_FAT32 == fs_desc) {
    dir_cluster = rootdir_cluster;
  }
  fat_rewind();
  return fs_desc;
}

void
fat_rewind
(void)
{
  dir_offset = MAX_CLUSTERS;
}

#if defined(USE_EXFAT)
int
fat_next
(void)
{
  long offset, off;
  unsigned char first_char, *p;
  int rest, len;
  if (FS_EXFAT != fs_desc)
    return do_fat_next();
  if (fat_debug & 2) printf("fat_next: %ld, %ld\n", dir_offset, dir_entries);
  while ((offset = do_fat_next()) >= 0) {
    off = dir_offset % 16;
    first_char = sdcard_read(off << 5);
    if (fat_debug & 2) printf("first_char: %02X\n", first_char);
    if (0 == first_char) break; // end-of-entry mark
    if (0 == (0x80 & first_char)) continue;  // unused entry
    if (0x85 != first_char) continue; // skip any entry if it is not a file-directory entry
    // Now we have 0x85, file-directory entry
    rest = sdcard_read((off << 5) + 1);  // number of the rest entries
    exfat_attr = sdcard_read((off << 5) + 4);
    namestr[0] = '\0';
    if (fat_debug) printf("%02X: attr: %02X, rest: %d\n", first_char, exfat_attr, rest);
    for (; rest > 0; rest--) {
      if ((offset = do_fat_next()) < 0) break;
      off = offset % 16;
      switch (first_char = sdcard_read(off << 5)) {
      case 0xC0:
        exfat_gsflag = sdcard_read((off << 5) + 1);
        exfat_namelen = sdcard_read((off << 5) + 3);
        exfat_namehash = read2((off << 5) + 4);
        exfat_filelen = read4((off << 5) + 24);
        exfat_cluster = read4((off << 5) + 20);
        if (fat_debug) printf("%02X: flag: %02X, nlen: %ld, hash: %04X, flen: %ld, cluster: %08X\n", first_char, exfat_gsflag, exfat_namelen, exfat_namehash, exfat_filelen, exfat_cluster);
        exfat_size = exfat_filelen;
        break;
      case 0xC1:
        len = strlen(namestr);
        p = namestr + len;
        len = sizeof namestr - len;
        if (len <= 1) {
          // ignore this entry
          if (fat_debug) printf("%02X: name: %s ignored\n", first_char, namestr);
          break;
        }
        for (int i = 2; len > 1 && i < 32; i += 2, len--) {
          unsigned short wc = read2((off << 5) + i);
          if(fat_debug & 4) printf("[%04X]", wc);
          if (0 == wc) break;
          *p++ = (wc < 0x7f) ? (wc & 0xff) : 0xe5;
        }
        *p = '\0';
        if (fat_debug) printf( "\n%02X: name: %s\n", first_char, namestr);
        if (fat_debug & 4) {
          for (int i = 0; i < strlen(namestr); ++i)
            printf("[%02X]", namestr[i]);
          printf("\n");
        }
        break;
      default:
        if (fat_debug & 2) printf("%02X: ignore\n", first_char);
        break;
      }

    }
    // end-of-exFAT
    if (fat_debug & 2) printf("off: %08X\n", dir_offset);
    return offset >= 0 ? dir_offset : -1;
  }
  return -1;
}
#else
int
fat_next
(void)
{
  return do_fat_next();
}
#endif //defined(USE_EXFAT)

static int
do_fat_next
(void)
{
  static int count = 200;
  if (fat_debug) printf("do_fat_next: dir_offset: %ld, dir_entries: %ld\n", dir_offset, dir_entries);
  for (dir_offset++; dir_offset < dir_entries; dir_offset++) {
    unsigned short off = dir_offset % 16;
    //if (count-- <= 0) return -1;
    if (fetch_cluster(dir_cluster, ((unsigned long)dir_offset << 5)) < 0)
      return -2;
    unsigned char first_char = sdcard_read(off << 5);
    //if (fat_debug) printf("first_char: %02X\n", first_char);
    if (0 == first_char) break; // end-of-entry mark
#if defined(USE_EXFAT)
    if (FS_EXFAT == fs_desc) return dir_offset;
#endif //defined(USE_EXFAT)
    // FAT12,16,32
    if (0xe5 == first_char) continue; // unused entry
    if (0x0f == fat_attr()) continue; // LFN entry, we ignore it in FAT12,16,32
    return dir_offset;
  }
  return -1;
}



void
fat_name
(char *namebuf, int len)
{
  char *p = namebuf + strlen(namebuf);
  len -= strlen(namebuf);
  unsigned short off = dir_offset % 16;
  unsigned char first_char;
  int i;
#if defined(USE_EXFAT)
  if (FS_EXFAT == fs_desc) {
    int len2 = strlen(namestr);
    if (len2 > len - 1) {
      len2 = len - 1;
    }
    memcpy(namebuf, namestr, len2);
    namebuf[len2] = '\0';
    return;
  }
#endif //defined(USE_EXFAT)
  // for FAT12,16,32 ... rewrite for 8.3 format
  for (i = 0; i < (8 + 3); i++) {
    unsigned char c = sdcard_read((off << 5) + i);
    if (0x20 != c) {
      if (8 == i) *namebuf++ = '.';
      *namebuf++ = c;
    }
  }
  *namebuf = 0;
}

char
fat_attr
(void)
{
  unsigned short off = dir_offset % 16;
  if (FS_EXFAT == fs_desc) {
    if (0x85 == sdcard_read((off << 5) + 0)) {
      exfat_attr = sdcard_read((off << 5) + 4);
    }
  } else {
    return sdcard_read((off << 5) + 11);
  }
}

#if defined(USE_EXFAT)
char
fat_gsflag
(void)
{
  return exfat_gsflag;
}
#endif //defined(USE_EXFAT)

int
fat_chdir
(void)
{
  unsigned short off = dir_offset % 16;
  dir_cluster = ((unsigned long)read2((off << 5) + 20) << 16) | read2((off << 5) + 26);
  printf("chdir: new cluster: %0X (%ld)\n", dir_cluster, dir_cluster);
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
  unsigned long cluster;
  if (fs_desc == FS_EXFAT) {
    // exFAT
    if (0 == exfat_size) return -1;
    file_gsflag = exfat_gsflag;
    file_cluster = exfat_cluster;
    file_size = exfat_size;
    if (fat_debug) printf("file_cluster: %ld, file_size: %ld\n", file_cluster, file_size);
  } else {
    // FAT12,16,32
    cluster = ((unsigned long)read2((off << 5) + 20)) << 16;
    file_gsflag = 0x01;   // default, FATChain enable(bit1 is 0)
    file_cluster = cluster | read2((off << 5) + 26);
    file_size = fat_size();
  }
  if (fat_debug) printf("file_cluster: %lX (%ld)\n", file_cluster, file_cluster);
  if (fat_debug) printf("file_size: %ld\n", file_size);
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
    // use file_gsflag, file_cluster, file_offset.
#if defined(USE_EXFAT)
  if (FS_EXFAT == fs_desc) {
    if (file_gsflag & 2) {
      // NoFatChain mode
      unsigned long cluster_sect;
      cluster_sect = top_of_cluster + (file_cluster - offset_cluster) * sectors_per_cluster;
      // cluster_sect ... top_of_file sector
      unsigned long sector = cluster_sect + (file_offset >> 9);
      if (fat_debug) printf("fr: sector = %ld\n", sector);
      return sdcard_fetch(sector << 9);
    }
    // FatChain mode, go down to use fetch_cluster.
  }
#endif
  return fetch_cluster(file_cluster, file_offset);
}

#if defined(CHK_FAT)
void
fat_chk(void)
{
  if (file_cluster == 0) {
    printf("fat file has not yet opened\n");
    return;
  }
  //
  int flag_save = fat_debug;
  //fat_debug = 1;
  unsigned long offset;
  printf("file_size: %ld\n", file_size);
  for (offset = 0; offset < file_size; offset += 512) {
    fat_seek(offset);
    fat_read();
    for (int i = 0; i < 16; ++i) {
      if ((i % 16) == 0) {
        printf("%08X ", offset);
      }
      printf("%02X ", sdcard_read(i));
      if ((i % 16) == 15) {
        printf("\n");
      }
    }
  }
  fat_debug = flag_save;
}
#endif //defined(CHK_FAT)