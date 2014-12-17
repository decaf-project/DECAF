/*
 * vnode_aff.cpp:
 * 
 * Functions for the manipulation of AFF files...
 */

#include "afflib.h"
#include "afflib_i.h"
#include "vnode_afd.h"
#include "aff_db.h"

/****************************************************************
 *** Service routines
 ****************************************************************/

struct afd_private {
    AFFILE **afs;			// list of AFFILEs...
    int num_afs;			// number of them
    int cur_file;			// current segment number...
};

static inline struct afd_private *AFD_PRIVATE(AFFILE *af)
{
    assert(af->v == &vnode_afd);
    return (struct afd_private *)(af->vnodeprivate);
}


static bool last4_is_afd(const char *filename)
{

    return af_ext_is(filename,"afd");
}


static bool last4_is_aff(const char *filename)
{
    return af_ext_is(filename,"aff");
}


/* afd_file_with_seg:
 * Returns the AFFILE for a given segment, or 0 if it isn't found.
 */

static AFFILE *afd_file_with_seg(AFFILE *af,const char *name)
{
    struct afd_private *ap = AFD_PRIVATE(af);

    for(int i=0;i<ap->num_afs;i++){
	if(af_get_seg(ap->afs[i],name,0,0,0)==0){
	    return ap->afs[i];
	}
    }
    errno = ENOTDIR;			// get ready for error return
    return 0;
}

static void aff_filename(AFFILE *afd,char *buf,int buflen,int num)
{
    snprintf(buf,buflen,"%s/file_%03d.aff",afd->fname,num);
}

/* Return 1 if a file is an AFF file */
static int afd_identify_file(const char *fn_)
{
    if(fn_==0 || strlen(fn_)==0) return 0;	// zero-length filenames aren't welcome

    /* If it ends with a '/', remove it */
    char *fn = (char *)alloca(strlen(fn_)+1);
    strcpy(fn,fn_);
    char *lastc = fn + strlen(fn) - 1;
    if(*lastc=='/') *lastc = '\000';


    /* If filename exists and it is a dir, it needs to end afd */
    struct stat sb;
    if(stat(fn,&sb)==0){
	if((sb.st_mode & S_IFMT)==S_IFDIR){
	    if(last4_is_afd(fn)) return 1;
	}
	return 0;			//
    }
    /* Doesn't exist. Does it end .afd ? */
    if(last4_is_afd(fn)) return 1;
    return 0;
}

/* Add a file to the AFF system.
 * if fname==0, create a new one and copy over the relevant metadata...
 */
static int afd_add_file(AFFILE *af,const char *fname_)
{
    struct afd_private *ap = AFD_PRIVATE(af);
    const char *segs_to_copy[] = {AF_BADFLAG,
				  AF_CASE_NUM,
				  AF_IMAGE_GID,
				  AF_ACQUISITION_ISO_COUNTRY,
				  AF_ACQUISITION_COMMAND_LINE,
				  AF_ACQUISITION_DATE,
				  AF_ACQUISITION_NOTES,
				  AF_ACQUISITION_DEVICE,
				  AF_ACQUISITION_TECHNICIAN,
				  AF_DEVICE_MANUFACTURER,
				  AF_DEVICE_MODEL,
				  AF_DEVICE_SN,
				  AF_DEVICE_FIRMWARE,
				  AF_DEVICE_SOURCE,
				  AF_CYLINDERS,
				  AF_HEADS,
				  AF_SECTORS_PER_TRACK,
				  AF_LBA_SIZE,
				  AF_HPA_PRESENT,
				  AF_DCO_PRESENT,
				  AF_LOCATION_IN_COMPUTER,
				  AF_DEVICE_CAPABILITIES,
				  0};

    char fname[MAXPATHLEN+1];
    memset(fname,0,sizeof(fname));
    if(fname_){
	strlcpy(fname,fname_,sizeof(fname));
    }
    else {
	aff_filename(af,fname,sizeof(fname),ap->num_afs);
    }

    int new_file = access(fname,F_OK)!=0;	// Is this a new file?

    AFFILE *af2 = af_open(fname,af->openflags,af->openmode); 
    if(af2==0){
	(*af->error_reporter)("open(%s,%d,%d) failed: %s\n",
			      fname,af->openflags,af->openmode,strerror(errno));
	return -1;			// this is bad
    }

    if(new_file){
	if(af->writing==0){
	    (af->error_reporter)("afd_add_file: created new file and not writing?\n");
	    return -1;
	}
	/* Copy over configuration from AFD vnode*/
	af_enable_writing(af2,af->writing);
	af_enable_compression(af2,af->compression_type,af->compression_level);
	af_set_pagesize(af2,af->image_pagesize);		//
	af_update_seg(af,AF_AFF_FILE_TYPE,0,"AFD",3,1);
	
	/* If this is the second file, copy over additional metadata from first... */
	if(ap->num_afs>0){
	    AFFILE *af0 = ap->afs[0];
	    memcpy(af2->badflag,af0->badflag,af->image_sectorsize);
	    af2->bytes_memcpy += af->image_sectorsize;
	    
	    for(const char **segname=segs_to_copy;*segname;segname++){
		unsigned char data[65536];	// big enough for most metadata
		size_t datalen = sizeof(data);
		unsigned long arg=0;
		
		if(af_get_seg(af0,*segname,&arg,data,&datalen)==0){
		    int r = af_update_seg(af2,*segname,arg,data,datalen,1);
		    if(r!=0){
			(*af->error_reporter)("afd_add_file: could not update %s in %s (r=%d)",
					      *segname,af_filename(af2),r);
		    }
		}
	    }
	}
    }
    
    ap->num_afs += 1;
    ap->afs = (AFFILE **)realloc(ap->afs,sizeof(AFFILE *) * ap->num_afs);
    ap->afs[ap->num_afs-1] = af2;
    return 0;
}
    
	
	    
