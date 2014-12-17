/*
 * endian.h - Endianness conversion header
 *
 * Copyright (C) 2007  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LIBDISARM_ENDIAN_H
#define _LIBDISARM_ENDIAN_H


#if defined(__FreeBSD__) && defined(HAVE_SYS_ENDIAN_H)
# include <sys/endian.h>
#else

# ifdef HAVE_CONFIG_H
#  include <config.h>
# endif

# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

# ifdef HAVE_BYTESWAP_H
#  include <byteswap.h>
#  define bswap16(x)  (bswap_16((x)))
#  define bswap32(x)  (bswap_32((x)))
#  define bswap64(x)  (bswap_64((x)))
# else /* ! HAVE_BYTESWAP_H */
#  define bswap16(x)  \
       ((((x) & 0x00ff) << 8) |  \
        (((x) & 0xff00) >> 8))
#  define bswap32(x)  \
       ((((x) & 0x000000ff) << 24) |  \
        (((x) & 0x0000ff00) <<  8) |  \
        (((x) & 0x00ff0000) >>  8) |  \
        (((x) & 0xff000000) >> 24))
#  define bswap64(x)  \
       ((((x) & 0x00000000000000ffull) << 56) |  \
        (((x) & 0x000000000000ff00ull) << 40) |  \
        (((x) & 0x0000000000ff0000ull) << 24) |  \
        (((x) & 0x00000000ff000000ull) <<  8) |  \
        (((x) & 0x000000ff00000000ull) >>  8) |  \
        (((x) & 0x0000ff0000000000ull) >> 24) |  \
        (((x) & 0x00ff000000000000ull) >> 40) |  \
        (((x) & 0xff00000000000000ull) >> 56))
# endif /* HAVE_BYTESWAP_H */

# ifdef WORDS_BIGENDIAN
#  define be16toh(x)  (x)
#  define be32toh(x)  (x)
#  define be64toh(x)  (x)
#  define le16toh(x)  bswap16((uint16_t)(x))
#  define le32toh(x)  bswap32((uint32_t)(x))
#  define le64toh(x)  bswap64((uint64_t)(x))
# else /* ! WORDS_BIGENDIAN */
#  define be16toh(x)  bswap16((uint16_t)(x))
#  define be32toh(x)  bswap32((uint32_t)(x))
#  define be64toh(x)  bswap64((uint64_t)(x)) 
#  define le16toh(x)  (x)
#  define le32toh(x)  (x)
#  define le64toh(x)  (x)
# endif /* WORDS_BIGENDIAN */

# define htobe16(x)  be16toh(x)
# define htobe32(x)  be32toh(x)
# define htobe64(x)  be64toh(x)
# define htole16(x)  le16toh(x)
# define htole32(x)  le32toh(x)
# define htole64(x)  le32toh(x)

#endif /* HAVE_SYS_ENDIAN_H */


#endif /* ! _LIBDISARM_ENDIAN_H */
