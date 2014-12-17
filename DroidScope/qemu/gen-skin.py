#!/usr/bin/python
#
# a python script used to generate the "default-skin.h' header file
# from a given skin directory
#
# usage:
#    progname skin-directory-path > default-skin.h
#
import sys, os, string, re

header = """\
/* automatically generated, do not touch */

"""


footer = """\

static const FileEntry  _file_entries[] =
{
"""

footer2 = """\
    { NULL, NULL, 0 }
};
"""


entries = []

def process_files( basepath, files ):
    for file in files:
        fp = open(basepath + "/" + file, "rb")
        data = fp.read()
        data_len  = len(data)
        data_add  = 0
        data_name = "_data_" + string.replace(file,".","_")

        entries.append( (file, data_name, len(data)) )
        print "static const unsigned char %s[%d] = {" % (data_name, data_len + data_add)
        comma = "    "
        do_line = 0
        do_comma = 0
        count = 0
        line  = "    "
        for b in data:
            d = ord(b)

            if do_comma:
                line = line + ","
                do_comma = 0

            if do_line:
                print line
                line = "    "
                do_line = 0

            line = line + "%3d" % d
            do_comma = 1
            count += 1
            if count == 16:
                count = 0
                do_line = 1

        if len(line) > 0:
            print line
        print "};\n"

if len(sys.argv) != 2:
    print "usage: progname  skindirpath > default-skin.h"
else:
    print header
    skindir = sys.argv[1]
    process_files( skindir, os.listdir(skindir) )
    print footer
    for e in entries:
        print "    { \"%s\", %s, %d }," % (e[0], e[1], e[2])
    print footer2
