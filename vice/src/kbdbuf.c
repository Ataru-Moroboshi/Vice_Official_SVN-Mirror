/*
 * kbdbuf.c - Kernal keyboard buffer handling for VICE.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andre Fachat <a.fachat@physik.tu-chemnitz.de>
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

/* #define DEBUG_KBDBUF */

#include "vice.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "alarm.h"
#include "autostart.h"
#include "charset.h"
#include "cmdline.h"
#include "initcmdline.h"
#include "kbdbuf.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "maincpu.h"
#include "mem.h"
#include "resources.h"
#include "types.h"

#ifdef DEBUG_KBDBUF
#define DBG(x) log_printf  x
#else
#define DBG(x)
#endif

/* Maximum number of characters we can queue.  */
#define QUEUE_SIZE      16384

/* First location of the buffer.  */
static int buffer_location;

/* Location that stores the number of characters pending in the
   buffer.  */
static int num_pending_location;

/* Maximum number of characters that fit in the buffer.  */
static int buffer_size;

/* Number of cycles needed to initialize the Kernal.  */
static CLOCK kernal_init_cycles;

/* Characters in the queue.  */
static uint8_t queue[QUEUE_SIZE];

/* Next element in `queue' we must push into the kernal's queue.  */
static int head_idx = 0;

/* Number of pending characters in the incoming queue  */
static int num_pending = 0;

/* Flag if we are initialized already.  */
static int kbd_buf_enabled = 0;

/* Flag that indicates if the cmdline option was used to provide the string  */
static bool kbd_buf_cmdline = false;

/* String to feed into the keyboard buffer.  */
static char *kbd_buf_string = NULL;

static int KbdbufDelay = 0;

static int use_kbdbuf_flush_alarm = 0;

static alarm_t *kbdbuf_flush_alarm = NULL;

/* Only feed the cmdline -kbdbuf argument to the buffer once */
static bool kbdbuf_init_cmdline_fed = false;

CLOCK kbdbuf_flush_alarm_time = 0;

/* ------------------------------------------------------------------------- */

/*! \internal \brief set additional keybuf delay. 0 means default. (none) */
static int set_kbdbuf_delay(int val, void *param)
{
    if (val < 0) {
        val = 0;
    }
    KbdbufDelay = val;
    return 0;
}

/*! \brief integer resources used by keybuf */
static const resource_int_t resources_int[] = {
    { "KbdbufDelay", 0, RES_EVENT_NO, (resource_value_t)0,
      &KbdbufDelay, set_kbdbuf_delay, NULL },
    RESOURCE_INT_LIST_END
};

/*! \brief initialize the resources
 \return
   0 on success, else -1.

 \remark
   Registers the integer resources
*/
int kbdbuf_resources_init(void)
{
    return resources_register_int(resources_int);
}

/* ------------------------------------------------------------------------- */

static void kbd_buf_parse_string(const char *string)
{
    unsigned int i, j;
    size_t len;

    len = strlen(string);

    if (len > QUEUE_SIZE) {
        len = QUEUE_SIZE;
    }

    kbd_buf_string = lib_realloc(kbd_buf_string, len + 1);
    memset(kbd_buf_string, 0, len + 1);

    for (i = 0, j = 0; i < len; i++) {
        if (string[i] == '\\') {
            /* printf("esc:%s\n", &string[i]); */
            if((i < (len - 1)) && (string[i + 1] == '\\')) {
                /* escaped backslash "\\" */
                kbd_buf_string[j] = charset_p_topetscii('\\');
                i += 1;
                j++;
            } else if((i < (len - 1)) && (string[i + 1] == 'n')) {
                /* escaped line ending "\n" */
                kbd_buf_string[j] = charset_p_topetscii('\n');
                i += 1;
                j++;
            } else if((i < (len - 3)) && (string[i + 1] == 'x') && isxdigit((unsigned char)string[i + 2]) && isxdigit((unsigned char)string[i + 3])) {
                /* escaped hex value in c-style format "\x00" */
                char hexvalue[3];

                hexvalue[0] = string[i + 2];
                hexvalue[1] = string[i + 3];
                hexvalue[2] = '\0';

                kbd_buf_string[j] = (char)strtol(hexvalue, NULL, 16);
                i += 3;
                j++;
            }
        } else {
            /* printf("chr:%s\n", &string[i]); */
            /* regular character, convert to petscii */
            kbd_buf_string[j] = charset_p_topetscii(string[i]);
            j++;
        }
    }
}

