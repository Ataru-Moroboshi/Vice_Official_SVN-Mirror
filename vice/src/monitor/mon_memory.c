/*
 * mon_memory.c - The VICE built-in monitor memory functions.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Daniel Sladic <sladic@eecg.toronto.edu>
 *  Ettore Perazzoli <ettore@comm2000.it>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charset.h"
#include "console.h"
#include "lib.h"
#include "mem.h"
#include "montypes.h"
#include "mon_memory.h"
#include "mon_util.h"
#include "types.h"


#define ADDR_LIMIT(x) ((uint16_t)(addr_mask(x)))

void mon_memory_move(MON_ADDR start_addr, MON_ADDR end_addr, MON_ADDR dest)
{
    long i;
    unsigned int dst;
    long len;
    uint16_t start;
    MEMSPACE src_mem, dest_mem;
    uint8_t *buf;

    len = mon_evaluate_address_range(&start_addr, &end_addr, TRUE, -1);
    if (len <= 0) {
        mon_out("Invalid range.\n");
        return;
    }
    src_mem = addr_memspace(start_addr);
    start = addr_location(start_addr);

    mon_evaluate_default_addr(&dest);
    dst = addr_location(dest);
    dest_mem = addr_memspace(dest);

    buf = lib_malloc(sizeof(uint8_t) *len);

    for (i = 0; i < len; i++) {
        buf[i] = mon_get_mem_val(src_mem, (uint16_t)ADDR_LIMIT(start + i));
    }

    for (i = 0; i < len; i++) {
        mon_set_mem_val(dest_mem, (uint16_t)ADDR_LIMIT(dst + i), buf[i]);
    }

    lib_free(buf);
}

void mon_memory_compare(MON_ADDR start_addr, MON_ADDR end_addr, MON_ADDR dest)
{
    uint16_t start;
    MEMSPACE src_mem, dest_mem;
    uint8_t byte1, byte2;
    long i;
    unsigned int dst;
    long len;

    len = mon_evaluate_address_range(&start_addr, &end_addr, TRUE, -1);
    if (len < 0) {
        mon_out("Invalid range.\n");
        return;
    }
    src_mem = addr_memspace(start_addr);
    start = addr_location(start_addr);

    mon_evaluate_default_addr(&dest);
    dst = addr_location(dest);
    dest_mem = addr_memspace(dest);

    for (i = 0; i < len; i++) {
        byte1 = mon_get_mem_val(src_mem, (uint16_t)ADDR_LIMIT(start + i));
        byte2 = mon_get_mem_val(dest_mem, (uint16_t)ADDR_LIMIT(dst + i));

        if (byte1 != byte2) {
            mon_out("$%04x $%04x: %02x %02x\n",
                    ADDR_LIMIT(start + i), ADDR_LIMIT(dst + i), byte1, byte2);
        }
    }
}

void mon_memory_fill(MON_ADDR start_addr, MON_ADDR end_addr,
                     unsigned char *data)
{
    uint16_t start;
    MEMSPACE dest_mem;
    unsigned int mon_index;
    long i;
    long len;

    len = mon_evaluate_address_range(&start_addr, &end_addr, FALSE,
                                     (uint16_t)data_buf_len);
    if (len < 0) {
        mon_out("Invalid range.\n");
        return;
    }
    start = addr_location(start_addr);

    if (!mon_is_valid_addr(start_addr)) {
        mon_out("Invalid start address\n");
        return;
    }

    dest_mem = addr_memspace(start_addr);

    i = 0;
    mon_index = 0;
    while (i < len) {
        mon_set_mem_val(dest_mem, (uint16_t)ADDR_LIMIT(start + i),
                        data_buf[mon_index++]);
        if (mon_index >= data_buf_len) {
            mon_index = 0;
        }
        i++;
    }

    mon_clear_buffer();
}

void mon_memory_hunt(MON_ADDR start_addr, MON_ADDR end_addr,
                     unsigned char *data)
{
    uint8_t *buf;
    uint16_t start, next_read;
    MEMSPACE mem;
    long i;
    long len;

    len = mon_evaluate_address_range(&start_addr, &end_addr, TRUE, -1);
    if (len < 0 || len < data_buf_len) {
        mon_out("Invalid range.\n");
        return;
    }
    mem = addr_memspace(start_addr);
    start = addr_location(start_addr);

    buf = lib_malloc(data_buf_len);

    /* Fill buffer */
    for (i = 0; i < data_buf_len; i++) {
        buf[i] = mon_get_mem_val(mem, (uint16_t)ADDR_LIMIT(start + i));
    }

    /* Do compares */
    next_read = start + (uint16_t)data_buf_len;

    for (i = 0; i <= (len - data_buf_len); i++, next_read++) {
        int not_found = 0;
        unsigned int j;
        for (j = 0; j < data_buf_len; j++) {
            if ((buf[j] & data_mask_buf[j]) != data_buf[j]) {
                not_found = 1;
                break;
            }
        }
        if (!not_found) {
            mon_out("%04x\n", ADDR_LIMIT(start + i));
        }

        if (data_buf_len > 1) {
            memmove(&(buf[0]), &(buf[1]), data_buf_len - 1);
        }
        buf[data_buf_len - 1] = mon_get_mem_val(mem, next_read);
    }

    mon_clear_buffer();
    lib_free(buf);
}

