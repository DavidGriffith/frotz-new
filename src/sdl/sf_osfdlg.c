#include "sf_frotz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define STATIC static

typedef struct {
  void *left, *right;
  char *value;
  } ENTRY;


extern SFONT *sf_VGA_SFONT;

#define FRAMECOLOR 222275

static char buffer[512];
static char lastdir[FILENAME_MAX] = "";
static char filename[FILENAME_MAX];
static char pattern[64];
static zword pushed = 0;
static int wentry;

static ulong *sbuffer = NULL;
static int sbpitch;		// in longs
static int ewidth, eheight;
static int X,Y,W,H, xdlg,ydlg,wdlg,hdlg;

#define HTEXT 18

STATIC void cleanlist( ENTRY *t);
STATIC void drawlist();
STATIC ENTRY * dodir(
	char *dirname, char *pattern, char *resdir, int size, int *ndirs, int *ntot);
static int Numdirs, Numtot, First;
static ENTRY *curdir = NULL, *selected;

STATIC void updatelist()
  {
  if (curdir) cleanlist(curdir);
  curdir = dodir(lastdir,pattern,lastdir,FILENAME_MAX,&Numdirs,&Numtot);
  First = 0;
  selected = NULL;
  drawlist();
  }

STATIC void goright();
STATIC void goleft();
// assumes a / at end
STATIC void goup()
  {
  char *p;
  if (strlen(lastdir) < 2) return;
  lastdir[strlen(lastdir)-1] = 0;
  p = strrchr(lastdir,'/');
  if (p){ p[1] = 0; updatelist();}
  else strcat(lastdir,"/");
  }

typedef struct {
  int x,y,w,h;	// internal
  zword (*click)(int,int);
  ulong back;
  int isbutton;
  } BAREA;

#define MAXBAREA 20
static BAREA bareas[MAXBAREA];
static int nbareas=0;
static SF_textsetting *ts;

#define BFRAME 2
#define SPC 5

#define WDLG (63*8)
#define HDLG 208

#define HCURSOR 8

#define O_BLACK	0
#define O_GRAY1	0x8a8a8a
#define O_GRAY2	0xd6d6d6
#define O_GRAY3	0xe2e2e2
#define O_WHITE	0xf5f5f5

STATIC void frame_upframe( int x, int y, int w, int h){
  ulong v = O_WHITE;
  sf_chline(x,y,v,w);
  sf_cvline(x,y,v,--h);
  v = O_BLACK;
  sf_chline(x,y+h,v,w--);
  sf_cvline(x+w--,y,v,h--);
  x++; y++;
  v = O_GRAY3;
  sf_chline(x,y,v,w);
  sf_cvline(x,y,v,--h);
  v = O_GRAY1;
  sf_chline(x,y+h,v,w--);
  sf_cvline(x+w,y,v,h);
  }

STATIC void frame_downframe( int x, int y, int w, int h){
  ulong v = O_BLACK;
  sf_chline(x,y,v,w);
  sf_cvline(x,y,v,--h);
  v = O_WHITE;
  sf_chline(x,y+h,v,w--);
  sf_cvline(x+w--,y,v,h--);
  x++; y++;
  v = O_GRAY1;
  sf_chline(x,y,v,w);
  sf_cvline(x,y,v,--h);
  v = O_GRAY3;
  sf_chline(x,y+h,v,w--);
  sf_cvline(x+w,y,v,h);
  }

// internal coords
STATIC int addarea( int x, int y, int w, int h, zword (*click)(int,int))
  {
  BAREA *a = bareas+nbareas;
  a->x = x; a->y = y; a->w = w; a->h = h; a->click = click;
  a->back = O_GRAY2;
  return nbareas++;
  }

STATIC void clarea( int n)
  {
  BAREA *a = bareas+n;
  sf_fillrect(a->back,a->x,a->y,a->w,a->h);
  }