/* parse (un-escape) a string and put it into the keyboard buffer,
   used by the monitor */
int kbdbuf_feed_string(const char *string)
{
    DBG(("kbdbuf_feed_string kbd_buf_cmdline:%d", kbd_buf_cmdline));
    kbd_buf_parse_string(string);

    return kbdbuf_feed(kbd_buf_string);
}

/* used by the -keybuf cmdline option */
static int kdb_buf_feed_cmdline(const char *param, void *extra_param)
{
    DBG(("kdb_buf_feed_cmdline kbd_buf_cmdline:%d", kbd_buf_cmdline));
    kbd_buf_cmdline = true;
    kbd_buf_parse_string(param);

    return 0;
}

static const cmdline_option_t cmdline_options[] =
{
    { "-keybuf", CALL_FUNCTION, CMDLINE_ATTRIB_NEED_ARGS,
      kdb_buf_feed_cmdline, NULL, NULL, NULL,
      "<string>", "Put the specified string into the keyboard buffer." },
    { "-keybuf-delay", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "KbdbufDelay", NULL,
      "<value>", "Set additional keyboard buffer delay (0: use default)" },
    CMDLINE_LIST_END
};

int kbdbuf_cmdline_options_init(void)
{
    return cmdline_register_options(cmdline_options);
}

/* ------------------------------------------------------------------------- */

/* put character into the keyboard queue inside the emulation */
static void tokbdbuffer(int c)
{
    int num = mem_read((uint16_t)(num_pending_location));
    /* printf("tokbdbuffer c:%d num:%d\n", c, num); */
    mem_inject_key((uint16_t)(buffer_location + num), (uint8_t)c);
    mem_inject_key((uint16_t)(num_pending_location), (uint8_t)(num + 1));
}

/* Return nonzero if the keyboard buffer is empty.  */
int kbdbuf_is_empty(void)
{
    return (int)(mem_read((uint16_t)(num_pending_location)) == 0);
}

/* Return nonzero if there are keys in the buffer queue */
int kbdbuf_queue_is_empty(void)
{
    return num_pending > 0 ? 0 : 1;
}

/* Feed `string' into the incoming queue.  */
static int string_to_queue(const char *string)
{
    const int num = (int)strlen(string);
    int i, p;

    if ((num_pending + num) > QUEUE_SIZE || !kbd_buf_enabled) {
        return -1;
    }

    for (p = (head_idx + num_pending) % QUEUE_SIZE, i = 0;
         i < num; p = (p + 1) % QUEUE_SIZE, i++) {
        queue[p] = string[i];
        /* printf("string_to_queue %d:'%c'\n",p,queue[p]); */
    }

    num_pending += num;

    kbdbuf_flush();

    return 0;
}

/* remove current character from incoming queue */
static void removefromqueue(void)
{
    --num_pending;
    head_idx = (head_idx + 1) % QUEUE_SIZE;
}

/* used by autostart */
void kbdbuf_feed_cmdline(void)
{
    DBG(("kbdbuf_feed_cmdline kbd_buf_cmdline:%d", kbd_buf_cmdline));
    if (kbd_buf_string != NULL) {
        /* printf("kbdbuf_feed_cmdline: %d '%s'\n", KbdbufDelay, kbd_buf_string); */
        if (KbdbufDelay) {
            kbdbuf_feed_runcmd(kbd_buf_string);
        } else {
            kbdbuf_feed(kbd_buf_string);
        }
    }
}

static void kbdbuf_flush_alarm_triggered(CLOCK offset, void *data)
{
    alarm_unset(kbdbuf_flush_alarm);
    kbdbuf_flush_alarm_time = 0;

    tokbdbuffer(13);
    removefromqueue();
}

void kbdbuf_reset(int location, int plocation, int size, CLOCK mincycles)
{
    buffer_location = location;
    num_pending_location = plocation;
    buffer_size = size;
    kernal_init_cycles = mincycles;

    if (mincycles) {
        kbd_buf_enabled = 1;
    } else {
        kbd_buf_enabled = 0;
    }
}

