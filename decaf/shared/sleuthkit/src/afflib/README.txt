		   The Advanced Forensic Format 1.0

			 Simson L. Garfinkel
				 and
			Basis Technology, Inc.
			       (C) 2005



INTRODUCTION

The Advanced Forensic Format --- AFF(tm) --- is an extensible open
format for the storage of disk images and related forensic
information.  

Features of AFF include:

* Open format, free from any patent or license restriction.
  Can be used with both open-source and proprietary forensic tools.

* Extensible. Any amount of metadata can be encoded in AFF files in the
  format of name/value pairs. 

* Efficient. AFF supports both compression and seeking within
  compressed files. 

* Open Source C/C++ Implementation. A freely redistributable C/C++
  implementation including the AFF Library and basic conversion tools
  is available for download. AFFLIB(tm) is being distributed under the BSD
  license, allowing it to be incorporated in free and proprietary
  programs without the need to pay license fees.

* Byte-order independent. AFFLib has been tested on both Intel and
  PowerPC-based systems. Images created on one platform can be read on
  another. 

* Automatic calculation and storage of MD5 and SHA-1 hash codes,
  allowing AFF files to be automatically validated after they are
  copied to check for accidentally corruption.

* Explicit identification of sectors that could not be read from the
  original disk. 

* Planned support for digital signatures and direct authoring of AFF
  files using dd_recover. 

AFF is distributed as a subroutine library. The library implements a
FILE-like abstraction that supports the full range of POSIX-like file
routines, including af_open(), af_read(), af_seek(), af_close(). The
af_open() routine checks to see if a file is an AFF file and, if not,
can automatically fall-back into raw mode. Thus, most existing
forensic tools can be trivially modified to work with AFF-formatted
files.


The AFF library can be downloaded from:

http://www.simson.net/afflib/


================================================================
			    HOW AFF WORKS

AFF is a segmented archive file specification. Each AFF file consists
of an AFF File Header followed by one or more AFF Segments. 


AFF SEGMENTS: 

AFF Segments are used to store all information inside the AFF
File. This includes the image itself and image metadata.

Segments can be between 32 bytes and 2^32-1 bytes long. When used to
store the contents of a disk image, the image is broken up into a
number of equal-sized Image Segments. These Image Segments are then optionally
compressed and stored sequentially in the AFF file. 

Each AFF Segment has a header, a name, a 32-bit argument, an optional data
payload, and finally a tail. The header and tail make it possible to
seek rapidly through the AFF file, skipping much of the image data. 

The segment size of the image file is determined when the file is
converted from a RAW file to an AFF file. Once a file is converted, it
can be opened using the af_open() function call and read using
af_read() and af_seek(). The AFF library automatically handles the
locating, reading, and optional decompressing of each segment as needed.

Other segments can be used to hold information such as the time that
the disk was imaged, a case number, the forensic examiner, and the MD5
or SHA-1 of the original unconverted image file. Utility programs are
included in the AFF Library to display this information and validate
the contents of an AFF file against the stored hashes. 


AFF uses OpenSSL for computing hash functions and ZLIB for compressing
image segments.

================================================================
			 AFF UTILITY PROGRAMS

The AFF Library comes with the following utility programs:

afcat     - copies from the contents of an AFFILE to stdout.
afcompare - compares two AFF files or an AFF file and a raw file
afconvert - converts AFF->raw, raw->AFF, or AFF->AFF (or even raw->raw, if you want)
            optionally recompressed files.
affix     - Reports errors with AFF files and optioanlly fixes them. 
afinfo    - prints info about an AFF file from an examination of the segments
afstats	  - prints statistics about one or more AFF files
aftest	  - regression testing for AFF library
afxml	  - outputs an AFF file's metadata as XML

aimage	  - Image a hard drive into AFF or raw format


================================================================
			     AFF DETAILS

AFF SEGMENT NAMES:

The following AFF Segment Names have been defined in the initial
release:

segsize   - The size of each image segment, stored as a 32-bit value.

imagesize - The total number of bytes in the file, stored as a 64-bit
	    value.

md5       - The MD5 of the uncompressed image file, stored as 
	    128-bit value.

sha1      - The SHA-1 of the uncompressed image file, stored as a
	    160-bit value.