#ifdef USE_UTF8
/* Convert UTF-8 encoded char starting at in[idx] to zchar (UCS-2) if
 * representable in 16 bits or '?' otherwise and return index to next
 * char of input array. */
STATIC int utf8_to_zchar(zchar *out, const char *in, int idx)
{
  zchar ch;
  int i;
  if ((in[idx] & 0x80) == 0) {
    ch = in[idx++];
  } else if ((in[idx] & 0xe0) == 0xc0) {
    ch = in[idx++] & 0x1f;
    if ((in[idx] & 0xc0) != 0x80)
      goto error;
    ch = (ch << 6) | (in[idx++] & 0x3f);
  } else if ((in[idx] & 0xf0) == 0xe0) {
    ch = in[idx++] & 0xf;
    for (i = 0; i < 2; i++) {
      if ((in[idx] & 0xc0) != 0x80)
        goto error;
      ch = (ch << 6) | (in[idx++] & 0x3f);
    }
  } else {
    /* Consume all subsequent continuation bytes. */
    while ((in[++idx] & 0xc0) == 0x80)
      ;
error:
    ch = '?';
  }
  *out = ch;
  return idx;
}

STATIC size_t utf8_len(const char *str)
  {
  size_t ret = 0;
  while (*str)
	 {
	 if ((*str++ & 0xc0) != 0x80)
	   ret++;
	 }
  return ret;
  }
#endif

STATIC void writetext( ulong color, const char *s, int x, int y, int w, int center)
  {
  int ox,oy,ow,oh;
  int wtext, htext;
  os_font_data(0, &htext, &wtext);
//printf("W %p [%s]\n",s,s ? s : "??");
  if (!s) return;
  if (!s[0]) return;
  sf_getclip(&ox,&oy,&ow,&oh);
  sf_setclip(x,y,w,htext);
//printf("1\n");
  if (center)
	{
#ifdef USE_UTF8
	int wt = wtext * utf8_len(s);
#else
	int wt = wtext * strlen(s);
#endif
	x += (w-wt)/2;
	}
//printf("2 ts %p\n",ts); fflush(stdout); if (ts < 1000){sf_flushdisplay(); getchar();}
  ts->cx = x;
  ts->cy = y;
  ts->fore = color;
//printf("3\n"); fflush(stdout);
#ifndef USE_UTF8
  while (*s)
      sf_writeglyph(ts->font->getglyph(ts->font, (unsigned char)(*s++), 1));
#else
  while (*s)
      {
      zchar ch;
      s += utf8_to_zchar(&ch, s, 0);
      sf_writeglyph(ts->font->getglyph(ts->font, ch, 1));
      }
#endif
//printf("4\n");
  sf_setclip(ox,oy,ow,oh);
//printf("5\n");
  }

STATIC int addbutton( int x, int y, int w, int h, char *text, zword (*click)(int,int))
  {
  int b = addarea(x,y,w,h,click);
  bareas[b].isbutton = 1;
  frame_upframe(x-2,y-2,w+4,h+4);
  clarea(b);
  if (text) writetext(0,text,x,y,w,1);
  return b;
  }

static int B_up, B_ok, B_cancel;
static int A_dir, A_filter, A_entry, A_list;

#define BUTTW 60

STATIC void showfilename( int pos)
  {
  BAREA *a = bareas+A_entry;
  clarea(A_entry);
  writetext(0,filename,a->x,a->y,a->w,0);
  if (pos >= 0)
    {
    int width, height;
    os_font_data(0, &height, &width);
    sf_cvline(a->x+width*pos,a->y,O_BLACK,height);
    }
  }

STATIC void clicked( BAREA *a)
  {
  frame_downframe(a->x-2,a->y-2,a->w+4,a->h+4);
  sf_flushdisplay();
  sf_sleep(100);
  frame_upframe(a->x-2,a->y-2,a->w+4,a->h+4);
  sf_flushdisplay();
  }

