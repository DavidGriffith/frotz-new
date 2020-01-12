/*
 * x_screen.c
 *
 * X interface, screen manipulation
 *
 * Copyright (c) 1998-2000 Daniel Schepler
 *
 */

#include "x_frotz.h"
#include <stdio.h>


/*
 * os_erase_area
 *
 * Fill a rectangular area of the screen with the current background
 * colour. Top left coordinates are (1,1). The cursor does not move.
 *
 */
void os_erase_area(int top, int left, int bottom, int right, int UNUSED(win))
{
	XFillRectangle(dpy, main_window, reversed_gc,
		       left - 1, top - 1, right - left + 1, bottom - top + 1);
} /* os_erase_area */


/*
 * os_scroll_area
 *
 * Scroll a rectangular area of the screen up (units > 0) or down
 * (units < 0) and fill the empty space with the current background
 * colour. Top left coordinates are (1,1). The cursor stays put.
 *
 */
void os_scroll_area(int top, int left, int bottom, int right, int units)
{
	if (units == 0)
		return;
	else if (units > bottom - top || units < top - bottom)
		XFillRectangle(dpy, main_window, reversed_gc,
			       left - 1, top - 1, right - left + 1,
			       bottom - top + 1);
	else if (units > 0) {
		XCopyArea(dpy, main_window, main_window, current_gc,
			  left - 1, top - 1 + units,
			  right - left + 1, bottom - top - units + 1,
			  left - 1, top - 1);
		XFillRectangle(dpy, main_window, reversed_gc,
			       left - 1, bottom - units, right - left + 1,
			       units);
	} else {
		XCopyArea(dpy, main_window, main_window, current_gc,
			  left - 1, top - 1, right - left + 1,
			  bottom - top + units + 1, left - 1, top - 1 + units);
		XFillRectangle(dpy, main_window, reversed_gc, left - 1, top - 1,
			       right - left + 1, -units);
	}
} /* os_scroll_area */


bool os_repaint_window(int win, int ypos_old, int ypos_new, int xpos,
		       int ysize, int xsize)
{
	return TRUE;
}
