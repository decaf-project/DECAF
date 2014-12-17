#ifndef _AFFLIB_H_
#define _AFFLIB_H_ 

/*
 * afflib.h:
 * 
 * This file describes the public AFFLIB interface.
 * The interface to reading AFF files and Raw files. 
 * (Soon Encase files!)
 */

#define AFF_VERSION	1.1
#define AFFLIB_VERSION  1.6.22

/* Figure out what kind of OS we are running on */

#ifdef __FreeBSD__
#define BSD_LIKE
#define HAS_GETPROGNAME
#endif

#ifdef __OpenBSD__
#define BSD_LIKE
#endif

#ifdef __NetBSD__
#define BSD_LIKE
#endif

#ifdef __APPLE__
#define BSD_LIKE
#define HAS_GETPROGNAME
#endif

#ifdef BSD_LIKE
typedef unsigned long long uint64;
typedef long long int64;
#define I64d "qd"
#define I64u "qu"
#include <sys/cdefs.h>
#define UNIX
#endif

#ifdef linux
typedef unsigned long long uint64;
typedef long long  int64;
#define I64d "qd"
#define I64u "qu"
/* Horrible lossage stuff for largefile support under Linux */
#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64
#include <sys/cdefs.h>
#define UNIX
#endif

#ifdef sun
#include <inttypes.h>
typedef unsigned long long uint64;
typedef long long  int64;
#define I64d "qd"
#define I64u "qu"
#define UNIX
#endif 

#ifdef WIN32                            
typedef unsigned _int64 uint64;		/* 64-bit types Types */
typedef          _int64  int64;
#define I64d      "I64d"		/* windows doesn't do %qd */
#define I64u      "I64u"
int64   ftello(FILE *stream);	 /* Functions that Microsoft forgot */
int     fseeko(FILE *stream,int64 offset,int whence);
#endif

#ifndef I64d
/* Assume some kind of Unix running GCC */
typedef unsigned long long uint64;
typedef long long int64;
#define I64d "qd"
#define I64u "qu"
#define UNIX
#endif 

#include <stdio.h>

struct affcallback_info;
typedef struct _AFFILE {
    int   version;			// 2
    void   *tag;			// available to callers; unused by AFFLIB
    struct af_vnode *v;			// which function table to use.
    struct _AFFILE *parent;		// if we have one

    /* For all files */

    int openflags;			// how it was opened
    int openmode;			// how we were asked to open it; more
    int exists;				// did file exist before open was called?

    /* For extended logging */
    FILE    *logfile;
    char    *fname;			// Filename of file; be sure to free when done
    char    error_str[64];		// what went wrong

    /* Implement a stream abstraction */
    uint64        image_size;		// last mappable byte of disk image
    uint64        image_size_in_file;	// see if it was changed...
    unsigned long image_pagesize;	// the size of image data segments in this file
    unsigned long image_sectorsize;
    int64         pagenum;		// -1 means no page loaded 
    unsigned char *pagebuf;		// where the data is
    size_t        pagebuf_bytes;        // number of bytes in the pagebuf that are valid.
    uint64	  pos;			// location in stream
    unsigned int  writing:1;		// is file open for writing?
    unsigned int  dirty:1;		// is buffer dirty?
    int		  debug;		// for debugging, of course

    unsigned char *badflag;		// bad sector flag


    /****************************************************************/
    /* Right now the instance variables for each implementation are here,
     * which is ugly but easier for development...
     */

    /* For AFF Segment Files; this needs to be moved into private storage... */
    FILE          *aseg;
    struct af_toc_mem *toc;		// table of contents
    int	           toc_count;	       // number of directory elements

    /****************************************************************/

    /* additional support for writing. */
    unsigned int usegs_written;		// uncompressed segments written
    unsigned int csegs_written;		// compressed segments written
    unsigned int compression_type;	// 0 is no compression
    int		 compression_level;	// 0 is no compression

    /* w_callback:
     * A callback that is called before and after each segment is written.
     * Called with the arguments (i,0,0) at the beginning of the write operation.
     * Called with the arguments (i,j,k) at the end of the write operation.
     * i = segment number
     * j = segment length
     * If segment is being written with compresison, k = compressed length.
     * If segment is written w/o compression, k = 0
     */
    void (*w_callback)(struct affcallback_info *acbi);
    // called at start and end of compression.

    /* Misc */
    uint64	bytes_memcpy;		// total number of bytes memcpy'ed
    uint64	maxsize;		// maximum file size of a multi-segment files,
                                        // or 0 if this is not a multi-segment file

    void	*vnodeprivate;	      // private storage for the vnode
    void	(*error_reporter)(const char *fmt, ...);
} AFFILE;

