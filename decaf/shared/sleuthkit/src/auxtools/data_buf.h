/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * I/O buffer, used for all forms of I/O.
 */

#ifndef _DATA_BUF_H
#define _DATA_BUF_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct DATA_BUF DATA_BUF;

    struct DATA_BUF {
	char *data;		/* buffer memory */
	size_t size;		/* buffer size */
	size_t used;		/* amount of space used */
	DADDR_T addr;		/* start block */
    };

    extern DATA_BUF *data_buf_alloc(size_t);
    extern void data_buf_free(DATA_BUF *);


#ifdef __cplusplus
}
#endif
#endif
