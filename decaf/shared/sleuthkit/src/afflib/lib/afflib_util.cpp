/*
 * utility functions used by AFFLIB.
 * These functions do not actually read or write data into the AFF File.
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

#include "afflib.h"
#include "afflib_i.h"

/* 
 * af_hexbuf:
 * Turn a binay string into a hex string, optionally with spaces.
 */

const char *af_hexbuf(char *dst,int dst_len,const unsigned char *bin,int bytes,int flag)
{
    int charcount = 0;
    const char *start = dst;		// remember where the start of the string is
    const char *fmt = (flag & AF_HEXBUF_UPPERCASE) ? "%02X" : "%02x";

    *dst = 0;				// begin with null termination
    while(bytes>0 && dst_len > 3){
	sprintf(dst,fmt,*bin); // convert the next byte
	dst += 2;
	bin += 1;
	dst_len -= 2;
	bytes--;
	charcount++;			// how many characters
	
	bool add_spaces = false;
	if(flag & AF_HEXBUF_SPACE2) add_spaces = true;
	if((flag & AF_HEXBUF_SPACE4) && charcount%2==0){
	    *dst++ = ' ';
	    *dst   = '\000';
	    dst_len -= 1;
	}
    }
    return start;			// return the start
}

uint64 af_decode_q(unsigned char buf[8])
{
    struct aff_quad *q = (struct aff_quad *)buf;		// point q to buf.

    assert(sizeof(*q)==8);		// be sure!
    return (((uint64)ntohl(q->high)) << 32) +
	((uint64)ntohl(q->low));
}

/* parse the segment number.
 * The extra %c picks up characters that might be after the number,
 * so that page5_hash doesn't match for page5.
 */
int64	af_segname_page_number(const char *name)
{
    int64 pagenum;
    char  ch;
    if(sscanf(name,AF_PAGE"%c",&pagenum,&ch)==1){
	return pagenum;			// new-style page number
    }
    if(sscanf(name,AF_SEG_D"%c",&pagenum,&ch)==1){
	return pagenum;			// old-style page number
    }
    return -1;
}

#ifndef __FreeBSD__
void strlcpy(char *dest,const char *src,int dest_size)
{
    strncpy(dest,src,dest_size);
    dest[dest_size-1] = '\000';
}

void strlcat(char *dest,const char *src,int dest_size)
{
    int dest_len = strlen(dest);
    int src_len  = strlen(src);
    int room     = dest_size - (dest_len +src_len+1);
    if(room>0){
	/* There is room; just copy over what we have and return */
	strcat(dest,src);
	return;
    }
    /* Not room; figure out how many bytes we can copy... */
    int left = dest_size - (dest_len+1);
    strncpy(dest+dest_len,src,left);
    dest[dest_len-1] = '\000';
}
#endif

