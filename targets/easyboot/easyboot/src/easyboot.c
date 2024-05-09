/*
 * src/easyboot.c
 * https://gitlab.com/bztsrc/easyboot
 *
 * Copyright (C) 2023 bzt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @brief The bootable disk image generator and loader installer utility
 *
 * Since this tool is run by users in a hosted environment, and not during boot
 * like the rest, so for portability it has both POSIX and WIN32 bindings. Mostly
 * tested on Linux though, the mingw version might have bugs.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef PATH_MAX
#undef PATH_MAX
#endif
#define HOSTED
#include "loader.h"
#include "data.h"
#ifdef __WIN32__
#include <windows.h>
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define FIRST_PARTITION 2048        /* in sectors, 1M alignment */
#define GPT_SECTORS       62        /* must be smaller than than FIRST_PARTITION - 2 and 16 * 2048 (ISO9660 start) */

const char ebver[] = "1.0.0";       /* tool's version (loaders have different versions) */

/*** multilingual stuff ***/
enum {
    /* error messages */
    ERRMEM,
    ERRCLU,
    ERRMNT,
    ERRSIZ,
    ERRSPC,
    ERRDIR,
    ERRMUCH,
    ERRENC,
    ERRWR,
    ERRRD,
    ERRCFG,
    ERRPART,
    ERRPMBR,
    ERRBPB,
    ERRFAT,
    ERRPLG,
    /* usage instructions */
    DISKTOOL,
    VERBOSITY,
    DISKSIZE,
    PARTSIZE,
    PARTUUID,
    ROOTPART,
    ELTORITO,
    CREATE,
    INDIR,
    OUTFILE,
    CFGFILE,
    LDRVER,
    /* messages */
    EXPPART,
    EXPDISK,
    FORMAT,
    ADD,
    LINE,
    NOTINME,
    NOTOUTME,
    BADVERBOSE,
    BADFB,
    BADCOLOR,
    BADDEFAULT,
    BADALIGN,
    NOKERNEL,
    TOOMANY,
    TOOLONG,
    NOTFND,
    BADTGA,
    BADICON,
    BADLABEL,
    BADUUID,
    NOMULTI,
    UNKNOWN,
    NOMENU,
    NOPART,
    NOFSPLG,
    NOSMPPLG,
    /* must be the last */
    NUMTEXTS
};
#define NUMLANGS         10

char **lang, *dict[NUMLANGS][NUMTEXTS + 1] = {
{
#include "lang/en.h"
},{
#include "lang/hu.h"
},{
#include "lang/de.h"
},{
#include "lang/fr.h"
},{
#include "lang/es.h"
},{
#include "lang/it.h"
},{
#include "lang/pl.h"
},{
#include "lang/ru.h"
},{
#include "lang/zh.h"
},{
#include "lang/ja.h"
} };

/*** variables ***/
char *archs[] = { "x86_64", "aarch64", NULL };
guid_t dguid, pguid, ptguid[100], puguid[100], menupart[9];
int64_t disksize = 35, partsize = 33, psize[100] = { 0 }, imgsize, totsize = 0, fat_numclu, fat_freeclu, read_size;
uint8_t *img = NULL, img_tail[(GPT_SECTORS+1)<<9], *fs_base, *cluster = NULL, **plgs = NULL, *pdata = NULL;
uint32_t *fat_fat32 = NULL, fat_nextcluster, ts_x86, ts_rpi;
uint64_t boot_lba = 0, data_lba = 0;
esp_bpb_t *bpb;
void *f;
char *in = NULL, *out = NULL, *pfile[100] = { 0 };
char full[PATH_MAX], guidstr[64];
int verbose = 0, skipbytes = 0, fat_bpc, fat_spf, num_plgs = 0, gpt_spe = 0, nump = 0;
struct tm *fat_ts;
struct stat dev_st;
#ifdef __WIN32__
WCHAR szFile[PATH_MAX];
#else
char *realpath(const char*, char*);
int fdatasync(int);
#endif
int dev_read(void *f, uint64_t off, void *buf, uint32_t size);
int dev_write(void *f, uint64_t off, void *buf, uint32_t size);
int check_plugin(char *fn, uint8_t *buf, uint32_t size);

void dump(uint8_t *ptr)
{
    int i;
    for(i = 0; i < 16; i++) printf("%02x ", ptr[i]);
    printf(" ");
    for(i = 0; i < 16; i++) printf("%c", ptr[i] >= 32 && ptr[i] < 127 ? ptr[i] : '.');
    printf("\n");
}

/**
 * Convert hex string into integer
 */
uint64_t gethex(char *ptr, int len)
{
    uint64_t ret = 0;
    for(;len--;ptr++) {
        if(*ptr>='0' && *ptr<='9') {          ret <<= 4; ret += (unsigned int)(*ptr-'0'); }
        else if(*ptr >= 'a' && *ptr <= 'f') { ret <<= 4; ret += (unsigned int)(*ptr-'a'+10); }
        else if(*ptr >= 'A' && *ptr <= 'F') { ret <<= 4; ret += (unsigned int)(*ptr-'A'+10); }
        else break;
    }
    return ret;
}

/**
 * Parse a GUID in string into binary representation
 */
void getguid(char *ptr, guid_t *guid)
{
    int i;

    if(!ptr || !*ptr || ptr[8] != '-' || ptr[13] != '-' || ptr[18] != '-') return;
    memset(guid, 0, sizeof(guid_t));
    guid->Data1 = gethex(ptr, 8); ptr += 9;
    guid->Data2 = gethex(ptr, 4); ptr += 5;
    guid->Data3 = gethex(ptr, 4); ptr += 5;
    guid->Data4[0] = gethex(ptr, 2); ptr += 2;
    guid->Data4[1] = gethex(ptr, 2); ptr += 2; if(*ptr == '-') ptr++;
    for(i = 2; i < 8; i++, ptr += 2) guid->Data4[i] = gethex(ptr, 2);
}

/**
 * Print a binary GUID in human readable form
 */
char *prtguid(guid_t *guid)
{
    sprintf(guidstr, "%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X",
        guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3], guid->Data4[4],
        guid->Data4[5], guid->Data4[6], guid->Data4[7]);
    return guidstr;
}

/**
 * Read a file entirely into memory
 */
unsigned char* readfileall(char *file, int chkonly)
{
#ifdef __WIN32__
    HANDLE f;
    DWORD r, t;
    int i;
#else
    FILE *f;
#endif
    unsigned char *data = NULL;

    read_size = 0;
    if(!file || !*file) return NULL;
    data = (unsigned char*)strrchr(file, '/');
    if(data && !*(data + 1)) return NULL;
    data = NULL;
#ifdef __WIN32__
    memset(&szFile, 0, sizeof(szFile));
    MultiByteToWideChar(CP_UTF8, 0, file, -1, szFile, PATH_MAX);
    for(i = 0; szFile[i]; i++) if(szFile[i] == L'/') szFile[i] = L'\\';
    f = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(f != INVALID_HANDLE_VALUE) {
        r = GetFileSize(f, NULL);
        read_size = (int64_t)r;
        if(!chkonly) {
            data = (unsigned char*)malloc(read_size + 1);
            if(!data) { fprintf(stderr, "%seasyboot: %s\r\n", verbose ? "\n" : "", lang[ERRMEM]); exit(1); }
            memset(data, 0, read_size + 1);
            if(!ReadFile(f, data, r, &t, NULL)) t = 0;
            read_size = (int64_t)t;
        }
        CloseHandle(f);
    }
#else
    f = fopen(file, "rb");
    if(f) {
        fseek(f, 0L, SEEK_END);
        read_size = (int64_t)ftell(f);
        fseek(f, 0L, SEEK_SET);
        if(!chkonly) {
            data = (unsigned char*)malloc(read_size + 1);
            if(!data) { fprintf(stderr, "%seasyboot: %s\r\n", verbose ? "\n" : "", lang[ERRMEM]); exit(1); }
            memset(data, 0, read_size + 1);
            read_size = (int64_t)fread(data, 1, read_size, f);
        }
        fclose(f);
    }
#endif
    if(!read_size && data) { free(data); data = NULL; }
    return data;
}

/**
 * CRC calculation with precalculated lookup table
 */
uint32_t crc32_lookup[256]={
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832,
    0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a,
    0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
    0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
    0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4,
    0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074,
    0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525,
    0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
    0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76,
    0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6,
    0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
    0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7,
    0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
    0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330,
    0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};
uint32_t crc32_calc(unsigned char *start,int length)
{
    uint32_t crc32_val=0xffffffff;
    while(length--) crc32_val=(crc32_val>>8)^crc32_lookup[(crc32_val&0xff)^(unsigned char)*start++];
    crc32_val^=0xffffffff;
    return crc32_val;
}

/**
 * Convert LBA to CHS
 */
void chs(uint32_t lba, void *chs)
{
    /* we don't have a real geometry, so assume 16 heads and 63 sectors */
    uint64_t c, h = 16, s = 63;
    c = lba / (h*s); lba %= h*s;
    *((uint8_t*)(chs+3)) = c & 0xff;
    *((uint8_t*)(chs+2)) = ((c >> 2) & 0xc0) | ((lba % s) + 1);
    *((uint8_t*)(chs+1)) = lba / s;
}

/**
 * Add GUID Partitioning Table
 */
