/*
 * The AFFLIB data stream interface.
 * Supports the page->segment name translation, and the actual file pointer.
 */

/*
 * Copyright (c) 2005, 2006
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


/****************************************************************
 *** Internal Functions.
 ****************************************************************/

/* af_read_sizes:
 * Get the page sizes if they are set in the file.
 */
void af_read_sizes(AFFILE *af)
{
    if(af_get_seg(af,AF_PAGESIZE,&af->image_pagesize,0,0)){
	af_get_seg(af,AF_SEGSIZE_D,&af->image_pagesize,0,0); // try old name
    }
    af_get_segq(af,AF_IMAGESIZE,(int64 *)&af->image_size);
    af->image_size_in_file = af->image_size;

    af_get_seg(af,AF_SECTORSIZE,&af->image_sectorsize,0,0);
    if(af->image_sectorsize==0) af->image_sectorsize = 512; // reasonable default
    size_t sectorsize = af->image_sectorsize;
    if(af->badflag==0) af->badflag = (unsigned char *)malloc(sectorsize);
    af_get_seg(af,AF_BADFLAG,0,af->badflag,(size_t *)&sectorsize);
}


int af_page_size(AFFILE *af)
{
    return af->image_pagesize;
}

/* af_set_sectorsize:
 * Sets the sectorsize. Fails with -1 if imagesize >=0 unless these changes permitted
 */
int af_set_sectorsize(AFFILE *af,int sectorsize)
{
    struct af_vnode_info vni;
    af_vstat(af,&vni);
    if(vni.changable_pagesize==0 && af->image_size>0){
	errno = EINVAL;
	return -1;
    }
    af->image_sectorsize =sectorsize;
    if(af->badflag==0) af->badflag = (unsigned char *)malloc(sectorsize);
    else af->badflag = (unsigned char *)realloc(af->badflag,sectorsize);

    if(af_update_seg(af,AF_SECTORSIZE,sectorsize,0,0,1)){
	if(errno != ENOTSUP) return -1;
    }
    return 0;
}


/*
 * af_set_pagesize:
 * Sets the pagesize. Fails with -1 if it can't be changed.
 */
int af_set_pagesize(AFFILE *af,long pagesize)
{
    /* Allow the pagesize to be changed if it hasn't been set yet
     * and if this format doesn't support metadata updating (which is the raw formats)
     */
    struct af_vnode_info vni;

    af_vstat(af,&vni);

    if(vni.changable_pagesize==0 && af->image_size>0){
	errno = EINVAL;
	return -1;
    }
    if(pagesize % af->image_sectorsize != 0){
	(*af->error_reporter)("Cannot set pagesize to %d (sectorsize=%d)\n",
			      pagesize,af->image_sectorsize);
	errno = EINVAL;
	return -1;
    }

    af->image_pagesize = pagesize;
    if(af_update_seg(af,AF_PAGESIZE,pagesize,0,0,1)){
	if(errno != ENOTSUP) return -1;	// error updating (don't report ENOTSUP);
    }
    return 0;
}


/*
 * af_set_maxsize
 * Sets the maxsize.
 Fails with -1 if imagesize >= 0 unless this is a raw or split_raw file
 */
int af_set_maxsize(AFFILE *af,int64 maxsize)
{
    if(af_imagesize(af)>0){
	(*af->error_reporter)("Cannot set maxsize as imagesize is %"I64d,af_imagesize(af));
	return -1;	// now allowed to set if imagesize is bigger than 0
    }
    if((af->image_sectorsize==0) || (maxsize % af->image_sectorsize != 0)){
	(*af->error_reporter)("Cannot set maxsize to %"I64d" (sectorsize=%d)\n",
			      maxsize,af->image_sectorsize);
	return -1;
    }
    if((af->image_pagesize!=0) && (maxsize % af->image_pagesize != 0)){
	(*af->error_reporter)("Cannot set maxsize to %"I64d" (pagesize=%d)\n",
			      maxsize,af->image_pagesize);
	return -1;
    }
    af->maxsize = maxsize;
    return 0;
}

const unsigned char *af_badflag(AFFILE *af)
{
    return af->badflag;
}


/****************************************************************
 *** page-level interface
 ****************************************************************/

int af_get_page_raw(AFFILE *af,int64 pagenum,unsigned long *arg,unsigned char *data,size_t *bytes)
{
    char segname[AF_MAX_NAME_LEN];
    
    memset(segname,0,sizeof(segname));
    sprintf(segname,AF_PAGE,pagenum);
    int r = af_get_seg(af,segname,arg,data,bytes);
    if(r!=0){
	sprintf(segname,AF_SEG_D,pagenum);
	r = af_get_seg(af,segname,arg,data,bytes);
    }
    return r;
}

