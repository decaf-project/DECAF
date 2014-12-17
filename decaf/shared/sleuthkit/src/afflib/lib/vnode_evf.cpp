/**
 ** EnCase Evidence File Implementation.
 **
 ** (C) 2006 by David J. Malan
 **
 ** Additional inputs by Simson L. Garfinkel, 2005.
 **
 **
 **/



#include "afflib.h"
#include "afflib_i.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vnode_evf.h"
#include "zlib.h"

static unsigned char EVF_SIGNATURE[] = {'E', 'V', 'F', 0x09, 0x0d, 0x0a, 0xff, 0x00};
static bool verbose = FALSE;
static uint32 CBIT = (uint32) 1 << 31;

////////////////////////////////////////////////////////////////////////////////
////// vnode_evf.c
//////
////// v. 1.0
//////
////// David J. Malan
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//// libraries
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// ConversionResult
// ConvertUTF16toUTF8
// (const UTF16** sourceStart, const UTF16* sourceEnd, 
//  UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags)
//
// Converts UTF-16 value between [sourceStart, sourceEnd) to
// UTF-8, storing result between [targetStart, targetEnd)
// per specified flags.
//
// http://www.unicode.org/Public/PROGRAMS/CVTUTF/ConvertUTF.{c,h}
////////////////////////////////////////////////////////////////////////////////

// Unicode constants
// http://www.unicode.org/Public/PROGRAMS/CVTUTF/ConvertUTF.{c,h}
#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF
static const UTF8 firstByteMark[7]  = {0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc};
static const UTF32 halfBase = 0x0010000UL;
static const int halfShift  = 10;

static ConversionResult ConvertUTF16toUTF8(const UTF16** sourceStart, const UTF16* sourceEnd,
					   UTF8** targetStart, UTF8* targetEnd,
					   ConversionFlags flags) 
{
    ConversionResult result = conversionOK;
    const UTF16* source = *sourceStart;
    UTF8* target = *targetStart;
    while (source < sourceEnd) {
    UTF32 ch;
    unsigned short bytesToWrite = 0;
    const UTF32 byteMask = 0xBF;
    const UTF32 byteMark = 0x80;
    const UTF16* oldSource = source; /* In case we have to back up because of target overflow. */
    ch = *source++;
    /* If we have a surrogate pair, convert to UTF32 first. */
    if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
        /* If the 16 bits following the high surrogate are in the source buffer... */
        if (source < sourceEnd) {
        UTF32 ch2 = *source;
        /* If it's a low surrogate, convert to UTF32. */
        if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
            ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
            + (ch2 - UNI_SUR_LOW_START) + halfBase;
            ++source;
        } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
            --source; /* return to the illegal value itself */
            result = sourceIllegal;
            break;
        }
        } else { /* We don't have the 16 bits following the high surrogate. */
        --source; /* return to the high surrogate */
        result = sourceExhausted;
        break;
        }
    } else if (flags == strictConversion) {
        /* UTF-16 surrogate values are illegal in UTF-32 */
        if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
        --source; /* return to the illegal value itself */
        result = sourceIllegal;
        break;
        }
    }
    /* Figure out how many bytes the result will require */
    if (ch < (UTF32)0x80) {      bytesToWrite = 1;
    } else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
    } else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
    } else if (ch < (UTF32)0x110000) {  bytesToWrite = 4;
    } else {                bytesToWrite = 3;
                        ch = UNI_REPLACEMENT_CHAR;
    }

    target += bytesToWrite;
    if (target > targetEnd) {
        source = oldSource; /* Back up source pointer! */
        target -= bytesToWrite; result = targetExhausted; break;
    }
    switch (bytesToWrite) { /* note: everything falls through. */
        case 4: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
        case 3: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
        case 2: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
        case 1: *--target =  (UTF8)(ch | firstByteMark[bytesToWrite]);
    }
    target += bytesToWrite;
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}


////////////////////////////////////////////////////////////////////////////////
// unsigned int
// Expert_Witness_Compression_crc(void *buffer, int size, unsigned int prevkey)
//
// "The following function returns the 4-byte CRC value used in the Expert 
// Witness Compression format. When calling the function to start a new CRC, 
// the third argument (prevkey) should be '1'."
//
// http://www.asrdata.com/SMART/whitepaper.html
////////////////////////////////////////////////////////////////////////////////