/****************************************************************
 *** User-visible functions.
 ****************************************************************/

static int afd_open(AFFILE *af)
{
    if(af->fname==0 || strlen(af->fname)==0) return -1;	// zero-length filenames aren't welcome

    /* If the name ends with a '/', remove it */
    char *lastc = af->fname + strlen(af->fname) - 1;
    if(*lastc=='/') *lastc = '\000';
    

    /* If the directory doesn't exist, make it (if we are O_CREAT) */
    struct stat sb;
    if(stat(af->fname,&sb)!=0){
	if((af->openflags & O_CREAT) == 0){ // flag not set
	    errno = ENOTDIR;
	    return -1;
	}
	mode_t omask = umask(0);	// reset umask for now...
	mkdir(af->fname,af->openmode|0111); // make the directory
	umask(omask);			// put back the old mask
    }
    af->maxsize = AFD_DEFAULT_MAXSIZE;
    af->vnodeprivate = (void *)calloc(1,sizeof(struct afd_private));
    struct afd_private *ap = AFD_PRIVATE(af);
    ap->afs = (AFFILE **)malloc(0);

    /* Open the directory and read all of the AFF files */
    DIR *dirp = opendir(af->fname);
    if(!dirp){
	return -1;			// something is wrong...
    }
    struct dirent *dp;
    while ((dp = readdir(dirp)) != NULL){
	if (last4_is_aff(dp->d_name)){
	    char path[MAXPATHLEN+1];
	    strcpy(path,af->fname);
	    strlcat(path,"/",sizeof(path));
	    strlcat(path,dp->d_name,sizeof(path));
	    if(afd_add_file(af,path)){
		return -1;
	    }
	}
    }
    closedir(dirp);
    af_read_sizes(af);			// set up the metadata
    return 0;				// "we were successful"
}


static int afd_close(AFFILE *af)
{
    struct afd_private *ap = AFD_PRIVATE(af);
    uint64 image_size = af_imagesize(af); // get the largest imagesize

    /* Close all of the subfiles, then free the memory, then close this file */
    for(int i=0;i<ap->num_afs;i++){
	ap->afs[i]->image_size = image_size; // set each to have current imagesize
	af_close(ap->afs[i]);		// and close each file
    }
    free(ap->afs);
    memset(ap,0,sizeof(*ap));		// clean object reuse
    free(ap);				// won't need it again
    return 0;
}


static uint64 max(uint64 a,uint64 b)
{
    return a > b ? a : b;
}

