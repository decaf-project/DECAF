# Ensure /usr/lib/libbfd.so and /usr/lib/libopcodes.so points to multiarch
CC=gcc
CPP=g++
STRIP=strip
CFLAGS=-g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -I. -lbfd -lopcodes -lboost_program_options
#CFLAGS += `pkg-config --cflags --libs gtkmm-2.4`
DEPS =  
CPPFiles = $(wildcard *.cpp)
CFiles = $(wildcard *.c)
OBJ = $(patsubst %.cpp, %.o, $(CPPFiles)) 
OBJ += $(patsubst %.c, %.o, $(CFiles))

all: hostfile trace_reader_cpp

trace_reader_cpp: $(OBJ)
	$(CPP) -o $@ $^ $(CFLAGS)

hostfile: host.s
	$(CC) -nostdlib -nostdinc host.s -o hostfile
	$(STRIP) hostfile
	./setupVerifyDefs

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CFLAGS)


clean:
	rm trace_reader_cpp *.o hostfile TraceProcessorX86VerifyDefs.h
