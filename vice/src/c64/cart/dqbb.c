/*
 * dqbb.c - Double Quick Brown Box emulation.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
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
#include <stdlib.h>
#include <string.h>

#define CARTRIDGE_INCLUDE_SLOT1_API
#include "c64cartsystem.h"
#undef CARTRIDGE_INCLUDE_SLOT1_API
#include "c64mem.h"
#include "cartio.h"
#include "cartridge.h"
#include "cmdline.h"
#include "export.h"
#include "lib.h"
#include "log.h"
#include "mem.h"
#include "monitor.h"
#include "ram.h"
#include "resources.h"
#include "snapshot.h"
#include "types.h"
#include "util.h"

#define CARTRIDGE_INCLUDE_PRIVATE_API
#include "dqbb.h"
#undef CARTRIDGE_INCLUDE_PRIVATE_API

/*
    "Double Quick Brown box"

    - 16k RAM

    The Double Quick Brown box is a banked memory system.
    It uses a register at $de00 to control the areas used, the
    read/write / read-only state and on/off of the cart.

    This is done as follows:

    bit 2:   1 = $A000-$BFFF mapped in, 0 = $A000-$BFFF not mapped in.
    bit 4:   1 = read/write, 0 = read-only.
    bit 7:   1 = cart off, 0 = cart on.

    The register is write-only. Attempting to read it will
    only return random values.

    The current emulation has the register mirrorred through the
    range of $de00-$deff
*/

/* #define DBGDQBB */

#ifdef DBGDQBB
#define DBG(x) log_printf  x
#else
#define DBG(x)
#endif

static log_t dqbb_log = LOG_DEFAULT; /*!< the log output for the dqbb_log */

/* DQBB register bits */
static int dqbb_a000_mapped;
static int dqbb_readwrite;
static int dqbb_off;

/* DQBB image.  */
static uint8_t *dqbb_ram = NULL;

static int dqbb_activate(void);
static int dqbb_deactivate(void);
static void dqbb_change_config(void);

/* Flag: Do we enable the DQBB?  */
static int dqbb_enabled = 0;

/* Filename of the DQBB image.  */
static char *dqbb_filename = NULL;

#define DQBB_RAM_SIZE   0x4000

static int reg_value = 0;

static int dqbb_write_image = 0;

/* ------------------------------------------------------------------------- */

static uint8_t dqbb_io1_peek(uint16_t addr);
static void dqbb_io1_store(uint16_t addr, uint8_t byte);
static int dqbb_dump(void);

static io_source_t dqbb_io1_device = {
    CARTRIDGE_NAME_DQBB,  /* name of the device */
    IO_DETACH_RESOURCE,   /* use resource to detach the device when involved in a read-collision */
    "DQBB",               /* resource to set to '0' */
    0xde00, 0xdeff, 0xff, /* range for the device, address is ignored, reg:$de00, mirrors: $de01-$deff */
    0,                    /* read is never valid, device is write only */
    dqbb_io1_store,       /* store function */
    NULL,                 /* NO poke function */
    NULL,                 /* NO read function */
    dqbb_io1_peek,        /* peek function */
    dqbb_dump,            /* device state information dump function */
    CARTRIDGE_DQBB,       /* cartridge ID */
    IO_PRIO_NORMAL,       /* normal priority, device read needs to be checked for collisions */
    0,                    /* insertion order, gets filled in by the registration function */
    IO_MIRROR_NONE        /* NO mirroring */
};

static io_source_list_t *dqbb_io1_list_item = NULL;

static const export_resource_t export_res = {
    CARTRIDGE_NAME_DQBB, 1, 1, &dqbb_io1_device, NULL, CARTRIDGE_DQBB
};

/* ------------------------------------------------------------------------- */

int dqbb_cart_enabled(void)
{
    return dqbb_enabled;
}

static void dqbb_change_config(void)
{
    if (dqbb_enabled) {
        if (dqbb_off) {
            cart_config_changed_slot1(2, 2, CMODE_READ);
        } else {
            if (dqbb_a000_mapped) {
                cart_config_changed_slot1(1, 1, CMODE_READ);
            } else {
                cart_config_changed_slot1(0, 0, CMODE_READ);
            }
        }
    } else {
        cart_config_changed_slot1(2, 2, CMODE_READ);
    }
}

