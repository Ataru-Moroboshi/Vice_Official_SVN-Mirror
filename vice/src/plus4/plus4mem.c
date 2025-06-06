/*
 * plus4mem.c -- Plus4 memory handling.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Tibor Biczo <crown@axelero.hu>
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
#include <string.h>

#include "cartridge.h"
#include "cartio.h"
#include "datasette.h"
#include "digiblaster.h"
#include "iecbus.h"
#include "log.h"
#include "maincpu.h"
#include "mem.h"
#include "monitor.h"
#include "plus4iec.h"
#include "plus4cart.h"
#include "plus4mem.h"
#include "plus4memrom.h"
#include "plus4memcsory256k.h"
#include "plus4memhannes256k.h"
#include "plus4memhacks.h"
#include "plus4memlimit.h"
#include "plus4memrom.h"
#include "plus4pio1.h"
#include "plus4pio2.h"
#include "plus4tcbm.h"
#include "ram.h"
#include "resources.h"
#include "tapeport.h"
#include "ted.h"
#include "tedtypes.h"
#include "ted-mem.h"
#include "types.h"
#include "mos6510.h"

/* #define DEBUG_MEM */

#ifdef DEBUG_MEM
#define DBG(x) log_printf x
#else
#define DBG(x)
#endif

static int hard_reset_flag = 1;

/* ------------------------------------------------------------------------- */

/* Number of possible memory configurations.  */
#define NUM_CONFIGS     32

/* The Plus4 memory.  */
uint8_t mem_ram[PLUS4_RAM_SIZE];

/* Pointers to the currently used memory read and write tables.  */
read_func_ptr_t *_mem_read_tab_ptr;
store_func_ptr_t *_mem_write_tab_ptr;
read_func_ptr_t *_mem_read_tab_ptr_dummy;
store_func_ptr_t *_mem_write_tab_ptr_dummy;
static uint8_t **_mem_read_base_tab_ptr;
static int *mem_read_limit_tab_ptr;

/* Memory read and write tables.  */
static store_func_ptr_t mem_write_tab[NUM_CONFIGS][0x101];
static read_func_ptr_t mem_read_tab[NUM_CONFIGS][0x101];
static uint8_t *mem_read_base_tab[NUM_CONFIGS][0x101];
static int mem_read_limit_tab[NUM_CONFIGS][0x101];

static store_func_ptr_t mem_write_tab_watch[0x101];
static read_func_ptr_t mem_read_tab_watch[0x101];

/* Processor port.  */
pport_t pport;

/* Current memory configuration.  */
unsigned int mem_config;

/* ------------------------------------------------------------------------- */

#define RAM0 mem_ram + 0x0000
#define RAM4 mem_ram + 0x4000
#define RAM8 mem_ram + 0x8000
#define RAMC mem_ram + 0xc000

static uint8_t *chargen_base_tab[8][16] = {
    /* 0000-3fff, RAM selected  */
    {       RAM0, RAM0, RAM0, RAM0,
            RAM0, RAM0, RAM0, RAM0,
            RAM0, RAM0, RAM0, RAM0,
            RAM0, RAM0, RAM0, RAM0 },
    /* 4000-7fff, RAM selected  */
    {       RAM4, RAM4, RAM4, RAM4,
            RAM4, RAM4, RAM4, RAM4,
            RAM4, RAM4, RAM4, RAM4,
            RAM4, RAM4, RAM4, RAM4 },
    /* 8000-bfff, RAM selected  */
    {       RAM8, RAM8, RAM8, RAM8,
            RAM8, RAM8, RAM8, RAM8,
            RAM8, RAM8, RAM8, RAM8,
            RAM8, RAM8, RAM8, RAM8 },
    /* c000-ffff, RAM selected  */
    {       RAMC, RAMC, RAMC, RAMC,
            RAMC, RAMC, RAMC, RAMC,
            RAMC, RAMC, RAMC, RAMC,
            RAMC, RAMC, RAMC, RAMC },

    /* 0000-3fff, ROM selected  */
    /* FIXME: this should be "open", ie read the last value from the bus */
    {       RAM0, RAM0, RAM0, RAM0,
            RAM0, RAM0, RAM0, RAM0,
            RAM0, RAM0, RAM0, RAM0,
            RAM0, RAM0, RAM0, RAM0 },
    /* 4000-7fff, ROM selected  */
    /* FIXME: this should be "open", ie read the last value from the bus */
    {       RAM4, RAM4, RAM4, RAM4,
            RAM4, RAM4, RAM4, RAM4,
            RAM4, RAM4, RAM4, RAM4,
            RAM4, RAM4, RAM4, RAM4 },
    /* 8000-bfff, ROM selected  */
    /* FIXME: we cant directly point to cartridge ROM here, we need a better
              (indirect) way to do this */
    {  plus4memrom_basic_rom, extromlo1, extromlo2, extromlo3,
       plus4memrom_basic_rom, extromlo1, extromlo2, extromlo3,
       plus4memrom_basic_rom, extromlo1, extromlo2, extromlo3,
       plus4memrom_basic_rom, extromlo1, extromlo2, extromlo3 },
    /* c000-ffff, ROM selected  */
    {  plus4memrom_kernal_rom, plus4memrom_kernal_rom,
           plus4memrom_kernal_rom, plus4memrom_kernal_rom,
       extromhi1, extromhi1, extromhi1, extromhi1,
       extromhi2, extromhi2, extromhi2, extromhi2,
       extromhi3, extromhi3, extromhi3, extromhi3 }
};

typedef uint8_t *read_base_func_t(unsigned int segment);
typedef read_base_func_t *read_base_func_ptr_t;

/* function(s) to fetch a dynamic base from */
static read_base_func_ptr_t chargen_read_base_tab[8][16] = {
    /* 0000-3fff, RAM selected  */
    {       NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL },
    /* 4000-7fff, RAM selected  */
    {       NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL },
    /* 8000-bfff, RAM selected  */
    {       NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL },
    /* c000-ffff, RAM selected  */
    {       NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL },

    /* 0000-3fff, ROM selected  */
    /* FIXME: this should be "open", ie read the last value from the bus */
    {       NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL },
    /* 4000-7fff, ROM selected  */
    /* FIXME: this should be "open", ie read the last value from the bus */
    {       NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL },
    /* 8000-bfff, ROM selected  */
    /* FIXME: we cant directly point to cartridge ROM here, we need a better
              (indirect) way to do this */
    {  NULL, NULL, plus4cart_get_tedmem_base, NULL,
       NULL, NULL, plus4cart_get_tedmem_base, NULL,
       NULL, NULL, plus4cart_get_tedmem_base, NULL,
       NULL, NULL, plus4cart_get_tedmem_base, NULL },

    /* c000-ffff, ROM selected  */
    {  NULL, NULL, NULL, NULL,
       NULL, NULL, NULL, NULL,
    /* FIXME: we cant directly point to cartridge ROM here, we need a better
              (indirect) way to do this */
       NULL, NULL, NULL, NULL,
       NULL, NULL, NULL, NULL }
};

/*
  segment:
    bit 0-1:    2 upper bits of address
    bit 2:      ROM select (0: RAM, 1: ROM)
*/
uint8_t *mem_get_tedmem_base(unsigned int segment)
{
    uint8_t *base;
    read_base_func_ptr_t basefunc;

    if ((basefunc = chargen_read_base_tab[segment][mem_config >> 1]) == NULL) {
        /* no base read function, use the base tab */
        return chargen_base_tab[segment][mem_config >> 1];
    }

    if ((base = basefunc(segment)) != NULL) {
        return base;
    }

    /* if the base read function returned NULL, use the base tab */
    return chargen_base_tab[segment][mem_config >> 1];
}

