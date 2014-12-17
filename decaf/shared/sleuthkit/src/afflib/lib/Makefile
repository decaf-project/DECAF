include ../Makefile.defs
include Makefile.files

LIBDIRS += -L..

clean:
	rm -f $(AFFLIB_OBJS) $(AFFLIB_TARGETS) $(AFFLIB)

testall: aftest
	@echo Testing all implementations
	/bin/rm -rf tmp
	mkdir tmp
	./aftest -d tmp -e aff -a
	./aftest -d tmp -e afd -a
	./aftest -d tmp -e afm -a

