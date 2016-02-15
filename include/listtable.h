/**
 * @abstruct list container with key/value pair in linked data structure
*/

#ifndef __listtable_h_
#define __listtable_h_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LISTTABLE_THREADSAFE		= (0x01),      // make it thread-safe
    LISTTABLE_UNIQUE			= (0x01 << 1), // keys are unique
    LISTTABLE_CASEINSENSITIVE	= (0x01 << 2), // keys are case insensitive
    LISTTABLE_INSERTTOP			= (0x01 << 3), // insert new key at the top
    LISTTABLE_LOOKUPFORWARD		= (0x01 << 4), // find key from the top (default: backward)
};

typedef struct listtable_s		listtable;
typedef struct listtable_obj_t	listtableObj;
typedef struct listtable_data_t	listtableData;

struct listtable_s {
    // capsulated member functions
    bool	(*put)			(listtable* tbl, const char* name, const void* data, size_t size);
    bool	(*putstr)		(listtable* tbl, const char* name, const char* str);
    bool	(*putstrf) 		(listtable* tbl, const char* name, const char* format, ...);
    bool	(*putint)		(listtable* tbl, const char* name, int64_t num);

    void*	(*get)			(listtable* tbl, const char* name, size_t* size, bool newmem);
    char*	(*getstr) 		(listtable* tbl, const char* name, bool newmem);
    int64_t	(*getint)		(listtable* tbl, const char* name);

    listtableData *(*getmulti) (listtable* tbl, const char* name, bool newmem, size_t* numobjs);
    void	(*freemulti)	(listtableData* objs);

    size_t	(*remove)		(listtable* tbl, const char* name);
    bool	(*removeobj)	(listtable* tbl, const listtableObj* obj);

    bool	(*getnext)		(listtable* tbl, listtableObj* obj, const char* name, bool newmem);

    size_t	(*size)			(listtable* tbl);
    void	(*sort)			(listtable* tbl);
    void 	(*clear)		(listtable* tbl);

    bool	(*save)			(listtable* tbl, const char* filepath, char sepchar, bool encode);
    ssize_t (*load)			(listtable* tbl, const char* filepath, char sepchar, bool decode);
    bool	(*debug)		(listtable* tbl, FILE* out);

    void	(*lock)			(listtable* tbl);
    void	(*unlock)		(listtable* tbl);

    void	(*free)			(listtable* tbl);

    // private methods
    bool	(*namematch)	(listtableObj* obj, const char* name, uint32_t hash);
    int		(*namecmp)		(const char* s1, const char* s2);

    // private variables - do not access directly
    bool			unique;				// keys are unique
    bool			caseinsensitive;	// case insensitive key comparison
    bool			keepsorted;			// keep table in sorted (default: insertion order)
    bool			inserttop;			// add new key at the top. (default: bottom)
    bool			lookupforward;		// find keys from the top. (default: backward)

    void*			qmutex;				// initialized when listableOPT_THREADSAFE is given
    size_t			num;				// number of elements
    listtableObj*	first; 				// first object pointer
    listtableObj*	last;				// last object pointer
};

struct listtable_obj_t {
    uint32_t		hash;	// 32bit-hash value of object name
    char*			name;	// object name
    void* 			data;	// data
    size_t			size;	// data size

    listtableObj*	prev;	// previous link
    listtableObj*	next;	// next link
};

struct listtable_data_t {
    void*	data;
    size_t	size;
    uint8_t	type;
};

// public functions

extern listtable*	listTable(int options);

extern bool			listablePut(listtable* tbl, const char* name, const void* data, size_t size);
extern bool			listablePutAsString(listtable* tbl, const char* name, const char* str);
extern bool			listablePutAsStringf(listtable* tbl, const char* name, const char* format, ...);
extern bool			listablePutAsInt(listtable* tbl, const char* name, int64_t num);

extern void*		listableGet(listtable* tbl, const char* name, size_t* size, bool newmem);
extern char*		listableGetAsString(listtable* tbl, const char* name, bool newmem);
extern int64_t		listableGetAsInt(listtable* tbl, const char* name);

extern listtableData* listableGetMulti(listtable* tbl, const char* name, bool newmem, size_t* numobjs);
extern void			listableFreeMulti(listtableData* objs);

extern size_t		listableRemove(listtable* tbl, const char* name);
extern bool			listableRemoveObj(listtable* tbl, const listtableObj* obj);

extern bool			listableGetNext(listtable* tbl, listtableObj* obj, const char* name, bool newmem);

extern size_t		listableSize(listtable* tbl);
extern void			listableSort(listtable* tbl);
extern void			listableClear(listtable* tbl);
extern bool			listableSave(listtable* tbl, const char* filepath, char sepchar, bool encode);
extern ssize_t		listableLoad(listtable* tbl, const char* filepath, char sepchar, bool decode);

extern bool			listableDebug(listtable* tbl, FILE *out);

extern void			listableLock(listtable* tbl);
extern void			listableUnlock(listtable* tbl);

extern void			listableFree(listtable* tbl);

#ifdef __cplusplus
}
#endif

#endif