/* ------------------------------------------------------------------------- */

/* Tape motor status.  */
static uint8_t old_port_data_out = 0xff;

/* Tape write line status.  */
static uint8_t old_port_write_bit = 0xff;

/* Tape read input.  */
static uint8_t tape_read = 0xff;

static uint8_t tape_write_in = 0xff;
static uint8_t tape_motor_in = 0xff;

/* Current watchpoint state.
          0 = no watchpoints
    bit0; 1 = watchpoints active
    bit1; 2 = watchpoints trigger on dummy accesses
*/
static int watchpoints_active = 0;

inline static void mem_proc_port_store(void)
{
    /*  Correct clock */
    ted_handle_pending_alarms(maincpu_rmw_flag + 1);

    pport.data_out = (pport.data_out & ~pport.dir)
                     | (pport.data & pport.dir);

    if (((~pport.dir | pport.data) & 0x02) != old_port_write_bit) {
        old_port_write_bit = (~pport.dir | pport.data) & 0x02;
        tapeport_toggle_write_bit(TAPEPORT_PORT_1, (~pport.dir | ~pport.data) & 0x02);
    }

    (*iecbus_callback_write)((uint8_t)~pport.data_out, last_write_cycle);

    if (((pport.dir & pport.data) & 0x08) != old_port_data_out) {
        old_port_data_out = (pport.dir & pport.data) & 0x08;
        tapeport_set_motor(TAPEPORT_PORT_1, !old_port_data_out);
    }
}

inline static uint8_t mem_proc_port_read(uint16_t addr)
{
    uint8_t tmp;
    uint8_t input;

    /*  Correct clock */
    ted_handle_pending_alarms(0);

    if (addr == 0) {
        return pport.dir;
    }

    input = ((*iecbus_callback_read)(maincpu_clk) & 0xc0);
    if (tape_read) {
        input |= 0x10;
    } else {
        input &= ~0x10;
    }

    if (tape_write_in) {
        input |= 0x02;
    } else {
        input &= ~0x02;
    }

    if (tape_motor_in) {
        input |= 0x08;
    } else {
        input &= ~0x08;
    }

    tmp = ((input & ~pport.dir) | (pport.data_out & pport.dir)) & 0xdf;

    return tmp;
}

void mem_proc_port_trigger_flux_change(unsigned int on)
{
    /*printf("FLUXCHANGE\n");*/
    tape_read = on;
}

void mem_proc_port_set_write_in(int val)
{
    tape_write_in = val;
}

void mem_proc_port_set_motor_in(int val)
{
    tape_motor_in = val;
}

/* ------------------------------------------------------------------------- */

uint8_t zero_read(uint16_t addr)
{
    addr &= 0xff;

    switch ((uint8_t)addr) {
        case 0:
        case 1:
            ted.last_cpu_val = mem_proc_port_read(addr);
        break;
        default:
            if (!cs256k_enabled) {
                ted.last_cpu_val = mem_ram[addr];
            } else {
                ted.last_cpu_val = cs256k_read(addr);
            }
        break;
    }
    return ted.last_cpu_val;
}

void zero_store(uint16_t addr, uint8_t value)
{
    addr &= 0xff;

    ted.last_cpu_val = value;

    switch ((uint8_t)addr) {
        case 0:
            if (pport.dir != value) {
                pport.dir = value & 0xdf;
                mem_proc_port_store();
            }
            if (!cs256k_enabled) {
                mem_ram[addr] = value;
            } else {
                cs256k_store(addr, value);
            }
            break;
        case 1:
            if (pport.data != value) {
                pport.data = value;
                mem_proc_port_store();
            }
            if (!cs256k_enabled) {
                mem_ram[addr] = value;
            } else {
                cs256k_store(addr, value);
            }
            break;
        default:
            mem_ram[addr] = value;
    }
}

/* ------------------------------------------------------------------------- */

/* called by mem_config_set(), mem_toggle_watchpoints() */
static void mem_update_tab_ptrs(int flag)
{
    if (flag) {
        _mem_read_tab_ptr = mem_read_tab_watch;
        _mem_write_tab_ptr = mem_write_tab_watch;
        if (flag > 1) {
            /* enable watchpoints on dummy accesses */
            _mem_read_tab_ptr_dummy = mem_read_tab_watch;
            _mem_write_tab_ptr_dummy = mem_write_tab_watch;
        } else {
            _mem_read_tab_ptr_dummy = mem_read_tab[mem_config];
            _mem_write_tab_ptr_dummy = mem_write_tab[mem_config];
        }
    } else {
        /* all watchpoints disabled */
        _mem_read_tab_ptr = mem_read_tab[mem_config];
        _mem_write_tab_ptr = mem_write_tab[mem_config];
        _mem_read_tab_ptr_dummy = mem_read_tab[mem_config];
        _mem_write_tab_ptr_dummy = mem_write_tab[mem_config];
    }
}

static void mem_config_set(unsigned int config)
{
    mem_config = config;

    mem_update_tab_ptrs(watchpoints_active);

    _mem_read_base_tab_ptr = mem_read_base_tab[mem_config];
    mem_read_limit_tab_ptr = mem_read_limit_tab[mem_config];

    maincpu_resync_limits();
}

void mem_config_ram_set(unsigned int config)
{
    mem_config_set((mem_config & ~0x01) | config);
}

void mem_config_rom_set(unsigned int config)
{
    mem_config_set((mem_config & ~0x1e) | config);
}

/* ------------------------------------------------------------------------- */

static uint8_t zero_read_watch(uint16_t addr)
{
    addr &= 0xff;
    monitor_watch_push_load_addr(addr, e_comp_space);
    ted.last_cpu_val = mem_read_tab[mem_config][0](addr);
    return ted.last_cpu_val;
}

static void zero_store_watch(uint16_t addr, uint8_t value)
{
    addr &= 0xff;
    ted.last_cpu_val = value;
    monitor_watch_push_store_addr(addr, e_comp_space);
    mem_write_tab[mem_config][0](addr, value);
}

static uint8_t read_watch(uint16_t addr)
{
    monitor_watch_push_load_addr(addr, e_comp_space);
    ted.last_cpu_val = mem_read_tab[mem_config][addr >> 8](addr);
    return ted.last_cpu_val;
}


static void store_watch(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    monitor_watch_push_store_addr(addr, e_comp_space);
    mem_write_tab[mem_config][addr >> 8](addr, value);
}

void mem_toggle_watchpoints(int flag, void *context)
{
    mem_update_tab_ptrs(flag);
    watchpoints_active = flag;
}

/* ------------------------------------------------------------------------- */

static uint8_t ram_read(uint16_t addr)
{
    ted.last_cpu_val = mem_ram[addr];
    return ted.last_cpu_val;
}

static uint8_t ram_read_32k(uint16_t addr)
{
    ted.last_cpu_val = mem_ram[addr & 0x7fff];
    return ted.last_cpu_val;
}

static uint8_t ram_read_16k(uint16_t addr)
{
    ted.last_cpu_val = mem_ram[addr & 0x3fff];
    return ted.last_cpu_val;
}

