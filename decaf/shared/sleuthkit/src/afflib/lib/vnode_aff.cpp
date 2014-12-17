/*
 * vnode_aff.cpp:
 * 
 * Functions for the manipulation of AFF files...
 */

#include "afflib.h"
#include "afflib_i.h"
#include "vnode_aff.h"
#include "aff_db.h"

#define xstr(s) str(s)
#define str(s) #s



/* Low-level functions for putting
 * af_put_segv --- put a segment with an argument (also useful for putting numbers)
 * af_put_segq --- put a quadword, useful for putting 8-byte values.
 */

static int	aff_ignore_overhead();	
static int      aff_write_ignore(AFFILE *af,unsigned long bytes);
static int	aff_write_seg(AFFILE *af,const char *name,unsigned long arg, const void *value,unsigned int vallen); 
static int	af_make_badflag(AFFILE *af);	// creates a badflag and puts it
static int	aff_get_seg(AFFILE *af,const char *name,unsigned long *arg,unsigned char *data,size_t *datalen);
static int	aff_get_next_seg(AFFILE *af,char *segname,size_t segname_len,
				 unsigned long *arg, unsigned char *data, size_t *datalen);

/* af_write_ignore:
 * write an AF_IGNORE structure of a given size.
 */

static int aff_ignore_overhead()
{
    return sizeof(struct af_segment_head)+sizeof(struct af_segment_tail);
}

static int aff_write_ignore(AFFILE *af,unsigned long bytes)
{
    unsigned char *invalidate_data = (unsigned char *)calloc(bytes,1);

    aff_write_seg(af,AF_IGNORE,0,invalidate_data,bytes); // overwrite with NULLs
    free(invalidate_data);
    return(0);
}


/* aff_write_seg:
 * put the given named segment at the current position in the file.
 * Return 0 for success, -1 for failure (probably disk full?)
 * This is the only place where a segment actually gets written
 */

int aff_write_seg(AFFILE *af, const char *segname,unsigned long arg,const void *data,unsigned int datalen)
{
    //printf("aff_write_seg(%s) af->aseg=%p\n",segname,af->aseg);

    struct af_segment_head segh;
    struct af_segment_tail segt;

    if(af->debug){
      (*af->error_reporter)("aff_write_seg(" POINTER_FMT ",'%s',%lu,data=" POINTER_FMT ",datalen=%u)",
			    af,segname,arg,data,datalen);
    }

    assert(sizeof(segh)==16);
    assert(sizeof(segt)==8);

    /* If the last command was not a probe (so we know where we are), and
     * we are not at the end of the file, something is very wrong.
     */

    unsigned int segname_len = strlen(segname);

    strcpy(segh.magic,AF_SEGHEAD);
    segh.name_len = htonl(segname_len);
    segh.data_len = htonl(datalen);
    segh.flag      = htonl(arg);

    strcpy(segt.magic,AF_SEGTAIL);
    segt.segment_len = htonl(sizeof(segh)+segname_len + datalen + sizeof(segt));
    
    int64 offset = ftello(af->aseg);
    
    af_toc_insert(af,segname,offset);

#ifdef DEBUG   
    {
	fprintf(stderr,"aff_write_segv: putting segment %s (datalen=%d) offset=%qd\n",
		segname,datalen,o);
    }
#endif

    if(fwrite(&segh,sizeof(segh),1,af->aseg)!=1) return -10;
    if(fwrite(segname,1,segname_len,af->aseg)!=segname_len) return -11;
    if(fwrite(data,1,datalen,af->aseg)!=datalen) return -12;
    if(fwrite(&segt,sizeof(segt),1,af->aseg)!=1) return -13;
    fflush(af->aseg);			// make sure it is on the disk
    return 0;
}


/****************************************************************
 *** low-level routines for reading 
 ****************************************************************/

