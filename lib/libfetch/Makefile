prefix = /usr
DESTDIR =
DEBUG = true
ENABLE_HTTPS = true

CFLAGS   = -O2 -pipe -I. -DINET6
WARNINGS = -Wall -Wstrict-prototypes -Wsign-compare -Wchar-subscripts \
           -Wpointer-arith -Wcast-align -Wsign-compare
CFLAGS  += $(WARNINGS)

ifeq ($(strip $(DEBUG)), true)
CFLAGS += -g
else
CFLAGS += -DNDEBUG
endif

ifeq ($(strip $(ENALE_HTTPS)),true)
CFLAGS  += -DWITH_SSL
LDFLAGS += -lssl -lcrypto
endif

CC = gcc
LD = gcc
AR = ar
RANLIB = ranlib
INSTALL = install -c -D

OBJS= fetch.o common.o ftp.o http.o file.o
INCS= fetch.h common.h
GEN = ftperr.h httperr.h
MAN = fetch.3

#pretty print!
E = @echo
Q = @

all: libfetch.so libfetch.a
	$(E) "  built with:  " $(CFLAGS)
.PHONY: all

%.o: %.c
	$(E) "  compile   " $@
	$(Q) $(CC) $(CFLAGS) -c $<

ftperr.h: ftp.errors
	$(E) "  generate  " $@
	@echo "static struct fetcherr _ftp_errlist[] = {" > $@
	@cat $< \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> $@
	@echo -e "    { -1, FETCH_UNKNOWN, \"Unknown FTP error\" }\n};" >> $@

httperr.h: http.errors
	$(E) "  generate  " $@
	@echo "static struct fetcherr _http_errlist[] = {" > $@
	@cat $< \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> $@
	@echo -e "    { -1, FETCH_UNKNOWN, \"Unknown HTTP error\" }\n};" >> $@

libfetch.so: $(GEN) $(INCS) $(OBJS)
	$(E) "  build     " $@
	$(Q) rm -f $@
	$(Q) $(LD) $(LDFLAGS) *.o -shared -o $@

libfetch.a: $(GEN) $(INCS) $(OBJS)
	$(E) "  build     " $@
	$(Q) rm -f $@
	$(Q) $(AR) rcs $@ *.o
	$(Q) $(RANLIB) $@

clean:
	$(E) "  clean     "
	$(Q) rm -f libfetch.so libfetch.a *.o $(GEN)
.PHONY: clean

install: all
	$(Q) $(INSTALL) -m 755 libfetch.so $(DESTDIR)$(prefix)/lib/libfetch.so
	$(Q) $(INSTALL) -m 644 libfetch.a $(DESTDIR)$(prefix)/lib/libfetch.a
	$(Q) $(INSTALL) -m 644 fetch.h $(DESTDIR)$(prefix)/include/fetch.h
	$(Q) $(INSTALL) -m 644 fetch.3 $(DESTDIR)$(prefix)/man/man3/fetch.3
.PHONY: install

uninstall:
	$(Q) rm -f $(DESTDIR)$(prefix)/lib/libfetch.so
	$(Q) rm -f $(DESTDIR)$(prefix)/lib/libfetch.a
	$(Q) rm -f $(DESTDIR)$(prefix)/include/fetch.h
	$(Q) rm -f $(DESTDIR)$(prefix)/man/man3/fetch.3
.PHONY: uninstall
