SHELL	= /bin/sh
CC	= g++
CC2	= g++
# Uncomment this for a static build
# OPT	= -O -Wall  -static
OPT	= -O2 -Wall 
DEBUG	= -g
INCL	= -I../auxtools -I../imgtools
CFLAGS	= $(DEFS) $(INCL) $(OPT) $(DEBUG)
LIBOBJ	= mm_part.o mm_open.o mm_io.o mm_types.o dos.o mac.o bsd.o sun.o gpt.o
LIBS	= -L../../lib -L/usr/local/ssl/lib -lmmtools -lauxtools -limgtools ../afflib/lib/libafflib.a ../libewf/libewf.a -lz -lcrypto
LIB	= ../../lib/libmmtools.a
BIN_DIR	= ../../bin
PROGS	= $(BIN_DIR)/mmls $(BIN_DIR)/mmstat

defs:
	@CC="$(CC)" sh ../makedefs

all:	$(LIB) $(PROGS)

$(LIB):	$(LIBOBJ)
	$(AR) $@ $?
	$(RANLIB) $@

$(BIN_DIR)/mmls: mmls.o $(LIB) 
	$(CC2) $(CFLAGS) -o $@ mmls.o $(LIBS) 

$(BIN_DIR)/mmstat: mmstat.o $(LIB) 
	$(CC2) $(CFLAGS) -o $@ mmstat.o $(LIBS) 

indent:
	indent *.c *.h

clean:
	rm -f $(PROGS) $(LIB) *.o *core *~

depend: $(MAKES)
	(sed '1,/^# do not edit/!d' Makefile; \
	set -e; for i in [a-z][a-z0-9]*.c; do \
	    $(CC) -E $(DEFS) $(INCL) $$i | sed -n -e '/^# *1 *"\([^"]*\)".*/{' \
	    -e 's//'`echo $$i|sed 's/c$$/o/'`': \1/' -e 'p' -e '}'; \
	done) | grep -v '[.][o][:][ ][/]' >$$$$ && mv $$$$ Makefile

# do not edit - this was generated with make depend
dos.o: dos.c
dos.o: dos.c
dos.o: mm_tools.h
dos.o: mm_types.h
dos.o: ../auxtools/mymalloc.h
dos.o: dos.h
mac.o: mac.c
mac.o: mm_types.h
mac.o: ../auxtools/mymalloc.h
mac.o: mac.h
sun.o: sun.c
sun.o: mm_types.h
sun.o: ../auxtools/mymalloc.h
sun.o: sun.h
bsd.o: bsd.c
bsd.o: mm_types.h
bsd.o: ../auxtools/mymalloc.h
bsd.o: bsd.h
gpt.o: gpt.c
gpt.o: mm_types.h
gpt.o: ../auxtools/mymalloc.h
gpt.o: gpt.h
gpt.o: dos.h
mmls.o: mmls.c
mmls.o: mm_tools.h
mmstat.o: mmstat.c
mmstat.o: mm_tools.h
mm_open.o: mm_open.c
mm_open.o: mm_tools.h
mm_part.o: mm_part.c
mm_part.o: mm_tools.h
mm_part.o: ../auxtools/mymalloc.h
mm_types.o: mm_types.c
mm_types.o: mm_tools.h
mm_io.o: mm_io.c
mm_io.o: mm_tools.h