/* aff_get_segment:
 * Get the named segment, using the toc cache if it is present.
 * If there is no cache, 
 *  -  If the requested segment is the next segment, just get it.
 *  -  Otherwise go to the beginning and scan for it.
 */

static int aff_get_seg(AFFILE *af,const char *name,
		       unsigned long *arg,unsigned char *data,size_t *datalen)
{
    char next[AF_MAX_NAME_LEN];
    int first = 1;
    size_t segsize = 0;


    /* If the segment is in the directory, then seek the file to that location.
     * Otherwise, we'll probe the next segment, and if it is not there,
     * we will rewind to the beginning and go to the end.
     */
    struct af_toc_mem *adm = af_toc(af,name);

    if(adm){
	fseeko(af->aseg,adm->offset,SEEK_SET);
    }
    do {
	int r = af_probe_next_seg(af,next,sizeof(next),0,0,&segsize,1);
	if(r==0 && strcmp(next,name)==0){	// found the segment!
	    int ret = af_get_next_seg(af,next,sizeof(next),arg,data,datalen);
	    assert(strcmp(next,name)==0);	// hopefully it's still the same!
	    return ret;
	}
	if(first){			// okay, should we try to rewind?
	    af_rewind_seg(af);
	    first = 0;			// no longer the first time through
	    continue;
	}
	if(r!=0){			// did we encounter an error reading?
	    break;			// yes
	}
	fseeko(af->aseg,segsize,SEEK_CUR); // seek to next segment
    } while(1);
    return -1;				// couldn't find segment
}



/****************************************************************
 *** Get the next segment.
 *** data must not be freed.
 *** *datalen_ is size of data; *datalen_ is modified to amount of segment
 *** if no data is specified, then just seek past it...
 *** Returns 0 on success, 
 *** -1 on end of file. (AF_ERROR_EOF)
 *** -2 if *data is not large enough to hold the segment (AF_ERROR_DATASMALL)
 *** -3 af file is corrupt; no tail (AF_ERROR_CORRUPT)
 ****************************************************************/
static int aff_get_next_seg(AFFILE *af,char *segname,size_t segname_len,unsigned long *arg,
			unsigned char *data,size_t *datalen_)
{
    if(!af->aseg){
	snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv only works with aff files");
	return AF_ERROR_INVALID_ARG;
    }

    int64 start = ftello(af->aseg);
    size_t data_len;

    int r = af_probe_next_seg(af,segname,segname_len,arg,&data_len,0,0);
    if(r<0) return r;			// propigate error code
    if(data){				/* Read the data? */
	if(datalen_ == 0){
	    snprintf(af->error_str,sizeof(af->error_str),"af_get_next_seg: data provided but datalen is NULL");
	    return AF_ERROR_INVALID_ARG;
	}
	size_t read_size = data_len<=*datalen_ ? data_len : *datalen_;

	if(fread(data,1,read_size,af->aseg)!=read_size){
	    snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv: EOF on reading segment? File is corrupt.");
	    return AF_ERROR_SEGH;
	}
	if(data_len > *datalen_){
	    /* Read was incomplete;
	     * go back to the beginning of the segment and return
	     * the incomplete code.
	     */
	    fseeko(af->aseg,start,SEEK_SET);	// go back
	    return AF_ERROR_DATASMALL;
	}
    } else {
	fseeko(af->aseg,data_len,SEEK_CUR); // skip past the data
    }
    if(datalen_) *datalen_ = data_len;

    /* Now read the tail */
    struct af_segment_tail segt;
    if(fread(&segt,sizeof(segt),1,af->aseg)!=1){
	snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv: end of file reading segment tail...");
	return AF_ERROR_TAIL;
    }
    /* Validate tail */
    unsigned long stl = ntohl(segt.segment_len);
    unsigned long calculated_segment_len =
	sizeof(struct af_segment_head)
	+ strlen(segname)
	+ data_len + sizeof(struct af_segment_tail);

    if(strcmp(segt.magic,AF_SEGTAIL)!=0){
	snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv: AF file corrupt. No tail!");
	return AF_ERROR_TAIL;
    }
    if(stl != calculated_segment_len){
	snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv: AF file corrupt (%lu!=%lu)!",
		 stl,calculated_segment_len);
	return AF_ERROR_TAIL;
    }
    return 0;
}


