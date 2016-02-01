/**
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/time.h>
#include "string.h"

/**
* Remove white spaces(including CR, LF) from head and tail of the string.
*
* @param str source string
*
* @return a pointer of source string if rsuccessful, otherewise returns NULL
*
* @note This modify source string directly.
*/
char* strTrim(char* str) {

	if (str == NULL) return NULL;

	char *ss, *se;

	for (ss = str; *ss == ' ' || *ss == '\t' || *ss == '\r' || *ss == '\n'; ss++)
	;

	for (se = ss; *se != '\0'; se++)
	;

	for (se--; se >= ss && (*se == ' ' || *se == '\t' || *se == '\r' || *se == '\n'); se--)
	;

	se++;
	*se = '\0';

	if (ss > str) {
		size_t len = (se - ss) + 1;
		memmove(str, ss, len);
	}

	return str;
}

char* strReplace(const char* mode, char* srcstr, const char* tokstr, const char* word) {

	if (mode == NULL || strlen(mode) != 2|| srcstr == NULL || tokstr == NULL
	|| word == NULL) {
	DEBUG("Unknown mode \"%s\".", mode);
	return NULL;
	}
	char *newstr, *newp, *srcp, *tokenp, *retp;
	newstr = newp = srcp = tokenp = retp = NULL;
	char method = mode[0], memuse = mode[1];
	int maxstrlen, tokstrlen;
	/* Put replaced string into malloced 'newstr' */
	if (method == 't') { /* Token replace */
	maxstrlen = strlen(srcstr) * ((strlen(word) > 0) ? strlen(word) : 1);
	newstr = (char *) malloc(maxstrlen + 1);
	for (srcp = (char *) srcstr, newp = newstr; *srcp; srcp++) {
	for (tokenp = (char *) tokstr; *tokenp; tokenp++) {
	if (*srcp == *tokenp) {
	char *wordp;
	for (wordp = (char *) word; *wordp; wordp++) {
	*newp++ = *wordp;
	}
	break;
	}
	}
	if (!*tokenp)
	*newp++ = *srcp;
	}
	*newp = '\0';
	} else if (method == 's') { /* String replace */
	if (strlen(word) > strlen(tokstr)) {
	maxstrlen = ((strlen(srcstr) / strlen(tokstr)) * strlen(word))
	+ (strlen(srcstr) % strlen(tokstr));
	} else {
	maxstrlen = strlen(srcstr);
	}
	newstr = (char *) malloc(maxstrlen + 1);
	tokstrlen = strlen(tokstr);
	for (srcp = srcstr, newp = newstr; *srcp; srcp++) {
	if (!strncmp(srcp, tokstr, tokstrlen)) {
	char *wordp;
	for (wordp = (char *) word; *wordp; wordp++)
	*newp++ = *wordp;
	srcp += tokstrlen - 1;
	} else
	*newp++ = *srcp;
	}
	*newp = '\0';
	} else {
	DEBUG("Unknown mode \"%s\".", mode);
	return NULL;
	}
	/* decide whether newing the memory or replacing into exist one */
	if (memuse == 'n')
	retp = newstr;
	else if (memuse == 'r') {
	strcpy(srcstr, newstr);
	free(newstr);
	retp = srcstr;
	} else {
	DEBUG("Unknown mode \"%s\".", mode);
	free(newstr);
	return NULL;
	}
	return retp;
}