/* The information that is provided in the aff callback */
struct affcallback_info {
    int info_version;			// version number for this segment
    AFFILE *af;				// v1: the AFFILE responsibile for the callback 
    int phase;				// v1: 1 = before compress; 2 = after compressing;
					//     3 = before writing; 4 = after writing
    int64 pagenum;			// v1: page number being written
    int bytes_to_write;			// v1: >0 if we are going to write bytes
    int bytes_written;			// v1: >0 if bytes were written
    int compressed;			// v1: >0 if bytes were/will be compressed
    int compression_alg;		// v1: compression algorithm
    int compression_level;		// v1: compression level
};




/* AFF Log Mask */
#define AF_LOG_af_seek  0x0001
#define AF_LOG_af_write 0x0002
#define AF_LOG_af_close 0x0004

/* Utility Functions */

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __never_defined__
}
#endif

/****************************************************************
 ***
 *** Intended user AFF interface
 ***
 ****************************************************************/

/* af_file stream functions */
int     af_identify_type(const char *filename); // returns type of a file
const char *af_identify_name(const char *filename); // returns name of a file type

AFFILE *af_open(const char *filename,int flags,int mode);
AFFILE *af_freopen(FILE *file);		// reopen a raw file as an AFFILE
AFFILE *af_popen(const char *command,const char *type);	// no need to use pclose(); af_close() is fine
int	af_close(AFFILE *af);
void	af_set_error_reporter(AFFILE *af,void (*reporter)(const char *fmt,...));

/* Special AFOPEN flags */
#define AF_OPEN_PRIMITIVE (1<<31)	// only open primtive, not compound files


/* navigating within the data segments as if they were a single file */
int	af_read(AFFILE *af,unsigned char *buf,int count);
uint64  af_seek(AFFILE *af,uint64 pos,int whence); // returns new position
uint64  af_tell(AFFILE *af);
int	af_eof(AFFILE *af);		// is the virtual file at the end?

/* Additional routines for writing */
void	af_enable_writing(AFFILE *af,int flag);	// set true to enable writing; returns old value
void	af_set_callback(AFFILE *af, void (*cb)(struct affcallback_info *acbi)); 
void	af_enable_compression(AFFILE *af,int type,int level); // set/gunset compression for writing
int	af_compression_type(AFFILE *af);
int	af_write(AFFILE *af,unsigned char *buf,int count);
const unsigned char
	*af_badflag(AFFILE *af); // return the pattern used to identify bad blocks
int	af_is_badblock(AFFILE *af,unsigned char *buf); // 0 if not, 1 if it is, -1 if error


/* AFF implementation types returned by af_identify() */

#define AF_IDENTIFY_RAW 0		// file is a raw file
#define AF_IDENTIFY_AFF 1		// file is an AFF file
#define AF_IDENTIFY_AFD 2		// file is a directory of AFF files
#define AF_IDENTIFY_EVF 3		// file is an EnCase file
#define AF_IDENTIFY_EVD 4		// file is a .E01 file when there are more files following
#define AF_IDENTIFY_SPLIT_RAW 5		// file is a split raw file
#define AF_IDENTIFY_AFM 6             // file is raw file with metadata



#define AF_IDENTIFY_ERR -1		// error encountered on identify
#define AF_IDENTIFY_NOEXIST -2		// file does not exist


