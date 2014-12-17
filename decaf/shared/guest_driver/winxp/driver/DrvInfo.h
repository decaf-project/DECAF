
// __________________________________________________________
//
//                          DrvInfo.h
//                Driver Info Definitions V1.00
//                06-02-2000 Sven B. Schreiber
//                        sbs@orgon.com
// __________________________________________________________

#ifndef _DRVINFO_H_
#define _DRVINFO_H_

// =================================================================
// DISCLAIMER
// =================================================================

/*

This software is provided "as is" and any express or implied
warranties, including, but not limited to, the implied warranties of
merchantability and fitness for a particular purpose are disclaimed.
In no event shall the author Sven B. Schreiber be liable for any
direct, indirect, incidental, special, exemplary, or consequential
damages (including, but not limited to, procurement of substitute
goods or services; loss of use, data, or profits; or business
interruption) however caused and on any theory of liability,
whether in contract, strict liability, or tort (including negligence
or otherwise) arising in any way out of the use of this software,
even if advised of the possibility of such damage.

*/

// =================================================================
// REVISION HISTORY
// =================================================================

/*

05-26-2000 V1.00 Original version (SBS).

*/

// =================================================================
// BASIC TYPES
// =================================================================

typedef UCHAR               BYTE,  *PBYTE,  **PPBYTE;
typedef USHORT              WORD,  *PWORD,  **PPWORD;
typedef ULONG               DWORD, *PDWORD, **PPDWORD;
typedef unsigned __int64    QWORD, *PQWORD, **PPQWORD;
typedef int                 BOOL,  *PBOOL,  **PPBOOL;
typedef void                                **PPVOID;

// -----------------------------------------------------------------

#define BYTE_               sizeof (BYTE)
#define WORD_               sizeof (WORD)
#define DWORD_              sizeof (DWORD)
#define QWORD_              sizeof (QWORD)
#define BOOL_               sizeof (BOOL)
#define PVOID_              sizeof (PVOID)
#define HANDLE_             sizeof (HANDLE)
#define PHYSICAL_ADDRESS_   sizeof (PHYSICAL_ADDRESS)

// =================================================================
// MACROS
// =================================================================

#define _DRV_DEVICE(_name)  \\Device\\     ## _name
#define _DRV_LINK(_name)    \\DosDevices\\ ## _name
#define _DRV_PATH(_name)    \\\\.\\        ## _name

// -----------------------------------------------------------------

#define _CSTRING(_text) #_text
#define CSTRING(_text) _CSTRING (_text)

#define _USTRING(_text) L##_text
#define USTRING(_text) _USTRING (_text)

#define PRESET_UNICODE_STRING(_symbol,_buffer) \
        UNICODE_STRING _symbol = \
            { \
            sizeof (USTRING (_buffer)) - sizeof (WORD), \
            sizeof (USTRING (_buffer)), \
            USTRING (_buffer) \
            };

// -----------------------------------------------------------------

#if DRV_VERSION_LOW < 10

#define _DRV_V2(_a,_b)       _a ## .0 ## _b
#define _DRV_V2X(_a,_b) V ## _a ## .0 ## _b

#else // #if DRV_VERSION_LOW < 10

#define _DRV_V2(_a,_b)       _a ## .  ## _b
#define _DRV_V2X(_a,_b) V ## _a ## .  ## _b

#endif // #if DRV_VERSION_LOW < 10 #else

#define DRV_V2(_a,_b)  _DRV_V2(_a,_b)
#define DRV_V2X(_a,_b) _DRV_V2X(_a,_b)

// -----------------------------------------------------------------

#define _DRV_V4(_a,_b,_c) _a ## . ## _b ## .0. ## _c
#define DRV_V4(_a,_b,_c) _DRV_V4(_a,_b,_c)

// -----------------------------------------------------------------

#define DRV_V               DRV_V2X (DRV_VERSION_HIGH, \
                                     DRV_VERSION_LOW)

#define DRV_VERSION         DRV_V2  (DRV_VERSION_HIGH, \
                                     DRV_VERSION_LOW)

#define DRV_VERSION_QUAD    DRV_V4  (DRV_VERSION_HIGH, \
                                     DRV_VERSION_LOW,  \
                                     DRV_BUILD)

