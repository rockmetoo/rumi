/**
 * @abstruct common library
 * @author rockmetoo <rockmetoo@gmail.com>
 */

#ifndef __common_h__
#define __common_h__

#define	TRUE			1
#define	FALSE			0
#define BUG_EXIT()		ASSERT(false)
#define ASSERT(c)		assert(c)
#define DESTROYALLP(a)	if(a){ delete[] a; a = NULL; }
#define FREE(a)			((void)(free(a), (a)= 0))
#define	GETENV(a)		getenv(a) ? getenv(a) : (char*)""
#define NEW(t)			((t*) calloc(1, sizeof(t)))
#define STRLEN(s)		(sizeof(s) - 1)
#define IS_EMPTY_STR(s) ((*s == '\0') ? true : false)
#define IS_EQUAL_STR(s1,s2) (!strcmp(s1,s2))
#define ENDING_CHAR(s)	(*(s + strlen(s) - 1))

extern int g_log_level;

#define ERROR(fmt, args...)	fprintf(stderr, "[ERROR] " fmt "\n", ##args);

#define WARN(fmt, args...)	fprintf(stderr, "[WARN] " fmt "\n", ##args);

#define INFO(fmt, args...)	fprintf(stderr, "[INFO] " fmt "\n", ##args);

#define DEBUG(fmt, args...)	\
	if (g_log_level >= 4) { \
		fprintf( \
			stdout, "[DEBUG] " fmt " [%s(),%s:%d]\n", ##args, \
			__func__, __FILE__, __LINE__ \
		); \
	}

#endif