void gpt_create(void)
{
    uint64_t lba;
    guid_t efiguid = EFI_PART_TYPE_EFI_SYSTEM_PART_GUID;
    gpt_header_t *hdr = (gpt_header_t *)(img + 512);
    gpt_entry_t *entry = (gpt_entry_t *)(img + 1024);
    int i, j;
    char *name = "EFI System Partition", *part = "Partition ";

    /* Protective Master Boot Record (we don't really need nor use this, just for backward compatibility) */
    memcpy(img, boot_x86_bin, 512);
    img[0x1FE]=0x55; img[0x1FF]=0xAA;
    memcpy(img+0x1B8, &dguid.Data1, 4);             /* WinNT disk id */
    /* MBR, EFI System Partition / boot partition. */
    img[0x1C0-2]=0x80;                              /* bootable flag */
    chs(FIRST_PARTITION, img+0x1BE);                /* start CHS */
    img[0x1C0+2]=0xC;                               /* type, LBA FAT32 (0xC) */
    chs(FIRST_PARTITION+partsize/512-1, img+0x1C2); /* end CHS */
    *((uint32_t*)(img+0x1C0+6)) = FIRST_PARTITION;  /* start LBA */
    *((uint32_t*)(img+0x1C0+10)) = partsize/512;    /* number of sectors */
    /* MBR, protective img entry */
    chs(1, img+0x1CE);                              /* start CHS */
    img[0x1D0+2]=0xEE;                              /* type */
    chs(GPT_SECTORS-1, img+0x1D2);                  /* end CHS */
    *((uint32_t*)(img+0x1D0+6)) = 1;                /* start LBA */
    *((uint32_t*)(img+0x1D0+10)) = GPT_SECTORS-1;   /* number of sectors */

    /* GPT header */
    memcpy(&hdr->Signature, EFI_PTAB_HEADER_ID, 8);
    hdr->Revision = 0x10000;
    hdr->HeaderSize = 92;
    hdr->MyLBA = 1;
    hdr->AlternateLBA = disksize / 512 - 1;
    hdr->FirstUsableLBA = FIRST_PARTITION;
    hdr->LastUsableLBA = disksize / 512 - GPT_SECTORS - 1;
    memcpy(&hdr->DiskGUID, &dguid, sizeof(guid_t));
    hdr->PartitionEntryLBA = 2;
    hdr->NumberOfPartitionEntries = GPT_SECTORS * 512 / sizeof(gpt_entry_t);
    hdr->SizeOfPartitionEntry = sizeof(gpt_entry_t);

    /* GPT, EFI System Partition (ESP) */
    memcpy(&entry->PartitionTypeGUID, &efiguid, sizeof(guid_t));
    memcpy(&entry->UniquePartitionGUID, &pguid, sizeof(guid_t));
    entry->StartingLBA = FIRST_PARTITION;
    entry->EndingLBA = FIRST_PARTITION + (partsize / 512) - 1;
    for(i = 0; name[i]; i++) entry->PartitionName[i] = name[i];
    lba = (FIRST_PARTITION * 512 + partsize + 1024*1024-1) & ~(1024*1024-1);

    for(j = 0; j < nump; j++)
        if(psize[j] > 0) {
            /* GPT, Extra Partition(s) */
            entry++;
            memcpy(&entry->PartitionTypeGUID, &ptguid[j], sizeof(guid_t));
            memcpy(&entry->UniquePartitionGUID, &puguid[j], sizeof(guid_t));
            entry->StartingLBA = lba / 2048;
            lba += (psize[i] + 1024*1024-1) & ~(1024*1024-1);
            entry->EndingLBA = lba / 2048 - 1;
            for(i = 0; part[i]; i++) entry->PartitionName[i] = part[i];
            entry->PartitionName[i++] = (j / 10) + '0';
            entry->PartitionName[i++] = (j % 10) + '0';
        }

    /* calculate checksums */
    hdr->PartitionEntryArrayCRC32 = crc32_calc((unsigned char*)entry, hdr->NumberOfPartitionEntries * hdr->SizeOfPartitionEntry);
    hdr->CRC32 = crc32_calc((unsigned char*)hdr, hdr->HeaderSize);

    /* secondary GPT */
    memcpy(img_tail, img + 1024, GPT_SECTORS * 512);
    hdr = (gpt_header_t*)(img_tail + GPT_SECTORS * 512);
    memcpy(hdr, img + 512, 512);
    hdr->MyLBA = disksize / 512 - 1;
    hdr->AlternateLBA = 1;
    hdr->PartitionEntryLBA = disksize / 512 - GPT_SECTORS - 1;
    hdr->CRC32 = 0; hdr->CRC32 = crc32_calc((unsigned char*)hdr, hdr->HeaderSize);
}

/**
 * Set an integer value both little-endian and big-endian in El Torito records
 */
void setinte(uint32_t val, unsigned char *ptr) {
    uint8_t *v = (uint8_t*)&val;
    ptr[0] = ptr[7] = v[0]; ptr[1] = ptr[6] = v[1]; ptr[2] = ptr[5] = v[2]; ptr[3] = ptr[4] = v[3];
}

/**
 * Add ISO9660 El Torito Boot Catalog for bootable EFI CDROM
 */
void etbc_create(void)
{
    uint8_t *iso = img + 16 * 2048;
    char isodate[128];
    int i;

    /* from the UEFI spec section 12.3.2.1 ISO-9660 and El Torito
      "...A Platform ID of 0xEF indicates an EFI System Partition. The Platform ID is in either the Section
      Header Entry or the Validation Entry of the Booting Catalog as defined by the “El Torito”
      specification. EFI differs from “El Torito” “no emulation” mode in that it does not load the “no
      emulation” image into memory and jump to it. EFI interprets the “no emulation” image as an EFI
      system partition."
     * so we must record the ESP in the Boot Catalog, that's how UEFI locates it */

    sprintf((char*)&isodate, "%04d%02d%02d%02d%02d%02d00",
        fat_ts->tm_year+1900,fat_ts->tm_mon+1,fat_ts->tm_mday,fat_ts->tm_hour,fat_ts->tm_min,fat_ts->tm_sec);

    /* 16th sector: Primary Volume Descriptor */
    iso[0]=1;   /* Header ID */
    memcpy(&iso[1], "CD001", 5);
    iso[6]=1;   /* version */
    for(i=8;i<72;i++) iso[i]=' ';
    memcpy(&iso[40], "EFI CDROM", 9);   /* Volume Identifier */
    setinte((FIRST_PARTITION*512+partsize+2047)/2048, &iso[80]);
    iso[120]=iso[123]=1;        /* Volume Set Size */
    iso[124]=iso[127]=1;        /* Volume Sequence Number */
    iso[129]=iso[130]=8;        /* logical blocksize (0x800) */
    iso[156]=0x22;              /* root directory recordsize */
    setinte(20, &iso[158]);     /* root directory LBA */
    setinte(2048, &iso[166]);   /* root directory size */
    iso[174]=fat_ts->tm_year;   /* root directory create date */
    iso[175]=fat_ts->tm_mon+1;
    iso[176]=fat_ts->tm_mday;
    iso[177]=fat_ts->tm_hour;
    iso[178]=fat_ts->tm_min;
    iso[179]=fat_ts->tm_sec;
    iso[180]=0;                 /* timezone UTC (GMT) */
    iso[181]=2;                 /* root directory flags (0=hidden,1=directory) */
    iso[184]=1;                 /* root directory number */
    iso[188]=1;                 /* root directory filename length */
    for(i=190;i<813;i++) iso[i]=' ';    /* Volume data */
    memcpy(&iso[318], "EASYBOOT <HTTPS://GITLAB.COM/BZTSRC/EASYBOOT>", 45);
    memcpy(&iso[446], "EASYBOOT", 8);
    memcpy(&iso[574], "EFI CDROM", 9);
    for(i=702;i<813;i++) iso[i]=' ';    /* file descriptors */
    memcpy(&iso[813], &isodate, 16);    /* volume create date */
    memcpy(&iso[830], &isodate, 16);    /* volume modify date */
    for(i=847;i<863;i++) iso[i]='0';    /* volume expiration date */
    for(i=864;i<880;i++) iso[i]='0';    /* volume shown date */
    iso[881]=1;                         /* filestructure version */
    for(i=883;i<1395;i++) iso[i]=' ';   /* file descriptors */
    /* 17th sector: Boot Record Descriptor */
    iso[2048]=0;    /* Header ID */
    memcpy(&iso[2049], "CD001", 5);
    iso[2054]=1;    /* version */
    memcpy(&iso[2055], "EL TORITO SPECIFICATION", 23);
    setinte(19, &iso[2048+71]);         /* Boot Catalog LBA */
    /* 18th sector: Volume Descritor Terminator */
    iso[4096]=0xFF; /* Header ID */
    memcpy(&iso[4097], "CD001", 5);
    iso[4102]=1;    /* version */
    /* 19th sector: Boot Catalog */
    iso[6144]=1;    /* Header ID, Validation Entry */
    iso[6145]=0;    /* Platform 80x86 */
    iso[6172]=0xaa; /* magic bytes */
    iso[6173]=0x55;
    iso[6174]=0x55;
    iso[6175]=0xaa;
    iso[6176]=0x88; /* Bootable, Initial/Default Entry */
    iso[6182]=4;    /* Sector Count */
    *((uint32_t*)(iso + 6184)) = FIRST_PARTITION/4;  /* ESP Start LBA */
    iso[6208]=0x91; /* Header ID, Final Section Header Entry */
    iso[6209]=0xEF; /* Platform EFI */
    iso[6210]=1;    /* Number of entries */
    iso[6240]=0x88; /* Bootable, Section Entry */
    *((uint32_t*)(iso + 6248)) = FIRST_PARTITION/4;  /* ESP Start LBA */
    /* 20th sector: Root Directory */
    /* . */
    iso[8192]=0x21 + 1;          /* recordsize */
    setinte(20, &iso[8194]);     /* LBA */
    setinte(2048, &iso[8202]);   /* size */
    iso[8210]=fat_ts->tm_year;   /* date */
    iso[8211]=fat_ts->tm_mon+1;
    iso[8212]=fat_ts->tm_mday;
    iso[8213]=fat_ts->tm_hour;
    iso[8214]=fat_ts->tm_min;
    iso[8215]=fat_ts->tm_sec;
    iso[8216]=0;                 /* timezone UTC (GMT) */
    iso[8217]=2;                 /* flags (0=hidden,1=directory) */
    iso[8220]=1;                 /* serial */
    iso[8224]=1;                 /* filename length */
    iso[8225]=0;                 /* filename '.' */
    /* .. */
    iso[8226]=0x21 + 1;          /* recordsize */
    setinte(20, &iso[8228]);     /* LBA */
    setinte(2048, &iso[8236]);   /* size */
    iso[8244]=fat_ts->tm_year;   /* date */
    iso[8245]=fat_ts->tm_mon+1;
    iso[8246]=fat_ts->tm_mday;
    iso[8247]=fat_ts->tm_hour;
    iso[8248]=fat_ts->tm_min;
    iso[8249]=fat_ts->tm_sec;
    iso[8250]=0;                 /* timezone UTC (GMT) */
    iso[8251]=2;                 /* flags (0=hidden,1=directory) */
    iso[8254]=1;                 /* serial */
    iso[8258]=1;                 /* filename length */
    iso[8259]='\001';            /* filename '..' */
}

/**
 * Finish FAT
 */
void fat_finish(void)
{
    int i;

    fat_nextcluster -= 2;
    i = fat_freeclu ? fat_freeclu : ((partsize - (fat_spf*fs_base[0x10]+fs_base[0xE]) * 512)/fat_bpc) - fat_nextcluster;
    fs_base[0x3E8] = i & 0xFF; fs_base[0x3E9] = (i >> 8) & 0xFF;
    fs_base[0x3EA] = (i >> 16) & 0xFF; fs_base[0x3EB] = (i >> 24) & 0xFF;
    fs_base[0x3EC] = fat_nextcluster & 0xFF; fs_base[0x3ED] = (fat_nextcluster >> 8) & 0xFF;
    fs_base[0x3EE] = (fat_nextcluster >> 16) & 0xFF; fs_base[0x3EF] = (fat_nextcluster >> 24) & 0xFF;
    /* copy backup boot sectors */
    memcpy(fs_base + (fs_base[0x32]*512), fs_base, 1024);
}

/**
 * Add a 8.3 FAT directory entry
 */
