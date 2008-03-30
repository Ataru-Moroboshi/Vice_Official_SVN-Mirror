/*
 * raster.h - Raster-based video chip emulation helper.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Boose <boose@linux.rz.fh-hannover.de>
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

#ifndef _RASTER_H
#define _RASTER_H

#include "vice.h"

#include <string.h>

#include "raster-changes.h"
#include "types.h"

struct palette_s;

/* We assume that, if already #defined, the provided `MAX' and `MIN' actually
   work.  */
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Yeah, static allocation sucks.  But it's faster, and we are not wasting
   much space anyway.  */
#define RASTER_GFX_MSK_SIZE 0x100

/* A simple convenience type for defining rectangular areas.  */
struct raster_rectangle_s {
    unsigned int width;
    unsigned int height;
};
typedef struct raster_rectangle_s raster_rectangle_t;

/* A simple convenience type for defining screen positions.  */
struct raster_position_s {
    unsigned int x;
    unsigned int y;
};
typedef struct raster_position_s raster_position_t;

/* A simple convenience type for defining a rectangular area on the screen.  */
struct raster_area_s {
    unsigned int xs;
    unsigned int ys;
    unsigned int xe;
    unsigned int ye;
    int is_null;
};
typedef struct raster_area_s raster_area_t;

struct canvas_s;

struct raster_viewport_s
  {
    /* Output canvas.  */
    struct canvas_s *canvas;

    /* Portion of the screen displayed on the output window window.
       FIXME: We should get this from the canvas.  */
    unsigned int width, height;

    /* Title for the viewport.  FIXME: Duplicated info from the canvas?  */
    char *title;

    /* Offset of the screen on the window.  */
    unsigned int x_offset, y_offset;

    /* First and last lines shown in the output window.  */
    unsigned int first_line, last_line;

    /* First pixel in one line of the frame buffer to be shown on the output
       window.  */
    unsigned int first_x;

    /* Pixel size.  */
    raster_rectangle_t pixel_size;

    /* Exposure handler.  */
    void *exposure_handler;

    /* Only display canvas if this flag is set.  */
    int update_canvas;
};
typedef struct raster_viewport_s raster_viewport_t;

struct raster_geometry_s {
    /* Total size of the screen, including borders and unused areas.
       (SCREEN_WIDTH, SCREEN_HEIGHT)  */
    raster_rectangle_t screen_size;

    /* Size of the graphics area (i.e. excluding borders and unused areas.
       (SCREEN_XPIX, SCREEN_YPIX)  */
    raster_rectangle_t gfx_size;

    /* Size of the text area.  (SCREEN_TEXTCOLS)  */
    raster_rectangle_t text_size;

    /* Position of the graphics area.  (SCREEN_BORDERWIDTH,
       SCREEN_BORDERHEIGHT) */
    raster_position_t gfx_position;

    /* If nonzero, `gfx_position' is expected to be moved around when poking
       to the chip registers.  */
    int gfx_area_moves;

    /* FIXME: Bad names.  */
    unsigned int first_displayed_line, last_displayed_line;

    unsigned int extra_offscreen_border;
};
typedef struct raster_geometry_s raster_geometry_t;

struct raster_cache_s;
struct raster_modes_s;
struct raster_sprite_status_s;

struct raster_s {
    raster_viewport_t viewport;

    raster_geometry_t geometry;

    struct raster_modes_s *modes;

    struct raster_sprite_status_s *sprite_status;

    struct
      {
        raster_changes_t background;
        raster_changes_t foreground;
        raster_changes_t border;
        raster_changes_t next_line;
        int have_on_this_line;
      }
    changes;

    struct
      {
        PIXEL sing[0x100];
        PIXEL2 doub[0x100];
        PIXEL4 quad[0x100];
      }
    pixel_table;

    struct video_frame_buffer_s *frame_buffer;
    PIXEL *frame_buffer_ptr;

    /* This is a temporary frame buffer line used for sprite collision
       checking without drawing to the real frame buffer.  */
    PIXEL *fake_frame_buffer_line;

    /* Palette used for drawing.  */
    struct palette_s *palette;

    /* This is a bit mask representing each pixel on the screen (1 =
       foreground, 0 = background) and is used both for sprite-background
       collision checking and background sprite drawing.  When cache is
       turned on, a cached mask for each line is used instead (see
       `raster_cache_t.gfx_msk').  */
    BYTE gfx_msk[RASTER_GFX_MSK_SIZE];

