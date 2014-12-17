/*
 * EWF header section specification
 *
 * Copyright (c) 2006, Joachim Metz <forensics@hoffmannbv.nl>,
 * Hoffmann Investigations. All rights reserved.
 *
 * This code is derrived from information and software contributed by
 * - Expert Witness Compression Format specification by Andrew Rosen
 *   (http://www.arsdata.com/SMART/whitepaper.html)
 * - libevf from PyFlag by Michael Cohen
 *   (http://pyflag.sourceforge.net/)
 * - Open SSL for the implementation of the MD5 hash algorithm
 * - Wietse Venema for error handling code
 *
 * Additional credits go to
 * - Robert Jan Mora for testing and other contribution
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the creator, related organisations, nor the names of
 *   its contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * - All advertising materials mentioning features or use of this software
 *   must acknowledge the contribution by people stated above.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER, COMPANY AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _EWFHEADER_H
#define _EWFHEADER_H

#include <inttypes.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The header section
 * as long as necessary
 * character data compressed by zlib, contains a text string in the following format
 * ( spaces added for readability )
 *
 * 1                                                                                                                                \n
 * main                                                                                                                             \n
 * c           \t n               \t a                  \t e             \t t     \t m             \t u           \t p      \t r    \n
 * case number \t evidence number \t unique description \t examiner name \t notes \t acquired date \t system date \t pwhash \t char \n
 *
 * case number, evidence number, unique description, examiner name, and notes are free form strings (except for \t and \n)
 *
 * acquired date, and system date are in the form "2002 3 4 10 19 59", which is March 4, 2002 10:19:59
 *
 * pwhash the password hash should be the character '0' for no password
 *
 * char contains one of the following letters
 * b => best compression
 * f => fastest compression
 * n => no compression
 */

/* Header definition found in FTK Imager 2.3
 * A fifth line is present which is empty
 *
 * 1                                                                                                                                                       \n
 * main                                                                                                                                                    \n
 * c           \t n               \t a                  \t e             \t t     \t av      \t ov       \t m             \t u           \t p      \t r    \n
 * case number \t evidence number \t unique description \t examiner name \t notes \t version \t platform \t acquired date \t system date \t pwhash \t char \n
 *                                                                                                                                                         \n
 *
 * case number, evidence number, unique description, examiner name, and notes are free form strings (except for \t and \n)
 *
 * acquired date, and system date are in the form "2002 3 4 10 19 59", which is March 4, 2002 10:19:59
 *
 * version is the Encase version used to acquire the image
 *
 * platform is the operating system used to acquire the image
 *
 * pwhash the password hash should be the character '0' for no password
 *
 * char contains one of the following letters
 * b => best compression
 * f => fastest compression
 * n => no compression
 */

/* Header definition found in Encase 1, 2, 3
 * A fifth line is present which is empty
 *
 * 1                                                                                                                                                       \r\n
 * main                                                                                                                                                    \r\n
 * c           \t n               \t a                  \t e             \t t     \t av      \t ov       \t m             \t u           \t p      \t r    \r\n
 * case number \t evidence number \t unique description \t examiner name \t notes \t version \t platform \t acquired date \t system date \t pwhash \t char \r\n
 *                                                                                                                                                         \r\n
 *
 * case number, evidence number, unique description, examiner name, and notes are free form strings (except for \t and \n)
 *
 * acquired date, and system date are in the form "2002 3 4 10 19 59", which is March 4, 2002 10:19:59
 *
 * version is the Encase version used to acquire the image
 *
 * platform is the operating system used to acquire the image
 *
 * pwhash the password hash should be the character '0' for no password
 *
 * char contains one of the following letters
 * b => best compression
 * f => fastest compression
 * n => no compression
 */

/* Header definition found in Encase 4 and 5
 * A fifth line is present which is empty
 *
 * 1                                                                                                                                               \r\n
 * main                                                                                                                                            \r\n
 * c           \t n               \t a                  \t e             \t t     \t av      \t ov       \t m             \t u           \t p      \r\n
 * case number \t evidence number \t unique description \t examiner name \t notes \t version \t platform \t acquired date \t system date \t pwhash \r\n
 *                                                                                                                                                 \r\n
 *
 * case number, evidence number, unique description, examiner name, and notes are free form strings (except for \t and \n)
 *
 * acquired date, and system date are in the form "2002 3 4 10 19 59", which is March 4, 2002 10:19:59
 *
 * version is the Encase version used to acquire the image
 *
 * platform is the operating system used to acquire the image
 *
 * pwhash the password hash should be the character '0' for no password
 *
 */

#define EWF_HEADER uint8_t

EWF_HEADER *ewf_header_alloc( size_t size );
void ewf_header_free( EWF_HEADER *header );
EWF_HEADER *ewf_header_uncompress( EWF_HEADER *compressed_header, uint32_t *size );
EWF_HEADER *ewf_header_compress( EWF_HEADER *header, uint32_t *size, int8_t compression_level );
EWF_HEADER *ewf_header_read( int file_descriptor, size_t *size );
ssize_t ewf_header_write( EWF_HEADER *header, int file_descriptor, size_t size );
void ewf_header_fprint( FILE *stream, EWF_HEADER *header );

#ifdef __cplusplus
}
#endif

#endif