uint8_t *fat_dirent83(uint8_t *ptr, char *name, int type, uint32_t clu, uint32_t size)
{
    int i;

    for(i = 0; i < 11 && name[i]; i++) ptr[i] = name[i];
    while(i < 11) ptr[i++] = ' ';
    memset(ptr + 11, 0, 21);
    ptr[0xB] = type;
    i = (fat_ts->tm_hour << 11) | (fat_ts->tm_min << 5) | (fat_ts->tm_sec/2);
    ptr[0xE] = ptr[0x16] = i & 0xFF; ptr[0xF] = ptr[0x17] = (i >> 8) & 0xFF;
    i = ((fat_ts->tm_year+1900-1980) << 9) | ((fat_ts->tm_mon+1) << 5) | (fat_ts->tm_mday);
    ptr[0x10] = ptr[0x12] = ptr[0x18] = i & 0xFF; ptr[0x11] = ptr[0x13] = ptr[0x19] = (i >> 8) & 0xFF;
    ptr[0x1A] = clu & 0xFF; ptr[0x1B] = (clu >> 8) & 0xFF;
    ptr[0x14] = (clu >> 16) & 0xFF; ptr[0x15] = (clu >> 24) & 0xFF;
    ptr[0x1C] = size & 0xFF; ptr[0x1D] = (size >> 8) & 0xFF;
    ptr[0x1E] = (size >> 16) & 0xFF; ptr[0x1F] = (size >> 24) & 0xFF;
    return ptr + 32;
}

/**
 * Format the partition to FAT32
 */
void fat_format(void)
{
    int i = FIRST_PARTITION;
    uint32_t *fat_fat32_1, *fat_fat32_2;
    uint8_t *fat_rootdir;

    fat_numclu = partsize / 512; fat_freeclu = 0;
    if(fat_numclu < 67584) { fprintf(stderr, "easyboot: %s\r\n", lang[ERRCLU]); exit(1); }
    /* make BPB (FAT superblock) */
    memcpy(fs_base + 3, "MSWIN4.1", 8);
    fs_base[0xC] = 2; fs_base[0x10] = 2; fs_base[0x15] = 0xF8; fs_base[0x1FE] = 0x55; fs_base[0x1FF] = 0xAA;
    fs_base[0x18] = 240; fs_base[0x19] = 3; fs_base[0x1A] = 16;
    memcpy(fs_base + 0x1C, &i, 4);
    memcpy(fs_base + 0x20, &fat_numclu, 4);
    fs_base[0xD] = 1; fs_base[0xE] = 8;
    fat_spf = (fat_numclu*4) / 512 + fs_base[0xE];
    fs_base[0x24] = fat_spf & 0xFF; fs_base[0x25] = (fat_spf >> 8) & 0xFF;
    fs_base[0x26] = (fat_spf >> 16) & 0xFF; fs_base[0x27] = (fat_spf >> 24) & 0xFF;
    fs_base[0x2C] = 2; fs_base[0x30] = 1; fs_base[0x32] = 6; fs_base[0x40] = 0x80; fs_base[0x42] = 0x29;
    memcpy(fs_base + 0x43, &pguid, 4);
    memcpy(fs_base + 0x47, "EFI System FAT32   ", 19);
    memcpy(fs_base + 0x200, "RRaA", 4); memcpy(fs_base + 0x3E4, "rrAa", 4);
    for(i = 0; i < 8; i++) fs_base[0x3E8 + i] = 0xFF;
    fs_base[0x3FE] = 0x55; fs_base[0x3FF] = 0xAA;
    fat_bpc = fs_base[0xD] * 512;
    fat_rootdir = fs_base + (fat_spf*fs_base[0x10]+fs_base[0xE]) * 512;
    fat_fat32_1 = (uint32_t*)(&fs_base[fs_base[0xE] * 512]);
    fat_fat32_2 = (uint32_t*)(&fs_base[(fs_base[0xE]+fat_spf) * 512]);
    fat_fat32_1[0] = fat_fat32_2[0] = 0x0FFFFFF8;
    fat_fat32_1[1] = fat_fat32_2[1] = fat_fat32_1[2] = fat_fat32_2[2] = 0x0FFFFFFF;
    fat_nextcluster = 3;
    /* label in root directory */
    fat_dirent83(fat_rootdir, "EFI System ", 8, 0, 0);
    fat_finish();
}

/**
 * Find a free cluster (or more)
 */
uint32_t fat_findclu(uint32_t cnt)
{
    uint32_t i, clu;

    if(cnt < 1) cnt = 1;
    if(fat_freeclu < (int64_t)cnt) {
        fprintf(stderr, "%seasyboot: %s\r\n", verbose ? "\n" : "", lang[ERRSPC]); return 0;
    }
    for(clu = 3; clu < fat_nextcluster; clu++) {
        for(i = 0; i < cnt && clu + i < fat_nextcluster && !fat_fat32[clu + i]; i++);
        if(i == cnt) break;
    }
    if(clu == fat_nextcluster) fat_nextcluster += cnt;
    fat_freeclu -= cnt;
    for(i = 0; i < cnt; i++) fat_fat32[clu + i] = clu + i + 1;
    fat_fat32[clu + i - 1] = 0x0FFFFFFF;
    return clu;
}

/**
 * Look up or add a directory entry
 * parent: cluster where we need to add
 * add: 0=just check if entry exists, 1=update or add if doesn't exists, 2=update or add, allocate contiguous clusters
 * name: UTF-8 filename
 * isdir: 1 if the entry is a directory
 * *clu: returned cluster
 * flen: file's length
 */
int fat_dirent(uint32_t parent, int add, char *name, int isdir, uint32_t *clu, uint32_t flen)
{
    char ucase[256], fn[256];
    uint16_t uc2[32 * 13 + 1], *u;
    uint32_t i, n, size, last, cnt = 0;
    uint8_t *s, *dir, *ptr, c;

    *clu = -1U;

    /* convert name to uppercase */
    for(i = 0; i < 255 && name[i]; i++)
        ucase[i] = name[i] >= 'a' && name[i] <= 'z' ? name[i] - 'a' + 'A' : name[i];
    ucase[i] = 0;

    /* get the size */
    for(i = parent, size = fat_bpc;
        i && i < fat_nextcluster && fat_fat32[i] && fat_fat32[i] < 0xFFFFFF8; i = fat_fat32[i], size += fat_bpc);
    if(!(ptr = (uint8_t*)malloc(size + 4 * fat_bpc))) { fprintf(stderr, "%seasyboot: %s\r\n", verbose ? "\n" : "", lang[ERRMEM]); return 0; }
    memset(ptr, 0, size + 4 * fat_bpc);

    /* load the entries */
    i = last = parent; dir = ptr;
    do {
        if(!dev_read(f, (data_lba + i * bpb->spc) << 9, dir, fat_bpc)) {
            free(ptr); fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRRD], out); return 0;
        }
        last = i; i = fat_fat32[i]; dir += fat_bpc;
    } while(i && i < fat_nextcluster && i < 0xFFFFFF8);

    /* iterate on entries */
    for(dir = ptr; dir < ptr + size && *dir && (dir < ptr + 64 || *dir != '.'); dir += 32, cnt++) {
        memset(fn, 0, sizeof(fn));
        if(dir[0] == 0xE5) continue;
        if(dir[0xB] == 0xF) {
            /* this is an LFN block */
            memset(uc2, 0, sizeof(uc2));
            n = dir[0] & 0x1F;
            u = uc2 + (n - 1) * 13;
            while(n--) {
                for(i = 0; i < 5; i++) u[i] = dir[i*2+2] << 8 | dir[i*2+1];
                for(i = 0; i < 6; i++) u[i+5] = dir[i*2+0xF] << 8 | dir[i*2+0xE];
                u[11] = dir[0x1D] << 8 | dir[0x1C];
                u[12] = dir[0x1F] << 8 | dir[0x1E];
                u -= 13;
                dir += 32;
            }
            for(s = (uint8_t*)fn, u = uc2; *u && s < (uint8_t*)fn + 255; u++)
                if(*u < 0x80) { *s++ = *u >= 'a' && *u <= 'z' ? *u - 'a' + 'A' : *u; } else
                if(*u < 0x800) { *s++ = ((*u>>6)&0x1F)|0xC0; *s++ = (*u&0x3F)|0x80; } else
                { *s++ = ((*u>>12)&0x0F)|0xE0; *s++ = ((*u>>6)&0x3F)|0x80; *s++ = (*u&0x3F)|0x80; }
            *s = 0;
        } else {
            /* use 8.3 name otherwise */
            for(i = 0, s = (uint8_t*)fn; i < 8 && dir[i] != ' '; i++, s++)
                *s = dir[i] >= 'a' && dir[i] <= 'z' ? dir[i] - 'a' + 'A' : dir[i];
            if(dir[8] != ' ') {
                *s++ = '.';
                for(i = 8; i < 11 && dir[i] != ' '; i++, s++)
                    *s = dir[i] >= 'a' && dir[i] <= 'z' ? dir[i] - 'a' + 'A' : dir[i];
            }
            *s = 0;
        }
        if(!strcmp(fn, ucase)) {
            *clu = (dir[0x15] << 24) | (dir[0x14] << 16) | (dir[0x1B] << 8) | dir[0x1A];
            /* if it's a file, delete it */
            if(!dir[0xB] && add) {
                for(i = *clu; i && i < fat_nextcluster && i < 0xFFFFFF8;) {
                    n = fat_fat32[i]; fat_fat32[i] = 0; i = n; fat_freeclu++;
                }
                if(*clu) fat_freeclu--;
                if(!flen) {
                    *clu = 0; dir[0x1A] = dir[0x1B] = dir[0x14] = dir[0x15] = 0; fat_freeclu++;
                } else {
                    /* if we need contiguous allocation, update the cluster too */
                    if(add == 2) {
                        if(*clu && *clu < fat_nextcluster && *clu < 0xFFFFFF8) { fat_fat32[*clu] = 0; fat_freeclu++; }
                        *clu = fat_findclu((flen + fat_bpc - 1) / fat_bpc);
                        dir[0x1A] = *clu & 0xFF; dir[0x1B] = (*clu >> 8) & 0xFF;
                        dir[0x14] = (*clu >> 16) & 0xFF; dir[0x15] = (*clu >> 24) & 0xFF;
                    } else
                    if(*clu && *clu < fat_nextcluster && *clu < 0xFFFFFF8) fat_fat32[*clu] = 0xFFFFFFF;
                }
                dir[0x1C] = flen & 0xFF; dir[0x1D] = (flen >> 8) & 0xFF;
                dir[0x1E] = (flen >> 16) & 0xFF; dir[0x1F] = (flen >> 24) & 0xFF;
                /* write out modified directory entry */
                for(s = ptr, i = parent; s + fat_bpc < dir; s += fat_bpc) i = fat_fat32[i];
                if(!dev_write(f, ((data_lba + i * bpb->spc) << 9), s, fat_bpc)) {
                    fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRWR], out);
                }
            }
            break;
        }
    }

    /* if not found, add the directory entry */
    if(*clu == -1U && add) {
        for(i = n = 0; ucase[i] && ucase[i] != '.'; i++);
        if(ucase[i] == '.') for(; ucase[i + n + 1]; n++);
        if(i <= 8 && n <= 3 && (!strcmp(name, ucase) || (parent == bpb->rc && !strcmp(ucase, "EASYBOOT")))) {
            /* use the 8.3 name if possible */
            memset(fn, ' ', 11);
            for(i = n = 0; ucase[i]; i++) { if(ucase[i] == '.') n = 8; else fn[n++] = ucase[i]; }
            n = 0;
        } else {
            memset(uc2, 0, sizeof(uc2));
            for(i = 0, u = uc2, s = (uint8_t*)name; *s; i++, u++) {
                if((*s & 128) != 0) {
                    if((*s & 32) == 0) { *u = ((*s & 0x1F)<<6)|(*(s+1) & 0x3F); s += 2; } else
                    if((*s & 16) == 0) { *u = ((*s & 0xF)<<12)|((*(s+1) & 0x3F)<<6)|(*(s+2) & 0x3F); s += 3; }
                } else
                    *u = *s++;
            }
            i = (i + 12) / 13;
            /* don't convert "Microsoft" to "MICROS~1   ", that's patented... */
            sprintf(fn, "~%07xLFN", cnt++);
            for(c = 0, n = 0; n < 11; n++)
                c = (((c & 1) << 7) | ((c & 0xfe) >> 1)) + fn[n];
            u = uc2 + (i - 1) * 13;
            for(n = 0; i--; n += 32, u -= 13) {
                dir[n] = (!n ? 0x40 : 0) | (i + 1);
                dir[n + 0xB] = 0xF;
                dir[n + 0xD] = c;
                memcpy(dir + n + 1, (uint8_t*)u, 10);
                memcpy(dir + n + 14, (uint8_t*)u + 10, 12);
                memcpy(dir + n + 28, (uint8_t*)u + 22, 4);
            }
        }
        if(dir + n + 32 >= ptr + size) {
            /* add new cluster(s) to the parent directory */
            fat_fat32[last] = fat_findclu(((n + 1) * 32 + fat_bpc - 1) / fat_bpc);
            if(!fat_fat32[last]) { fat_fat32[last] = 0xFFFFFFF; free(ptr); return 0; }
        }
        /* add new cluster(s) for file contents */
        if(!isdir && !flen) *clu = 0;
        else *clu = fat_findclu(add == 2 ? (flen + fat_bpc - 1) / fat_bpc : 1);
        fat_dirent83(dir + n, fn, isdir ? 0x10 : 0, *clu, flen);
        n += 32;
        /* write out new directory entry */
        for(s = ptr, i = parent; s + fat_bpc < dir; s += fat_bpc) i = fat_fat32[i];
        if(!dev_write(f, ((data_lba + i * bpb->spc) << 9), s, fat_bpc)) {
            fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRWR], out);
        }
        /* long filenames might cross cluster border */
        if(dir + n > s + fat_bpc) {
            i = fat_fat32[i];
            if(!dev_write(f, ((data_lba + i * bpb->spc) << 9), s + fat_bpc, fat_bpc)) {
                fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRWR], out);
            }
        }
        /* if we have added a directory, then we must create . and .. entries in it as well */
        if(isdir) {
            memset(fn, ' ', 11); memset(dir, 0, fat_bpc);
            fn[0] = '.'; s = fat_dirent83(dir, fn, 0x10, *clu, 0);
            fn[1] = '.'; s = fat_dirent83(s, fn, 0x10, parent == bpb->rc ? 0 : parent, 0);
            if(!dev_write(f, ((data_lba + *clu * bpb->spc) << 9), dir, fat_bpc)) {
                fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRWR], out);
            }
        }
    }
    free(ptr);
    return (!*clu && !flen) || (*clu > 2 && *clu != bpb->rc && *clu < fat_nextcluster);
}