static int afd_vstat(AFFILE *af,struct af_vnode_info *vni)
{
    struct afd_private *ap = AFD_PRIVATE(af);
    memset(vni,0,sizeof(*vni));		// clear it

    /* See if there is some device that knows how big the disk is */
    if(ap->num_afs>0){
	af_vstat(ap->afs[0],vni);	// get disk free bytes
    }

    /* Get the file with the largest imagesize from either the
     * AFD or any of the sub AFDs...
     */
    vni->imagesize = af->image_size;
    for(int i=0;i<ap->num_afs;i++){
	vni->imagesize = max(vni->imagesize,ap->afs[i]->image_size);
    }
    vni->has_pages = 1;
    vni->supports_metadata = 1;
    return 0;
}

static int afd_get_seg(AFFILE *af,const char *name,unsigned long *arg,unsigned char *data,
		       size_t *datalen)
{
    AFFILE *af2 = afd_file_with_seg(af,name);
    if(af2){
	return af_get_seg(af2,name,arg,data,datalen); // use this one
    }
    return -1;				// not found
}


static int afd_get_next_seg(AFFILE *af,char *segname,size_t segname_len,unsigned long *arg,
			unsigned char *data,size_t *datalen_)
{
    /* See if there are any more in the current segment */
    struct afd_private *ap = AFD_PRIVATE(af);
    while (ap->cur_file < ap->num_afs) {
	int r = af_get_next_seg(ap->afs[ap->cur_file],segname,segname_len,arg,data,datalen_);
	if(r!=AF_ERROR_EOF){		// if it is not EOF
	    return r;
	}
	ap->cur_file++;			// advance to the next file
	if(ap->cur_file < ap->num_afs){	// rewind it to the beginning
	    af_rewind_seg(ap->afs[ap->cur_file]);
	}
    } while(ap->cur_file < ap->num_afs);
    return AF_ERROR_EOF;		// really made it to the end
}


/* Rewind all of the segments */
static int afd_rewind_seg(AFFILE *af)
{
    struct afd_private *ap = AFD_PRIVATE(af);
    ap->cur_file = 0;
    for(int i=0;i<ap->num_afs;i++){
	af_rewind_seg(ap->afs[i]);
    }
    return 0;		
}



/* Update:
 * If this segment is in any of the existing files, update it there.
 * Otherwise, if the last file isn't too big, add it there.
 * Otherwise, ada a new file.
 */
static int afd_update_seg(AFFILE *af, const char *name,
		    unsigned long arg,const void *value,unsigned int vallen,int append)
    
{
    struct afd_private *ap = AFD_PRIVATE(af);
    AFFILE *af2 = afd_file_with_seg(af,name);
    if(af2){
	return af_update_seg(af2,name,arg,value,vallen,append); // update where it was found
    }
    /* Segment doesn't exist anywhere... */
    if(append==0){
	errno = ENOENT;
	return -1;
    }
    /* Append to the last file if there is space and a space limitation... */
    if(ap->num_afs>0){
	AFFILE *af2 = ap->afs[ap->num_afs-1];
	FILE *aseg = af2->aseg;

	uint64 offset = ftello(aseg);
	fseeko(aseg,0,SEEK_END);

	uint64 len = ftello(aseg);
	fseeko(aseg,offset,SEEK_SET);

	if((len + vallen + 1024 < af->maxsize) && (af->maxsize!=0)){
	    /* It should fit with room left over! */
	    return af_update_seg(af2,name,arg,value,vallen,append);
	}
    }
    /* Add a new segment and update that one */
    if(afd_add_file(af,0)) return -1;
    af2 = ap->afs[ap->num_afs-1];
    return af_update_seg(af2,name,arg,value,vallen,append);
}

int afd_del_seg(AFFILE *af,const char *segname)
{
    AFFILE *af2 = afd_file_with_seg(af,segname);
    if(af2){
	return af_del_seg(af2,segname);
    }
    return -1;				// not found
}


struct af_vnode vnode_afd = {
    AF_IDENTIFY_AFD,		// 
    AF_VNODE_TYPE_COMPOUND,		// 
    "AFF Directory",
    afd_identify_file,
    afd_open,			// open
    afd_close,			// close
    afd_vstat,			// vstat
    afd_get_seg,		// get_seg
    afd_get_next_seg,		// get_next_seg
    afd_rewind_seg,		// rewind_seg
    afd_update_seg,		// update_seg
    afd_del_seg,		// del_seg
    0,				// read
    0				// write
};
