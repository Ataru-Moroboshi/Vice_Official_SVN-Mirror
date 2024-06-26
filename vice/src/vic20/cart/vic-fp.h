/*
 * vic-fp.h -- Vic Flash Plugin emulation.
 *
 * Written by
 *  Marko Makela <marko.makela@iki.fi>
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

#ifndef VICE_VIC_FP_H
#define VICE_VIC_FP_H

#include <stdio.h>

#include "types.h"

uint8_t vic_fp_ram123_read(uint16_t addr);
void vic_fp_ram123_store(uint16_t addr, uint8_t value);
uint8_t vic_fp_blk1_read(uint16_t addr);
void vic_fp_blk1_store(uint16_t addr, uint8_t value);
uint8_t vic_fp_blk23_read(uint16_t addr);
void vic_fp_blk23_store(uint16_t addr, uint8_t value);
uint8_t vic_fp_blk5_read(uint16_t addr);
void vic_fp_blk5_store(uint16_t addr, uint8_t value);

void vic_fp_init(void);
void vic_fp_reset(void);
void vic_fp_powerup(void);

void vic_fp_config_setup(uint8_t *rawcart);
int vic_fp_bin_attach(const char *filename);
/* int vic_fp_bin_attach(const char *filename, uint8_t *rawcart); */
int vic_fp_crt_attach(FILE *fd, uint8_t *rawcart, const char *filename);
void vic_fp_detach(void);

int vic_fp_bin_save(const char *filename);
int vic_fp_crt_save(const char *filename);
int vic_fp_flush_image(void);

int vic_fp_resources_init(void);
void vic_fp_resources_shutdown(void);
int vic_fp_cmdline_options_init(void);

struct snapshot_s;

int vic_fp_snapshot_write_module(struct snapshot_s *s);
int vic_fp_snapshot_read_module(struct snapshot_s *s);

#endif
