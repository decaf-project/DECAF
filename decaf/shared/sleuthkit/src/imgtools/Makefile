SHELL	= /bin/sh
CC	= gcc
CC2	= g++
# Uncomment this for a static build
# OPT	= -O -Wall  -static
OPT	= -O -Wall -fPIC 
DEBUG	= #-g
INCL	= -I../auxtools -I../afflib/lib -I../libewf/ 
CFLAGS	= $(DEFS) $(INCL) $(OPT) $(DEBUG)
LIBOBJ	= img_open.o img_types.o raw.o split.o aff.o ewf.o
LIBS    = -L../../lib -L/usr/local/ssl/lib -limgtools -lauxtools ../afflib/lib/libafflib.a ../libewf/libewf.a -lz -lcrypto
LIB	= ../../lib/libimgtools.a
BIN_DIR	= ../../bin
PROGS	= $(BIN_DIR)/img_stat $(BIN_DIR)/img_cat

defs:
	@CC="$(CC)" sh ../makedefs

all:	$(LIB) $(PROGS)

$(LIB):	$(LIBOBJ)
	$(AR) $@ $?
	$(RANLIB) $@

$(BIN_DIR)/img_stat: img_stat.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ img_stat.o $(LIBS) 

$(BIN_DIR)/img_cat: img_cat.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ img_cat.o $(LIBS) 

clean:
	rm -f $(PROGS) $(LIB) *.o *core *~

indent:
	indent *.c *.h

depend: $(MAKES)
	(sed '1,/^# do not edit/!d' Makefile; \
	set -e; for i in [a-z][a-z0-9]*.c; do \
	    $(CC) -E $(DEFS) $(INCL) $$i | sed -n -e '/^# *1 *"\([^"]*\)".*/{' \
	    -e 's//'`echo $$i|sed 's/c$$/o/'`': \1/' -e 'p' -e '}'; \
	done) | grep -v '[.][o][:][ ][/]' >$$$$ && mv $$$$ Makefile

# do not edit - this was generated with make depend
img_open.o: img_open.c
img_open.o: img_tools.h
img_open.o: ../auxtools/tsk_os.h
img_open.o: ../auxtools/tsk_types.h
img_types.o: img_types.c
img_types.o: img_tools.h
img_types.o: ../auxtools/tsk_os.h
img_types.o: ../auxtools/tsk_types.h
raw.o: raw.c
raw.o: img_tools.h
raw.o: ../auxtools/tsk_os.h
raw.o: ../auxtools/tsk_types.h
split.o: split.c
split.o: img_tools.h
split.o: ../auxtools/tsk_os.h
split.o: ../auxtools/tsk_types.h
aff.o: aff.c
aff.o: img_tools.h
aff.o: ../auxtools/tsk_os.h
aff.o: ../auxtools/tsk_types.h
ewf.o: ewf.c
ewf.o: img_tools.h
ewf.o: ../auxtools/tsk_os.h
ewf.o: ../auxtools/tsk_types.h
img_stat.o: img_stat.c
img_stat.o: img_tools.h
img_stat.o: ../auxtools/tsk_os.h
img_stat.o: ../auxtools/tsk_types.h
img_cat.o: img_cat.c
img_cat.o: img_tools.h
