# this is included by various Makefiles
ZLIB_SOURCES := adler32.c compress.c crc32.c deflate.c gzio.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c
ZLIB_SOURCES := $(ZLIB_SOURCES:%=$(ZLIB_DIR)/%)

