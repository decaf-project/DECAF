SHELL	= /bin/sh
CC	=g++
# uncomment the following for a static build
# OPT	= -O
OPT	= -O2 -Wall -fPIC
DEBUG	= #-g
CFLAGS	= $(DEFS) $(OPT) $(DEBUG)
LIBOBJ	= mymalloc.o strerror.o split_at.o tsk_endian.o \
    unicode.o data_buf.o tsk_version.o tsk_error.o tsk_parse.o
LIB	= ../../lib/libauxtools.a
BIN_DIR	= 
PROGS	= 

defs:
	@CC="$(CC)" sh ../makedefs

all:	$(LIB)

manpages: 

$(LIB):	$(LIBOBJ)
	$(AR) $@ $?
	$(RANLIB) $@

indent:
	indent *.h *.c

clean:
	rm -f $(PROGS) $(LIB) *.o *core *~

depend: $(MAKES)
	(sed '1,/^# do not edit/!d' Makefile; \
	set -e; for i in [a-z][a-z0-9]*.c; do \
	    $(CC) -E $(DEFS) $(INCL) $$i | sed -n -e '/^# *1 *"\([^"]*\)".*/{' \
	    -e 's//'`echo $$i|sed 's/c$$/o/'`': \1/' -e 'p' -e '}'; \
	done) | grep -v '[.][o][:][ ][/]' >$$$$ && mv $$$$ Makefile

# do not edit - this was generated with make depend
data_buf.o: data_buf.c
data_buf.o: data_buf.c
data_buf.o: data_buf.h
tsk_endian.o: tsk_endian.c
tsk_endian.o: tsk_endian.c
tsk_endian.o: tsk_os.h
tsk_endian.o: tsk_types.h
tsk_endian.o: tsk_endian.h
mymalloc.o: mymalloc.c
mymalloc.o: mymalloc.c
mymalloc.o: mymalloc.h
split_at.o: split_at.c
split_at.o: split_at.c
split_at.o: split_at.h
strerror.o: strerror.c
strerror.o: strerror.c
unicode.o: unicode.c
tsk_version.o:tsk_version.c
tsk_error.o:tsk_error.c
tsk_error.o:tsk_error.h
tsk_parse.o:tsk_parse.c