/* af_get_page:
 * Get a page from its named segment.
 * If the page is compressed, uncompress it.
 * data points to a segmenet of at least *bytes;
 * *bytes is then modified to indicate the actual amount of bytes read.
 * if shouldfree is set, then data should be freed.
 * Return 0 if success, -1 if fail.
 */

int af_get_page(AFFILE *af,int64 pagenum,unsigned char *data,size_t *bytes)
{
    unsigned long arg=0;
    size_t page_len=0;

    /* Figure out the segment name from the number */
    
    /* Find out the size of the segment and if it is compressed or not.
     * If we can't find it with new nomenclature, try new one...
     */
    if(af_get_page_raw(af,pagenum,&arg,0,&page_len)){
	/* Segment doesn't exist. Fill with the 'bad segment' flag */
	for(size_t i = 0;i <= af->image_pagesize - af->image_sectorsize;
	    i+= af->image_sectorsize){
	    memcpy(data+i,af->badflag,af->image_sectorsize);
	    af->bytes_memcpy += af->image_sectorsize;
	}
	return -1;			// segment doesn't exist
    }
       
    /* If the segment isn't compressed, just get it*/
    if((arg & AF_PAGE_COMPRESSED)==0){
	int ret = af_get_page_raw(af,pagenum,0,data,bytes);
	if(*bytes > page_len) *bytes = page_len; // we only read this much
	if(ret!=0) return ret;		// some error happened?
    }
    else {
	/* Allocate memory to hold the compressed segment */
	unsigned char *compressed_data = (unsigned char *)malloc(page_len);
	size_t compressed_data_len = page_len;
	if(compressed_data==0){
	    return -2;			// memory error
	}

	/* Get the data */
	if(af_get_page_raw(af,pagenum,0,compressed_data,&compressed_data_len)){
	    free(compressed_data);
	    return -3;			// read error
	}

	/* Now uncompress into the user-provided buffer */
	/* Currently this only supports one compression algorithm */
	uLongf bytes_ulf = *bytes;
	int r = uncompress(data,&bytes_ulf,compressed_data,compressed_data_len);
	*bytes = bytes_ulf;
	switch(r){
	case Z_OK:
	    break;
	case Z_ERRNO:
	  (*af->error_reporter)("Z_ERRNOR decompressing segment %"I64d,pagenum);
	case Z_STREAM_ERROR:
	  (*af->error_reporter)("Z_STREAM_ERROR decompressing segment %"I64d,pagenum);
	case Z_DATA_ERROR:
	  (*af->error_reporter)("Z_DATA_ERROR decompressing segment %"I64d,pagenum);
	case Z_MEM_ERROR:
	  (*af->error_reporter)("Z_MEM_ERROR decompressing segment %"I64d,pagenum);
	case Z_BUF_ERROR:
	  (*af->error_reporter)("Z_BUF_ERROR decompressing segment %"I64d,pagenum);
	case Z_VERSION_ERROR:
	  (*af->error_reporter)("Z_VERSION_ERROR decompressing segment %"I64d,pagenum);
	default:
	  (*af->error_reporter)("uncompress returned an invalid value in get_segment");
	}
	free(compressed_data);		// don't need this one anymore
	if(r!=Z_OK) return -1;
    }

    /* If the page size is larger than the sector_size,
     * make sure that the rest of the sector is zeroed, and that the
     * rest after that has the 'bad block' notation.
     */
    if(af->image_pagesize > af->image_sectorsize){
	const int SECTOR_SIZE = af->image_sectorsize;	// for ease of typing
	size_t bytes_left_in_sector = (SECTOR_SIZE - (*bytes % SECTOR_SIZE)) % SECTOR_SIZE;
	for(size_t i=0;i<bytes_left_in_sector;i++){
	    data[*bytes + i] = 0;
	}
	size_t end_of_data = *bytes + bytes_left_in_sector;
	
	/* Now fill to the end of the page... */
	for(size_t i = end_of_data; i <= af->image_pagesize-SECTOR_SIZE; i+=SECTOR_SIZE){
	    memcpy(data+i,af->badflag,SECTOR_SIZE);
	    af->bytes_memcpy += SECTOR_SIZE;
	}
    }
    
    return 0;
}


