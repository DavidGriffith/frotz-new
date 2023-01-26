# For GNU Make.

ZFONTSBDF = Zork_r400-10.bdf Zork_r400-11.bdf Zork_r400-13.bdf Zork_r400-16.bdf Zork_r400-20.bdf
ZFONTSPCF = $(ZFONTSBDF:.bdf=.pcf)

X11_FONTDIR = $(DESTDIR)$(PREFIX)/share/fonts/X11/zork

MKFONTDIR ?= $(shell which mkfontdir)
XSET ?= $(shell which xset)



.SUFFIXES: .bdf .pcf
.DELETE_ON_ERROR:

help:
	@echo "Font Targets:"
	@echo "         pcf: Build PCF fonts from BDF fonts"

pcf:	$(ZFONTSPCF)
	@echo "CFLAGS is $(CFLAGS)"
	@echo "DESTDIR is $(DESTDIR)"
	@echo "X11_FONTDIR is $(X11_FONTDIR)"

install_pcf:	pcf
	@echo "Installing X11 fonts to $(X11_FONTDIR)"
	mkdir -p $(X11_FONTDIR)
	install -m 644 -t $(X11_FONTDIR) $(ZFONTSPCF)
	$(MKFONTDIR) $(X11_FONTDIR)
	$(XSET) +fp $(X11_FONTDIR)

uninstall_pcf:
	rm -f $(X11_FONTDIR)/*pcf
	rm -f $(X11_FONTDIR)/fonts.dir
	find $(X11_FONTDIR) -depth -type d -empty -exec rmdir "{}" \;
	$(XSET) -fp $(X11_FONTDIR)

clean:
	rm -f $(TARGET) $(OBJECTS) $(ZFONTSPCF)

.bdf.pcf:
	bdftopcf -t $< -o $@


