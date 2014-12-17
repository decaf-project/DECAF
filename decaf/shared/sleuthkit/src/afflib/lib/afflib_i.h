/*
 * afflib_i.h:
 * The "master include file" of the AFF Library.
 * Includes many fucntions that are not designed to be used by application programmers.
 */

/*
 * Copyright (c) 2005
 *	Simson L. Garfinkel and Basis Technology, Inc. 
 *      All rights reserved.
 *
 * This code is derrived from software contributed by
 * Simson L. Garfinkel
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Simson L. Garfinkel
 *    and Basis Technology Corp.
 * 4. Neither the name of Simson Garfinkel, Basis Technology, or other
 *    contributors to this program may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIMSON GARFINKEL, BASIS TECHNOLOGY,
 * AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL SIMSON GARFINKEL, BAIS TECHNOLOGy,
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.  
 */

#ifndef AFFLIB_I_H
#define AFFLIB_I_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <openssl/rand.h>

#ifdef WIN32
#include "unix4win32.h"
#include <winsock.h>			// htonl()
#endif

#ifdef UNIX
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#endif

/* Handle systems that don't define things properly */

#ifndef O_BINARY
#define O_BINARY 0			// for Windows compatability
#endif

#ifndef ENOTSUP
#define ENOTSUP EOPNOTSUPP
#endif


#ifdef BSD_LIKE
#include <err.h>
#include <openssl/rand.h>
#endif

#ifdef __APPLE__
#define POINTER_FMT "%p"
#endif

#ifdef linux
#define POINTER_FMT "%p"
#include <err.h>
#endif

#ifdef sun
#define NO_ERR
#define NO_WARNX
void err(int eval, const char *fmt, ...);
void warnx(const char *fmt, ...);
#include <alloca.h>
#endif

#ifndef POINTER_FMT
#define POINTER_FMT "%x"		// guess
#endif


/* The AFF STREAM VNODE */
struct af_vnode {
    int type;				// numeric vnode type
    int flag;				// file system flag type
    char *name;
    int (*identify)(const char *fname);	// returns 1 if file system is identified by implementation
    int (*open)(AFFILE *af);
    int (*close)(AFFILE *af);
    int (*vstat)(AFFILE *af,struct af_vnode_info *);	// returns info about the vnode image file
    int (*get_seg)(AFFILE *af,const char *name,unsigned long *arg, unsigned char *data,size_t *datalen);
    int	(*get_next_seg)(AFFILE *af,char *segname,size_t segname_len,
			unsigned long *arg, unsigned char *data, size_t *datalen);
    int (*rewind_seg)(AFFILE *af);
    int (*update_seg)(AFFILE *af,const char *name,unsigned long arg,
		      const void *value,unsigned int vallen,int append);
    int (*del_seg)(AFFILE *af,const char *name);
    int (*read)(AFFILE *af,unsigned char *buf,uint64 offset,int count);
    int (*write)(AFFILE *af,unsigned char *buf,uint64 offset,int count);
};

#define AF_VNODE_TYPE_PRIMITIVE 0x01	// single-file impelemntation
#define AF_VNODE_TYPE_COMPOUND  0x02	// multi-file impelemntation

struct af_vnode_info {
    int64 imagesize;			// size of this image
    int   pagesize;			// what is the natural page size?
    unsigned int supports_compression:1; // supports writing compressed segments
    unsigned int has_pages:1;		// does system support page segments?
    unsigned int supports_metadata:1;	// does it support metadata?
    unsigned int use_eof:1;		// should we use the EOF flag?
    unsigned int at_eof:1;		// are we at the EOF?
    unsigned int changable_pagesize:1;	// pagesize can be changed at any time
    unsigned int changable_sectorsize:1;// sectorsize can be changed at any time
};


int af_vstat(AFFILE *af,struct af_vnode_info *vni); // does the stat

/* The header for an AFF file. All binary numbers are stored in network byte order. */
#define AF_HEADER "AFF10\r\n\000"
struct af_head {
    char header[8];			// "AFF10\r\n\000"
    /* segments follow */
};


/* The header of each segment */
#define AF_SEGHEAD "AFF\000"
struct af_segment_head {
    char magic[4];			// "AFF\000"
    unsigned long name_len:32;		// length of segment name
    unsigned long data_len:32;		// length of segment data, if any
    unsigned long flag:32;		// argument for name;
    /* name follows, then data */
};

/* The tail of each segment */
#define AF_SEGTAIL "ATT\000"
struct af_segment_tail {
    char magic[4];			// "ATT\000"
    unsigned long segment_len:32;      // includes head, tail, name & length
};


/* How 64-bit values are stored in a segment */
struct aff_quad {
    unsigned long low:32;
    unsigned long high:32;
};


