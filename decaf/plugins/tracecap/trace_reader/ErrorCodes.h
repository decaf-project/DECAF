/**
 * @file Definitions for common error codes
 * @Author Lok Yan
 */

#ifndef ERRORCODES_H
#define ERRORCODES_H

#define UNKNOWN_ERROR -100
#define NULL_POINTER_ERROR -101
#define OOM_ERROR -102
#define END_UNKNOWN_ERROR -102

/**
 * Table for converting these error codes to strings
 */
static const char* ERR_CODE_TABLE[] = {
    "UNKNOWN_ERROR",
    "NULL Pointer Error",
    "Out of Memory Error"
};

/**
 * Converts error codes to strings
 * @param code The error code
 * @return The error code string
 */
inline const char* errCodeToString(int code)
{
  if ( (code >= UNKNOWN_ERROR) || (code <= END_UNKNOWN_ERROR) )
  {
    return ERR_CODE_TABLE[0];
  }
  return ERR_CODE_TABLE[(-code) - 100];
}

#endif//ERRORCODES_H