static int aff_rewind_seg(AFFILE *af)
{
    fseeko(af->aseg,sizeof(struct af_head),SEEK_SET); // go to the beginning
    return 0;
}





/****************************************************************
 *** Update functions
 ****************************************************************/

/*
 * af_update_seg:
 * Update the given named segment with the new value.
 * if "append" is false, don't append if it can't be found. 
 * if "append" is true and the named segment can't be found, append it.
 */

int aff_update_seg(AFFILE *af, const char *name,
		    unsigned long arg,const void *value,unsigned int vallen,int append)
{
    char   next_segment_name[AF_MAX_NAME_LEN];
    size_t next_segsize = 0;
    size_t next_datasize = 0;

    /* if we are updating with a different size,
     * remember the location and size of the AF_IGNORE segment that
     * has the smallest size that is >= strlen(name)+vallen
     */
    size_t size_needed = strlen(name)+vallen+aff_ignore_overhead();
    size_t size_closest = 0;
    uint64         loc_closest = 0;
    struct af_toc_mem *adm = af_toc(af,name);
       
#ifdef DEBUG
    fprintf(stderr,"aff_update_seg(name=%s,arg=%lu,vallen=%u,append=%d)\n",name,arg,vallen,append);
#endif

    /* Is the last segment an ignore? */

    if(adm){
	fseeko(af->aseg,adm->offset,SEEK_SET);
    }
    else {
	af_rewind_seg(af);			// start at the beginning
    }
    while(af_probe_next_seg(af,next_segment_name,sizeof(next_segment_name),
			    0,&next_datasize,&next_segsize,1)==0){
	uint64 next_segment_loc = ftello(af->aseg);

#ifdef DEBUG
	fprintf(stderr,"  next_segment_name=%s next_datasize=%d "
		"next_segsize=%d next_segment_loc=%qd\n",
		next_segment_name,
		next_datasize,
		next_segsize,next_segment_loc);
#endif

	if(strcmp(next_segment_name,name)==0){	// found the segment!

	    /* Okay. Is there room to fit it here? */
	    if(next_datasize == vallen){
		/* We can just write in place! */
		int r = aff_write_seg(af,name,arg,value,vallen);
		return r;
	    }

	    /* Invalidate this segment and continue scanning */
	    
	    uint64 loc_invalidated = next_segment_loc; // remember where this is

	    aff_write_ignore(af,next_datasize+strlen(name));

	    uint64 loc_before_seek = ftello(af->aseg); // before the seek

	    fseeko(af->aseg,(uint64)0,SEEK_END);              // go to the end of the file

	    uint64 pos_eof = ftello(af->aseg);


	    if(pos_eof == loc_before_seek){
		/* Apparently I didn't move at all with the seek.
		 * If the segment being written is larger than the segment being replaced,
		 * we can just backspace.
		 */
		if(next_segsize < vallen){
		    //fprintf(stderr,"  *** seeking to %qd\n",loc_invalidated);
		    fseeko(af->aseg,loc_invalidated,SEEK_SET);
		}
	    }

	    append = 1;			// be sure to add the data to the end!
	    break;			// and exit this loop
	}

	/* If this is an AF_IGNORE, see if it is a close match */
	if(next_segment_name[0]==AF_IGNORE[0]){
	    if(next_datasize>=size_needed && ((next_datasize<size_closest || size_closest==0))){
		size_closest = next_datasize;
		loc_closest  = next_segment_loc;
	    }
	}
	fseeko(af->aseg,next_segsize,SEEK_CUR); // skip this segment
    }
    if(append){				// if appending, just write to the end
	/* See if there is a place where this would fit. */
	//fprintf(stderr,"size_closest=%d\n",size_closest);
	if(size_closest>0){
	    /* Yes. Put it here and put a new AF_IGNORE in the space left-over
	     * TODO: If the following space is also an AF_IGNORE, then combine the two.
	     */
#ifdef DEBUG
	    fprintf(stderr,"size_closest=%d\n",size_closest);
	    fprintf(stderr,"loc_closest=%qd\n",loc_closest);
	    fprintf(stderr,"*** Squeezing it in at %qd. name=%s. vallen=%d\n",loc_closest,name,vallen);
#endif

	    fseeko(af->aseg,loc_closest,SEEK_SET); // move to the location
	    aff_write_seg(af,name,arg,value,vallen); // write the new segment

	    int newsize = size_closest - vallen - aff_ignore_overhead() - strlen(name);

	    aff_write_ignore(af,newsize); // write the smaller ignore
	    return 0;
	}
	return aff_write_seg(af,name,arg,value,vallen);
    }
    return -2;				// couldn't find segment
}