/* Misc. Functions */
const char *af_ext(const char *filename);	// return the extension of str including the dot
int	    af_ext_is(const char *filename,const char *ext);
const char *af_filename(AFFILE *af);	// returns the filename of an open stream.
int	    af_identify(AFFILE *af);	// returns type of AFFILE pointer

/* Accessor Functions */
int64	    af_imagesize(AFFILE *af);	// byte # of last mapped byte in image, or size of device;
					// returns -1 if error
int	    af_get_segq(AFFILE *af,const char *name,int64 *quad);/* Get/set 8-byte values */
int	    af_update_segq(AFFILE *af,const char *name,int64 quad,int append);


/****************************************************************
 * Functions for manipulating the AFFILE as if it were a name/value database.
 ****************************************************************/

/* get functions:
 * get the named segment.
 * If arg!=0, set *arg to be the segment's flag.
 * if data==0, don't return it.
 * if datalen && *datalen==0, return the size of the data segment.
 *** Returns 0 on success, 
 *** -1 on end of file. (AF_ERROR_EOF)
 *** -2 if *data is not large enough to hold the segment (AF_ERROR_DATASMALL)
 *** -3 file is corrupt or other internal error. (AF_ERROR_CORRUPT)
 */

int	af_get_seg(AFFILE *af,const char *name,unsigned long *arg,
		   unsigned char *data,size_t *datalen);
int	af_get_next_seg(AFFILE *af,char *segname,size_t segname_len,
			unsigned long *arg, unsigned char *data, size_t *datalen);

int	af_rewind_seg(AFFILE *af); // rewind seg pointer to beginning

/*
 * af_update_seg() should be your primary routine for writing new values.
 * if append==0, then the value will not be written if the segment doesn't
 * already exist. If append!=0, then the segment will be appended to the end
 * of the file if it is not there already.
 */

/* This one writes arbitrary name/value pairs */
int	af_update_seg(AFFILE *af,const char *name,unsigned long arg,
		      const void *value,unsigned int vallen,int append);

/* Delete functions */

int	af_del_seg(AFFILE *af,const char *name); // complete delete of first name
                                                 // returns 0 if success, -1 if seg not found

int64	af_segname_page_number(const char *name);
/* Returns page number if segment name is a page #, and -1 if it is not */

/****************************************************************/



/* Metadata access */


/* Compression #defines */
#define AF_COMPRESSION_NONE 0
#define AF_COMPRESSION_ZLIB 1
#define AF_COMPRESSION_MIN  1
#define AF_COMPRESSION_DEFAULT -1
#define AF_COMPRESSION_MAX 9
#define AF_COMPRESSION_MIN 1


/****************************************************************
 *** AF segment names that you might be interested in...
 ****************************************************************/


#define AF_IGNORE       ""		// ignore segments with 0-length name
#define AF_DIRECTORY    "dir"		// the directory
#define AF_RAW_IMAGE_FILE_EXTENSION "raw_image_file_extension"
#define AF_PAGES_PER_RAW_IMAGE_FILE "pages_per_raw_image_file"

#define AF_PAGESIZE	"pagesize"	// page data size, in bytes, stored in arg
#define AF_IMAGESIZE	"imagesize"	// last logical byte in image, stored as a 64-bit number
#define AF_BADSECTORS	"badsectors"	// number of bad sectors
#define AF_SECTORSIZE	"sectorsize"	// in bytes, stored in arg
#define AF_DEVICE_SECTORS "devicesectors"// stored as a 64-bit number
#define AF_BADFLAG      "badflag"	// data used to mark a bad sector
#define AF_PAGE		"page%"I64d	// segment flag indicates compression (replaces seg%d)
#define AF_BLANKSECTORS "blanksectors"	// all NULs; 8-bytes
#define AF_RAW_IMAGE_FILE_EXTENSION "raw_image_file_extension"
#define AF_PAGES_PER_RAW_IMAGE_FILE "pages_per_raw_image_file"
#define AF_AFF_FILE_TYPE "aff_file_type" // contents should be "AFF", "AFM" or "AFD"

