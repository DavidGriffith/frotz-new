# Makefile for Unix Frotz
# GNU make is required

SOURCES = dblorb.c dinit.c dinput.c doutput.c dpic.c

OBJECTS = $(SOURCES:.c=.o)

TARGET = frotz_dumb.a

ARFLAGS = rc

.PHONY: clean
.DELETE_ON_ERROR:

$(TARGET): $(OBJECTS)
ifndef NO_BLORB
	$(AR) $(ARFLAGS) $@ $?
	$(RANLIB) $@
	@echo "** Done with dumb interface."
endif

clean:
	rm -f $(TARGET) $(OBJECTS)

%.o: %.c
ifndef NO_BLORB
	$(CC) $(CFLAGS) -fPIC -fpic -o $@ -c $<
endif
