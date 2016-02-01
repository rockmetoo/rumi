/**
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "coder.h"
#include "helper.h"
#include "string.h"

listtable* parseQueries(listtable* tbl, const char* query, char equalchar, char sepchar, int* count) {

	if (tbl == NULL && (tbl = qlisttbl(0)) == NULL) {
		return NULL;
	}

	if (query == NULL) {
		return tbl;
	}

	int cnt			= 0;
	char* newquery	= strdup(query);

	while (newquery && *newquery) {

		char* value	= makeword(newquery, sepchar);
		char* name	= strTrim(makeword(value, equalchar));

		urlDecode(name);
		urlDecode(value);

		if (tbl->putstr(tbl, name, value) == true) {
			cnt++;
		}

		free(name);
		free(value);
	}

	if (count != NULL) {
		*count = cnt;
	}

	free(newquery);

	return tbl;
}