static void dqbb_io1_store(uint16_t addr, uint8_t byte)
{
    dqbb_a000_mapped = (byte & 4) >> 2;
    dqbb_readwrite = (byte & 0x10) >> 4;
    dqbb_off = (byte & 0x80) >> 7;
    dqbb_change_config();
    reg_value = byte;
}

static uint8_t dqbb_io1_peek(uint16_t addr)
{
    return reg_value;
}

static int dqbb_dump(void)
{
    mon_out("$A000-$BFFF RAM: %s, cart status: %s\n",
            (reg_value & 4) ? "mapped in" : "not mapped in",
            (reg_value & 0x80) ? ((reg_value & 0x10) ? "read/write" : "read-only") : "disabled");
    return 0;
}

/* ------------------------------------------------------------------------- */

/* FIXME: this still needs to be tweaked to match the hardware */
static RAMINITPARAM ramparam = {
    .start_value = 255,
    .value_invert = 2,
    .value_offset = 1,

    .pattern_invert = 0x100,
    .pattern_invert_value = 255,

    .random_start = 0,
    .random_repeat = 0,
    .random_chance = 0,
};

void dqbb_powerup(void)
{
    DBG(("dqbb_powerup"));
    if ((dqbb_filename != NULL) && (*dqbb_filename != 0)) {
        /* do not init ram if a file is used for ram content (like battery backup) */
        return;
    }
    if (dqbb_ram) {
        DBG(("dqbb_powerup ram clear"));
        ram_init_with_pattern(dqbb_ram, DQBB_RAM_SIZE, &ramparam);
    }
}

static int dqbb_activate(void)
{
    DBG(("dqbb_activate"));
    lib_free(dqbb_ram);
    dqbb_ram = lib_malloc(DQBB_RAM_SIZE);
    ram_init_with_pattern(dqbb_ram, DQBB_RAM_SIZE, &ramparam);

    if (dqbb_log == LOG_DEFAULT) {
        dqbb_log = log_open("DQBB");
    }

    if (!util_check_null_string(dqbb_filename)) {
        if (util_file_load(dqbb_filename, dqbb_ram, DQBB_RAM_SIZE, UTIL_FILE_LOAD_RAW) < 0) {
            /* only create a new file if no file exists, so we dont accidently overwrite any files */
            if (!util_file_exists(dqbb_filename)) {
                if (util_file_save(dqbb_filename, dqbb_ram, DQBB_RAM_SIZE) < 0) {
                    return -1;
                }
                log_message(dqbb_log, "created '%s'", dqbb_filename);
            }
        } else {
            log_message(dqbb_log, "loaded '%s'", dqbb_filename);
        }
    }
    return 0;
}

static int dqbb_deactivate(void)
{
    if (dqbb_ram == NULL) {
        return 0;
    }

    if (!util_check_null_string(dqbb_filename)) {
        if (dqbb_write_image) {
            if (util_file_save(dqbb_filename, dqbb_ram, DQBB_RAM_SIZE) < 0) {
                return -1;
            }
        }
    }

    lib_free(dqbb_ram);
    dqbb_ram = NULL;

    export_remove(&export_res);

    return 0;
}

static int set_dqbb_enabled(int value, void *param)
{
    int val = value ? 1 : 0;

    if ((!val) && (dqbb_enabled)) {
        cart_power_off();
        if (dqbb_deactivate() < 0) {
            return -1;
        }
        io_source_unregister(dqbb_io1_list_item);
        dqbb_io1_list_item = NULL;
        dqbb_enabled = 0;
        dqbb_reset();
        dqbb_change_config();
    } else if ((val) && (!dqbb_enabled)) {
        cart_power_off();
        if (export_add(&export_res) < 0) {
            return -1;
        }
        if (dqbb_activate() < 0) {
            return -1;
        }
        dqbb_io1_list_item = io_source_register(&dqbb_io1_device);
        dqbb_enabled = 1;
        dqbb_reset();
        dqbb_change_config();
    }
    return 0;
}

static int set_dqbb_filename(const char *name, void *param)
{
    if (dqbb_filename != NULL && name != NULL && strcmp(name, dqbb_filename) == 0) {
        return 0;
    }

    if (name != NULL && *name != '\0') {
        if (util_check_filename_access(name) < 0) {
            return -1;
        }
    }

    if (dqbb_enabled) {
        dqbb_deactivate();
        util_string_set(&dqbb_filename, name);
        dqbb_activate();
    } else {
        util_string_set(&dqbb_filename, name);
    }

    return 0;
}