/* Delete the first occurance of the named segment.
 * Special case code: See if the segment being deleted
 * is the last segment. If it is, truncate the file...
 * This handles the case of AF_DIRECTORY and possibly other cases
 * as well...
 */

static int aff_del_seg(AFFILE *af,const char *segname)
{
    char last_segname[AF_MAX_NAME_LEN];
    int64 last_pos;

    af_toc_del(af,segname);		// remove it from the directory

    /* Find out if the last segment is the one we are deleting;
     * If so, we can just truncate the file.
     */
    af_last_seg(af,last_segname,sizeof(last_segname),&last_pos);
    if(strcmp(segname,last_segname)==0){
	fflush(af->aseg);		// flush any ouput
	ftruncate(fileno(af->aseg),last_pos); // make the file shorter
	return 0;
    }

    size_t datasize=0,segsize=0;
    if(aff_find_seg(af,segname,0,&datasize,&segsize)!=0){
	return -1;			// nothing to delete?
    }
    /* Now wipe it out */
    size_t ignore_size = datasize+strlen(segname);
    aff_write_ignore(af,ignore_size);
    return 0;
}



/* 
 * af_make_badflag:
 * Create a randomized bag flag and
 * leave an empty segment of how many badsectors there are
 * in the image...
 */
int af_make_badflag(AFFILE *af)
{
    RAND_pseudo_bytes(af->badflag,af->image_sectorsize);
    strcpy((char *)af->badflag,"BAD SECTOR");

    if(af_update_seg(af,AF_BADFLAG,0,af->badflag,af->image_sectorsize,1)){
	return -1;
    }
    if(af_update_segq(af,AF_BADSECTORS,0,1)){
	return -1;
    }
    return 0;
}




/* aff_create:
 * af is an empty file that is being set up.
 */
static int aff_create(AFFILE *af)
{
    fwrite(AF_HEADER,1,8,af->aseg);  // writes the header 
    af_toc_build(af);	             // build the toc (will be pretty small)
    af_make_badflag(af);	     // writes the flag for bad blocks
    
    char *version = xstr(AFFLIB_VERSION);
    af_update_seg(af,AF_AFFLIB_VERSION,0,version,strlen(version),1);
    
#ifdef HAS_GETPROGNAME
    const char *progname = getprogname();
    if(af_update_seg(af,AF_CREATOR,0,progname,strlen(progname),1)) return -1;
#endif
    if(af_update_seg(af,AF_AFF_FILE_TYPE,0,"AFF",3,1)) return -1;

    return 0;
}




/****************************************************************
 *** User-visible functions.
 ****************************************************************/

