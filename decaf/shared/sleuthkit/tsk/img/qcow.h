/*
 * The Sleuth Kit - Add on for QEMU Copy-On-Write (QCOW) image support
 *
 * Copyright (c) 2011 Joachim Metz <jbmetz@users.sourceforge.net>
 *
 * This software is distributed under the Common Public License 1.0
 */

/* 
 * Header files for QCOW-specific data structures and functions. 
 */

#if !defined( _TSK_IMG_QCOW_H )
#define _TSK_IMG_QCOW_H

#if defined( TSK_WIN32 )
#include <config_msc.h>
#endif

#if HAVE_LIBQCOW

#include <libqcow.h>

#ifdef __cplusplus
extern "C" {
#endif

extern \
TSK_IMG_INFO *qcow_open(
               int,
               const TSK_TCHAR * const images[],
               unsigned int a_ssize );

typedef struct
{
	TSK_IMG_INFO img_info;

	libqcow_file_t *file;

} IMG_QCOW_INFO;

#ifdef __cplusplus
}
#endif

#endif /* HAVE_LIBQCOW */

#endif
