/*
 * userport.c - userport handling.
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

/*
    C64/C128/VIC20/Plus4/PET

     1  2  3  4  5  6  7  8  9 10 11 12
    --|-- -- -- -- -- -- -- -- --|-- --
     A  B  C  D  E  F  H  J  K  L  M  N


        C64/C128         C64DTV    VIC20               Plus4               PET                      CBM2 (internal)

     1  GND                        GND                 GND                 GND
     2  +5V DC                     +5V DC              +5V DC              TV Video
     3  !RESET                     !RESET              !RESET              CB1 (PIA2) IEEE-SRQ IN
     4  CNT1 (CIA1)                JOY0                P2                  PA6 (PIA1) IEEE-EOI
     5  SP1                        JOY1                P3                  PA7 (PIA1) Diag Pin
     6  CNT2 (CIA2)                JOY2                P4                  CB1 (VIA) Tape#2 Read
     7  SP2                        Lightpen Fire       P5                  PB3 (VIA) Tape Write
     8  PC2                        PA6, Tape Sense     RS232 clock         CA1 (PIA1) Tape#1 Read               5 (?)
     9  Serial ATN in              Serial ATN in       ATN (Cpu Port)      PB5 (VIA); CB1 (PIA1) TV Vertical    4 (?)
    10  9V AC                      9V AC               9V AC               TV Horizontal
    11  9V AC                      9V AC               9V AC               CA2 (VIA) Graphic
    12  GND                        GND                 GND                 GND

     A  GND                        GND                 GND                 GND
     B  !FLAG2                     CB1                 P0                  CA1 (VIA)
     C  PB0 (CIA2)       PB0       PB0                 RXD (ACIA)          PA0 (VIA)                 14 (?)
     D  PB1              PB1       PB1                 RTS (ACIA)          PA1 (VIA)                 13 (?)
     E  PB2              PB2       PB2                 DTR (ACIA)          PA2 (VIA)                 12 (?)
     F  PB3              PB3       PB3                 P7                  PA3 (VIA)                 11 (?)
     H  PB4              PB4       PB4                 DCD (ACIA)          PA4 (VIA)                 10 (?)
     J  PB5              PB5       PB5                 P6                  PA5 (VIA)                  9 (?)
     K  PB6              PB6       PB6                 P1                  PA6 (VIA)                  8 (?)
     L  PB7              PB7       PB7                 DSR (ACIA)          PA7 (VIA)                  7 (?)
     M  PA2 (CIA2)       PA2       CB2                 TXD (ACIA)          CB2 (VIA)                  2 (?)
     N  GND                        GND                 GND                 GND

   CBM2 (internal)

     25 o o o o o o o 9 7 5 3 1
     26 o o o o o o o o 8 6 4 2

    FIXME: indicate what pin goes where

    Pin  ID     IC          Use
     1   GND
     2   PB2    TPI 6525    PRB2
     3   GND
     4   PB3    TPI 6525    PRB3
     5   !PC    CIA 6526    -PC         (Handshake PRB I/0, Output)
     6   !FL.   Cass-Read   -FLAG       (Interrupt, Input)
     7   2D7    CIA 6526    PRB7
     8   2D6    CIA 6526    PRB6
     9   2D5    CIA 6526    PRB5
    10   2D4    CIA 6526    PRB4
    11   2D3    CIA 6526    PRB3
    12   2D2    CIA 6526    PRB2
    13   2D1    CIA 6526    PRB1
    14   2D0    CIA 6526    PRB0
    15   1D7    CIA 6526    PRA7
    16   1D6    CIA 6526    PRA6
    17   1D5    CIA 6526    PRA5
    18   1D4    CIA 6526    PRA4
    19   lD3    CIA 6526    PRA3
    20   1D2    CIA 6526    PRA2
    21   1D1    CIA 6526    PRA1
    22   1D0    CIA 6526    PRA0
    23   !CNT   CIA 6526    -CNT
    24   +5V DC
    25   !IRQ   TPI 6525    PRC5
    26   SP     CIA 6526    SP          (Serial Port I/O)

    C64DTV (internal)

    - "Userport" pins must be tapped at the ASIC
    - DTV V1 has no userport pins exposed
    - "Hummer" has only PB0-PB4

    PA2
    PB7
    PB6
    PB5
    PB4
    PB3
    PB2
    PB1
    PB0
 */

/* #define DEBUG_UP */

