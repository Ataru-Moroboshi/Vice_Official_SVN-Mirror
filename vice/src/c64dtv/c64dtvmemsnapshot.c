/*
 * c64dtvmemsnapshot.c - C64DTV memory snapshot handling.
 *
 * Written by
 *  Hannu Nuotio <hannu.nuotio@tut.fi>
 * Based on code by
 *  Andreas Boose <viceteam@t-online.de>
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

#include <stdio.h>
#include <string.h>

#include "c64dtv-resources.h"
#include "c64cart.h"
#include "c64mem.h"
#include "c64memrom.h"
#include "c64memsnapshot.h"
#include "c64pla.h"
#include "c64rom.h"
#include "log.h"
#include "mem.h"
#include "resources.h"
#include "snapshot.h"
#include "types.h"
#include "uiapi.h"

#include "c64dtvmem.h"
#include "c64dtvflash.h"
#include "c64dtvmemsnapshot.h"
#include "hummeradc.h"

#define SNAP_ROM_MAJOR 0
#define SNAP_ROM_MINOR 0

static log_t c64_snapshot_log = LOG_DEFAULT;

static const char snap_rom_module_name[] = "C64ROM";

#define NUM_TRAP_DEVICES 9  /* FIXME: is there a better constant ? */
static int trapfl[NUM_TRAP_DEVICES];
static int trapdevices[NUM_TRAP_DEVICES + 1] = { 1, 4, 5, 6, 7, 8, 9, 10, 11, -1 };

static void get_trapflags(void)
{
    int i;
    for(i = 0; trapdevices[i] != -1; i++) {
        resources_get_int_sprintf("VirtualDevice%d", &trapfl[i], trapdevices[i]);
    }
}

static void clear_trapflags(void)
{
    int i;
    for(i = 0; trapdevices[i] != -1; i++) {
        resources_set_int_sprintf("VirtualDevice%d", 0, trapdevices[i]);
    }
}

static void restore_trapflags(void)
{
    int i;
    for(i = 0; trapdevices[i] != -1; i++) {
        resources_set_int_sprintf("VirtualDevice%d", trapfl[i], trapdevices[i]);
    }
}

static int c64dtv_snapshot_write_rom_module(snapshot_t *s)
{
    snapshot_module_t *m;

    /* Main memory module.  */

    m = snapshot_module_create(s, snap_rom_module_name,
                               SNAP_ROM_MAJOR, SNAP_ROM_MINOR);
    if (m == NULL) {
        return -1;
    }

    /* disable traps before saving the ROM */
    get_trapflags();
    clear_trapflags();

    if (SMW_BA(m, c64dtvflash_mem, 0x200000) < 0 /* hack */
        || SMW_B(m, c64dtvflash_state) < 0
        || SMW_BA(m, c64dtvflash_mem_lock, 39) < 0) {
        goto fail;
    }

    if (snapshot_module_close(m) < 0) {
        goto fail;
    }

    restore_trapflags();

    return 0;

fail:
    if (m != NULL) {
        snapshot_module_close(m);
    }

    restore_trapflags();

    return -1;
}

static int c64dtv_snapshot_read_rom_module(snapshot_t *s)
{
    uint8_t major_version, minor_version;
    snapshot_module_t *m;

    /* Main memory module.  */

    m = snapshot_module_open(s, snap_rom_module_name,
                             &major_version, &minor_version);
    if (m == NULL) {
        /* this module is optional */
        /* FIXME: reset all cartridge stuff to standard C64 behaviour */
        return 0;
    }

    if (snapshot_version_is_bigger(major_version, minor_version, SNAP_ROM_MAJOR, SNAP_ROM_MINOR)) {
        log_error(c64_snapshot_log,
                  "Snapshot module version (%d.%d) newer than %d.%d.",
                  major_version, minor_version,
                  SNAP_ROM_MAJOR, SNAP_ROM_MINOR);
        snapshot_module_close(m);
        return -1;
    }

    /* disable traps before loading the ROM */
    get_trapflags();
    clear_trapflags();

    if (SMR_BA(m, c64dtvflash_mem, 0x200000) < 0 /* hack */
        || SMR_B(m, &c64dtvflash_state) < 0
        || SMR_BA(m, c64dtvflash_mem_lock, 39) < 0) {
        goto fail;
    }

    if (snapshot_module_close(m) < 0) {
        goto fail;
    }

    /* enable traps again when necessary */

    restore_trapflags();

    return 0;

fail:
    if (m != NULL) {
        snapshot_module_close(m);
    }
    restore_trapflags();
    return -1;
}


#define SNAP_MAJOR 0
#define SNAP_MINOR 0
static const char snap_mem_module_name[] = "C64MEM";

int c64dtv_snapshot_write_module(snapshot_t *s, int save_roms)
{
    snapshot_module_t *m;

    /* Main memory module.  */
    m = snapshot_module_create(s, snap_mem_module_name, SNAP_MAJOR, SNAP_MINOR);
    if (m == NULL) {
        return -1;
    }

    if (SMW_B(m, pport.data) < 0
        || SMW_B(m, pport.dir) < 0
        || SMW_BA(m, mem_ram, 0x200000) < 0 /* hack */
        || SMW_B(m, c64dtvmem_memmapper[0]) < 0
        || SMW_B(m, c64dtvmem_memmapper[1]) < 0
        || SMW_B(m, pport.data_out) < 0
        || SMW_B(m, pport.data_read) < 0
        || SMW_B(m, pport.dir_read) < 0) {
        goto fail;
    }

    if (snapshot_module_close(m) < 0) {
        goto fail;
    }
    m = NULL;

    if (save_roms && c64dtv_snapshot_write_rom_module(s) < 0) {
        goto fail;
    }

    return 0;

fail:
    if (m != NULL) {
        snapshot_module_close(m);
    }
    return -1;
}

int c64dtv_snapshot_read_module(snapshot_t *s)
{
    uint8_t major_version, minor_version;
    snapshot_module_t *m;

    /* Main memory module.  */

    m = snapshot_module_open(s, snap_mem_module_name,
                             &major_version, &minor_version);
    if (m == NULL) {
        return -1;
    }

    if (snapshot_version_is_bigger(major_version, minor_version, SNAP_MAJOR, SNAP_MINOR)) {
        log_error(c64_snapshot_log,
                  "Snapshot module version (%d.%d) newer than %d.%d.",
                  major_version, minor_version,
                  SNAP_MAJOR, SNAP_MINOR);
        goto fail;
    }

    if (SMR_B(m, &pport.data) < 0
        || SMR_B(m, &pport.dir) < 0
        || SMR_BA(m, mem_ram, 0x200000) < 0 /* hack */
        || SMR_B(m, &c64dtvmem_memmapper[0]) < 0
        || SMR_B(m, &c64dtvmem_memmapper[1]) < 0) {
        goto fail;
    }

    /* new since 1.15.x */
    SMR_B(m, &pport.data_out);
    SMR_B(m, &pport.data_read);
    SMR_B(m, &pport.dir_read);

    mem_pla_config_changed();

    if (snapshot_module_close(m) < 0) {
        goto fail;
    }
    m = NULL;

    if (c64dtv_snapshot_read_rom_module(s) < 0) {
        goto fail;
    }

    return 0;

fail:
    if (m != NULL) {
        snapshot_module_close(m);
    }
    return -1;
}

