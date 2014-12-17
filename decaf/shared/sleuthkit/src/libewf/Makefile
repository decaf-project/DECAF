# Use additional debug code makes hexdumps of
# unknown parts of EWF file format
# DEFS	= -D_LIBEWF_DEBUG_

CC	= gcc
#CC = g++
CFLAGS	= -O4 -Wall -g -fPIC
AR	= ar
ARFLAGS	= -q
RANLIB	= ranlib
LIBS	= -L/usr/local/ssl/lib -lz -lcrypto

LIBEWF_OBJS  = endian.o ewf_compress.o ewf_crc.o ewf_data.o ewf_error2.o ewf_file_header.o ewf_hash.o ewf_header.o ewf_header2.o ewf_md5hash.o ewf_section.o ewf_sectors.o ewf_table.o ewf_volume.o handle.o header_values.o file.o file_read.o notify.o offset_table.o section_list.o segment_table.o
LIBEWF       = libewf.a
LIBEWF_EXECS = ewfinfo ewfexport ewfverify

all : $(LIBEWF) $(LIBEWF_EXECS)

remake: clean all

clean :
	rm -f *.o
	rm -f *.a
	rm -f $(LIBEWF_EXECS)
	rm -f $(LIBEWF_EXECS:=.exe)

$(LIBEWF) : $(LIBEWF_OBJS)
	$(AR) $(ARFLAGS) $@ $?
	$(RANLIB) $@

.c.o :
	$(CC) $(CFLAGS) $(DEFS) -c $<

$(LIBEWF_EXECS) :
	$(CC) $(CFLAGS) $(DEFS) -o $@ $@.c libewf.a $(LIBS)