static uint32 Expert_Witness_Compression_crc(void *buffer, int size, unsigned int prevkey)
{
    // index variable
    int i;

    // casted buffer
    unsigned char *cbuffer = (unsigned char *) buffer; 

    // temporary variables
    unsigned int b = prevkey & 0xffff;
    unsigned int d = (prevkey >> 16) & 0xffff;

    // compute CRC
    for (i = 0; i < size; i++) 
    {
        b += cbuffer[i];
        d += b;
        if (i != 0 && ((i % 0x15b0 == 0) || (i == size - 1))) 
        {
            b = b % 0xfff1;
            d = d % 0xfff1;
        }
    }

    // return CRC
    return (d << 16) | b;
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_uchars(unsigned char *buffer, uint32 n, FILE * f)
// 
// Reads n unsigned chars from f into buffer, returning TRUE upon success
// else FALSE.
////////////////////////////////////////////////////////////////////////////////

static bool fread_uchars(unsigned char *buffer, uint32 n, FILE * f)
{
    return (fread(buffer, n, 1, f) == 1);
}

static bool fread_chars(char *buffer, uint32 n, FILE * f)
{
    return (fread(buffer, n, 1, f) == 1);
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_uint8(uint8 *i, FILE *f)
//
// Reads an 8-bit, unsigned integer from f into i, returning TRUE upon 
// success else FALSE.
////////////////////////////////////////////////////////////////////////////////

static bool fread_uint8(uint8 *i, FILE *f)
{
    return (fread(i, 1, 1, f) == 1);
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_uint16(uint16 *i, FILE *f)
//
// Reads a 16-bit, unsigned, little-endian integer from f into i (where
// it's stored using the host's byte order), returning TRUE upon success
// else FALSE.
////////////////////////////////////////////////////////////////////////////////

static bool fread_uint16(uint16 *i, FILE *f)
{
    *i  = (uint16) (fgetc(f) & 0xff);
    *i |= (uint16) (fgetc(f) & 0xff) << 0x08;
    return (!feof(f) && !ferror(f));
}

// converts a 16-bit, unsigned, little-endian integer, b, to host's byte order
static int read_uint16(const unsigned char buf[2]) 
{
    return (uint16) ((buf[0] & 0xff) | ((buf[1] & 0xff) << 0x08));
}




////////////////////////////////////////////////////////////////////////////////
// bool
// fread_uint32(uint32 *i, FILE *f)
//
// Reads a 32-bit, unsigned, little-endian integer from f into i (where
// it's stored using the host's byte order), returning TRUE upon success
// else FALSE.
////////////////////////////////////////////////////////////////////////////////

static uint32 read_uint_32(unsigned char buf[4])
{
    return buf[0] | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);
}

static bool
fread_uint32(uint32 *i, FILE *f)
{
    unsigned char buf[4];
    if(fread(buf,1,4,f)!=4) return false;
    *i = read_uint_32(buf);
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_uint64(uint64 *i, FILE *f)
//
// Reads a 64-bit, unsigned, little-endian integer from f into i (where
// it's stored using the host's byte order, returning TRUE upon success
// else FALSE.
////////////////////////////////////////////////////////////////////////////////

bool
fread_uint64(uint64 *i, FILE *f)
{
    *i  = (uint64) (fgetc(f) & 0xff);
    *i |= (uint64) (fgetc(f) & 0xff) << 0x08;
    *i |= (uint64) (fgetc(f) & 0xff) << 0x10;
    *i |= (uint64) (fgetc(f) & 0xff) << 0x18;
    *i |= (uint64) (fgetc(f) & 0xff) << 0x20;
    *i |= (uint64) (fgetc(f) & 0xff) << 0x28;
    *i |= (uint64) (fgetc(f) & 0xff) << 0x30;
    *i |= (uint64) (fgetc(f) & 0xff) << 0x38;
    return (!feof(f) && !ferror(f));
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_evf_file_header(evf_file_header *file_header, FILE *f)
//
// Reads an EVF file header from f into file_header (where integers
// are stored using host's byte order), returning TRUE upon success
// else FALSE.
//
// See vnode_evf.h for the structure of an evf_file_header structure.
////////////////////////////////////////////////////////////////////////////////

bool
fread_evf_file_header(evf_file_header *file_header, FILE *f)
{
    return (fread_uchars(file_header->signature,
                         sizeof(file_header->signature), f) &&
            fread_uint8(&file_header->one, f) &&
            fread_uint16(&file_header->segment_number, f) &&
            fread_uint16(&file_header->zero, f));
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_comments
// (unsigned char **comments, uint64 *comments_size, FILE *f, uint64 n)
//
// Reads a deflated comments structure of n bytes from f and inflates it,
// storing the inflated comments' starting address in *comments and the
// inflated comments' size in *comments_size, returning TRUE upon success
// else FALSE.
//
// See vnode_evf.h for the structure of a comments structure.
////////////////////////////////////////////////////////////////////////////////

bool
fread_comments(unsigned char **comments, uint64 *comments_size, FILE *f, uint64 n)
{
    // comments
    unsigned char *ccomments;

    // return value
    int result;

    // stream for inflate
    z_stream strm;

    // read deflated comments
    ccomments = (unsigned char *)calloc(n,1);
    if (ccomments == NULL || fread(ccomments, n, 1, f) != 1)
    {
        fprintf(stderr, "error reading comments\n");
        goto error;
    }   
            
    // allocate space for inflated comments, 
    // preparing for a 1:2 inflation ratio
    *comments_size = 2 * n * sizeof(unsigned char);
    *comments = (unsigned char *)calloc(1,*comments_size);
    if (*comments == NULL) {
        fprintf(stderr, "error allocating memory for comments\n");
        goto error;
    }   
            
    // prepare state for inflate
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = n;
    strm.next_in = ccomments;
    if (inflateInit(&strm) != Z_OK)
    {
        fprintf(stderr, "error allocating inflate state\n");
        goto error;
    }
    strm.avail_out = *comments_size;
    strm.next_out = *comments;

    // inflate comments
    do
    {
        // inflate a buffer's worth of comments
        result = inflate(&strm, Z_SYNC_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END)
        {
            (void) inflateEnd(&strm);
            fprintf(stderr, "error decompressing comments\n");
            goto error;
        }

        // allocate additional memory for comments as needed
        if (result == Z_OK && strm.avail_out == 0)
        {
            // double our buffer's size
            strm.avail_out = *comments_size;
            *comments_size *= 2;
	    *comments = (unsigned char *)realloc(*comments, *comments_size);
            if (*comments == NULL)
            {
                fprintf(stderr, "error allocating memory for comments\n");
                goto error;
            }

            // don't clobber comments already inflated
            strm.next_out = *comments + strm.avail_out;
        }
    }
    while (result != Z_STREAM_END);

    // tidy up
    return TRUE;
error:
    if (*comments != NULL)
        free(*comments);
    if (ccomments != NULL)
        free(ccomments);
    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_evf_header_section
// (evf_header_section *header_section, FILE *f, uint64 n)
//
// Reads an EVF header section of n bytes from f into header_section,
// returning TRUE upon success else FALSE.
//
// See vnode_evf.h for the structure of an evf_header_section structure.
////////////////////////////////////////////////////////////////////////////////

bool
fread_evf_header_section
(evf_header_section *header_section, FILE *f, uint64 n)
{
    // comments
    unsigned char *comments = NULL;

    // inflated comments' size
    uint64 comments_size = 0;

    // comments' keys and values
    unsigned char *eok, *eov;
    unsigned char *sok, *sov;
    unsigned char *keys, *values;
    char *key, *value;			// really UTF8

    // read comments
    if (!fread_comments(&comments, &comments_size, f, n))
    {
        fprintf(stderr, "error reading comments\n");
        return FALSE;
    }

    // seek to keys in comments' third row
    keys = comments;
    while (keys < comments + comments_size && *keys != '\n')
        keys++;
    keys++;
    while (keys < comments + comments_size && *keys != '\n')
        keys++;
    keys++;
    if (keys >= comments + comments_size)
    {
        fprintf(stderr, "error parsing comments' keys\n");
        return FALSE;
    }

    // seek to values in comments' fourth row
    values = keys;
    while (values < comments + comments_size && *values != '\n')
        values++;
    values++;
    if (values >= comments + comments_size)
    {
        fprintf(stderr, "error parsing comments' values\n");
        return FALSE;
    }

    // iterate over comments' keys and values
    for (sok = eok = keys, sov = eov = values;
         sok < values && sov < comments + comments_size;
         sok = eok, sov = eov)
    {
        // seek to end of key
        while (*eok != '\t' && *eok != '\r') eok++;
    
        // seek to end of value
        while (*eov != '\t' && *eov != '\r') eov++;

        // copy comment
        key = (char *)malloc((eok - sok) * sizeof(UTF8) + 1);
        if (key == NULL)
        {
            fprintf(stderr, "error storing comment\n");
            return FALSE;
        }
        value = (char *)malloc((eov - sov) * sizeof(UTF8) + 1);
        if (value == NULL)
        {
            free(key);
            fprintf(stderr, "error storing comment\n");
            return FALSE;
        }
        strncpy(key, (const char *)sok, eok - sok);
        key[eok-sok] = '\0';
        strncpy((char *)value, (const char *)sov, eov - sov);
        value[eov-sov] = '\0';
    
        // store comment, if recognized
        if (!strcmp(key, "a"))
            header_section->comments.a = (UTF8 *)value;
        else if (!strcmp(key, "av"))
            header_section->comments.av = (UTF8 *)value;
        else if (!strcmp(key, "c"))
            header_section->comments.c = (UTF8 *)value;
        else if (!strcmp(key, "e"))
            header_section->comments.e = (UTF8 *)value;
        else if (!strcmp(key, "m"))
            header_section->comments.m = (UTF8 *)value;
        else if (!strcmp(key, "n"))
            header_section->comments.n = (UTF8 *)value;
        else if (!strcmp(key, "ov"))
            header_section->comments.ov = (UTF8 *)value;
        else if (!strcmp(key, "p"))
            header_section->comments.p = (UTF8 *)value;
        else if (!strcmp(key, "t"))
            header_section->comments.t = (UTF8 *)value;
        else if (!strcmp(key, "u"))
            header_section->comments.u = (UTF8 *)value;
        else
        {
            fprintf(stderr, "unknown comment encountered: %s\n", key);
            free(key);
            free(value);
        }

        // seek to start of next key
        // (which is 2 bytes away if we've hit a CRLF, else just 1 away)
        eok += (*eok == '\r') ? 2 : 1;
    
        // seek to start of next value
        // (which is 2 bytes away if we've hit a CRLF, else just 1 away)
        eov += (*eov == '\r') ? 2 : 1;
        
    }

    // phew, done
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_evf_header2_section
// (evf_header2_section *header2_section, FILE *f, uint64 n)
//
// Reads an EVF header2 section of n bytes from f into header2_section,
// returning TRUE upon success else FALSE.
//
// See vnode_evf.h for the structure of an evf_header2_section structure.
////////////////////////////////////////////////////////////////////////////////

bool
fread_evf_header2_section
(evf_header2_section *header2_section, FILE *f, uint64 n)
{
    // comments
    unsigned char *comments = NULL;

    // inflated comments' size
    uint64 comments_size = 0;

    // comments' keys and values
    unsigned char *eok, *eov;
    unsigned char *sok, *sov;
    unsigned char *keys, *values;
    UTF8 *key, *value;

    // Unicode buffers
    UTF8 *utf8, *utf8_start;
    UTF16 *utf16;
    const UTF16 *utf16_start;

    // read comments
    if (!fread_comments(&comments, &comments_size, f, n))
    {
        fprintf(stderr, "error reading comments\n");
        return FALSE;
    }

    // seek to keys in comments' third row
    keys = comments;
    while (keys < comments + comments_size && read_uint16(keys) != '\n')
         keys += sizeof(uint16);
    keys += sizeof(uint16);
    while (keys < comments + comments_size && read_uint16(keys) != '\n')
         keys += sizeof(uint16);
    keys += sizeof(uint16);
    if (keys >= comments + comments_size)
    {
        fprintf(stderr, "error parsing comments' keys\n");
        return FALSE;
    }

    // seek to values in comments' fourth row
    values = keys;
    while (values < comments + comments_size && read_uint16(values) != '\n')
         values += sizeof(uint16);
    values += sizeof(uint16);
    if (values >= comments + comments_size)
    {
        fprintf(stderr, "error parsing comments' values\n");
        return FALSE;
    }

    // allocate memory for Unicode buffers
    if ((utf8 = (UTF8 *)malloc(sizeof(UTF8))) == NULL)
    {
        fprintf(stderr, "error allocating memory for comment\n");
        return FALSE;
    }
    if ((utf16 = (UTF16 *)malloc(sizeof(UTF16))) == NULL)
    {
        free(utf8);
        fprintf(stderr, "error allocating memory for comment\n");
        return FALSE;
    }

    // iterate over comments' keys and values
    for (sok = eok = keys, sov = eov = values;
         sok < values && sov < comments + comments_size;
         sok = eok, sov = eov)
    {
        // seek to end of key
        while (read_uint16(eok) != '\t' && read_uint16(eok) != '\n') 
            eok += sizeof(uint16);
    
        // seek to end of value
        while (read_uint16(eov) != '\t' && read_uint16(eov) != '\n') 
            eov += sizeof(uint16);

        // copy comment
        if ((key = (UTF8 *)malloc((eok - sok) * sizeof(UTF8) + sizeof(UTF8))) == NULL)
        {
            free(utf8);
            free(utf16);
            fprintf(stderr, "error storing comment\n");
            return FALSE;
        }
        if ((value = (UTF8 *)malloc((eov - sov) * sizeof(UTF8) + sizeof(UTF8))) == NULL)
        {
            free(utf8);
            free(utf16);
            free(key);
            fprintf(stderr, "error storing comment\n");
            return FALSE;
        }
	int i;
        for (i = 0; i < eok - sok; i += sizeof(uint16)) {
            utf8_start = utf8;
            utf16_start = utf16;
            *utf16 = read_uint16((sok + i));
            if (ConvertUTF16toUTF8(&utf16_start, utf16 + 1,
                                   &utf8_start, utf8 + 1,
                                   lenientConversion) != conversionOK)
            {
                fprintf(stderr, "error converting UTF-16 comment to UTF-8\n");
                return FALSE;
            }
            *(key + i/sizeof(uint16)) = *utf8;
        }
        *(key + i/sizeof(uint16)) = 0x00;

        for (i = 0; i < eov - sov; i += sizeof(uint16)) {
            utf8_start = utf8;
            utf16_start = utf16;
            *utf16 = read_uint16((sov + i));
            if (ConvertUTF16toUTF8(&utf16_start, utf16 + 1,
                                   &utf8_start, utf8 + 1,
                                   lenientConversion) != conversionOK)
            {
                fprintf(stderr, "error converting UTF-16 comment to UTF-8\n");
                return FALSE;
            }
            *(value + i/sizeof(uint16)) = *utf8;
        }
        *(value + i/sizeof(uint16)) = 0x00;

        // store comment, if recognized
        if (!strcmp((const char *)key, "a")) header2_section->comments.a = value;
        else if (!strcmp((const char *)key, "av")) header2_section->comments.av = value;
        else if (!strcmp((const char *)key, "c")) header2_section->comments.c = value;
        else if (!strcmp((const char *)key, "dc")) header2_section->comments.dc = value;
        else if (!strcmp((const char *)key, "e")) header2_section->comments.e = value;
        else if (!strcmp((const char *)key, "m")) header2_section->comments.m = value;
        else if (!strcmp((const char *)key, "n")) header2_section->comments.n = value;
        else if (!strcmp((const char *)key, "ov")) header2_section->comments.ov = value;
        else if (!strcmp((const char *)key, "p")) header2_section->comments.p = value;
        else if (!strcmp((const char *)key, "t")) header2_section->comments.t = value;
        else if (!strcmp((const char *)key, "u")) header2_section->comments.u = value;
        else
        {
            fprintf(stderr, "unknown comment encountered: %s\n", key);
            free(value);
        }
        free(key);

        // seek to start of next key
        eok += sizeof(uint16);
    
        // seek to start of next value
        eov += sizeof(uint16);
    }

    // phew, done
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_evf_section_header(evf_section_header *section_header, FILE *f)
//
// Reads an EVF section header from f into section_header (where integers
// are stored using host's byte order), returning TRUE upon success 
// else FALSE.
//
// See vnode_evf.h for the structure of an evf_section_header structure.
////////////////////////////////////////////////////////////////////////////////

bool
fread_evf_section_header(evf_section_header *section_header, FILE *f)
{
    return (fread_chars(section_header->type, sizeof(section_header->type), f) &&
            fread_uint64(&section_header->next, f) && 
            fread_uint64(&section_header->size , f) &&
            fread_chars(section_header->padding, sizeof(section_header->padding), f) &&
            fread_uint32(&section_header->crc, f));
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_evf_volume_section(evf_volume_section *volume_section, FILE *f)
//
// Reads an EVF volume header from f into volume_header (where integers
// are stored using host's byte order), returning TRUE upon success
// else FALSE.
//
// See vnode_evf.h for the structure of an evf_volume_section structure.
////////////////////////////////////////////////////////////////////////////////

bool
fread_evf_volume_section(evf_volume_section *volume_section, FILE *f)
{
    return (fread_uint32(&volume_section->reserved, f) &&
            fread_uint32(&volume_section->chunk_count, f) &&
            fread_uint32(&volume_section->sectors_per_chunk, f) &&
            fread_uint32(&volume_section->bytes_per_sector, f) &&
            fread_uint32(&volume_section->sector_count, f) &&
            fread_uchars(volume_section->reserved2, 
                         sizeof(volume_section->reserved2), f) && 
            fread_uchars(volume_section->padding,
                         sizeof(volume_section->padding), f) &&
            fread_uchars(volume_section->signature,
                         sizeof(volume_section->signature), f) &&
            fread_uint32(&volume_section->crc, f));
}


////////////////////////////////////////////////////////////////////////////////
// bool
// fread_evf_table_section(evf_table_section *table_section, FILE *f)
//
// Reads an EVF table section from f into table_header (where integers
// are stored using host's byte order), returning TRUE upon success
// else FALSE.
//
// See vnode_evf.h for the structure of an evf_table_section structure.
////////////////////////////////////////////////////////////////////////////////

bool
fread_evf_table_section(evf_table_section *table_section, FILE *f)
{
    // index variable
    unsigned int i;

    // read fixed-size fields
    bool result = fread_uint32(&table_section->chunk_count, f) &&
                  fread_uchars(table_section->padding, 
                               sizeof(table_section->padding), f) &&
                  fread_uint32(&table_section->crc, f);

    // short-circuit if there's a problem
    if (!result)
        return FALSE;

    // allocate offset array (with one extra entry so that
    // we can determine size of last chunk)
    table_section->offsets = (uint32 *)calloc((table_section->chunk_count + 1),sizeof(uint32));

    // short-circuit if there's a problem
    if (table_section->offsets == NULL)
        return FALSE;

    // populate offset array
    for (i = 0; i < table_section->chunk_count; i++)
        result &= fread_uint32(&table_section->offsets[i], f);

    // not sure what this is
    result &= fread_uint32(&table_section->unknown, f);

    // all done
    return result;
}


////////////////////////////////////////////////////////////////////////////////
// inline
// bool
// check_cbit(uint32 n)
// 
// Returns TRUE if highest-ordered bit of n is 1 (thereby signifying that
// a chunk is compressed) else FALSE.
////////////////////////////////////////////////////////////////////////////////

inline
bool
check_cbit(uint32 n)
{
    return (n & CBIT) ? TRUE : FALSE;
}


////////////////////////////////////////////////////////////////////////////////
// inline
// bool
// clear_cbit(uint32 n)
// 
// Returns n with its highest-order bit cleared (so as not to mistake 
// compressed chunks' offsets as being 2 gigabytes larger than they
// should be).
////////////////////////////////////////////////////////////////////////////////

inline
uint32
clear_cbit(uint32 n)
{
    return (n & (CBIT ^ 0xffffffff));
}


////////////////////////////////////////////////////////////////////////////////
////// EOF
////////////////////////////////////////////////////////////////////////////////


/****************************************************************
 *** AFFLIB Glue Follows
 ****************************************************************/


/* Return 1 if a file is a evf file... */
static int evf_identify_file(const char *filename)
{
    FILE *f = fopen(filename,"r");
    if(f){
	unsigned char buf[8];
	if(fread(buf,1,8,f)==8){
	    if(memcmp(buf,EVF_SIGNATURE,8)==0){
		fclose(f);
		return 1;		// signature matches
	    }
	}
	fclose(f);
	return 0;
    }
    /* I can't open the file, so it must not be an EVF file */
    return 0;
}


/* Return the size of the evf file */
static int64 evf_filesize(AFFILE *af)
{
    struct evf_private *ep = EVF_PRIVATE(af);
    return ep->table_section->chunk_count * ep->chunk_size; //  last chunk?
}

static int evf_close(AFFILE *af)
{
    struct evf_private *ep = EVF_PRIVATE(af);

    if(ep){
	if(ep->header2_section){
	    if (ep->header2_section->comments.a) free(ep->header2_section->comments.a);
	    if (ep->header2_section->comments.av) free(ep->header2_section->comments.av);
	    if (ep->header2_section->comments.c) free(ep->header2_section->comments.c);
	    if (ep->header2_section->comments.dc) free(ep->header2_section->comments.dc);
	    if (ep->header2_section->comments.e) free(ep->header2_section->comments.e);
	    if (ep->header2_section->comments.m) free(ep->header2_section->comments.m);
	    if (ep->header2_section->comments.n) free(ep->header2_section->comments.n);
	    if (ep->header2_section->comments.ov) free(ep->header2_section->comments.ov);
	    if (ep->header2_section->comments.p) free(ep->header2_section->comments.p);
	    if (ep->header2_section->comments.t) free(ep->header2_section->comments.t);
	    if (ep->header2_section->comments.u) free(ep->header2_section->comments.u);
	    free(ep->header2_section);
	}
	if(ep->header_section){
	    if (ep->header_section->comments.a) free(ep->header_section->comments.a);
	    if (ep->header_section->comments.av) free(ep->header_section->comments.av);
	    if (ep->header_section->comments.c) free(ep->header_section->comments.c);
	    if (ep->header_section->comments.dc) free(ep->header_section->comments.dc);
	    if (ep->header_section->comments.e) free(ep->header_section->comments.e);
	    if (ep->header_section->comments.m) free(ep->header_section->comments.m);
	    if (ep->header_section->comments.n) free(ep->header_section->comments.n);
	    if (ep->header_section->comments.ov) free(ep->header_section->comments.ov);
	    if (ep->header_section->comments.p) free(ep->header_section->comments.p);
	    if (ep->header_section->comments.t) free(ep->header_section->comments.t);
	    if (ep->header_section->comments.u) free(ep->header_section->comments.u);
	    free(ep->header_section);
	}
	if(ep->volume_section)  free(ep->volume_section);
	if(ep->table_section){
	    if (ep->table_section->offsets) free(ep->table_section->offsets);
	    free(ep->table_section);
	}

	if (ep->finfile && fclose(ep->finfile) != 0){
	    (*af->error_reporter)("error closing infile: %s", af_filename(af));
	}

	memset(ep,0,sizeof(*ep));		// clean object reuse
	free(ep);				// won't need it again
    }
    return 0;
}


static int evf_open(AFFILE *af)
{
    /* Allocate our private area */
    af->vnodeprivate = (void *)calloc(1,sizeof(struct evf_private));
    struct evf_private *ep = EVF_PRIVATE(af);

    /* Open an EnCase file (just incase we get one). */
    ep->finfile = fopen(af->fname,"rb");
    if (verbose){
        fprintf(stderr, "Opened %s in binary mode for reading.\n", af->fname);
    }

    // read file header
    if (!fread_evf_file_header(&ep->file_header, ep->finfile))  {
        if(verbose) fprintf(stderr, "error reading infile: %s\n", af->fname);
	evf_close(af);			// cleanup what we can
        return -1;
    }

    if (verbose) {
        fprintf(stderr, "signature: %s\n", ep->file_header.signature);
        fprintf(stderr, "one: %u\n", ep->file_header.one);
        fprintf(stderr, "segment_number: %u\n", ep->file_header.segment_number);
        fprintf(stderr, "zero: %u\n", ep->file_header.zero);
    }

    // check signature
    if (memcmp((const unsigned char *)ep->file_header.signature, EVF_SIGNATURE, sizeof(EVF_SIGNATURE)))    {
        (*af->error_reporter)("invalid signature in evidence infile");
	evf_close(af);
	return -1;
    }


    // iterate over file's sections and copy out the metadata
    int eof = FALSE;
    uint32 sectors_section_end = 0;
    while (!eof) {
	evf_section_header section_header;
	memset(&section_header,0,sizeof(section_header));

        // read a section header
        if (!fread_evf_section_header(&section_header, ep->finfile)) {
            fprintf(stderr,"error reading section header\n");
	    evf_close(af);
	    return -1;
        }

        // verbosity
        if (verbose) {
            fprintf(stderr, 
                    "Section's type is \"%s\".\n", section_header.type);
            fprintf(stderr, 
                    "Section's size is %llu bytes.\n", section_header.size);
        }

        // process section: header2
        if (strcmp("header2", section_header.type) == 0)
        {
            // read header2 section, unless we've read one already
            if (ep->header2_section == NULL)
            {
                ep->header2_section = (evf_header2_section *)calloc(1,sizeof(evf_header2_section));
                if (ep->header2_section == NULL)
                {
                    fprintf(stderr, "out of memory allocating header2 section\n");
		    evf_close(af);
		    return -1;
                }
                if (!fread_evf_header2_section(ep->header2_section, ep->finfile, 
                                               section_header.size - 
                                                sizeof(section_header)))
                {
                    fprintf(stderr, "error reading header2 section\n");
		    evf_close(af);
		    return -1;
                }
            }
        }

        // process section: header
        else if (strcmp("header", section_header.type) == 0)
        {
            // read header section
            ep->header_section = (evf_header_section *)calloc(1,sizeof(*ep->header_section));
            if (ep->header_section == NULL)
            {
                fprintf(stderr, "error allocating evf_header_section section\n");
		evf_close(af);
		return -1;
            }
            if (!fread_evf_header_section(ep->header_section, ep->finfile, 
                                          section_header.size - 
                                           sizeof(section_header)))
            {
                fprintf(stderr, "error reading header section\n");
		evf_close(af);
		return -1;
            }
        }

        // process section: volume
        else if (strcmp("volume", section_header.type) == 0)
        {
            // read volume section
            ep->volume_section = (evf_volume_section *)calloc(1,sizeof(*ep->volume_section));
            if (ep->volume_section == NULL)
            {
                fprintf(stderr, "error allocating volume section\n");
		evf_close(af);
		return -1;
            }
            if (!fread_evf_volume_section(ep->volume_section, ep->finfile)) {
                fprintf(stderr, "error reading volume section\n");
		evf_close(af);
		return -1;
            }

            // verbosity
            if (verbose) {
                fprintf(stderr, "chunk_count=%u\n", 
                        ep->volume_section->chunk_count);
                fprintf(stderr, "sectors_per_chunk=%u\n", 
                        ep->volume_section->sectors_per_chunk);
                fprintf(stderr, "bytes_per_sector=%u\n", 
                        ep->volume_section->bytes_per_sector);
                fprintf(stderr, "sector_count=%u\n", 
                        ep->volume_section->sector_count);
            }
        }

        // process section: sectors
        else if (strcmp("sectors", section_header.type) == 0)
        {
            // remember size of sectors section so that we
            // can determine size of last chunk
            sectors_section_end = ftell(ep->finfile) - sizeof(section_header) + section_header.size - 1;

            // verbosity
            if (verbose)
                fprintf(stderr, "sectors section ends at byte: %u\n",
                        sectors_section_end);
        }

        // process section: table 
	// table is the section that has the offsets of each chunk in the file
        else if (strcmp("table", section_header.type) == 0)
        {
            // read table section
            if (ep->table_section == NULL)
            {
                ep->table_section = (evf_table_section *)calloc(1,sizeof(evf_table_section));
                if (ep->table_section == NULL)
                {
                    fprintf(stderr, "error reading table section\n");
		    evf_close(af);
		    return -1;
                }
            }
            if (!fread_evf_table_section(ep->table_section, ep->finfile))
            {
                fprintf(stderr, "error reading table section\n");
		evf_close(af);
		return -1;
            }

            // verbosity
            if (verbose)
            {
                fprintf(stderr, "chunk_count=%u\n", 
                        ep->table_section->chunk_count);
                fprintf(stderr, "padding=%s\n", 
                        ep->table_section->padding);
                fprintf(stderr, "crc=%u\n", 
                        ep->table_section->crc);
                for (unsigned int i = 0; i < ep->table_section->chunk_count; i++)
                    fprintf(stderr, "ep->table_section->offsets[%u]=%u\n",
                            i, ep->table_section->offsets[i]);
                fprintf(stderr, "unknown=%u\n", ep->table_section->unknown);
            }
        }

        // process section: table2
        else if (strcmp("table2", section_header.type) == 0)
        {
            // TODO: what is this?  it seems to be a copy of the 
            // table section...
            //
            // ignore this section
        }

        // process section: data
        else if (strcmp("data", section_header.type) == 0)
        {
            // TODO: what is this? it seems to be identical
            // to the "disk" section in EnCase 4...
            //
            // ignore this section
        }

        // process section: disk
        else if (strcmp("disk", section_header.type) == 0)
        {
            // TODO: what is this? it seems to be identical
            // to the "data" section in EnCase 4...
            //
            // ignore this section
        }

        // process section: hash
        else if (strcmp("hash", section_header.type) == 0)
        {
            // ignore this section
        }

        // process section: next
        else if (strcmp("next", section_header.type) == 0)
        {
            // TODO: as soon as we're ready to support .E02 files and beyond,
            // this section might prove useful

            // we're at the file's end
            eof = TRUE;
        }

        // process section: done
        else if (strcmp("done", section_header.type) == 0)
        {
            // we're at the file's end
            eof = TRUE;
        }

        // process unknown section
        else 
        {
            // report unknown section
            fprintf(stderr, "unknown section: %s\n", section_header.type);
        }

        // verbosity
        if (verbose)
            fprintf(stderr, "Next section is at %llu.\n", section_header.next);
    
        // seek to next section
        if (fseek(ep->finfile, section_header.next, SEEK_SET) != 0)
        {
            fprintf(stderr, "error seeking to next section\n");
	    evf_close(af);
	    return -1;
        }
    }

    
    // If a volume_sectionw as present, set the chunk size.
    // note chunk size --- which is the AFF page size
    
    if(ep->volume_section){
	ep->chunk_size = ep->volume_section->sectors_per_chunk * ep->volume_section->bytes_per_sector;
    }
    else{
	ep->chunk_size = 0;		// don't know what it should be
    }


    af->image_pagesize = ep->chunk_size;
    af->image_size     = evf_filesize(af);

    // we'll use last byte of sectors section to determine size of last chunk
    if (sectors_section_end <= 0) {
        fprintf(stderr,
		"invalid last byte of sectors section: %u\n",
                sectors_section_end);
	evf_close(af);
	return -1;
    }
    else {
        ep->table_section->offsets[ep->table_section->chunk_count] = sectors_section_end + 1;
    }

    return 0;
}

static int evf_get_seg(AFFILE *af,const char *segname,unsigned long *arg,
		       unsigned char *data,size_t *datalen)
{
    struct evf_private *ep = EVF_PRIVATE(af);

    /* Right now, this only supports reading page segments (chunks).
     * TODO: Read name and see if it is a metadata segment name.
     */
    int page_number = af_segname_page_number(segname);
    if(page_number<0) return -1;	// only support getting data pages for now

    /* the (unsigned) is okay because we're previously determined if it is less than 0 */
    if((unsigned)page_number  < ep->chunk_start ||
       (unsigned)page_number >= ep->chunk_start + ep->table_section->chunk_count){
	return -1;			// the requested page is not in this E0x file
    }

    int chunk_number = page_number - ep->chunk_start;

    if(ep->table_section==0) return -1;	// no tables!
    
    /* If arg was requested, it's 0 */
    if(arg) *arg = 0;
    
    /* If datalen was requested and data is 0, return the size of the chunk */
    if(data==0 && datalen && *datalen==0){
	*datalen = ep->chunk_size;
	/* Note: the last chunk could be smaller; fix this. */
	return 0;
    }

    if(data==0 && datalen==0) return 0;	// caller was just checking to see if the segment existed.
    if(data && datalen==0) return -1;	// caller didn't tell me how much data I can write

    /* If we are requesting data and the data size isn't big enough,
     * return an error.
     */
    if(data && *datalen < ep->chunk_size) return AF_ERROR_DATASMALL;

    /* Make sure that the requested page number is in range */
    if((unsigned)chunk_number >= ep->table_section->chunk_count ){
	return -1;			// requested segment doesn't exist
    }

    /* Get the chunk */
    //fprintf(stderr, "Converting chunk %u of %u...\r", i, table_section->chunk_count);

    // See if the chunk is compresed.
    if (check_cbit(ep->table_section->offsets[chunk_number])) {
	// Read a compressed chunk into a local buffer
	// seek to chunk
	if (fseek(ep->finfile, clear_cbit(ep->table_section->offsets[chunk_number]),SEEK_SET) != 0) {
	    fprintf(stderr, "error seeking to chunk %u\n", chunk_number);
	    return -1;
	}

	// verbosity
	if (verbose)
	    fprintf(stderr, "about to read chunk %u of %u bytes " 
		    "between %u and %u\n", chunk_number,
		    clear_cbit(ep->table_section->offsets[chunk_number+1]) - 
		    clear_cbit(ep->table_section->offsets[chunk_number]),
		    clear_cbit(ep->table_section->offsets[chunk_number]),
		    clear_cbit(ep->table_section->offsets[chunk_number+1]));

	// prepare state for inflate
	z_stream strm;
	memset(&strm,0,sizeof(strm));

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = clear_cbit(ep->table_section->offsets[chunk_number+1]) - 
	    clear_cbit(ep->table_section->offsets[chunk_number]);
	strm.next_out = data;	// decompress to the user's buffer
	strm.avail_out = *datalen;

	// Allocate the buffer to get the compressed data
	unsigned char *cchunk = (unsigned char *)calloc(1,strm.avail_in);	// for a compressed chunk
	if(cchunk==0) return -1;	


	// That's where it is going to go */
	strm.next_in = cchunk;

	// read chunk into local buffer
	if (fread(cchunk, strm.avail_in, 1, ep->finfile) != 1) {
	    fprintf(stderr, "error reading chunk %u\n", chunk_number);
	    free(cchunk);
	    return -1;
	}

	// Get read to inflate
	if (inflateInit(&strm) != Z_OK) {
	    fprintf(stderr, "error allocating inflate state\n");
	    free(cchunk);
	    return -1;
	}

	// inflate chunk
	if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
	    inflateEnd(&strm);
	    free(cchunk);
	    return -1;
	}
	// deallocate the decompression state
	inflateEnd(&strm);
	    
	// verbosity
	if (verbose) fprintf(stderr, "%u bytes inflated to %lu bytes\n",
			     clear_cbit(ep->table_section->offsets[chunk_number+1]) -
			     clear_cbit(ep->table_section->offsets[chunk_number]), 
			     strm.total_out);

	free(cchunk);		// free our temporary area
	return 0;		
    }

    // handle raw chunks (uncompressed)
    // seek to chunk
    if (fseek(ep->finfile, ep->table_section->offsets[chunk_number], SEEK_SET) != 0) {
	fprintf(stderr, "error seeking to chunk %u\n", chunk_number);
	return -1;
    }

    // read chunk; the last 4 bytes of the chunk are the CRC
    // which we will load into a different buffer
    int chunk_len_plus_crc = clear_cbit(ep->table_section->offsets[chunk_number+1]) - ep->table_section->offsets[chunk_number];
    unsigned char crc_buf[4];
    unsigned int chunk_len = chunk_len_plus_crc - 4;

    if(ep->chunk_size != chunk_len){
	(*af->error_reporter)("chunk size (%u) != chunk_len (%d)\n",(unsigned)ep->chunk_size,chunk_len);
    }

    if(*datalen < chunk_len) return AF_ERROR_DATASMALL;

    if (fread(data, chunk_len, 1, ep->finfile) != 1) {
	fprintf(stderr, "error reading chunk %u\n", chunk_number);
	return -1;
    }
    if (fread(crc_buf,sizeof(crc_buf),1,ep->finfile) != 1){
	fprintf(stderr, "error reading chunk %u CRC\n", chunk_number);
	return -1;
    }
	
    // verbosity
    if (verbose) fprintf(stderr, "read chunk %u of %u bytes " \
			 "between %u and %u\n", chunk_number,
			 clear_cbit(ep->table_section->offsets[chunk_number+1]) - ep->table_section->offsets[chunk_number],
			 ep->table_section->offsets[chunk_number],
			 clear_cbit(ep->table_section->offsets[chunk_number+1]));
    
    // compare CRCs;
    //
    if (Expert_Witness_Compression_crc(data, ep->chunk_size, 1) != read_uint_32(crc_buf)){
	fprintf(stderr, "CRC mismatch at chunk %u\n", chunk_number);
	fprintf(stderr, "computed CRC is: 0x%x\n", 
		Expert_Witness_Compression_crc(data, ep->chunk_size, 1));
	fprintf(stderr, "image's CRC is: 0x%x\n", read_uint_32(crc_buf));
    }
    return 0;				// no problems!
}


int evf_update_seg(AFFILE *af, const char *name,
		    unsigned long arg,const void *value,unsigned int vallen,int append)
{
    /* Write back to the EVF file. Currently unsupported. */
    return -1;				// some kind of error...
}


static int evf_vstat(AFFILE *af,struct af_vnode_info *vni)
{
    struct evf_private *ep = EVF_PRIVATE(af);
    memset(vni,0,sizeof(*vni));		// clear it
    vni->imagesize = -1;		// set to number of bytes in the image
    vni->pagesize  = ep->chunk_size;	// set to the page size
    vni->has_pages = 1;
    vni->supports_metadata    = 0;
    return 0;
}

static int evf_rewind_seg(AFFILE *af)
{
    struct evf_private *ep = EVF_PRIVATE(af);
    ep->cur_seg = ep->chunk_start;
    return 0;
}


static int evf_get_next_seg(AFFILE *af,char *segname,size_t segname_len,
			    unsigned long *arg,
			    unsigned char *data,size_t *datalen_)
{
    struct evf_private *ep = EVF_PRIVATE(af);
    if(ep->cur_seg < ep->chunk_start + ep->table_section->chunk_count ){
	snprintf(segname,segname_len,AF_PAGE,ep->cur_seg++);
	if(arg) *arg = 0;
	if(data) return evf_get_seg(af,segname,arg,data,datalen_);
	if(datalen_) *datalen_ = ep->chunk_size; // TK: handle short chunks
	return 0;
    }
    return -1;				// not implemented yet;
}


struct af_vnode vnode_evf = {
    AF_IDENTIFY_EVF,
    AF_VNODE_TYPE_PRIMITIVE,
    "EVF",
    evf_identify_file,
    evf_open,
    evf_close,
    evf_vstat,
    evf_get_seg,			// get seg
    evf_get_next_seg,			// get_next_seg
    evf_rewind_seg,			// rewind_seg
    evf_update_seg,			// update_seg
    0,					// del_seg
    0,					// read
    0					// write
};

