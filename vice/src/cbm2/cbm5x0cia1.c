/*
 * cbm2cia1.c - Definitions for the MOS6526 (CIA) chip in the CBM-II
 *
 * Written by
 *  Andre Fachat <fachat@physik.tu-chemnitz.de>
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
 * */

/* #define DEBUG_CIA1 */

#ifdef DEBUG_CIA1
#define DBG(_x_) log_printf  _x_
#else
#define DBG(_x_)
#endif

#include "vice.h"

#include <stdio.h>

#include "cbm2-resources.h"
#include "cbm2.h"
#include "cbm2cia.h"
#include "cia.h"
#include "drive.h"
#include "interrupt.h"
#include "joyport.h"
#include "joystick.h"
#include "keyboard.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "maincpu.h"
#include "parallel.h"
#include "tpi.h"
#include "types.h"

void cia1_store(uint16_t addr, uint8_t data)
{
#ifdef DEBUG_CIA1
    if (!((addr >= 0x08) && (addr <= 0x0b))) {
        DBG(("cia1_store: %04x %02x", addr, data));
    }
#endif
    ciacore_store(machine_context.cia1, addr, data);
}

uint8_t cia1_read(uint16_t addr)
{
#ifdef DEBUG_CIA1
    static int olddata, oldaddr;
    uint8_t data;
    data = ciacore_read(machine_context.cia1, addr);
/*    if (!((addr >= 0x08) && (addr <= 0x0b))) { */
    if ((oldaddr != addr) || (olddata != data)) {
        DBG(("cia1_read: %04x %02x", addr, data));
        oldaddr = addr; olddata = data;
    }
    return data;
#else
    return ciacore_read(machine_context.cia1, addr);
#endif
}

uint8_t cia1_peek(uint16_t addr)
{
    return ciacore_peek(machine_context.cia1, addr);
}

void cia1_update_model(void)
{
    if (machine_context.cia1) {
        machine_context.cia1->model = cia1_model;
    }
}

static void cia_set_int_clk(cia_context_t *cia_context, int value, CLOCK clk)
{
    tpicore_set_int(machine_context.tpi1, 2, value);
}

static void cia_restore_int(cia_context_t *cia_context, int value)
{
    tpicore_restore_int(machine_context.tpi1, 2, value);
}

/*************************************************************************
 * I/O
 */

#define cycles_per_sec               machine_get_cycles_per_second()

static int cia1_ieee_is_output;

void cia1_set_ieee_dir(cia_context_t *cia_context, int isout)
{
    cia1_ieee_is_output = isout;
    if (isout) {
        parallel_cpu_set_bus(cia_context->old_pa);
    } else {
        parallel_cpu_set_bus(0xff);
    }
}

static void do_reset_cia(cia_context_t *cia_context)
{
}

static void pulse_ciapc(cia_context_t *cia_context, CLOCK rclk)
{
}

static void store_ciapa(cia_context_t *cia_context, CLOCK rclk, uint8_t byte)
{
    set_joyport_pot_mask((byte >> 6) & 3);

    store_joyport_dig(JOYPORT_1, byte >> 2, 0x10);
    store_joyport_dig(JOYPORT_2, byte >> 3, 0x10);

    parallel_cpu_set_bus((uint8_t)(cia1_ieee_is_output ? byte : 0xff));
}

static void undump_ciapa(cia_context_t *cia_context, CLOCK rclk, uint8_t byte)
{
    parallel_cpu_set_bus((uint8_t)(cia1_ieee_is_output ? byte : 0xff));
}

static void undump_ciapb(cia_context_t *cia_context, CLOCK rclk, uint8_t b)
{
}

static void store_ciapb(cia_context_t *cia_context, CLOCK rclk, uint8_t byte)
{
    store_joyport_dig(JOYPORT_1, byte, 0x0f);
    store_joyport_dig(JOYPORT_2, byte >> 4, 0x0f);
}