static void ram_store(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    mem_ram[addr] = value;
}

static void ram_store_32k(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    mem_ram[addr & 0x7fff] = value;
}

static void ram_store_16k(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    mem_ram[addr & 0x3fff] = value;
}

/* ------------------------------------------------------------------------- */

/* Generic memory access.  */

void mem_store(uint16_t addr, uint8_t value)
{
    _mem_write_tab_ptr[addr >> 8](addr, value);
}

uint8_t mem_read(uint16_t addr)
{
    return _mem_read_tab_ptr[addr >> 8](addr);
}

/* this should return the data floating on the bus (CPU reads the bus) */
uint8_t mem_read_open_space(uint16_t addr)
{
    /* FIXME: this is less than correct :) */

    /* YAPE uses this if "clockingState == TDS" */
    unsigned int pc = reg_pc;
    if ((pc ^ addr) & 0xff00) {
        return ted.last_cpu_val;
    }
    return addr >> 8;
}

/* HACK: the following is an ugly hack, which is needed because of how the non-sc
         architecture works. Much of the TED emulation works on pointers that are
         not reassigned/updated on ever access, which would be required to wrap to
         the above function as needed. */

/* NOTE: fortunately only the TED fetching is affected, so the difference made by
         the pattern below is only on the visual result, and can not be detected
         by the CPU in any way - that probably means that "close" is "good enough". */

static uint8_t open_space[64 * 1024];
static int open_space_initizialized = 0;

uint8_t *mem_get_open_space(void)
{
    /* FIXME: this is even lesser than less correct :) */

    int addr;
    if (open_space_initizialized) {
        for (addr = 0; addr < 0x10000; addr++) {
            /* HACK: we can't really produce a "correct" pattern here. So this
                     is just randomly tweaked a little to produce somewhat not
                     completely stupid results in the tests */
            open_space[addr] = ((addr >> 8) ^ 0xaa) ^ (addr ^ 0x55);
            if ((addr & 7) == 0) {
                if ((addr % (40 * 8 * 2)) < (40 * 8)) { open_space[addr] = 0x00; }
                if ((addr % (40 * 8 * 2)) >= (40 * 8)) { open_space[addr] = 0xff; }
            }
        }
    }
    return open_space;
}

/* ------------------------------------------------------------------------- */

/*
    note: the TED pseudo registers at ff3e/3f can't be read.

    FIXME: "You should also note that if RAM is selected (after a write to ff3f)
    the contents of RAM are never visible in these locations (usually seems to
    return FF) Likewise, writing to these locations is never mirrored to
    underlying RAM either. You can prove this quite easily on a stock 16k
    machine, where the memory is mirrored 4x across the entire address space."
*/
static uint8_t h256k_ram_ffxx_read(uint16_t addr)
{
    if ((addr >= 0xff20) && (addr != 0xff3e) && (addr != 0xff3f)) {
        ted.last_cpu_val = h256k_read(addr);
    } else {
        ted.last_cpu_val = ted_read(addr);
    }
    return ted.last_cpu_val;
}

static uint8_t cs256k_ram_ffxx_read(uint16_t addr)
{
    if ((addr >= 0xff20) && (addr != 0xff3e) && (addr != 0xff3f)) {
        ted.last_cpu_val = cs256k_read(addr);
    } else {
        ted.last_cpu_val = ted_read(addr);
    }
    return ted.last_cpu_val;
}

static uint8_t ram_ffxx_read(uint16_t addr)
{
    if ((addr >= 0xff20) && (addr != 0xff3e) && (addr != 0xff3f)) {
        ted.last_cpu_val = ram_read(addr);
    } else {
        ted.last_cpu_val = ted_read(addr);
    }
    return ted.last_cpu_val;
}

static uint8_t ram_ffxx_read_32k(uint16_t addr)
{
    if ((addr >= 0xff20) && (addr != 0xff3e) && (addr != 0xff3f)) {
        ted.last_cpu_val = ram_read_32k(addr);
    } else {
        ted.last_cpu_val = ted_read(addr);
    }
    return ted.last_cpu_val;
}

static uint8_t ram_ffxx_read_16k(uint16_t addr)
{
    if ((addr >= 0xff20) && (addr != 0xff3e) && (addr != 0xff3f)) {
        ted.last_cpu_val = ram_read_16k(addr);
    } else {
        ted.last_cpu_val = ted_read(addr);
    }
    return ted.last_cpu_val;
}


static void h256k_ram_ffxx_store(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        h256k_store(addr, value);
    }
}

static void cs256k_ram_ffxx_store(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        cs256k_store(addr, value);
    }
}

static void ram_ffxx_store(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        ram_store(addr, value);
    }
}

static void ram_ffxx_store_32k(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        ram_store_32k(addr, value);
    }
}

static void ram_ffxx_store_16k(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        ram_store_16k(addr, value);
    }
}
/*
    If the ROM is currently visible, ie after a write to ff3e, then reading ff3e
    and ff3f returns the contents of the underlying ROM, exactly as is does with
    ff20 - ff3d.
*/
static uint8_t rom_ffxx_read(uint16_t addr)
{
    if (addr >= 0xff20) {
        ted.last_cpu_val = plus4memrom_rom_read(addr);
    } else {
        ted.last_cpu_val = ted_read(addr);
    }
    return ted.last_cpu_val;
}

static void rom_ffxx_store(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        ram_store(addr, value);
    }
}

static void h256k_rom_ffxx_store(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        h256k_store(addr, value);
    }
}

static void cs256k_rom_ffxx_store(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        cs256k_store(addr, value);
    }
}

static void rom_ffxx_store_32k(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        ram_store_32k(addr, value);
    }
}

static void rom_ffxx_store_16k(uint16_t addr, uint8_t value)
{
    ted.last_cpu_val = value;
    if (addr < 0xff20 || addr == 0xff3e || addr == 0xff3f) {
        ted_store(addr, value);
    } else {
        ram_store_16k(addr, value);
    }
}

/* ------------------------------------------------------------------------- */

static void set_write_hook(int config, int page, store_func_t *f)
{
    mem_write_tab[config][page] = f;
}

