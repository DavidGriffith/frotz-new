# Makefile for Unix Frotz
# GNU make is required.

PKG_CONFIG ?= pkg-config

SOURCES = blorbreader.c charutils.c colourutils.c dialogs.c fileutils.c \
	gfx_font_data.c Interpreter.c kbcomboboxtext.c main.c MainWindow.c \
	MediaPlayer.c MetaData.c Picture.c Settings.c SettingsDialog.c \
	Sound.c StoryReader.c StoryTerminal.c ZMachine.c ZTerminal.c

HEADERS = blorbreader.h charutils.h colourutils.h dialogs.h fileutils.h \
	Interpreter.h kbcomboboxtext.h MainWindow.h MediaPlayer.h \
	MetaData.h Picture.h SettingsDialog.h Settings.h Sound.h \
	StoryReader.h StoryTerminal.h ZMachine.h ZTerminal.h

OBJECTS = $(SOURCES:.c=.o)

TARGET = frotz_gtk.a

ARFLAGS = rc


.PHONY: clean
.DELETE_ON_ERROR:

$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $?
	$(RANLIB) $@
	@echo "** Done with curses interface."

clean:
	rm -f $(TARGET) $(OBJECTS)

%.o: override CFLAGS += $(shell $(PKG_CONFIG) $(GTK_PKGS) --cflags) $(GTK_CFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(CURSES_CFLAGS) $(NO_SOUND) -fPIC -fpic -o $@ -c $<