#include "vice.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "cmdline.h"
#include "drive.h"
#include "joyport.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "printer.h"
#include "resources.h"
#include "rsuser.h"
#include "snapshot.h"
#include "spaceballs.h"
#include "uiapi.h"
#include "userport.h"
#include "userport_joystick.h"
#include "userport_hks_joystick.h"
#include "userport_woj_joystick.h"
#include "userport_4bit_sampler.h"
#include "userport_8bss.h"
#include "userport_dac.h"
#include "userport_diag_586220_harness.h"
#include "userport_diag_pin.h"
#include "userport_digimax.h"
#include "userport_hummer_joystick.h"
#include "userport_io_sim.h"
#include "userport_petscii_snespad.h"
#include "userport_ps2mouse.h"
#include "userport_rtc_58321a.h"
#include "userport_rtc_ds1307.h"
#include "userport_spt_joystick.h"
#include "userport_superpad64.h"
#include "userport_synergy_joystick.h"
#include "userport_wic64.h"

#include "c64parallel.h"
#include "plus4parallel.h"

#include "util.h"
#include "init.h"

#ifdef DEBUG_UP
#define DBG(x)  log_printf x
#else
#define DBG(x)
#endif

typedef struct type2text_s {
    int type;
    const char *text;
} type2text_t;

static type2text_t device_type_desc[] = {
    { USERPORT_DEVICE_TYPE_NONE, "None" },
    { USERPORT_DEVICE_TYPE_PRINTER, "Printer" },
    { USERPORT_DEVICE_TYPE_MODEM, "Modem" },
    { USERPORT_DEVICE_TYPE_DRIVE_PAR_CABLE, "Parallel drive cable" },
    { USERPORT_DEVICE_TYPE_JOYSTICK_ADAPTER, "Joystick adapter" },
    { USERPORT_DEVICE_TYPE_AUDIO_OUTPUT, "Audio output" },
    { USERPORT_DEVICE_TYPE_SAMPLER, "Sampler" },
    { USERPORT_DEVICE_TYPE_RTC, "Real-time clock" },
#ifdef HAVE_LIBCURL
    { USERPORT_DEVICE_TYPE_WIFI, "WiFi modem" },
#endif
#ifdef USERPORT_EXPERIMENTAL_DEVICES
    { USERPORT_DEVICE_TYPE_HARNESS, "Diagnostic harness" },
#endif
    { USERPORT_DEVICE_TYPE_MOUSE_ADAPTER, "Mouse adapter" },
    { -1, NULL }
};

static const char *userport_type2text(int type)
{
    int i;
    const char *retval = NULL;

    for (i = 0; device_type_desc[i].type != -1; ++i) {
        if (device_type_desc[i].type == type) {
            retval = device_type_desc[i].text;
        }
    }
    return retval;
}

/* flag indicating if the userport exists on the current emulated model */
static int userport_active = 1;

/* current userport device */
static int userport_current_device = USERPORT_DEVICE_NONE;

/* this will hold all the information about the userport devices */
static userport_device_t userport_device[USERPORT_MAX_DEVICES] = {0};

/* this will hold all the information about the userport itself */
static userport_port_props_t userport_props;

/* ---------------------------------------------------------------------------------------------------------- */

void userport_port_register(userport_port_props_t *props)
{
    userport_props.has_pa2 = props->has_pa2;
    userport_props.has_pa3 = props->has_pa3;
    userport_props.set_flag = props->set_flag;
    userport_props.has_pc = props->has_pc;
    userport_props.has_sp12 = props->has_sp12;
    userport_props.has_reset = props->has_reset;
}

/* register a device to be used in the userport system if possible */
int userport_device_register(int id, userport_device_t *device)
{
    if (id < 1 || id > USERPORT_MAX_DEVICES) {
        return -1;
    }

    userport_device[id].name = device->name;
    userport_device[id].joystick_adapter_id = device->joystick_adapter_id;
    userport_device[id].device_type = device->device_type;
    userport_device[id].enable = device->enable;
    userport_device[id].read_pbx = device->read_pbx;
    userport_device[id].store_pbx = device->store_pbx;
    userport_device[id].read_pa2 = device->read_pa2;
    userport_device[id].store_pa2 = device->store_pa2;
    userport_device[id].read_pa3 = device->read_pa3;
    userport_device[id].store_pa3 = device->store_pa3;
    userport_device[id].needs_pc = device->needs_pc;
    userport_device[id].store_sp1 = device->store_sp1;
    userport_device[id].read_sp1 = device->read_sp1;
    userport_device[id].store_sp2 = device->store_sp2;
    userport_device[id].read_sp2 = device->read_sp2;
    userport_device[id].reset = device->reset;
    userport_device[id].powerup = device->powerup;
    userport_device[id].write_snapshot = device->write_snapshot;
    userport_device[id].read_snapshot = device->read_snapshot;
    return 0;
}

/* ---------------------------------------------------------------------------------------------------------- */

static int userport_reset_started = 0;

int userport_get_device(void)
{
    return userport_current_device;
}