void mem_initialize_memory(void)
{
    int i, j;
    int ram_size;

    if (resources_get_int("RamSize", &ram_size) < 0) {
        return;
    }

    switch (ram_size) {
        default:
        case 256:
        case 64:
            for (i = 0; i < 16; i++) {
                chargen_base_tab[1][i] = RAM4;
                chargen_base_tab[2][i] = RAM8;
                chargen_base_tab[3][i] = RAMC;
                chargen_base_tab[5][i] = RAM4;
            }
            break;
        case 32:
            for (i = 0; i < 16; i++) {
                chargen_base_tab[1][i] = RAM4;
                chargen_base_tab[2][i] = RAM0;
                chargen_base_tab[3][i] = RAM4;
                chargen_base_tab[5][i] = RAM4;
            }
            break;
        case 16:
            for (i = 0; i < 16; i++) {
                chargen_base_tab[1][i] = RAM0;
                chargen_base_tab[2][i] = RAM0;
                chargen_base_tab[3][i] = RAM0;
                chargen_base_tab[5][i] = RAM0;
            }
            break;
    }

    mem_limit_init(mem_read_limit_tab);

    /* setup watchpoint tables */
    mem_read_tab_watch[0] = zero_read_watch;
    mem_write_tab_watch[0] = zero_store_watch;
    for (i = 1; i <= 0x100; i++) {
        mem_read_tab_watch[i] = read_watch;
        mem_write_tab_watch[i] = store_watch;
    }

    /* Default is RAM.  */
    for (i = 0; i < NUM_CONFIGS; i++) {
        set_write_hook(i, 0, zero_store);
        mem_read_tab[i][0] = zero_read;
        mem_read_base_tab[i][0] = mem_ram;
        for (j = 1; j <= 0xff; j++) {
            switch (ram_size) {
                case 4096:
                case 1024:
                case 256:
                    if (h256k_enabled && j < 0x10) {
                        mem_read_tab[i][j] = ram_read;
                        mem_write_tab[i][j] = ted_mem_vbank_store;
                    }
                    if (h256k_enabled && j >= 0x10) {
                        mem_read_tab[i][j] = h256k_read;
                        mem_write_tab[i][j] = h256k_store;
                    }
                    if (cs256k_enabled) {
                        mem_read_tab[i][j] = cs256k_read;
                        mem_write_tab[i][j] = cs256k_store;
                    }
                    mem_read_base_tab[i][j] = mem_ram + (j << 8);
                    break;
                default:
                case 64:
                    mem_read_tab[i][j] = ram_read;
                    mem_read_base_tab[i][j] = mem_ram + (j << 8);
                    mem_write_tab[i][j] = ted_mem_vbank_store;
                    break;
                case 32:
                    mem_read_tab[i][j] = ram_read_32k;
                    mem_read_base_tab[i][j] = mem_ram + ((j & 0x7f) << 8);
                    mem_write_tab[i][j] = ted_mem_vbank_store_32k;
                    break;
                case 16:
                    mem_read_tab[i][j] = ram_read_16k;
                    mem_read_base_tab[i][j] = mem_ram + ((j & 0x3f) << 8);
                    mem_write_tab[i][j] = ted_mem_vbank_store_16k;
                    break;
            }
#if 0
            if ((j & 0xc0) == (k << 6)) {
                switch (j & 0x3f) {
                    case 0x39:
                        mem_write_tab[i][j] = ted_mem_vbank_39xx_store;
                        break;
                    case 0x3f:
                        mem_write_tab[i][j] = ted_mem_vbank_3fxx_store;
                        break;
                    default:
                        mem_write_tab[i][j] = ted_mem_vbank_store;
                }
            } else {
#endif
#if 0
        }
#endif
        }
#if 0
        mem_read_tab[i][0xff] = ram_read;
        mem_read_base_tab[i][0xff] = ram + 0xff00;
        set_write_hook(i, 0xff, ram_store);
#endif
    }

    /* Setup BASIC ROM and extension ROMs at $8000-$BFFF.  */
    for (i = 0x80; i <= 0xbf; i++) {
        mem_read_tab[1][i] = plus4memrom_basic_read;
        mem_read_base_tab[1][i] = plus4memrom_basic_rom + ((i & 0x3f) << 8);
        mem_read_tab[3][i] = plus4memrom_extromlo1_read;
        mem_read_base_tab[3][i] = extromlo1 + ((i & 0x3f) << 8);
        mem_read_tab[5][i] = plus4cart_c1lo_read;
        /*mem_read_base_tab[5][i] = extromlo2 + ((i & 0x3f) << 8);*/
        mem_read_base_tab[5][i] = NULL;
        mem_read_tab[7][i] = plus4memrom_extromlo3_read;
        mem_read_base_tab[7][i] = extromlo3 + ((i & 0x3f) << 8);

        mem_read_tab[9][i] = plus4memrom_basic_read;
        mem_read_base_tab[9][i] = plus4memrom_basic_rom + ((i & 0x3f) << 8);
        mem_read_tab[11][i] = plus4memrom_extromlo1_read;
        mem_read_base_tab[11][i] = extromlo1 + ((i & 0x3f) << 8);
        mem_read_tab[13][i] = plus4cart_c1lo_read;
        /*mem_read_base_tab[13][i] = extromlo2 + ((i & 0x3f) << 8);*/
        mem_read_base_tab[13][i] = NULL;
        mem_read_tab[15][i] = plus4memrom_extromlo3_read;
        mem_read_base_tab[15][i] = extromlo3 + ((i & 0x3f) << 8);

        mem_read_tab[17][i] = plus4memrom_basic_read;
        mem_read_base_tab[17][i] = plus4memrom_basic_rom + ((i & 0x3f) << 8);
        mem_read_tab[19][i] = plus4memrom_extromlo1_read;
        mem_read_base_tab[19][i] = extromlo1 + ((i & 0x3f) << 8);
        mem_read_tab[21][i] = plus4cart_c1lo_read;
        /*mem_read_base_tab[21][i] = extromlo2 + ((i & 0x3f) << 8);*/
        mem_read_base_tab[21][i] = NULL;
        mem_read_tab[23][i] = plus4memrom_extromlo3_read;
        mem_read_base_tab[23][i] = extromlo3 + ((i & 0x3f) << 8);

        mem_read_tab[25][i] = plus4memrom_basic_read;
        mem_read_base_tab[25][i] = plus4memrom_basic_rom + ((i & 0x3f) << 8);
        mem_read_tab[27][i] = plus4memrom_extromlo1_read;
        mem_read_base_tab[27][i] = extromlo1 + ((i & 0x3f) << 8);
        mem_read_tab[29][i] = plus4cart_c1lo_read;
        /*mem_read_base_tab[29][i] = extromlo2 + ((i & 0x3f) << 8);*/
        mem_read_base_tab[29][i] = NULL;
        mem_read_tab[31][i] = plus4memrom_extromlo3_read;
        mem_read_base_tab[31][i] = extromlo3 + ((i & 0x3f) << 8);
    }

    /* Setup Kernal ROM and extension ROMs at $E000-$FFFF.  */
    for (i = 0xc0; i <= 0xff; i++) {
#if 0
        mem_read_tab[1][i] = plus4memrom_kernal_read;
        mem_read_base_tab[1][i] = plus4memrom_kernal_trap_rom + ((i & 0x3f) << 8);
        mem_read_tab[3][i] = plus4memrom_kernal_read;
        mem_read_base_tab[3][i] = plus4memrom_kernal_trap_rom + ((i & 0x3f) << 8);
        mem_read_tab[5][i] = plus4memrom_kernal_read;
        mem_read_base_tab[5][i] = plus4memrom_kernal_trap_rom + ((i & 0x3f) << 8);
        mem_read_tab[7][i] = plus4memrom_kernal_read;
        mem_read_base_tab[7][i] = plus4memrom_kernal_trap_rom + ((i & 0x3f) << 8);
#else
        /* redirect kernal to cartridge port */
        mem_read_tab[1][i] = plus4cart_kernal_read;
        mem_read_base_tab[1][i] = plus4memrom_kernal_trap_rom + ((i & 0x3f) << 8);
        mem_read_tab[3][i] = plus4cart_kernal_read;
        mem_read_base_tab[3][i] = plus4memrom_kernal_trap_rom + ((i & 0x3f) << 8);
        mem_read_tab[5][i] = plus4cart_kernal_read;
        mem_read_base_tab[5][i] = plus4memrom_kernal_trap_rom + ((i & 0x3f) << 8);
        mem_read_tab[7][i] = plus4cart_kernal_read;
        mem_read_base_tab[7][i] = plus4memrom_kernal_trap_rom + ((i & 0x3f) << 8);
#endif
        mem_read_tab[9][i] = plus4memrom_extromhi1_read;
        mem_read_base_tab[9][i] = extromhi1 + ((i & 0x3f) << 8);
        mem_read_tab[11][i] = plus4memrom_extromhi1_read;
        mem_read_base_tab[11][i] = extromhi1 + ((i & 0x3f) << 8);
        mem_read_tab[13][i] = plus4memrom_extromhi1_read;
        mem_read_base_tab[13][i] = extromhi1 + ((i & 0x3f) << 8);
        mem_read_tab[15][i] = plus4memrom_extromhi1_read;
        mem_read_base_tab[15][i] = extromhi1 + ((i & 0x3f) << 8);

        mem_read_tab[17][i] = plus4cart_c1hi_read;
        /*mem_read_base_tab[17][i] = extromhi2 + ((i & 0x3f) << 8);*/
        mem_read_base_tab[17][i] = NULL;
        mem_read_tab[19][i] = plus4cart_c1hi_read;
        /*mem_read_base_tab[19][i] = extromhi2 + ((i & 0x3f) << 8);*/
        mem_read_base_tab[19][i] = NULL;
        mem_read_tab[21][i] = plus4cart_c1hi_read;
        /*mem_read_base_tab[21][i] = extromhi2 + ((i & 0x3f) << 8);*/
        mem_read_base_tab[21][i] = NULL;
        mem_read_tab[23][i] = plus4cart_c1hi_read;
        /*mem_read_base_tab[23][i] = extromhi2 + ((i & 0x3f) << 8);*/
        mem_read_base_tab[23][i] = NULL;

        mem_read_tab[25][i] = plus4memrom_extromhi3_read;
        mem_read_base_tab[25][i] = extromhi3 + ((i & 0x3f) << 8);
        mem_read_tab[27][i] = plus4memrom_extromhi3_read;
        mem_read_base_tab[27][i] = extromhi3 + ((i & 0x3f) << 8);
        mem_read_tab[29][i] = plus4memrom_extromhi3_read;
        mem_read_base_tab[29][i] = extromhi3 + ((i & 0x3f) << 8);
        mem_read_tab[31][i] = plus4memrom_extromhi3_read;
        mem_read_base_tab[31][i] = extromhi3 + ((i & 0x3f) << 8);
    }

    for (i = 0; i < NUM_CONFIGS; i += 2) {
        mem_read_tab[i + 1][0xfc] = plus4memrom_kernal_read;
        mem_read_base_tab[i + 1][0xfc] = plus4memrom_kernal_trap_rom
                                         + ((0xfc & 0x3f) << 8);

        mem_read_tab[i + 0][0xfd] = plus4io_fd00_read;
        mem_write_tab[i + 0][0xfd] = plus4io_fd00_store;
        mem_read_base_tab[i + 0][0xfd] = NULL;
        mem_read_tab[i + 1][0xfd] = plus4io_fd00_read;
        mem_write_tab[i + 1][0xfd] = plus4io_fd00_store;
        mem_read_base_tab[i + 1][0xfd] = NULL;

        mem_read_tab[i + 0][0xfe] = plus4io_fe00_read;
        mem_write_tab[i + 0][0xfe] = plus4io_fe00_store;
        mem_read_base_tab[i + 0][0xfe] = NULL;
        mem_read_tab[i + 1][0xfe] = plus4io_fe00_read;
        mem_write_tab[i + 1][0xfe] = plus4io_fe00_store;
        mem_read_base_tab[i + 1][0xfe] = NULL;

        switch (ram_size) {
            case 4096:
            case 1024:
            case 256:
                if (h256k_enabled) {
                    mem_read_tab[i + 0][0xff] = h256k_ram_ffxx_read;
                    mem_write_tab[i + 0][0xff] = h256k_ram_ffxx_store;
                    mem_write_tab[i + 1][0xff] = h256k_rom_ffxx_store;
                }
                if (cs256k_enabled) {
                    mem_read_tab[i + 0][0xff] = cs256k_ram_ffxx_read;
                    mem_write_tab[i + 0][0xff] = cs256k_ram_ffxx_store;
                    mem_write_tab[i + 1][0xff] = cs256k_rom_ffxx_store;
                }
                mem_read_base_tab[i + 0][0xff] = NULL;
                mem_read_tab[i + 1][0xff] = rom_ffxx_read;
                mem_read_base_tab[i + 1][0xff] = NULL;
                break;
            default:
            case 64:
                mem_read_tab[i + 0][0xff] = ram_ffxx_read;
                mem_write_tab[i + 0][0xff] = ram_ffxx_store;
                mem_read_base_tab[i + 0][0xff] = NULL;
                mem_read_tab[i + 1][0xff] = rom_ffxx_read;
                mem_write_tab[i + 1][0xff] = rom_ffxx_store;
                mem_read_base_tab[i + 1][0xff] = NULL;
                break;
            case 32:
                mem_read_tab[i + 0][0xff] = ram_ffxx_read_32k;
                mem_write_tab[i + 0][0xff] = ram_ffxx_store_32k;
                mem_read_base_tab[i + 0][0xff] = NULL;
                mem_read_tab[i + 1][0xff] = rom_ffxx_read;
                mem_write_tab[i + 1][0xff] = rom_ffxx_store_32k;
                mem_read_base_tab[i + 1][0xff] = NULL;
                break;
            case 16:
                mem_read_tab[i + 0][0xff] = ram_ffxx_read_16k;
                mem_write_tab[i + 0][0xff] = ram_ffxx_store_16k;
                mem_read_base_tab[i + 0][0xff] = NULL;
                mem_read_tab[i + 1][0xff] = rom_ffxx_read;
                mem_write_tab[i + 1][0xff] = rom_ffxx_store_16k;
                mem_read_base_tab[i + 1][0xff] = NULL;
                break;
        }

        mem_read_tab[i + 0][0x100] = mem_read_tab[i + 0][0];
        mem_write_tab[i + 0][0x100] = mem_write_tab[i + 0][0];
        mem_read_base_tab[i + 0][0x100] = mem_read_base_tab[i + 0][0];
        mem_read_tab[i + 1][0x100] = mem_read_tab[i + 1][0];
        mem_write_tab[i + 1][0x100] = mem_write_tab[i + 1][0];
        mem_read_base_tab[i + 1][0x100] = mem_read_base_tab[i + 1][0];
    }
    if (hard_reset_flag) {
        hard_reset_flag = 0;
        mem_config = 1;
    }
    _mem_read_tab_ptr = mem_read_tab[mem_config];
    _mem_write_tab_ptr = mem_write_tab[mem_config];
    _mem_read_base_tab_ptr = mem_read_base_tab[mem_config];
    mem_read_limit_tab_ptr = mem_read_limit_tab[mem_config];
}

