SBIN_IRMPLIRCD = irmplircd
SBIN_IRMPEXEC  = irmpexec
MAN8 = irmplircd.8

CC ?= gcc
CFLAGS ?= -Wall -g -O2 -pipe -Ic_hashmap #-DDEBUG
PREFIX ?= /usr/local
INSTALL ?= install
STRIP ?= strip
SBINDIR ?= $(PREFIX)/sbin
BINDIR  ?= $(PREFIX)/bin
SHAREDIR ?= $(PREFIX)/share
MANDIR ?= $(SHAREDIR)/man

all: $(SBIN_IRMPLIRCD) $(SBIN_IRMPEXEC)

irmplircd.o: irmplircd.c debug.h
	$(CC) $(CFLAGS) -c $<

irmpexec.o: irmpexec.c debug.h
	$(CC) $(CFLAGS) -c $<

mapping.o: mapping.c mapping.h debug.h
	$(CC) $(CFLAGS) -c $<

hashmap.o: c_hashmap/hashmap.c c_hashmap/hashmap.h
	$(CC) $(CFLAGS) -c $<

irmplircd: irmplircd.o mapping.o c_hashmap/hashmap.o
	$(CC) $(CFLAGS) -o $@ irmplircd.o mapping.o c_hashmap/hashmap.o

irmpexec: irmpexec.o mapping.o c_hashmap/hashmap.o
	$(CC) $(CFLAGS) -o $@ irmpexec.o mapping.o c_hashmap/hashmap.o

install: install-sbin install-man

install-sbin: $(SBIN_IRMPLIRCD) $(SBIN_IRMPEXEC)
	mkdir -p $(SBINDIR)
	$(STRIP) $(SBIN_IRMPLIRCD)
	$(STRIP) $(SBIN_IRMPEXEC)
	$(INSTALL) $(SBIN_IRMPLIRCD) $(SBINDIR)/
	$(INSTALL) $(SBIN_IRMPEXEC) $(BINDIR)/

install-man: $(MAN1) $(MAN5) $(MAN8)
	mkdir -p $(DESTDIR)$(MANDIR)/man8/
	$(INSTALL) -m 644 $(MAN8) $(DESTDIR)$(MANDIR)/man8/

clean:
	rm -f $(SBIN_IRMPLIRCD) $(SBIN_IRMPEXEC) *.o c_hashmap/hashmap.o
