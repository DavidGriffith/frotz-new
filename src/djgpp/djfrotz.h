/*
 * "djfrotz.h"
 *
 * DJGPP interface, declarations
 *
 */

#define MASK_LINEAR(addr)     (addr & 0x000FFFFF)
#define RM_TO_LINEAR(addr)    (((addr & 0xFFFF0000) >> 12) + (addr & 0xFFFF))
#define RM_OFFSET(addr)       (addr & 0xF)
#define RM_SEGMENT(addr)      ((addr >> 4) & 0xFFFF)

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

/* djinit  */	int	dectoi (const char *);
/* djinit  */	int	hextoi (const char *);
/* djmouse */	bool 	detect_mouse (void);
/* djmouse */	int 	read_mouse (void);
/* djpic   */	bool 	init_pictures (void);
/* djpic   */	void 	reset_pictures (void);

/* djsmpl  */	void 	cleanup_sound (void);
/* djsmpl  */	void 	check_end_of_sound (void);

/* djtext  */	void	switch_scrn_attr (bool);
/* djtext  */	void 	load_fonts (void);