/* As it is kept in memory */
struct af_toc_mem {
    char *name;			        // name of this directory entry
    int64 offset;			// offset, stored as an aff_quad; native byte-order
};



void af_initialize();			// initialize the AFFLIB
                                        // automatically called by af_open()
AFFILE *af_open_with(const char *filename,int flags,int mode, struct af_vnode *v);
extern struct af_vnode *af_vnode_array[]; // array of filesystems; last is a "0"




/****************************************************************
 *** Not AFF functions at all, but placed here for convenience.
 ****************************************************************/
 
int	    af_last_seg(AFFILE *af,char *last_segname,int last_segname_len,int64 *pos);
const char *af_hexbuf(char *dst,int dst_len,const unsigned char *bin,int bytes,int format_flag);

/* af_hexbuf formats: */
#define AF_HEXBUF_NO_SPACES 0
#define AF_HEXBUF_SPACE2    0x0001	// space every 2 characters
#define AF_HEXBUF_SPACE4    0x0002	// space every 4 characters
#define AF_HEXBUF_UPPERCASE 0x1000	// uppercase



/* afflib_os.cpp:
 * Operating-system specific code.
 */

/* af_figure_media:
 * Returns information about the media in a structure.
 * Returns 0 if successful, -1 if error.
 */

int	af_figure_media(int fd,struct af_figure_media_buf *);
struct af_figure_media_buf {
    int version;
    int sector_size;
    uint64 total_sectors;
    int max_read_blocks;
};

/****************************************************************
 *** Lowest-level routines for manipulating the AFF File...
 ****************************************************************/

/* Navigating within the AFFILE */
/* probe the next segment.
 * Returns: 0 if success
 *          -1 if error
 *          -2 if segname_len was not large enough to hold segname
 *         - segname - the name of the next segment.
 *         - segsize - number of bytes the entire segment is.
 *      
 * doesn't change af->aseg pointer if do_rewind is true, otherwise leaves stream
 *           positioned ready to read the data
 */

int	af_probe_next_seg(AFFILE *af,char *segname,size_t segname_len,
			   unsigned long *arg,size_t *datasize, size_t *segsize,int do_rewind);
int	af_backspace(AFFILE *af);	// back up one segment



/* find the given segment and return 0 if found, filling in the fields.
 * Leave the file pointer positioned at the start of the segment.
 * Return -1 if segment is not found, and leave pointer at the end
 */
int	af_get_seg(AFFILE *af,const char *name,unsigned long *arg,
		    unsigned char *data,size_t *datalen);

/****************************************************************
 *** Writing functions
 ****************************************************************/


/* Support for data pages. This is what the stream system is built upon.
 * Note: pagename to string translation happens inside afflib.cpp, not inside
 * the vnode driver. 
 */
void	af_read_sizes(AFFILE *af);	// sets up values if we can get them.
int	af_set_pagesize(AFFILE *af,long pagesize); // sets the pagesize; fails with -1 if imagesize >=0
int	af_set_sectorsize(AFFILE *AF,int sectorsize); // fails with -1 if imagesize>=0
int	af_set_maxsize(AFFILE *af,int64 size); // sets maximum AFF file size
int	af_has_pages(AFFILE *af);	// does the underlying system support pages?
int	af_update_page(AFFILE *af,int64 pagenum,unsigned char *data,int datalen);
int	af_page_size(AFFILE *af);	// returns page size, or -1
int	af_get_page_raw(AFFILE *af,int64 pagenum,unsigned long *arg,unsigned char *data,size_t *bytes);
int	af_get_page(AFFILE *af,int64 pagenum,unsigned char *data,size_t *bytes);
int	af_purge(AFFILE *af);		// write buffers to disk

/* afflib_util.cpp
 */
unsigned long long af_decode_q(unsigned char buf[8]); // return buf[8] into an unsigned quad
#ifndef __FreeBSD__
void strlcpy(char *dest,const char *src,int dest_size);
void strlcat(char *dest,const char *src,int dest_size);
#endif


/* afflib_toc.cpp:
 * Table of contents management routines
 * Remember: all of these routines may fail, because the whole TOC may not
 * fit in memory...
 *
 * This is all experimental right now.
 */

int	af_toc_free(AFFILE *af);
void	af_toc_print(AFFILE *af);
void	af_toc_append(AFFILE *af,const char *segname,int64 offset);
int	af_toc_build(AFFILE *af);	// build by scanning the AFFILE
struct af_toc_mem *af_toc(AFFILE *af,const char *segname);
void af_toc_del(AFFILE *af,const char *segname);
void af_toc_insert(AFFILE *af,const char *segname,int64 offset);

#endif
