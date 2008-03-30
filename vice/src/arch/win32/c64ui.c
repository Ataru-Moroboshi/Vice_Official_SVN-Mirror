/*
 * c64ui.c - C64-specific user interface.
 *
 * Written by
 *  Andreas Boose <boose@linux.rz.fh-hannover.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Tibor Biczo <crown@axelero.hu>
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
#include <windows.h>
#include <windowsx.h>

#include "ui.h"

#include "c64ui.h"
#include "cartridge.h"
#include "kbd.h"
#include "keyboard.h"
#include "res.h"
#include "resources.h"
#include "uilib.h"
#include "uireu.h"
#include "uivicii.h"
#include "uivideo.h"
#include "vsync.h"
#include "winmain.h"

ui_menu_toggle  c64_ui_menu_toggles[] = {
    { "DoubleSize", IDM_TOGGLE_DOUBLESIZE },
    { "DoubleScan", IDM_TOGGLE_DOUBLESCAN },
    { "DelayLoopEmulation", IDM_TOGGLE_FASTPAL },
    { "VideoCache", IDM_TOGGLE_VIDEOCACHE },
    { "Mouse", IDM_MOUSE },
    { "Mouse", IDM_MOUSE|0x00010000 },
    { NULL, 0 }
};

ui_res_value_list c64_ui_res_values[] = {
    { NULL, NULL }
};

static ui_cartridge_params c64_ui_cartridges[] = {
    {
        IDM_CART_ATTACH_CRT,
        CARTRIDGE_CRT,
        "Attach CRT cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_CRT
    },
    {
        IDM_CART_ATTACH_8KB,
        CARTRIDGE_GENERIC_8KB,
        "Attach raw 8KB cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        IDM_CART_ATTACH_16KB,
        CARTRIDGE_GENERIC_16KB,
        "Attach raw 16KB cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        IDM_CART_ATTACH_AR,
        CARTRIDGE_ACTION_REPLAY,
        "Attach Action Replay cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        IDM_CART_ATTACH_AT,
        CARTRIDGE_ATOMIC_POWER,
        "Attach Atomic Power cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        IDM_CART_ATTACH_EPYX,
        CARTRIDGE_EPYX_FASTLOAD,
        "Attach Epyx fastload cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        IDM_CART_ATTACH_IEEE488,
        CARTRIDGE_IEEE488,
        "Attach IEEE interface cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        IDM_CART_ATTACH_RR,
        CARTRIDGE_RETRO_REPLAY,
        "Attach Retro Replay cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        IDM_CART_ATTACH_SS4,
        CARTRIDGE_SUPER_SNAPSHOT,
        "Attach Super Snapshot 4 cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        IDM_CART_ATTACH_SS5,
        CARTRIDGE_SUPER_SNAPSHOT_V5,
        "Attach Super Snapshot 5 cartridge image",
        UI_LIB_FILTER_ALL | UI_LIB_FILTER_BIN
    },
    {
        0, 0, NULL, 0
    }
};

static void c64_ui_attach_cartridge(WPARAM wparam, HWND hwnd,
                                    ui_cartridge_params *cartridges)
{
    int i;
    char *s;

    i = 0;
    while ((cartridges[i].wparam != wparam) && (cartridges[i].wparam != 0))
        i++;
    if (cartridges[i].wparam == 0) {
        ui_error("Bad cartridge config in UI!");
        return;
    }
    if ((s = ui_select_file(hwnd, cartridges[i].title,
        cartridges[i].filter, FILE_SELECTOR_DEFAULT_STYLE, NULL)) != NULL) {
        if (cartridge_attach_image(cartridges[i].type, s) < 0)
            ui_error("Invalid cartridge image");
        free(s);
    }
}

static void c64_ui_specific(WPARAM wparam, HWND hwnd)
{
    switch (wparam) {
      case IDM_CART_ATTACH_CRT:
      case IDM_CART_ATTACH_8KB:
      case IDM_CART_ATTACH_16KB:
      case IDM_CART_ATTACH_AR:
      case IDM_CART_ATTACH_AT:
      case IDM_CART_ATTACH_EPYX:
      case IDM_CART_ATTACH_IEEE488:
      case IDM_CART_ATTACH_RR:
      case IDM_CART_ATTACH_SS4:
      case IDM_CART_ATTACH_SS5:
        c64_ui_attach_cartridge(wparam, hwnd, c64_ui_cartridges);
        break;
      case IDM_CART_SET_DEFAULT:
        cartridge_set_default();
        break;
      case IDM_CART_DETACH:
        cartridge_detach_image();
        break;
      case IDM_CART_FREEZE|0x00010000:
      case IDM_CART_FREEZE:
        keyboard_clear_keymatrix();
        cartridge_trigger_freeze();
        break;
      case IDM_VICII_SETTINGS:
        ui_vicii_settings_dialog(hwnd);
        break;
      case IDM_REU_SETTINGS:
        ui_reu_settings_dialog(hwnd);
        break;
      case IDM_VIDEO_SETTINGS:
        ui_video_settings_dialog(hwnd);
        break;
    }
}

int c64_ui_init(void)
{
    ui_register_machine_specific(c64_ui_specific);
    ui_register_menu_toggles(c64_ui_menu_toggles);
    ui_register_res_values(c64_ui_res_values);
    return 0;
}

