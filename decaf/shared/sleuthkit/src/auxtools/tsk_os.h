/*
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2004-2005 Brian Carrier.  All rights reserved
*/

#ifndef _TSK_OS_H
#define _TSK_OS_H

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * Solaris 2.x. Build for large files when dealing with filesystems > 2GB.
     * With the 32-bit file model, needs pread() to access filesystems > 2GB.
     */
#if defined(SUNOS5)
#define SUPPORTED

#include <sys/sysmacros.h>

/* Sol 5.7 has inttypes, but sys/inttypes is needed for PRI.. macros */
#include <inttypes.h>
#include <sys/inttypes.h>
#endif


    /*
     * FreeBSD can handle filesystems > 2GB.
     */
#if defined(FREEBSD2) || defined(FREEBSD3) || defined(FREEBSD4) || defined(FREEBSD5)
#define SUPPORTED

/* FreeBSD 5 has inttypes and support for the printf macros */
#if defined(FREEBSD4) || defined(FREEBSD5)
#include <inttypes.h>
#endif

#endif				/* FREEBSD */

    /*
     * BSD/OS can handle filesystems > 2GB.
     */
#if defined(BSDI2) || defined(BSDI3) || defined(BSDI4)
#define SUPPORTED
#include <inttypes.h>
#endif				/* BSDI */


/*
 * NetBSD
 */
#if defined(NETBSD16)
#define SUPPORTED
#include <inttypes.h>
#endif				/* NETBSD */


    /*
     * OpenBSD looks like BSD/OS 3.x.
     */
#if defined(OPENBSD2) || defined (OPENBSD3)
#define SUPPORTED
#include <inttypes.h>
#endif



#if defined(DARWIN)
#define SUPPORTED
#include <inttypes.h>
#endif				/* DARWIN */


    /*
     * Linux 2.whatever. 
     */
#if defined(LINUX2)
#define SUPPORTED
#include <inttypes.h>
#endif				/* LINUX */



#if defined(CYGWIN)
#define SUPPORTED
#include <inttypes.h>

#define roundup(x, y)	\
	( ( ((x)+((y) - 1)) / (y)) * (y) )

#endif				/* CYGWIN */


#if defined(__INTERNIX)
#define SUPPORTED
#include <inttypes.h>

#define roundup(x, y)	\
	( ( ((x)+((y) - 1)) / (y)) * (y) )

#endif				/* INTERNIX */


    /*
     * Catch-all.
     */
#ifndef SUPPORTED
#error "This operating system is not supported"
#endif

#ifdef __cplusplus
}
#endif
#endif
