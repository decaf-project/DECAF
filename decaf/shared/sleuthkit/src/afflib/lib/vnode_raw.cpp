#include "afflib.h"
#include "afflib_i.h"
#include "vnode_raw.h"


#define RAW_PAGESIZE 16*1024*1024	// a good size

/* raw file implementation */
struct raw_private {
    /* For Raw files */
    FILE *raw;				// if it is a raw file
    int raw_popen;			// opened with popen
    uint64 cur_page;			// current page number...
};

static inline struct raw_private *RAW_PRIVATE(AFFILE *af)
{
    assert(af->v == &vnode_raw);
    return (struct raw_private *)(af->vnodeprivate);
}

/* Return 1 if a file is a raw file... */
static int raw_identify_file(const char *filename)
{
    return access(filename,R_OK)==0;	// if we can read it, it's raw...
}


/* Return the size of the raw file */
static int64 raw_filesize(AFFILE *af)
{
    struct raw_private *rp = RAW_PRIVATE(af);

    struct stat sb;
    if(fstat(fileno(rp->raw),&sb)){
	(*af->error_reporter)("raw_open: stat(%s): ",af->fname);
	return -1;			// kind of odd...
    }
    return sb.st_size;
}

static int raw_open(AFFILE *af)
{
    /* Raw is the passthrough system.
     * Right now, it is read only...
     */
    af->vnodeprivate = (void *)calloc(1,sizeof(struct raw_private));
    struct raw_private *rp = RAW_PRIVATE(af);

    if(af->fname) rp->raw=fopen(af->fname,"rb");
    af->image_size = raw_filesize(af);
    af->image_pagesize = RAW_PAGESIZE;
    rp->cur_page = 0;
    return 0;
}

int raw_freopen(AFFILE *af,FILE *file)
{
    af->fname = 0;
    af->vnodeprivate = (void *)calloc(1,sizeof(struct raw_private));
    struct raw_private *rp = RAW_PRIVATE(af);
    rp->raw = file;
    af->image_size = raw_filesize(af);
    af->image_pagesize = RAW_PAGESIZE;
    rp->cur_page = 0;
    return 0;
}

int raw_popen(AFFILE *af,const char *command,const char *type)
{

    if(strcmp(type,"r")!=0){
	(*af->error_reporter)("af_popen: only type 'r' supported");
	return -1;
    }
    af->fname = 0;
    af->vnodeprivate = (void *)calloc(1,sizeof(struct raw_private));
    struct raw_private *rp = RAW_PRIVATE(af);
    rp->raw = popen(command,"r");
    rp->raw_popen = 1;
    return 0;
}


static int raw_close(AFFILE *af)
{
    struct raw_private *rp = RAW_PRIVATE(af);

    if(rp->raw_popen){
	pclose(rp->raw);
    }
    else {
	fclose(rp->raw);
    }
    memset(rp,0,sizeof(*rp));		// clean object reuse
    free(rp);				// won't need it again
    return 0;
}

static int raw_get_seg(AFFILE *af,const char *name,
		       unsigned long *arg,unsigned char *data,size_t *datalen)
{
    struct raw_private *rp = RAW_PRIVATE(af);

    /* Right now, this only supports page segments */
    int64 segnum = af_segname_page_number(name);
    if(segnum<0) return -1;		// unknown segment number

    fflush(rp->raw);			// make sure that any buffers are flushed

    int64 pos = (int64)segnum * af->image_pagesize; // where we are to start reading
    int64 bytes_left = af->image_size - pos;	// how many bytes left in the file

    int bytes_to_read = af->image_pagesize; // copy this many bytes, unless
    if(bytes_to_read > bytes_left) bytes_to_read = bytes_left; // only this much is left
    
    if(arg) *arg = 0;			// arg is always 0
    if(datalen){
	if(*datalen==0 && data==0){ // asked for 0 bytes, so give the actual size
	    *datalen = bytes_to_read;
	    return 0;
	}
	if(*datalen < (unsigned)bytes_to_read){
	    *datalen = bytes_to_read;
	    return AF_ERROR_DATASMALL;
	}
    }
    if(data){
	fseeko(rp->raw,pos,SEEK_SET);    
	int bytes_read = fread(data,1,bytes_to_read,rp->raw);
	if(bytes_read==bytes_to_read){
	    if(datalen) *datalen = bytes_read;
	    return 0;
	}
	return -1;			// some kind of EOF?
    }
    return 0;				// no problems!
}


