# Tracecap is owned and copyright (C) BitBlaze, 2007-2009.
# All rights reserved.
# Do not copy, disclose, or distribute without explicit written
# permission.

include config-plugin.mak
include $(SRC_PATH)/$(TARGET_DIR)/config-target.mak
include $(SRC_PATH)/config-host.mak

#SLEUTHKIT_PATH=$(SRC_PATH)/shared/sleuthkit/
LLCONF_PATH=$(SRC_PATH)/shared/llconf/

DEFINES=-I. -I$(SRC_PATH) -I$(SRC_PATH)/plugins -I$(SRC_PATH)/fpu -I$(SRC_PATH)/shared -I$(SRC_PATH)/target-$(TARGET_ARCH) -I$(SRC_PATH)/$(TARGET_DIR) -I$(SRC_PATH)/slirp -I$(SRC_PATH)/shared/hooks 
DEFINES+=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -DNEED_CPU_H
DEFINES+=-I$(LLCONF_PATH)/install/include
DEFINES+=-I$(GLIB_CFLAGS)

CC=gcc
CPP=g++
CFLAGS=-Wall -O2 -g -fPIC -MMD 
# CFLAGS=-Wall -g -fPIC 
LDFLAGS=-g -shared 
#LIBS=-lcrypto
LIBS+=$(LLCONF_PATH)/src/.libs/libllconf.a

ifeq ($(ARCH), x86_64)
LIBS+=-L$(SRC_PATH)/shared/xed2/xed2-intel64/lib -lxed
DEFINES+= -I$(SRC_PATH)/shared/xed2/xed2-intel64/include
endif
ifeq ($(ARCH), i386)
LIBS+=-L$(SRC_PATH)/shared/xed2/xed2-ia32/lib -lxed
DEFINES+= -I$(SRC_PATH)/shared/xed2/xed2-ia32/include
endif

OBJS=unpacker.o mem_mark.o
# temu, qemu-tools removed as target
all: unpacker.so

SHARED_LIBS=$(LLCONF_PATH)/src/.libs/libllconf.a

#deps:

#$(LLCONF_PATH)/src/.libs/libllconf.a:
#	./build-deps.sh $(SRC_PATH)

qemu-tools:
	make -C $(SRC_PATH) all

%.o: %.c 
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

%.o: %.cpp
	$(CPP) $(CFLAGS) $(DEFINES) -c -o $@ $<

unpacker.so: $(SHARED_LIBS) $(OBJS)
	$(CPP) $(LDFLAGS) $^ -o $@ $(LIBS)

unpacker-static.so: $(OBJS)
	$(CPP) -static-libgcc -Wl,-static $(LDFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f *.o *.d *.so *.a *~ $(PLUGIN) 

realclean:
	rm -f *.o  *.d *.so *.a *~ $(PLUGIN)

# Include automatically generated dependency files
-include $(wildcard *.d)

