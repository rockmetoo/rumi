#include "server.h"

#include <stdbool.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <assert.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/thread.h>
#include <event2/listener.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>

struct hook_t {
	char* method;
	callback cb;
	void* userdata;
};

typedef struct hook_t hook;

static int		notifyLoopExit(server* aserver);
static void		notifyCallback(struct bufferevent* buffer, void* userdata);
static void*	serverLoop(void* instance);
static void		closeServer(server* aserver);
static void		libeventLogCallback(int severity, const char* msg);
static int		setUndefinedOptions(server* webserver);
static SSL_CTX* initSSL(const char* certPath, const char* pkeyPath);
static void		listenerCallback(struct evconnlistener* listener, evutil_socket_t evsocket, struct sockaddr* sockaddr, int socklen, void* userdata);
static connection* connectionNew(server* aserver, struct bufferevent* buffer);
static void		connectionReset(connection* conn);
static void		connectionFree(connection* conn);
static void		connectionReadCallback(struct bufferevent* buffer, void* userdata);
static void		connectionWriteCallback(struct bufferevent* buffer, void* userdata);
static void		connectionEventCallback(struct bufferevent* buffer, short what, void* userdata);
static void		connectionCallback(connection* conn, int event);
static int		callHooks(short event, connection* conn);
static void*	setUserData(connection* conn, int index, const void* userdata, callback_free_userdata free_cb);
static void*	getUserData(connection* conn, int index);

// Local variables.
static bool initialized = false;

int g_log_level = LOG_WARN;

enum loglevel setLogLevel(enum loglevel lv) {
	int prev	= g_log_level;
	g_log_level = lv;

	return prev;
}

// Create a server object.
server* serverNew(void) {

	if (initialized) {
		initialized = TRUE;
	}

	server* aserver = NEW(server);

	if (aserver == NULL) {
		return NULL;
	}

	// Initialize instance.
	aserver->options	= ahashtable(0, 0);
	aserver->stats		= ahashtable(100, QHASHTBL_THREADSAFE);
	aserver->hooks		= alist(0);

	if (aserver->options == NULL || aserver->stats == NULL || aserver->hooks == NULL) {
		serverFree(aserver);
		return NULL;
	}

	DEBUG("Created a server object.");

	return server;
}