int raw_update_seg(AFFILE *af, const char *name,
		    unsigned long arg,const void *value,unsigned int vallen,int append)
{
    struct raw_private *rp = RAW_PRIVATE(af);

    /* Simple implementation; only updates data segments */
    int64 pagenum = af_segname_page_number(name);
    if(pagenum<0){
	errno = ENOTSUP;
	return -1;			// not a segment number
    }
    int64 pos = pagenum * af->image_pagesize; // where we are to start reading
    fseeko(rp->raw,pos,SEEK_SET);

    if(fwrite(value,vallen,1,rp->raw)==1){
	return 0;
    }
    return -1;				// some kind of error...
}


static int raw_vstat(AFFILE *af,struct af_vnode_info *vni)
{
    struct raw_private *rp = RAW_PRIVATE(af);

    vni->imagesize            = -1;
    vni->pagesize	      = RAW_PAGESIZE;	// decent page size
    vni->supports_metadata    = 0;
    vni->changable_pagesize   = 1;	// change it at any time
    vni->changable_sectorsize = 1;	// change it at any time

    /* If we can stat the file, use that. */
    fflush(rp->raw);
    struct stat sb;
    if(fstat(fileno(rp->raw),&sb)==0){
	if(sb.st_mode & S_IFREG){	// only do this for regular files
	    vni->imagesize = sb.st_size;
	}
    }

    if(vni->imagesize==-1){
	/* See if this is a device that we can figure */
	struct af_figure_media_buf afb;
	if(af_figure_media(fileno(rp->raw),&afb)==0){
	    if(afb.total_sectors>0 && afb.sector_size>0){
		vni->imagesize = afb.total_sectors * afb.sector_size;
	    }
	}
    }
    vni->supports_compression = 0;
    vni->has_pages = 1;

    if(rp->raw_popen){
	/* popen files require special handling */
	vni->has_pages = 0;
	vni->use_eof   = 1;
	vni->at_eof    = feof(rp->raw);	// are we there yet?
    }
    return 0;
}

static int raw_rewind_seg(AFFILE *af)
{
    struct raw_private *rp = RAW_PRIVATE(af);
    
    rp->cur_page = 0;
    return 0;
}


static int raw_get_next_seg(AFFILE *af,char *segname,size_t segname_len,unsigned long *arg,
			unsigned char *data,size_t *datalen_)
{
    struct raw_private *rp = RAW_PRIVATE(af);
    char pagename[AF_MAX_NAME_LEN];		//
    
    /* See if we are at the end of the "virtual" segment list */
    if((rp->cur_page)*af->image_pagesize >= af->image_size) return -1;

    /* Make the segment name */
    memset(pagename,0,sizeof(pagename));
    snprintf(pagename,sizeof(pagename),AF_PAGE,rp->cur_page++);

    /* Get the segment, if we can */
    int r = raw_get_seg(af,pagename,arg,data,datalen_);

    /* If r==0 and there is room for copying in the segment name, return it */
    if(r==0){
	if(strlen(pagename)+1 < segname_len){
	    strcpy(segname,pagename);
	    return 0;
	}
	/* segname wasn't big enough */
	return -2;
    }
    return r;				// some other error
}

static int raw_read(AFFILE *af, unsigned char *buf, uint64 pos,int count)
{
    struct raw_private *rp = RAW_PRIVATE(af);
    fseek(rp->raw,pos,SEEK_SET);
    return fread(buf,1,count,rp->raw);
}

static int raw_write(AFFILE *af, unsigned char *buf, uint64 pos,int count)
{
    struct raw_private *rp = RAW_PRIVATE(af);
    fseek(rp->raw,pos,SEEK_SET);
    return fwrite(buf,1,count,rp->raw);
}



struct af_vnode vnode_raw = {
    AF_IDENTIFY_RAW,
    AF_VNODE_TYPE_PRIMITIVE,
    "Raw",
    raw_identify_file,
    raw_open,
    raw_close,
    raw_vstat,
    raw_get_seg,			// get seg
    raw_get_next_seg,			// get_next_seg
    raw_rewind_seg,			// rewind_seg
    raw_update_seg,			// update_seg
    0,					// del_seg
    raw_read,				// read
    raw_write				// write
};

