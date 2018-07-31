# Versioning

MAJOR=2
MINOR=44
VERSION = $(MAJOR).$(MINOR)

# If we're working from git, we have access to proper variables. If
# not, make it clear that we're working from a release.
GIT_DIR ?= .git
ifneq ($(and $(wildcard $(GIT_DIR)),$(shell which git)),)
	GIT_BRANCH = $(shell git rev-parse --abbrev-ref HEAD)
	GIT_HASH = $(shell git rev-parse HEAD)
	GIT_HASH_SHORT = $(shell git rev-parse --short HEAD)
	GIT_TAG = $(shell git describe --abbrev=0 --tags)
else
	GIT_BRANCH = none
	GIT_HASH = none
	GIT_HASH_SHORT = none
	GIT_TAG = $(VERSION)
endif


BUILD_DATE_TIME = $(shell date +%Y%m%d.%k%M%S | sed s/\ //g)

export CFLAGS

# Enable compiler warnings. This is an absolute minimum.
CFLAGS += -Wall -std=c99 #-Wextra 

# strdup, strndup
CFLAGS += -D_POSIX_C_SOURCE=200809L

# Define your optimization flags.
#
# These are good for regular use.
#CFLAGS += -O2 -fomit-frame-pointer -falign-functions=2 -falign-loops=2 -falign-jumps=2
# These are handy for debugging.
CFLAGS += -g

# Define where you want Frotz installed
PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man
SYSCONFDIR ?= /etc
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib

## INCLUDEDIR path for Apple MacOS Sierra 10.12 plus MacPorts
#INCLUDEDIR ?= /opt/local/include
## LIBDIR path for Apple MacOS Sierra 10.12 plus MacPorts
#LIBDIR ?= /opt/local/lib

CFLAGS += -I$(INCLUDEDIR)
LDFLAGS += -L$(LIBDIR)

RANLIB ?= $(shell which ranlib)

# Choose your sound support
# OPTIONS: ao, none
export SOUND ?= ao

# Default sample rate for sound effects.
# All modern sound interfaces can be expected to support 44100 Hz sample
# rates.  Earlier ones, particularly ones in Sun 4c workstations support
# only up to 8000 Hz.
SAMPLERATE ?= 44100

# Audio buffer size in frames
BUFFSIZE ?= 4096

# Default sample rate converter type
DEFAULT_CONVERTER ?= SRC_SINC_MEDIUM_QUALITY

ifeq ($(SOUND), ao)
  CURSES_LDFLAGS = -lao -ldl -lpthread -lm \
	-lsndfile -lvorbisfile -lmodplug -lsamplerate
endif

##########################################################################
# The configuration options below are intended mainly for older flavors
# of Unix.  For Linux, BSD, and Solaris released since 2003, you can
# ignore this section.
##########################################################################

# If your machine's version of curses doesn't support color...
#
COLOR ?= yes

# If this matters, you can choose -lcurses or -lncurses
CURSES ?= -lncurses

# Uncomment this if you're compiling Unix Frotz on a machine that lacks
# the strrchr() libc library call.  If you don't know what this means,
# leave it alone.
#
#STRRCHR_DEF = -DNO_STRRCHR

# Uncomment this if you're compiling Unix Frotz on a machine that lacks
# the memmove(3) system call.  If you don't know what this means, leave it
# alone.
#
#NO_MEMMOVE = yes

#########################################################################
# This section is where Frotz is actually built.
# Under normal circumstances, nothing in this section should be changed.
#########################################################################


# Source locations

SRCDIR = src

COMMON_DIR = $(SRCDIR)/common
COMMON_LIB = $(COMMON_DIR)/frotz_common.a
COMMON_STRINGS = $(COMMON_DIR)/version.c
COMMON_DEFINES = $(COMMON_DIR)/defs.h
HASH = $(COMMON_DIR)/git_hash.h

CURSES_DIR = $(SRCDIR)/curses
CURSES_LIB = $(CURSES_DIR)/frotz_curses.a
CURSES_DEFINES = $(CURSES_DIR)/ux_defines.h

DUMB_DIR = $(SRCDIR)/dumb
DUMB_LIB = $(DUMB_DIR)/frotz_dumb.a

BLORB_DIR = $(SRCDIR)/blorb
BLORB_LIB = $(BLORB_DIR)/blorblib.a

SDL_DIR = $(SRCDIR)/sdl
SDL_LIB = $(SDL_DIR)/frotz_sdl.a
export SDL_PKGS = libpng libjpeg sdl2 SDL2_mixer freetype2 zlib
SDL_LDFLAGS = `pkg-config $(SDL_PKGS) --libs` -lm


SUBDIRS = $(COMMON_DIR) $(CURSES_DIR) $(SDL_DIR) $(DUMB_DIR) $(BLORB_DIR)
SUB_CLEAN = $(SUBDIRS:%=%-clean)


FROTZ_BIN = frotz$(EXTENSION)
DFROTZ_BIN = dfrotz$(EXTENSION)
SFROTZ_BIN = sfrotz$(EXTENSION)


all: $(FROTZ_BIN) $(DFROTZ_BIN) $(SFROTZ_BIN)

$(COMMON_LIB): $(COMMON_DEFINES) $(HASH) $(COMMON_DIR);
$(CURSES_LIB): $(CURSES_DEFINES) $(CURSES_DIR);
$(SDL_LIB): $(SDL_DIR);
$(DUMB_LIB): $(DUMB_DIR);
$(BLORB_LIB): $(BLORB_DIR);

$(SUBDIRS):
	$(MAKE) -C $@

$(SUB_CLEAN):
	-$(MAKE) -C $(@:%-clean=%) clean


# Main programs

$(FROTZ_BIN): $(COMMON_LIB) $(CURSES_LIB) $(BLORB_LIB) $(COMMON_LIB)
	$(CC) $(CFLAGS) $+ -o $@$(EXTENSION) $(CURSES) $(LDFLAGS) \
		$(CURSES_LDFLAGS)

dumb: $(DFROTZ_BIN)
$(DFROTZ_BIN): $(COMMON_LIB) $(DUMB_LIB) $(BLORB_LIB) $(COMMON_LIB)
	$(CC) $(CFLAGS) $+ -o $@$(EXTENSION)

sdl: $(SFROTZ_BIN)
$(SFROTZ_BIN): $(COMMON_LIB) $(SDL_LIB) $(BLORB_LIB) $(COMMON_LIB)
	$(CC) $(CFLAGS) $+ -o $@$(EXTENSION) $(LDFLAGS) $(SDL_LDFLAGS)


# Libs

%.a:
	$(AR) rc $@ $^
	$(RANLIB) $@

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -fpic -o $@ -c $<

common_lib:	$(COMMON_LIB)

curses_lib:	$(CURSES_LIB)

dumb_lib:	$(DUMB_LIB)

blorb_lib:	$(BLORB_LIB)


# Compile-time generated defines and strings

common_strings:	$(COMMON_STRINGS)
$(COMMON_STRINGS):
	@echo "Generating $@"
	@echo "#include \"frotz.h\"" > $@
	@echo "const char frotz_version[] = \"$(VERSION)\";" >> $@
	@echo "const char frotz_v_major[] = \"$(MAJOR)\";" >> $@
	@echo "const char frotz_v_minor[] = \"$(MINOR)\";" >> $@
	@echo "const char frotz_v_build[] = \"$(BUILD_DATE_TIME)\";" >> $@

common_defines: $(COMMON_DEFINES)
$(COMMON_DEFINES):
	@echo "Generating $@"
	@echo "#ifndef COMMON_DEFINES_H" > $@
	@echo "#define COMMON_DEFINES_H" >> $@
ifdef NO_MEMMOVE
	@echo "#define NO_MEMMOVE" >> $@
endif
	@echo "#endif /* COMMON_DEFINES_H */" >> $@

curses_defines: $(CURSES_DEFINES)
$(CURSES_DEFINES):
	@echo "Generating $@"
	@echo "#ifndef CURSES_DEFINES_H" > $@
	@echo "#define CURSES_DEFINES_H" >> $@
	@echo "#define CONFIG_DIR \"$(SYSCONFDIR)\"" >> $@
	@echo "#define SOUND \"$(SOUND)\"" >> $@
	@echo "#define SAMPLERATE $(SAMPLERATE)" >> $@
	@echo "#define BUFFSIZE $(BUFFSIZE)" >> $@
	@echo "#define DEFAULT_CONVERTER $(DEFAULT_CONVERTER)" >> $@

ifeq ($(SOUND), none)
	@echo "#define NO_SOUND" >> $@
endif

ifndef SOUND
	@echo "#define NO_SOUND" >> $@
endif

ifdef COLOR
	@echo "#define COLOR_SUPPORT" >> $@
endif

	@echo "#endif /* CURSES_DEFINES_H */" >> $@


hash: $(HASH)
$(HASH):
	@echo "Creating $@"
	@echo "#define GIT_BRANCH \"$(GIT_BRANCH)\"" > $@
	@echo "#define GIT_HASH \"$(GIT_HASH)\"" >> $@
	@echo "#define GIT_HASH_SHORT \"$(GIT_HASH_SHORT)\"" >> $@
	@echo "#define GIT_TAG \"$(GIT_TAG)\"" >> $@


# Administrative stuff

install: install_frotz
install_frotz: $(FROTZ_BIN)
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(MANDIR)/man6"
	install "frotz$(EXTENSION)" "$(DESTDIR)$(PREFIX)/bin/"
	install -m 644 doc/frotz.6 "$(DESTDIR)$(MANDIR)/man6/"

install: install_frotz
uninstall_frotz:
	rm -f "$(DESTDIR)$(PREFIX)/bin/frotz"
	rm -f "$(DESTDIR)$(MANDIR)/man6/frotz.6"

install_dfrotz: $(DFROTZ_BIN)
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(MANDIR)/man6"
	install "$(DFROTZ_BIN)" "$(DESTDIR)$(PREFIX)/bin/"
	install -m 644 doc/dfrotz.6 "$(DESTDIR)$(MANDIR)/man6/"

uninstall_dfrotz:
	rm -f "$(DESTDIR)$(PREFIX)/bin/dfrotz"
	rm -f "$(DESTDIR)$(MANDIR)/man6/dfrotz.6"

install_sfrotz: $(SFROTZ_BIN)
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(MANDIR)/man6"
	install "$(DFROTZ_BIN)" "$(DESTDIR)$(PREFIX)/bin/"
	install -m 644 doc/sfrotz.6 "$(DESTDIR)$(MANDIR)/man6/"

uninstall_sfrotz:
	rm -f "$(DESTDIR)$(PREFIX)/bin/dfrotz"
	rm -f "$(DESTDIR)$(MANDIR)/man6/dfrotz.6"


dist: frotz-$(GIT_TAG).tar.gz
frotz-$(GIT_TAG).tar.gz:
	git archive --format=tar.gz -o "frotz-$(GIT_TAG).tar.gz" "$(GIT_TAG)"

clean: $(SUB_CLEAN)
	rm -f $(SRCDIR)/*.h $(SRCDIR)/*.a $(COMMON_DEFINES) \
		$(COMMON_DIR)/git_hash.h $(CURSES_DEFINES) \
		$(OBJECTS) frotz*.tar.gz

distclean: clean
	rm -f frotz$(EXTENSION) dfrotz$(EXTENSION) sfrotz$(EXTENSION)

help:
	@echo "Default target is \"all\""
	@echo ""
	@echo "Targets:"
	@echo "    frotz: the standard edition"
	@echo "    dfrotz: for dumb terminals and wrapper scripts"
	@echo "    sfrotz: for SDL graphics and sound"
	@echo "    install"
	@echo "    uninstall"
	@echo "    install_dfrotz"
	@echo "    uninstall_dfrotz"
	@echo "    install_sfrotz"
	@echo "    uninstall_sfrotz"
	@echo "    install_all"
	@echo "    uninstall_all"
	@echo "    clean: clean up files created by compilation"
	@echo "    distclean: like clean, but also delete executables"
	@echo "    dist: create a source tarball of the latest tagged release"
	@echo ""

.SUFFIXES:
.SUFFIXES: .c .o .h

.PHONY: all clean dist dumb sdl hash help \
	common_defines curses_defines \
	blorb_lib common_lib curses_lib dumb_lib \
	install install_dfrotz install_sfrotz \
	$(SUBDIRS) $(SUB_CLEAN) \
	$(COMMON_DIR)/defines.h $(COMMON_DIR)/version.c $(CURSES_DIR)/defines.h
