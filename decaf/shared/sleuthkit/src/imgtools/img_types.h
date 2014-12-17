/*
** img_types
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier.  All rights reserved
**
*/

#ifndef _IMG_TYPES_H
#define _IMG_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

    extern uint8_t img_parse_type(const char *);
    extern void img_print_types(FILE *);
    extern char *img_get_type(uint8_t);

/*
** the most-sig-nibble is the image type, which indicates which
** _open function to call.  The least-sig-nibble is the specific type
** of implementation.  
*/
#define IMGMASK			0xf0
#define OSMASK			0x0f

#define UNSUPP_IMG		0x00

/* RAW */
#define RAW_TYPE		0x10
#define RAW_SING		0x11
#define RAW_SPLIT		0x12

/* AFF */
#define AFF_TYPE		0x20
#define AFF_AFF			0x21
#define AFF_AFD			0x22
#define AFF_AFM			0x23

/* EWF */
#define EWF_TYPE		0x30
#define EWF_EWF			0x31

/* QEMU */
#define QEMU_IMG		0x40

#ifdef __cplusplus
}
#endif
#endif