STATIC zword checkmouse( int i0)
  {
  int x = mouse_x-1, y = mouse_y-1;
  int i;
  for (i=i0;i<nbareas;i++)
    {
    BAREA *a = bareas+i;
    if (x > a->x && x < a->x+a->w && y > a->y && y < a->y+a->h)
	{
	if (a->click)
		{
		if (a->isbutton) clicked(a);
		return a->click(x-a->x,y-a->y);
		}
	else return 0;
	}
    }
  return 0;
  }

STATIC zword Zup( int x, int y)
  {
  goup();
  return 0;
  }

STATIC zword Zok( int x, int y)
  {
  return ZC_RETURN;
  }

STATIC zword Zcanc( int x, int y)
  {
  return ZC_ESCAPE;
  }

STATIC zword Zselect( int x, int y);
STATIC zword yesnoover( int xc, int yc);
STATIC zword Zentry( int x, int y);

STATIC zword inputkey(bool text)
  {
  zword c = sf_read_key(0, false, true, text);
  if (c == ZC_SINGLE_CLICK)
    {
    switch (mouse_button)
	{
	case 4: c = ZC_ARROW_LEFT; break;
	case 5: c = ZC_ARROW_RIGHT; break;
	case 1: break;
	default: c = 0; break;
	}
    }
//    if (os_read_mouse() != 1) c = 0;
  return c;
  }

int (*sf_sysdialog)( bool existing, const char *def, const char *filt, const char *tit, char **res) = NULL;

