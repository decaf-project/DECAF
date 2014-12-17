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

#include "afflib.h"
#include "afflib_i.h"

#include "vnode_raw.h"
#include "vnode_split_raw.h"
#include "vnode_afm.h"
#include "vnode_aff.h"
#include "vnode_afd.h"
#include "vnode_evf.h"
#include "vnode_evd.h"

#include <fcntl.h>
#include <assert.h>
#include <openssl/rand.h>
#include <errno.h>

// vnode implementations.
// order matters
struct af_vnode *af_vnode_array[] = {
    &vnode_afd, 
    &vnode_afm,				// must be before aff
    &vnode_aff,
    &vnode_evd,
    &vnode_evf,
    &vnode_split_raw,			// must be before raw
    &vnode_raw,				// greedy; must be last
    0};



/* Returns the name and offset of the last segment */
int	af_last_seg(AFFILE *af,char *last_segname,int last_segname_len,int64 *pos)
{
    /* Find the name of the last segment */
    fseeko(af->aseg,0,SEEK_END);
    af_backspace(af);			// back up one segment
    *pos = ftello(af->aseg);	// remember where it is
    last_segname[0] = 0;
    return af_probe_next_seg(af,last_segname,last_segname_len,0,0,0,0);
}


/****************************************************************
 *** GET FUNCTIONS
 ****************************************************************/

/****************************************************************
 *** Probe the next segment:
 *** Return its name and argument, but don't advance the pointer....
 *** Returns 0 on success, -1 on end of file or other error.
 ****************************************************************/

/****************************************************************
 *** AFF creation functions
 ****************************************************************/

static int aff_initialized = 0;

void af_initialize()
{
    if(aff_initialized) return;

    /* make sure things were compiled properly */
    assert(sizeof(struct af_head)==8);
    assert(sizeof(struct af_segment_head)==16);
    assert(sizeof(struct af_segment_tail)==8);
    aff_initialized = 1;
}




/* Return 1 if a file is probably an AFF file
 * 0 if it is not.
 * -1 if failure.
 */

int af_identify_type(const char *filename)
{
    if(access(filename,R_OK)!=0) return AF_IDENTIFY_NOEXIST;
    for(int i = 0; af_vnode_array[i]; i++){
	if( (*af_vnode_array[i]->identify)(filename)==1 ){
	    return (af_vnode_array[i]->type);
	}
    }
    return AF_IDENTIFY_ERR;
}

const char *af_identify_name(const char *filename)
{
    for(int i = 0; af_vnode_array[i]; i++){
	if( (*af_vnode_array[i]->identify)(filename)==1 ){
	    return (af_vnode_array[i]->name);
	}
    }
    return 0;
}

AFFILE *af_open_with(const char *filename,int flags,int mode, struct af_vnode *v)
{
    /* Alloate the space for the AFFILE structure */
    AFFILE *af = (AFFILE *)calloc(sizeof(AFFILE),1);
    af->v	  = v;
    af->version   = 2;
    af->openflags = flags;
    af->fname	  = strdup(filename);	                // remember file name
    af->exists    = (access(filename,R_OK) == 0);	// does the file exist?
    af->openmode  = mode;
    af->pagenum   = -1;
    af->image_sectorsize = 512;		// default size
    af->badflag   = (unsigned char *)malloc(af->image_sectorsize);
    af->error_reporter = warnx;

    /* Try opening it! */
    if((*af->v->open)(af)){		
	/* Got an error; Free what was allocated and return */
	free(af->fname);
	free(af);
	return 0;
    }
    return af;
}


AFFILE *af_open(const char *filename,int flags,int mode)
{
    if(!aff_initialized) af_initialize();
    
    if(flags & O_WRONLY){
	errno = EINVAL;
	return 0;			// this flag not supported
    }

    /* Figure out it's format, then hand off to the correct subsystem. */
    for(int i = 0; af_vnode_array[i]; i++){
	/* Check to see if the implementation identifies the file */
	if( (*af_vnode_array[i]->identify)(filename)==1 ){
	    return af_open_with(filename,flags,mode,af_vnode_array[i]);
	}
    }
    return 0;				// can't figure it out.
}

/* Open a regular file as an affile.
 * Can only be a raw file...
 */
AFFILE *af_freopen(FILE *file)
{
    if(!aff_initialized) af_initialize();

    AFFILE *af = (AFFILE *)calloc(sizeof(AFFILE),1);
    af->v = &vnode_raw;
    af->image_sectorsize = 512;		// default
    raw_freopen(af,file);
    return af;
}

/* Open a regular file as an affile */
AFFILE *af_popen(const char *command,const char *type)
{
    if(!aff_initialized) af_initialize();
    AFFILE *af = (AFFILE *)calloc(sizeof(AFFILE),1);
    af->v   = &vnode_raw;
    raw_popen(af,command,type);
    af->image_sectorsize = 512;		// default
    af->openflags = O_RDONLY;
    af->fname     = strdup(command);
    return af;
}


