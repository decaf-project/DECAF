////////////////////////////////////////////////////////////////////////////////
////// vnode_evf.h
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//// types
////////////////////////////////////////////////////////////////////////////////

// for Boolean values
//typedef enum { FALSE=0, TRUE } bool;

// fixed-size types
#ifdef WIN32
typedef unsigned _int8 uint8;
typedef unsigned _int16 uint16;
typedef unsigned _int32 uint32;
typedef unsigned _int64 uint64;
#define evf_typed
#endif

/* Otherwise, assume gcc */
#ifndef evf_typed
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
#endif

// Unicode types
typedef uint8 UTF8;
typedef uint16 UTF16;
typedef uint32 UTF32;


////////////////////////////////////////////////////////////////////////////////
//// constants
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// UTF conversion flags
// http://www.unicode.org/Public/PROGRAMS/CVTUTF/ConvertUTF.{c,h}
////////////////////////////////////////////////////////////////////////////////

typedef enum 
{
    strictConversion = 0,
    lenientConversion
} 
ConversionFlags;


////////////////////////////////////////////////////////////////////////////////
// UTF conversion result
// http://www.unicode.org/Public/PROGRAMS/CVTUTF/ConvertUTF.{c,h}
////////////////////////////////////////////////////////////////////////////////

typedef enum 
{
    conversionOK,       /* conversion successful */
    sourceExhausted,    /* partial character in source, but hit end */
    targetExhausted,    /* insuff. room in target for conversion */
    sourceIllegal       /* source sequence is illegal/malformed */
} 
ConversionResult;


////////////////////////////////////////////////////////////////////////////////
// EVF file header
////////////////////////////////////////////////////////////////////////////////

struct evf_file_header 
{
    unsigned char signature[8];    // 8-byte signature

    // 1-byte constant
    uint8 one;

    // 2-byte segment number
    uint16 segment_number;

    // 2-byte constant
    uint16 zero;
}
__attribute__((packed));
typedef struct evf_file_header evf_file_header;


////////////////////////////////////////////////////////////////////////////////
// EVF section header
////////////////////////////////////////////////////////////////////////////////

struct evf_section_header 
{
    char type[16];    // 16-byte type; it's a string, so it's signed char, not unsigned
    uint64 next;    // 8-byte pointer to next section's offset in EVF file
    uint64 size;    // 8-byte size of current section
    char padding[40];    // 40-byte constant
    uint32 crc;    // 4-byte CRC
}
__attribute__((packed));
typedef struct evf_section_header evf_section_header;


////////////////////////////////////////////////////////////////////////////////
// EVF comments
//
// Essentially, tab- and newline-delimited keys and values.  We use this
// for comments in header and header2 sections alike, even though the former
// does not appear to boast all of these keys.  The latter, meanwhile, seems
// to boast (on different lines) yet other keys whose meaning is, as of yet, 
// unknown, so we omit them altogether for now.
////////////////////////////////////////////////////////////////////////////////

struct evf_comments 
{
    // evidence name
    UTF8 *a;

    // EnCase version
    UTF8 *av;

    // case name
    UTF8 *c;

    // TODO: what is this?
    //
    // unknown
    UTF8 *dc;

    // investigator
    UTF8 *e;

    // acquired date
    UTF8 *m;

    // evidence number
    UTF8 *n;

    // operating system
    UTF8 *ov;

    // TODO: what is this?
    //
    // unknown
    UTF8 *p;

    // notes
    UTF8 *t;

    // TODO: what's the difference between this and m?
    //       they seem to be the same consistently
    //
    // system date
    UTF8 *u;

}
__attribute__((packed));
typedef struct evf_comments evf_comments;


////////////////////////////////////////////////////////////////////////////////
// EVF header section
//
// Not to be confused with a "section header," 
// this section contains zlib-deflated data.
//
// This section should only appear in a .E01 file (and not in .E02 files
// or beyond).
////////////////////////////////////////////////////////////////////////////////

struct evf_header_section 
{
    // comments
    evf_comments comments;
}
__attribute__((packed));
typedef struct evf_header_section evf_header_section;


////////////////////////////////////////////////////////////////////////////////
// EVF header2 section
//
// This section contains zlib-deflated data, even more than appears to be
// in EVF header sections.  
//
// This section should only appear in a .E01 file (and not in .E02 files
// or beyond).
////////////////////////////////////////////////////////////////////////////////

struct evf_header2_section 
{
    // comments
    evf_comments comments;
}
__attribute__((packed));
typedef struct evf_header2_section evf_header2_section;


////////////////////////////////////////////////////////////////////////////////
// EVF volume section
//
// This section should only appear in a .E01 file (and not in .E02 files
// or beyond).
////////////////////////////////////////////////////////////////////////////////

struct evf_volume_section 
{
    // 4 reserved bytes
    uint32 reserved;

    // 4-byte count of number of chunks across all segment files
    uint32 chunk_count;

    // 4-byte count of number of sectors assigned to each chunk
    uint32 sectors_per_chunk;

    // 4-byte count of number of bytes in each sector
    uint32 bytes_per_sector;

    // 4-byte count of number of sectors across all segment files,
    // seemingly redundant in light of previous fields
    uint32 sector_count;

    // 20 reserved bytes
    unsigned char reserved2[20];

    // 1003-byte constant
    unsigned char padding[1003];

    // 5-byte signature
    unsigned char signature[5];

    // 4-byte CRC
    uint32 crc;
}
__attribute__((packed));
typedef struct evf_volume_section evf_volume_section;


////////////////////////////////////////////////////////////////////////////////
// EVF table section
//
// This section essentially tells us where in a segment we can find
// chunks.
////////////////////////////////////////////////////////////////////////////////

struct evf_table_section 
{
    // 4-byte count of number of chunks addressed in table
    uint32 chunk_count;

    // 16-byte constant
    unsigned char padding[16];

    // 4-byte CRC
    uint32 crc;

    // TODO: can a segment file have multiple table sections that aren't
    // duplicates of each other?  In the original Expert Witness Compression
    // Format, each "table section can hold 16375 entries."  Does that limit 
    // also apply to EnCase?
    //
    // array of 4-byte addresses of chunks in segment
    uint32 *offsets;

    // TODO: what is this?
    //
    // 4-byte unknown
    uint32 unknown;
}
__attribute__((packed));
typedef struct evf_table_section evf_table_section;


/* evf file implementation */
struct evf_private {
    FILE *finfile;			/* the file we are reading */
    uint64 chunk_start;			// where this encase file starts

    /* These are read from the EnCase file */
    uint64 chunk_size;
    evf_header_section *header_section;
    evf_header2_section *header2_section;

    evf_table_section *table_section;
    evf_volume_section *volume_section;
    evf_file_header file_header;        // last byte of sectors section

    uint64 cur_seg;		// current metadata segment, for iterator

    AFFILE **evfs;			// *.E02, .E03, .E04, ...
    unsigned int num_evfs;		// number of them
};

static inline struct evf_private *EVF_PRIVATE(AFFILE *af)
{
    return (struct evf_private *)(af->vnodeprivate);
}

extern struct af_vnode vnode_evf;	/* vnode_evf.cpp */


#ifndef FALSE
#define FALSE 0
#endif


#ifndef TRUE
#define TRUE  (!FALSE)
#endif

