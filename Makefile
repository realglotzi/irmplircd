SBIN_IRMPLIRCD = irmplircd
SBIN_IRMPEXEC  = irmpexec
MAN8 = irmplircd.8

CC ?= gcc
CFLAGS ?= -Wall -g -O2 -pipe #-DDEBUG
INCLUDES ?= -Ic_hashmap
PREFIX ?= /usr/local
INSTALL ?= install
STRIP ?= strip
BINDIR  ?= $(DESTDIR)$(PREFIX)/bin
SHAREDIR ?= $(DESTDIR)$(PREFIX)/share
MANDIR ?= $(SHAREDIR)/man

all: $(SBIN_IRMPLIRCD) $(SBIN_IRMPEXEC)

irmplircd.o: irmplircd.c debug.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

irmpexec.o: irmpexec.c debug.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

mapping.o: mapping.c mapping.h debug.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

hashmap.o: c_hashmap/hashmap.c c_hashmap/hashmap.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

irmplircd: irmplircd.o mapping.o c_hashmap/hashmap.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ irmplircd.o mapping.o c_hashmap/hashmap.o

irmpexec: irmpexec.o mapping.o c_hashmap/hashmap.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ irmpexec.o mapping.o c_hashmap/hashmap.o

install: install-sbin install-man

install-sbin: $(SBIN_IRMPLIRCD) $(SBIN_IRMPEXEC)
	mkdir -p $(BINDIR)
	$(STRIP) $(SBIN_IRMPLIRCD)
	$(STRIP) $(SBIN_IRMPEXEC)
	$(INSTALL) $(SBIN_IRMPLIRCD) $(BINDIR)/
	$(INSTALL) $(SBIN_IRMPEXEC) $(BINDIR)/

install-man: $(MAN1) $(MAN5) $(MAN8)
	mkdir -p $(MANDIR)/man8/
	$(INSTALL) -m 644 $(MAN8) $(MANDIR)/man8/

clean:
	rm -f $(SBIN_IRMPLIRCD) $(SBIN_IRMPEXEC) *.o c_hashmap/hashmap.o