/**
 * Add a file to boot partition
 */
int fat_add(uint32_t parent, char *name, uint8_t *content, uint32_t size)
{
    uint32_t clu, i, nc;

    if(fat_dirent(parent, 1, name, 0, &clu, size) && content && size) {
        /* write out data in cluster sized blocks */
        for(i = 0; i < size; i += fat_bpc) {
            if(!dev_write(f, ((data_lba + clu * bpb->spc) << 9), content, fat_bpc)) {
                fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRWR], out);
                return 0;
            }
            if(i + fat_bpc >= size) break;
            content += fat_bpc;
            nc = fat_findclu(1);
            if(!nc) { fat_fat32[clu] = 0xFFFFFFF; return 0; }
            fat_fat32[clu] = nc;
            clu = nc;
        }
        fat_fat32[clu] = 0xFFFFFFF;
        /* if we have remaining data, copy it to an empty cluster and write that */
        if(i < size) {
            memcpy(cluster, content, size - i);
            memset(cluster + size - i, 0, fat_bpc - (size - i));
            if(!dev_write(f, ((data_lba + clu * bpb->spc) << 9), cluster, fat_bpc)) {
                fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRWR], out);
                return 0;
            }
        }
    }
    return 1;
}

/**
 * Remove directory contents recursively
 */
int fat_rmdir(uint32_t clu)
{
    uint32_t i, n, size;
    uint8_t *dir, *ptr;

    if(!clu || clu == bpb->rc) return 0;

    /* get the size */
    for(i = clu, size = fat_bpc;
        i && i < fat_nextcluster && fat_fat32[i] && fat_fat32[i] < 0xFFFFFF8; i = fat_fat32[i], size += fat_bpc);
    if(!(ptr = (uint8_t*)malloc(size))) { fprintf(stderr, "%seasyboot: %s\r\n", verbose ? "\n" : "", lang[ERRMEM]); return 0; }
    memset(ptr, 0, size);

    /* load the entries */
    i = clu; dir = ptr;
    do {
        if(!dev_read(f, (data_lba + i * bpb->spc) << 9, dir, fat_bpc)) {
            free(ptr); fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRRD], out); return 0;
        }
        i = fat_fat32[i]; dir += fat_bpc;
    } while(i && i < fat_nextcluster && i < 0xFFFFFF8);

    /* iterate on entries */
    for(dir = ptr + 64; dir < ptr + size && *dir; dir += 32) {
        if(dir[0] == 0xE5 || dir[0xB] == 0xF) continue;
        i = (dir[0x15] << 24) | (dir[0x14] << 16) | (dir[0x1B] << 8) | dir[0x1A];
        if(dir[0xB] == 0x10) fat_rmdir(i);
        for(; i && i < fat_nextcluster && i < 0xFFFFFF8;) {
            n = fat_fat32[i]; fat_fat32[i] = 0; i = n; fat_freeclu++;
        }
    }

    /* clear all directory entries, except "." and ".." */
    memset(ptr + 64, 0, fat_bpc - 64);
    if(!dev_write(f, ((data_lba + clu * bpb->spc) << 9), ptr, fat_bpc)) {
        fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRWR], out);
    }
    free(ptr);
    return 1;
}

/**
 * Print status
 */
void status(char *msg, char *par)
{
#ifndef __WIN32__
#define CL "\x1b[K"
#else
#define CL
#endif
    if(verbose) {
        if(!par) printf("\r%-78s" CL "\r", msg);
        else printf("\r%s: %-70s" CL "\r", msg, par);
        fflush(stdout);
    }
}

/**
 * Recursively parse a directory
 */
int parsedir(char *directory, int parent, int calcsize, uint32_t to)
{
#ifdef __WIN32__
    WIN32_FIND_DATAW ffd;
    HANDLE h;
    int i, j;
#else
    DIR *dir;
    struct dirent *ent;
    struct stat st;
    char *s;
#endif
    int r = 1;
    int64_t dirsize = 0;
    uint32_t clu = to;
    unsigned char *tmp;

    if(!parent) { parent = strlen(directory); skipbytes = parent + 1; strncpy(full, directory, sizeof(full)-1); }
#ifdef __WIN32__
    memset(&szFile, 0, sizeof(szFile));
    MultiByteToWideChar(CP_UTF8, 0, directory, -1, szFile, PATH_MAX);
    for(i = 0; szFile[i]; i++) if(szFile[i] == L'/') szFile[i] = L'\\';
    if(i && szFile[i - 1] != L'\\') szFile[i++] = L'\\';
    wcscpy_s(szFile + i, 255, L"*.*");
    h = FindFirstFileW(szFile, &ffd);
    if(h != INVALID_HANDLE_VALUE) {
        dirsize = 64;
        do {
            if(!wcscmp(ffd.cFileName, L".") || !wcscmp(ffd.cFileName, L"..")) continue;
            wcscpy_s(szFile + i, 255, ffd.cFileName);
            memset(full, 0, sizeof(full)); parent = 0;
            WideCharToMultiByte(CP_UTF8, 0, szFile, -1, full, sizeof(full) - 1, NULL, NULL);
            for(j = 0; full[j]; j++) if(full[j] == '\\') { full[j] = '/'; parent = j; }
            read_size = 0;
            /* no need to check filenames, we've converted it from UCS2 */
            if(calcsize) dirsize += (((wcslen(ffd.cFileName) + 12) / 13) + 1) * 32;
            else status(lang[ADD], full + skipbytes);
            if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if(!calcsize) fat_dirent(to, 1, full + parent + 1, 1, &clu, 0);
                r = parsedir(full, strlen(full), calcsize, clu);
            } else {
                tmp = readfileall(full, calcsize);
                if(calcsize) totsize += (read_size + 511) & ~511; else
                if(!tmp || memcmp(tmp, PLG_MAGIC, 4) || check_plugin(full, tmp, read_size))
                    r = fat_add(to, full + parent + 1, tmp, read_size);
                if(tmp) free(tmp);
            }
        } while(r && FindNextFileW(h, &ffd) != 0);
        FindClose(h);
    }
#else
    if((dir = opendir(directory)) != NULL) {
        dirsize = 64;
        while(r && (ent = readdir(dir)) != NULL) {
            if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
            strncpy(full + parent, "/", sizeof(full)-parent-1);
            strncat(full + parent, ent->d_name, sizeof(full)-parent-1);
            if(stat(full, &st)) continue;
            read_size = 0;
            if(calcsize) {
                /* we must check if all filenames can be encoded as UCS2 */
                for(s = ent->d_name; *s; s++)
                    if((*s & 128) != 0) {
                        if((*s & 32) == 0) s++; else
                        if((*s & 16) == 0) s += 2;
                        else { fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRENC], full); exit(1); }
                    }
                dirsize += (((s - ent->d_name + 12) / 13) + 1) * 32;
            } else status(lang[ADD], full + skipbytes);
            if(S_ISDIR(st.st_mode)) {
                if(!calcsize) fat_dirent(to, 1, full + parent + 1, 1, &clu, 0);
                r = parsedir(full, strlen(full), calcsize, clu);
            } else
            if(S_ISREG(st.st_mode)) {
                tmp = readfileall(full, calcsize);
                if(calcsize) totsize += (read_size + 511) & ~511; else
                if(!tmp || memcmp(tmp, PLG_MAGIC, 4) || check_plugin(full, tmp, read_size))
                    r = fat_add(to, full + parent + 1, tmp, read_size);
                if(tmp) free(tmp);
            }
        }
        closedir(dir);
    }
