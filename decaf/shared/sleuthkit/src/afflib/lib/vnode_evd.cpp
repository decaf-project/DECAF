/*
 * vnode_evd.cpp:
 * 
 * Functions for the manipulation of multiple EnCase files
 */

#include "afflib.h"
#include "afflib_i.h"
#include "vnode_evf.h"
#include "vnode_evd.h"

/****************************************************************
 *** Service routines
 ****************************************************************/

struct evd_private {
    AFFILE **evfs;				// list of AFFILEs...
    unsigned int num_evfs;			// number of them
    unsigned int cur_file;			// current file for get_next
};

static inline struct evd_private *EVD_PRIVATE(AFFILE *af)
{
    assert(af->v == &vnode_evd);
    return (struct evd_private *)(af->vnodeprivate);
}


/* evd_file_with_seg:
 * Returns the AFFILE for a given segment, or 0 if it isn't found.
 */

static AFFILE *evd_file_with_seg(AFFILE *af,const char *name)
{
    struct evd_private *ap = EVD_PRIVATE(af);
    for(unsigned int i=0;i<ap->num_evfs;i++){
	if(af_get_seg(ap->evfs[i],name,0,0,0)==0){
	    return ap->evfs[i];
	}
    }
    errno = ENOTDIR;			// get ready for error return
    return 0;
}

/* Return 1 if a file is an EnCase .E01 file and there are other EnCase files that follow*/
static int evd_identify_file(const char *filename)
{
    if(access(filename,R_OK)!=0) return 0; // can't read the file
    if(((*vnode_evf.identify)(filename))!=1) return 0; // base file in not an EnCase file

    /* Figure out the name for the .E02 file */
    char *e02 = (char *)alloca(strlen(filename)+1);
    strcpy(e02,filename);
    char *ext = strrchr(e02,'.');
    if(!ext) return 0;			// no extension; go home
    if(strcmp(ext,".E01")!=0 && strcmp(ext,".e01")!=0) return 0; // extension of first file was not .e01; quit

    /* change e02 to be a .e02 file */
    ext[3] = '2';
    if(access(e02,R_OK)!=0) return 0;	// no .E02 file
    if((*vnode_evf.identify)(e02)!=1) return 0; // .e02 file is not an EnCase file

    return 1;				// both .E01 and .E02 files exist
}

/* Add an EVF file to our list:
 * Open the file and add it to the list
 */
static void evd_add_file(AFFILE *af,AFFILE *anew)
{
    struct evd_private *ep = EVD_PRIVATE(af);
    ep->num_evfs += 1;
    ep->evfs = (AFFILE **)realloc(ep->evfs,sizeof(AFFILE *) * ep->num_evfs);
    ep->evfs[ep->num_evfs-1] = anew;
}
    
	
	    
/****************************************************************
 *** User-visible functions.
 ****************************************************************/

static int evd_close(AFFILE *af);

static int evd_open(AFFILE *af)
{
    /* If we reach here, we must be opening an E01 file.
     * Try to open all Enn files until we can't find them anymore.
     */
    char buf[MAXPATHLEN+1];
    strcpy(buf,af_filename(af));    // get the filename
    
    /* check extension */
    char *ext = buf+strlen(buf)-3;
    assert(ext > buf && !strncmp(ext, "E01", 3));
    
    /* Allocate our private storage */
    af->vnodeprivate = (void *)calloc(1,sizeof(struct evd_private));
    
    struct evd_private *ed = EVD_PRIVATE(af);
    ed->evfs = (AFFILE **)malloc(0);
    
    /* Now open all of the files until as high as .ZZZ */
    for (int i = 1; i <= 99; i++) {
	sprintf(ext+1,"%02d",i);
	if(access(buf,R_OK)!=0) break;  // file can't be read
	
	AFFILE *en = af_open_with(buf,af->openflags,af->openmode,&vnode_evf);
	if(!en) break;          // this file doens't exist
	evd_add_file(af,en);        // add to our array
    }
    for (int i = 4*26*26; i <= 26*26*26; i++) {
	sprintf(ext, "%c%c%c", i/26/26%26+'A', i/26%26+'A', i%26+'A');
	if(access(buf,R_OK)!=0) break;  // file can't be read
	
	AFFILE *en = af_open_with(buf,af->openflags,af->openmode,&vnode_evf);
	if(!en) break;          // this file doens't exist
	evd_add_file(af,en);        // add to our array
    }
    

    /* Now that everything is opened, lets validate each file and set the chunk_start pointers */
    struct evf_private *ef0 = EVF_PRIVATE(ed->evfs[0]);
    if(ef0->chunk_size==0){	// E01 file didn't have a volume section...
	evd_close(af);		// free the stuff
	return -1;		//
    }

    /* Set the chunk sizes and offsets in all of the other files */
    for(unsigned int i=1;i<ed->num_evfs;i++){
	struct evf_private *ef_this = EVF_PRIVATE(ed->evfs[i]);
	struct evf_private *ef_prev = EVF_PRIVATE(ed->evfs[i-1]);
	ef_this->chunk_size  = ef0->chunk_size;
	ef_this->chunk_start = ef_prev->chunk_start + ef_prev->table_section->chunk_count;
    }
    af->image_pagesize = ed->evfs[0]->image_pagesize;
    
    return 0;				// "we were successful"
}


