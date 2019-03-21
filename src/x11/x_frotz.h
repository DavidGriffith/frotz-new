/*
 * x_frotz.h
 *
 * X interface, declarations
 *
 * Copyright (c) 1998-2000 Daniel Schepler
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../common/frotz.h"
#include "x_setup.h"

#include <X11/Xlib.h>

/* X connection */
extern Display *dpy;

/* Window to draw into */
extern Window main_window;

extern char *x_class, *x_name;

/* The font resource for displaying text */
extern const XFontStruct *current_font_info;
extern GC current_gc, normal_gc, reversed_gc, bw_gc, cursor_gc;
extern char *font_names[9];

extern int curr_x, curr_y;

extern unsigned long bg_pixel, fg_pixel;
extern unsigned long def_bg_pixel, def_fg_pixel;

const XFontStruct * get_font(int font, int style);

void x_init_colour(char *bg_name, char *fg_name);