STATIC int myosdialog( bool existing, const char *def, const char *filt, const char *tit, char **res, ulong *sbuf, int sbp, int ew, int eh, int isfull)
  {
  char *pp; ulong *saved; int y0, y1, y2, x1;
  zword c = 0;
  int wtext, htext, buttw;

	// allow system-specific dialog if not fullscreen
  if (isfull == 0) if (sf_sysdialog)
	return sf_sysdialog(existing,def,filt,tit,res);

  ts = sf_curtextsetting();
  if (!ts) return SF_NOTIMP;

//printf("0 ts %p (%p)\n",ts,&ts);

  if (!def) def = "";
  strcpy(filename,def);
  pp = strrchr(filename,'/');
  if (pp)
	{
	*pp = 0;
	strcpy(lastdir,filename);
	strcpy(filename,pp+1);
	}

  if (!filt) filt = "*|All files";

  if (!lastdir[0]) strcpy(lastdir,"./");

  strcpy(buffer,filt);
  pp = strchr(buffer,'|'); if (pp) *pp = 0;
  strcpy(pattern,buffer);

  ewidth = ew;
  eheight = eh;
  sbuffer = sbuf;
  sbpitch = sbp;

  wdlg = WDLG;
  hdlg = HDLG;

  nbareas = 0;

#ifndef USE_UTF8
  htext = HTEXT;
  buttw = BUTTW;
#else
  os_font_data(FIXED_WIDTH_FONT, &htext, &wtext);
  buttw = 6 * wtext + 12;
#endif

  W = WDLG+4*BFRAME+2*SPC;
  H = HDLG+4*BFRAME+6*SPC+6*BFRAME+3*(htext+2)+HCURSOR+htext;

  if (W > ew) return SF_NOTIMP;
  if (H > eh) return SF_NOTIMP;

  X = (ew-W)/2;
  Y = (eh-H)/2;

	// internal!!
  xdlg = X+SPC+2*BFRAME;
  ydlg = Y+2*SPC+4*BFRAME+htext+htext;

  wentry = wdlg - buttw - SPC - 2*BFRAME;

  saved = sf_savearea(X,Y,W,H);
  if (!saved) return SF_NOTIMP;

//printf("saved: %p %d %d %d %d\n",saved,saved[0],saved[1],saved[2],saved[3]);
  sf_pushtextsettings();
#ifndef USE_UTF8
  ts->font = sf_VGA_SFONT;
#else
  os_set_font(FIXED_WIDTH_FONT);
#endif
  ts->style = 0;
  ts->oh = 0;
  ts->fore = 0;
  ts->backTransparent = 1;

  sf_fillrect(O_GRAY2,X,Y,W,H);
//  frame_upframe(X,Y,W,H);
  sf_rect(FRAMECOLOR,X,Y,W,H);
  sf_rect(FRAMECOLOR,X+1,Y+1,W-2,H-2);
  sf_fillrect(FRAMECOLOR,X,Y+2,W,htext);
  if (tit) writetext(O_WHITE,tit,X+2+SPC,Y+2,W-4,0);
  A_list = addarea(xdlg,ydlg,wdlg,hdlg,Zselect);
  bareas[A_list].back = O_WHITE;
  clarea(A_list);
  frame_downframe(xdlg-2,ydlg-2,wdlg+4,hdlg+4);

  y0 = Y+SPC+2*BFRAME+htext;
  y2 = Y+H-SPC-2*BFRAME-htext;
  y1 = y2-SPC-htext-2*BFRAME;
  x1 = xdlg+wentry+2*BFRAME+SPC;

  A_dir = addarea(xdlg,y0,wentry,htext,NULL);
  A_entry = addarea(xdlg,y1,wentry,htext,Zentry);
  bareas[A_entry].back = O_WHITE;
  clarea(A_entry);
  frame_downframe(xdlg-2,y1-2,wentry+4,htext+4);
  B_up = addbutton(x1,y0,buttw,htext,"^up^",Zup);
  A_filter = addarea(xdlg,y2,wentry,htext,NULL);
  strcpy(buffer,"Filter: ");
  strcat(buffer,filt);
  writetext(0,buffer,xdlg,y2,wentry,0);
  B_cancel = addbutton(x1,y2,buttw,htext,"Cancel",Zcanc);
  B_ok = addbutton(x1,y1,buttw,htext,"OK",Zok);

  showfilename(-1);
  updatelist();

  for (;;)
    {
    if (pushed) { c = pushed; pushed = 0;}
    else c = inputkey(false);
    if (c == ZC_SINGLE_CLICK) c = checkmouse(0);
    if (c == VK_INS) c = Zentry(0,-1);
    if (c == ZC_ARROW_LEFT) goleft();
    if (c == ZC_ARROW_RIGHT) goright();
    if (c == ZC_ESCAPE) break;
    if (c == ZC_RETURN)
	{
	strcpy(buffer,lastdir);
	strcat(buffer,filename);
	*res = buffer;
	if ((existing==0) && (access(buffer,F_OK)==0))
		c = yesnoover(xdlg+wdlg/2,ydlg+hdlg/2);
	if (c == ZC_RETURN) break;
	}
    }

  sf_poptextsettings();

  cleanlist(curdir); curdir = NULL;

//printf("2saved: %p %d %d %d %d\n",saved,saved[0],saved[1],saved[2],saved[3]);
  sf_restoreareaandfree(saved);

  if (c == ZC_ESCAPE) return -1;

  if (c == ZC_RETURN)
	{
	strcpy(buffer,lastdir);
	strcat(buffer,filename);
	*res = buffer;
	return 0;
	}

  return SF_NOTIMP;
  }

void sf_setdialog(void)
{
  sf_osdialog = myosdialog;
}

///////////////////////////////////

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#define strcasecmp stricmp
#else
#include <strings.h>
#endif
#include <unistd.h>
#include <dirent.h>
//#include <fnmatch.h>
#include <sys/stat.h>

// simplified fnmatch - only allows a single * at beginning
STATIC int myfnmatch( const char *pattern, const char *p, int dummy)
  {
  int lpat, lp;
  if (!pattern) return -1;
  if (!p) return -1;
  if (pattern[0] != '*') return strcmp(pattern,p);
  lpat = strlen(pattern);
  if (lpat == 1) return 0;	// * matches anything
  lpat--; pattern++;
  lp = strlen(p);
  if (lp < lpat) return 1;	// too short
  return strcmp(pattern,p+lp-lpat);
  }

