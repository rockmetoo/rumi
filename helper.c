/**
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "helper.h"

char* makeword(char* str, char stop) {

	char* word;
	int len, i;

	for (len = 0; ((str[len] != stop) && (str[len])); len++)
	;

	word = (char*) malloc(sizeof(char) * (len + 1));

	if (word == NULL) return NULL;

	for (i = 0; i < len; i++) word[i] = str[i];

	word[i] = '\0';

	if (str[len]) len++;

	for (i = len; str[i]; i++) str[i - len] = str[i];

	str[i - len] = '\0';

	return word;
}