static int set_dqbb_image_write(int val, void *param)
{
    dqbb_write_image = val ? 1 : 0;

    return 0;
}

/* ---------------------------------------------------------------------*/

static const resource_string_t resources_string[] = {
    { "DQBBfilename", "", RES_EVENT_NO, NULL,
      &dqbb_filename, set_dqbb_filename, NULL },
    RESOURCE_STRING_LIST_END
};

static const resource_int_t resources_int[] = {
    { "DQBB", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &dqbb_enabled, set_dqbb_enabled, NULL },
    { "DQBBImageWrite", 0, RES_EVENT_NO, NULL,
      &dqbb_write_image, set_dqbb_image_write, NULL },
    RESOURCE_INT_LIST_END
};

int dqbb_resources_init(void)
{
    if (resources_register_string(resources_string) < 0) {
        return -1;
    }

    return resources_register_int(resources_int);
}

void dqbb_resources_shutdown(void)
{
    lib_free(dqbb_filename);
    dqbb_filename = NULL;
}

/* ------------------------------------------------------------------------- */

static const cmdline_option_t cmdline_options[] =
{
    { "-dqbb", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "DQBB", (resource_value_t)1,
      NULL, "Enable Double Quick Brown Box" },
    { "+dqbb", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "DQBB", (resource_value_t)0,
      NULL, "Disable Double Quick Brown Box" },
    { "-dqbbimage", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "DQBBfilename", NULL,
      "<Name>", "Specify Double Quick Brown Box filename" },
    { "-dqbbimagerw", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "DQBBImageWrite", (resource_value_t)1,
      NULL, "Allow writing to DQBB image" },
    { "+dqbbimagerw", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "DQBBImageWrite", (resource_value_t)0,
      NULL, "Do not write to DQBB image" },
    CMDLINE_LIST_END
};

int dqbb_cmdline_options_init(void)
{
    return cmdline_register_options(cmdline_options);
}

/* ------------------------------------------------------------------------- */

const char *dqbb_get_file_name(void)
{
    return dqbb_filename;
}

void dqbb_reset(void)
{
    dqbb_a000_mapped = 0;
    dqbb_readwrite = 0;
    dqbb_off = 0;
    if (dqbb_enabled) {
        dqbb_change_config();
    }
}

void dqbb_mmu_translate(unsigned int addr, uint8_t **base, int *start, int *limit)
{
    switch (addr & 0xf000) {
        case 0xb000:
        case 0xa000:
        case 0x9000:
        case 0x8000:
            *base = dqbb_ram - 0x8000;
            *start = 0x8000;
            *limit = 0xbffd;
            return;
        default:
            break;
    }
    *base = NULL;
    *start = 0;
    *limit = 0;
}

void dqbb_init_config(void)
{
    dqbb_reset();
}

void dqbb_config_setup(uint8_t *rawcart)
{
    memcpy(dqbb_ram, rawcart, DQBB_RAM_SIZE);
}

/* ------------------------------------------------------------------------- */

void dqbb_detach(void)
{
    resources_set_int("DQBB", 0);
}

int dqbb_enable(void)
{
    if (resources_set_int("DQBB", 1) < 0) {
        return -1;
    }
    return 0;
}


/** \brief  Disable the cart
 *
 * Does the same as dqbb_detach(), but required for symmetry I suppose.
 *
 * \return  0 on success, -1 on failure
 */
int dqbb_disable(void)
{
    if (resources_set_int("DQBB", 0) < 0) {
        return -1;
    }
    return 0;
}


int dqbb_bin_attach(const char *filename, uint8_t *rawcart)
{
    if (util_file_load(filename, rawcart, DQBB_RAM_SIZE, UTIL_FILE_LOAD_RAW) < 0) {
        return -1;
    }
    util_string_set(&dqbb_filename, filename);
    return dqbb_enable();
}

int dqbb_bin_save(const char *filename)
{
    if (dqbb_ram == NULL) {
        return -1;
    }

    if (filename == NULL) {
        return -1;
    }

    if (util_file_save(filename, dqbb_ram, DQBB_RAM_SIZE) < 0) {
        return -1;
    }
    return 0;
}