/* Write a actual data segment to the disk */
int af_update_page(AFFILE *af,int64 pagenum,unsigned char *data,int datalen)
{
    /* Check for bypass */
    if(af->v->write){
	int r = (*af->v->write)(af,data,af->image_pagesize * pagenum,datalen);
	if(r!=datalen) return -1;
	return 0;
    }


    struct affcallback_info acbi;
    int ret = 0;
    unsigned int starting_csegs_written = af->csegs_written;

    /* Setup the callback structure */
    memset(&acbi,0,sizeof(acbi));
    acbi.info_version = 1;
    acbi.af = af->parent ? af->parent : af;
    acbi.pagenum = pagenum;
    acbi.bytes_to_write = datalen;
    if(af->compression_type != AF_COMPRESSION_NONE){ // it will be compressedn
	acbi.compressed = 1;		// it was compressed
	acbi.compression_alg = AF_PAGE_COMP_ALG_ZLIB; // only one that we support
	acbi.compression_level = af->compression_level;
    }

    /* Set up the segnment */
    char vbuf[32];
    sprintf(vbuf,AF_PAGE,pagenum);
    uLongf destLen = af->image_pagesize;	// it could be this big.
    if(af->compression_type != AF_COMPRESSION_NONE){
	/* Try to compress the data */
	
	unsigned char *cdata = (unsigned char *)malloc(destLen); 
	if(cdata!=0){		// If data could be allocated
	    
	    if(af->w_callback) {acbi.phase = 1;(*af->w_callback)(&acbi);}
	    int cres = compress2((Bytef *)cdata, &destLen,
				 (Bytef *)data,datalen,
				 af->compression_level);

	    if(af->w_callback) {acbi.phase = 2;(*af->w_callback)(&acbi);}
	    if(cres==0 && destLen < af->image_pagesize){

		/* Notify caller */

		/* Prepare to write out the compressed segment with compression */
		unsigned int flag = 0;
		flag |= AF_PAGE_COMPRESSED;
		flag |= AF_PAGE_COMP_ALG_ZLIB; // that's what we are using now
		if(af->compression_level == AF_COMPRESSION_MAX){
		    flag |= AF_PAGE_COMP_MAX; // useful to know it can't be better
		}

		if(af->w_callback) {acbi.phase = 3;(*af->w_callback)(&acbi);}
		ret = af_update_seg(af,vbuf,flag,cdata,destLen,1);
		acbi.bytes_written = destLen;
		if(af->w_callback) {acbi.phase = 4;(*af->w_callback)(&acbi);}
		if(ret==0){
		    acbi.bytes_written = destLen;	// because that is how much we wrote
		    af->csegs_written++;
		}
	    }
	    free(cdata);
	    cdata = 0;
	}
    }
    
    /* If a compressed segment was not written, write it uncompressed */
    if(af->csegs_written == starting_csegs_written){ 
	if(af->w_callback) {acbi.phase = 3;(*af->w_callback)(&acbi);}
	ret = af_update_seg(af,vbuf,0,data,datalen,1);
	acbi.bytes_written = datalen;
	if(af->w_callback) {acbi.phase = 4;(*af->w_callback)(&acbi);} 
	if(ret==0){
	    acbi.bytes_written = datalen;	// because that is how much we wrote
	    af->usegs_written++;
	}
    }
    return ret;
}

/****************************************************************
 *** Stream-level interface
 ****************************************************************/


/* Throw out the current segment */
int af_purge(AFFILE *af)
{
    int ret = 0;

    /* If the segment is dirty, we need to write it back
     * (and, if there is already a segment, we need to invalidate it.)
     */
    if(af->dirty){
	ret = af_update_page(af,af->pagenum,af->pagebuf,af->pagebuf_bytes); // was af->image_pagesize
	af->dirty = 0;
    }
    af->pagenum = -1;			// don't trust it anymore
    return ret;
}

int af_read(AFFILE *af,unsigned char *buf,int count)
{
    if (af->v->read){			// bypass
	int r = (af->v->read)(af, buf, af->pos, count);
	if(r>0) af->pos += r;
	return r;
    }

    uint64 image_size = af_imagesize(af); /* get the imagesize */
    if(image_size==0) return 0;		// no data in file

    uint64 offset = af->pos;		/* where to start */

    /* Make sure we have a pagebuf if none was defined */
    if(af->image_pagesize==0){		// page size not defined
	errno = EFAULT;
	return -1;
    }

    if(af->pagebuf==0) af->pagebuf = (unsigned char *)malloc(af->image_pagesize);

    int total = 0;
    while(count>0){

	/* If the correct segment is not loaded, purge the segment */
	int new_page = offset / af->image_pagesize;
	if(new_page != af->pagenum){
	    af_purge(af);
	}

	/* If no segment is loaded, load the current segment */
	if(af->pagenum<0){
	    af->pagenum = offset / af->image_pagesize;	// will be the segment we want
	    af->pagebuf_bytes = af->image_pagesize;		// we can hold this much
	    if(af_get_page(af,af->pagenum,af->pagebuf, &af->pagebuf_bytes)){
		break;			// no more to get
	    }
	}
	// Compute how many bytes can be copied...
	// where we were reading from
	unsigned int page_offset = offset - af->pagenum * af->image_pagesize; 
	unsigned int page_left   = af->pagebuf_bytes - page_offset; // number we can get out
	unsigned int bytes_to_read = count;

	if(bytes_to_read > page_left) bytes_to_read = page_left;
	if(bytes_to_read > image_size - offset) bytes_to_read = image_size - offset;

	assert(bytes_to_read >= 0);	// 
	if(bytes_to_read==0) break; // that's all we could get

	/* Copy out the bytes for the user */
	memcpy(buf,af->pagebuf+page_offset,bytes_to_read); // copy out
	af->bytes_memcpy += bytes_to_read;
	buf     += bytes_to_read;
	offset  += bytes_to_read;
	count   -= bytes_to_read;
	total   += bytes_to_read;
	af->pos += bytes_to_read;
    }
    /* We have copied all of the user's requested data, so return */
    return total;
}


