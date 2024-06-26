/*
 * cardkey.c - Cardco Cardkey 1 keypad emulation.
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

#include "joyport.h"
#include "keyboard.h"

#include "cardkey.h"

/* Control port <--> Cardkey connections:

   cport | keypad | I/O
   -------------------------
     1   | KEY0   |  I
     2   | KEY1   |  I
     3   | KEY2   |  I
     4   | KEY3   |  I
     6   | PRESS  |  I
     8   | GND    |  Ground

Works on:
- native port(s) (x64/x64sc/scpu64/xvic)
- sidcart joystick port (xplus4)

The keypad keys ground certain lines when pressed.


The keypad has the following layout:

KEYPAD                  KEYMAP KEYS
-----------------   ---------------------
| 7 | 8 | 9 | * |   |  1 |  2 |  3 |  4 |
-----------------   ---------------------
| 4 | 5 | 6 | D |   |  6 |  7 |  8 |  9 |
-----------------   ---------------------
| 1 | 2 | 3 | - |   | 11 | 12 | 13 | 14 |
-----------------   ---------------------
| . | 0 | E | + |   | 16 | 17 | 18 | 19 |
-----------------   ---------------------

E = Enter

The keypad grounds a line when a key is pressed.

The following logic is used:

KEY3 = 8 || 9 || + || - || / || * || dot || ret
KEY2 = 4 || 5 || 6 || 7 || / || * || dot || ret
KEY1 = 2 || 3 || 6 || 7 || + || - || dot || ret
KEY0 = 1 || 3 || 5 || 7 || 9 || - || *   || ret

which results in:

KEY PRESSED   KEY3 KEY2 KEY1 KEY0
-----------   ---- ---- ---- ----
0               0    0    0    0
1               0    0    0    1
2               0    0    1    0
3               0    0    1    1
4               0    1    0    0
5               0    1    0    1
6               0    1    1    0
7               0    1    1    1
8               1    0    0    0
9               1    0    0    1
+               1    0    1    0
-               1    0    1    1
/               1    1    0    0
*               1    1    0    1
.               1    1    1    0
Enter           1    1    1    1

The PRESS (Fire) line is used to indicate a key press.
 */

#define ROW_COL(x, y) ((x * 4) + y)

#define KEYPAD_KEY_7     ROW_COL(0,0)
#define KEYPAD_KEY_8     ROW_COL(0,1)
#define KEYPAD_KEY_9     ROW_COL(0,2)
#define KEYPAD_KEY_MULT  ROW_COL(0,3)

#define KEYPAD_KEY_4     ROW_COL(1,0)
#define KEYPAD_KEY_5     ROW_COL(1,1)
#define KEYPAD_KEY_6     ROW_COL(1,2)
#define KEYPAD_KEY_DIV   ROW_COL(1,3)

#define KEYPAD_KEY_1     ROW_COL(2,0)
#define KEYPAD_KEY_2     ROW_COL(2,1)
#define KEYPAD_KEY_3     ROW_COL(2,2)
#define KEYPAD_KEY_MINUS ROW_COL(2,3)

#define KEYPAD_KEY_DOT   ROW_COL(3,0)
#define KEYPAD_KEY_0     ROW_COL(3,1)
#define KEYPAD_KEY_ENTER ROW_COL(3,2)
#define KEYPAD_KEY_PLUS  ROW_COL(3,3)

#define KEYPAD_NUM_KEYS  16

static int cardkey_enabled = 0;

static unsigned int keys[KEYPAD_NUM_KEYS];

/* ------------------------------------------------------------------------- */

static void handle_keys(int row, int col, int pressed)
{
    /* sanity check of the rows and cols, rows should be 0-3 and cols should be 1-4 */
    if (row < 0 || row > 3 || col < 1 || col > 4) {
        return;
    }

    /* change the state of the key that the row/col is wired to */
    keys[(row * 4) + col - 1] = (unsigned int)pressed;
}

/* ------------------------------------------------------------------------- */

static int joyport_cardkey_set_enabled(int port, int enabled)
{
    int new_state = enabled ? 1 : 0;

    if (new_state == cardkey_enabled) {
        return 0;
    }

    if (new_state) {
        /* enabled, clear keys and register the keypad */
        memset(keys, 0, KEYPAD_NUM_KEYS * sizeof(unsigned int));
        keyboard_register_joy_keypad(handle_keys);
    } else {
        /* disabled, unregister the keypad */
        keyboard_register_joy_keypad(NULL);
    }

    /* set the current state */
    cardkey_enabled = new_state;

    return 0;
}