static int evd_close(AFFILE *af)
{
    struct evd_private *ed = EVD_PRIVATE(af);

    /* Close all of the subfiles, then free the memory, then close this file */
    for(unsigned int i=0;i<ed->num_evfs;i++){
	af_close(ed->evfs[i]);
    }
    free(ed->evfs);
    memset(ed,0,sizeof(*ed));		// clean object reuse
    free(ed);				// won't need it again
    return 0;
}


/* vstat: return information about the EVD */
static int evd_vstat(AFFILE *af,struct af_vnode_info *vni)
{
    struct evd_private *ed = EVD_PRIVATE(af);

    memset(vni,0,sizeof(*vni));		// clear it

    /* Return the size of the whole image */
    vni->imagesize = 0;			// start off with 0
    for(unsigned int i=0;i<ed->num_evfs;i++){
	/* Stat each file and add its size */
	struct af_vnode_info v2;
	memset(&v2,0,sizeof(v2));
	if(af_vstat(ed->evfs[i],&v2)==0){
	    vni->imagesize += v2.imagesize;
	}
    }
    vni->has_pages		= 1;
    vni->supports_metadata      = 0;
    return 0;
}

/* get_seg needs to figure out which of the individual EVF files
 * has the requested segment and call it.
 */
static int evd_get_seg(AFFILE *af,const char *name,unsigned long *arg,unsigned char *data,
		       size_t *datalen)
{
    AFFILE *af2 = evd_file_with_seg(af,name);
    if(af2){
	return af_get_seg(af2,name,arg,data,datalen); // use this one
    }
    return -1;				// not found
}


static int evd_get_next_seg(AFFILE *af,char *segname,size_t segname_len,unsigned long *arg,
			unsigned char *data,size_t *datalen_)
{
    /* See if there are any more in the current segment */
    struct evd_private *ep = EVD_PRIVATE(af);
    if(ep->num_evfs==0) return AF_ERROR_EOF; // no files, so we are at the end
    do {
	int r = af_get_next_seg(ep->evfs[ep->cur_file],segname,segname_len,arg,data,datalen_);
	if(r!=AF_ERROR_EOF){		// if it is not EOF
	    return r;
	}
	if(ep->cur_file < ep->num_evfs-1){
	    ep->cur_file++;		// go to the next file if there is one
	    if(ep->cur_file < ep->num_evfs){
		af_rewind_seg(ep->evfs[ep->cur_file]); // and rewind that file
	    }
	}
    } while(ep->cur_file < ep->num_evfs-1);
    return AF_ERROR_EOF;		// really made it to the end
}


static int evd_rewind_seg(AFFILE *af)
{
    struct evd_private *ep = EVD_PRIVATE(af);
    ep->cur_file = 0;
    if(ep->num_evfs>0){
	return af_rewind_seg(ep->evfs[0]);
    }
    return 0;				// no files, so nothing to rewind
}



struct af_vnode vnode_evd = {
    AF_IDENTIFY_EVD,		// 
    AF_VNODE_TYPE_COMPOUND,
    "Set of EnCase Files",
    evd_identify_file,
    evd_open,				// open
    evd_close,				// close
    evd_vstat,				// vstat
    evd_get_seg,			// get_seg
    evd_get_next_seg,			// get_next_seg
    evd_rewind_seg,			// rewind_seg
    0,					// update_seg
    0,					// del_seg
    0,					// read
    0					// write
};
