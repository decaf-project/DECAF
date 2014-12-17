SHELL	= /bin/sh
CC	= gcc
CC2	= g++
# uncomment the following for a static build
# OPT	= -O -Wall -static
OPT	= -O4 -Wall -fPIC
DEBUG	= #-g
INCL	= -I../auxtools -I../imgtools 
CFLAGS	= $(DEFS) $(INCL) $(OPT) $(DEBUG)
LIBOBJ	= fs_inode.o fs_io.o fs_open.o \
	fs_dent.o fs_types.o fs_data.o fs_load.o fs_unicode.o \
	ffs.o ffs_dent.o ext2fs.o ext2fs_dent.o ext2fs_journal.o \
	fatfs.o fatfs_dent.o ntfs.o ntfs_dent.o swapfs.o rawfs.o \
	dcalc_lib.o dcat_lib.o dls_lib.o dstat_lib.o ffind_lib.o \
	fls_lib.o icat_lib.o ifind_lib.o ils_lib.o \
iso9660.o iso9660_dent.o 
# hfs.o hfs_dent.o hfs_journal.o
LIBS	= -L../../lib -L/usr/local/ssl/lib -lfstools -lauxtools -limgtools ../afflib/lib/libafflib.a ../libewf/libewf.a -lz -lcrypto

LIB	= ../../lib/libfstools.a
BIN_DIR	= ../../bin
PROGS	= $(BIN_DIR)/ils $(BIN_DIR)/dls $(BIN_DIR)/icat \
	$(BIN_DIR)/fls $(BIN_DIR)/dcat $(BIN_DIR)/ffind \
        $(BIN_DIR)/istat $(BIN_DIR)/ifind $(BIN_DIR)/dcalc $(BIN_DIR)/dstat \
		$(BIN_DIR)/fsstat $(BIN_DIR)/jls $(BIN_DIR)/jcat 
# $(BIN_DIR)/fscheck

defs:
	@CC="$(CC)" sh ../makedefs

all:	$(LIB) $(PROGS)

$(LIB):	$(LIBOBJ)
	$(AR) $@ $?
	$(RANLIB) $@

$(BIN_DIR)/dls: dls.o $(LIB) 
	$(CC2) $(CFLAGS) -o $@ dls.o $(LIBS) 

$(BIN_DIR)/ils: ils.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ ils.o $(LIBS) 

$(BIN_DIR)/icat: icat.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ icat.o $(LIBS) 

$(BIN_DIR)/ifind: ifind.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ ifind.o $(LIBS) 

$(BIN_DIR)/ffind: ffind.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ ffind.o $(LIBS) 

$(BIN_DIR)/fls: fls.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ fls.o $(LIBS) 

$(BIN_DIR)/dcat: dcat.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ dcat.o $(LIBS) 

$(BIN_DIR)/istat: istat.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ istat.o $(LIBS) 

$(BIN_DIR)/dcalc: dcalc.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ dcalc.o $(LIBS) 

$(BIN_DIR)/dstat: dstat.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ dstat.o $(LIBS) 

$(BIN_DIR)/fsstat: fsstat.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ fsstat.o $(LIBS) 

$(BIN_DIR)/fscheck: fscheck.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ fscheck.o $(LIBS) 

$(BIN_DIR)/jls: jls.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ jls.o $(LIBS) 