#endif
    totsize += (dirsize + 511) & ~511;
    if(parent + 1 == skipbytes) skipbytes = 0;
    return r;
}

/**
 * Open a device
 */
void *dev_open(char *file)
{
#ifdef __WIN32__
    HANDLE f = 0;
    int i;

    memset(&szFile, 0, sizeof(szFile));
    MultiByteToWideChar(CP_UTF8, 0, file, -1, szFile, PATH_MAX);
    for(i = 0; szFile[i]; i++) if(szFile[i] == L'/') szFile[i] = L'\\';
    f = CreateFileW(szFile, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING, NULL);
    if(f == INVALID_HANDLE_VALUE) f = NULL;
#else
    char *full, *mnt, *s;
    intptr_t f;
    int l;

    memset(&dev_st, 0, sizeof(dev_st));

    /* resolve relative paths and symlinks first */
    if(!(full = realpath(file, NULL))) full = file;
    /* check if the file is mounted by any chance */
    mnt = (char*)readfileall(
#ifdef __linux__
        "/proc/self/mountinfo"
#else
        "/etc/mtab"
#endif
        , 0);
    if(mnt) {
        for(s = mnt, l = strlen(full); *s; s++) {
            /* find beginning of a line */
            while(*s && (*s == '\r' || *s == '\n' || *s == ' ' || *s == '\t')) s++;
            if(!memcmp(s, full, l)) {
                fprintf(stderr, "easyboot: '%s' %s\r\n", file, lang[ERRMNT]);
                free(mnt); if(full != file) free(full);
                return 0;
            }
            /* skip to the end of the line */
            while(*s && *s != '\r' && *s != '\n') s++;
        }
        free(mnt);
    }
    /* not fopen because this might be an actual device file, in which case we need exclusive access and non-buffered IO */
    stat(full, &dev_st);
    f = open(full, O_RDWR | (S_ISBLK(dev_st.st_mode) ? O_SYNC | O_EXCL : 0));
    if(full != file) free(full);
    if(f < 3) f = 0;
#endif
    return (void *)f;
}

/**
 * Read from the device
 */
int dev_read(void *f, uint64_t off, void *buf, uint32_t size)
{
#ifdef __WIN32__
    DWORD r = size, t;
    LARGE_INTEGER pos;
    pos.QuadPart = off;
    SetFilePointerEx((HANDLE)f, pos, NULL, FILE_BEGIN);
    if(!ReadFile((HANDLE)f, buf, r, &t, NULL) || r != t) return 0;
#else
    if(lseek((intptr_t)f, (off_t)off, SEEK_SET) != (off_t)off || read((intptr_t)f, buf, (ssize_t)size) != (ssize_t)size) return 0;
#endif
    return 1;
}

/**
 * Write to the device
 */
int dev_write(void *f, uint64_t off, void *buf, uint32_t size)
{
#ifdef __WIN32__
    DWORD r = size, t;
    LARGE_INTEGER pos;
    pos.QuadPart = off;
#endif
    if(verbose > 2) printf("easyboot: dev_write offset %08lx %d bytes\n", (long unsigned int)off, size);
#ifdef __WIN32__
    SetFilePointerEx((HANDLE)f, pos, NULL, FILE_BEGIN);
    if(!WriteFile((HANDLE)f, buf, r, &t, NULL) || r != t) return 0;
#else
    if(lseek((intptr_t)f, (off_t)off, SEEK_SET) != (off_t)off || write((intptr_t)f, buf, (ssize_t)size) != (ssize_t)size) return 0;
#endif
    return 1;
}

/**
 * Close a device
 */
void dev_close(void *f)
{
#ifdef __WIN32__
    CloseHandle((HANDLE)f);
#else
    if(S_ISBLK(dev_st.st_mode)) fdatasync((intptr_t)f);
    close((intptr_t)f);
#endif
}

/**
 * Do some rigolous checking on plugins before we add them to the image
 */
int check_plugin(char *fn, uint8_t *buf, uint32_t size)
{
    plg_hdr_t *hdr = (plg_hdr_t*)buf;
    plg_id_t *ids;
    plg_rel_t *rels;
    uint32_t i, code;

    /* we do not just check plugin format validity here. We strictly check what our loaders actually do support */

    /* check header fields */
    if(!buf || size < sizeof(plg_hdr_t) + sizeof(plg_id_t) + sizeof(plg_rel_t) || memcmp(buf, PLG_MAGIC, 4) ||
        hdr->filesz != size || hdr->filesz > hdr->memsz || !hdr->codesz || !hdr->nrel || (!hdr->nid && hdr->type != PLG_T_TAG) ||
        (hdr->arch != EM_X86_64 && hdr->arch != EM_AARCH64) || hdr->type < PLG_T_FS || hdr->type > PLG_T_TAG) goto err;
    ids = (plg_id_t*)(buf + sizeof(plg_hdr_t));
    rels = (plg_rel_t*)((uint8_t*)ids + hdr->nid * sizeof(plg_id_t));
    code = sizeof(plg_hdr_t) + hdr->nid * sizeof(plg_id_t) + hdr->nrel * sizeof(plg_rel_t);
    if(code + hdr->codesz + hdr->rosz > hdr->filesz) goto err;

    /* check entry point */
    if(hdr->entry < code || hdr->entry >= code + hdr->codesz) goto err;

    /* check identifier match records */
    for(i = 0; i < hdr->nid; i++, ids++)
        if(ids->type < PLG_M_CONST || ids->type > PLG_M_SRCH) goto err;

    /* check relocation records */
    for(i = 0; i < hdr->nrel; i++, rels++)
        if(rels->offs < code || rels->offs >= hdr->filesz || PLG_R_SYM(rels->type) >= PLG_NUMSYM ||
          (hdr->arch == EM_X86_64 && PLG_R_IMSK(rels->type) > 0) ||
          (hdr->arch == EM_AARCH64 && PLG_R_IMSK(rels->type) > 3)) goto err;

    /* if this is a file system plugin, store its identifier match records */
    if(hdr->type == PLG_T_FS) {
        plgs = (uint8_t**)realloc(plgs, (num_plgs + 1) * sizeof(uint8_t*));
        if(plgs) {
            i = sizeof(plg_hdr_t) + hdr->nid * sizeof(plg_id_t);
            plgs[num_plgs] = (uint8_t*)malloc(i);
            if(plgs[num_plgs]) { memcpy(plgs[num_plgs], buf, i); num_plgs++; }
        } else num_plgs = 0;
    }
    return 1;

err:fprintf(stderr, "%seasyboot: %s '%s'\r\n", verbose ? "\n" : "", lang[ERRPLG], fn);
    return 0;
}

/**
 * Free plugins
 */
void free_plugins(void)
{
    int i;

    if(!plgs || !num_plgs) return;
    for(i = 0; i < num_plgs; i++)
        if(plgs[i]) free(plgs[i]);
    free(plgs);
    plgs = NULL; num_plgs = 0;
}

/**
 * Check if plugin can handle this buffer
 */
int check_magic(uint8_t *buf, uint8_t *plg)
{
    plg_hdr_t *hdr = (plg_hdr_t*)plg;
    plg_id_t *ids = (plg_id_t*)(plg + sizeof(plg_hdr_t));
    uint32_t i, offs = 0, o;

    if(!buf || !plg || memcmp(plg, PLG_MAGIC, 4) || !hdr->nid) return 0;
    for(i = 0; i < hdr->nid; i++, ids++) {
        o = ids->offs + offs;
        switch(ids->type) {
            case PLG_M_BYTE: o = o > 65535 ? 0 : buf[o]; break;
            case PLG_M_WORD: o = o > 65534 ? 0 : *((uint16_t*)(buf + o)); break;
            case PLG_M_DWORD: o = o > 65532 ? 0 : *((uint32_t*)(buf + o)); break;
            case PLG_M_BADD: o = (o > 65535 ? 0 : buf[o]) + offs; break;
            case PLG_M_WADD: o = (o > 65534 ? 0 : *((uint16_t*)(buf + o))) + offs; break;
            case PLG_M_DADD: o = (o > 65532 ? 0 : *((uint32_t*)(buf + o))) + offs; break;
        }
        if(!ids->size) offs = o;
        else if(ids->type == PLG_M_SRCH) {
            for(o = offs; o + ids->size < 65536; o += (ids->offs ? ids->offs : 1))
                if(!memcmp(buf + o, ids->magic, ids->size)) { offs = o; break; }
            if(o + ids->size >= 65536) return 0;
        } else if(o + ids->size > 65535 || memcmp(buf + o, ids->magic, ids->size)) return 0;
    }
    return 1;
}

/**
 * Check for hex color
 */
int check_hexcolor(char *str, int line)
{
    int i = 0;
    char *h = str;

    if(*str == '#') {
        for(str++, i = 0; i < 6 &&
            ((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'f') || (str[i] >= 'A' && str[i] <= 'F')); i++);
    }
    if(i != 6) {
        for(;*str && *str != ' ' && *str != '\r' && *str != '\n';str++);
        *str = 0; fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[BADCOLOR], h);
        return 0;
    }
    return 1;
}

/**
 * Check the configuration file for validity and correctness
 */