static int aff_open(AFFILE *af)
{
    int fd = open(af->fname,af->openflags | O_BINARY,af->openmode);
    if(fd<0){				// couldn't open
	return -1;
    }
    af->compression_type     = AF_COMPRESSION_ZLIB; // default
    af->compression_level    = Z_DEFAULT_COMPRESSION;

    /* Open the FILE  for the AFFILE */
    char strflag[8];
    strcpy(strflag,"r");		// we have to be able to read
    if(af->openflags & O_RDWR) 	strcpy(strflag,"w+"); 

    af->aseg = fdopen(fd,strflag);
    if(!af->aseg){
      (*af->error_reporter)("fdopen(%d,%s)",fd,strflag);
      return -1;
    }

    /* Get file size */
    struct stat sb;
    if(fstat(fd,&sb)){
	(*af->error_reporter)("aff_open: fstat(%s): ",af->fname);	// this should not happen
	return -1;
    }

    /* If file is empty, then put out an AFF header, badflag, and AFF version */
    if(sb.st_size==0){
	return aff_create(af);
    }
    
    /* We are opening an existing file. Verify once more than it is an AFF file
     * and skip past the header...
     */

    char buf[8];
    if(fread(buf,sizeof(buf),1,af->aseg)!=1){
	/* Hm. End of file. That shouldn't happen here. */
	(*af->error_reporter)("aff_open: couldn't read AFF header on existing file?");
	return -1;			// should not happen
    }

    if(strcmp(buf,AF_HEADER)!=0){
	buf[7] = 0;
	(*af->error_reporter)("aff_open: %s is not an AFF file (header=%s)\n",af->fname,buf);
	return -1;
    }

    /* File has been validated */
    af_toc_build(af);			// build the TOC
    af_read_sizes(af);			// read all of the relevant sizes
    return 0;				// everything must be okay.
}


static bool last4_is_aff(const char *filename)
{
    if(strlen(filename)>4){
	const char *last4 = filename+strlen(filename)-4;
	if(strcasecmp(last4,".aff")==0) return 1;
    }
    return 0;
}

/* Return 1 if a file is an AFF file */
static int aff_identify_file(const char *filename)
{
    int fd = open(filename,O_RDONLY | O_BINARY);
    if(fd>0){
	int len = strlen(AF_HEADER)+1;	
	char buf[64];
	int r = read(fd,buf,len);
	close(fd);
	if(r==len){			// if I could read the header
	    if(strcmp(buf,AF_HEADER)==0) return 1; // must be an AFF file
	    return 0;			// not an AFF file
	}
	/* If it is a zero-length file and the file extension ends AFF,
	 * then let it be an AFF file...
	 */
	if(r==0 && last4_is_aff(filename)) return 1;
	return 0;			// must not be an aff file
    }
    /* File doesn't exist. Is this an AFF name? */
    if(af_ext_is(filename,"aff")) return 1;
    return 0;
}

/*
 * aff_close:
 * If the imagesize changed, write out a new value.
 */
static int aff_close(AFFILE *af)
{
    fclose(af->aseg);
    return 0;
}


#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/mount.h>
#define HAS_STATFS
#endif

static int aff_vstat(AFFILE *af,struct af_vnode_info *vni)
{
    memset(vni,0,sizeof(*vni));		// clear it
    vni->imagesize = af->image_size;		// we can just return this
    vni->pagesize = af->image_pagesize;
    vni->supports_compression = 1;
    vni->has_pages            = 1;
    vni->supports_metadata    = 1;
    return 0;
}


struct af_vnode vnode_aff = {
    AF_IDENTIFY_AFF,
    AF_VNODE_TYPE_PRIMITIVE,
    "AFF",
    aff_identify_file,
    aff_open,
    aff_close,
    aff_vstat,
    aff_get_seg,
    aff_get_next_seg,
    aff_rewind_seg,
    aff_update_seg,
    aff_del_seg,
    0,				// read; keep 0
    0				// write
};