STATIC void cleanlist( ENTRY *t)
  {
  while (t)
	{
	ENTRY *n = t->right;
	if (t->value) free(t->value);
	free(t);
	t = n;
	}
  }

STATIC ENTRY * newentry( char *s)
  {
  ENTRY *r = calloc(1,sizeof(ENTRY));

  if (r){
	r->value = strdup(s);
	if (!r->value){ free(r); return NULL;}
	}
  return r;
  }

STATIC void addentry( char *s, ENTRY **ae)
  {
  ENTRY *t = *ae;
  if (!t)
	{
	*ae = newentry(s);
	return;
	}
  for (;;)
	{
	int k = strcasecmp(s,t->value);
	if (!k) return;
	if (k > 0)
		{
		if (t->right) t = t->right;
		else
			{
			t->right = newentry(s);
			return;
			}
		}
	else
		{
		if (t->left) t = t->left;
		else
			{
			t->left = newentry(s);
			return;
			}
		}
	}
  }

STATIC char *resolvedir( char *dir, char *res, int size)
  {
  char cwd[FILENAME_MAX], *p; int i;
  if (!getcwd(cwd,FILENAME_MAX)) return NULL;
  if (chdir(dir)) return NULL;
  p = getcwd(res,size);
  for (i=0;p[i];i++) if (p[i]=='\\') p[i] = '/';
  chdir(cwd);
  if (p)
	{
	int n = strlen(p);
	if (n) if (p[n-1] != '/') { p[n] = '/'; p[n+1] = 0;}
	}
  return p;
  }

static void exhaust( ENTRY *e, ENTRY **resp, int *n)
    {
    if (!e) return;
    exhaust(e->left, resp, n);
    e->left = *resp;
    *resp = e;
    (*n)++;
    exhaust(e->right, resp, n);
    }

STATIC ENTRY * dodir(
	char *dirname, char *pattern, char *resdir, int size, int *ndirs, int *ntot)
  {
  DIR *dir;
  ENTRY *dirs = NULL;
  ENTRY *files = NULL, *res = NULL;
  struct dirent *d;
  char *p, *resdend;
  struct stat fst;
  int n;

//printf("\ndodir\n");
  if (!resolvedir(dirname,resdir,size)) return NULL;
  resdend = resdir+strlen(resdir);

//printf("[%s]\n",resdir);
	// MinGW opendir() does not like the final slash
#ifdef WIN32
  n = strlen(resdir);
  if (n > 2 && (resdir[n-2] != ':'))
	resdir[n-1] = 0;
  dir = opendir(resdir);
  resdir[n-1] = '/';
#else
  dir = opendir(resdir);
#endif
  if (!dir) return NULL;

//printf("opened [%s]\n",resdir);
  for (;;)
    {
    d = readdir(dir);
    if (!d) break;
    p = d->d_name;
    if (strcmp(p,".")==0) continue;
    if (strcmp(p,"..")==0) continue;
    strcpy(resdend,p);
//printf("-%s\n",resdir);
    if (stat(resdir,&fst)) continue;
//printf("--mode %x\n",fst.st_mode);
    if (S_ISDIR(fst.st_mode))
	addentry(p,&dirs);
    else
	{
//printf("--fnmatch: %d\n",fnmatch(pattern,p,0));
	if (myfnmatch(pattern,p,0)==0) addentry(p,&files);
	}
    }

  closedir(dir);
  *resdend = 0;

  n = 0;
  exhaust(dirs, &res, &n);
  *ndirs = n;
  exhaust(files, &res, &n);
  *ntot = n;

  if (res)
    while (res->left)
	{
	((ENTRY *)(res->left))->right = res;
	res = res->left;
	}

  return res;
  }

#ifdef USE_UTF8
// Convert character count into index in UTF-8 encoded string.
// Works by skipping over continuation bytes.
STATIC int utf8_char_pos(char *s, int pos)
  {
  int cpos;
  int idx = 0;
  for (cpos = 0; s[cpos] && idx < pos; cpos++)
    {
    if ((s[cpos+1] & 0xc0) != 0x80)
      idx++;
    }
  return cpos;
}
#endif