/* attach device 'id' to the userport */
static int userport_set_device(int id)
{
    int hasparcable = 0;
    int dnr;

    /* 1st some sanity checks */
    if (id < USERPORT_DEVICE_NONE || id >= USERPORT_MAX_DEVICES) {
        return -1;
    }

    /* Nothing changes */
    if (id == userport_current_device) {
        return 0;
    }

    /* check if id is registered */
    if (id != USERPORT_DEVICE_NONE && !userport_device[id].name) {
        ui_error("Selected userport device %d is not registered", id);
        return -1;
    }

    /* check if device is a joystick adapter and a different joystick adapter is already active */
    if (id != USERPORT_DEVICE_NONE && userport_device[id].joystick_adapter_id) {
        if (!userport_device[userport_current_device].joystick_adapter_id) {
            /* if the current device in the userport is not a joystick adapter
               we need to check if a different joystick adapter is already
               active */
            if (joystick_adapter_get_id()) {
                ui_error("Selected userport device %s is a joystick adapter, but joystick adapter %s is already active.", userport_device[id].name, joystick_adapter_get_name());
                return -1;
            }
        }
    }

    /* all checks done, now disable the current device and enable the new device */
    if (userport_device[userport_current_device].enable) {
        userport_device[userport_current_device].enable(0);
    }
    if (userport_device[id].enable) {
        if (userport_device[id].enable(1) < 0) {
            return -1;
        }
    }
    userport_current_device = id;

    /* FIXME: some emulators have userport devices, but no userport parallel
              cable, remove this once it is implemented. */
    if (machine_class == VICE_MACHINE_VIC20) {
        return 0;
    }

    /* check if any drive has a parallel cable enabled */
    for (dnr = 0; dnr < NUM_DISK_UNITS; dnr++) {
        int cable;
        resources_get_int_sprintf("Drive%iParallelCable", &cable, dnr + 8);
        if (cable != DRIVE_PC_NONE) {
            hasparcable = 1;
        }
    }

    if ((userport_current_device == USERPORT_DEVICE_DRIVE_PAR_CABLE) && (hasparcable == 0)) {
        if (init_main_is_done()) {
            /* show a message if parallel cable was enabled on the user port, but no drive has it enabled */
            ui_message("Remember that you'll have to select a parallel cable in the drive settings.\n");
        }
    } else if ((userport_current_device == USERPORT_DEVICE_NONE) && (hasparcable == 1)) {
        /* if parallel cable was removed from user port, disable it in the drive settings as well */
        for (dnr = 0; dnr < NUM_DISK_UNITS; dnr++) {
            resources_set_int_sprintf("Drive%iParallelCable", 0, dnr + 8);
        }
    } else if ((userport_current_device != USERPORT_DEVICE_DRIVE_PAR_CABLE) && (hasparcable == 1)) {
        if (init_main_is_done()) {
            /* if user port device was set to something that isn't parallel cable, and any drive has
            parallel cable enabled, show a message */
            ui_message("Some drive(s) still have parallel cable enabled - remember you'll have to change this setting "
                    "back to parallel cable in order to use it\n");
        }
    }

    return 0;
}

static int userport_valid_devices_compare_names(const void* a, const void* b)
{
    const userport_desc_t *arg1 = (const userport_desc_t*)a;
    const userport_desc_t *arg2 = (const userport_desc_t*)b;

    if (arg1->device_type != arg2->device_type) {
        if (arg1->device_type < arg2->device_type) {
            return -1;
        } else {
            return 1;
        }
    }

    return strcmp(arg1->name, arg2->name);
}

userport_desc_t *userport_get_valid_devices(int sort)
{
    userport_desc_t *retval = NULL;
    int i;
    int valid = 0;
    int j = 0;

    for (i = 0; i < USERPORT_MAX_DEVICES; ++i) {
        if (userport_device[i].name) {
               ++valid;
        }
    }


    retval = lib_malloc(((size_t)valid + 1) * sizeof(userport_desc_t));
    for (i = 0; i < USERPORT_MAX_DEVICES; ++i) {
        if (userport_device[i].name) {
            retval[j].name = userport_device[i].name;
            retval[j].id = i;
            retval[j].device_type = userport_device[i].device_type;
            ++j;
        }
    }
    retval[j].name = NULL;

    if (sort) {
        qsort(retval, valid, sizeof(userport_desc_t), userport_valid_devices_compare_names);
    }

    return retval;
}


/** \brief  Get short description of of userport device \a type
 *
 * \param[in]   type    userport device type
 */
const char *userport_get_device_type_desc(int type)
{
    return userport_type2text(type);
}


