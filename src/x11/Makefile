# For GNU Make.

SOURCES = x_init.c x_input.c x_pic.c x_sample.c x_screen.c x_text.c

HEADERS = x_frotz.h x_setup.h

OBJECTS = $(SOURCES:.c=.o)

TARGET = frotz_x11.a

ARFLAGS = rc

SOUND ?= ao

CFLAGS += -DNO_SOUND

.PHONY: clean
.DELETE_ON_ERROR:

$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $?
	$(RANLIB) $@
	@echo "** Done with X11 interface."

clean:
	rm -f $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -fpic -o $@ -c $<
