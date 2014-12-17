# Makefile used to compile libpng statically
# you need to define ZLIB_INCLUDE to the Zlib include path
# and PREFIX to the installation path
#
LIBPNG_LIB    := $(SRC_PATH)/libpng.a
LIBPNG_CFLAGS := -I$(LIBPNG_DIR)

HOST_ARCH := $(shell uname -p)
HOST_OS   := $(shell uname -s)
ifeq ($(HOST_OS),Darwin)
    HOST_OS := darwin
endif

include $(LIBPNG_DIR)/sources.make

LIBPNG_OBJS := $(LIBPNG_SOURCES:%.c=%.o)

$(LIBPNG_LIB): $(LIBPNG_OBJS)
	ar ru $@ $(LIBPNG_OBJS)

$(LIBPNG_OBJS): CFLAGS += $(ZLIB_CFLAGS) $(LIBPNG_CFLAGS)

clean-libpng:
	rm -f $(LIBPNG_OBJS) $(LIBPNG_LIB)

