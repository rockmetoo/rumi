/**
 * @abstruct link list container library
 * @author rockmetoo <rockmetoo@gmail.com>
 */

#ifndef __list_h__
#define __list_h__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	QLIST_THREADSAFE = (0x01)
};

extern list*	alist(int options);
extern size_t	listSetSize(list* alist, size_t max);
extern bool		listAddFirst(list* alist, const void* data, size_t size);
extern bool 	listAddLast(list* alist, const void* data, size_t size);
extern bool		listAddAt(list* alist, int index, const void* data, size_t size);
extern void* 	listGetFirst(list* alist, size_t* size, bool newmem);
extern void*	listGetLast(list* alist, size_t* size, bool newmem);
extern void*	listGetAt(list* alist, int index, size_t* size, bool newmem);
extern void*	listPopFirst(list* alist, size_t* size);
extern void*	listPopLast(list* alist, size_t* size);
extern void*	listPopAt(list* alist, int index, size_t* size);
extern bool		listRemoveFirst(list* alist);
extern bool		listRemoveLast(list* alist);
extern bool		listRemoveAt(list* alist, int index);
extern bool		listGetNext(list* alist, listObj* obj, bool newmem);
extern size_t	listSize(list* alist);
extern size_t	listDataSize(list* alist);
extern void		listReverse(list* alist);
extern void		listClear(list* alist);
extern void*	listToArray(list* alist, size_t* size);
extern char*	listToString(list* alist);
extern bool		listDebug(list* alist, FILE* out);
extern void		listLock(list* alist);
extern void		listUnlock(list* alist);
extern void		listFree(list* alist);

// list structure

struct list_t {
	size_t	(*setsize)(list* alist, size_t max);
	bool	(*addfirst)(list* alist, const void *data, size_t size);
	bool	(*addlast)(list* alist, const void *data, size_t size);
	bool	(*addat)(list* alist, int index, const void *data, size_t size);
	void*	(*getfirst)(list* alist, size_t *size, bool newmem);
	void*	(*getlast)(list* alist, size_t *size, bool newmem);
	void*	(*getat)(list* alist, int index, size_t *size, bool newmem);
	void*	(*popfirst)(list* alist, size_t *size);
	void*	(*poplast)(list* alist, size_t *size);
	void*	(*popat)(list* alist, int index, size_t *size);
	bool	(*removefirst)(list* alist);
	bool	(*removelast)(list* alist);
	bool	(*removeat)(list* alist, int index);
	bool	(*getnext)(list* alist, listObj *obj, bool newmem);
	void	(*reverse)(list* alist);
	void	(*clear)(list* alist);
	size_t	(*size)(list* alist);
	size_t	(*datasize)(list* alist);
	void*	(*toarray)(list* alist, size_t *size);
	char*	(*tostring)(list* alist);
	bool	(*debug)(list* alist, FILE *out);
	void	(*lock)(list* alist);
	void	(*unlock)(list* alist);
	void	(*free)(list* alist);

	// initialized when QLIST_OPT_THREADSAFE is given
	void* qmutex;
	// number of elements
	size_t num;
	// maximum number of elements. 0 means no limit
	size_t max;
	// total sum of data size, does not include name size
	size_t datasum;
	// first object pointer
	listObj* first;
	// last object pointer
	listObj* last;
};

// list object data structure
struct listObj_t {
	void*			data;
	size_t			size;
	listObj*		prev;
	listObj*		next;
};

typedef struct listObj_t listObj;
typedef struct list_t list;

#ifdef __cplusplus
}
#endif
#endif
