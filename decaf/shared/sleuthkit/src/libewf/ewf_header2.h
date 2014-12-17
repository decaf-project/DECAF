/*
 * EWF header2 section specification
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

#ifndef _EWFHEADER2_H
#define _EWFHEADER2_H

#include <sys/types.h>
#include <stdio.h>

#include "ewf_header.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The header2 section
 * as long as necessary
 * character data compressed by zlib, contains a UTF16 text string in the following format
 * ( spaces added for readability )
 *
 */

/* Header2 found in Encase4
 *
 * 1                                                                                                                                               \n
 * main                                                                                                                                            \n
 * a                     c           \t n               \t e             \t t     \t av      \t ov       \t m             \t u           \t p      \n
 * unique description \t case number \t evidence number \t examiner name \t notes \t version \t platform \t acquired date \t system date \t pwhash \n
 *                                                                                                                                                 \n
 *
 * unique description, case number, evidence number, examiner name, and notes are free form strings (except for \t and \n)
 *
 * acquired date, and system date are in the form unix time stamp "1142163845", which is March 12 2006, 11:44:05
 *
 * version is the Encase version used to acquire the image
 *
 * platform is the operating system used to acquire the image
 *
 * pwhash the password hash should be the character '0' for no password
 *
 */

/* Header2 found in Encase5
 *
 * 3                                                                                                                                                       \n
 * main                                                                                                                                                    \n
 * c           \t n               \t a                  \t e             \t t     \t av      \t ov       \t m             \t u           \t p      \t dc   \n
 * case number \t evidence number \t unique description \t examiner name \t notes \t version \t platform \t acquired date \t system date \t pwhash \t ?    \n
 *                                                                                                                                                         \n
 * srce                                                                                                                                                    \n
 * 0       1                                                                                                                                               \n
 * p       n       id      ev      tb      lo      po      ah      gu      aq                                                                              \n
 * 0       0                                                                                                                                               \n
 *                                         -1      -1                                                                                                      \n
 *                                                                                                                                                         \n
 * sub                                                                                                                                                     \n
 * 0       1                                                                                                                                               \n
 * p       n       id      nu      co      gu                                                                                                              \n
 * 0       0                                                                                                                                               \n
 *                                 1                                                                                                                       \n
 *                                                                                                                                                         \n
 * unique description, case number, evidence number, examiner name, and notes are free form strings (except for \t and \n)
 *
 * acquired date, and system date are in the form unix time stamp "1142163845", which is March 12 2006, 11:44:05
 *
 * version is the Encase version used to acquire the image
 *
 * platform is the operating system used to acquire the image
 *
 * pwhash the password hash should be the character '0' for no password
 *
 * TODO the remaining values are currently unknown
 */

EWF_HEADER *ewf_header2_convert_utf16_to_ascii( EWF_HEADER *utf16_header, size_t size_utf16 );
EWF_HEADER *ewf_header2_convert_ascii_to_utf16( EWF_HEADER *ascii_header, size_t size_ascii );
EWF_HEADER *ewf_header2_read( int file_descriptor, size_t size );

#ifdef __cplusplus
}
#endif

#endif