/**
* Start server.
*
* @return 0 if successful, otherwise -1.
*/
int serverStart(server* webserver) {

	DEBUG("Starting a server.");

	setUndefinedOptions(webserver);

	// Hookup libevent's log message.
	if (g_log_level >= LOG_DEBUG) {

		event_set_log_callback(libeventLogCallback);

		if (g_log_level >= LOG_DEBUG) {

			event_enable_debug_mode();
		}
	}

	// parse addr
	int port					= serverGetOptionInt(webserver, "server.port");
	char* addr					= serverGetOptionAsString(webserver, "server.addr");
	struct sockaddr* sockaddr	= NULL;
	size_t sockaddr_len			= 0;

	// Unix socket.
	if (addr[0] == '/') {

		struct sockaddr_un unixaddr;
		bzero((void *) &unixaddr, sizeof(struct sockaddr_un));

		if (strlen(addr) >= sizeof(unixaddr.sun_path)) {
			errno = EINVAL;
			DEBUG("Too long unix socket name. '%s'", addr);
			return -1;
		}

		unixaddr.sun_family = AF_UNIX;

		// no need of strncpy()
		strcpy(unixaddr.sun_path, addr);

		sockaddr		= (struct sockaddr *) &unixaddr;
		sockaddr_len	= sizeof(unixaddr);

	} else if (strstr(addr, ":")) { // IPv6

		struct sockaddr_in6 ipv6addr;
		bzero((void *) &ipv6addr, sizeof(struct sockaddr_in6));
		ipv6addr.sin6_family	= AF_INET6;
		ipv6addr.sin6_port		= htons(port);
		evutil_inet_pton(AF_INET6, addr, &ipv6addr.sin6_addr);
		sockaddr		= (struct sockaddr *) &ipv6addr;
		sockaddr_len	= sizeof(ipv6addr);

	} else { // IPv4

		struct sockaddr_in ipv4addr;
		bzero((void *) &ipv4addr, sizeof(struct sockaddr_in));
		ipv4addr.sin_family	= AF_INET;
		ipv4addr.sin_port	= htons(port);
		ipv4addr.sin_addr.s_addr = (IS_EMPTY_STR(addr)) ? INADDR_ANY : inet_addr(addr);
		sockaddr			= (struct sockaddr *) &ipv4addr;
		sockaddr_len		= sizeof(ipv4addr);
	}

	// SSL
	if (!webserver->sslctx && ad_server_get_option_int(server, "server.enable_ssl")) {
		char *cert_path = ad_server_get_option(server, "server.ssl_cert");
		char *pkey_path = ad_server_get_option(server, "server.ssl_pkey");
		webserver->sslctx = initSSL(cert_path, pkey_path);

		if (webserver->sslctx == NULL) {

			ERROR(
				"Couldn't load certificate file(%s) or private key file(%s).",
				cert_path, pkey_path
			);

			return -1;
		}

		DEBUG("SSL Initialized.");
	}

	// Bind
	if (!webserver->evbase) {
		webserver->evbase = event_base_new();
		if (!webserver->evbase) {
			ERROR("Failed to create a new event base.");
			return -1;
		}
	}

	// Create a eventfd for notification channel.
	int notifyfd = eventfd(0, 0);
	webserver->notify_buffer = bufferevent_socket_new(webserver->evbase, notifyfd, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(webserver->notify_buffer, NULL, notifyCallback, NULL, server);

	if (!webserver->listener) {
		webserver->listener = evconnlistener_new_bind(
			webserver->evbase, listenerCallback, (void *)webserver,
			LEV_OPT_THREADSAFE | LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
			ad_server_get_option_int(server, "server.backlog"),
			sockaddr, sockaddr_len
		);

		if (!webserver->listener) {
			ERROR("Failed to bind on %s:%d", addr, port);
			return -1;
		}
	}

	// Listen
	INFO("Listening on %s:%d%s", addr, port, ((webserver->sslctx) ? " (SSL)" : ""));

	int exitstatus = 0;

	if (serverGetOptionAsInt(webserver, "server.thread")) {

		DEBUG("Launching server as a thread.");

		webserver->thread = NEW(pthread_t);
		pthread_create(webserver->thread, NULL, &serverLoop, (void *)webserver);
		//pthread_detach(server->thread);
	} else {

		int* retval = serverLoop(webserver);

		exitstatus = *retval;
		free(retval);
		closeServer(webserver);

		if (serverGetOptionAsInt(webserver, "server.free_on_stop")) {
			serverFree(webserver);
		}
	}

	return exitstatus;
}

/**
* Stop server.
*
* This call is be used to stop a server from different thread.
*
* @return 0 if successful, otherwise -1.
*/
void serverStop(server* webserver) {

	DEBUG("Send loopexit notification.");

	notifyLoopExit(webserver);
	sleep(1);

	if (serverGetOptionAsInt(webserver, "server.thread")) {
		closeServer(webserver);

		if (serverGetOptionAsInt(webserver, "server.free_on_stop")) {
			serverFree(webserver);
		}
	}
}

/**
* Release server object and all the resources.
*/
void serverFree(server* webserver) {

	if (webserver == NULL) return;

	int thread = serverGetOptionAsInt(webserver, "server.thread");

	if (thread && webserver->thread) {

		notifyLoopExit(webserver);
		sleep(1);
		closeServer(webserver);
	}

	if (webserver->evbase) {
		event_base_free(webserver->evbase);
	}

	if (webserver->sslctx) {
		SSL_CTX_free(webserver->sslctx);
		ERR_clear_error();
		ERR_remove_state(0);
	}

	if (webserver->options) {
		webserver->options->free(webserver->options);
	}

	if (webserver->stats) {
		webserver->stats->free(webserver->stats);
	}

	if (webserver->hooks) {

		list* tbl = webserver->hooks;

		hook* ahook;

		while((ahook = tbl->popfirst(tbl, NULL))) {
			if (ahook->method) free(ahook->method);
			free(hook);
		}

		webserver->hooks->free(webserver->hooks);
	}

	free(webserver);

	DEBUG("Server terminated.");
}

/**
* Clean up all the global objects.
*
* This will make memory-leak checkers happy.
* There are globally shared resources in libevent and openssl and
* it's usually not a problem since they don't grow but having these
* can confuse some debugging tools into thinking as memory leak.
* If you need to make sure that libasyncd has released all internal
* library-global data structures, call this.
*/
void serverGlobalFree(void) {

	// Libevent related.
	//libevent_global_shutdown(); // From libevent v2.1

	// OpenSSL related.
	ENGINE_cleanup();
	CONF_modules_free();
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
}

//  set server option
void serverSetOption(server* webserver, const char* key, const char* value) {

	webserver->options->putstr(webserver->options, key, value);
}

// get server option
char* serverGetOptionAsString(server* webserver, const char* key) {

	return webserver->options->getstr(webserver->options, key, false);
}

// get server option in integer format
int serverGetOptionAsInt(server* webserver, const char* key) {

	char* value = serverGetOptionAsString(webserver, key);
	return (value) ? atoi(value) : 0;
}

/**
* Helper method for creating minimal OpenSSL SSL_CTX object.
*
* @param cert_path path to a PEM encoded certificate file
* @param pkey_path path to a PEM encoded private key file
*
* @return newly allocated SSL_CTX object or NULL on failure
*
* @note
* This function initializes SSL_CTX with minimum default with
* "SSLv23_server_method" which will make the server understand
* SSLv2, SSLv3, and TLSv1 protocol.
*
* @see ad_server_set_ssl_ctx()
*/
SSL_CTX* serverSSLCTXCreateSimple(const char *cert_path, const char *pkey_path) {
	SSL_CTX* sslctx = SSL_CTX_new(SSLv23_server_method());

	if (! SSL_CTX_use_certificate_file(sslctx, cert_path, SSL_FILETYPE_PEM) ||
	! SSL_CTX_use_PrivateKey_file(sslctx, pkey_path, SSL_FILETYPE_PEM)) {

		ERROR(
			"Couldn't load certificate file(%s) or private key file(%s).",
			cert_path, pkey_path
		);

		return NULL;
	}

	return sslctx;
}

/**
* Attach OpenSSL SSL_CTX to the server.
*
* @param server a valid server instance
* @param sslctx allocated and configured SSL_CTX object
*
* @note
* This function attached SSL_CTX object to the server causing it to
* communicate over SSL. Use ad_server_ssl_ctx_create_simple() to
* quickly create a simple SSL_CTX object or make your own with OpenSSL
* directly. This function must never be called when ad_server is running.
*
* @see ad_server_ssl_ctx_create_simple()
*/
void serverSetSSLCTX(server* webserver, SSL_CTX* sslctx) {

	if (webserver->sslctx) {
		SSL_CTX_free(webserver->sslctx);
	}

	webserver->sslctx = sslctx;
}

/**
* Get OpenSSL SSL_CTX object.
*
* @param server a valid server instance
* @return SSL_CTX object, NULL if not enabled.
*
* @note
* As a general rule the returned SSL_CTX object must not be modified
* while server is running as it may cause unpredictable results.
* However, it is safe to use it for reading SSL statistics.
*/
SSL_CTX* serverGetSSLCTX(server* webserver) {

	return webserver->sslctx;
}

/**
* return internal statistic counter map.
*/
hashtable* serverGetStats(server* webserver, const char* key) {
	return webserver->stats;
}

/**
* Register user hook.
*/
void serverRegisterHook(server* webserver, callback cb, void* userdata) {

	serverRegisterHookOnMethod(webserver, NULL, cb, userdata);
}

/**
* Register user hook on method name.
*/
void serverRegisterHookOnMethod(server* webserver, const char* method, callback cb, void* userdata) {

	hook ahook;
	bzero((void*)&ahook, sizeof(hook));
	ahook.method = (method) ? strdup(method) : NULL;
	ahook.cb = cb;
	ahook.userdata = userdata;
	webserver->hooks->addlast(webserver->hooks, (void*)&ahook, sizeof(hook));
}

/**
* Attach userdata into the connection.
*
* @return previous userdata;
*/
void* connectionSetUserdata(connection* conn, const void* userdata, callback_free_userdata free_cb) {

	return setUserData(conn, 0, userdata, free_cb);
}

/**
* Get userdata attached in the connection.
*
* @return previous userdata;
*/
void* connectionGetUserdata(connection* conn) {

	return getUserData(conn, 0);
}

/**
* Set extra userdata into the connection.
*
* @return previous userdata;
*
* @note
* Extra userdata is for default protocol handler such as ad_http_handler to
* provide higher abstraction. End users should always use only ad_conn_set_userdata()
* to avoid any conflict with default handlers.
*/
void* connectionSetExtra(connection* conn, const void* extra, callback_free_userdata free_cb) {

	return setUserData(conn, 1, extra, free_cb);
}

/**
* Get extra userdata attached in this connection.
*/
void* connectionGetExtra(connection* conn) {

	return getUserData(conn, 1);
}

/**
* Set method name on this connection.
*
* Once the method name is set, hooks registered by ad_server_register_hook_on_method()
* will be called if method name is matching as registered.
*
* @see serverRegisterHookOnMethod()
*/
char* connectionSetMethod(connection* conn, char* method) {

	char* prev = conn->method;

	if (conn->method) {
		free(conn->method);
	}

	conn->method = strdup(method);

	return prev;
}










// Set default options that were not set by user..
static int setUndefinedOptions(server* webserver) {

	int newentries				= 0;
	char* defaultOptions[][2]	= SERVER_OPTIONS;

	for (int i = 0; !IS_EMPTY_STR(defaultOptions[i][0]); i++) {

		if (! ad_server_get_option(server, defaultOptions[i][0])) {
			ad_server_set_option(server, defaultOptions[i][0], defaultOptions[i][1]);
			newentries++;
		}

		DEBUG("%s=%s", defaultOptions[i][0], ad_server_get_option(server, defaultOptions[i][0]));
	}

	return newentries;
}

static SSL_CTX* initSSL(const char* cert_path, const char* pkey_path) {

	SSL_CTX* sslctx = SSL_CTX_new(SSLv23_server_method());

	if (!SSL_CTX_use_certificate_file(sslctx, cert_path, SSL_FILETYPE_PEM) ||
	!SSL_CTX_use_PrivateKey_file(sslctx, pkey_path, SSL_FILETYPE_PEM)) {
		return NULL;
	}

	return sslctx;
}

static void listenerCallback(struct evconnlistener* listener, evutil_socket_t socket,
struct sockaddr* sockaddr, int socklen, void* userdata) {

	DEBUG("New connection.");
	server* webserver = (server*) userdata;

	// create a new buffer
	struct bufferevent* buffer = NULL;

	if (webserver->sslctx) {
		buffer = bufferevent_openssl_socket_new(webserver->evbase, socket,
		SSL_new(webserver->sslctx),
		BUFFEREVENT_SSL_ACCEPTING,
		BEV_OPT_CLOSE_ON_FREE);
	} else {
		buffer = bufferevent_socket_new(webserver->evbase, socket, BEV_OPT_CLOSE_ON_FREE);
	}

	if (buffer == NULL) goto error;

	// set read timeout
	int timeout = serverGetOptionAsInt(webserver, "server.timeout");

	if (timeout > 0) {
		struct timeval tm;
		bzero((void *)&tm, sizeof(struct timeval));
		tm.tv_sec = timeout;
		bufferevent_set_timeouts(buffer, &tm, NULL);
	}

	// create a connection
	void* conn = connectionNew(webserver, buffer);

	if (!conn) goto error;

	return;

	error:
		if (buffer) bufferevent_free(buffer);
		ERROR("Failed to create a connection handler.");
		event_base_loopbreak(webserver->evbase);
		webserver->errcode = ENOMEM;
}

static connection* connectionNew(server* webserver, struct bufferevent* buffer) {

	if (server == NULL || buffer == NULL) {
		return NULL;
	}

	// create a new connection container.
	connection* conn = NEW(connection);

	if (conn == NULL) return NULL;

	// initialize with default values.
	conn->webserver = webserver;
	conn->buffer	= buffer;
	conn->in		= bufferevent_get_input(buffer);
	conn->out		= bufferevent_get_output(buffer);

	connectionReset(conn);

	// bind callback
	bufferevent_setcb(buffer, connectionReadCallback, connectionWriteCallback, conn_event_cb, (void*)conn);
	bufferevent_setwatermark(buffer, EV_WRITE, 0, 0);
	bufferevent_enable(buffer, EV_WRITE);
	bufferevent_enable(buffer, EV_READ);

	// run callbacks with AD_EVENT_INIT event.
	conn->status = call_hooks(EVENT_INIT | EVENT_WRITE, conn);
	return conn;
}

static void connectionReset(connection* conn) {

	conn->status = OK;
	for(int i = 0; i < NUM_USER_DATA; i++) {

		if (conn->userdata[i]) {

			if (conn->userdata_free_cb[i] != NULL) {
				conn->userdata_free_cb[i](conn, conn->userdata[i]);
			} else {
				WARN("Found unreleased userdata.");
			}

			conn->userdata[i] = NULL;
		}
	}

	if (conn->method) {
		free(conn->method);
		conn->method = NULL;
	}
}

static void connectionFree(connection* conn) {

	if (conn) {
		if (conn->status != CLOSE) {
			callHooks(EVENT_CLOSE | EVENT_SHUTDOWN , conn);
		}

		connectionReset(conn);

		if (conn->buffer) {
			if (conn->webserver->sslctx) {
				int sslerr = bufferevent_get_openssl_error(conn->buffer);
				if (sslerr) {
					char errmsg[256];
					ERR_error_string_n(sslerr, errmsg, sizeof(errmsg));
					ERROR("SSL %s (err:%d)", errmsg, sslerr);
				}
			}

			bufferevent_free(conn->buffer);
		}

		free(conn);
	}
}

static void connectionReadCallback(struct bufferevent* buffer, void* userdata) {

	DEBUG("read_cb");
	connection* conn = userdata;
	connectionCallback(conn, EVENT_READ);
}

static void connectionWriteCallback(struct bufferevent* buffer, void* userdata) {

	DEBUG("write_cb");
	connection* conn = userdata;

	connectionCallback(conn, EVENT_WRITE);
}

static void connectionEventCallback(struct bufferevent* buffer, short what, void* userdata) {

	DEBUG("event_cb 0x%x", what);

	connection* conn = userdata;

	if (what & BEV_EVENT_EOF || what & BEV_EVENT_ERROR || what & BEV_EVENT_TIMEOUT) {
		conn->status = CLOSE;
		connectionCallback(conn, EVENT_CLOSE | ((what & BEV_EVENT_TIMEOUT) ? EVENT_TIMEOUT : 0));
	}
}

static void connectionCallback(connection* conn, int event) {

	DEBUG("conn_cb: status:0x%x, event:0x%x", conn->status, event);

	if(conn->status == OK || conn->status == TAKEOVER) {
		int status = callHooks(event, conn);
		// update status only when it's higher then before
		if (! (conn->status == CLOSE || (conn->status == DONE && conn->status >= status))) {
			conn->status = status;
		}
	}

	if(conn->status == DONE) {
		if (serverGetOptionAsInt(conn->webserver, "server.request_pipelining")) {
			callHooks(EVENT_CLOSE , conn);
			connectionReset(conn);
			callHooks(EVENT_INIT , conn);
		} else {

			// do nothing but drain input buffer.
			if (event == EVENT_READ) {
				DEBUG("Draining in-buffer. %d", conn->status);
				DRAIN_EVBUFFER(conn->in);
			}
		}

		return;
	} else if(conn->status == CLOSE) {
		if (evbuffer_get_length(conn->out) <= 0) {
			int newevent = (event & EVENT_CLOSE) ? event : EVENT_CLOSE;
			callHooks(newevent, conn);
			connectionFree(conn);
			DEBUG("Connection closed.");
			return;
		}
	}
}

static int callHooks(short event, connection *conn) {

	DEBUG("call_hooks: event 0x%x", event);

	list* hooks = conn->webserver->hooks;

	listObj obj;

	bzero((void*)&obj, sizeof(listObj));

	while (hooks->getnext(hooks, &obj, false) == true) {

		hook* ahook = (hook*) obj.data;

		if (ahook->cb) {
			if (ahook->method && conn->method && strcmp(ahook->method, conn->method)) {
				continue;
			}

			int status = ahook->cb(event, conn, ahook->userdata);
			if (status != OK) {
				return status;
			}
		}
	}

	return OK;
}

static int notifyLoopExit(server* webserver) {

	uint64_t x = 0;
	return bufferevent_write(webserver->notify_buffer, &x, sizeof(uint64_t));
}

static void notifyCallback(struct bufferevent* buffer, void* userdata) {

	server* webserver = (server*) userdata;
	event_base_loopexit(webserver->evbase, NULL);
	DEBUG("Existing loop.");
}

static void* serverLoop(void* instance) {

	server* webserver = (server*) instance;

	int* retval = NEW(int);

	DEBUG("Loop start");

	event_base_loop(webserver->evbase, 0);

	DEBUG("Loop finished");
	*retval = (event_base_got_break(webserver->evbase)) ? -1 : 0;

	return retval;
}

static void closeServer(server* webserver) {
	DEBUG("Closing server.");

	if (webserver->notify_buffer) {
		bufferevent_free(webserver->notify_buffer);
		webserver->notify_buffer = NULL;
	}

	if (webserver->listener) {
		evconnlistener_free(webserver->listener);
		webserver->listener = NULL;
	}

	if (webserver->thread) {
		void* retval = NULL;
		DEBUG("Waiting server's last loop to finish.");
		pthread_join(*(webserver->thread), &retval);
		free(retval);
		free(webserver->thread);
		webserver->thread = NULL;
	}

	INFO("Server closed.");
}

static void libeventLogCallback(int severity, const char* msg) {

	switch(severity) {
		case _EVENT_LOG_MSG : {
			INFO("%s", msg);
			break;
		}

		case _EVENT_LOG_WARN : {
			WARN("%s", msg);
			break;
		}

		case _EVENT_LOG_ERR : {
			ERROR("%s", msg);
			break;
		}

		default : {
			DEBUG("%s", msg);
			break;
		}
	}
}

static void* setUserData(connection* conn, int index, const void* userdata, callback_free_userdata free_cb) {

	void* prev						= conn->userdata;
	conn->userdata[index]			= (void*) userdata;
	conn->userdata_free_cb[index]	= free_cb;

	return prev;
}

static void* getUserData(connection* conn, int index) {

	return conn->userdata[index];
}
