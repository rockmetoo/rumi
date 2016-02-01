/**
 * @abstruct encoding-decoding helpers
 * @author rockmetoo <rockmetoo@gmail.com>
 */

#ifndef __coder_h__
#define __coder_h__

#include <stdlib.h>
#include <stdbool.h>
#include "listtable.h"

#ifdef __cplusplus
extern "C" {
#endif

extern listtable*	parseQueries(listtable* tbl, const char* query, char equalchar, char sepchar, int* count);
extern char*		urlEncode(const void* bin, size_t size);
extern size_t		urlDecode(char* str);
extern char*		base64Encode(const void* bin, size_t size);
extern size_t		base64Decode(char* str);
extern char*		hexEncode(const void* bin, size_t size);
extern size_t		hexDecode(char* str);

#ifdef __cplusplus
}
#endif
#endif
