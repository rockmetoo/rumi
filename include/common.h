/**
 * @abstruct common library
 * @author rockmetoo <rockmetoo@gmail.com>
 */

#ifndef __common_h__
#define __common_h__

#include <unistd.h>
#include <pthread.h>

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

#define MUTEX_NEW(x,r) do { \
		x = (qmutex_t *)calloc(1, sizeof(qmutex_t)); \
		if(x == NULL) break; \
		memset((void*)x, 0, sizeof(qmutex_t)); \
		pthread_mutexattr_t _mutexattr; \
		pthread_mutexattr_init(&_mutexattr); \
		if(r == true) { \
			pthread_mutexattr_settype(&_mutexattr, PTHREAD_MUTEX_RECURSIVE); \
		} \
		int _ret = pthread_mutex_init(&(x->mutex), &_mutexattr); \
		pthread_mutexattr_destroy(&_mutexattr); \
		if(_ret != 0) { \
			char _errmsg[64]; \
			strerror_r(_ret, _errmsg, sizeof(_errmsg)); \
			DEBUG("MUTEX: can't initialize mutex. [%d:%s]", _ret, _errmsg); \
			free(x); \
			x = NULL; \
		} \
	} while(0)

#define MUTEX_LEAVE(x) do { \
		if(x == NULL) break; \
		if(!pthread_equal(x->owner, pthread_self())) { \
			DEBUG("MUTEX: unlock - owner mismatch."); \
		} \
		if((x->count--) < 0) x->count = 0; \
		pthread_mutex_unlock(&(x->mutex)); \
	} while(0)

#define MAX_MUTEX_LOCK_WAIT (5000)

#define MUTEX_ENTER(x) do { \
		if(x == NULL) break; \
		while(true) { \
			int _ret, i; \
			for(i = 0; (_ret = pthread_mutex_trylock(&(x->mutex))) != 0 && i < MAX_MUTEX_LOCK_WAIT; i++) { \
				if(i == 0) { \
					DEBUG("MUTEX: mutex is already locked - retrying"); \
				} \
				usleep(1); \
			} \
			if(_ret == 0) break; \
			char _errmsg[64]; \
			strerror_r(_ret, _errmsg, sizeof(_errmsg)); \
			DEBUG("MUTEX: can't get lock - force to unlock. [%d:%s]", _ret, _errmsg); \
			MUTEX_LEAVE(x); \
		} \
		x->count++; \
		x->owner = pthread_self(); \
	} while(0)

#define MUTEX_DESTROY(x) do { \
		if(x == NULL) break; \
		if(x->count != 0) DEBUG("Q_MUTEX: mutex counter is not 0."); \
		int _ret; \
		while((_ret = pthread_mutex_destroy(&(x->mutex))) != 0) { \
			char _errmsg[64]; \
			strerror_r(_ret, _errmsg, sizeof(_errmsg)); \
			DEBUG("MUTEX: force to unlock mutex. [%d:%s]", _ret, _errmsg); \
			MUTEX_LEAVE(x); \
		} \
		free(x); \
	} while(0)

#endif
