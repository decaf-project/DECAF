/* macros.h */

#ifndef _LIBDISARM_MACROS_H
#define _LIBDISARM_MACROS_H


/* Declarations for header files */
#ifdef __cplusplus
# define DA_BEGIN_DECLS  extern "C" {
# define DA_END_DECLS  }
#else
# define DA_BEGIN_DECLS
# define DA_END_DECLS
#endif

/* API macros */
#if __GNUC__ >= 4
# define DA_API  __attribute__ ((__visibility__("default")))
#else
# define DA_API
#endif


#endif /* ! _LIBDISARM_MACROS_H */
