# For GNU Make.

SOURCES = owblorb.c owinit.c owinput.c owmouse.c owpic.c owsample.c owscreen.c owtext.c
HEADERS = blorb.h blorblow.h fontdata.h owblorb.h owfrotz.h owhash.h
OBJECTS = $(SOURCES:.c=.o)

%.o: %.c
	$(WCC) $(CFLAGS) $(DOS_CFLAGS) -o $@ -c $<

.PHONY:	clean

clean:
	rm -f $(TARGET) $(OBJECTS)
	rm -f *.O