int check_config(char *buf, uint32_t size)
{
    uint8_t *tmp;
    int line = 1, ma = 0, l, i, w = 0, h, b, de = 0, dl = 0, me = 0, ml = 0, hk = 0, hr = 0, hb = 0, hf = 0, hm = 0;
    char *end, *s, *e, *a, *n, *d, fn[PATH_MAX];

    if(!in || !buf || size < 1) return 1;

    l = strlen(in); if(l && full[l - 1] != '/') l++;
    memcpy(fn, full, l);
    memset(menupart, 0, sizeof(menupart));

    /* check if we have a UNICODE font. If not, then we must check that labels are ASCII-only */
    memcpy(fn + l, "easyboot/font.sfn", 17);
    (void)readfileall(fn, 1);
    hf = read_size > 32;

    for(s = buf, end = buf + size; s < end && *s; s = e) {
        /* find beginning of a line */
        while(s < end && *s && (*s == '\r' || *s == '\n' || *s == ' ' || *s == '\t' || *s == '{' || *s == '}')) { if(*s == '\n') { line++; } s++; }
        for(a = s; a < end && *a && *a != ' ' && *a != '\r' && *a != '\n'; a++);
        for(e = a; e < end && *e && *e != '{' && *e != '}' && *e != '\r' && *e != '\n'; e++);
        while(a < e && *a == ' ') a++;
        for(n = a; n < e && *n && *n != ' ' && *n != '\r' && *n != '\n'; n++)
            if(*n == '\\' && n[1] == ' ') n++;
        /* 's' points to the start of the command,
         * 'a' to the first argument,
         * 'n' to the end of the first argument (next argument almost), and
         * 'e' to the end of the line */
        if(a >= e) { s = e; continue; }
        if(!memcmp(s, "verbose", 7)) {
            if(me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTINME]); return 0; }
            i = atoi(a);
            if(i < 0 || i > 3) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[BADVERBOSE]); return 0; }
        } else
        if(!memcmp(s, "framebuffer", 11)) {
            if(me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTINME]); return 0; }
            if(w) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOMULTI]); return 0; }
            w = atoi(a); a = n;
            while(a < e && *a == ' ') a++;
            h = atoi(a);
            for(; a < e && *a && *a != ' ' && *a != '\r' && *a != '\n'; a++);
            while(a < e && *a == ' ') a++;
            b = atoi(a);
            if(w < 320 || w > 65536 || h < 200 || h > 65536 || b < 15 || b > 32) {
                fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[BADFB]); return 0;
            }
            for(; a < e && *a && *a != ' ' && *a != '\r' && *a != '\n'; a++);
            while(a < e && *a == ' ') a++;
            if(*a == '#') {
                if(!check_hexcolor(a, line)) return 0;
                for(; a < e && *a && *a != ' ' && *a != '\r' && *a != '\n'; a++);
                while(a < e && *a == ' ') a++;
                if(*a == '#') {
                    if(!check_hexcolor(a, line)) return 0;
                    for(; a < e && *a && *a != ' ' && *a != '\r' && *a != '\n'; a++);
                    while(a < e && *a == ' ') a++;
                    if(*a == '#' && !check_hexcolor(a, line)) return 0;
                }
            }
        } else
        if(!memcmp(s, "default", 7)) {
            if(me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTINME]); return 0; }
            if(de) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOMULTI]); return 0; }
            de = atoi(a); while(n < e && *n == ' ') n++;
            i = atoi(n); dl = line;
            if(de < 1 || de > 9 || i < 1 || i > 60000) {
                fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[BADDEFAULT]); return 0;
            }
        } else
        if(!memcmp(s, "menualign", 9)) {
            if(me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTINME]); return 0; }
            if(ma) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOMULTI]); return 0; }
            ma = 1;
            if(memcmp(a, "vertical", 8) && memcmp(a, "horizontal", 10)) {
                fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[BADALIGN]); return 0;
            }
        } else
        if(!memcmp(s, "menuentry", 9)) {
            if(me && !hk) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], ml, lang[NOKERNEL]); return 0; }
            ml = line; hk = hr = hb = hm = 0; me++;
            if(me > 9) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[TOOMANY]); return 0; }
            memcpy(fn + l, "easyboot/", 9); memcpy(fn + l + 9, a, n - a); memcpy(fn + l + 9 + (n - a), ".tga", 5);
            if(n - a > 64) { fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[TOOLONG], fn + l); return 0; }
            if(!(tmp = readfileall(fn, 0))) {
                fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[NOTFND], fn);
                return 0;
            }
            if(tmp[0] || tmp[1] != 1 || tmp[2] != 9 || tmp[3] || tmp[4] || (tmp[7] != 24 && tmp[7] != 32)) {
                free(tmp); fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[BADTGA], fn); return 0;
            }
            if(tmp[12] != 64 || tmp[13] || tmp[14] != 64 || tmp[15]) {
                free(tmp); fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[BADICON], fn); return 0;
            }
            free(tmp);
            /* if we don't have a UNICODE font, check label */
            if(!hf) {
                while(n < e && *n == ' ') n++;
                for(a = n; a < e; a++)
                    if(*a < 32 || *a > 126) {
                        *e = 0; fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[BADLABEL], n); return 0;
                    }
            }
        } else
        if(!memcmp(s, "partition", 9)) {
            if(!me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTOUTME]); return 0; }
            if(hr) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOMULTI]); return 0; }
            hr = 1;
            for(s = a, i = 0; s < n; s++) {
                if((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')) i++;
                else if(*s != '-' || (i != 8 && i != 12 && i != 16)) break;
            }
            if(s != n || i != 32) {
                *n = 0; fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[BADUUID], a); return 0;
            }
            getguid(a, &menupart[me - 1]);
        } else
        if(!memcmp(s, "kernel", 6)) {
            if(!me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTOUTME]); return 0; }
            if(hk) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOMULTI]); return 0; }
            hk = 1;
            /* look for root partition argument */
            if(!hr)
                for(s = n; s < e - 5; s++)
                    if(!memcmp(s, "root=", 5)) {
                        for(s += 5, d = s; d < e && *d && *d != ' ' && *d != '='; d++);
                        if(*d == '=') s = d + 1;
                        if(s + 35 <= e && s[8] == '-' && s[13] == '-' && s[18] == '-') { hr = 1; getguid(a, &menupart[me - 1]); }
                        break;
                    }
            if(*a == '/') a++;
            for(d = fn + l, s = a; s < n; s++) { if(*s == '\\' && s[1] == ' ') { s++; } *d++ = *s; } *d = 0;
            /* only check the kernel if the file's on the boot partition */
            if(!hr) {
                (void)readfileall(fn, 1);
                if(!read_size) { fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[NOTFND], fn); return 0; }
            }
            if(d - (fn + l) > 255) { fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[TOOLONG], fn + l); return 0; }
        } else
        if(!memcmp(s, "module", 6)) {
            if(!me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTOUTME]); return 0; }
            if(*a == '/') a++;
            for(d = fn + l, s = a; s < n; s++) { if(*s == '\\' && s[1] == ' ') { s++; } *d++ = *s; } *d = 0;
            /* only check the module if the file's on the boot partition */
            if(!hr) {
                (void)readfileall(fn, 1);
                if(!read_size) { fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[NOTFND], fn); return 0; }
            }
            if(d - (fn + l) > 255) { fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[TOOLONG], fn + l); return 0; }
        } else
        if(!memcmp(s, "bootsplash", 10)) {
            if(!me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTOUTME]); return 0; }
            if(hb) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOMULTI]); return 0; }
            hb = 1;
            if(*a == '#') {
                if(!check_hexcolor(a, line)) return 0;
                while(n < e && *n == ' ') n++;
                a = n;
            }
            if(a < e) {
                if(*a == '/') a++;
                for(d = fn + l, s = a; s < e; s++) { if(*s == '\\' && s[1] == ' ') { s++; } *d++ = *s; } *d = 0;
                if(!(tmp = readfileall(fn, 0))) {
                    fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[NOTFND], fn); return 0;
                }
                if(tmp[0] || tmp[1] != 1 || tmp[2] != 9 || tmp[3] || tmp[4] || (tmp[7] != 24 && tmp[7] != 32)) {
                    free(tmp); fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[BADTGA], fn); return 0;
                }
                free(tmp);
                if(d - (fn + l) > 255) { fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[TOOLONG], fn + l); return 0; }
            }
        } else
        if(!memcmp(s, "multicore", 9)) {
            if(!me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOTOUTME]); return 0; }
            if(hm) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOMULTI]); return 0; }
            hm = 1;
            for(read_size = 0, i = 0; read_size < (int)sizeof(plg_hdr_t) && archs[i]; i++) {
                sprintf(fn + l, "easyboot/smp_%s.plg", archs[i]); (void)readfileall(fn, 1);
            }
            if(read_size < (int)sizeof(plg_hdr_t)) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOSMPPLG]); return 0; }
        } else
        if(*s && *s != '#' && *s != '\r' && *s != '\n') {
            while(a > s && a[-1] == ' ') { a--; } *a = 0;
            fprintf(stderr, "easyboot: %s (%s:%d) %s '%s'\r\n", full, lang[LINE], line, lang[UNKNOWN], s);
            return 0;
        }
    }
    if(!me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], line, lang[NOMENU]); return 0; }
    if(!hk) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], ml, lang[NOKERNEL]); return 0; }
    if(de && de > me) { fprintf(stderr, "easyboot: %s (%s:%d) %s\r\n", full, lang[LINE], dl, lang[BADDEFAULT]); return 0; }

    return 1;
}

/**
 * Check partitions
 */
void check_partitions(void)
{
    guid_t empty = { 0 };
    uint64_t lba;
    uint8_t buf[65536*2];
    /*esp_bpb_t *bpb2 = (esp_bpb_t*)buf;*/
    int i, j;

    /* failsafe */
    if(gpt_spe < 2 || !img) return;

    for(j = 0; j < 9; j++)
        if(memcmp(&menupart[j], &empty, sizeof(guid_t))) {
            for(lba = 0, i = 0; i + gpt_spe <= GPT_SECTORS * 512 &&
              memcmp(&((gpt_entry_t*)(img + 512 + i))->PartitionTypeGUID, &empty, sizeof(guid_t)); i += gpt_spe)
                if(!memcmp(&((gpt_entry_t*)(img + 512 + i))->UniquePartitionGUID, &menupart[j], sizeof(guid_t))) {
                    lba = ((gpt_entry_t*)(img + 512 + i))->StartingLBA;
                    if(!lba || !dev_read(f, lba << 9, buf, 65536*2)) {
                        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRRD], out); lba = 0;
                    }
                    break;
                }
            /* we just report this, isn't an error, the specified partition could be on another disk */
            if(!lba) {
                fprintf(stderr, "easyboot: menuentry %u: %s %s\r\n", j + 1, lang[NOPART], prtguid(&menupart[j]));
                continue;
            }
            /* see if we can find a file system plugin that handles this */
            for(i = 0; i < num_plgs && !check_magic(buf, plgs[i]) && !check_magic(buf + 65536, plgs[i]); i++);
            /* no plugin, but maybe it's a FAT32 partition? */
            if(i >= num_plgs /*&& (bpb2->fst2[0] != 'F' || bpb2->fst2[1] != 'A' || bpb2->fst2[2] != 'T' ||
              bpb2->bps != 512 || !bpb2->spc || bpb2->spf16 || !bpb2->spf32 || buf[0x32] > 127)*/)
                fprintf(stderr, "easyboot: menuentry %u: %s %s\r\n", j + 1, lang[NOFSPLG], prtguid(&menupart[j]));
        }
}

/**
 * Detect language and get the dictionary for the user's language
 */