static const int radix_chars_per_byte[] = {
    2, /* default = hex */
    2, /* hexadecimal */
    3, /* decimal */
    3, /* octal */
    8, /* binary */
};

static void set_addr_location(MON_ADDR *a, unsigned l)
{
    *a = new_addr(addr_memspace(*a), addr_mask(l));
}

/* display memory dump (binary, text) 'm', 'i', 'ii' commands */
void mon_memory_display(int radix_type, MON_ADDR start_addr, MON_ADDR end_addr, mon_display_format_t format)
{
    long i;
    unsigned int m;
    unsigned int cnt = 0;
    long len;
    unsigned int max_width; /* max. amount of bytes to print per line */
    unsigned int real_width; /* actual amount of bytes printed per line */
    static uint8_t screen_width = 0;
    uint16_t addr = 0;
    char *printables;
    char prefix;
    MEMSPACE mem;
    uint16_t display_number;
    uint8_t v;
    size_t plen;

    /* FIXME: this should really be handled by the UI instead, ie the UI should
              always just give us valid numbers */
    static int last_known_xres = 40, last_known_yres = 25;
    if (console_log) {
        last_known_xres = console_log->console_xres;
        last_known_yres = console_log->console_yres;
    }

    prefix = (format == DF_PETSCII) ? '>' : '*';

    if (radix_type != e_text) {
        /* numeric memory dump */
#if 0
        switch (radix_type) {
            case e_hexadecimal: /* 8 chars prompt,
                                   2 chars per byte, plus 1 space per byte
                                     every block of 4 starts with an extra space,
                                   1 char per byte, plus two spaces per line */
            case e_decimal: /* 8 chars prompt,
                               3 chars per byte, plus 1 space per byte
                                 every block of 4 starts with an extra space,
                               1 char per byte, plus two spaces per line */
            case e_octal:
            case e_binary: /* 8 chars prompt,
                              8 chars per byte, plus 1 space per byte
                                every block of 4 starts with an extra space,
                              1 char per byte, plus two spaces per line */
            default:
                max_width = (last_known_xres - (8 + 2)) * 4;
                max_width /= ((radix_chars_per_byte[radix_type] + 1 + 1) * 4) + 1;
                break;
        }
#endif
        max_width = (last_known_xres - (8 + 2)) * 4;
        max_width /= ((radix_chars_per_byte[radix_type] + 1 + 1) * 4) + 1;

        /* to make the output easier to read, make sure the number of items
           each line equals a power of two */
        m = 1;
        while ((m * 2) <= max_width)  {
            m *= 2;
        }
        max_width = m;

        display_number = max_width * ((last_known_yres - 6) / 2);
    } else {
        /* textual memory dump */
        uint16_t base;
        uint8_t rows;
        int bank;

        mem_get_screen_parameter(&base, &rows, &screen_width, &bank);
        /* HACK: mem_get_screen_parameter() returns actual screen size from the
           video chip registers - this can be zero rows or zero columns! If
           width is 0, use last known terminal width instead, to avoid division
           by 0 further below. */
        if (screen_width == 0) {
            screen_width = last_known_xres;
        }

        max_width = screen_width;

        if (max_width > (last_known_xres - (7 + 2))) {
            max_width = (last_known_xres - (7 + 2));
            /* to make the output easier to read, make sure the number of items
            each line equals a power of two */
            m = 1;
            while ((m * 2) <= max_width)  {
                m *= 2;
            }
            max_width = m;
        } else {
    /* NOTE: show N multiples of screens width, disable this again if it
            turns out thats not what we want */
#if 1
            /* FIXME: limit width vs console width */
            while ((7 + 2) + (max_width * 2) + (((max_width * 2) + (screen_width - 1)) / screen_width) <= last_known_xres)  {
                max_width *= 2;
            }
#endif
        }
        display_number = max_width * ((last_known_yres - 6) / 2);
    }

    /* allocate proper buffer for 'printables' */
    plen = max_width + 1;
#if 0
    printf("allocating %lu bytes for 'printables' array\n",
            (unsigned long)plen);
#endif
    printables = lib_malloc(plen);

    len = mon_evaluate_address_range(&start_addr, &end_addr, FALSE,
                                     display_number);
    mem = addr_memspace(start_addr);
    addr = addr_location(start_addr);

    while (cnt < len) {
        memset(printables, 0, plen);
        /* prompt, ">C:e9e6 " (8 chars) */
        mon_out("%c%s:%04x ", prefix, mon_memspace_string[mem], addr);
        for (i = 0, real_width = 0; i < max_width; i++) {
            v = mon_get_mem_val(mem, (uint16_t)ADDR_LIMIT(addr + i));

            switch (radix_type) {
                case e_text: /* special case == petscii text/screencode */
                    /* every block of screen_width bytes starts with an extra space */
                    if (screen_width) {
                        if (!(i % screen_width)) {
                            mon_out(" ");
                        }
                    }
                    if (cnt < len) {
                        if (format == DF_PETSCII) {
                            mon_petscii_out(1, "%c", v);
                        } else {
                            mon_scrcode_out(1, "%c", v);
                        }
                    } else {
                        mon_out(" ");
                    }
                    real_width++;
                    cnt++;
                    break;
                case e_hexadecimal:
                    /* every block of 4 bytes starts with an extra space */
                    if (!(i % 4)) {
                        mon_out(" ");
                    }
                    if (cnt < len) {
                        mon_out("%02x ", v); /* 3 chars */
                        real_width++;
                    } else {
                        mon_out("   ");
                    }
                    cnt++;
                    break;
                case e_decimal:
                    /* every block of 4 bytes starts with an extra space */
                    if (!(i % 4)) {
                        mon_out(" ");
                    }
                    if (cnt < len) {
                        mon_out("%3d ", v); /* 4 chars */
                        real_width++;
                        cnt++;
                    } else {
                        mon_out("    ");
                    }
                    break;
                case e_octal:
                    /* every block of 4 bytes starts with an extra space */
                    if (!(i % 4)) {
                        mon_out(" ");
                    }
                    if (cnt < len) {
                        mon_out("%03o ", v); /* 4 chars */
                        real_width++;
                        cnt++;
                    } else {
                        mon_out("    ");
                    }
                    break;
                case e_binary:
                    /* every block of 4 bytes starts with an extra space */
                    if (!(i % 4)) {
                        mon_out(" ");
                    }
                    /* every block of 4 bytes starts with an extra space */
                    if (cnt < len) {
                        mon_print_bin(v, '1', '0'); /* 8 chars */
                        mon_out(" "); /* 1 char */
                        real_width++;
                        cnt++;
                    } else {
                        mon_out("         ");
                    }
                    break;
                default:
                    lib_free(printables);
                    return;
            }
        }

        if (radix_type != e_text) {
            /* for numeric output, add N chars ascii output, plus two spaces */
            uint8_t val;

            mon_out("  ");

            for (i = 0; i < real_width; i++) {
                val = mon_get_mem_val(mem, addr + i);
                mon_petscii_out(1, "%c", val);
            }
        }

        mon_out("\n");
        addr = ADDR_LIMIT(addr + real_width);
        if (mon_stop_output != 0) {
            break;
        }
    }

    lib_free(printables);
    set_addr_location(&(dot_addr[mem]), addr);
}

