/*
 * dosfrotz.h
 *
 * DOS interface, declarations, definitions, and defaults.
 *
 */

#ifndef DOS_OWFROTZ_H
#define DOS_OWFROTZ_H

#define MASK_LINEAR(addr)	(addr & 0x000FFFFF)
#define RM_TO_LINEAR(addr)	(((addr & 0xFFFF0000) >> 12) + (addr & 0xFFFF))
#define RM_OFFSET(addr)		(addr & 0xF)
#define RM_SEGMENT(addr)	((addr >> 4) & 0xFFFF)

#define OS_PATHSEP ';'

#define byte0(v)	((byte *)&v)[0]
#define byte1(v)	((byte *)&v)[1]
#define byte2(v)	((byte *)&v)[2]
#define byte3(v)	((byte *)&v)[3]
#define word0(v)	((word *)&v)[0]
#define word1(v)	((word *)&v)[1]

#ifndef HISTORY_MIN_ENTRY
#define HISTORY_MIN_ENTRY 1
#endif

#define SPECIAL_KEY_MIN 256
#define SPECIAL_KEY_HOME 256
#define SPECIAL_KEY_END 257
#define SPECIAL_KEY_WORD_LEFT 258
#define SPECIAL_KEY_WORD_RIGHT 259
#define SPECIAL_KEY_DELETE 260
#define SPECIAL_KEY_INSERT 261
#define SPECIAL_KEY_PAGE_UP 262
#define SPECIAL_KEY_PAGE_DOWN 263
#define SPECIAL_KEY_TAB 264
#define SPECIAL_KEY_MAX 264

#define _MONO_	0
#define _TEXT_	1
#define _CGA_	2
#define _MCGA_	3
#define _EGA_	4
#define _AMIGA_	5

/*
 * For ease of porting from Borland Turbo C to Open Watcom C, define these
 * names for text mode colours.  Borland C has these in <conio.h> and
 * <graphics.h> --- as `enum COLORS' --- but Watcom does not.
 */
enum bc_compatible_colours {
	BLACK,
	BLUE,
	GREEN,
	CYAN,
	RED,
	MAGENTA,
	BROWN,
	LIGHTGRAY,
	DARKGRAY,
	LIGHTBLUE,
	LIGHTGREEN,
	LIGHTCYAN,
	LIGHTRED,
	LIGHTMAGENTA,
	YELLOW,
	WHITE
};


typedef unsigned char byte;
typedef unsigned short word;

extern int display;

extern int cursor_x;
extern int cursor_y;

extern char latin1_to_ibm[];
extern char latin1_to_ascii[];

extern byte text_bg;
extern byte text_fg;

extern byte scrn_attr;

extern int user_background;
extern int user_foreground;
extern int user_emphasis;
extern int user_reverse_bg;
extern int user_reverse_fg;
extern int user_screen_height;
extern int user_screen_width;
extern int user_tandy_bit;
extern int user_bold_typing;
extern int user_random_seed;
extern int user_font;

extern char stripped_story_name[];
extern char *prog_name;

extern int current_bg;
extern int current_fg;
extern int current_style;
extern int current_font;

extern int scaler;

#ifdef SOUND_SUPPORT
extern volatile int end_of_sound_flag;
#endif

/* owinit  */	int	dectoi (const char *);
/* owinit  */	int	hextoi (const char *);
/* owmouse */	bool 	detect_mouse (void);
/* owmouse */	int 	read_mouse (void);
/* owpic   */	bool 	init_pictures (void);
/* owpic   */	void 	reset_pictures (void);

#ifdef SOUND_SUPPORT
/* owsmpl  */	bool 	dos_init_sound (void);
/* owsmpl  */	void 	dos_reset_sound (void);
/* owinput */	void	end_of_sound(void);
#endif
/* owtext  */	void	switch_scrn_attr (bool);
/* owtext  */	void 	load_fonts (void);