    /* This is a temporary graphics mask used for sprite collision checking
       without drawing to the real frame buffer, and is set up to be always
       filled with zeroes.  */
    BYTE zero_gfx_msk[RASTER_GFX_MSK_SIZE];

    /* Smooth scroll values for the graphics (not the whole screen).  */
    int xsmooth, ysmooth;

    /* If nonzero, we should skip the next frame. (used for automatic refresh
       rate setting) */
    int skip_frame;

    /* Next line to be calculated.  */
    unsigned int current_line;

    /* Border and background colors.  */
    int border_color, background_color;

    /* Color of the overscan area.  */
    int overscan_background_color;

    /* If this is != 0, no graphics is drawn and the whole line is painted with
       border_color.  */
    int blank_enabled;

    /* If this is != 0, the current raster line is blank.  The value of this
       variable is set to zero again after the current line is updated.  */
    int blank_this_line;

    /* Open border flags.  */
    int open_right_border, open_left_border;

    /* blank_enabled is set when line display_ystop is reached and reset when
       line display_ystart is reached and blank is 0.  */
    int blank;
    unsigned int display_ystart, display_ystop;

    /* These define the borders for the current line.  */
    int display_xstart, display_xstop;

    /* Flag: should we display the line in idle state? */
    int draw_idle_state;

    /* Count character lines (i.e. RC on the VIC-II).  */
    int ycounter;

    /* Current video mode.  */
    int video_mode;

    /* Cache.  */
    struct raster_cache_s *cache;
    int cache_enabled;          /* FIXME: Method to toggle it. */

    /* This is != 0 if we cannot use the values in the cache anymore.  */
    int dont_cache;

    /* Number of lines that have been recalculated.  When this value reaches
       the number of lines that are displayed in the output, then the cache
       is valid again.  */
    unsigned int num_cached_lines;

    /* Area to update.  */
    raster_area_t update_area;

    unsigned int do_double_scan;

    /* Function to call when internal tables have to be refreshed after a
       mode change. E.g. vicii::init_drawing_tables(). NULL allowed */
    void (*refresh_tables)(void);
};
typedef struct raster_s raster_t;

struct screenshot_s;

#define RASTER_PIXEL(raster, c) (raster)->pixel_table.sing[(c)]

/* FIXME: MSDOS does not need double or quad pixel.
`ifdef' them out once all video chips actually honour this.  */

#define RASTER_PIXEL2(raster, c) (raster)->pixel_table.doub[(c)]
#define RASTER_PIXEL4(raster, c) (raster)->pixel_table.quad[(c)]

extern int raster_init(raster_t *raster, unsigned int num_modes,
                       unsigned int num_sprites);
extern raster_t *raster_new(unsigned int num_modes, unsigned int num_sprites);
extern void raster_reset(raster_t *raster);
extern int raster_realize(raster_t *raster);
extern void raster_set_exposure_handler(raster_t *raster,
                                        void *exposure_handler);
extern void raster_set_table_refresh_handler(raster_t *raster,
                                             void (*handler)(void));
extern void raster_set_geometry(raster_t *raster,
                                unsigned int screen_width,
                                unsigned int screen_height,
                                unsigned int gfx_width,
                                unsigned int gfx_height,
                                unsigned int text_width,
                                unsigned int text_height,
                                unsigned int gfx_position_x,
                                unsigned int gfx_position_y,
                                int gfx_area_moves,
                                unsigned int first_displayed_line,
                                unsigned int last_displayed_line,
                                unsigned int extra_offscreen_border);
extern void raster_invalidate_cache(raster_t *raster,
                                    unsigned int screen_height);
extern void raster_resize_viewport(raster_t *raster,
                                   unsigned int width, unsigned int height);
extern void raster_set_pixel_size(raster_t *raster, unsigned int width,
                                  unsigned int height);
