# Makefile for Unix Frotz
# GNU make is required.

# Your C compiler
#CC ?= gcc
#CC ?= clang

# Enable compiler warnings. This is an absolute minimum.
CFLAGS += -Wall -std=c99 -O3 #-Wextra

# Define your optimization flags.
#
# These are good for regular use.
#CFLAGS += -O2 -fomit-frame-pointer -falign-functions=2 -falign-loops=2 -falign-jumps=2
# These are handy for debugging.
CFLAGS += -g

# Define where you want Frotz installed
PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man
BINDIR ?= $(PREFIX)/bin
#BINDIR ?= $(PREFIX)/games
MAN_PREFIX ?= $(PREFIX)/share
SYSCONFDIR ?= /etc

# Choose your sound support
# OPTIONS: ao, none
SOUND_TYPE ?= ao


##########################################################################
# The configuration options below are intended mainly for older flavors
# of Unix.  For Linux, BSD, and Solaris released since 2003, you can
# ignore this section.
##########################################################################

# Default sample rate for sound effects.
# All modern sound interfaces can be expected to support 44100 Hz sample
# rates.  Earlier ones, particularly ones in Sun 4c workstations support
# only up to 8000 Hz.
SAMPLERATE ?= 44100

# Audio buffer size in frames
BUFFSIZE ?= 4096

# Default sample rate converter type
DEFAULT_CONVERTER ?= SRC_SINC_MEDIUM_QUALITY

# Comment this out if you don't want UTF-8 support
USE_UTF8 ?= yes

# Comment this out if your machine's version of curses doesn't support color.
COLOR ?= yes

# Select your chosen version of curses.  Unless something old is going
# on, ncursesw should be used because that's how UTF8 is supported.
#CURSES ?= curses
#CURSES ?= ncurses
CURSES ?= ncursesw

# This Makefile uses the pkg-config utility to get information on
# installed libraries.  If your system is missing that utility and
# cannot install it for whatever reason (usually very old ones), you
# will need to uncomment and perhaps modify some of these lines.
# I don't see any way around not having pkg-config for SDL.
#CURSES_LDLAGS += -l$(CURSES) -ltinfo
#CURSES_CFLAGS += -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600

# Uncomment this to disable Blorb support for dumb and curses interfaces.
# SDL interface always has Blorb support.
#NO_BLORB = yes

# These are for enabling local version of certain functions which may be
# missing or behave differently from what's expected in modern system.
# If you're running on a system made in the past 20 years, you should be
# safe leaving these alone.  If not or you're using something modern,
# but very strange intended for very limited machines, you probably know
# what you're doing.  Therefore further commentary on what these
# functions do is probably not necessary.

# For missing memmove()
#NO_MEMMOVE = yes

# For missing strdup() and strndup()
#NO_STRDUP = yes

# For missing strrchr()
#NO_STRRCHR = yes

# Uncomment to disable format codes for dumb interface
#DISABLE_FORMATS = yes

# Assorted constants
MAX_UNDO_SLOTS = 500
MAX_FILE_NAME = 80
TEXT_BUFFER_SIZE = 512
INPUT_BUFFER_SIZE = 200
STACK_SIZE = 1024


#########################################################################
# This section is where Frotz is actually built.
# Under normal circumstances, nothing in this section should be changed.
#########################################################################

# Windows - don't build
ifeq ($(OS),Windows_NT)
$(error Compilation on Windows not supported)
endif

# Determine what system we are on.
RANLIB ?= $(shell which ranlib)
AR ?= $(shell which ar)
# For now, assume !windows == unix.
OS_TYPE ?= unix
UNAME_S := $(shell uname -s)
PKG_CONFIG ?= pkg-config

# If we have pkg-config...
ifneq ($(shell which $(PKG_CONFIG)),)
# if pkg-config has $(CURSES)
ifeq ($(shell $(PKG_CONFIG) --exists $(CURSES) && echo 0),0)
# use pkg-config $(CURSES) info
PKG_CONFIG_CURSES = yes
CURSES_LDFLAGS += $(shell $(PKG_CONFIG) $(CURSES) --libs)
CURSES_CFLAGS += $(shell $(PKG_CONFIG) $(CURSES) --cflags)
else # Otherwise, try something obvious like before.
CURSES_LDFLAGS += -l$(CURSES)
endif
endif