/*
 * Inline functions for calling BIOS video and date/time services, and DOS
 * mouse services.
 */
word bios_video_ah(byte ah);
#pragma aux bios_video_ah = "int 0x10" parm [ah] value [ax] modify [bx cx dx];
word bios_video_ah_al_bh_bl_cx(byte ah, byte al, byte bh, byte bl, word cx);
#pragma aux bios_video_ah_al_bh_bl_cx = "int 0x10" \
					parm [ah] [al] [bh] [bl] [cx] \
					value [ax] modify [bx cx dx];
word bios_video_ah_bh_dh_dl(byte ah, byte bh, byte dh, byte dl);
#pragma aux bios_video_ah_bh_dh_dl = "int 0x10" parm [ah] [bh] [dh] [dl] \
				     value [ax] modify [bx cx dx];
word bios_video_ah_bh_cx_dx(byte ah, byte bh, word cx, word dx);
#pragma aux bios_video_ah_bh_cx_dx = "int 0x10" parm [ah] [bh] [cx] [dx] \
				     value [ax] modify [bx cx dx];
word bios_video_ah_cx(byte ah, word cx);
#pragma aux bios_video_ah_cx = "int 0x10" parm [ah] [cx] value [ax] \
			       modify [bx cx dx];
word bios_video_ax(word ax);
#pragma aux bios_video_ax = "int 0x10" parm [ax] value [ax] modify [bx cx dx];
word bios_video_ax_bl(word ax, byte bl);
#pragma aux bios_video_ax_bl = "int 0x10" parm [ax] [bl] value [ax] \
			       modify [bx cx dx];
/*
 * This is used for int 0x10, ah = 6 or 7, which may destroy bp on some
 * buggy BIOSes (per Ralf Brown's Interrupt List).
 */
word bios_video_ax_bh_ch_cl_dh_dl(word ax, byte bh, byte ch, byte cl,
						    byte dh, byte dl);
#pragma aux bios_video_ax_bh_ch_cl_dh_dl = "push bp", \
					   "int 0x10", \
					   "pop bp" \
    parm [ax] [bh] [ch] [cl] [dh] [dl] value [ax] modify [bx cx dx];
word bios_video_ax_bx(word ax, word bx);
#pragma aux bios_video_ax_bx = "int 0x10" parm [ax] [bx] value [ax] \
			       modify [bx cx dx];
word bios_video_ax_bx_cx_esdx(word ax, word bx, word cx, void _far *esdx);
#pragma aux bios_video_ax_bx_cx_esdx = "int 0x10" parm [ax] [bx] [cx] [es dx] \
				       value [ax] modify [bx cx dx];
word bios_video_ax_bx_dh_ch_cl(word ax, word bx, byte dh, byte ch, byte cl);
#pragma aux bios_video_ax_bx_dh_ch_cl = "int 0x10" \
					parm [ax] [bx] [dh] [ch] [cl] \
					value [ax] modify [bx cx dx];
word bios_video_ax_esdx_bh(word ax, void _far *esdx, byte bh);
#pragma aux bios_video_ax_esdx_bh = "int 0x10" parm [ax] [es dx] [bh] \
				    value [ax] modify [bx cx dx];
long bios_time_ah(byte ah);
#pragma aux bios_time_ah = "int 0x1a" parm [ah] value [cx dx] modify [ax bx];
word dos_mouse_ax(word ax);
#pragma aux dos_mouse_ax = "int 0x33" parm [ax] value [ax] modify [bx cx dx];
__int64 dos_mouse_ax_bx(word ax, word bx);
#pragma aux dos_mouse_ax_bx = "int 0x33" parm [ax] [bx] value [ax bx cx dx];
/*
 * Used for latching operations in EGA or VGA screen output.
 */
void video_latch(volatile byte);
#pragma aux video_latch = "" parm [al] modify [];

#ifdef BUILD_WATCOM_C
#include "inline.h"
#endif

#endif
