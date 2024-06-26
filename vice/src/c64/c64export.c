/*
 * c64export.c - Expansion port and devices handling for the C64.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
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

#include "assert.h"
#include "c64cart.h"
#include "c64cartsystem.h"
#include "export.h"
#include "lib.h"
#include "monitor.h"
#include "uiapi.h"

/* #define DEBUGEXPORT */

#ifdef DEBUGEXPORT
#define DBG(x)  printf x
#else
#define DBG(x)
#endif

export_list_t c64export_head = { NULL, NULL, NULL };

export_list_t *export_query_list(export_list_t *item)
{
    if (item) {
        return item->next;
    } else {
        return c64export_head.next;
    }
}

/* assigned to monitor interface in src/c64/cart/c64cart.c */
void export_dump(void)
{
    export_list_t *current = NULL;
    io_source_t *io;
    int cartid;
    int is128 = (machine_class == VICE_MACHINE_C128);

    current = export_query_list(current);

    if (current == NULL) {
        mon_out("No expansion port devices.\n");
    } else {
               /*- -----    -     - --------- --------- ------------------------ */
        if (is128) {
                   /*12345*/
            mon_out("     ");
        }
        mon_out("  CRTID GAME EXROM IO1-usage IO2-usage Name\n");
        while (current != NULL) {
            int c128cart = CARTRIDGE_C128_ISID(current->device->cartid);
            if (cart_is_slotmain(current->device->cartid)) {
                mon_out("* ");
            } else {
                mon_out("  ");
            }
            if (is128) {
                if (c128cart) {
                           /*12345*/
                    mon_out("C128:");
                } else {
                    mon_out(" C64:");
                }
            }
            cartid = ((int)current->device->cartid);
            if (cartid < 0) {
                mon_out("0/%d  ", cartid);
            } else if (is128 && c128cart) {
                mon_out("%5d ", CARTRIDGE_C128_CRTID(cartid));
            } else {
                mon_out("%5d ", cartid);
            }
            mon_out("%4s ", current->device->game ? "*" : "-");
            mon_out("%5s ", current->device->exrom ? "*" : "-");
            io = current->device->io1;
            if (io) {
                mon_out("%04x-%04x ", io->start_address, io->end_address);
            } else {
                mon_out("     none ");
            }
            io = current->device->io2;
            if (io) {
                mon_out("%04x-%04x ", io->start_address, io->end_address);
            } else {
                mon_out("     none ");
            }
            /* show (inactive) in front of the name when no PLA lines nor
               I/O resources are used, this should not happen in normal
               operation and usually indicates a bug UNLESS this is a C128 :) */
            if ((!current->device->game) &&
                (!current->device->exrom) &&
                (!current->device->io1) &&
                (!current->device->io2)) {
                mon_out("(inactive) ");
            }
            mon_out("%s\n", current->device->name);
            current = current->next;
        }
        mon_out("Current mode: %s, GAME status: (%d) (%s), EXROM status: (%d) (%s)\n",
                cart_config_string(((export.exrom ^ 1) << 1) | export.game),
                !export.game, (export.game) ? "active" : "inactive",
                !export.exrom, (export.exrom) ? "active" : "inactive");
    }
}

int export_add(const export_resource_t *export_res)
{
    export_list_t *current;
    export_list_t *newentry = lib_malloc(sizeof(export_list_t));

    assert(export_res != NULL);
    DBG(("EXP: register name:%s\n", export_res->name));

    /* find last entry */
    current = &c64export_head;
    while (current->next != NULL) {
        current = current->next;
    }
    /* add new entry at end of list */
    current->next = newentry;
    newentry->previous = current;
    newentry->device = (export_resource_t *)export_res;
    newentry->next = NULL;

    return 0;
}

int export_remove(const export_resource_t *export_res)
{
    export_list_t *current;
    export_list_t *prev;

    assert(export_res != NULL);
    DBG(("EXP: unregister name:%s\n", export_res->name));

    /* find entry */
    current = c64export_head.next;
    while (current != NULL) {
        if (current->device) {
            if (current->device == export_res) {
                /* if entry found, remove it from list */
                prev = current->previous;
                prev->next = current->next;
                if (current->next) {
                    current->next->previous = prev;
                }
                lib_free(current);
                return 0;
            }
        }
        current = current->next;
    }
    /* FIXME: when all structs have been updated we can place an assertion here */
    DBG(("EXP: BUG unregister name: '%s' not found\n", export_res->name));
    return -1;
}

int export_resources_init(void)
{
    return 0;
}
