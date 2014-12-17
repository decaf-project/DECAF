/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2005 Brian Carrier.  All rights reserved 
 */
#ifndef _RAW_H
#define _RAW_H

#ifdef __cplusplus
extern "C" {
#endif

    extern IMG_INFO *raw_open(const char **, IMG_INFO *);
    extern IMG_INFO *qemu_image_open(void *opaque);

    typedef struct IMG_RAW_INFO IMG_RAW_INFO;
    struct IMG_RAW_INFO {
	IMG_INFO img_info;
	int fd;
	off_t seek_pos;
    };

    typedef struct IMG_QEMU_INFO IMG_QEMU_INFO;
    struct IMG_QEMU_INFO {
	IMG_INFO img_info;
	void * opaque; 
    };


#ifdef __cplusplus
}
#endif
#endif