void mem_mmu_translate(unsigned int addr, uint8_t **base, int *start, int *limit)
{
    uint8_t *p = _mem_read_base_tab_ptr[addr >> 8];

    if (p != NULL && addr > 1) {
        *base = (p - (addr & 0xff00));
        *start = addr; /* TODO */
        *limit = mem_read_limit_tab_ptr[addr >> 8];
    } else {
        cartridge_mmu_translate(addr, base, start, limit);
    }
}

/* ------------------------------------------------------------------------- */

/* Initialize RAM for power-up.  */
void mem_powerup(void)
{
    ram_init(mem_ram, 0x10000);

    hard_reset_flag = 1;
}

/* ------------------------------------------------------------------------- */

/* FIXME: this part needs to be checked.  */

void mem_get_basic_text(uint16_t *start, uint16_t *end)
{
    if (start != NULL) {
        *start = mem_ram[0x2b] | (mem_ram[0x2c] << 8);
    }
    if (end != NULL) {
        *end = mem_ram[0x2d] | (mem_ram[0x2e] << 8);
    }
}

void mem_set_basic_text(uint16_t start, uint16_t end)
{
    mem_ram[0x2b] = mem_ram[0xac] = start & 0xff;
    mem_ram[0x2c] = mem_ram[0xad] = start >> 8;
    mem_ram[0x2d] = mem_ram[0x2f] = mem_ram[0x31] = mem_ram[0xae] = end & 0xff;
    mem_ram[0x2e] = mem_ram[0x30] = mem_ram[0x32] = mem_ram[0xaf] = end >> 8;
}