/* display binary data (sprites/chars), "mc" and "ms" commands */
void mon_memory_display_data(MON_ADDR start_addr, MON_ADDR end_addr,
                             unsigned int x, unsigned int y)
{
    long i;
    long j;
    long len;
    long cnt = 0;
    uint16_t addr = 0;
    MEMSPACE mem;

    len = mon_evaluate_address_range(&start_addr, &end_addr, FALSE,
                                     (uint16_t)((x * y) / 8));
    mem = addr_memspace(start_addr);
    addr = addr_location(start_addr);

    while (cnt < len) {
        for (i = 0; i < y; i++) {
            mon_out(">%s:%04x ", mon_memspace_string[mem], addr);
            for (j = 0; j < (x / 8); j++) {
                mon_print_bin(mon_get_mem_val(mem,
                                              (uint16_t)(ADDR_LIMIT(addr + j))), '#', '.');
                cnt++;
            }
            for (j = 0; j < (x / 8); j++) {
                mon_out(" %02x", mon_get_mem_val(mem,(uint16_t)(ADDR_LIMIT(addr + j))));
            }
            mon_out("\n");
            addr = ADDR_LIMIT(addr + (x / 8));
            if (mon_stop_output != 0) {
                break;
            }
        }

        mon_out("\n");
        if (mon_stop_output != 0) {
            break;
        }
        if ((x == 24) && (y == 21)) {
            addr++; /* continue at next even address when showing sprites */
        }
    }

    set_addr_location(&(dot_addr[mem]), addr);
}