# OS-specific configurations.
# NetBSD
ifeq ($(UNAME_S),NetBSD)
NETBSD = yes
CFLAGS += -D_NETBSD_SOURCE -I/usr/pkg/include
LDFLAGS += -Wl,-R/usr/pkg/lib -L/usr/pkg/lib
SDL_LDFLAGS += -lexecinfo
ifeq ($(CURSES), ncursesw)
CURSES_CFLAGS += -I/usr/pkg/include/ncursesw
else
CURSES_CFLAGS += -I/usr/pkg/include/ncurses
endif
endif

# FreeBSD
ifeq ($(UNAME_S),FreeBSD)
FREEBSD = yes
CFLAGS += -I/usr/local/include -D__BSD_VISIBLE=1
LDFLAGS += -L/usr/local/lib
SDL_LDFLAGS += -lexecinfo
endif

# OpenBSD
ifeq ($(UNAME_S),OpenBSD)
OPENBSD = yes
NO_EXECINFO_H = yes
NO_UCONTEXT_H = yes
NO_IMMINTRIN_H = yes
CFLAGS += -I/usr/local/include
LDFLAGS += -L/usr/local/lib
SDL_CFLAGS += -DSDL_DISABLE_IMMINTRIN_H
SDL_LDFLAGS += -lexecinfo
endif

# Linux
ifeq ($(UNAME_S),Linux)
LINUX = yes
CFLAGS += -D_POSIX_C_SOURCE=200809L
NPROCS = $(shell grep -c ^processor /proc/cpuinfo)
endif

# MacOS
ifeq ($(UNAME_S),Darwin)
MACOS = yes
# On MacOS, curses is actually ncurses, but to get wide
# char support you need to define _XOPEN_SOURCE_EXTENDED
# Often ncursesw is not present, but has wide support anyhow.
# Reset CURSES, use CURSES_MACOS on command line to override.
CURSES_MACOS ?= ncurses
CURSES = $(CURSES_MACOS)
# get flags from pkg-config curses if available
ifndef PKG_CONFIG_CURSES
# MacOS provided ncurses, but doesn't have pkg-config .pc files installed.
# Since ncursesx.x-config (should be) available if curses is installed,
# use that instead of pkg-config to get --libs and --cflags.
# Search $PATH for ncursesx.x-config, use the first one.
CURSES_CONFIG ?= $(shell echo $$PATH | while read -d ':' p; do \
	stat -f'%N' $$p/$(CURSES)[0-9]*-config 2>/dev/null; done | head -n1)
# Reset CURSES_LDFLAGS, CURSES_CFLAGS.
CURSES_LDFLAGS = $(shell $(CURSES_CONFIG) --libs)
CURSES_CFLAGS = $(shell $(CURSES_CONFIG) --cflags)
ifeq ($(CURSES_CONFIG),)
$(error no $(CURSES)x.x-config found)
endif
endif
SDL_CFLAGS += -D_XOPEN_SOURCE
endif


# Make sure the right curses include file is included.
ifeq ($(CURSES), curses)
CURSES_DEFINE = USE_CURSES_H
else ifneq ($(findstring ncurses,$(CURSES)),)
CURSES_CFLAGS += -D_XOPEN_SOURCE_EXTENDED
CURSES_DEFINE = USE_NCURSES_H
endif

export CC
export CFLAGS
export CURSES_CFLAGS
export NPROCS
export MAKEFLAGS
export AR
export RANLIB
export PREFIX
export MANDIR
export SYSCONFDIR
export SDL_CFLAGS
export COLOR
export SOUND_TYPE
export NO_SOUND

NAME = frotz
VERSION = 2.51
RELEASE_NOTES = "Development release."