$(BIN_DIR)/jcat: jcat.o $(LIB)
	$(CC2) $(CFLAGS) -o $@ jcat.o $(LIBS) 

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
dcalc.o: fs_tools.h
dcalc.o: fs_types.h
dcalc_lib.o: fs_tools.h
dcalc_lib.o: fs_types.h
dcat.o: fs_tools.h
dcat.o: fs_types.h
dcat_lib.o: fs_tools.h
dcat_lib.o: fs_types.h
dls.o: fs_tools.h
dls.o: fs_types.h
dls_lib.o: fs_tools.h
dls_lib.o: fs_types.h
dstat.o: fs_tools.h
dstat.o: fs_types.h
dstat_lib.o: fs_tools.h
dstat_lib.o: fs_types.h
dstat_lib.o: fatfs.h
dstat_lib.o: ntfs.h
dstat_lib.o: ffs.h
dstat_lib.o: ext2fs.h
ext2fs.o: fs_tools.h
ext2fs.o: fs_types.h
ext2fs.o: fs_types.h
ext2fs.o: ext2fs.h
ext2fs_dent.o: fs_tools.h
ext2fs_dent.o: fs_types.h
ext2fs_dent.o: ext2fs.h
ext2fs_journal.o: fs_tools.h
ext2fs_journal.o: fs_types.h
ext2fs_journal.o: ext2fs.h
fatfs_dent.o: fs_tools.h
fatfs_dent.o: fs_types.h
fatfs_dent.o: fatfs.h
ffind_lib.o: fs_tools.h
ffind_lib.o: fs_types.h
ffs.o: fs_tools.h
ffs.o: fs_types.h
ffs.o: ffs.h
ffs_dent.o: fs_tools.h
ffs_dent.o: fs_types.h
ffs_dent.o: ffs.h
fls.o: fs_tools.h
fls.o: fs_types.h
fls_lib.o: fs_tools.h
fls_lib.o: fs_types.h
fls_lib.o: ntfs.h
fs_data.o: fs_tools.h
fs_data.o: fs_types.h
fs_dent.o: fs_tools.h
fs_dent.o: fs_types.h
fs_inode.o: fs_tools.h
fs_inode.o: fs_types.h
fs_inode.o: fs_data.h
fs_io.o: fs_tools.h
fs_io.o: fs_types.h
fs_io.o: fs_types.h
fs_unicode.o: fs_tools.h
fs_unicode.o: fs_unicode.h
fs_open.o: fs_tools.h
fs_open.o: fs_types.h
fsstat.o: fs_tools.h
fsstat.o: fs_types.h
fscheck.o: fs_tools.h
fscheck.o: fs_types.h
fs_load.o: fs_tools.h
fs_load.o: fs_load.c
fs_types.o: fs_tools.h
fs_types.o: fs_types.h
hfs.o: fs_tools.h
hfs.o: fs_types.h
hfs.o: fs_types.h
hfs.o: hfs.h
hfs_dent.o: fs_tools.h
hfs_dent.o: fs_types.h
hfs_dent.o: hfs.h
hfs_journal.o: fs_tools.h
hfs_journal.o: fs_types.h
hfs_journal.o: hfs.h
icat.o: fs_tools.h
icat.o: fs_types.h
icat_lib.o: fs_tools.h
icat_lib.o: fs_types.h
ifind.o: fs_tools.h
ifind.o: fs_types.h
ifind_lib.o: fs_tools.h
ifind_lib.o: fs_types.h
ils.o: fs_tools.h
ils.o: fs_types.h
ils_lib.o: fs_tools.h
ils_lib.o: fs_types.h
iso9660.o: fs_tools.h
iso9660.o: fs_types.h
iso9660.o: fs_types.h
iso9660.o: iso9660.h
iso9660_dent.o: fs_tools.h
iso9660_dent.o: fs_types.h
iso9660_dent.o: iso9660.h
istat.o: fs_tools.h
istat.o: fs_types.h
istat.o: ffs.h
istat.o: ext2fs.h
jls.o: fs_tools.h
jls.o: fs_types.h
jcat.o: fs_tools.h
jcat.o: fs_types.h
ntfs.o: fs_tools.h
ntfs.o: fs_types.h
ntfs.o: fs_types.h
ntfs.o: ntfs.h
ntfs.o: fs_data.h
ntfs_dent.o: fs_tools.h
ntfs_dent.o: fs_types.h
ntfs_dent.o: ntfs.h
ntfs_dent.o: fs_data.h
rawfs.o: fs_tools.h
rawfs.o: fs_types.h
rawfs.o: fs_tools.h
swapfs.o: fs_types.h