badflag   - A 512-bit value that is stored in the file to denote a bad
            sector. This value typically consists of the string 
	    "BAD SECTOR\000" followed by a timestamp and a block of
            random data.


badsector  - The total number of bad sectors in the image, stored as a
             32-bit number. 

seg0	   - The contents of the first segment in the image file. 
	     A flag of '1' stored in the segment argument indicates
	     that the segment was compressed with zlib. 

seg1	   - The contents of the second segment in the image file

segNNN	   - The contents of the NNNth segment of the image file.

a.manufacturer - The manufacturer of the disk drive, stored as a UTF-8
	         string.

a.model    - The model number of the disk drive, stored as a UTF-8
            string

a.property - Any arbitrary "property" of the disk drive, stored as a
             UTF-8 string.

xxx        - This segment should be ignored.  (Space may be left for
             future use.)

""	     - Segments with 0-length name are to be ignored and can
               be garbage collected.
	

THE AFF SEGMENT FORMAT:

Each AFF Segment contains the following information:

     - The Segment Header 
     - The Segment Data Payload
     - The Segment Footer

The Segment Header consists of the following
     - A 4-byte Segment Header Flag ("AFF\000")
     - The Length of the segment name (as an unsigned 4-byte value)
     - The Length of the segment data payload
     - The "argument", a 32-bit unsigned value
     - The data segment name (stored as a Unicode UTF-8  string)

The Segment Footer consists of:
    - The 4-byte Segment Footer Flag ("ATT\000")
    - The length of the entire segment, as a 32-bit unsigned value

Because the segment length can be determined by reading both the
Header or the Footer, the AFF library can seek forwards or backwards
in the AFF file, similar to the way that a tape drive seeks forwards
and backwards through a tape drive. 

All 4-byte binary values are stored in network byte order to provide
for byte order independence. These values are automatically written
with the htonl() macro and read with the ntohl() macro by the AFF
Library.


AFF OPTIMIZATIONS:

Although the AFF file format is quite simple, the library and
conversion routines implement a variety of optimizations to speed
conversion and reading. Among these optimizations are:

* Image segments are only compressed in the AFF file if compression
  would decrease the amount of data required by 5%. Otherwise no
  compression is performed. As a result, images containing
  uncompressable data are not compressed. This saves CPU time. 

* When an image is converted, space is left at the beginning of the
  AFF file for the image hash and other metadata. As a result, this
  information can be rapidly read when a new AFF image is opened.

* AFF's af_read() routine caches the current image segment being read,
  allowing for rapid seeking within the segment. And because all image
  segments represent the same number of bytes in the original image
  file, the library routine can rapidly locate the image segment that
  corresponds to any byte offset within the original raw image, load
  that image segment into memory, and return the sectors that are
  requested.


LICENSE

The AFF Library is distributed with a modified BSD license that allows
the use of AFF in any program, free or commercial, provided that the
copyright statement is included in both the source and binary file,
and in any advertisements for products based on the Garfinkel/Basis
AFF implementation.

================================================================
ACKNOWLEDGEMENTS

Brian Carrier provided useful feedback on the initial AFF design. 

Jean-Francois Beckers has provided many suggestions for functionality
and has found many bugs in his extensive testing. 

================================================================
AFF DIRECTORIES

Many file systems limit the maximum file size to 2^32-1 or
4,294,967,295 bytes.  Unfortunately, even compressed images can easily
exceed this size.

An AFF Directory (.affd) is a directory of AFF files that the AFF
Library knits together to give the appearance of a single AFF
file. No rules are imposed on the names of these files or the order of
the segments that they contain. If a segment with the same name exists
in more than one AFF file it is undefined which file's segment will be
returned if that segment is read.

The AFFDIR is actually implemented using the AFF VNODE abstraction
(which allows arbitrary file system interfaces) and a special driver
that recursively opens multiple AFF sub-files. The current system
opens all of the files in the directory and keeps them all open.


The "aimage" analyzes the disk being imaged and the file system and
determines if the image can be stored in an AFF file or in an
AFFDIR. If it creates a dir, it automatically creates new AFF files
when the files being imaged exceeds 500MB. 

================
Note: AFF and AFFLIB are trademarks of Simson L. Garfinkel and Basis
Technology, Inc. 


# Local Variables:
# mode: auto-fill
# mode: flyspell
# End:
