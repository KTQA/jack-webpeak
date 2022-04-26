# override e.g. `make install PREFIX=/usr`
PREFIX ?= /usr/local

CFLAGS=-Wall `pkg-config --cflags jack` `pkg-config --cflags wsserver` -O3
LIBS=`pkg-config --libs jack` `pkg-config --libs wsserver` -lpthread -lm 
#compat w/ NetBSD and GNU Make
LDADD=${LIBS}
LDLIBS=${LIBS}

all: jack-webpeak

jack-webpeak: jack-webpeak.c

jack-webpeak.1: jack-webpeak
	help2man -N -o jack-webpeak.1 -n "live peak-signal meter for JACK" ./jack-webpeak

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 jack-webpeak $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 644 jack-webpeak.1 $(DESTDIR)$(PREFIX)/share/man/man1/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/jack-webpeak
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/jack-webpeak.1
	-rmdir $(DESTDIR)$(PREFIX)/bin
	-rmdir $(DESTDIR)$(PREFIX)/share/man/man1

clean:
	/bin/rm -f jack-webpeak

maintainerclean: clean
	/bin/rm -f jack-webpeak.1

.PHONY: all install uninstall clean
