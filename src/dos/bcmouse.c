/*
 * bcmouse.h - DOS interface, mouse support
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
#include "frotz.h"
#include "bcfrotz.h"

/*
 * detect_mouse
 *
 * Return true if a mouse driver is present.
 *
 */

bool detect_mouse (void)
{

    asm xor ax,ax
    asm int 0x33

    return _AX;

}/* detect_mouse */

/*
 * read_mouse
 *
 * Report any mouse clicks. Return 2 for a double click, 1 for a single
 * click or 0 if there was no mouse activity at all.
 *
 */

int read_mouse (void)
{
    int click;

    /* Read the current mouse status */

    for (click = 0; click < 2; click++) {

	if (click == 1)
	    delay (222);

	asm mov ax,6
	asm xor bx,bx
	asm int 0x33

	if (_BX == 0)
	    break;

	mouse_x = _CX;
	mouse_y = _DX;

	if (display <= _TEXT_) {
	    mouse_x /= 8;
	    mouse_y /= 8;
	}

	if (display == _MCGA_)
	    mouse_x /= 2;

	mouse_x++;
	mouse_y++;

    }

    /* Return single or double click */

    return click;

}/* read_mouse */