# If we're working from git, we have access to proper variables. If
# not, make it clear that we're working from a release.
#
GIT_DIR ?= .git
ifneq ($(and $(wildcard $(GIT_DIR)),$(shell which git)),)
GIT_HASH = $(shell git rev-parse HEAD)
GIT_HASH_SHORT = $(shell git rev-parse --short HEAD)
GIT_DATE = $(shell git show -s --format=%ci)
else
GIT_HASH = "$$Format:%H$$"
GIT_HASH_SHORT = "$$Format:%h$$"
GIT_DATE = "$$Format:%ci$$"
endif
export CFLAGS


# Source locations
#
SRCDIR = src
COMMON_DIR = $(SRCDIR)/common
COMMON_LIB = $(COMMON_DIR)/frotz_common.a
COMMON_DEFINES = $(COMMON_DIR)/defs.h
HASH = $(COMMON_DIR)/git_hash.h

BLORB_DIR = $(SRCDIR)/blorb
BLORB_LIB = $(BLORB_DIR)/blorblib.a

CURSES_DIR = $(SRCDIR)/curses
CURSES_LIB = $(CURSES_DIR)/frotz_curses.a
CURSES_DEFINES = $(CURSES_DIR)/ux_defines.h

DUMB_DIR = $(SRCDIR)/dumb
DUMB_LIB = $(DUMB_DIR)/frotz_dumb.a

SDL_DIR = $(SRCDIR)/sdl
SDL_LIB = $(SDL_DIR)/frotz_sdl.a
export SDL_PKGS = libpng libjpeg sdl2 SDL2_mixer freetype2 zlib
SDL_LDFLAGS += $(shell $(PKG_CONFIG) $(SDL_PKGS) --libs) -lm

DOS_DIR = $(SRCDIR)/dos

SUBDIRS = $(COMMON_DIR) $(CURSES_DIR) $(SDL_DIR) $(DUMB_DIR) $(BLORB_DIR) $(DOS_DIR)
SUB_CLEAN = $(SUBDIRS:%=%-clean)

FROTZ_BIN = frotz$(EXTENSION)
DFROTZ_BIN = dfrotz$(EXTENSION)
SFROTZ_BIN = sfrotz$(EXTENSION)
DOS_BIN = frotz.exe

FROTZ_LIBS  = $(COMMON_LIB) $(CURSES_LIB) $(BLORB_LIB) $(COMMON_LIB)
DFROTZ_LIBS = $(COMMON_LIB) $(DUMB_LIB) $(BLORB_LIB) $(COMMON_LIB)
SFROTZ_LIBS = $(COMMON_LIB) $(SDL_LIB) $(BLORB_LIB) $(COMMON_LIB)


ifdef NO_BLORB
SOUND_TYPE = none
CURSES_SOUND = disabled
BLORB_SUPPORT = disabled
FROTZ_LIBS  = $(COMMON_LIB) $(CURSES_LIB) $(COMMON_LIB)
DFROTZ_LIBS = $(COMMON_LIB) $(DUMB_LIB) $(COMMON_LIB)
else
BLORB_SUPPORT = enabled
endif

ifeq ($(SOUND_TYPE), ao)
CURSES_SOUND_LDFLAGS += -lao -lpthread -lm \
			-lsndfile -lvorbisfile -lmodplug -lsamplerate
CURSES_SOUND = enabled
else
CURSES_SOUND = disabled
endif

# Check to see if the -Orecurse option is available.  It's nice for
# watching the output of a parallel build, but is otherwise not
# necessary.
ifneq ($(filter output-sync,$(value .FEATURES)),)
MAKEFLAGS += -Orecurse
endif

