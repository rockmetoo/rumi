/**
 * @abstruct asynchronous server library
 * @author rockmetoo <rockmetoo@gmail.com>
 */

#ifndef __common_h__
#define __common_h__

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <openssl/ssl.h>

#include "common.h"
#include "hashtable.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

// default webserver settings
#define SERVER_OPTIONS { \
{ "server.port", "8888" }, \
\
/* Addr format IPv4="1.2.3.4", IPv6="1:2:3:4:5:6", Unix="/path" */ \
{ "server.addr", "0.0.0.0" }, \
\
{ "server.backlog", "128" }, \
\
/* Set read timeout seconds. 0 means no timeout. */ \
{ "server.timeout", "0" }, \
\
/* SSL options */ \
{ "server.enable_ssl", "0" }, \
{ "server.ssl_cert", "/usr/local/etc/server.crt" }, \
{ "server.ssl_pkey", "/usr/local/etc/server.key" }, \
\
/* Enable or disable request pipelining, this change AD_DONE's behavior */ \
{ "server.request_pipelining", "1" }, \
\
/* Run server in a separate thread */ \
{ "server.thread", "0" }, \
\
/* Collect resources after stop */ \
{ "server.free_on_stop", "1" }, \
\
/* End of array marker. Do not remove */ \
{ "", "_END_" } \
};

// server structure
struct server_t {
	int 					errcode;
	pthread_t*				thread;
	hashtable*				options;
	hashtable*				stats;
	list*					hooks;
	struct evconnlistener*	listener;
	struct event_base*		evbase;
	SSL_CTX*				sslctx;
	struct bufferevent*		notify_buffer;
};

// connection structure.
struct connection_t {
	server*					webserver;
	struct bufferevent*		buffer;
	struct evbuffer*		in;
	struct evbuffer*		out;
	int						status;
	void*					userdata[2];
	callback_free_userdata	userdata_free_cb[2];
	char*					method;
};

// these flags are used for log_level();
enum log_e {
	LOG_DISABLE = 0,
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG
};

// event types
#define EVENT_INIT		(1)
#define EVENT_READ		(1 << 1)
#define EVENT_WRITE		(1 << 2)
#define EVENT_CLOSE		(1 << 3)
#define EVENT_TIMEOUT	(1 << 4)
#define EVENT_SHUTDOWN	(1 << 5)

// return values of user callback
#define OK			(0) /*!< I'm done with this request. Escalate to other hooks. */
#define TAKEOVER	(1) /*!< I'll handle the buffer directly this time, skip next hook */
#define DONE		(2) /*!< We're done with this request but keep the connection open. */
#define CLOSE		(3) /*!< We're done with this request. Close as soon as we sent all data out. */

#define NUM_USER_DATA (2) /*!< Number of userdata. Currently 0 is for userdata, 1 is for extra. */

typedef void (*callback_free_userdata)(connection* conn, void* userdata);
typedef int (*callback)(short event, connection* conn, void* userdata);

typedef struct server_t		server;
typedef struct connection_t	connection;
typedef struct log_e		loglevel;

// public functions
enum loglevel	setLogLevel(enum loglevel lv);
extern server*	serverNew(void);
extern int		serverStart(server* webserver);
extern void		serverStop(server* webserver);
extern void		serverFree(server* webserver);
extern void		serverGlobalFree(void);
extern void		serverSetOption(server* webserver, const char* key, const char* value);
extern char*	serverGetOptionAsString(server* webserver, const char* key);
extern int		serverGetOptionAsInt(server* webserver, const char* key);

extern SSL_CTX*	serverSSLCTXCreateSimple(const char* certPath, const char* pkeyPath);
extern void		serverSetSSLCTX(server* webserver, SSL_CTX* sslctx);
extern SSL_CTX*	serverGetSSLCTX(server* webserver);

extern hashtable* serverGetStats(server* webserver, const char* key);

extern void		serverRegisterHook(server* webserver, callback cb, void* userdata);
extern void		serverRegisterHookOnMethod(server* webserver, const char* method,
callback cb, void* userdata);

extern void*	connectionSetUserdata(connection* conn, const void* userdata, callback_free_userdata free_cb);
extern void*	connectionGetUserdata(connection* conn);
extern void*	connectionSetExtra(connection* conn, const void* extra, callback_free_userdata free_cb);
extern void*	connectionGetExtra(connection* conn);

extern char*	connectionSetMethod(connection* conn, char* method);

#ifdef __cplusplus
}
#endif
#endif
