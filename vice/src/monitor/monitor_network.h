/*! \file monitor_network.h \n
 *  \author Spiro Trikaliotis
 *  \brief   Monitor implementation - network access
 *
 * monitor_network.h - Monitor implementation - network access.
 *
 * Written by
 *  Spiro Trikaliotis <spiro.trikaliotis@gmx.de>
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


#ifndef VICE_MONITOR_NETWORK_H
#define VICE_MONITOR_NETWORK_H

#include "types.h"
#include "uiapi.h"
#include "vicesocket.h"

int monitor_network_resources_init(void);
void monitor_network_resources_shutdown(void);
int monitor_network_cmdline_options_init(void);

void monitor_check_remote(void);
ssize_t monitor_network_receive(char * buffer, size_t buffer_length);
int monitor_network_transmit(const char * buffer, size_t buffer_length);
int monitor_network_get_command_line(char **prompt);

int monitor_is_remote(void);
vice_network_socket_t *monitor_get_connected_socket(void);

ui_jam_action_t monitor_network_ui_jam_dialog(const char *format, ...) VICE_ATTR_PRINTF;

#endif
