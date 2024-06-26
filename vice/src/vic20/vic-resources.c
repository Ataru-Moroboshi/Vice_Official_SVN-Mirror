/*
 * vic-resources.c - Resource handling for the VIC-I emulation.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
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

#include "archdep.h"
#include "fullscreen.h"
#include "machine.h"
#include "raster-resources.h"
#include "resources.h"
#include "vic-resources.h"
#include "vic.h"
#include "victypes.h"
#include "video.h"
#include "vsync.h"

vic_resources_t vic_resources = { 0 };
static video_chip_cap_t video_chip_cap;

static void on_vsync_set_border_mode(void *unused)
{
    int sync;

    if (resources_get_int("MachineVideoStandard", &sync) < 0) {
        sync = MACHINE_SYNC_PAL;
    }

    machine_change_timing(sync, 0 /* power freq unused */, vic_resources.border_mode);
}

static int set_border_mode(int val, void *param)
{
    switch (val) {
        case VIC_NORMAL_BORDERS:
        case VIC_FULL_BORDERS:
        case VIC_DEBUG_BORDERS:
        case VIC_NO_BORDERS:
            break;
        default:
            return -1;
    }

    vic_resources.border_mode = val;
    vsync_on_vsync_do(on_vsync_set_border_mode, NULL);

    return 0;
}

static const resource_int_t resources_int[] =
{
    { "VICBorderMode", VIC_NORMAL_BORDERS, RES_EVENT_SAME, NULL,
      &vic_resources.border_mode,
      set_border_mode, NULL },
    RESOURCE_INT_LIST_END
};

int vic_resources_init(void)
{
    video_chip_cap.dsize_allowed = ARCHDEP_VIC_DSIZE;
    video_chip_cap.dsize_default = ARCHDEP_VIC_DSIZE;
    video_chip_cap.dsize_limit_width = 0;
    video_chip_cap.dsize_limit_height = 0;
    video_chip_cap.dscan_allowed = ARCHDEP_VIC_DSCAN;
    video_chip_cap.interlace_allowed = 1;
    video_chip_cap.external_palette_name = "mike-pal";
    video_chip_cap.single_mode.sizex = 1;
    video_chip_cap.single_mode.sizey = 1;
    video_chip_cap.single_mode.rmode = VIDEO_RENDER_PAL_NTSC_1X1;
    video_chip_cap.double_mode.sizex = 2;
    video_chip_cap.double_mode.sizey = 2;
    video_chip_cap.double_mode.rmode = VIDEO_RENDER_PAL_NTSC_2X2;
    video_chip_cap.video_has_palntsc = 1;

    fullscreen_capability(&(video_chip_cap.fullscreen));

    vic.video_chip_cap = &video_chip_cap;

    if (raster_resources_chip_init("VIC", &vic.raster, &video_chip_cap) < 0) {
        return -1;
    }

    return resources_register_int(resources_int);
}