void detlang(void)
{
    int i = 0;
    char *lng = getenv("LANG");
#ifdef __WIN32__
    int lid;
    if(!lng) {
        lid = GetUserDefaultLangID(); /* GetUserDefaultUILanguage(); */
        /* see https://docs.microsoft.com/en-us/windows/win32/intl/language-identifier-constants-and-strings */
        switch(lid & 0xFF) {
            case 0x01: lng = "ar"; break;   case 0x02: lng = "bg"; break;   case 0x03: lng = "ca"; break;
            case 0x04: lng = "zh"; break;   case 0x05: lng = "cs"; break;   case 0x06: lng = "da"; break;
            case 0x07: lng = "de"; break;   case 0x08: lng = "el"; break;   case 0x0A: lng = "es"; break;
            case 0x0B: lng = "fi"; break;   case 0x0C: lng = "fr"; break;   case 0x0D: lng = "he"; break;
            case 0x0E: lng = "hu"; break;   case 0x0F: lng = "is"; break;   case 0x10: lng = "it"; break;
            case 0x11: lng = "ja"; break;   case 0x12: lng = "ko"; break;   case 0x13: lng = "nl"; break;
            case 0x14: lng = "no"; break;   case 0x15: lng = "pl"; break;   case 0x16: lng = "pt"; break;
            case 0x17: lng = "rm"; break;   case 0x18: lng = "ro"; break;   case 0x19: lng = "ru"; break;
            case 0x1A: lng = "hr"; break;   case 0x1B: lng = "sk"; break;   case 0x1C: lng = "sq"; break;
            case 0x1D: lng = "sv"; break;   case 0x1E: lng = "th"; break;   case 0x1F: lng = "tr"; break;
            case 0x20: lng = "ur"; break;   case 0x21: lng = "id"; break;   case 0x22: lng = "uk"; break;
            case 0x23: lng = "be"; break;   case 0x24: lng = "sl"; break;   case 0x25: lng = "et"; break;
            case 0x26: lng = "lv"; break;   case 0x27: lng = "lt"; break;   case 0x29: lng = "fa"; break;
            case 0x2A: lng = "vi"; break;   case 0x2B: lng = "hy"; break;   case 0x2D: lng = "bq"; break;
            case 0x2F: lng = "mk"; break;   case 0x36: lng = "af"; break;   case 0x37: lng = "ka"; break;
            case 0x38: lng = "fo"; break;   case 0x39: lng = "hi"; break;   case 0x3A: lng = "mt"; break;
            case 0x3C: lng = "gd"; break;   case 0x3E: lng = "ms"; break;   case 0x3F: lng = "kk"; break;
            case 0x40: lng = "ky"; break;   case 0x45: lng = "bn"; break;   case 0x47: lng = "gu"; break;
            case 0x4D: lng = "as"; break;   case 0x4E: lng = "mr"; break;   case 0x4F: lng = "sa"; break;
            case 0x53: lng = "kh"; break;   case 0x54: lng = "lo"; break;   case 0x56: lng = "gl"; break;
            case 0x5E: lng = "am"; break;   case 0x62: lng = "fy"; break;   case 0x68: lng = "ha"; break;
            case 0x6D: lng = "ba"; break;   case 0x6E: lng = "lb"; break;   case 0x6F: lng = "kl"; break;
            case 0x7E: lng = "br"; break;   case 0x92: lng = "ku"; break;   case 0x09: default: lng = "en"; break;
        }
    }
#endif
    if(lng && *lng) for(i = 0; i < NUMLANGS; i++) if(lng[0] == dict[i][0][0] && lng[1] == dict[i][0][1]) break;
    if(i >= NUMLANGS) i = 0;
    lang = &dict[i][1];
}

/**
 * Print usage
 */
void usage(char *cmd)
{
    char *tmp = strrchr(cmd,
#ifdef __WIN32__
    '\\'
#else
    '/'
#endif
    );
    if(!tmp) tmp = cmd; else tmp++;

    printf("Easyboot %s v%s, Copyright (c) 2023 bzt, GPLv3+\r\nhttps://gitlab.com/bztsrc/easyboot\r\n\r\n",
        lang[DISKTOOL], ebver);
    printf(" %s [-v|-vv] [-s <mb>] [-b <mb>] [-u <guid>] [-p <t> <u> <i>] [-e]\r\n    [-c] <indir> <outfile>\r\n\r\n", tmp);
    printf("  -v, -vv         %s\r\n", lang[VERBOSITY]);
    printf("  -s <mb>         %s\r\n", lang[DISKSIZE]);
    printf("  -b <mb>         %s\r\n", lang[PARTSIZE]);
    printf("  -u <guid>       %s\r\n", lang[PARTUUID]);
    printf("  -p <t> <u> <i>  %s\r\n", lang[ROOTPART]);
    printf("  -e              %s\r\n", lang[ELTORITO]);
    printf("  -c              %s\r\n", lang[CREATE]);
    printf("  <indir>         %s\r\n", lang[INDIR]);
    printf("  <outfile>       %s\r\n", lang[OUTFILE]);
    printf("\r\n %s\r\n %s: %08x (x86), %08x (rpi)\r\n\r\n", lang[CFGFILE], lang[LDRVER], ts_x86, ts_rpi);
    exit(1);
}

/**
 * Main function
 */