//////////////////////////////////////////////
// white,black,gray,yellow
static ulong bcolors[4] = {0xfcfcfc,0,0xa0a0a0,0xa0d0e0};

static unsigned char folderbmp[] = {
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,
	0,1,3,3,3,3,3,3,
	1,1,1,1,1,1,0,0,
	0,1,3,3,3,3,3,3,
	3,3,3,3,3,3,1,0,
	0,1,3,3,3,3,3,3,
	3,1,1,1,1,1,1,0,
	0,1,3,3,1,1,1,1,
	1,3,3,3,3,3,1,0,
	0,1,3,1,3,3,3,3,
	3,3,3,3,3,3,1,0,
	0,1,3,1,3,3,3,3,
	3,3,3,3,3,3,1,0,
	0,1,3,1,3,3,3,3,
	3,3,3,3,3,3,1,0,
	0,1,3,1,3,3,3,3,
	3,3,3,3,3,3,1,0,
	0,1,3,1,3,3,3,3,
	3,3,3,3,3,3,1,0,
	0,1,3,1,3,3,3,3,
	3,3,3,3,3,3,1,0,
	0,0,1,1,1,1,1,1,
	1,1,1,1,1,1,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0};


static unsigned char docbmp[] = {
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,1,1,1,1,1,1,
	1,1,1,1,1,1,0,0,
	0,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,0,
	0,1,0,2,2,2,2,2,
	2,2,2,2,2,0,1,0,
	0,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,0,
	0,1,0,2,2,2,2,2,
	2,2,2,2,2,0,1,0,
	0,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,0,
	0,1,0,2,2,2,2,2,
	2,2,2,2,2,0,1,0,
	0,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,0,
	0,1,0,2,2,2,2,2,
	2,2,2,2,2,0,1,0,
	0,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,0,
	0,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,0,
	0,0,1,1,1,1,1,1,
	1,1,1,1,1,1,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0};

////////////////////////////////

STATIC void drawit( int x, int y, ENTRY *e, int w, int issub)
  {
  int i, j, n, color;
  unsigned char *bmp;
  char *s = e->value;
  int width, height;
  os_font_data(0, &height, &width);
  if (height < 16) height = 16;
  bmp = (issub ? folderbmp : docbmp);
  for (i=0;i<16;i++) for (j=0;j<16;j++) sf_wpixel(x+j,y+i+height/2-8,bcolors[*bmp++]);
  x += 17;
  w -= 17;
  n = w/width;
  if (n < 1) return;
  if (strlen(s) > n)
	{
	strcpy(buffer,s);
#ifndef USE_UTF8
	buffer[n] = 0;
	buffer[n-1] = '>';
	s = buffer;
#else
	i = utf8_len(buffer);
	if (i > n-1)
	  {
	  j = utf8_char_pos(buffer, n-1);
	  buffer[j+1] = 0;
	  buffer[j] = '>';
	  s = buffer;
	  }
#endif
	}
  if (e == selected)
	{
	color = O_WHITE;
	sf_fillrect(0,x,y,w,16);
	}
  else
	color = O_BLACK;
  writetext(color,s,x,y,w,0);
  }

static int Nrows, Ncols, Ewid, Fh;

STATIC void drawnames( int x, int y, int w, int h, ENTRY *files, int first, int nsub, int ntot, int ewid)
  {
  int i;

#ifdef USE_UTF8
  int width;
  os_font_data(0, &Fh, &width);
  if (Fh < 16) Fh = 16;
#else
  Fh = 16;
#endif
  Ewid = ewid;
  Ncols = w/ewid;
  Nrows = h/Fh;

  sf_fillrect(O_WHITE,x,y,w,h);
  if (!files) return;
  if (first < 0) return;
  if (nsub > ntot) nsub = ntot;
  while (first > 0)
	{
	files = files->right;
	if (!files) return;
	nsub--;
	ntot--;
	first--;
	}
  if (ntot <= 0) return;
  if (Ncols < 1) return;
  if (Nrows < 1) return;
  if (Nrows*Ncols < ntot) ntot = Nrows*Ncols;
  for (i=0;i<ntot;i++)
	{
	drawit(x+ewid*(i/Nrows),y+Fh*(i % Nrows),files,ewid,i < nsub);
	files = files->right;
	}
  }