/* this function should always read from the screen currently used by the kernal
   for output, normally this does just return system ram - except when the
   videoram is not memory mapped.
   used by autostart to "read" the kernal messages
*/
uint8_t mem_read_screen(uint16_t addr)
{
    return ram_read(addr);
}

void mem_inject(uint32_t addr, uint8_t value)
{
    /* printf("mem_inject addr: %04x  value: %02x\n", addr, value); */
    if (!plus4_memory_hacks_ram_inject(addr, value)) {
        /* just call mem_store() to be safe.
           This could possibly be changed to write straight into the
           memory array.  mem_ram[addr & mask] = value; */
        mem_store((uint16_t)(addr & 0xffff), value);
    }
}

/* In banked memory architectures this will always write to the bank that
   contains the keyboard buffer and "number of keys in buffer", regardless of
   what the CPU "sees" currently.
   In all other cases this just writes to the first 64kb block, usually by
   wrapping to mem_inject().
*/
void mem_inject_key(uint16_t addr, uint8_t value)
{
    mem_inject(addr, value);
}

/* ------------------------------------------------------------------------- */

int mem_rom_trap_allowed(uint16_t addr)
{
    return addr >= 0x8000 && (mem_config & 0x1);
}

/* ------------------------------------------------------------------------- */

/* Exported banked memory access functions for the monitor.  */
#define MAXBANKS (8)

static const char *banknames[MAXBANKS + 1] = {
    "default",
    "cpu",
    "ram",
    "rom",
    "io",
    "funcrom",
    "cart1rom",
    "cart2rom",
    /* by convention, a "bank array" has a 2-hex-digit bank index appended */
    NULL
};

static const int banknums[MAXBANKS + 1] = { 0, 0, 1, 2, 6, 3, 4, 5, -1 };
static const int bankindex[MAXBANKS + 1] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static const int bankflags[MAXBANKS + 1] = { 0, 0, 0, 0, 0, 0, 0, 0, -1 };

const char **mem_bank_list(void)
{
    return banknames;
}

const int *mem_bank_list_nos(void) {
    return banknums;
}

/* return bank number for a given literal bank name */
int mem_bank_from_name(const char *name)
{
    int i = 0;

    while (banknames[i]) {
        if (!strcmp(name, banknames[i])) {
            return banknums[i];
        }
        i++;
    }
    return -1;
}

/* return current index for a given bank */
int mem_bank_index_from_bank(int bank)
{
    int i = 0;

    while (banknums[i] > -1) {
        if (banknums[i] == bank) {
            return bankindex[i];
        }
        i++;
    }
    return -1;
}

int mem_bank_flags_from_bank(int bank)
{
    int i = 0;

    while (banknums[i] > -1) {
        if (banknums[i] == bank) {
            return bankflags[i];
        }
        i++;
    }
    return -1;
}

void store_bank_io(uint16_t addr, uint8_t byte)
{
    if (addr >= 0xfd00 && addr <= 0xfdff) {
        plus4io_fd00_store(addr, byte);
    }

    if (addr >= 0xfe00 && addr <= 0xfeff) {
        plus4io_fe00_store(addr, byte);
    }

    if ((addr >= 0xff00) && (addr <= 0xff3f)) {
        ted_store(addr, byte);
    } else {
        mem_store(addr, byte);
    }
}

/* read i/o without side-effects */
static uint8_t peek_bank_io(uint16_t addr)
{
    if ((addr >= 0xff00) && (addr <= 0xff3f)) {
        return ted_peek(addr);
    }

    if (addr >= 0xfd00 && addr <= 0xfdff) {
        return plus4io_fd00_peek(addr);
    }

    if (addr >= 0xfe00 && addr <= 0xfeff) {
        return plus4io_fe00_peek(addr);
    }
    return mem_read_open_space(addr);
}

/* read i/o with side-effects */
static uint8_t read_bank_io(uint16_t addr)
{
    if ((addr >= 0xff00) && (addr <= 0xff3f)) {
        return ted_peek(addr);
    }

    if (addr >= 0xfd00 && addr <= 0xfdff) {
        return plus4io_fd00_read(addr);
    }

    if (addr >= 0xfe00 && addr <= 0xfeff) {
        return plus4io_fe00_read(addr);
    }

    return mem_read_open_space(addr);
}

/* read memory without side-effects */
uint8_t mem_bank_peek(int bank, uint16_t addr, void *context)
{
    switch (bank) {
        case 0:                   /* current */
            /* FIXME: we must check for which bank is currently active, and only use peek_bank_io
                      when needed. doing this without checking is wrong, but we do it anyways to
                      avoid side effects
           */
            /*
                $0000-$7fff   RAM
                $8000-$9fff   RAM / BASIC / Function LO
                $a000-$bfff   RAM / Kernal / Function HI

                $c000-$cfff   RAM / Basic Extension

                $d000-$d7ff   RAM / character ROM / Function HI
                $d800-$fbff   RAM / operating system

                $FC00-        Kernal Routines for switching banks

                $FD00-$FF3F always I/O:

                    $FD00-FD0F: 6551  (only on the +4.  4 registers.)
                    $FD10-FD1F: 6529B (1 register)
                    $FD30-FD3F: 6529B (1 register)

                $FDD0-$FDDF ROM bank select

                    a0 a1 bank
                    0  0  BASIC (low internal #1)
                    0  1  Function LO (low internal #2)
                    1  0  Cartridge LO (low external #1)
                    1  1  reserved

                    a2 a3 bank
                    0  0  Kernal (hi internal #1)
                    0  1  Function HI (hi internal #2)
                    1  0  Cartridge HI (hi external #1)
                    1  1  reserved

                    fdd0 "BASIC",  "KERNAL"
                    fdd1 "3+1",    "KERNAL"
                    fdd2 "CART-1", "KERNAL"
                    fdd3 "CART-2", "KERNAL"
                    fdd4 "BASIC",  "3+1"
                    fdd5 "3+1",    "3+1"
                    fdd6 "CART-1", "3+1"
                    fdd7 "CART-2", "3+1"
                    fdd8 "BASIC",  "CART-1"
                    fdd9 "3+1",    "CART-1"
                    fdda "CART-1", "CART-1"
                    fddb "CART-2", "CART-1"
                    fddc "BASIC",  "CART-2"
                    fddd "3+1",    "CART-2"
                    fdde "CART-1", "CART-2"
                    fddf "CART-2", "CART-2"

                $FF00-  TED registers

                $FF3E   ROM select, Write switches on ROM bank
                $FF3F   RAM select, Write switches on RAM bank

                $FF40-$FFFF RAM / Kernal / Function HI
            */
            if ((addr >= 0xfd00) && (addr <= 0xfd3f)) {
                return peek_bank_io(addr);
            }
            if ((addr >= 0xff00) && (addr <= 0xff3f)) {
                return peek_bank_io(addr);
            }
            break;
        case 6:                   /* io */
            if (addr >= 0xfd00) {
                return peek_bank_io(addr);
            }
            break;
    }

    return mem_bank_read(bank, addr, context); /* FIXME */
}

