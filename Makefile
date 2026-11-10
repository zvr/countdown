# countdown -- Makefile
#
# Copyright (C) 2026 Alexios Zavras
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Defaults can be overridden, e.g.:
#   make CFLAGS='-O2 -DDEFAULT_DICT=\"/usr/share/dict/words\"'

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -Wall -Wextra
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man/man1

PROG    = countdown
OBJS    = countdown.o numbers.o letters.o

.PHONY: all clean install uninstall check

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

countdown.o: countdown.c numbers.h letters.h
numbers.o: numbers.c numbers.h
letters.o: letters.c letters.h

check: $(PROG)
	sh tests/run.sh ./$(PROG) words.txt

install: $(PROG)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/
	mkdir -p $(DESTDIR)$(MANDIR)
	cp $(PROG).1 $(DESTDIR)$(MANDIR)/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROG)
	rm -f $(DESTDIR)$(MANDIR)/$(PROG).1

clean:
	rm -f $(PROG) $(OBJS)
