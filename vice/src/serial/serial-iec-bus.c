/*
 * serial-iec-bus.c - Common IEC bus emulation.
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

/* #define DEBUG_SERIAL_IEC_BUS */

#include "vice.h"

#include "fsdrive.h"
#include "realdevice.h"
#include "serial-iec-bus.h"
#include "serial.h"
#include "types.h"

#ifdef DEBUG_SERIAL_IEC_BUS
#include "log.h"
#define DBG(x) log_printf  x
#else
#define DBG(x)
#endif

void serial_iec_bus_open(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t))
{
    DBG(("serial_iec_bus_open device: 0x%2x secondary: 0x%02x", device, secondary));
#ifdef HAVE_REALDEVICE
    if (serial_device_type_get(device & 0x0f) == SERIAL_DEVICE_REAL) {
        realdevice_open(device, secondary, st_func);
    } else
#endif
    fsdrive_open(device, secondary, st_func);
}

void serial_iec_bus_close(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t))
{
#ifdef HAVE_REALDEVICE
    if (serial_device_type_get(device & 0x0f) == SERIAL_DEVICE_REAL) {
        realdevice_close(device, secondary, st_func);
    } else
#endif
    fsdrive_close(device, secondary, st_func);
}

void serial_iec_bus_listen(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t))
{
#ifdef HAVE_REALDEVICE
    if (serial_device_type_get(device & 0x0f) == SERIAL_DEVICE_REAL) {
        realdevice_listen(device, secondary, st_func);
    } else
#endif
    fsdrive_listentalk(device, secondary, st_func);
}

void serial_iec_bus_talk(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t))
{
#ifdef HAVE_REALDEVICE
    if (serial_device_type_get(device & 0x0f) == SERIAL_DEVICE_REAL) {
        realdevice_talk(device, secondary, st_func);
    } else
#endif
    fsdrive_listentalk(device, secondary, st_func);
}

void serial_iec_bus_unlisten(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t))
{
#ifdef HAVE_REALDEVICE
    if (serial_device_type_get(device & 0x0f) == SERIAL_DEVICE_REAL) {
        realdevice_unlisten(st_func);
    } else
#endif
    fsdrive_unlisten(device, secondary, st_func);
}

void serial_iec_bus_untalk(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t))
{
#ifdef HAVE_REALDEVICE
    if (serial_device_type_get(device & 0x0f) == SERIAL_DEVICE_REAL) {
        realdevice_untalk(st_func);
    } else
#endif
    fsdrive_untalk(device, secondary, st_func);
}

void serial_iec_bus_write(unsigned int device, uint8_t secondary, uint8_t data, void (*st_func)(uint8_t))
{
    DBG(("serial_iec_bus_write device: 0x%2x secondary: 0x%02x data: 0x%02x", device, secondary, data));
#ifdef HAVE_REALDEVICE
    if (serial_device_type_get(device & 0x0f) == SERIAL_DEVICE_REAL) {
        realdevice_write(data, st_func);
    } else
#endif
    fsdrive_write(device, secondary, data, st_func);
}

uint8_t serial_iec_bus_read(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t))
{
#ifdef HAVE_REALDEVICE
    if (serial_device_type_get(device & 0x0f) == SERIAL_DEVICE_REAL) {
        return realdevice_read(st_func);
    } else
#endif
    return fsdrive_read(device, secondary, st_func);
}

void serial_iec_bus_reset(void)
{
    fsdrive_reset();
#ifdef HAVE_REALDEVICE
    realdevice_reset();
#endif
}

void serial_iec_bus_init(void)
{
    fsdrive_init();
#ifdef HAVE_REALDEVICE
    realdevice_init();
#endif
}
