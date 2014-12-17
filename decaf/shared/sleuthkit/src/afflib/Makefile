#!/usr/local/bin/gmake
#
# Makefile for Advanced Forensics Format library
#
# Targets:
# make lib  --- just make the library
# make tools --- just make the tools
#
# Directories:
# lib   - the libraries and the test program
# tools - the tools
# aimage - the advanced imager

# Applications:
# aimage    - imaging program
#
# Utilities:
# afcat      - copies out the raw info from the AFF file
# afcompare  - compares an ISO/RAW file with an AFF file to say if they are equal
#             optional [-d] option will delete the ISO file if they are
# afconvert  - converts a ISO/RAW file to an AFF file, and back.
# afinfo     - prints out information about an AFF file
# aftest     - Validate the AFF library with a lot of tests.
# afxml      - Output xfxml

RELEASE      := afflib-$(shell sh current_version.sh)
RELEASE_HOST := www.afflib.org
RELEASE_LOC  := public_html/afflib/downloads/
RELEASE_VER  := http://www.afflib.org/downloads/version.txt

INSTALL_TOOLDIR ?= /usr/local/bin
INSTALL_LIBDIR  ?= /usr/local/lib
INSTALL_INCDIR  ?= /usr/local/include

all: all_targets

# Bring in defines
include Makefile.defs

#
# Bring in the Makefiles for the library, the tools, and aimage
#
LIBD := lib/
include $(LIBD)/Makefile.files

TD := tools/
include $(TD)/Makefile.files

D := aimage/
include aimage/Makefile.files

TARGETS := $(AIMAGE_TARGETS) $(AFFLIB_TARGETS) $(AFFTOOLS_TARGETS)

SRCS += Makefile Makefile.defs Makefile.dep $(wildcard *.txt) $(wildcard *.doc) $(wildcard zlib*) make.bat TAGS

all_targets: $(TARGETS)

version:
	@echo This is AFFLIB $(RELEASE)

lib: $(AFFLIB)

install: $(AIMAGE_TARGETS) $(AFFTOOLS_TARGETS)
	install $(AIMAGE_TARGETS) $(AFFTOOLS_TARGETS) $(INSTALL_TOOLDIR)
	install $(AFFLIB) $(INSTALL_LIBDIR)/libafflib.a
	ranlib $(INSTALL_LIBDIR)/libafflib.a			# sometimes a second is needed
	install lib/afflib.h $(INSTALL_INCDIR)/afflib.h

install-lib: $(AFFLIB)
	install $(AFFLIB) $(INSTALL_LIBDIR)/libafflib.a
	ranlib $(INSTALL_LIBDIR)/libafflib.a			# sometimes a second is needed
	install lib/afflib.h $(INSTALL_INCDIR)/afflib.h

tests: $(AFFTEST)
	echo Running AFF tests on `uname -a`
	$(AFFTEST) -a

testlocal:
	@echo ***
	@echo ***
	@echo *** testing AFFLIB on `hostname` `uname -m`
	@echo ***
	svn update
	$(MAKE) -s clean
	$(MAKE) -s all
	$(MAKE) -s tests

testall:
	@echo Testing AFFLIB on all platforms
	make testlocal
	ssh l1 '(cd slg/drives/afflib;make testlocal)'
	ssh h1 '(cd slg/drives/afflib;make testlocal)'
	ssh s  '(cd slg/drives/afflib;make testlocal)'

echosrcs: 
	@echo $(SRCS)

tags: ETAGS

ETAGS: $(*/*.cpp) $(*/*.h)
	etags */*.cpp */*.h


#
# This is for Simson to make an AFF release.
#

TARFILE := $(RELEASE).tar.gz

$(TARFILE): $(SRCS)

tarfile: $(TARFILE)

release:
	@echo Making release $(RELEASE)
	#
	# First make sure this version isn't on the server already
	#
	ftp $(RELEASE_VER)
	@echo Version `cat version.txt` is on the server.
	python checkversion.py `cat version.txt` $(RELEASE)
	exit 0
	/bin/rm version.txt
	#
	# Clean things up
	#
	$(MAKE) clean
	/bin/rm -rf /tmp/$(RELEASE)        # erase the old release
	mkdir /tmp/$(RELEASE)              # make a dir
	cp -r * /tmp/$(RELEASE)            # copy the files
	cp -f /dev/null /tmp/$(RELEASE)/Makefile.dep  # zero the dep file
	find -d /tmp/$(RELEASE) -name .svn -exec rm -rf {} \;
	(cd /tmp;tar cfvz $(RELEASE).tar.gz ./$(RELEASE))
	#
	#
	# Upload this release tarfile and version.txt to the server
	#
	scp /tmp/$(RELEASE).tar.gz $(RELEASE_HOST):$(RELEASE_LOC)
	@echo $(RELEASE) > version.txt
	scp version.txt $(RELEASE_HOST):$(RELEASE_LOC)
	/bin/rm version.txt
	@echo Release $(RELEASE) uploaded to server

validate: $(TARGETS) 
	(cd tests;$(MAKE) all)


clean:
	(cd aimage;$(MAKE) clean)
	(cd tests;$(MAKE) clean)
	$(RM) $(TARGETS) *.o *~ */*~ .gdb_history *.core *.a */*.a
	$(RM) */*.o
	$(RM) */.gdb_history
	$(RM) $(AFFLIB) $(AFFLIB_OBJS) 
	$(RM) -r work aimage/random.* random.*
	$(RM) test*.aff *.bak version.txt
	$(RM) TAGS 
	$(MAKE) tags

################################################################




