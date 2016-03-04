/*
 * The Sleuth Kit - Add on for QEMU Copy-On-Write (QCOW) image support
 *
 * Copyright (c) 2011 Joachim Metz <jbmetz@users.sourceforge.net>
 *
 * qcow
 *
 * This software is distributed under the Common Public License 1.0
 */

/** \file qcow.c
 * Internal code for TSK to interface with libqcow.
 */

#include "tsk_img_i.h"

#if HAVE_LIBQCOW

#include "qcow.h"

#define TSK_QCOW_ERROR_STRING_SIZE	512

static \
ssize_t qcow_image_read(
         TSK_IMG_INFO *img_info,
         TSK_OFF_T offset,
         char *buffer,
         size_t size )
{
	char error_string[ TSK_QCOW_ERROR_STRING_SIZE ];

	IMG_QCOW_INFO *qcow_info    = (IMG_QCOW_INFO *) img_info;
	libqcow_error_t *qcow_error = NULL;
	ssize_t read_count          = 0;

	if( tsk_verbose != 0 )
	{
		tsk_fprintf(
		 stderr,
		 "qcow_read: byte offset: %" PRIuOFF " len: %" PRIuSIZE "\n",
		 offset,
		 size );
	}
	if( offset > img_info->size )
	{
		tsk_error_reset();
    tsk_error_set_errno(TSK_ERR_IMG_READ_OFF);

		tsk_error_set_errstr("split_read - %" PRIuOFF,offset );

		return( -1 );
	}
	read_count = libqcow_file_read_buffer_at_offset(
	              qcow_info->file,
	              buffer,
	              size,
	              offset,
	              &qcow_error );

	if( read_count < 0 )
	{
		tsk_error_reset();
    tsk_error_set_errno(TSK_ERR_IMG_READ);

		if( libqcow_error_backtrace_sprint(
		     qcow_error,
		     error_string,
		     TSK_QCOW_ERROR_STRING_SIZE ) == -1 )
		{
			 tsk_error_set_errstr("qcow_read - offset: %" PRIuOFF " - len: %" PRIuSIZE " - %s",
			 offset,
			 size,
			 strerror( errno ) );
		}
		else
		{
	     tsk_error_set_errstr("qcow_read - offset: %" PRIuOFF " - len: %" PRIuSIZE "\n%s",
			 offset,
			 size,
			 error_string );
		}
                libqcow_error_free(
                 &qcow_error );

		return( -1 );
	}
	return( read_count );
}

static \
void qcow_image_imgstat(
      TSK_IMG_INFO *img_info,
      FILE * hFile )
{
	tsk_fprintf(
	 hFile,
	 "IMAGE FILE INFORMATION\n"
	 "--------------------------------------------\n"
	 "Image Type:\t\tqcow\n"
	 "\nSize of data in bytes:\t%" PRIuOFF "\n",
	 img_info->size );

	return;
}

static \
void qcow_image_close(
      TSK_IMG_INFO *img_info )
{
	IMG_QCOW_INFO *qcow_info = (IMG_QCOW_INFO *) img_info;

	libqcow_file_close(
	 qcow_info->file,
	 NULL );
	libqcow_file_free(
	 &( qcow_info->file ),
	 NULL );
	free(
	 img_info );
}

