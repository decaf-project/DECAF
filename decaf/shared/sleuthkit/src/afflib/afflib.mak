# Makefile for AFFLIB under Windows
# Don't expect this to work; I haven't used it in a while

OBJS = afflib.obj afflib_stream.obj afflib_util.obj unix4win32.obj win32_getopt.obj 

INCS=/Izlib123\include /Ic:\OpenSSL\Include

CC=cl

LIBS=ws2_32.lib c:\openssl\lib\vc\ssleay32.lib c:\openssl\lib\vc\libeay32.lib zlib123\lib\zdll.lib

CFLAGS=/Od /W4 /Yd /Gm /GX /Zi /ZI /GF /GZ /Ge \
        /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "WIN32_NT" \
        /D "MSC" /Fp"slib.pch" /c  /YX /nologo /MTd $(INCS) 


CPPFLAGS=/Od /W4 /Yd /Gm /GX /Zi /ZI /GF /GZ /Ge \
        /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "WIN32_NT" \
        /D "MSC" /Fp"slib.pch" /c  /YX /nologo /MTd $(INCS) 

#
# C/C++ Compiler options
#
# -OPTIMIZATION-
# /Od       - disable optimizations
#
# -CODE GENERATION-
# /W4       - warning level 4
# /Yd       - put debug info in every .OBJ (rather than in vc60.pcb)
# /Gm       - enable minimal rebuild
# /GX       - enabled C++ EH
# /Zi       - enable debugging info
# /ZI       - enable full Edit and Continue info
# /GF       - enable read-only string pooling
# /GZ       - enable runtime debug checks
# /Ge       - force stack checking for all funcs
#
# -PREPROCESSOR-
# /I        - specifies include directory
# /D        - define a switch
#
# -OUTPUT FILES-
# /Fp       - Precompiled headers
#
# -MISCELLANEOUS-
# /nologo   - Disable logo
# /YX       - automatic .PCH
# /c        - compile only, don't link
#
# -LINKING-
# /MTd      - Like multithreaded with debug info

LINKSTD=/nologo /incremental:yes /pdb:"afflib.pdb" \
        /machine:I386 /pdbtype:sept /map 
LINKDEBUG=$(LINKSTD) /debug

# Previously had /nodefaultlib:libcmt  in there

all: afflib.lib afinfo.exe afconvert.exe

afinfo.exe: afflib.lib afinfo.obj 
	link -out:afinfo.exe afinfo.obj afflib.lib $(LINKDEBUG) $(LIBS)

afconvert.exe: afflib.lib afconvert.obj
	link -out:afconvert.exe afconvert.obj afflib.lib $(LINKDEBUG) $(LIBS)

afflib.lib: $(OBJS) afflib.mak 
	-del afflib.lib
	lib /out:afflib.lib $(OBJS)