int mem_get_current_bank_config(void) {
    return 0; /* TODO: not implemented yet */
}

uint8_t mem_peek_with_config(int config, uint16_t addr, void *context) {
    /* TODO, config not implemented yet */
    return mem_bank_peek(0 /* current */, addr, context);
}

/* read memory with side-effects */
uint8_t mem_bank_read(int bank, uint16_t addr, void *context)
{
    switch (bank) {
        case 0:                   /* current */
            return mem_read(addr);
            break;
        case 1:                   /* ram */
            break;
        case 2:                   /* rom */
            if (addr >= 0x8000 && addr <= 0xbfff) {
                return plus4memrom_basic_rom[addr & 0x3fff];
            }
            if (addr >= 0xc000) {
                return plus4memrom_kernal_rom[addr & 0x3fff];
            }
            break;
        case 3:                   /* funcrom */
            if (addr >= 0x8000 && addr <= 0xbfff) {
                return extromlo1[addr & 0x3fff];
            }
            if (addr >= 0xc000) {
                return extromhi1[addr & 0x3fff];
            }
            break;
        case 4:                   /* cart1rom */
            if (addr >= 0x8000 && addr <= 0xbfff) {
                /* return extromlo2[addr & 0x3fff]; */
                return plus4cart_c1lo_read(addr);
            }
            if (addr >= 0xc000) {
                /* return extromhi2[addr & 0x3fff]; */
                return plus4cart_c1hi_read(addr);
            }
            break;
        case 5:                   /* cart2rom */
            if (addr >= 0x8000 && addr <= 0xbfff) {
                return extromlo3[addr & 0x3fff];
            }
            if (addr >= 0xc000) {
                return extromhi3[addr & 0x3fff];
            }
            break;
        case 6:                   /* i/o */
            if (addr >= 0xfd00) {
                return read_bank_io(addr);
            }
            return mem_read(addr);
            break;
    }
    return mem_ram[addr];
}

void mem_bank_write(int bank, uint16_t addr, uint8_t byte, void *context)
{
    switch (bank) {
        case 0:                 /* current */
            mem_store(addr, byte);
            return;
        case 1:                 /* ram */
            break;
        case 2:                 /* rom */
            if (addr >= 0x8000 && addr <= 0xbfff) {
                return;
            }
            if (addr >= 0xc000) {
                return;
            }
            break;
        case 3:                 /* funcrom */
            if (addr >= 0x8000 && addr <= 0xbfff) {
                return;
            }
            if (addr >= 0xc000) {
                return;
            }
            break;
        case 4:                 /* cart1rom */
            if (addr >= 0x8000 && addr <= 0xbfff) {
                return;
            }
            if (addr >= 0xc000) {
                return;
            }
            break;
        case 5:                 /* cart2rom */
            if (addr >= 0x8000 && addr <= 0xbfff) {
                return;
            }
            if (addr >= 0xc000) {
                return;
            }
            break;
        case 6:                 /* i/o */
            store_bank_io(addr, byte);
            return;
    }
    mem_ram[addr] = byte;
}

/* used by monitor if sfx off */
void mem_bank_poke(int bank, uint16_t addr, uint8_t byte, void *context)
{
    mem_bank_write(bank, addr, byte, context);
}

static int mem_dump_io(void *context, uint16_t addr)
{
    if ((addr >= 0xff00) && (addr <= 0xff3f)) {
        return ted_dump();
    }
    return -1;
}

mem_ioreg_list_t *mem_ioreg_list_get(void *context)
{
    mem_ioreg_list_t *mem_ioreg_list = NULL;

    io_source_ioreg_add_list(&mem_ioreg_list);
    mon_ioreg_add_list(&mem_ioreg_list, "TED", 0xff00, 0xff3f, mem_dump_io, NULL, IO_MIRROR_NONE);

    return mem_ioreg_list;
}

void mem_get_screen_parameter(uint16_t *base, uint8_t *rows, uint8_t *columns, int *bank)
{
    *base = (ted_peek(0xff14) & 0xf8) << 8 | 0x400;
    *rows = 25;
    *columns = 40;
    *bank = 0;
}

/* used by autostart to locate and "read" kernal output on the current screen
 * this function should return whatever the kernal currently uses, regardless
 * what is currently visible/active in the UI
 */
void mem_get_cursor_parameter(uint16_t *screen_addr, uint8_t *cursor_column, uint8_t *line_length, int *blinking)
{
    unsigned int cursorposition = (ted_peek(0xff0d) + ((ted_peek(0xff0c) & 3) * 256));
    unsigned int screenbase = ((ted_peek(0xff14) & 0xf8) << 8 | 0x400);

    *line_length = 40;                 /* Physical Screen Line Length */

    if (cursorposition < 1000) {
        /* cursor position < 1000 means the cursor is visible/enabled, in
           this case we will derive screen line and column from the position
           of the hardware cursor */
        *cursor_column = cursorposition % 40; /* Cursor Column on Current Line */
        *screen_addr = screenbase + cursorposition - *cursor_column; /* Current Screen Line Address */
        /* Cursor Blink enable: 1 = Flash Cursor, 0 = Cursor disabled, -1 = n/a */
        *blinking = 1;
    } else {
        /* when cursor is disabled, we use the kernal pointers and hope they
           are not completely wrong */
        *screen_addr = mem_ram[0xc8] + mem_ram[0xc9] * 256; /* Current Screen Line Address */
        *cursor_column = mem_ram[0xca];    /* Cursor Column on Current Line */
        *blinking = 0;
    }
    /* printf("mem_get_cursor_parameter screen_addr:%04x column: %d line len: %d blinking:%d\n",
           *screen_addr, *cursor_column, *line_length, *blinking); */
}

/* ------------------------------------------------------------------------- */

typedef struct mem_config_s {
    char *mem_8000;
    char *mem_c000;
} mem_config_t;

static mem_config_t mem_config_table[] = {
    { "BASIC",  "KERNAL" }, /* 0xfdd0 */
    { "3+1",    "KERNAL" }, /* 0xfdd1 */
    { "CART-1", "KERNAL" }, /* 0xfdd2 */
    { "CART-2", "KERNAL" }, /* 0xfdd3 */
    { "BASIC",  "3+1"    }, /* 0xfdd4 */
    { "3+1",    "3+1"    }, /* 0xfdd5 */
    { "CART-1", "3+1"    }, /* 0xfdd6 */
    { "CART-2", "3+1"    }, /* 0xfdd7 */
    { "BASIC",  "CART-1" }, /* 0xfdd8 */
    { "3+1",    "CART-1" }, /* 0xfdd9 */
    { "CART-1", "CART-1" }, /* 0xfdda */
    { "CART-2", "CART-1" }, /* 0xfddb */
    { "BASIC",  "CART-2" }, /* 0xfddc */
    { "3+1",    "CART-2" }, /* 0xfddd */
    { "CART-1", "CART-2" }, /* 0xfdde */
    { "CART-2", "CART-2" }  /* 0xfddf */
};

