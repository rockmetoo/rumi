/**
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "listtable.h"

static listtableObj*	newObject(const char* name, const void* data, size_t size);
static bool				insertObject(listtable* tbl, listtableObj* obj);
static listtableObj*	findObject(listtable* tbl, const char* name, listtableObj* retobj);
static bool				nameMatch(listtableObj* obj, const char* name, uint32_t hash);
static bool				nameCaseMatch(listtableObj* obj, const char* name, uint32_t hash);

listtable* listTable(int options) {

	listtable* tbl = (listtable*) calloc(1, sizeof(listtable));

	if (tbl == NULL) {

		errno = ENOMEM;

		return NULL;
	}

	// assign member methods.
	tbl->put		= listablePut;
	tbl->putstr		= listablePutAsString;
	tbl->putstrf	= listablePutAsStringf;
	tbl->putint		= listablePutAsInt;
	tbl->get		= listableGet;
	tbl->getstr		= listableGetAsString;
	tbl->getint		= listableGetAsInt;
	tbl->getmulti	= listableGetMulti;
	tbl->freemulti	= listableFreeMulti;
	tbl->remove		= listableRemove;
	tbl->removeobj	= listableRemoveObj;
	tbl->getnext	= listableGetNext;
	tbl->size		= listableSize;
	tbl->sort		= listableSort;
	tbl->clear		= listableClear;
	tbl->save		= listableSave;
	tbl->load		= listableLoad;
	tbl->debug		= listableDebug;
	tbl->lock		= listableLock;
	tbl->unlock		= listableUnlock;
	tbl->free		= listableFree;

	// assign private methods.
	tbl->namematch	= nameMatch;
	tbl->namecmp	= strcmp;

	// handle options.
	if (options & LISTTABLE_THREADSAFE) {

		MUTEX_NEW(tbl->qmutex, true);

		if (tbl->qmutex == NULL) {

			errno = ENOMEM;
			free(tbl);
			return NULL;
		}
	}

	if (options & LISTTABLE_UNIQUE) {
		tbl->unique = true;
	}

	if (options & LISTTABLE_CASEINSENSITIVE) {
		tbl->namematch = nameCaseMatch;
		tbl->namecmp = strcasecmp;
	}

	if (options & LISTTABLE_INSERTTOP) {
		tbl->inserttop = true;
	}

	if (options & LISTTABLE_LOOKUPFORWARD) {
		tbl->lookupforward = true;
	}

	return tbl;
}

bool listablePut(listtable* tbl, const char* name, const void* data, size_t size) {

	// make new object table
	listtableObj* obj = newObject(name, data, size);

	if (obj == NULL) {
		return false;
	}

	// lock table
	listableLock(tbl);

	// if unique flag is set, remove same key
	if (tbl->unique == true) listableRemove(tbl, name);

	// insert into table
	if (tbl->num == 0) {

		obj->prev = NULL;
		obj->next = NULL;

	} else {

		if (tbl->inserttop == false) {

			obj->prev = tbl->last;
			obj->next = NULL;

		} else {
			obj->prev = NULL;
			obj->next = tbl->first;
		}
	}

	insertObject(tbl, obj);

	// unlock table
	listableUnlock(tbl);

	return true;
}

bool listablePutAsString(listtable* tbl, const char* name, const char* str) {

	size_t size = (str) ? (strlen(str) + 1) : 0;

	return listablePut(tbl, name, (const void*)str, size);
}

bool listablePutAsStringf(listtable* tbl, const char* name, const char* format, ...) {

	char* str;

	DYNAMIC_VSPRINTF(str, format);

	if (str == NULL) {
		errno = ENOMEM;
		return false;
	}

	bool ret = listablePutAsString(tbl, name, str);

	free(str);

	return ret;
}

bool qlisttbl_putint(listtable* tbl, const char* name, int64_t num) {

	char str[20+1];

	snprintf(str, sizeof(str), "%"PRId64, num);

	return listablePutAsString(tbl, name, str);
}

void* listtableGget(listtable* tbl, const char* name, size_t* size, bool newmem) {

	if (name == NULL) {
		errno = EINVAL;
		return NULL;
}

	listableLock(tbl);

	void* data = NULL;

	listtableObj* obj = findObject(tbl, name, NULL);

	if (obj != NULL) {

		// get data
		if (newmem == true) {

			data = malloc(obj->size);

			if (data == NULL) {

				errno = ENOMEM;
				listableUnlock(tbl);
				return NULL;
			}

			memcpy(data, obj->data, obj->size);
		} else {

			data = obj->data;
		}

		// set size
		if (size != NULL) *size = obj->size;
	}

	listableUnlock(tbl);

	if (data == NULL) {
		errno = ENOENT;
	}

	return data;
}

char* listableGetAsString(listtable* tbl, const char* name, bool newmem) {

	return (char*) listableGet(tbl, name, NULL, newmem);
}

int64_t listableGetAsInt(listtable* tbl, const char* name) {

	int64_t num	= 0;
	char* str	= listableGetAsString(tbl, name, true);

	if (str != NULL) {
		num = atoll(str);
		free(str);
	}

	return num;
}

listtableData* listableGetMulti(listtable* tbl, const char* name, bool newmem, size_t* numobjs) {

	// objects container
	listtableData* objs	= NULL;

	// allocated number of objs
	size_t allocobjs	= 0;

	// number of keys found
	size_t numfound		= 0;
	listtableObj obj;

	// must be cleared before call
	memset((void*)&obj, 0, sizeof(obj));
	listableLock(tbl);

	while (tbl->getnext(tbl, &obj, name, newmem) == true) {

		numfound++;
		// allocate object array.
		if (numfound >= allocobjs) {
			// start from 10
			if (allocobjs == 0) allocobjs = 10;
			else allocobjs *= 2; // double size
			objs = (listtableData*) realloc(objs, sizeof(listtableData) * allocobjs);

			if (objs == NULL) {

				DEBUG("qlisttbl->getmulti(): Memory reallocation failure.");
				errno = ENOMEM;
				break;
			}
		}

		// copy reference
		listtableData* newobj = &objs[numfound - 1];
		newobj->data = obj.data;
		newobj->size = obj.size;
		newobj->type = (newmem == false) ? 1 : 2;

		// release resource
		if (newmem == true) {
			if (obj.name != NULL) free(obj.name);
		}

		// clear next block
		newobj = &objs[numfound];
		memset((void *)newobj, '\0', sizeof(listtableData));

		newobj->type = 0; // mark, end of objects
	}

	listableUnlock(tbl);

	// return found counter
	if (numobjs != NULL) {
		*numobjs = numfound;
	}

	if (numfound == 0) {
		errno = ENOENT;
	}

	return objs;
}

void listableFreeMulti(listtableData* objs) {

	if (objs == NULL) return;
	listtableData* obj;

	for (obj = &objs[0]; obj->type == 2; obj++) {
		if (obj->data != NULL) free(obj->data);
	}

	free(objs);
}

size_t listableRemove(listtable* tbl, const char* name) {

	if (name == NULL) return false;
	size_t numremoved = 0;
	listtableObj obj;

	// must be cleared before call
	memset((void*)&obj, 0, sizeof(obj));

	listableLock(tbl);

	while(listableGetNext(tbl, &obj, name, false) == true) {

		listableRemoveObj(tbl, &obj);
		numremoved++;
	}

	listableUnlock(tbl);

	return numremoved;
}

bool listableRemoveObj(listtable* tbl, const listtableObj* obj) {

	if (obj == NULL) return false;

	listableLock(tbl);

	// copy chains
	listtableObj* prev = obj->prev;
	listtableObj* next = obj->next;

	// find this object
	listtableObj* this = NULL;

	if (prev != NULL) this = prev->next;
	else if (next != NULL) this = next->prev;
	else this = tbl->first; // table has only one object.

	// double check
	if (this == NULL) {

		listableUnlock(tbl);
		DEBUG("qlisttbl->removeobj(): Can't veryfy object.");
		errno = ENOENT;
		return false;
	}

	// adjust chain links
	if (prev == NULL) tbl->first = next; // if the object is first one
	else prev->next = next; // not the first one

	if (next == NULL) tbl->last = prev; // if the object is last one
	else next->prev = prev; // not the first one

	// adjust counter
	tbl->num--;
	listableUnlock(tbl);

	// free object
	free(this->name);
	free(this->data);
	free(this);

	return true;
}

bool listableGetNext(listtable* tbl, listtableObj* obj, const char* name, bool newmem) {

	if (obj == NULL) return NULL;

	listableLock(tbl);

	listtableObj* cont = NULL;

	if (obj->size == 0) { // first time call

		if (name == NULL) { // full scan

			cont = (tbl->lookupforward) ? tbl->first : tbl->last;
		} else { // name search

			cont = findobj(tbl, name, NULL);
		}

	} else { // next call

		cont = (tbl->lookupforward) ? obj->next : obj->prev;
	}

	if (cont == NULL) {

		errno = ENOENT;
		listableUnlock(tbl);
		return false;
	}

	uint32_t hash = (name != NULL) ? hashmurmur3_32(name, strlen(name)) : 0;
	bool ret = false;

	while (cont != NULL) {
		if (name == NULL || tbl->namematch(cont, name, hash) == true) {
			if (newmem == true) {
				obj->name = strdup(cont->name);
				obj->data = malloc(cont->size);
				if (obj->name == NULL || obj->data == NULL) {
					if (obj->data != NULL) free(obj->data);

					obj->name = NULL;

					if (obj->name != NULL) free(obj->name);

					obj->data = NULL;
					errno = ENOMEM;
					break;
				}

				memcpy(obj->data, cont->data, cont->size);

			} else {
				obj->name = cont->name;
				obj->data = cont->data;
			}

			obj->hash = cont->hash;
			obj->size = cont->size;
			obj->prev = cont->prev;
			obj->next = cont->next;
			ret = true;

			break;
		}

		cont = (tbl->lookupforward) ? cont->next : cont->prev;
	}

	listableUnlock(tbl);

	if (ret == false) {
		errno = ENOENT;
	}

	return ret;
}

size_t listableSize(listtable* tbl) {

	return tbl->num;
}

void listableSort(listtable* tbl) {

	// run bubble sort
	listableLock(tbl);

	listtableObj *obj1, *obj2;
	listtableObj tmpobj;

	int n, n2, i;
	for (n = tbl->num; n > 0;) {
		n2 = 0;
		for (i = 0, obj1 = tbl->first; i < (n - 1); i++, obj1 = obj1->next) {
			obj2 = obj1->next; // this can't be null.
			if (tbl->namecmp(obj1->name, obj2->name) > 0) {
				// swapping contents is faster than adjusting links.
				tmpobj = *obj1;
				obj1->hash = obj2->hash;
				obj1->name = obj2->name;
				obj1->data = obj2->data;
				obj1->size = obj2->size;
				obj2->hash = tmpobj.hash;
				obj2->name = tmpobj.name;
				obj2->data = tmpobj.data;
				obj2->size = tmpobj.size;
				n2 = i + 1;
			}
		}

		n = n2; // skip sorted tailing elements
	}

	listableUnlock(tbl);
}

void listableClear(listtable* tbl) {

	listableLock(tbl);

	listtableObj* obj;

	for (obj = tbl->first; obj != NULL;) {
		listtableObj* next = obj->next;
		free(obj->name);
		free(obj->data);
		free(obj);
		obj = next;
	}

	tbl->num = 0;
	tbl->first = NULL;
	tbl->last = NULL;
	listableUnlock(tbl);
}

bool listableSave(qlisttbl_t *tbl, const char *filepath, char sepchar, bool encode) {

	if (filepath == NULL) {
errno = EINVAL;
return false;
}
int fd;
if ((fd = open(filepath, O_CREAT|O_WRONLY|O_TRUNC, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))) < 0) {
DEBUG("qlisttbl->save(): Can't open file %s", filepath);
return false;
}
char *gmtstr = qtime_gmt_str(0);
qio_printf(fd, -1, "# %s %s\n", filepath, gmtstr);
free(gmtstr);
qlisttbl_lock(tbl);
qlisttbl_obj_t *obj;
for (obj = tbl->first; obj; obj = obj->next) {
char *encval;
if (encode == true) encval = qurl_encode(obj->data, obj->size);
else encval = obj->data;
qio_printf(fd, -1, "%s%c%s\n", obj->name, sepchar, encval);
if (encode == true) free(encval);
}
qlisttbl_unlock(tbl);
close(fd);
return true;
}



// lock must be obtained from caller
static listtableObj* newObject(const char* name, const void* data, size_t size) {

	if (name == NULL || data == NULL || size <= 0) {
		errno = EINVAL;
		return false;
	}

	// make a new object
	char* dup_name = strdup(name);
	void* dup_data = malloc(size);

	listtableObj* obj = (listtableObj*) malloc(sizeof(listtableObj));

	if (dup_name == NULL || dup_data == NULL || obj == NULL) {

		if (dup_name != NULL) free(dup_name);
		if (dup_data != NULL) free(dup_data);
		if (obj != NULL) free(obj);
		errno = ENOMEM;
		return NULL;
	}

	memcpy(dup_data, data, size);
	memset((void *)obj, '\0', sizeof(listtableObj));

	// obj->hash = qhashmurmur3_32(dup_name);
	obj->name = dup_name;
	obj->data = dup_data;
	obj->size = size;

	return obj;
}

// lock must be obtained from caller
static bool insertObject(listtable* tbl, listtableObj* obj) {

	// update hash
	obj->hash = qhashmurmur3_32(obj->name, strlen(obj->name));

	listtableObj* prev = obj->prev;
	listtableObj* next = obj->next;

	if (prev == NULL) tbl->first = obj;
	else prev->next = obj;

	if (next == NULL) tbl->last = obj;
	else next->prev = obj;

	// increase counter
	tbl->num++;

	return true;
}

// lock must be obtained from caller
static listtableObj* findObject(listtable* tbl, const char* name, listtableObj* retobj) {

	if (retobj != NULL) {
		memset((void *)retobj, '\0', sizeof(listtableObj));
	}

	if (name == NULL || tbl->num == 0) {
		errno = ENOENT;
		return NULL;
	}

	uint32_t hash		= hashmurmur3_32(name, strlen(name));
	listtableObj* obj	= (tbl->lookupforward) ? tbl->first : tbl->last;

	while (obj != NULL) {

		// name string will be compared only if the hash matches.
		if (tbl->namematch(obj, name, hash) == true) {
			if (retobj != NULL) {
				*retobj = *obj;
			}

			return obj;
		}

		obj = (tbl->lookupforward)? obj->next : obj->prev;
	}

	// not found, set prev and next chain.
	if (retobj != NULL) {

		if (tbl->inserttop) {

			retobj->prev = NULL;
			retobj->next = tbl->first;

		} else {

			retobj->prev = tbl->last;
			retobj->next = NULL;
		}
	}

	errno = ENOENT;

	return NULL;
}

// key comp
static bool nameMatch(listtableObj* obj, const char* name, uint32_t hash) {

	if ((obj->hash == hash) && !strcmp(obj->name, name)) {
		return true;
	}

	return false;
}

static bool nameCaseMatch(listtableObj* obj, const char* name, uint32_t hash) {

	if (!strcasecmp(obj->name, name)) {
		return true;
	}

	return false;
}
