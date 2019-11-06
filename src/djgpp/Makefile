# For GNU Make.

SOURCES = djinit.c djinput.c djmouse.c djpic.c djsample.c djscreen.c djtext.c sbdrv.c

TARGET = frotz_djgpp.a

HEADERS = djfrotz.h fontdata.h sbdrv.h

ARFLAGS = rc

OBJECTS = $(SOURCES:.c=.o)

.PHONY: clean

$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $?
	$(RANLIB) $@
	@echo "** Done with DJGPP interface."

clean:
	rm -f $(TARGET) $(OBJECTS)
	rm -f *.O

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -I../common/ -o $@ -c $<