static void mem_config_rom_set_store(uint16_t addr, uint8_t value)
{
    mem_config_rom_set((addr & 0xf) << 1);
}

static int memconfig_dump(void)
{
    mon_out("$8000-$BFFF: %s\n", (mem_config & 1) ? mem_config_table[mem_config >> 1].mem_8000 : "RAM");
    mon_out("$C000-$FFFF: %s\n", (mem_config & 1) ? mem_config_table[mem_config >> 1].mem_c000 : "RAM");

    return 0;
}

static io_source_t mem_config_device = {
    "MEMCONFIG",              /* name of the chip */
    IO_DETACH_NEVER,          /* chip is never involved in collisions, so no detach */
    IO_DETACH_NO_RESOURCE,    /* does not use a resource for detach */
    0xfdd0, 0xfddf, 0x0f,     /* range of the device, regs:$fdd0-$fddf */
    0,                        /* read is never valid */
    mem_config_rom_set_store, /* store function */
    NULL,                     /* NO poke function */
    NULL,                     /* NO read function */
    NULL,                     /* NO peek function */
    memconfig_dump,           /* chip state information dump function */
    IO_CART_ID_NONE,          /* not a cartridge */
    IO_PRIO_NORMAL,           /* normal priority, device read needs to be checked for collisions */
    0,                        /* insertion order, gets filled in by the registration function */
    IO_MIRROR_NONE            /* NO mirroring */
};

/* ------------------------------------------------------------------------- */

static io_source_t pio1_with_mirrors_device = {
    "PIO1",                /* name of the chip */
    IO_DETACH_NEVER,       /* chip is never involved in collisions, so no detach */
    IO_DETACH_NO_RESOURCE, /* does not use a resource for detach */
    0xfd10, 0xfd1f, 0x00,  /* range for the device, reg:$fd10, mirrors:$fd11-$fd1f */
    1,                     /* read is always valid */
    pio1_store,            /* store function */
    NULL,                  /* NO poke function */
    pio1_read,             /* read function */
    NULL,                  /* TODO: peek function */
    NULL,                  /* nothing to dump */
    IO_CART_ID_NONE,       /* not a cartridge */
    IO_PRIO_NORMAL,        /* normal priority, device read needs to be checked for collisions */
    0,                     /* insertion order, gets filled in by the registration function */
    IO_MIRROR_NONE         /* NO mirroring */
};

static io_source_t pio1_only_device = {
    "PIO1",                /* name of the chip */
    IO_DETACH_NEVER,       /* chip is never involved in collisions, so no detach */
    IO_DETACH_NO_RESOURCE, /* does not use a resource for detach */
    0xfd10, 0xfd10, 0x00,  /* range for the device, reg:$fd10 */
    1,                     /* read is always valid */
    pio1_store,            /* store function */
    NULL,                  /* NO poke function */
    pio1_read,             /* read function */
    NULL,                  /* TODO: peek function */
    NULL,                  /* nothing to dump */
    IO_CART_ID_NONE,       /* not a cartridge */
    IO_PRIO_NORMAL,        /* normal priority, device read needs to be checked for collisions */
    0,                     /* insertion order, gets filled in by the registration function */
    IO_MIRROR_NONE         /* NO mirroring */
};

static io_source_t pio2_device = {
    "PIO2",                /* name of the chip */
    IO_DETACH_NEVER,       /* chip is never involved in collisions, so no detach */
    IO_DETACH_NO_RESOURCE, /* does not use a resource for detach */
    0xfd30, 0xfd3f, 0x00,  /* range for the device, reg:$fd30, mirrors:$fd31-$fd3f */
    1,                     /* read is always valid */
    pio2_store,            /* store function */
    NULL,                  /* NO poke function */
    pio2_read,             /* read function */
    NULL,                  /* TODO: peek function */
    NULL,                  /* nothing to dump */
    IO_CART_ID_NONE,       /* not a cartridge */
    IO_PRIO_NORMAL,        /* normal priority, device read needs to be checked for collisions */
    0,                     /* insertion order, gets filled in by the registration function */
    IO_MIRROR_NONE         /* NO mirroring */
};

static io_source_t tcbm1_device = {
    "TCBM1",               /* name of the chip */
    IO_DETACH_NEVER,       /* chip is never involved in collisions, so no detach */
    IO_DETACH_NO_RESOURCE, /* does not use a resource for detach */
    0xfee0, 0xfeff, 0x1f,  /* range for the device, regs:$fee0-$feff */
    1,                     /* read is always valid */
    plus4tcbm1_store,      /* store function */
    NULL,                  /* NO poke function */
    plus4tcbm1_read,       /* read function */
    NULL,                  /* TODO: peek function */
    NULL,                  /* TODO: chip state information dump function */
    IO_CART_ID_NONE,       /* not a cartridge */
    IO_PRIO_NORMAL,        /* normal priority, device read needs to be checked for collisions */
    0,                     /* insertion order, gets filled in by the registration function */
    IO_MIRROR_NONE         /* NO mirroring */
};

static io_source_t tcbm2_device = {
    "TCBM2",               /* name of the chip */
    IO_DETACH_NEVER,       /* chip is never involved in collisions, so no detach */
    IO_DETACH_NO_RESOURCE, /* does not use a resource for detach */
    0xfec0, 0xfedf, 0x1f,  /* range for the device, regs:$fec0-$fedf */
    1,                     /* read is always valid */
    plus4tcbm2_store,      /* store function */
    NULL,                  /* NO poke function */
    plus4tcbm2_read,       /* read function */
    NULL,                  /* TODO: peek function */
    NULL,                  /* TODO: chip state information dump function */
    IO_CART_ID_NONE,       /* not a cartridge */
    IO_PRIO_NORMAL,        /* normal priority, device read needs to be checked for collisions */
    0,                     /* insertion order, gets filled in by the registration function */
    IO_MIRROR_NONE         /* NO mirroring */
};

static io_source_list_t *mem_config_list_item = NULL;
static io_source_list_t *pio1_list_item = NULL;
static io_source_list_t *pio2_list_item = NULL;
static io_source_list_t *tcbm1_list_item = NULL;
static io_source_list_t *tcbm2_list_item = NULL;

static int pio1_devices_blocking_mirror = 0;

void plus4_pio1_init(int block)
{
    int rereg = 0;

    if (pio1_devices_blocking_mirror == 0 || (pio1_devices_blocking_mirror == 1 && block == -1)) {
        io_source_unregister(pio1_list_item);
        rereg = 1;
    }

    pio1_devices_blocking_mirror += block;

    if (rereg) {
        if (!pio1_devices_blocking_mirror) {
            pio1_list_item = io_source_register(&pio1_with_mirrors_device);
        } else {
            pio1_list_item = io_source_register(&pio1_only_device);
        }
    }
}

/* C16/C232/PLUS4/V364-specific I/O initialization, only common devices. */
void plus4io_init(void)
{
    mem_config_list_item = io_source_register(&mem_config_device);
    pio1_list_item = io_source_register(&pio1_with_mirrors_device);
    pio2_list_item = io_source_register(&pio2_device);
    tcbm1_list_item = io_source_register(&tcbm1_device);
    tcbm2_list_item = io_source_register(&tcbm2_device);
}