extern void raster_emulate_line(raster_t *raster);
extern void raster_force_repaint(raster_t *raster);
extern int raster_set_palette(raster_t *raster, struct palette_s *palette);
extern void raster_set_title(raster_t *raster, const char *title);
extern void raster_skip_frame(raster_t *raster, int skip);
extern void raster_enable_cache(raster_t *raster, int enable);
extern void raster_enable_double_scan(raster_t *raster, int enable);
extern void raster_mode_change(void);
extern void raster_rebuild_tables(raster_t *raster);
extern void raster_handle_end_of_frame(raster_t *raster);
extern void raster_set_canvas_refresh(raster_t *raster, int enable);
extern int raster_screenshot(raster_t *raster, struct screenshot_s *screenshot);
extern void raster_free (raster_t *raster);


/* Inlined functions.  These need to be *fast*.  */

inline static void raster_add_int_change_next_line(raster_t *raster,
                                                   int *ptr,
                                                   int new_value)
{
    if (raster->skip_frame)
      *ptr = new_value;
    else
      raster_changes_add_int(&raster->changes.next_line, 0, ptr, new_value);
}

inline static void raster_add_ptr_change_next_line(raster_t *raster,
                                                   void **ptr,
                                                   void *new_value)
{
    if (raster->skip_frame)
      *ptr = new_value;
    else
      raster_changes_add_ptr(&raster->changes.next_line, 0, ptr, new_value);
}

inline static void raster_add_int_change_foreground(raster_t *raster,
                                                    int char_x,
                                                    int *ptr,
                                                    int new_value)
{
    if (raster->skip_frame || char_x <= 0)
      *ptr = new_value;
    else if (char_x < (int) raster->geometry.text_size.width) {
        raster_changes_add_int(&raster->changes.foreground,
                                char_x, ptr, new_value);
        raster->changes.have_on_this_line = 1;
    } else {
        raster_add_int_change_next_line(raster, ptr, new_value);
    }
}

inline static void raster_add_ptr_change_foreground(raster_t *raster,
                                                    int char_x,
                                                    void **ptr,
                                                    void *new_value)
{
    if (raster->skip_frame || char_x <= 0)
        *ptr = new_value;
    else if (char_x < (int)raster->geometry.text_size.width) {
        raster_changes_add_ptr(&raster->changes.foreground,
                               char_x, ptr, new_value);
        raster->changes.have_on_this_line = 1;
    } else {
        raster_add_ptr_change_next_line(raster, ptr, new_value);
    }
}

inline static void raster_add_int_change_background(raster_t *raster,
                                                    int raster_x,
                                                    int *ptr,
                                                    int new_value)
{
    if (raster->skip_frame || raster_x <= 0)
      *ptr = new_value;
    else if (raster_x < (int)raster->geometry.screen_size.width) {
        raster_changes_add_int(&raster->changes.background,
                                raster_x, ptr, new_value);
        raster->changes.have_on_this_line = 1;
    } else {
        raster_add_int_change_next_line(raster, ptr, new_value);
    }
}

inline static void raster_add_ptr_change_background(raster_t *raster,
                                                    int raster_x,
                                                    void **ptr,
                                                    void *new_value)
{
    if (raster->skip_frame || raster_x <= 0)
        *ptr = new_value;
    else if (raster_x < (int)raster->geometry.screen_size.width) {
        raster_changes_add_ptr(&raster->changes.background,
                               raster_x, ptr, new_value);
        raster->changes.have_on_this_line = 1;
    } else {
        raster_add_ptr_change_next_line(raster, ptr, new_value);
    }
}

inline static void raster_add_int_change_border(raster_t *raster,
                                                int raster_x,
                                                int *ptr,
                                                int new_value)
{
    if (raster->skip_frame || raster_x <= 0)
        *ptr = new_value;
    else if (raster_x < (int)raster->geometry.screen_size.width) {
        raster_changes_add_int(&raster->changes.border,
                               raster_x, ptr, new_value);
        raster->changes.have_on_this_line = 1;
    } else {
        raster_add_int_change_next_line(raster, ptr, new_value);
    }
}

inline static void vid_memcpy(PIXEL *dst, PIXEL *src, unsigned int count)
{
    memcpy(dst, src, count * sizeof (PIXEL));
}

#if X_DISPLAY_DEPTH > 8

inline static void vid_memset(PIXEL *dst, PIXEL value, unsigned int count)
{
    int i;

    for (i = 0; i < count; i++)
        dst[i] = value;
}

#else

inline static void vid_memset(PIXEL *dst, PIXEL value, unsigned int count)
{
    memset (dst, value, count);
}

#endif

#endif /* _RASTER_H */