int main(int argc, char **argv)
{
    time_t t;
    int64_t siz;
    uint32_t clu;
    int i, j, docalc = 1, eltorito = 0, create = 0;
    guid_t espGuid = EFI_PART_TYPE_EFI_SYSTEM_PART_GUID;
    uint8_t *ptr = loader_x86_efi + ((mz_hdr*)loader_x86_efi)->peaddr;
    pe_sec *sec = (pe_sec*)(ptr + ((pe_hdr*)ptr)->opt_hdr_size + 24);
    uint32_t k, o, sectop = 0;
    char *cfg;

    detlang();

    /* get random GUIDs */
    t = time(NULL); fat_ts = gmtime(&t); srand(t);
    i = rand(); memcpy(&((uint8_t*)&dguid)[0], &i, 4); i = rand(); memcpy(&((uint8_t*)&dguid)[4], &i, 4);
    i = rand(); memcpy(&((uint8_t*)&dguid)[8], &i, 4); i = rand(); memcpy(&((uint8_t*)&dguid)[12], &i, 4);
    i = rand(); memcpy(&((uint8_t*)&pguid)[0], &i, 4); i = rand(); memcpy(&((uint8_t*)&pguid)[4], &i, 4);
    i = rand(); memcpy(&((uint8_t*)&pguid)[8], &i, 4); i = rand(); memcpy(&((uint8_t*)&pguid)[12], &i, 4);

    /* get loader versions */
    ts_x86 = crc32_calc(loader_x86_efi, sizeof(loader_x86_efi));
    ts_rpi = crc32_calc(loader_rpi_bin, sizeof(loader_rpi_bin));

    /* parse command line */
    for(i = 1; i < argc && argv[i]; i++)
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 's': disksize = atoi(argv[++i]); if(docalc) { partsize = disksize - 2; } docalc = 0; break;
                case 'b': partsize = atoi(argv[++i]); docalc = 0; break;
                case 'u': getguid(argv[++i], &pguid); break;
                case 'p':
                    if(i + 3 >= argc || nump + 1 >= (int)(sizeof(ptguid)/sizeof(ptguid[0]))) usage(argv[0]);
                    getguid(argv[++i], &ptguid[nump]); getguid(argv[++i], &puguid[nump]); pfile[nump++] = argv[i];
                break;
                default:
                    for(j = 1; argv[i][j]; j++)
                        switch(argv[i][j]) {
                            case 'v': verbose++; break;
                            case 'e': eltorito = 1; break;
                            case 'c': create = 1; break;
                            default: usage(argv[0]); break;
                        }
                break;
            }
        } else
        if(!in) in = argv[i]; else
        if(!out) out = argv[i]; else
            usage(argv[0]);
    if(!out) usage(argv[0]);

    /* self-integrity check */
    for(i = 0, sectop = 0; i < (int)sizeof(loader_rpi_bin) - 128; i += 16)
        if(!memcmp(loader_rpi_bin + i, "kernel\0\0\0\0\0\0\0\0\0", 16)) {
            sectop = *((uint32_t*)(loader_rpi_bin + i + 128 + 4));
            if(verbose > 2) printf("loader_rpi _bss_start %08x _bss_end %08x\r\n", *((uint32_t*)(loader_rpi_bin + i + 128)),
                sectop);
            break;
        }
    if(i >= (int)sizeof(loader_rpi_bin) - 128 || !sectop || sectop >= 0x20000) {
        fprintf(stderr, "easyboot: invalid inlined loader_rpi???\r\n");
        return 1;
    }
    for(i = j = 0, sectop = 0; i < ((pe_hdr*)ptr)->sections; i++, sec++) {
        if(!strcmp(sec->name, ".rdata")) {
            for(k = 0; k < sec->rsiz && strcmp((char*)loader_x86_efi + sec->raddr + k, "kernel"); k++);
            j = k < sec->rsiz;
        }
        o = sec->vaddr + 0x8000 - ((pe_hdr*)ptr)->code_base + sec->vsiz;
        if(o > sectop) sectop = o;
        if(verbose > 2) printf("loader_x86 section '%-8s' %08x %5u rel %08x top %08x\r\n", sec->name, sec->vaddr, sec->vsiz,
            o - sec->vsiz, sectop);
    }
    /* max size comes from boot_x86.asm line 71 (max BIOS can handle), sectop is the same plus the bss size */
    if(sizeof(loader_x86_efi) > 127 * 512 || !sectop || sectop >= 0x20000 || !j) {
        fprintf(stderr, "easyboot: invalid inlined loader_x86???\r\n");
        return 1;
    }

    /* calculate the required minimum partition size */
    totsize = 0;
    parsedir(in, 0, 1, 0);
    skipbytes = 0;
    if(totsize < 1) {
        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRDIR], in);
        return 1;
    }
    /* add FAT table size and round up to Megabytes */
    totsize += (8 + (((totsize / 512) * 4) / 512) * 2) * 512;
    totsize = (totsize + 1024 * 1024 - 1) & ~(1024 * 1024 - 1);
    if(totsize / 1024 > 2047 * 1024) {
        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRMUCH], in);
        return 1;
    }

    /* check configuration; even without -vv we check if it exists because menu.cfg is mandatory */
    strncpy(full, in, sizeof(full) - 1); strncat(full, "/easyboot/menu.cfg", sizeof(full) - 1);
    cfg = (char*)readfileall(full, 0);
    if(!cfg) { fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRCFG], full); return 1; }
    if(verbose > 1 && !check_config(cfg, read_size)) { free(cfg); return 1; }
    free(cfg);

    /* create image if it doesn't exists */
    if(create || !(f = dev_open(out))) {
        /* check partition and disk sizes */
        if(partsize == 2048) partsize = 2047;
        if(partsize < 33 || partsize > 2047) { fprintf(stderr, "easyboot: %s\r\n", lang[ERRSIZ]); return 1; }
        disksize *= 2048 * 512;
        partsize *= 2048 * 512;
        /* expand required minimum partition size (only if neither "-p" nor "-d" given) */
        if(docalc && totsize > partsize) {
            partsize = totsize;
            if(verbose) printf("easyboot: %s %lu Mb\r\n", lang[EXPPART], (unsigned long int)(partsize / 1024L / 1024L));
        }
        siz = partsize + (FIRST_PARTITION + GPT_SECTORS + 1) * 512;
        for(i = 0; i < nump; i++) {
            if(pfile[i]) { readfileall(pfile[i], 1); psize[i] = read_size; }
            siz += (psize[i] + 1024*1024-1) & ~(1024*1024-1);
        }
        if(disksize < siz) {
            disksize = siz;
            if(verbose) printf("easyboot: %s %lu Mb\r\n", lang[EXPDISK], (unsigned long int)(disksize / 1024 / 1024));
        }
#ifdef __WIN32__
        MultiByteToWideChar(CP_UTF8, 0, out, -1, szFile, PATH_MAX);
        for(i = 0; szFile[i]; i++) if(szFile[i] == L'/') szFile[i] = L'\\';
        f = CreateFileW(szFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#else
        umask(0111);
        f = (void*)(intptr_t)open(out, O_WRONLY | O_CREAT | O_TRUNC, 0666);
#endif
        if(!f) { fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRWR], out); return 2; }

        imgsize = ((FIRST_PARTITION + 2) << 9) + partsize;
        if(!(img = (uint8_t*)malloc(imgsize))) { fprintf(stderr, "easyboot: %s\r\n", lang[ERRMEM]); return 1; }
        /* generate disk image */
        status(lang[FORMAT], NULL);
        memset(img, 0, imgsize); fs_base = img + FIRST_PARTITION * 512;
        gpt_create();
        if(eltorito) etbc_create();
        fat_format();
        if(!dev_write(f, 0, img, imgsize) || !dev_write(f, disksize - sizeof(img_tail), img_tail, sizeof(img_tail))) {
            dev_close(f); free(img); fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRWR], out); return 2;
        }
        free(img); img = fs_base = NULL;
        /* write optional extra partition(s) */
        for(siz = (FIRST_PARTITION * 512 + partsize + 1024*1024-1) & ~(1024*1024-1), i = 0; i < nump; i++)
            if(pfile[i] && (pdata = (uint8_t*)readfileall(pfile[i], 0))) {
                if(!dev_write(f, siz, pdata, psize[i])) {
                    dev_close(f); free(pdata); fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRWR], out); return 2;
                }
                free(pdata); pdata = NULL;
                siz += (psize[i] + 1024*1024-1) & ~(1024*1024-1);
            }
        dev_close(f);
        if(!(f = dev_open(out))) { fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRRD], out); return 2; }
    }

    if(!(img = (uint8_t*)malloc((GPT_SECTORS + 1) << 9))) { fprintf(stderr, "easyboot: %s\r\n", lang[ERRMEM]); goto err; }
    if(!(fs_base = (uint8_t*)malloc(1024*1024))) { fprintf(stderr, "easyboot: %s\r\n", lang[ERRMEM]); goto err; }
    bpb = (esp_bpb_t*)fs_base;

    /* locate EFI System Partition */
    if(!dev_read(f, 0, img, 2 * 512)) {
        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRRD], out); goto err;
    }
    if(!memcmp(img + 512, EFI_PTAB_HEADER_ID, 8)) {
        /* found GPT */
        if(verbose > 1) printf("\r\nGUID Partitioning Table LBA %lu\r\n",
            (long unsigned int)((gpt_header_t*)(img + 512))->PartitionEntryLBA);
        memcpy(&dguid, &((gpt_header_t*)(img + 512))->DiskGUID, sizeof(guid_t));
        gpt_spe = j = ((gpt_header_t*)(img + 512))->SizeOfPartitionEntry;
        /* look for ESP in the first GPT_SECTORS sectors only. We never generate more, and should be the very first entry anyway */
        if(!dev_read(f, ((gpt_header_t*)(img + 512))->PartitionEntryLBA << 9, img + 512, GPT_SECTORS * 512)) goto noesp;
        for(i = 0; i + j <= GPT_SECTORS * 512; i += j)
            if(!memcmp(&((gpt_entry_t*)(img + 512 + i))->PartitionTypeGUID, &espGuid, sizeof(guid_t))) {
                boot_lba = ((gpt_entry_t*)(img + 512 + i))->StartingLBA;
                partsize = (((gpt_entry_t*)(img + 512 + i))->EndingLBA - ((gpt_entry_t*)(img + 512 + i))->StartingLBA + 1) << 9;
                memcpy(&pguid, &((gpt_entry_t*)(img + 512 + i))->UniquePartitionGUID, sizeof(guid_t));
                /* create hybrid MBR Partitioning scheme */
                if(img[0x1C2] != 0xC || *((uint32_t*)(img+0x1C0+6)) != boot_lba || *((uint32_t*)(img+0x1C0+10)) != partsize >> 9) {
                    img[0x1C2] = 0xC;
                    *((uint32_t*)(img+0x1C0+6)) = boot_lba;
                    *((uint32_t*)(img+0x1C0+10)) = partsize >> 9;
                    chs(boot_lba, img+0x1BE);
                    chs(boot_lba+partsize/512-1, img+0x1C2);
                }
                img[0x1BE] = 0x80; img[0x1CE] = img[0x1DE] = img[0x1EE] = 0;
                if(!*((uint32_t*)(img+0x1B8))) memcpy(img+0x1B8, &dguid.Data1, 4);
                break;
            }
    } else
    if(img[510] == 0x55 && img[511] == 0xAA) {
        /* fallback to MBR partitioning scheme */
        if(verbose > 1) printf("\r\nPMBR DOS Partitioning Table LBA 0\r\n");
        for(i = 0x1c0; i < 510; i += 16)
            if(img[i - 2] == 0x80/*active*/ && (img[i + 2] == 0xC/*FAT32*/ || img[i + 2] == 0xEF/*ESP*/)) {
                boot_lba = (uint64_t)(*((uint32_t*)(img + i + 6)));
                partsize = (uint64_t)*((uint32_t*)(img + i + 10)) << 9;
                break;
            }
        memset(&pguid, 0, sizeof(guid_t));
    }
    if(!boot_lba || !dev_read(f, boot_lba << 9, fs_base, 65536) || bpb->bps != 512 || !bpb->spc || bpb->spf16 || !bpb->spf32 ||
      fs_base[0x32] > 127) {
noesp:  if(verbose) printf("BPB bps %u spc %u spf16 %u spf32 %u BPB copy LBA %u\r\n", bpb->bps, bpb->spc, bpb->spf16, bpb->spf32,
            fs_base[0x32]);
        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRPART], out); goto err;
    }
    if(verbose > 1) {
        printf("Boot partition LBA %lu, UUID: %s\r\n", (long unsigned int)boot_lba, prtguid(&pguid));
        printf("BPB bps %u spc %u spf16 %u spf32 %u BPB copy LBA %u\r\n", bpb->bps, bpb->spc, bpb->spf16, bpb->spf32, fs_base[0x32]);
    }
    /* add loaders' size, 2 for the directories and 1 plus per loader because they are not multiple of cluster size */
    if((uint64_t)totsize + 4 * fat_bpc + sizeof(loader_x86_efi) + sizeof(loader_rpi_bin) > (uint64_t)partsize) {
        fprintf(stderr, "easyboot: %s\r\n", lang[ERRSPC]); goto err;
    }

    if(!(fat_fat32 = (uint32_t*)malloc(bpb->spf32 << 9))) { fprintf(stderr, "easyboot: %s\r\n", lang[ERRMEM]); goto err; }
    fat_nextcluster = *((uint32_t*)(fs_base + 0x3EC)) + 2; fat_numclu = partsize >> 9;
    fat_freeclu = (int64_t)*((uint32_t*)(fs_base + 0x3E8));
    fat_spf = bpb->spf32; fat_bpc = bpb->spc << 9; data_lba = bpb->spf32 * bpb->nf + bpb->rsc - 2 * bpb->spc + boot_lba;
    if(!(cluster = (uint8_t*)malloc(fat_bpc))) { fprintf(stderr, "easyboot: %s\r\n", lang[ERRMEM]); goto err; }
    /* load FAT */
    if(!dev_read(f, (bpb->rsc + boot_lba) << 9, fat_fat32, fat_spf << 9)) {
        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRRD], out); goto err;
    }

    /* delete contents of EASYBOOT directory if exists on the image or device (we might copy less files than was before) */
    if(fat_dirent(bpb->rc, 0, "EASYBOOT", 1, &clu, 0)) fat_rmdir(clu);

    /* add files from the given directory (also fills up plugins list) */
    parsedir(in, 0, 0, bpb->rc);

    /* now that we have the partitioning table and the file system plugins as well, do a through check */
    if(verbose > 1) check_partitions();
    free_plugins();

    /* add loaders */
    if(!fat_add(bpb->rc, "KERNEL8.IMG", loader_rpi_bin, sizeof(loader_rpi_bin))) {
        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRWR], out); goto err;
    }
    /* stage1 is not a file, it's in the boot sector */
    memcpy(img, boot_x86_bin, 0x1b8); img[0x1FE] = 0x55; img[0x1FF] = 0xAA;
    /* we can't use fat_add() for stage2, because it must be defragmented, stored on contiguous clusters */
    if(fat_dirent(bpb->rc, 1, "EFI", 1, &clu, 0) && fat_dirent(clu, 1, "BOOT", 1, &clu, 0) &&
      fat_dirent(clu, 2, "BOOTX64.EFI", 0, &clu, sizeof(loader_x86_efi)) &&
      dev_write(f, (data_lba + clu * bpb->spc) << 9, loader_x86_efi, sizeof(loader_x86_efi)))
        *((uint32_t*)(img + 0x1b0)) = data_lba + clu * bpb->spc;
    else { fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRWR], out); goto err; }

    /* write out metadata */
    fat_finish();
    if(!dev_write(f, boot_lba << 9, bpb, bpb->rsc << 9)) {
        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRBPB], out); goto err;
    }
    for(i = 0; (uint8_t)i < bpb->nf; i++) {
        data_lba = (fat_spf * i + bpb->rsc + boot_lba) << 9; j = fat_spf << 9;
        if(!dev_write(f, data_lba, fat_fat32, j)) {
            fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRFAT], out); goto err;
        }
    }
    if(!dev_write(f, 0, img, 512)) {
        fprintf(stderr, "easyboot: %s '%s'\r\n", lang[ERRPMBR], out); goto err;
    }
    if(verbose) {
        clu = (fat_numclu - fat_freeclu) * 1000 / fat_numclu;
        printf("Clusters total %u last %u free %u %u.%u%%\r\n", (uint32_t)fat_numclu, (uint32_t)fat_nextcluster,
            (uint32_t)fat_freeclu, clu / 10, clu % 10);
    }

    free(img);
    free(fs_base);
    free(fat_fat32);
    free(cluster);
    dev_close(f);
    verbose = 1; status("OK\n", NULL);
    return 0;
err:
    if(img) free(img);
    if(fs_base) free(fs_base);
    if(fat_fat32) free(fat_fat32);
    if(cluster) free(cluster);
    dev_close(f);
    return 1;
}
