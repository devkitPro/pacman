
CC=gcc
CFLAGS=-g -Wall -D_GNU_SOURCE -I. -I../.. -I../../lib/libalpm -I../../lib/libftp
LDFLAGS=-L../../lib/libalpm -lalpm -L../../lib/libftp -lftp -ltar -lz
AR=ar rc
RAN=ranlib

OBJECTS=util.o \
	log.o \
	list.o \
	package.o \
	db.o \
	download.o \
	trans.o \
	add.o \
	remove.o \
	upgrade.o \
	query.o \
	sync.o \
	conf.o \
	pacman.o

all: pacman

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<

pacman: $(OBJECTS) ../../lib/libalpm/libalpm.a
	$(CC) $(OBJECTS) -o $@ $(CFLAGS) $(LDFLAGS)
#	$(CC) $(OBJECTS) -o $@.static $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o *~ core
	rm -f pacman pacman.static convertdb vercmp

install: pacman vercmp convertdb
	$(INSTALL) -D -m0755 pacman $(DESTDIR)$(BINDIR)/pacman
	$(INSTALL) -D -m0755 pacman.static $(DESTDIR)$(BINDIR)/pacman.static
	$(INSTALL) -D -m0755 vercmp $(DESTDIR)$(BINDIR)/vercmp
	$(INSTALL) -D -m0755 convertdb $(DESTDIR)$(BINDIR)/convertdb

