/**
 * @abstruct string library
 * @author rockmetoo <rockmetoo@gmail.com>
 */

#ifndef __string_h__
#define __string_h__

#ifdef __cplusplus
extern "C" {
#endif

extern char* strTrim(char* str);
extern char* strReplace(const char* mode, char* srcstr, const char* tokstr, const char* word);

#ifdef __cplusplus
}
#endif
#endif