#define DRV_VERSION_BINARY         ((DRV_VERSION_HIGH * 100) \
                                   + DRV_VERSION_LOW)

// =================================================================
// DRIVER INFORMATION
// =================================================================

#define DRV_ID                  DRV_PREFIX.DRV_MODULE
#define DRV_ID_VERSION          DRV_ID.DRV_VERSION_HIGH
#define DRV_FILENAME            DRV_MODULE.DRV_EXTENSION
#define DRV_CAPTION             DRV_NAME DRV_V
#define DRV_COMMENT             DRV_DATE DRV_AUTHOR

// -----------------------------------------------------------------

#define DRV_DEVICE              _DRV_DEVICE (DRV_MODULE)
#define DRV_LINK                _DRV_LINK   (DRV_MODULE)
#define DRV_PATH                _DRV_PATH   (DRV_MODULE)
#define DRV_EXTENSION           sys

// -----------------------------------------------------------------

#define DRV_CLASS               DRV_MODULE.DRV_VERSION_QUAD
#define DRV_ICON                DRV_MODULE.Icon

// -----------------------------------------------------------------

#define DRV_COPYRIGHT           Copyright \xA9 DRV_YEAR
#define DRV_COPYRIGHT_EX        DRV_COPYRIGHT DRV_COMPANY

// -----------------------------------------------------------------

#define DRV_DATE_US             DRV_MONTH-DRV_DAY-DRV_YEAR
#define DRV_DATE_GERMAN         DRV_DAY.DRV_MONTH.DRV_YEAR
#define DRV_DATE                DRV_DATE_US

// =================================================================
// NT4 COMPATIBILITY
// =================================================================

#ifndef IRP_MJ_QUERY_POWER
#define IRP_MJ_QUERY_POWER      0x16
#endif

#ifndef IRP_MJ_SET_POWER
#define IRP_MJ_SET_POWER        0x17
#endif

#ifndef IRP_MJ_PNP_POWER
#define IRP_MJ_PNP_POWER        0x1B
#endif

////////////////////////////////////////////////////////////////////
#ifdef _RC_PASS_
////////////////////////////////////////////////////////////////////

// =================================================================
// HEADER FILES
// =================================================================

#include <winver.h>

// =================================================================
// VERSION INFO
// =================================================================

#define DRV_RC_VERSION \
VS_VERSION_INFO VERSIONINFO \
\
FILEVERSION    DRV_VERSION_HIGH, DRV_VERSION_LOW, 0, DRV_BUILD \
PRODUCTVERSION DRV_VERSION_HIGH, DRV_VERSION_LOW, 0, DRV_BUILD \
FILEFLAGSMASK  VS_FFI_FILEFLAGSMASK \
FILEFLAGS      0 \
FILEOS         VOS_NT \
FILETYPE       VFT_DRV \
FILESUBTYPE    VFT2_UNKNOWN \
  { \
  BLOCK "StringFileInfo" \
    { \
    BLOCK "040904B0" \
      { \
      VALUE "OriginalFilename", CSTRING (DRV_FILENAME\0) \
      VALUE "InternalName",     CSTRING (DRV_MODULE\0) \
      VALUE "ProductName",      CSTRING (DRV_NAME\0) \
      VALUE "FileDescription",  CSTRING (DRV_CAPTION\0) \
      VALUE "CompanyName",      CSTRING (DRV_COMPANY\0) \
      VALUE "ProductVersion",   CSTRING (DRV_VERSION_QUAD\0) \
      VALUE "FileVersion",      CSTRING (DRV_VERSION_QUAD\0) \
      VALUE "LegalCopyright",   CSTRING (DRV_COPYRIGHT_EX\0) \
      VALUE "Comments",         CSTRING (DRV_COMMENT\0) \
      } \
    } \
  BLOCK "VarFileInfo" \
      { \
      VALUE "Translation", 0x0409, 0x04B0 \
      } \
  }

// =================================================================
// RESOURCES
// =================================================================

#define DRV_RC_ICON DRV_ICON ICON DRV_MODULE.ico

////////////////////////////////////////////////////////////////////
#endif // #ifdef _RC_PASS_
////////////////////////////////////////////////////////////////////

#endif // #ifndef _DRVINFO_H_

// =================================================================
// END OF FILE
// =================================================================
