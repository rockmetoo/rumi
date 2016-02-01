/**
 * @abstruct hashtable library
 * @author rockmetoo <rockmetoo@gmail.com>
 */

#ifndef __hashtable_h__
#define __hashtable_h__

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hashtable_t		hashtable;
typedef struct hashtableObj_t	hashtableObj;

enum {
	QHASHTBL_THREADSAFE = (0x01)
};

extern hashtable*	ahashtable(size_t range, int options);
extern bool			hashtablePut(hashtable* tbl, const char* name, const void* data, size_t size);
extern bool			hashtablePutString(hashtable* tbl, const char* name, const char* str);
extern bool			hashtablePutStringf(hashtable* tbl, const char* name, const char* format, ...);
extern bool			hashtablePutInt(hashtable* tbl, const char* name, int64_t num);
extern void*		hashtableGet(hashtable* tbl, const char* name, size_t* size, bool newmem);
extern char*		hashtableGetString(hashtable* tbl, const char* name, bool newmem);
extern int64_t		hashtableGetInt(hashtable* tbl, const char* name);
extern bool			hashtableRemove(hashtable* tbl, const char* name);
extern bool			hashtableGetNext(hashtable* tbl, hashtableObj* obj, bool newmem);
extern size_t		hashtableSize(hashtable* tbl);
extern void			hashtableClear(hashtable* tbl);
extern bool			hashtableDebug(hashtable* tbl, FILE* out);
extern void			hashtableLock(hashtable* tbl);
extern void			hashtableUnlock(hashtable* tbl);
extern void			hashtableFree(hashtable* tbl);

// qhashtbl container object structure
struct hashtable_t {
	bool		(*put) (hashtable* tbl, const char* name, const void* data, size_t size);
	bool		(*putstr) (hashtable* tbl, const char* name, const char* str);
	bool		(*putstrf) (hashtable* tbl, const char* name, const char* format, ...);
	bool		(*putint) (hashtable* tbl, const char* name, const int64_t num);
	void*		(*get) (hashtable* tbl, const char* name, size_t* size, bool newmem);
	char*		(*getstr) (hashtable* tbl, const char* name, bool newmem);
	int64_t		(*getint) (hashtable* tbl, const char* name);
	bool		(*remove) (hashtable* tbl, const char* name);
	bool		(*getnext) (hashtable* tbl, hashtableObj* obj, bool newmem);
	size_t		(*size) (hashtable* tbl);
	void		(*clear) (hashtable* tbl);
	bool		(*debug) (hashtable* tbl, FILE* out);
	void		(*lock) (hashtable* tbl);
	void		(*unlock) (hashtable* tbl);
	void		(*free) (hashtable* tbl);

	void* qmutex;
	size_t num;
	size_t range;
	hashtableObj** slots;
};

// qhashtbl object data structure
struct hashtableObj_t {
	uint32_t		hash;
	char*			name;
	void*			data;
	size_t			size;
	hashtableObj*	next;
};

#ifdef __cplusplus
}
#endif
#endif