/* read_* functions must return 0xff if nothing to read!!! */
static uint8_t read_ciapa(cia_context_t *cia_context)
{
    uint8_t byte;
    uint8_t joy1 = ~read_joyport_dig(JOYPORT_1);
    uint8_t joy2 = ~read_joyport_dig(JOYPORT_2);

    drive_cpu_execute_all(maincpu_clk);

    /* this reads the 8 bit IEEE488 data bus, but joystick 1 and 2 buttons
       can pull down inputs pa6 and pa7 resp. */
    byte = parallel_bus;
#ifdef DEBUG
    if (debug.ieee) {
        log_message(LOG_DEFAULT,
                    "read: parallel_bus=%02x, pra=%02x, ddra=%02x -> %02x\n",
                    parallel_bus, cia_context->c_cia[CIA_PRA],
                    cia_context->c_cia[CIA_DDRA], byte);
    }
#endif
    byte = ((byte & ~(cia_context->c_cia[CIA_DDRA]))
            | (cia_context->c_cia[CIA_PRA] & cia_context->c_cia[CIA_DDRA]))
           & ~(((joy1 & 0x10) ? 0x40 : 0)
               | ((joy2 & 0x10) ? 0x80 : 0));
    return byte;
}

/* read_* functions must return 0xff if nothing to read!!! */
static uint8_t read_ciapb(cia_context_t *cia_context)
{
    uint8_t byte = 0xff;
    uint8_t joy1 = ~read_joyport_dig(JOYPORT_1);
    uint8_t joy2 = ~read_joyport_dig(JOYPORT_2);

    byte = ((0xff & ~(cia_context->c_cia[CIA_DDRB]))
            | (cia_context->c_cia[CIA_PRB] & cia_context->c_cia[CIA_DDRB]))
           & ~((joy1 & 0x0f)
               | ((joy2 & 0x0f) << 4));
    return byte;
}

static void read_ciaicr(cia_context_t *cia_context)
{
}

static void read_sdr(cia_context_t *cia_context)
{
}

static void store_sdr(cia_context_t *cia_context, uint8_t byte)
{
}

void cia1_init(cia_context_t *cia_context)
{
    ciacore_init(machine_context.cia1, maincpu_alarm_context,
                 maincpu_int_status);
}

void cia1_set_timing(cia_context_t *cia_context, int tickspersec, int powerfreq)
{
    DBG(("cia1_set_timing: %d ticks", todticks));
    cia_context->power_freq = powerfreq;
    cia_context->ticks_per_sec = tickspersec;
    cia_context->todticks = tickspersec / powerfreq;
    cia_context->power_tickcounter = 0;
    cia_context->power_ticks = 0;
}

void cia1_setup_context(machine_context_t *machinecontext)
{
    cia_context_t *cia;

    machinecontext->cia1 = lib_calloc(1, sizeof(cia_context_t));
    cia = machinecontext->cia1;

    cia->prv = NULL;
    cia->context = NULL;

    cia->rmw_flag = &maincpu_rmw_flag;
    cia->clk_ptr = &maincpu_clk;

    cia1_set_timing(cia, C500_NTSC_CYCLES_PER_SEC, 60);

    ciacore_setup_context(cia);

    cia->model = cia1_model;

    cia->debugFlag = 0;
    cia->irq_line = IK_IRQ;
    cia->myname = lib_msprintf("CIA1");

    cia->undump_ciapa = undump_ciapa;
    cia->undump_ciapb = undump_ciapb;
    cia->store_ciapa = store_ciapa;
    cia->store_ciapb = store_ciapb;
    cia->store_sdr = store_sdr;
    cia->read_ciapa = read_ciapa;
    cia->read_ciapb = read_ciapb;
    cia->read_ciaicr = read_ciaicr;
    cia->read_sdr = read_sdr;
    cia->cia_set_int_clk = cia_set_int_clk;
    cia->cia_restore_int = cia_restore_int;
    cia->do_reset_cia = do_reset_cia;
    cia->pulse_ciapc = pulse_ciapc;
    cia->pre_store = NULL;
    cia->pre_read = NULL;
    cia->pre_peek = NULL;
}
