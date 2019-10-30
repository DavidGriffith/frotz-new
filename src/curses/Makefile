# Makefile for Unix Frotz
# GNU make is required.

SOURCES = ux_audio.c ux_audio_none.c ux_audio_oss.c ux_blorb.c ux_init.c \
	ux_input.c ux_pic.c ux_screen.c ux_text.c

HEADERS = ux_blorb.h ux_frotz.h ux_setup.h defines.h

OBJECTS = $(SOURCES:.c=.o)

TARGET = frotz_curses.a

ARFLAGS = rc

SOUND_TYPE ?= ao

ifeq ($(SOUND_TYPE), ao)
  CFLAGS += -pthread
else ifeq ($(SOUND_TYPE), none)

else ifndef SOUND_TYPE

else
  $(error Invalid sound choice $(SOUND_TYPE))
endif

.PHONY: clean
.DELETE_ON_ERROR:

$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $?
	$(RANLIB) $@
	@echo "** Done with curses interface."

clean:
	rm -f $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) $(CURSES_CFLAGS) $(NO_SOUND) -fPIC -fpic -o $@ -c $<
