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

typedef struct listable_s		listtable;
typedef struct listable_obj_t	listableObj;
typedef struct listable_data_t	listableData;

struct listable_s {
    // capsulated member functions
    bool (*put)			(listable_s* tbl, const char* name, const void *data, size_t size);
    bool (*putstr)		(listable_s* tbl, const char* name, const char *str);
    bool (*putstrf) 	(listable_s* tbl, const char* name, const char *format, ...);
    bool (*putint)		(listable_s* tbl, const char* name, int64_t num);

    void *(*get)		(listable_s* tbl, const char* name, size_t *size, bool newmem);
    char *(*getstr) 	(listable_s* tbl, const char* name, bool newmem);
    int64_t (*getint)	(listable_s* tbl, const char* name);

    listable_data_t *(*getmulti) (listable_s* tbl, const char* name, bool newmem, size_t *numobjs);
    void (*freemulti)	(listable_data_t *objs);

    size_t (*remove)	(listable_s* tbl, const char* name);
    bool (*removeobj)	(listable_s* tbl, const listable_obj_t *obj);

    bool (*getnext)		(listable_s* tbl, listable_obj_t *obj, const char* name, bool newmem);

    size_t (*size)		(listable_s* tbl);
    void (*sort)		(listable_s* tbl);
    void (*clear)		(listable_s* tbl);

    bool (*save)		(listable_s* tbl, const char *filepath, char sepchar, bool encode);
    ssize_t (*load)		(listable_s* tbl, const char *filepath, char sepchar, bool decode);
    bool (*debug)		(listable_s* tbl, FILE *out);

    void (*lock)		(listable_s* tbl);
    void (*unlock)		(listable_s* tbl);

    void (*free)		(listable_s* tbl);

    // private methods
    bool (*namematch)	(listable_obj_t *obj, const char* name, uint32_t hash);
    int (*namecmp)		(const char *s1, const char *s2);

    // private variables - do not access directly
    bool unique;           // keys are unique
    bool caseinsensitive;  // case insensitive key comparison
    bool keepsorted;       // keep table in sorted (default: insertion order)
    bool inserttop;        // add new key at the top. (default: bottom)
    bool lookupforward;    // find keys from the top. (default: backward)

    void* qmutex;          // initialized when listableOPT_THREADSAFE is given
    size_t num;            // number of elements
    listable_obj_t* first; // first object pointer
    listable_obj_t* last;  // last object pointer
};

struct listable_obj_t {
    uint32_t		hash;	// 32bit-hash value of object name
    char*			name;	// object name
    void* 			data;	// data
    size_t			size;	// data size

    listable_obj_t* prev;	// previous link
    listable_obj_t* next;	// next link
};

struct listtable_data_t {
    void*	data;
    size_t	size;
    uint8_t	type;
};

// public functions

extern listable_s*	listtable(int options);

extern bool			listablePut(listable_s* tbl, const char* name, const void* data, size_t size);
extern bool			listablePutAsString(listable_s* tbl, const char* name, const char* str);
extern bool			listablePutAsStringf(listable_s* tbl, const char* name, const char* format, ...);
extern bool			listablePutAsInt(listable_s* tbl, const char* name, int64_t num);

extern void*		listableGet(listable_s* tbl, const char* name, size_t* size, bool newmem);
extern char*		listableGetAsString(listable_s* tbl, const char* name, bool newmem);
extern int64_t		listableGetAsInt(listable_s* tbl, const char* name);

extern listable_data_t* listableGetMulti(listable_s* tbl, const char* name, bool newmem, size_t* numobjs);
extern void			listableFreeMulti(listable_data_t* objs);

extern size_t		listableRemove(listable_s* tbl, const char* name);
extern bool			listableRemoveObj(listable_s* tbl, const listable_obj_t* obj);

extern bool			listableGetNext(listable_s* tbl, listable_obj_t* obj, const char* name, bool newmem);

extern size_t		listableSize(listable_s* tbl);
extern void			listableSort(listable_s* tbl);
extern void			listableClear(listable_s* tbl);
extern bool			listableSave(listable_s* tbl, const char* filepath, char sepchar, bool encode);
extern ssize_t		listableLoad(listable_s* tbl, const char* filepath, char sepchar, bool decode);

extern bool			listableDebug(listable_s* tbl, FILE *out);

extern void			listableLock(listable_s* tbl);
extern void			listableUnlock(listable_s* tbl);

extern void			listableFree(listable_s* tbl);

#ifdef __cplusplus
}
#endif

#endif