/* Deprecated terminology; pages were originally called data segments */
#define AF_SEG_D        "seg%"I64d	// segment flag indicates compression (deprecated)
#define AF_SEGSIZE_D	"segsize"	// segment data size (deprecated)

/* AFF Flags */
/* Flags for 8-byte segments */
#define AF_SEG_QUADWORD        0x0002	

/* Flags for data pages */
#define AF_PAGE_COMPRESSED      0x0001
#define AF_PAGE_COMP_MAX        0x0002	// compressed at maximum
#define AF_PAGE_COMP_ALG_MASK   0x00F0	// up to 16 compression algorithms may be used
#define AF_PAGE_COMP_ALG_ZLIB   0x0000	
#define AF_PAGE_COMP_ALG_BZIP   0x0010



#define AF_MD5  "md5"			// stores image md5
#define AF_SHA1 "sha1"			// stores image sha1

#define AF_CREATOR	"creator"	// progname of the program that created the AFF file

/* segment names: imaging */
#define AF_CASE_NUM			"case_num"      // case number
#define AF_IMAGE_GID			"image_gid"      // 128-bit unique number
#define AF_ACQUISITION_ISO_COUNTRY  "acquisition_iso_country" // ISO country code
#define AF_ACQUISITION_COMMAND_LINE "acquisition_commandline" // actual command line used to create the image
#define AF_ACQUISITION_DATE	    "acquisition_date" // YYYY-MM-DD HH:MM:SS TZT
#define AF_ACQUISITION_NOTES	    "acquisition_notes" // notes made while imaging
#define AF_ACQUISITION_DEVICE	    "acquisition_device" // device used to do the imaging
#define AF_ACQUISITION_SECONDS      "acquisition_seconds" // stored in arg
#define AF_ACQUISITION_TECHNICIAN   "acquisition_tecnician" 
#define AF_ACQUISITION_MACADDR      "acquisition_macaddr" 
#define AF_ACQUISITION_DMESG	    "acquisition_dmesg"


//  mac addresses are store in ASCII as a list of lines that end with \n,
//  for example, "00:03:93:14:c5:04\n" 
//  It is all the mac addresses that were on the acquisition system

// DMESG is the output from the "dmesg" command at the time of acquisition


/* segment names: device hardware */

#define AF_AFFLIB_VERSION	"afflib_version" // version of AFFLIB that made this file
#define AF_DEVICE_MANUFACTURER  "device_manufacturer"
#define AF_DEVICE_MODEL		"device_model"	// string for ident from drive
#define AF_DEVICE_SN		"device_sn"	// string of drive capabilities
#define AF_DEVICE_FIRMWARE	"device_firmware"	// string of drive capabilities
#define AF_DEVICE_SOURCE        "device_source" // string
#define AF_CYLINDERS		"cylinders" // quad with # cylinders
#define AF_HEADS		"heads"	// quad with # heads
#define AF_SECTORS_PER_TRACK	"sectors_per_track"// quad with # sectors/track
#define AF_LBA_SIZE		"lbasize"
#define AF_HPA_PRESENT          "hpa_present"   // flag = 1 or 0
#define AF_DCO_PRESENT          "dco_present"   // flag = 1 or 0
#define AF_LOCATION_IN_COMPUTER "location_in_computer" // text, where it was found
#define AF_DEVICE_CAPABILITIES	"device_capabilities" // string; human-readable

#define AF_MAX_NAME_LEN 64	// segment names should not be larger than this


/* AFF error codes */
#define AF_ERROR_EOF -1
#define AF_ERROR_DATASMALL -2
#define AF_ERROR_TAIL  -3		// no tail, or error reading tail
#define AF_ERROR_SEGH  -4		// no head, or error reading head
#define AF_ERROR_NAME  -5		// segment name invalid
#define AF_ERROR_INVALID_ARG -6		// argument invalid


/****************************************************************
 *** Internal implementation details below.
 ****************************************************************/


#ifdef __never_defined__
{
#endif
#ifdef __cplusplus
}
#endif
#endif
