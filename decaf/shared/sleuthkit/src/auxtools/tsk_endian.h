/*
 * The Sleuth Kit
 * 
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 */
#ifndef _TSK_ENDIAN_H
#define _TSK_ENDIAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* 
** Dealing with endian differences
*/

#define TSK_LIT_ENDIAN	0x01
#define TSK_BIG_ENDIAN	0x02

/* macros to read in multi-byte fields
 * file system is an array of 8-bit values, not 32-bit values
 */
    extern uint8_t guess_end_u16(uint8_t *, uint8_t *, uint16_t);
    extern uint8_t guess_end_u32(uint8_t *, uint8_t *, uint32_t);

/* 16-bit values */
#define getu16(foo, x)   \
    (uint16_t)(((foo)->endian & TSK_LIT_ENDIAN) ? \
	  (((uint8_t *)(x))[0] + (((uint8_t *)(x))[1] << 8)) :    \
	  (((uint8_t *)(x))[1] + (((uint8_t *)(x))[0] << 8)) )

#define gets16(foo, x)	\
	((int16_t)getu16(foo, x))

/* 32-bit values */
#define getu32(foo, x)	\
	(uint32_t)( ((foo)->endian & TSK_LIT_ENDIAN)  ?	\
     ((((uint8_t *)(x))[0] <<  0) + \
	  (((uint8_t *)(x))[1] <<  8) + \
	  (((uint8_t *)(x))[2] << 16) + \
	  (((uint8_t *)(x))[3] << 24) ) \
	:	\
	 ((((uint8_t *)(x))[3] <<  0) + \
	  (((uint8_t *)(x))[2] <<  8) + \
	  (((uint8_t *)(x))[1] << 16) + \
	  (((uint8_t *)(x))[0] << 24) ) )

#define gets32(foo, x)	\
	((int32_t)getu32(foo, x))

#define getu48(foo, x)   \
	(uint64_t)( ((foo)->endian & TSK_LIT_ENDIAN)  ?	\
      ((uint64_t) \
	  ((uint64_t)((uint8_t *)(x))[0] <<  0)+ \
	  ((uint64_t)((uint8_t *)(x))[1] <<  8) + \
      ((uint64_t)((uint8_t *)(x))[2] << 16) + \
	  ((uint64_t)((uint8_t *)(x))[3] << 24) + \
      ((uint64_t)((uint8_t *)(x))[4] << 32) + \
      ((uint64_t)((uint8_t *)(x))[5] << 40)) \
	: \
      ((uint64_t) \
	  ((uint64_t)((uint8_t *)(x))[5] <<  0)+ \
	  ((uint64_t)((uint8_t *)(x))[4] <<  8) + \
      ((uint64_t)((uint8_t *)(x))[3] << 16) + \
	  ((uint64_t)((uint8_t *)(x))[2] << 24) + \
      ((uint64_t)((uint8_t *)(x))[1] << 32) + \
      ((uint64_t)((uint8_t *)(x))[0] << 40)) )


#define getu64(foo, x)   \
	(uint64_t)( ((foo)->endian & TSK_LIT_ENDIAN)  ?	\
      ((uint64_t) \
	  ((uint64_t)((uint8_t *)(x))[0] << 0)  + \
	  ((uint64_t)((uint8_t *)(x))[1] << 8) + \
      ((uint64_t)((uint8_t *)(x))[2] << 16) + \
	  ((uint64_t)((uint8_t *)(x))[3] << 24) + \
      ((uint64_t)((uint8_t *)(x))[4] << 32) + \
      ((uint64_t)((uint8_t *)(x))[5] << 40) + \
      ((uint64_t)((uint8_t *)(x))[6] << 48) + \
      ((uint64_t)((uint8_t *)(x))[7] << 56)) \
	: \
      ((uint64_t) \
	  ((uint64_t)((uint8_t *)(x))[7] <<  0) + \
	  ((uint64_t)((uint8_t *)(x))[6] <<  8) + \
      ((uint64_t)((uint8_t *)(x))[5] << 16) + \
	  ((uint64_t)((uint8_t *)(x))[4] << 24) + \
      ((uint64_t)((uint8_t *)(x))[3] << 32) + \
      ((uint64_t)((uint8_t *)(x))[2] << 40) + \
      ((uint64_t)((uint8_t *)(x))[1] << 48) + \
      ((uint64_t)((uint8_t *)(x))[0] << 56)) )

#define gets64(foo, x)	\
	((int64_t)getu64(foo, x))

#ifdef __cplusplus
}
#endif
#endif