/* abort ongoing "paste", called by reset */
void kbdbuf_abort(void)
{
    DBG(("kbdbuf_abort kbd_buf_cmdline:%d", kbd_buf_cmdline));
    /* only abort if the keyboard buffer was NOT filled via the commandline
       option (else we cancel just that during the initial reset) */
    if (kbd_buf_cmdline == false) {
        num_pending = 0;
    }
}

/* Initialization.  */
void kbdbuf_init(int location, int plocation, int size, CLOCK mincycles)
{
    int isautoload = (cmdline_get_autostart_mode() != AUTOSTART_MODE_NONE);

    if (!isautoload) {
        mincycles += KbdbufDelay;
    }
    kbdbuf_flush_alarm = alarm_new(maincpu_alarm_context, "Keybuf", kbdbuf_flush_alarm_triggered, NULL);
    kbdbuf_reset(location, plocation, size, mincycles);
    /* printf("kbdbuf_init cmdline_get_autostart_mode(): %d\n", cmdline_get_autostart_mode()); */
    /* inject string given to -keybuf option on commandline into keyboard buffer,
       except autoload/start was used, then it is postponed to after the loading */
    if (!isautoload) {
        /* only feed command line argument when the buffer can be fed */
        if (size > 0) {
            /* only feed the command line argument once, see src/pet/petrom.c:
             * petrom_checksum() calls kbdbuf_init() for $reason and that
             * function is called twice */
            if (!kbdbuf_init_cmdline_fed) {
                kbdbuf_feed_cmdline();
                kbdbuf_init_cmdline_fed = true; /* trigger diet */
            }
        }
    }
}

void kbdbuf_shutdown(void)
{
    lib_free(kbd_buf_string);
}

/* used by autostart, monitor binary remote protocol, "paste" UI action */
int kbdbuf_feed(const char *string)
{
    use_kbdbuf_flush_alarm = 0;
    return string_to_queue(string);
}

/* used by autostart to feed "RUN" */
int kbdbuf_feed_runcmd(const char *string)
{
    use_kbdbuf_flush_alarm = 1;
    return string_to_queue(string);
}

/* Flush pending characters into the kernal's queue if possible.
   This is (at least) called once per frame in vsync handler */
void kbdbuf_flush(void)
{
    static bool prevent_recursion = false;

    unsigned int i, n;

    /* memory write side effects can end up calling draw handler -> vsync end of line -> kbdbuf_flush infinitely */
    if (prevent_recursion) {
        return;
    }
    prevent_recursion = true;

    if ((!kbd_buf_enabled)
        || (num_pending == 0)
        || !kbdbuf_is_empty()
        || (maincpu_clk < kernal_init_cycles)
        || (kbdbuf_flush_alarm_time != 0)) {
        prevent_recursion = false;
        return;
    }
    n = (num_pending > buffer_size) ? buffer_size : num_pending;
    /* printf("kbdbuf_flush pending: %d n: %d head_idx: %d\n", num_pending, n, head_idx); */
    for (i = 0; i < n; i++) {
        /* printf("kbdbuf_flush i:%d head_idx:%d queue[head_idx]: %d use_kbdbuf_flush_alarm: %d\n",i,head_idx,queue[head_idx],use_kbdbuf_flush_alarm); */
        /* use an alarm to randomly delay RETURN for up to one frame */
        if ((queue[head_idx] == 13) && (use_kbdbuf_flush_alarm == 1)) {
            /* we actually need to wait _at least_ one frame to not overrun the buffer */
            kbdbuf_flush_alarm_time = maincpu_clk + (CLOCK)machine_get_cycles_per_frame();
            kbdbuf_flush_alarm_time += lib_unsigned_rand(1, (unsigned int)machine_get_cycles_per_frame());
            alarm_set(kbdbuf_flush_alarm, kbdbuf_flush_alarm_time);

            prevent_recursion = false;
            return;
        }
        tokbdbuffer(queue[head_idx]);
        removefromqueue();
    }

    kbd_buf_cmdline = false;
    prevent_recursion = false;
}
