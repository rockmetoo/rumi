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

static listableObj*	newObject(const char* name, const void* data, size_t size);
static bool			insertObject(listtable* tbl, listableObj* obj);
static listableObj*	findObject(listtable* tbl, const char* name, listableObj* retobj);
static bool			nameMatch(listableObj* obj, const char* name, uint32_t hash);
static bool			nameCaseMatch(listableObj* obj, const char* name, uint32_t hash);

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
	listableObj* obj = newObject(name, data, size);

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

























// lock must be obtained from caller
static listableObj* newObject(const char* name, const void* data, size_t size) {

	if (name == NULL || data == NULL || size <= 0) {
		errno = EINVAL;
		return false;
	}

	// make a new object
	char* dup_name = strdup(name);
	void* dup_data = malloc(size);

	listableObj* obj = (listableObj*) malloc(sizeof(listableObj));

	if (dup_name == NULL || dup_data == NULL || obj == NULL) {

		if (dup_name != NULL) free(dup_name);
		if (dup_data != NULL) free(dup_data);
		if (obj != NULL) free(obj);
		errno = ENOMEM;
		return NULL;
	}

	memcpy(dup_data, data, size);
	memset((void *)obj, '\0', sizeof(listableObj));

	// obj->hash = qhashmurmur3_32(dup_name);
	obj->name = dup_name;
	obj->data = dup_data;
	obj->size = size;

	return obj;
}

// lock must be obtained from caller
static bool insertObject(listtable* tbl, listableObj* obj) {

	// update hash
	obj->hash = qhashmurmur3_32(obj->name, strlen(obj->name));

	listableObj* prev = obj->prev;
	listableObj* next = obj->next;

	if (prev == NULL) tbl->first = obj;
	else prev->next = obj;

	if (next == NULL) tbl->last = obj;
	else next->prev = obj;

	// increase counter
	tbl->num++;

	return true;
}
