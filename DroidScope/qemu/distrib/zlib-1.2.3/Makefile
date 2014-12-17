# Makefile used to compile zlib statically
#
ZLIB_LIB    := $(SRC_PATH)/libz.a
ZLIB_CFLAGS := -I$(ZLIB_DIR) $(MY_CFLAGS)

include $(ZLIB_DIR)/sources.make
ZLIB_OBJS := $(ZLIB_SOURCES:%.c=%.o)

$(ZLIB_LIB): $(ZLIB_OBJS)
	ar ru $(ZLIB_LIB) $(MY_LDLIBS) $(ZLIB_OBJS)

$(ZLIB_OBJS): CFLAGS += $(ZLIB_CFLAGS)

clean-zlib:
	rm -f $(ZLIB_OBJS) $(zlib_lib)
