# Tracecap is owned and copyright (C) BitBlaze, 2007-2009.
# All rights reserved.
# Do not copy, disclose, or distribute without explicit written
# permission.

include config-plugin.mak
include $(SRC_PATH)/$(TARGET_DIR)/config-target.mak
include $(SRC_PATH)/config-host.mak

#SLEUTHKIT_PATH=$(SRC_PATH)/shared/sleuthkit/

DEFINES=-I. -I$(SRC_PATH) -I$(SRC_PATH)/plugins -I$(SRC_PATH)/fpu -I$(SRC_PATH)/shared -I$(SRC_PATH)/target-$(TARGET_ARCH) -I$(SRC_PATH)/tcg/$(TARGET_ARCH) -I$(SRC_PATH)/$(TARGET_DIR) -I$(SRC_PATH)/slirp -I$(SRC_PATH)/shared/hooks 
DEFINES+=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -DNEED_CPU_H
DEFINES+=-Illconf
DEFINES+=-I$(GLIB_CFLAGS)
#DEFINES+=-I$(SLEUTHKIT_PATH)/src/fstools -I$(SLEUTHKIT_PATH)/src/auxtools -I$(SLEUTHKIT_PATH)/src/imgtools -DLINUX2 -DTRACE_ENABLED

CC=gcc
CPP=g++
CFLAGS=-Wall -O2 -g -fPIC -MMD 
# CFLAGS=-Wall -g -fPIC 
LDFLAGS=-g -shared 
LIBS=-lcrypto

ifeq ($(ARCH), x86_64)
LIBS+=-L$(SRC_PATH)/shared/xed2/xed2-intel64/lib -lxed
DEFINES+= -I$(SRC_PATH)/shared/xed2/xed2-intel64/include
endif
ifeq ($(ARCH), i386)
LIBS+=-L$(SRC_PATH)/shared/xed2/xed2-ia32/lib -lxed
DEFINES+= -I$(SRC_PATH)/shared/xed2/xed2-ia32/include
endif

OBJS= disasm.o commands.o trace.o operandinfo.o conditions.o network.o  tracecap.o readwrite.o conf.o trackproc.o
# llconf files
OBJS+=llconf/entry.o llconf/lines.o llconf/modules.o llconf/nodes.o llconf/parseerror.o llconf/strutils.o llconf/parsers/ini.o

# temu, qemu-tools removed as target
all: tracecap.so ini/main.ini ini/hook_plugin.ini

hooks:
	$(MAKE) -C $(SRC_PATH)/shared/hooks/hook_plugins protos_hooks

ini/main.ini: ini/main.ini.in
	@perl -pe 's[SRC_PATH][$(SRC_PATH)]g' $< >$@

ini/hook_plugin.ini: ini/hook_plugin.ini.in
	cp $< $@

%.o: %.c 
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

%.o: %.cpp
	$(CPP) $(CFLAGS) $(DEFINES) -c -o $@ $<

tracecap.so: $(OBJS)
	$(CPP) $(LDFLAGS) $^ -o $@ $(LIBS)
	ar cru libtracecap.a $@

tracecap-static.so: $(OBJS)
	$(CPP) -static-libgcc -Wl,-static $(LDFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f llconf/*.o llconf/*.d llconf/parsers/*.o llconf/parsers/*.d *.o *.d *.so *.a *~ $(PLUGIN) 

realclean:
	rm -f llconf/*.o llconf/*.d llconf/parsers/*.o llconf/parsers/*.d *.o *.d *.so *.a *~ $(PLUGIN) ini/main.ini ini/hook_plugin.ini

# Include automatically generated dependency files
-include $(wildcard *.d)

