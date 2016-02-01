/**
 * @author
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <event2/buffer.h>
#include "server.h"
#include "http.h"

// private functions
static http*	httpNew(struct evbuffer* out);
static void		httpFree(http* http);
static void		httpFreeCallback(connection* conn, void *userdata);
static size_t	httpAddInbuf(struct evbuffer* buffer, http* http, size_t maxsize);
static int		httpParser(http* http, struct evbuffer *in);
static int		parseRequestLine(http* http, char* line);
static int		parseHeaders(http* http, struct evbuffer* in);
static int		parseBody(http* http, struct evbuffer* in);
static ssize_t	parseChunkedBody(http* http, struct evbuffer* in);
static bool		isValidPathname(const char* path);
static void		correctPathname(char* path);
static char*	evbufferPeekln(struct evbuffer* buffer, size_t* n_reout, enum evbuffer_eol_style eol_style);
static ssize_t	evbufferDrainln(struct evbuffer* buffer, size_t* n_reout, enum evbuffer_eol_style eol_style);


/**
* HTTP protocol handler hook.
*
* This hook provides an easy way to handle HTTP request/response.
*
* @note
* This hook must be registered at the top of hook chain.
*
* @code
* server_t *server = server_new();
* server_register_hook(server, http_handler, NULL);
* @endcode
*/
int httpHandler(short event, connection* conn, void* userdata) {

	if (event & EVENT_INIT) {

		DEBUG("==> HTTP INIT");
		http* httpinit = httpNew(conn->out);
		if (httpinit == NULL) return CLOSE;

		connectionSetExtra(conn, http, http_free_cb);
		return OK;

	} else if (event & EVENT_READ) {

		DEBUG("==> HTTP READ");
		http* httpinit = (http*) connectionGetExtra(conn);

		int status = httpParser(http, conn->in);

		if (conn->method == NULL && httpinit->request.method != NULL) {

			connectionSetMethod(conn, httpinit->request.method);
		}

		return status;
	} else if (event & EVENT_WRITE) {

		DEBUG("==> HTTP WRITE");
		return OK;

	} else if (event & EVENT_CLOSE) {

		DEBUG(
			"==> HTTP CLOSE=%x (TIMEOUT=%d, SHUTDOWN=%d)",
			event, event & EVENT_TIMEOUT, event & EVENT_SHUTDOWN
		);

		return OK;
	}

	BUG_EXIT();

	return CLOSE;
}














static http* httpNew(struct evbuffer* out) {

	// create a new connection container
	http* ahttp = NEW(http);
	if (http == NULL) return NULL;

	// allocate additional resources
	ahttp->request.inbuf	= evbuffer_new();
	ahttp->request.headers	= list(QLISTTBL_UNIQUE | QLISTTBL_CASEINSENSITIVE);
	ahttp->response.headers = list(QLISTTBL_UNIQUE | QLISTTBL_CASEINSENSITIVE);

	if (ahttp->request.inbuf == NULL || ahttp->request.headers == NULL || ahttp->response.headers == NULL) {
		httpFree(ahttp);
		return NULL;
	}

	// initialize structure
	ahttp->request.status			= HTTP_REQ_INIT;
	ahttp->request.contentlength	= -1;
	ahttp->response.contentlength	= -1;
	ahttp->response.outbuf			= out;

	return http;
}
