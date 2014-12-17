SUBDIRS=parsers

pkginclude_HEADERS = entry.h lines.h modules.h nodes.h parseerror.h \
			strutils.h

AM_CFLAGS=-DLLCONF

lib_LTLIBRARIES = libllconf.la

bin_PROGRAMS=llconf

libllconf_la_SOURCES=\
	parsers/libllparsers.la \
	modules.c modules.h \
	entry.c  entry.h \
	parseerror.c  parseerror.h \
	lines.c  lines.h \
	nodes.c  nodes.h \
	strutils.c  strutils.h

llconf_SOURCES=	\
	entry.h \
	lines.h \
	modules.h \
	nodes.h \
	strutils.h \
	llconf.c

llconf_LDADD = libllconf.la

libllconf_la_LDFLAGS = -version-info 1:0:1
libllconf_la_LIBADD = -ldl parsers/libllparsers.la

.PHONY: clean