/* Close the image */
int af_close(AFFILE *af)
{
    int ret = 0;

    if(af->writing){
	ret = af_purge(af);			// empty any buffers in memory
    }

    if(af->writing && af->image_size != af->image_size_in_file){
	af_update_segq(af,AF_IMAGESIZE,(int64)af->image_size,1);
	af->image_size_in_file = af->image_size;
    }
    (*af->v->close)(af);
    if(af->pagebuf){
	memset(af->pagebuf,0,af->image_pagesize); // clean object reuse
	free(af->pagebuf);
    }
    if(af->fname) free(af->fname);
    if(af->badflag) free(af->badflag);
    memset(af,0,sizeof(*af));		// clean object reuse
    free(af);
    return ret;
}


/* Seek in the virtual file */
uint64 af_seek(AFFILE *af,uint64 pos,int whence)
{
    if(af->logfile) fprintf(af->logfile,"af_seek(%p,%" I64d",%d)\n",af,pos,whence);

    uint64 new_pos=0;
    switch(whence){
    case SEEK_SET:
	new_pos = pos;
	break;
    case SEEK_CUR:
	new_pos += pos;
	break;
    case SEEK_END:
	new_pos = af_imagesize(af) - pos;
	break;
    }
    if(new_pos < 0) new_pos = 0;	   // new change
    if(af->pos == new_pos) return af->pos; // no change
    af->pos = new_pos;			// set the new position

    return af->pos;
}

uint64  af_tell(AFFILE *af)
{
    return af->pos;
}

/* Return if we are at the end of the file */
int af_eof(AFFILE *af)
{
    af_vnode_info vni;

    if(af_vstat(af,&vni)) return -1;	// that's bad
    if(vni.use_eof) return vni.at_eof;	// if implementation wants to use it, use it
    if(af->pos<0){
	errno = EINVAL;
	return -1;		// this is bad
    }
    return (int64)af->pos >= af_imagesize(af);
}

void af_enable_writing(AFFILE *af,int flag)
{
    af->writing = flag;
}

void af_set_callback(AFFILE *af,void (*wcb)(struct affcallback_info *))
{
    af->w_callback	  = wcb;
}


void af_enable_compression(AFFILE *af,int type,int level)
{
    af->compression_type  = type; 
    af->compression_level = level;
}

int	af_compression_type(AFFILE *af)
{
    return af->compression_type;
}


/* Return the 'extension' of str.
 * af_str("filename.aff") = ".aff"
 */
const char *af_ext(const char *str)
{
    int len = strlen(str);
    if(len==0) return str;		// no extension
    for(int i=len-1;i>0;i--){
	if(str[i]=='.') return &str[i+1];
    }
    return str;
}

int af_ext_is(const char *filename,const char *ext)
{
    return strcasecmp(af_ext(filename),ext)==0;
}

const char *af_filename(AFFILE *af)
{
    return af->fname;
}

int	af_identify(AFFILE *af)
{
    return af->v->type;
}

/* af_imagesize:
 * Return the byte # of last mapped byte in image, or size of device;
 */
int64       af_imagesize(AFFILE *af)
{
    struct af_vnode_info vni;
    if(af_vstat(af,&vni)==0){
	return vni.imagesize;
    }
    return -1;
}

int af_get_seg(AFFILE *af,const char *name,unsigned long *arg,unsigned char *data,size_t *datalen)
{
    if(af->v->get_seg==0){
	errno = ENOTSUP;
	return -1;	// not supported by this file system
    }
    return (*af->v->get_seg)(af,name,arg,data,datalen);
}

int af_get_next_seg(AFFILE *af,char *segname,size_t segname_len,unsigned long *arg,
			unsigned char *data,size_t *datalen_)
{
    if(af->v->get_next_seg==0){
	errno = ENOTSUP;
	return -1;
    }
    return (*af->v->get_next_seg)(af,segname,segname_len,arg,data,datalen_);
}

int af_rewind_seg(AFFILE *af)
{
    if(af->v->rewind_seg==0){
	errno = ENOTSUP;
	return -1;
    }
    return (*af->v->rewind_seg)(af);
}

int af_update_seg(AFFILE *af, const char *name,
		  unsigned long arg,const void *value,unsigned int vallen,int append)
{
    if(af->v->update_seg==0){
	errno = ENOTSUP;
	return -1;	// not supported by this file system
    }
    return (*af->v->update_seg)(af,name,arg,value,vallen,append);
}


int af_del_seg(AFFILE *af,const char *name)
{
    if(af->v->del_seg==0){
	errno = ENOTSUP;
	return -1;	// not supported
    }
    return (*af->v->del_seg)(af,name);
}

int af_vstat(AFFILE *af,struct af_vnode_info *vni)
{
    memset(vni,0,sizeof(*vni));		// clear it
    if(af->v->vstat==0){
	errno = ENOTSUP;
	return -1;	// not supported
    }
    return (*af->v->vstat)(af,vni);
}

int af_has_pages(AFFILE *af)
{
    struct af_vnode_info vni;
    if(af_vstat(af,&vni)) return -1;	// can't figure it out
    return vni.has_pages;		// will return 0 or 1
}


#ifdef NO_WARNX
#include <stdarg.h>
void warnx(const char *fmt,...)
{
  va_list ap;
  va_start(ap,fmt);
  vfprintf(stderr,fmt,ap);
  va_end(ap);
}

void err(int eval,const char *fmt,...)
{
  va_list ap;
  va_start(ap,fmt);
  vfprintf(stderr,fmt,ap);
  fprintf(stderr,": %s\n",strerror(errno));
  va_end(ap);
}
#endif




