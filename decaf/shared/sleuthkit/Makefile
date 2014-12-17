SHELL	= /bin/sh
CC		= gcc
DIRS	= bin

all:	$(DIRS) compile

static:	$(DIRS) compile-static

$(DIRS):
	mkdir -p $@

compile: no-perl mactime sorter file test 

compile-static: no-perl-static mactime sorter file test

no-perl:
	cd src/auxtools; $(MAKE) "CC=$(CC)" MAKELEVEL=
	cd src/afflib/lib; $(MAKE) "CC=$(CC)" MAKELEVEL= 
	cd src/libewf; $(MAKE) "CC=$(CC)" MAKELEVEL= libewf.a
	cd src/imgtools; $(MAKE) "CC=$(CC)" MAKELEVEL=
	cd src/hashtools; $(MAKE) "CC=$(CC)" MAKELEVEL=
	cd src/fstools; $(MAKE) "CC=$(CC)" MAKELEVEL=
	cd src/mmtools; $(MAKE) "CC=$(CC)" MAKELEVEL=
	cd src/srchtools; $(MAKE) "CC=$(CC)" MAKELEVEL=
	cd src/disktools; $(MAKE) "CC=$(CC)" MAKELEVEL=

no-perl-static:
	cd src/auxtools; $(MAKE) "CC=$(CC)" OPT=-static MAKELEVEL=
	cd src/afflib/lib; $(MAKE) "CC=$(CC)" MAKELEVEL=
	cd src/libewf; $(MAKE) "CC=$(CC)" MAKELEVEL= libewf.a
	cd src/imgtools; $(MAKE) "CC=$(CC)" MAKELEVEL=
	cd src/hashtools; $(MAKE) "CC=$(CC)" OPT=-static MAKELEVEL= 
	cd src/fstools; $(MAKE) "CC=$(CC)" OPT=-static MAKELEVEL= 
	cd src/mmtools; $(MAKE) "CC=$(CC)" OPT=-static MAKELEVEL= 
	cd src/srchtools; $(MAKE) "CC=$(CC)" OPT=-static MAKELEVEL= 
	cd src/disktools; $(MAKE) "CC=$(CC)" OPT=-static MAKELEVEL= 
	cd src/file; CC="$(CC)" sh ./TSK.install


sorter:
	cd src/sorter; $(MAKE)

mactime:
	cd src/timeline;  $(MAKE)

file:
	cd src/file; CC="$(CC)" sh ./TSK.install

test:
	sh ./check-install

clean: 
	cd src/hashtools; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/afflib/lib; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/libewf; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/imgtools; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/auxtools; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/fstools; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/mmtools; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/srchtools; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/disktools; $(MAKE) $@ "CC=$(CC)" MAKELEVEL=
	cd src/file; sh ./TSK.clean
	cd src/timeline; $(MAKE) $@
	cd src/sorter; sh ./clean
	rm -f bin/*core
	rm -f bin/.gdb_history
	find . -name ".DS_Store" | xargs rm -f 
	find . -name ".*" | grep -v "cvsignore" | grep -v "indent.pro" | grep -v "tidyrc"
#	find . -type f -perm +g+x,o+x,u+x | grep -v "configure" | xargs  chmod -x -f
	cat patchlevel

release:
	find . -name "CVS" | xargs rm -rf 
