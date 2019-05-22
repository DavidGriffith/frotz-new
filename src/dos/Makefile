# For GNU Make.

SOURCES = bcblorb.c bcinit.c bcinput.c bcmouse.c bcpic.c bcsample.c bcscreen bctext.c

HEADERS = bcfrotz.h blorb.h blorblow.h fontdata.h

OBJECTS = $(SOURCES:.c=.o)

.PHONY: clean

$(TARGET): $(OBJECTS)
	@echo "** Done with DOS interface."

clean:
	rm -f $(TARGET) $(OBJECTS)
	rm -f *.O

%.o: %.c
	$(WCC) $(CFLAGS) $(DOS_CFLAGS) -o $@ -c $<