# Just the version number without the dot
DOSVER = $(shell echo $(VERSION) | sed s/\\.//g)

# Build recipes
#
curses: $(FROTZ_BIN)
ncurses: $(FROTZ_BIN)
$(FROTZ_BIN): $(FROTZ_LIBS)
	$(CC) $+ -o $@$(EXTENSION) $(LDFLAGS) $(CURSES_LDFLAGS) $(CURSES_SOUND_LDFLAGS)
	@echo "** Done building Frotz with curses interface"
	@echo "** Audio support $(CURSES_SOUND) (type $(SOUND_TYPE))"
	@echo "** Blorb support $(BLORB_SUPPORT)"

nosound: nosound_helper $(FROTZ_BIN) | nosound_helper
nosound_helper:
	$(eval SOUND_TYPE= none)
	$(eval NO_SOUND= -DNO_SOUND)
	$(eval CURSES_SOUND_LDFLAGS= )
	$(eval CURSES_SOUND= disabled)

dumb: $(DFROTZ_BIN)
$(DFROTZ_BIN): $(DFROTZ_LIBS)
	$(CC) $+ -o $@$(EXTENSION) $(LDFLAGS)
	@echo "** Done building Frotz with dumb interface."
	@echo "** Blorb support $(BLORB_SUPPORT)"

sdl: $(SFROTZ_BIN)
$(SFROTZ_BIN): $(SFROTZ_LIBS)
	$(CC) $+ -o $@$(EXTENSION) $(LDFLAGS) $(SDL_LDFLAGS)
	@echo "** Done building Frotz with SDL interface."

dos: $(DOS_BIN)
$(DOS_BIN):
	@echo
	@echo "  ** Cannot cross-compile for DOS yet."
	@echo "  ** Copy this zip file, $(NAME)src.zip, into a DOS machine and use Turbo C."
	@echo "  ** A virtualized DOS machine will do.  This zip file will fit on a single"
	@echo "  ** double-sided double-density 5.25-inch floppy disk.  Read the file"
	@echo "  ** DOSBUILD.txt for more information."
	@echo
ifneq ($(and $(wildcard $(GIT_DIR)),$(shell which git)),)
	@git archive --format=zip --prefix $(NAME)src/ HEAD -o $(NAME)src.zip
	@zip -d $(NAME)src.zip $(NAME)src/src/curses/* \
		$(NAME)src/src/dumb/* $(NAME)src/src/blorb/* \
		$(NAME)src/src/sdl/* $(NAME)src/src/misc/* \
		$(NAME)src/doc/*.6 $(NAME)src/doc/frotz.conf* \
		$(NAME)src/doc/Xresources  > /dev/null
else
	@echo "Not in a git repository or git command not found.  Cannot make a tarball."
endif

all: $(FROTZ_BIN) $(DFROTZ_BIN) $(SFROTZ_BIN)

common_lib:	$(COMMON_LIB)
curses_lib:	$(CURSES_LIB)
sdl_lib:	$(SDL_LIB)
dumb_lib:	$(DUMB_LIB)
blorb_lib:	$(BLORB_LIB)
dos_lib:	$(DOS_LIB)

$(COMMON_LIB): $(COMMON_DEFINES) $(HASH)
	$(MAKE) -C $(COMMON_DIR)

$(CURSES_LIB): $(COMMON_DEFINES) $(CURSES_DEFINES) $(HASH)
	$(MAKE) -C $(CURSES_DIR)

$(SDL_LIB): $(COMMON_DEFINES) $(HASH)
	$(MAKE) -C $(SDL_DIR)

$(DUMB_LIB): $(COMMON_DEFINES) $(HASH)
	$(MAKE) -C $(DUMB_DIR)

$(BLORB_LIB): $(COMMON_DEFINES)
	$(MAKE) -C $(BLORB_DIR)

$(SUB_CLEAN):
	-$(MAKE) -C $(@:%-clean=%) clean


# Compile-time generated defines and strings
#
defs: common_defines
common_defines: $(COMMON_DEFINES)
$(COMMON_DEFINES):
ifeq ($(wildcard $(COMMON_DEFINES)),)
	@echo "** Generating $@"
	@echo "#ifndef COMMON_DEFINES_H" > $@
	@echo "#define COMMON_DEFINES_H" >> $@
ifeq ($(OS_TYPE), unix)
	@echo "#define UNIX" >> $@
endif
	@echo "#define MAX_UNDO_SLOTS $(MAX_UNDO_SLOTS)" >> $@
	@echo "#define MAX_FILE_NAME $(MAX_FILE_NAME)" >> $@
	@echo "#define TEXT_BUFFER_SIZE $(TEXT_BUFFER_SIZE)" >> $@
	@echo "#define INPUT_BUFFER_SIZE $(INPUT_BUFFER_SIZE)" >> $@
	@echo "#define STACK_SIZE $(STACK_SIZE)" >> $@
ifdef NO_BLORB
	@echo "#define NO_BLORB" >> $@
endif
ifdef NO_STRRCHR
	@echo "#define NO_STRRCHR" >> $@
endif
ifdef NO_MEMMOVE
	@echo "#define NO_MEMMOVE" >> $@
endif
ifdef NO_STRDUP
	@echo "#define NO_STRDUP" >> $@
endif
ifdef NO_UCONTEXT_H
	@echo "#define NO_UCONTEXT_H" >> $@
endif
ifdef NO_EXECINFO_H
	@echo "#define NO_EXECINFO_H" >> $@
endif
ifeq ($(USE_UTF8), yes)
	@echo "#define USE_UTF8" >> $@
endif
ifdef DISABLE_FORMATS
	@echo "#define DISABLE_FORMATS" >> $@
endif
	@echo "#endif /* COMMON_DEFINES_H */" >> $@
endif


curses_defines: $(CURSES_DEFINES)
$(CURSES_DEFINES):
ifeq ($(wildcard $(CURSES_DEFINES)),)
ifndef CURSES
	@echo "** ERROR You need to pick a flavor of curses in the Makefile!"
	exit 1
endif
ifdef USE_UTF8
ifneq ($(CURSES),ncursesw)
ifndef MACOS
	@echo "** ERROR UTF-8 support only works with ncursesw!"
	exit 2
endif
endif
endif
	@echo "** Generating $@"
	@echo "#ifndef CURSES_DEFINES_H" > $@
	@echo "#define CURSES_DEFINES_H" >> $@
	@echo "#define $(CURSES_DEFINE)" >> $@
	@echo "#define CONFIG_DIR \"$(SYSCONFDIR)\"" >> $@
	@echo "#define SOUND_TYPE \"$(SOUND_TYPE)\"" >> $@
	@echo "#define SAMPLERATE $(SAMPLERATE)" >> $@
	@echo "#define BUFFSIZE $(BUFFSIZE)" >> $@
	@echo "#define DEFAULT_CONVERTER $(DEFAULT_CONVERTER)" >> $@
ifeq ($(SOUND_TYPE), none)
	@echo "#define NO_SOUND" >> $@
endif
ifndef SOUND_TYPE
	@echo "#define NO_SOUND" >> $@
endif
ifdef COLOR
	@echo "#define COLOR_SUPPORT" >> $@
endif
ifeq ($(USE_UTF8), yes)
	@echo "#define USE_UTF8" >> $@
endif
	@echo "#endif /* CURSES_DEFINES_H */" >> $@
endif


hash: $(HASH)
$(HASH):
ifeq ($(wildcard $(HASH)),)
	@echo "** Generating $@"
	@echo "#define VERSION \"$(VERSION)\"" > $@
	@echo "#define RELEASE_NOTES \"$(RELEASE_NOTES)\"" >> $@
	@echo "#define GIT_HASH \"$(GIT_HASH)\"" >> $@
	@echo "#define GIT_HASH_SHORT \"$(GIT_HASH_SHORT)\"" >> $@
	@echo "#define GIT_DATE \"$(GIT_DATE)\"" >> $@
endif


# Administrative stuff
#
install: install_frotz
install_frotz: $(FROTZ_BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -c -m 755 $(FROTZ_BIN) $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(MAN_PREFIX)/man/man6
	install -m 644 doc/frotz.6 $(DESTDIR)$(MAN_PREFIX)/man/man6/

uninstall: uninstall_frotz
uninstall_frotz:
	rm -f $(DESTDIR)$(BINDIR)/frotz
	rm -f $(DESTDIR)$(MAN_PREFIX)/man6/frotz.6

install_dumb: install_dfrotz
install_dfrotz: $(DFROTZ_BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -c -m 755 $(DFROTZ_BIN) $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(MAN_PREFIX)/man/man6
	install -m 644 doc/dfrotz.6 $(DESTDIR)$(MAN_PREFIX)/man/man6/

uninstall_dumb: uninstall_dfrotz
uninstall_dfrotz:
	rm -f $(DESTDIR)$(BINDIR)/dfrotz
	rm -f $(DESTDIR)$(MAN_PREFIX)/man/man6/dfrotz.6

install_sdl: install_sfrotz
install_sfrotz: $(SFROTZ_BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -c -m 755 $(SFROTZ_BIN) $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(MAN_PREFIX)/man/man6
	install -m 644 doc/sfrotz.6 $(DESTDIR)$(MAN_PREFIX)/man/man6/

uninstall_sdl: uninstall_sfrotz
uninstall_sfrotz:
	rm -f $(DESTDIR)$(BINDIR)/sfrotz
	rm -f $(DESTDIR)$(MAN_PREFIX)/man/man6/sfrotz.6

install_all:	install_frotz install_dfrotz install_sfrotz

uninstall_all:	uninstall_frotz uninstall_dfrotz uninstall_sfrotz


dist: $(NAME)-$(VERSION).tar.gz
frotz-$(VERSION).tar.gz:
ifneq ($(and $(wildcard $(GIT_DIR)),$(shell which git)),)
	git archive --format=tgz --prefix $(NAME)-$(VERSION)/ HEAD -o $(NAME)-$(VERSION).tar.gz
else
	@echo "Not in a git repository or git command not found.  Cannot make a zip file."
endif

dosdist:
	@echo
	@echo "  ** Populating $(NAME)$(DOSVER) with things for the DOS Frotz precompiled zipfile."
	@echo "  ** Just add frotz.exe compiled wherever."
	@echo "  ** Then do \"zip -r $(NAME)$(DOSVER).zip $(NAME)$(DOSVER)\""
	@echo
	@mkdir $(NAME)$(DOSVER)
	@cp ChangeLog $(NAME)$(DOSVER)/changes.txt
	@cp frotz.lsm $(NAME)$(DOSVER)
	@cp doc/frotz.txt $(NAME)$(DOSVER)
	@cp doc/file_id.diz $(NAME)$(DOSVER)
	@cp HOW_TO_PLAY $(NAME)$(DOSVER)/howto.txt
	@cp README $(NAME)$(DOSVER)/README.txt
	@cp AUTHORS $(NAME)$(DOSVER)/AUTHORS.txt
	@cp COPYING $(NAME)$(DOSVER)/COPYING.txt
	@unix2dos -q $(NAME)$(DOSVER)/*
	@touch $(NAME)$(DOSVER)/*

clean: $(SUB_CLEAN)
	rm -rf $(NAME)-$(VERSION)
	rm -rf $(COMMON_DEFINES) \
		$(CURSES_DEFINES) \
		$(HASH)
	rm -f FROTZ.BAK FROTZ.EXE FROTZ.LIB FROTZ.DSK *.OBJ

distclean: clean
	rm -f frotz$(EXTENSION) dfrotz$(EXTENSION) sfrotz$(EXTENSION) a.out
	rm -rf $(NAME)src $(NAME)$(DOSVER)
	rm -f $(NAME)*.tar.gz $(NAME)src.zip $(NAME)$(DOSVER).zip

help:
	@echo "Targets:"
	@echo "    frotz: (default target) the standard curses edition"
	@echo "    nosound: the standard curses edition without sound support"
	@echo "    dumb: for dumb terminals and wrapper scripts"
	@echo "    sdl: for SDL graphics and sound"
	@echo "    all: build curses, dumb, and SDL versions"
	@echo "    dos: Make a zip file containing DOS Frotz source code"
	@echo "    install"
	@echo "    uninstall"
	@echo "    install_dumb"
	@echo "    uninstall_dumb"
	@echo "    install_sdl"
	@echo "    uninstall_sdl"
	@echo "    install_all"
	@echo "    uninstall_all"
	@echo "    clean: clean up files created by compilation"
	@echo "    distclean: like clean, but also delete executables"
	@echo "    dist: create a source tarball"
	@echo ""

.SUFFIXES:
.SUFFIXES: .c .o .h

.PHONY: all clean dist dosdist curses ncurses dumb sdl hash help \
	common_defines curses_defines nosound nosound_helper\
	$(COMMON_DEFINES) $(CURSES_DEFINES) $(HASH) \
	blorb_lib common_lib curses_lib dumb_lib \
	install install_dfrotz install_sfrotz $(SUB_CLEAN)