/*
 * Handle writing to the file...
 * af_write() --- returns the number of bytes written
 *
 */

int af_write(AFFILE *af,unsigned char *buf,int count)
{
    /* First log the write request and make sure that this stream is enabled for writing */

    if (af->logfile) fprintf(af->logfile,"af_write(%p,%p,%d)\n",af,buf,count);

    /* vnode write bypass:
     * If a write function is defined, use it.
     */
    if (af->v->write){		
	int r = (af->v->write)(af, buf, af->pos, count);
	if(r>0) af->pos += r;
	if(af->pos >= af->image_size) af->image_size = af->pos;
	return r;
    }

    if (!af->writing){
	if(af->logfile) fprintf(af->logfile,"  EPERM\n");
	errno = EPERM;			// operation not permitted
	return -1;			// not opened for writing
    }

    uint64 offset = af->pos;		// where to start

    /* If the correct segment is not loaded, purge the current segment */
    int write_page = offset / af->image_pagesize;
    if(write_page != af->pagenum){
	af_purge(af);
    }

    int write_page_offset = offset % af->image_pagesize;

    /* Page Write Bypass:
     * If no data has been written into the current page buffer,
     * and if the position of the stream is byte-aligned on the page buffer,
     * and if an entire page is being written,
     * just write it out and update the pointers, then return.
     */
    if(af->pagenum == -1 &&
       af->image_pagesize==(unsigned)count &&
       write_page_offset == 0){
	int ret = af_update_page(af,write_page,buf,count);
	if(ret==0){			// no error
	    af->pos += count;
	    if(af->pos > af->image_size) af->image_size = af->pos;
	    return count;
	}
	return -1;			// error
    }
       


    /* Can't use high-speed optimization.
     * Make sure we have a pagebuf if none was defined
     */
    if(af->pagebuf==0) af->pagebuf = (unsigned char *)malloc(af->image_pagesize);

    int total = 0;
    while(count>0){
	/* If no segment is loaded, load the current segment */
	if(af->pagenum<0){
	    af->pagenum = offset / af->image_pagesize;	// will be the segment we want
	    af->pagebuf_bytes = af->image_pagesize;
	    if(af_get_page(af,af->pagenum,af->pagebuf, &af->pagebuf_bytes)){
		/* Create a new segment! */
		af->pagebuf_bytes = 0;
	    }
	}
	unsigned int seg_offset = offset - af->pagenum * af->image_pagesize; // where writing to
	unsigned int seg_left   = af->image_pagesize - seg_offset; // number we can write into
	unsigned int bytes_to_write = count;

	if(bytes_to_write > seg_left) bytes_to_write = seg_left;

	assert(bytes_to_write >= 0);	// 
	if(bytes_to_write==0) break; // that's all we could get

	/* Copy out the bytes for the user */
	memcpy(af->pagebuf+seg_offset,buf,bytes_to_write); // copy out
	af->bytes_memcpy += bytes_to_write;

	if(af->pagebuf_bytes < seg_offset+bytes_to_write){
	    af->pagebuf_bytes = seg_offset+bytes_to_write; // it has been extended.
	}

	buf     += bytes_to_write;
	offset  += bytes_to_write;
	count   -= bytes_to_write;
	total   += bytes_to_write;
	af->pos += bytes_to_write;
	af->dirty = 1;

	/* If we wrote out all of the bytes that were left in the segment,
	 * then we are at the end of the segment, write it back...
	 */
	if(seg_left == bytes_to_write){	
	    if(af_purge(af)) return -1;
	}

	/* If we have written more than the image size, update the image size */
	if(offset > af->image_size) af->image_size = offset;

    }
    /* We have copied all of the user's requested data, so return */
    return total;
}

int af_is_badblock(AFFILE *af,unsigned char *buf)
{
    return memcmp(af->badflag,buf,af->image_sectorsize)==0;
}