static uint8_t cardkey_read_dig(int port)
{
    unsigned int retval = 0;
    unsigned int tmp;

    /* KEY3   8 9 + - / * dot ret */
    tmp = keys[KEYPAD_KEY_8] |
          keys[KEYPAD_KEY_9] |
          keys[KEYPAD_KEY_PLUS] |
          keys[KEYPAD_KEY_MINUS] |
          keys[KEYPAD_KEY_DIV] |
          keys[KEYPAD_KEY_MULT] |
          keys[KEYPAD_KEY_DOT] |
          keys[KEYPAD_KEY_ENTER];
    tmp <<= JOYPORT_RIGHT_BIT;   /* output key 3 on the joyport 'right' pin */
    retval |= tmp;

    /* KEY2   4 5 6 7 / * dot ret */
    tmp = keys[KEYPAD_KEY_4] |
          keys[KEYPAD_KEY_5] |
          keys[KEYPAD_KEY_6] |
          keys[KEYPAD_KEY_7] |
          keys[KEYPAD_KEY_DIV] |
          keys[KEYPAD_KEY_MULT] |
          keys[KEYPAD_KEY_DOT] |
          keys[KEYPAD_KEY_ENTER];
    tmp <<= JOYPORT_LEFT_BIT;   /* output key 2 on the joyport 'left' pin */
    retval |= tmp;

    /* KEY1   2 3 6 7 + - dot ret */
    tmp = keys[KEYPAD_KEY_2] |
          keys[KEYPAD_KEY_3] |
          keys[KEYPAD_KEY_6] |
          keys[KEYPAD_KEY_7] |
          keys[KEYPAD_KEY_PLUS] |
          keys[KEYPAD_KEY_MINUS] |
          keys[KEYPAD_KEY_DOT] |
          keys[KEYPAD_KEY_ENTER];
    tmp <<= JOYPORT_DOWN_BIT;   /* output key 1 on the joyport 'down' pin */
    retval |= tmp;

    /* KEY0  1 3 5 7 9 - * ret */
    tmp = keys[KEYPAD_KEY_1] |
          keys[KEYPAD_KEY_3] |
          keys[KEYPAD_KEY_5] |
          keys[KEYPAD_KEY_7] |
          keys[KEYPAD_KEY_9] |
          keys[KEYPAD_KEY_MINUS] |
          keys[KEYPAD_KEY_MULT] |
          keys[KEYPAD_KEY_ENTER];
    retval |= tmp;   /* output key 0 on the joyport 'up' pin */

    /* output key pressed on 'fire' pin. */
    if (retval != 0 || keys[KEYPAD_KEY_0]) {
        retval |= JOYPORT_FIRE;
    }

    joyport_display_joyport(port, JOYPORT_ID_CARDCO_KEYPAD, (uint16_t)retval);

    return (uint8_t)~retval;
}

/* ------------------------------------------------------------------------- */

static joyport_t joyport_cardkey_device = {
    "Keypad (Cardco Cardkey 1)", /* name of the device */
    JOYPORT_RES_ID_KEYPAD,       /* device is a keypad, only 1 keypad can be active at the same time */
    JOYPORT_IS_NOT_LIGHTPEN,     /* device is NOT a lightpen */
    JOYPORT_POT_OPTIONAL,        /* device does NOT use the potentiometer lines */
    JOYPORT_5VDC_NOT_NEEDED,     /* device does NOT need +5VDC to work */
    JOYSTICK_ADAPTER_ID_NONE,    /* device is NOT a joystick adapter */
    JOYPORT_DEVICE_KEYPAD,       /* device is a Keypad */
    0,                           /* No output bits */
    joyport_cardkey_set_enabled, /* device enable/disable function */
    cardkey_read_dig,            /* digital line read function */
    NULL,                        /* NO digital line store function */
    NULL,                        /* NO pot-x read function */
    NULL,                        /* NO pot-y read function */
    NULL,                        /* NO powerup function */
    NULL,                        /* NO device write snapshot function */
    NULL,                        /* NO device read snapshot function */
    NULL,                        /* NO device hook function */
    0                            /* NO device hook function mask */
};

/* ------------------------------------------------------------------------- */

int joyport_cardkey_resources_init(void)
{
    return joyport_device_register(JOYPORT_ID_CARDCO_KEYPAD, &joyport_cardkey_device);
}
