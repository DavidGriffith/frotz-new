/*
 * djscreen.c - DJGPP interface, screen manipulation
 *
 * This file is part of Frotz.
 *
 * Frotz is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Frotz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or visit http://www.fsf.org/
 */

#include <dos.h>
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include "frotz.h"
#include "djfrotz.h"


/*
 * get_scrnptr
 *
 * Return a pointer to the given line in video RAM.
 *
 */
unsigned long get_scrnptr(int y)
{
	if (display == _CGA_)
		return ((y & 1) ? 0xba000 : 0xb8000) + 40 * (y & ~1);
	else if (display == _MCGA_)
		return 0xa0000 + 320 * y;
	else
		return 0xa0000 + 80 * y;
} /* get_scrnptr */


/*
 * clear_byte
 *
 * Helper function for clear_line.
 *
 */
static void clear_byte(unsigned long scrn, word mask)
{
	_farsetsel(_dos_ds);
	if (display == _CGA_) {
		if (scrn_attr == 0)
			_farnspokeb(scrn, _farnspeekb(scrn) & ~mask);
		else
			_farnspokeb(scrn, _farnspeekb(scrn) | mask);
	} else {
		outportw(0x03ce, 0x0205);
		outportb(0x03ce, 0x08);
		outportb(0x03cf, mask);
		(void)_farnspeekb(scrn); // NOP????
		_farnspokeb(scrn, scrn_attr);
	}
} /* clear_byte */


/*
 * clear_line
 *
 * Helper function for os_erase_area.
 *
 */
static void clear_line(int y, int left, int right)
{
	unsigned long scrn = get_scrnptr(y);
	int i;

	_farsetsel(_dos_ds);
	if (display == _MCGA_) {
		scrn += left;
		for (i = 0; i < right - left + 1; i++) {
			_farnspokeb(scrn, scrn_attr);
			scrn++;
		}
	} else {
		word mask1 = 0x00ff >> (left & 7);
		word mask2 = 0xff80 >> (right & 7);

		int x = right / 8 - left / 8;

		scrn += left / 8;

		if (x == 0) {
			mask1 &= mask2;
			mask2 = 0;
		}

		/* Clear first byte */
		clear_byte(scrn++, mask1);

		/* Clear middle bytes */
		if (display >= _EGA_)
			outportw(0x03ce, 0xff08);

		while (--x > 0) {
			_farnspokeb(scrn, scrn_attr);
			scrn++;
		}

		/* Clear last byte */
		clear_byte(scrn, mask2);
	}
} /* clear_line */


/*
 * os_erase_area
 *
 * Fill a rectangular area of the screen with the current background
 * colour. Top left coordinates are (1,1). The cursor does not move.
 *
 * The final argument gives the window being changed, -1 if only a
 * portion of a window is being erased, or -2 if the whole screen is
 * being erased.  This is not relevant for the DOS interface, and so
 * this function ignores that argument.
 *
 */
void os_erase_area(int top, int left, int bottom, int right, int win)
{
	int y;
	__dpmi_regs regs;

	top--;
	left--;
	bottom--;
	right--;

	if (display <= _TEXT_) {
		regs.x.ax = 0x0600;
		regs.h.ch = top;
		regs.h.cl = left;
		regs.h.dh = bottom;
		regs.h.dl = right;
		regs.h.bh = scrn_attr;
		__dpmi_int (0x10, &regs);
	} else {
		for (y = top; y <= bottom; y++)
			clear_line(y, left, right);
	}
} /* os_erase_area */


/*
 * copy_byte
 *
 * Helper function for copy_line.
 *
 */
static void copy_byte(unsigned long scrn1, unsigned long scrn2, byte mask)
{
	byte b;
	int i;

	if (display == _CGA_) {
		_farnspokeb(scrn1, (_farnspeekb(scrn1) & ~mask) | (_farnspeekb(scrn2) & mask));
	} else {
		outportw(0x03ce, 0x0005);
		outportb(0x03ce, 0x08);
		outportb(0x03cf, mask);
		outportb(0x03ce, 0x04);
		outportb(0x03c4, 0x02);

		for (i = 0; i < 4; i++) {
			outportb(0x03cf, i);
			outportb(0x03c5, 1 << i);
			b = _farnspeekb(scrn2);
			(void)_farnspeekb(scrn1); // NOP???
			_farnspokeb(scrn1, b);
		}
		outportb(0x03c5, 0x0f);
	}
} /* copy_byte */


/*
 * copy_line
 *
 * Helper function for os_scroll_area.
 *
 */
static void copy_line(int y1, int y2, int left, int right)
{
	unsigned long scrn1 = get_scrnptr(y1);
	unsigned long scrn2 = get_scrnptr(y2);

	if (display == _MCGA_) {
		movedata(_dos_ds, scrn2 + left, _dos_ds, scrn1 + left, right - left + 1);
	} else {
		word mask1 = 0x00ff >> (left & 7);
		word mask2 = 0xff80 >> (right & 7);

		int x = right / 8 - left / 8;

		scrn1 += left / 8;
		scrn2 += left / 8;

		if (x == 0) {
			mask1 &= mask2;
			mask2 = 0;
		}

		/* Copy first byte */
		copy_byte(scrn1++, scrn2++, mask1);

		/* Copy middle bytes */
		if (display >= _EGA_)
			outportw(0x03ce, 0x0105);

		_farsetsel(_dos_ds);
		while (--x > 0) {
			_farnspokeb(scrn1, _farnspeekb(scrn2));
			scrn1++;
			scrn2++;
		}

		/* Copy last byte */
		copy_byte(scrn1, scrn2, mask2);
	}
} /* copy_line */


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
	int y;
	__dpmi_regs regs;

	top--;
	left--;
	bottom--;
	right--;

	if (display <= _TEXT_) {
		regs.h.ah = 6;
		if (units < 0) {
			regs.h.ah = 7;
			units = -units;
		}
		regs.h.al = units;
		regs.h.ch = top;
		regs.h.cl = left;
		regs.h.dh = bottom;
		regs.h.dl = right;
		regs.h.bh = scrn_attr;
		__dpmi_int (0x10, &regs);
	} else if (units > 0) {
		for (y = top; y <= bottom; y++) {
			if (y <= bottom - units)
				copy_line(y, y + units, left, right);
			else
				clear_line(y, left, right);
		}
	} else {
		for (y = bottom; y >= top; y--) {
			if (y >= top - units)
				copy_line(y, y + units, left, right);
			else
				clear_line(y, left, right);
		}
	}
} /* os_scroll_area */


bool os_repaint_window(int win, int ypos_old, int ypos_new, int xpos,
		       int ysize, int xsize)
{
	return FALSE;
}