int dqbb_flush_image(void)
{
    return dqbb_bin_save(dqbb_filename);
}

/* ------------------------------------------------------------------------- */

uint8_t dqbb_roml_read(uint16_t addr)
{
    return dqbb_ram[addr & 0x1fff];
}

void dqbb_roml_store(uint16_t addr, uint8_t byte)
{
    if (dqbb_readwrite) {
        dqbb_ram[addr & 0x1fff] = byte;
    }
    mem_store_without_romlh(addr, byte);
}

uint8_t dqbb_romh_read(uint16_t addr)
{
    return dqbb_ram[(addr & 0x1fff) + 0x2000];
}

void dqbb_romh_store(uint16_t addr, uint8_t byte)
{
    if (dqbb_readwrite) {
        dqbb_ram[(addr & 0x1fff) + 0x2000] = byte;
    }
    mem_store_without_romlh(addr, byte);
}

int dqbb_peek_mem(uint16_t addr, uint8_t *value)
{
    if ((addr >= 0x8000) && (addr <= 0x9fff)) {
        *value = dqbb_ram[addr & 0x1fff];
        return CART_READ_VALID;
    } else if ((addr >= 0xa000) && (addr <= 0xbfff)) {
        *value = dqbb_ram[(addr & 0x1fff) + 0x2000];
        return CART_READ_VALID;
    }
    return CART_READ_THROUGH;
}

/* ---------------------------------------------------------------------*/

/* CARTDQBB snapshot module format:

   type  | name       | description
   --------------------------------
   BYTE  | enabled    | cartridge enabled flag
   BYTE  | read write | read/write flag
   BYTE  | a000 map   | $A000 mapped flag
   BYTE  | off        | dqbb off flag
   BYTE  | register   | register
   ARRAY | RAM        | 16768 BYTES of RAM data
 */

static const char snap_module_name[] = "CARTDQBB";
#define SNAP_MAJOR   0
#define SNAP_MINOR   0

int dqbb_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, snap_module_name, SNAP_MAJOR, SNAP_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (0
        || (SMW_B(m, (uint8_t)dqbb_enabled) < 0)
        || (SMW_B(m, (uint8_t)dqbb_readwrite) < 0)
        || (SMW_B(m, (uint8_t)dqbb_a000_mapped) < 0)
        || (SMW_B(m, (uint8_t)dqbb_off) < 0)
        || (SMW_B(m, (uint8_t)reg_value) < 0)
        || (SMW_BA(m, dqbb_ram, DQBB_RAM_SIZE) < 0)) {
        snapshot_module_close(m);
        return -1;
    }

    return snapshot_module_close(m);
}

int dqbb_snapshot_read_module(snapshot_t *s)
{
    uint8_t vmajor, vminor;
    snapshot_module_t *m;

    m = snapshot_module_open(s, snap_module_name, &vmajor, &vminor);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than current */
    if (snapshot_version_is_bigger(vmajor, vminor, SNAP_MAJOR, SNAP_MINOR)) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        snapshot_module_close(m);
        return -1;
    }

    dqbb_ram = lib_malloc(DQBB_RAM_SIZE);

    if (0
        || (SMR_B_INT(m, &dqbb_enabled) < 0)
        || (SMR_B_INT(m, &dqbb_readwrite) < 0)
        || (SMR_B_INT(m, &dqbb_a000_mapped) < 0)
        || (SMR_B_INT(m, &dqbb_off) < 0)
        || (SMR_B_INT(m, &reg_value) < 0)
        || (SMR_BA(m, dqbb_ram, DQBB_RAM_SIZE) < 0)) {
        snapshot_module_close(m);
        lib_free(dqbb_ram);
        dqbb_ram = NULL;
        return -1;
    }

    snapshot_module_close(m);

    /* dqbb_filetype = 0; */
    dqbb_write_image = 0;
    dqbb_enabled = 1;

    /* FIXME: ugly code duplication to avoid cart_config_changed calls */
    dqbb_io1_list_item = io_source_register(&dqbb_io1_device);

    if (export_add(&export_res) < 0) {
        lib_free(dqbb_ram);
        dqbb_ram = NULL;
        io_source_unregister(dqbb_io1_list_item);
        dqbb_io1_list_item = NULL;
        dqbb_enabled = 0;
        return -1;
    }

    return 0;
}
