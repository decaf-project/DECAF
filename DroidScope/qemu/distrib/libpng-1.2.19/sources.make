# this file is included by various Makefiles and defines the set of sources used by our version of LibPng
#
LIBPNG_SOURCES := png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c \
                  pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngvcrd.c pngwio.c \
                  pngwrite.c pngwtran.c pngwutil.c

# Enable MMX code path for x86, except on Darwin where it fails
PNG_MMX := no
ifeq ($(HOST_ARCH),x86)
    PNG_MMX := yes
endif
ifeq ($(HOST_OS),darwin)
    PNG_MMX := no
endif

ifeq ($(PNG_MMX),yes)
    LIBPNG_SOURCES += pnggccrd.c
else
    LIBPNG_CFLAGS += -DPNG_NO_MMX_CODE
endif

LIBPNG_SOURCES := $(LIBPNG_SOURCES:%=$(LIBPNG_DIR)/%)