STATIC void drawlist()
  {
  BAREA *a = bareas+A_list, *b = bareas+A_dir;

  clarea(A_dir);
  writetext(0,lastdir,b->x,b->y,b->w,0);
  drawnames(a->x,a->y,a->w,a->h,curdir,First,Numdirs,Numtot,21*8);

  }

STATIC void goright()
  {
  if (First+Nrows*Ncols > Numtot) return;
  First += Nrows;
  drawlist();
  }

STATIC void goleft()
  {
  if (!First) return;
  First -= Nrows;
  drawlist();
  }

STATIC ENTRY *filesat( int n){
  ENTRY *e = curdir;
  while (n--)
	{
	if (e) e = e->right;
	}
  return e;
  }

STATIC zword Zselect( int x, int y)
  {
  int n;
  x /= Ewid;
  y /= Fh;
  n = First + y + x*Nrows;
  if (n >= Numtot)
	{
	if (selected)
		{
		selected = NULL;
		drawlist();
		}
	return 0;
	}
  if (n < Numdirs)
	{
	ENTRY *e = filesat(n);
	if (!e) return 0;
	strcat(lastdir,e->value);
	updatelist();
	return 0;
	}
  selected = curdir;
  while (n--) selected = selected->right;
  strcpy(filename,selected->value);
  showfilename(-1);
  drawlist();
  return 0;
  }

extern void sf_videodata( ulong **sb, int *sp, int *ew, int *eh);
zword sf_yesnooverlay( int xc, int yc, char *t, int saverest)
  {
  zword c = ZC_RETURN;
  int nsav = nbareas;
  ulong *saved = NULL;
  int hx = BUTTW+3*SPC, hy = HTEXT+2*SPC, heff;

  heff = 8*strlen(t);
  if (heff > 2*hx) hx = (heff+3)/2;
  if (saverest)
	{
  	ts = sf_curtextsetting();
  	if (!ts) return ZC_ESCAPE;
	saved = sf_savearea(xc-hx-2,yc-hy-2,2*hx+4,2*hy+4);
	if (!saved) return ZC_ESCAPE;
  	sf_pushtextsettings();
  	ts->font = sf_VGA_SFONT;
  	ts->style = 0;
  	ts->oh = 0;
  	ts->fore = 0;
  	ts->backTransparent = 1;
	sf_videodata(&sbuffer, &sbpitch, &ewidth, &eheight);
	}

  sf_fillrect(FRAMECOLOR,xc-hx-2,yc-hy-2,2*hx+4,2*hy+4);
  sf_fillrect(O_WHITE,xc-hx,yc-hy,2*hx,2*hy);
  writetext(O_BLACK,t,xc-hx,yc-SPC-HTEXT,2*hx,1);
  addbutton(xc-SPC-BUTTW,yc+SPC,BUTTW,HTEXT,"Yes",Zok);
  addbutton(xc+SPC,yc+SPC,BUTTW,HTEXT,"No",Zcanc);
  for (;;)
    {
    c = inputkey(false);
    if (c == 'n' || c == 'N') c = ZC_ESCAPE;
    if (c == 'y' || c == 'Y') c = ZC_RETURN;
    if (c == ZC_SINGLE_CLICK) c = checkmouse(nsav);
    if (c == ZC_ESCAPE) break;
    if (c == ZC_RETURN) break;
    }

  if (saved)
	{
	sf_restoreareaandfree(saved);
	sf_poptextsettings();
	}

  nbareas = nsav;
  return c;
  }