TSK_IMG_INFO *qcow_open(
               int num_img,
               const TSK_TCHAR * const images[],
               unsigned int a_ssize )
{
	char error_string[ TSK_QCOW_ERROR_STRING_SIZE ];

	IMG_QCOW_INFO *qcow_info    = NULL;
	libqcow_error_t *qcow_error = NULL;
	TSK_IMG_INFO *img_info      = NULL;

	qcow_info = (IMG_QCOW_INFO *) tsk_malloc(
	                               sizeof( IMG_QCOW_INFO ) );

	if( qcow_info == NULL )
	{
		return NULL;
	}
	img_info = (TSK_IMG_INFO *) qcow_info;

	/* Check the file signature before we call the library open
	 */
#if defined( TSK_WIN32 )
	if( libqcow_check_file_signature_wide(
	     images[ 0 ],
	     &qcow_error ) != 1 )
#else
	if( libqcow_check_file_signature(
	     images[ 0 ],
	     &qcow_error ) != 1 )
#endif
	{
		tsk_error_reset();

		tsk_error_set_errno(TSK_ERR_IMG_MAGIC);

		if( libqcow_error_backtrace_sprint(
		     qcow_error,
		     error_string,
		     TSK_QCOW_ERROR_STRING_SIZE ) == -1 )
		{
		tsk_error_set_errstr("qcow_open: Not an QCOW file" );
		}
		else
		{
		tsk_error_set_errstr("qcow_open: Not an QCOW file\n%s",
			 error_string );
		}
                libqcow_error_free(
                 &qcow_error );

		free(
		 qcow_info );

		if(tsk_verbose != 0 )
		{
			tsk_fprintf(
			 stderr,
			 "Not an QCOW file\n" );
		}
		return( NULL );
	}
	if( libqcow_file_initialize(
	     &( qcow_info->file ),
	     &qcow_error ) != 1 )
	{
        	tsk_error_reset();

	        tsk_error_set_errno(TSK_ERR_IMG_OPEN);

		if( libqcow_error_backtrace_sprint(
		     qcow_error,
		     error_string,
		     TSK_QCOW_ERROR_STRING_SIZE ) == -1 )
		{
			tsk_error_set_errstr("qcow_open file: %" PRIttocTSK ": Error opening",
			 images[ 0 ] );
		}
		else
		{
			tsk_error_set_errstr("qcow_open file: %" PRIttocTSK ": Error opening\n%s",
			 images[ 0 ],
			 error_string );
		}
		free(
		 qcow_info);

		if( tsk_verbose != 0 )
		{
			tsk_fprintf(
			 stderr,
			 "Unable to create QCOW file\n" );
		}
		return( NULL );
	}
#if defined( TSK_WIN32 )
	if( libqcow_file_open_wide(
	     qcow_info->file,
	     images[ 0 ],
	     LIBQCOW_OPEN_READ,
	     &qcow_error ) != 1 )
#else
	if( libqcow_file_open(
	     qcow_info->file,
	     images[ 0 ],
	     LIBQCOW_OPEN_READ,
	     &qcow_error ) != 1 )
#endif
	{
        	tsk_error_reset();

	        tsk_error_set_errno(TSK_ERR_IMG_OPEN);

tsk_error_set_errstr("qcow_open file: %" PRIttocTSK ": Error opening",
		 images[ 0 ] );

	        free(
		 qcow_info );

		if( tsk_verbose != 0 )
		{
			tsk_fprintf(
			 stderr,
			 "Error opening QCOW file\n" );
		}
		return( NULL );
	}
	if( libqcow_file_get_media_size(
	     qcow_info->file,
	     (size64_t *) &( img_info->size ),
	     &qcow_error ) != 1 )
	{
		tsk_error_reset();

		tsk_error_set_errno(TSK_ERR_IMG_OPEN);

		if( libqcow_error_backtrace_sprint(
		     qcow_error,
		     error_string,
		     TSK_QCOW_ERROR_STRING_SIZE ) == -1 )
		{
		tsk_error_set_errstr("qcow_open file: %" PRIttocTSK ": Error getting size of image",
			 images[ 0 ] );
		}
		else
		{
		tsk_error_set_errstr("qcow_open file: %" PRIttocTSK ": Error getting size of image\n%s",
			 images[ 0 ],
			 error_string );
		}
		free(
		 qcow_info );

		if( tsk_verbose != 0 )
		{
			tsk_fprintf(
			 stderr,
			 "Error getting size of QCOW file\n" );
		}
		return( NULL );
	}
	if( a_ssize != 0 )
	{
		img_info->sector_size = a_ssize;
	}
	else
	{
		img_info->sector_size = 512;
	}
	img_info->itype   = TSK_IMG_TYPE_QCOW_QCOW;
	img_info->read    = &qcow_image_read;
	img_info->close   = &qcow_image_close;
	img_info->imgstat = &qcow_image_imgstat;

	return( img_info );
}

#endif /* HAVE_LIBQCOW */
