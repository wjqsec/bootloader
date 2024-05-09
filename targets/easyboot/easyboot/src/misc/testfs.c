/*
 * src/misc/testfs.c
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
 * @brief File system plugin tester on hosted environment
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* mock up boot environment for hosted */
#define _start testfs
#define free(a,b)
#define PLG_API
#define DEBUG 1
#ifdef PATH_MAX
#undef PATH_MAX
#endif
#define PATH_MAX 256
FILE *f = NULL;
int verbose = 3;
uint64_t file_size = 0;
int  (*plg_open)(char*) = NULL;
uint64_t (*plg_read)(uint64_t, uint64_t, void*) = NULL;
void (*plg_close)(void) = NULL;
uint8_t root_buf[2*65536];
uint64_t pb_init(uint64_t size) { (void)size; return 0; }
void pb_draw(uint64_t curr) { (void)curr; }
void pb_fini(void) { }
void *alloc(uint32_t num) { return malloc(num * 4096); }
void sethooks(void *o, void *r, void *c) { plg_open = o; plg_read = r; plg_close = c; }
void loadsec(uint64_t sec, void *dst) { memset(dst, 0, 512); fseek(f, sec << 9, SEEK_SET); fread(dst, 1, 512, f); }
void testfs(void);
void dump(void *data, int n)
{
    uint8_t *ptr = (uint8_t*)data;
    int i, j;
    for(j = 0; j < n; j++, ptr += 16) {
        for(i = 0; i < 16; i++) printf("%02x ", ptr[i]);
        printf(" ");
        for(i = 0; i < 16; i++) printf("%c", ptr[i] >= 32 && ptr[i] < 127 ? ptr[i] : '.');
        printf("\n");
    }
}

/* include exactly one of the file system plugins here */
/*#include "../plugins/fat1x.c"*/
/*#include "../plugins/ext234.c"*/
/*#include "../plugins/mfs3.c"*/
/*#include "../plugins/fsZ.c"*/
/*#include "../plugins/ntfs.c"*/
/*#include "../plugins/befs.c"*/
/*#include "../plugins/axfs.c"*/
/*#include "../plugins/xfs.c"*/
/*#include "../plugins/ufs2.c"*/
/*#include "../plugins/ufs44.c"*/
#include "../plugins/exfat.c"

/**
 * Main function
 */
int main(int argc, char **argv)
{
    int i, j;
    uint64_t offs;
    uint8_t *data;

    /* check arguments */
    if(argc < 2) {
        printf("Easyboot fs plugin tester, Copyright (c) 2023 bzt, GPLv3+\r\nhttps://gitlab.com/bztsrc/easyboot\r\n\r\n");
        printf(" testfs <imgfile> [<path> [-dump]]\r\n\r\n");
        printf("  <imgfile>     file system image\r\n");
        printf("  <path>        path within the file system\r\n");
        printf("  -dump         dump the contents of path\r\n\r\n");
        return 1;
    }

    /* open file system image file and read in 128k (hopefully including the superblock) */
    f = fopen(argv[1], "rb");
    if(!f) { fprintf(stderr, "testfs: unable to open image\r\n"); return 2; }
    fread(root_buf, 1, 2*65536, f);

    /* initialize fs plugin (we don't check plugin magic bytes here, we assume it is the correct plugin) */
    testfs();
    if(!plg_open || !plg_read || !plg_close) { fprintf(stderr, "testfs: unable to initialize plugin\r\n"); fclose(f); return 2; }

    /* test open / read / close functions */
    if(argc > 2) {
        (*plg_open)(argv[2][0] == '/' ? argv[2] + 1 : argv[2]);
        printf("open(%s) = %ld\r\n", argv[2], file_size);
        if(!file_size) { fprintf(stderr, "testfs: unable to locate path '%s' in image\r\n", argv[2]); fclose(f); return 3; }
        data = malloc(file_size);
        if(!data) { fprintf(stderr, "testfs: unable to allocate memory\r\n"); fclose(f); return 4; }
        printf("read(0,%ld,ptr) = %ld\r\n", file_size, (*plg_read)(0, file_size, data));
        if(argc > 3)
            for(offs = 0, j = 16; offs < file_size; offs += j, data += j) {
                j = offs + 16 < file_size ? 16 : file_size - offs;
                printf("%06lx: ", offs);
                for(i = 0; i < 16; i++) if(i < j) printf("%02x ", data[i]); else printf("   ");
                printf(" ");
                for(i = 0; i < j; i++) printf("%c", data[i] >= 32 && data[i] < 127 ? data[i] : '.');
                printf("\n");
            }
        (*plg_close)();
        printf("close()\r\n");
    }

    /* teardown */
    fclose(f);
    printf("OK\r\n");
    return 0;
}
