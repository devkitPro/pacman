
CXX=gcc
CFLAGS=-g -Wall -pedantic -D_GNU_SOURCE -I. -I../..
AR=ar rc
RAN=ranlib

OBJS=md5driver.o \
     md5.o \
     util.o \
     list.o \
     log.o \
     error.o \
     package.o \
     group.o \
     db.o \
     cache.o \
     conflict.o \
     deps.o \
     provide.o \
     rpmvercmp.o \
     backup.o \
     trans.o \
     add.o \
     remove.o \
     sync.o \
     handle.o \
     alpm.o

all: libalpm.a

%.o: %.c %.h
	$(CXX) -c $(CFLAGS) -o $@ $<

libalpm.a: $(OBJS) alpm.h
	$(AR) $@ $(OBJS)
	$(RAN) $@

clean:
	rm -f *.o *~ core
	rm -f libalpm.a
