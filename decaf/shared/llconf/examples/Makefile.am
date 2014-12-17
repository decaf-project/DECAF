EXTRA_DIST=wizard etc/hostname  etc/hosts  etc/interfaces  etc/interfaces  etc/resolv.conf

noinst_PROGRAMS=example

example_SOURCES=	\
	example.c

AM_CFLAGS = -I../src/ -I../src/parsers/

example_LDADD = ../src/libllconf.la

