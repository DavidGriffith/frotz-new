# For GNU Make.

SOURCES = x_init.c x_input.c x_pic.c x_oldpic.c x_sample.c x_screen.c x_text.c x_blorb.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = x_frotz.h x_setup.h x_blorb.h

ZFONTSBDF = Zork_r400-10.bdf Zork_r400-11.bdf Zork_r400-13.bdf Zork_r400-16.bdf Zork_r400-20.bdf
ZFONTSPCF = $(ZFONTSBDF:.bdf=.pcf)

TARGET = frotz_x11.a

ARFLAGS = rc

SOUND ?= ao

CFLAGS += -DNO_SOUND
CFLAGS += `pkg-config $(X11_PKGS) --cflags`

.PHONY: clean
.SUFFIXES: .bdf .c .o .pcf
.DELETE_ON_ERROR:

$(TARGET): $(OBJECTS) $(ZFONTSPCF)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)
	$(RANLIB) $@
	@echo "** Done with X11 interface."

clean:
	rm -f $(TARGET) $(OBJECTS) $(ZFONTSPCF)

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -fpic -o $@ -c $<

.bdf.pcf:
	bdftopcf -t $< -o $@