STATIC zword yesnoover( int xc, int yc)
  {
  zword c;

  c = sf_yesnooverlay(xc,yc,"Overwrite file?",0);

  drawlist();
  return c;
  }

// this is needed for overlapping source and dest in Zentry
// (lib does not guarantee correct behaviour in that case)
static void mystrcpy( char *d, const char *s)
  {
  while ((*d++ = *s++));
  }

STATIC zword Zentry( int x, int y)
  {
  static int pos = 10000;
  int i,n,nmax; zword c;
  int cpos,clen,nchars;
  int width, height;
  os_font_data(0, &height, &width);
  nmax = wentry/width;
  if (nmax >= FILENAME_MAX) nmax = FILENAME_MAX-1;
  n = strlen(filename);
#ifndef USE_UTF8
  if (n > nmax) { n = nmax; filename[n] = 0;}
  nchars=n;
#else
  nchars=utf8_len(filename);
  if (nchars > nmax) { nchars = nmax; n = utf8_char_pos(filename, nchars); filename[n] = 0;}
#endif

  if (y >= 0)
    {
    pos = x/(width/2)-1; if (pos < 0) pos = 0;
    pos /= 2;
    }
  if (pos > nchars) pos = nchars;
  showfilename(pos);
  for (;;)
    {
    c = inputkey(true);
    if (c == ZC_SINGLE_CLICK)
	{
	pushed = c;
	c = 0;
	break;
	}
    if (c == ZC_ESCAPE || c == VK_INS) { c = 0; break; }
    if (c == ZC_RETURN) break;
    if (c == ZC_ARROW_LEFT)
	{
	if (pos){ pos--; showfilename(pos); }
	continue;
	}
    if (c == ZC_ARROW_RIGHT)
	{
	if (pos < nchars){ pos++; showfilename(pos); }
	continue;
	}
    if (c == ZC_BACKSPACE)
	{
	if (pos)
		{
		clen = 1;
#ifndef USE_UTF8
		cpos = pos;
#else
		cpos = utf8_char_pos(filename, pos);
		while (cpos>clen && (filename[cpos-clen]&0xc0)==0x80) clen++;
#endif
			// needs mystrcpy() because overlapping src-dst
		if (cpos < n) mystrcpy(filename+cpos-clen,filename+cpos);
		n-=clen;
		nchars--;
		filename[n] = 0;
		pos--;
		showfilename(pos);
		}
	continue;
	}
#ifndef USE_UTF8
    if ((c >= 32 && c < 127) || (c >= 160 && c < 256))
#else
    if ((c >= 32 && c < 127) || (c >= 160 ))
#endif
	{
#ifndef USE_UTF8
	cpos = pos;
	clen = 1;
#else
	cpos = utf8_char_pos(filename, pos);
	if (c < 0x80) clen = 1;
	else if (c < 0x800) clen = 2;
	else if (c < 0x10000) clen = 3;
	else {clen = 1; c = '?';}
#endif
	if (nchars >= nmax) continue;
	if (n > cpos)
	  for (i=n-1;i>=cpos;i--) filename[i+clen] = filename[i];
#ifndef USE_UTF8
	filename[cpos] = c;
#else
	if (c > 0x7ff) {
	  filename[cpos] = 0xe0 | ((c >> 12) & 0xf);
	  filename[cpos+1] = 0x80 | ((c >> 6) & 0x3f);
	  filename[cpos+2] = 0x80 | (c & 0x3f);
	} else if (c > 0x7f) {
	  filename[cpos] = 0xc0 | ((c >> 6) & 0x1f);
	  filename[cpos+1] = 0x80 | (c & 0x3f);
	} else {
	  filename[cpos] = c;
	}
#endif
	n+=clen;
	nchars++;
	filename[n] = 0;
	pos++;
	showfilename(pos);
	}
    }
  showfilename(-1);
  return c;
  }