uint8_t read_userport_pbx(uint8_t orig)
{
    uint8_t retval = orig;

    if (userport_active) {
        /* read from new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].read_pbx) {
                    retval = userport_device[userport_current_device].read_pbx(orig);
                }
            }
        }

   }
   return retval;
}

void store_userport_pbx(uint8_t val, int pulse)
{
    if (!userport_reset_started) {
        if (userport_active) {
            /* store to new userport system if the device has been registered */
            if (userport_current_device != USERPORT_DEVICE_NONE) {
                if (userport_device[userport_current_device].name) {
                    if (userport_device[userport_current_device].store_pbx) {
                        userport_device[userport_current_device].store_pbx(val, pulse);
                    }
                }
            }
        }
    }
}

uint8_t read_userport_pa2(uint8_t orig)
{
    uint8_t retval = orig;

    if (userport_active) {
        /* read from new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].read_pa2) {
                    retval = userport_device[userport_current_device].read_pa2(orig);
                }
            }
        }
    }
    return retval;
}

void store_userport_pa2(uint8_t val)
{
    if (userport_active) {
        /* store to new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].store_pa2) {
                    userport_device[userport_current_device].store_pa2(val);
                }
            }
        }
    }
}

uint8_t read_userport_pa3(uint8_t orig)
{
    uint8_t retval = orig;

    if (userport_active) {
        /* read from new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].read_pa3) {
                    retval = userport_device[userport_current_device].read_pa3(orig);
                }
            }
        }
    }
    return retval;
}

void store_userport_pa3(uint8_t val)
{
    if (userport_active) {
        /* store to new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].store_pa3) {
                    userport_device[userport_current_device].store_pa3(val);
                }
            }
        }
    }
}

void set_userport_flag(uint8_t val)
{
    if (userport_active) {
        if (userport_props.set_flag) {
            userport_props.set_flag(val);
        }
    }
}

void store_userport_sp1(uint8_t val)
{
    if (userport_active) {
        /* store to new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].store_sp1) {
                    userport_device[userport_current_device].store_sp1(val);
                }
            }
        }
    }
}

uint8_t read_userport_sp1(uint8_t orig)
{
    uint8_t retval = orig;

    if (userport_active) {
        /* read from new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].read_sp1) {
                    retval = userport_device[userport_current_device].read_sp1(orig);
                }
            }
        }
    }
    return retval;
}

void store_userport_sp2(uint8_t val)
{
    if (userport_active) {
        /* store to new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].store_sp2) {
                    userport_device[userport_current_device].store_sp2(val);
                }
            }
        }
    }
}

uint8_t read_userport_sp2(uint8_t orig)
{
    uint8_t retval = orig;

    if (userport_active) {
        /* read from new userport system if the device has been registered */
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].read_sp2) {
                    retval = userport_device[userport_current_device].read_sp2(orig);
                }
            }
        }
    }
    return retval;
}

void userport_powerup(void)
{
    if (userport_current_device != USERPORT_DEVICE_NONE) {
        if (userport_device[userport_current_device].name) {
            if (userport_device[userport_current_device].powerup) {
                userport_device[userport_current_device].powerup();
            }
        }
    }
}

void userport_reset(void)
{
    if (userport_props.has_reset) {
        if (userport_current_device != USERPORT_DEVICE_NONE) {
            if (userport_device[userport_current_device].name) {
                if (userport_device[userport_current_device].reset) {
                    userport_device[userport_current_device].reset();
                }
            }
        }
    }
}

void userport_reset_start(void)
{
    userport_reset_started = 1;
}

void userport_reset_end(void)
{
    userport_reset_started = 0;
}

/* ---------------------------------------------------------------------------------------------------------- */

/* All machines that are 100% compatible */
#define UP_C64 (VICE_MACHINE_C64 | VICE_MACHINE_C128 | VICE_MACHINE_C64SC | VICE_MACHINE_SCPU64)
#define UP_PLUS4 (VICE_MACHINE_PLUS4)
#define UP_VIC20 (VICE_MACHINE_VIC20)
#define UP_DTV (VICE_MACHINE_C64DTV)
#define UP_PET (VICE_MACHINE_PET)
#define UP_CBM2 (VICE_MACHINE_CBM5x0 | VICE_MACHINE_CBM6x0)

static userport_init_t userport_devices_init[] = {
    { USERPORT_DEVICE_PRINTER,              /* device id */
      UP_C64 | UP_VIC20 | UP_PET | UP_CBM2 | UP_PLUS4, /* emulators this device works on */
      printer_userport_resources_init,                 /* resources init function */
      NULL,                                            /* resources shutdown function */
      printer_userport_cmdline_options_init            /* cmdline options init function */
    },
    /* NOTE: technically ACIA is also userport with a Plus4 */
    { USERPORT_DEVICE_RS232_MODEM,          /* device id */
      UP_C64 | UP_VIC20,                    /* emulators this device works on */
      rsuser_resources_init,                /* resources init function */
      NULL,                                 /* resources shutdown function */
      rsuser_cmdline_options_init           /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_CGA,         /* device id */
      UP_C64 | UP_VIC20 | UP_PET | UP_CBM2, /* emulators this device works on */
      userport_joystick_cga_resources_init, /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_PET,         /* device id */
      UP_C64 | UP_VIC20 | UP_PET | UP_CBM2, /* emulators this device works on */
      userport_joystick_pet_resources_init, /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_HUMMER,          /* device id */
      UP_DTV,                                   /* emulators this device works on */
      userport_joystick_hummer_resources_init,  /* resources init function */
      NULL,                                     /* resources shutdown function */
      NULL                                      /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_OEM,         /* device id */
      UP_C64 | UP_VIC20 | UP_PET | UP_CBM2, /* emulators this device works on */
      userport_joystick_oem_resources_init, /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_HIT,         /* device id */
      UP_C64,                               /* emulators this device works on */
      userport_joystick_hit_resources_init, /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_KINGSOFT,            /* device id */
      UP_C64,                                       /* emulators this device works on */
      userport_joystick_kingsoft_resources_init,    /* resources init function */
      NULL,                                         /* resources shutdown function */
      NULL                                          /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_STARBYTE,            /* device id */
      UP_C64,                                       /* emulators this device works on */
      userport_joystick_starbyte_resources_init,    /* resources init function */
      NULL,                                         /* resources shutdown function */
      NULL                                          /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_SYNERGY,         /* device id */
      UP_PLUS4,                                 /* emulators this device works on */
      userport_joystick_synergy_resources_init, /* resources init function */
      NULL,                                     /* resources shutdown function */
      NULL                                      /* cmdline options init function */
    },
    { USERPORT_DEVICE_JOYSTICK_WOJ,                     /* device id */
      UP_C64 | UP_VIC20 | UP_PLUS4 | UP_PET | UP_CBM2,  /* emulators this device works on */
      userport_joystick_woj_resources_init,             /* resources init function */
      NULL,                                             /* resources shutdown function */
      NULL                                              /* cmdline options init function */
    },
    { USERPORT_DEVICE_DAC,                  /* device id */
      UP_C64 | UP_VIC20 | UP_PET | UP_CBM2, /* emulators this device works on */
      userport_dac_resources_init,          /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_DIGIMAX,              /* device id */
      UP_C64 | UP_CBM2,                     /* emulators this device works on */
      userport_digimax_resources_init,      /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_4BIT_SAMPLER,         /* device id */
      UP_C64 | UP_CBM2,                     /* emulators this device works on */
      userport_4bit_sampler_resources_init, /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_8BSS,         /* device id */
      UP_C64 | UP_CBM2,             /* emulators this device works on */
      userport_8bss_resources_init, /* resources init function */
      NULL,                         /* resources shutdown function */
      NULL                          /* cmdline options init function */
    },
    { USERPORT_DEVICE_RTC_58321A,               /* device id */
      UP_C64 | UP_VIC20 | UP_PET | UP_CBM2,     /* emulators this device works on */
      userport_rtc_58321a_resources_init,       /* resources init function */
      userport_rtc_58321a_resources_shutdown,   /* resources shutdown function */
      userport_rtc_58321a_cmdline_options_init  /* cmdline options init function */
    },
    { USERPORT_DEVICE_RTC_DS1307,               /* device id */
      UP_C64 | UP_VIC20 | UP_PET | UP_CBM2,     /* emulators this device works on */
      userport_rtc_ds1307_resources_init,       /* resources init function */
      userport_rtc_ds1307_resources_shutdown,   /* resources shutdown function */
      userport_rtc_ds1307_cmdline_options_init  /* cmdline options init function */
    },
    { USERPORT_DEVICE_PETSCII_SNESPAD,                  /* device id */
      UP_C64 | UP_VIC20 | UP_PLUS4 | UP_PET | UP_CBM2,  /* emulators this device works on */
      userport_petscii_snespad_resources_init,          /* resources init function */
      NULL,                                             /* resources shutdown function */
      NULL                                              /* cmdline options init function */
    },
    { USERPORT_DEVICE_SUPERPAD64,           /* device id */
      UP_C64 | UP_CBM2,                     /* emulators this device works on */
      userport_superpad64_resources_init,   /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
#ifdef USERPORT_EXPERIMENTAL_DEVICES
    { USERPORT_DEVICE_DIAG_586220_HARNESS,          /* device id */
      UP_C64,                                       /* emulators this device works on */
      userport_diag_586220_harness_resources_init,  /* resources init function */
      NULL,                                         /* resources shutdown function */
      NULL                                          /* cmdline options init function */
    },
#endif
    { USERPORT_DEVICE_DRIVE_PAR_CABLE,      /* device id */
      UP_C64 | UP_PLUS4,                    /* emulators this device works on */
      parallel_cable_cpu_resources_init,    /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_IO_SIMULATION,                            /* device id */
      UP_C64 | UP_VIC20 | UP_PLUS4 | UP_DTV | UP_PET | UP_CBM2, /* emulators this device works on */
      userport_io_sim_resources_init,                           /* resources init function */
      NULL,                                                     /* resources shutdown function */
      NULL                                                      /* cmdline options init function */
    },
#ifdef HAVE_LIBCURL
    { USERPORT_DEVICE_WIC64,                /* device id */
      UP_C64 | UP_VIC20,                    /* emulators this device works on */
      userport_wic64_resources_init,        /* resources init function */
      userport_wic64_resources_shutdown,    /* resources shutdown function */
      userport_wic64_cmdline_options_init   /* cmdline options init function */
    },
#endif
    { USERPORT_DEVICE_SPACEBALLS,           /* device id */
      UP_C64 | UP_VIC20,                    /* emulators this device works on */
      userport_spaceballs_resources_init,   /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_SPT_JOYSTICK,         /* device id */
      UP_C64 | UP_VIC20 | UP_PET | UP_CBM2, /* emulators this device works on */
      userport_spt_joystick_resources_init, /* resources init function */
      NULL,                                 /* resources shutdown function */
      NULL                                  /* cmdline options init function */
    },
    { USERPORT_DEVICE_DIAGNOSTIC_PIN,           /* device id */
      UP_PET,                                   /* emulators this device works on */
      userport_diag_pin_resources_init,         /* resources init function */
      NULL,                                     /* resources shutdown function */
      userport_diag_pin_cmdline_options_init    /* cmdline options init function */
    },

    { USERPORT_DEVICE_MOUSE_PS2,                /* device id */
      UP_DTV,                                   /* emulators this device works on */
      userport_ps2mouse_resources_init,         /* resources init function */
      NULL,                                     /* resources shutdown function */
      NULL                                      /* cmdline options init function */
    },

    { USERPORT_DEVICE_NONE, VICE_MACHINE_NONE, NULL, NULL, NULL },   /* end of the devices list */
};

static int userport_devices_resources_init(void)
{
    int i = 0;

    while (userport_devices_init[i].device_id != USERPORT_DEVICE_NONE) {
        if (userport_devices_init[i].emu_mask & machine_class) {
            if (userport_devices_init[i].userport_device_resources_init) {
                if (userport_devices_init[i].userport_device_resources_init() < 0) {
                    DBG(("userport_devices_resources_init failed for id %d", userport_devices_init[i].device_id));
                    return -1;
                }
                DBG(("userport_devices_resources_init registered id %d", userport_devices_init[i].device_id));
            }
        }
        i++;
    }
    return 0;
}

static void userport_devices_resources_shutdown(void)
{
    int i = 0;

    while (userport_devices_init[i].device_id != USERPORT_DEVICE_NONE) {
        if (userport_devices_init[i].emu_mask & machine_class) {
            if (userport_devices_init[i].userport_device_resources_shutdown) {
                userport_devices_init[i].userport_device_resources_shutdown();
            }
        }
        i++;
    }
}


static int set_userport_device(int val, void *param)
{
    return userport_set_device(val);
}

static const resource_int_t resources_int[] = {
    { "UserportDevice", USERPORT_DEVICE_NONE, RES_EVENT_NO, NULL,
      &userport_current_device, set_userport_device, NULL },
    RESOURCE_INT_LIST_END
};

int userport_resources_init(void)
{
    memset(userport_device, 0, sizeof(userport_device));
    userport_device[0].name = "None";
    userport_device[0].joystick_adapter_id = JOYSTICK_ADAPTER_ID_NONE;

    if (resources_register_int(resources_int) < 0) {
        return -1;
    }

    if (machine_register_userport() < 0) {
        return -1;
    }

    return userport_devices_resources_init();
}

void userport_resources_shutdown(void)
{
    userport_devices_resources_shutdown();
}

struct userport_opt_s {
    const char *name;
    int id;
};

static const struct userport_opt_s id_match[] = {
    { "none",               USERPORT_DEVICE_NONE },
    { "printer",            USERPORT_DEVICE_PRINTER },
    { "plotter",            USERPORT_DEVICE_PRINTER },
    { "modem",              USERPORT_DEVICE_RS232_MODEM },
    { "cga",                USERPORT_DEVICE_JOYSTICK_CGA },
    { "cgajoy",             USERPORT_DEVICE_JOYSTICK_CGA },
    { "cgajoystick",        USERPORT_DEVICE_JOYSTICK_CGA },
    { "pet",                USERPORT_DEVICE_JOYSTICK_PET },
    { "petjoy",             USERPORT_DEVICE_JOYSTICK_PET },
    { "petjoystick",        USERPORT_DEVICE_JOYSTICK_PET },
    { "hummer",             USERPORT_DEVICE_JOYSTICK_HUMMER },
    { "hummerjoy",          USERPORT_DEVICE_JOYSTICK_HUMMER },
    { "hummerjoystick",     USERPORT_DEVICE_JOYSTICK_HUMMER },
    { "oem",                USERPORT_DEVICE_JOYSTICK_OEM },
    { "oemjoy",             USERPORT_DEVICE_JOYSTICK_OEM },
    { "oemjoystick",        USERPORT_DEVICE_JOYSTICK_OEM },
    { "hit",                USERPORT_DEVICE_JOYSTICK_HIT },
    { "dxs",                USERPORT_DEVICE_JOYSTICK_HIT },
    { "hitjoy",             USERPORT_DEVICE_JOYSTICK_HIT },
    { "dxsjoy",             USERPORT_DEVICE_JOYSTICK_HIT },
    { "hitjoystick",        USERPORT_DEVICE_JOYSTICK_HIT },
    { "dxsjoystick",        USERPORT_DEVICE_JOYSTICK_HIT },
    { "kingsoft",           USERPORT_DEVICE_JOYSTICK_KINGSOFT },
    { "kingsoftjoy",        USERPORT_DEVICE_JOYSTICK_KINGSOFT },
    { "kingsoftjoystick",   USERPORT_DEVICE_JOYSTICK_KINGSOFT },
    { "starbyte",           USERPORT_DEVICE_JOYSTICK_STARBYTE },
    { "starbytejoy",        USERPORT_DEVICE_JOYSTICK_STARBYTE },
    { "starbytejoystick",   USERPORT_DEVICE_JOYSTICK_STARBYTE },
    { "synergy",            USERPORT_DEVICE_JOYSTICK_SYNERGY },
    { "synergyjoy",         USERPORT_DEVICE_JOYSTICK_SYNERGY },
    { "synergyjoystick",    USERPORT_DEVICE_JOYSTICK_SYNERGY },
    { "dac",                USERPORT_DEVICE_DAC },
    { "digimax",            USERPORT_DEVICE_DIGIMAX },
    { "4bitsampler",        USERPORT_DEVICE_4BIT_SAMPLER },
    { "8bss",               USERPORT_DEVICE_8BSS },
    { "58321a",             USERPORT_DEVICE_RTC_58321A },
    { "58321artc",          USERPORT_DEVICE_RTC_58321A },
    { "58321rtc",           USERPORT_DEVICE_RTC_58321A },
    { "rtc58321a",          USERPORT_DEVICE_RTC_58321A },
    { "rtc58321",           USERPORT_DEVICE_RTC_58321A },
    { "ds1307",             USERPORT_DEVICE_RTC_DS1307 },
    { "ds1307rtc",          USERPORT_DEVICE_RTC_DS1307 },
    { "rtcds1307",          USERPORT_DEVICE_RTC_DS1307 },
    { "rtc1307",            USERPORT_DEVICE_RTC_DS1307 },
    { "petscii",            USERPORT_DEVICE_PETSCII_SNESPAD },
    { "petsciisnes",        USERPORT_DEVICE_PETSCII_SNESPAD },
    { "petsciisnespad",     USERPORT_DEVICE_PETSCII_SNESPAD },
    { "superpad",           USERPORT_DEVICE_SUPERPAD64 },
    { "superpad64",         USERPORT_DEVICE_SUPERPAD64 },
#ifdef USERPORT_EXPERIMENTAL_DEVICES
    { "diag",               USERPORT_DEVICE_DIAG_586220_HARNESS },
    { "diagharness",        USERPORT_DEVICE_DIAG_586220_HARNESS },
#endif
    { "parcable",           USERPORT_DEVICE_DRIVE_PAR_CABLE },
    { "driveparcable",      USERPORT_DEVICE_DRIVE_PAR_CABLE },
    { "driveparallelcable", USERPORT_DEVICE_DRIVE_PAR_CABLE },
    { "io",                 USERPORT_DEVICE_IO_SIMULATION },
    { "iosim",              USERPORT_DEVICE_IO_SIMULATION },
    { "iosimulation",       USERPORT_DEVICE_IO_SIMULATION },
#ifdef USERPORT_EXPERIMENTAL_DEVICES
    { "wic",                USERPORT_DEVICE_WIC64 },
#endif
#ifdef HAVE_LIBCURL
    { "wic64",              USERPORT_DEVICE_WIC64 },
#endif
    { "space",              USERPORT_DEVICE_SPACEBALLS },
    { "spaceballs",         USERPORT_DEVICE_SPACEBALLS },
    { NULL, -1 }
};

static int is_a_number(const char *str)
{
    size_t i;
    size_t len = strlen(str);

    for (i = 0; i < len; i++) {
        if (!isdigit((unsigned char)str[i])) {
            return 0;
        }
    }
    return 1;
}

static int set_userport_cmdline_device(const char *param, void *extra_param)
{
    int temp = -1;
    int i = 0;

    if (!param) {
        return -1;
    }

    do {
        if (strcmp(id_match[i].name, param) == 0) {
            temp = id_match[i].id;
        }
        i++;
    } while ((temp == -1) && (id_match[i].name != NULL));

    if (temp == -1) {
        if (!is_a_number(param)) {
            return -1;
        }
        temp = atoi(param);
    }

    return set_userport_device(temp, NULL);
}


/* ------------------------------------------------------------------------- */

static int userport_devices_cmdline_options_init(void)
{
    int i = 0;

    while (userport_devices_init[i].device_id != USERPORT_DEVICE_NONE) {
        if (userport_devices_init[i].emu_mask & machine_class) {
            if (userport_devices_init[i].userport_device_cmdline_options_init) {
                if (userport_devices_init[i].userport_device_cmdline_options_init() < 0) {
                    return -1;
                }
            }
        }
        i++;
    }
    return 0;
}

static char *build_userport_string(int something)
{
    int i = 0;
    char *tmp1;
    char *tmp2;
    char number[4];
    userport_desc_t *devices = userport_get_valid_devices(0);

    tmp1 = lib_msprintf("Set userport device (0: None");

    for (i = 1; devices[i].name; ++i) {
        sprintf(number, "%d", devices[i].id);
        tmp2 = util_concat(tmp1, ", ", number, ": ", devices[i].name, NULL);
        lib_free(tmp1);
        tmp1 = tmp2;
    }
    tmp2 = util_concat(tmp1, ")", NULL);
    lib_free(tmp1);
    lib_free(devices);
    return tmp2;
}

static cmdline_option_t cmdline_options[] =
{
    { "-userportdevice", CALL_FUNCTION, CMDLINE_ATTRIB_NEED_ARGS | CMDLINE_ATTRIB_DYNAMIC_DESCRIPTION,
      set_userport_cmdline_device, NULL, "UserportDevice", NULL,
      "<device>", NULL },
    CMDLINE_LIST_END
};

int userport_cmdline_options_init(void)
{
    union char_func cf;

    cf.f = build_userport_string;
    cmdline_options[0].description = cf.c;

    if (cmdline_register_options(cmdline_options) < 0) {
        return -1;
    }

    return userport_devices_cmdline_options_init();
}

void userport_enable(int val)
{
    userport_active = val ? 1 : 0;
}

int userport_get_active_state(void)
{
    return userport_active;
}

/* ---------------------------------------------------------------------------------------------------------- */

/* USERPORT snapshot module format:

   type  | name               | description
   ----------------------------------------
   BYTE  | active             | userport active flag
   BYTE  | id                 | device id
 */

#define DUMP_VER_MAJOR   1
#define DUMP_VER_MINOR   0
static char snap_module_name[] = "USERPORT";

int userport_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, snap_module_name, DUMP_VER_MAJOR, DUMP_VER_MINOR);

    if (m == NULL) {
        return -1;
    }

    /* save userport active and current device id */
    if (0
        || SMW_B(m, (uint8_t)userport_active) < 0
        || SMW_B(m, (uint8_t)userport_current_device) < 0) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);

    /* save seperate userport device module */
    switch (userport_current_device) {
        case USERPORT_DEVICE_NONE:
            break;
        default:
            if (userport_device[userport_current_device].write_snapshot) {
                if (userport_device[userport_current_device].write_snapshot(s) < 0) {
                    return -1;
                }
            }
            break;
    }

    return 0;
}

int userport_snapshot_read_module(struct snapshot_s *s)
{
    uint8_t major_version, minor_version;
    snapshot_module_t *m;
    int tmp_userport_device;

    m = snapshot_module_open(s, snap_module_name, &major_version, &minor_version);
    if (m == NULL) {
        return -1;
    }

    if (!snapshot_version_is_equal(major_version, minor_version, DUMP_VER_MAJOR, DUMP_VER_MINOR)) {
        snapshot_module_close(m);
        return -1;
    }

    /* load userport active and current device id */
    if (0
        || SMR_B_INT(m, &userport_active) < 0
        || SMR_B_INT(m, &tmp_userport_device) < 0) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);

    /* enable device */
    userport_set_device(tmp_userport_device);

    /* load device snapshot */
    switch (userport_current_device) {
        case USERPORT_DEVICE_NONE:
            break;
        default:
            if (userport_device[userport_current_device].read_snapshot) {
                if (userport_device[userport_current_device].read_snapshot(s) < 0) {
                    return -1;
                }
            }
            break;
    }

    return 0;
}
