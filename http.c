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
#include "coder.h"

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

		connectionSetExtra(conn, http, httpFreeCallback);
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

/**
* return the request status.
*/
enum http_request_status_e httpGetStatus(connection* conn) {

	http* ahttp = (http*) connectionGetExtra(conn);

	if (ahttp == NULL) return HTTP_ERROR;

	return ahttp->request.status;
}

struct evbuffer* httpGetInbuf(connection* conn) {

	http* ahttp = (http*) connectionGetExtra(conn);
	return ahttp->request.inbuf;
}

struct evbuffer* httpGetOutbuf(connection* conn) {

	http* ahttp = (http*) connectionGetExtra(conn);
	return ahttp->response.outbuf;
}













static http* httpNew(struct evbuffer* out) {

	// create a new connection container
	http* ahttp = NEW(http);
	if (http == NULL) return NULL;

	// allocate additional resources
	ahttp->request.inbuf	= evbuffer_new();
	ahttp->request.headers	= list(LISTTABLE_UNIQUE | LISTTABLE_CASEINSENSITIVE);
	ahttp->response.headers = list(LISTTABLE_UNIQUE | LISTTABLE_CASEINSENSITIVE);

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

static void httpFree(http* ahttp) {

	if (ahttp) {

		if (ahttp->request.inbuf)		evbuffer_free(ahttp->request.inbuf);

		if (ahttp->request.method)		free(ahttp->request.method);

		if (ahttp->request.uri)			free(ahttp->request.uri);

		if (ahttp->request.httpver) 	free(ahttp->request.httpver);

		if (ahttp->request.path)		free(ahttp->request.path);

		if (ahttp->request.query)		free(ahttp->request.query);

		if (ahttp->request.headers)		ahttp->request.headers->free(ahttp->request.headers);

		if (ahttp->request.host)		free(ahttp->request.host);

		if (ahttp->request.domain)		free(ahttp->request.domain);

		if (ahttp->response.headers)	ahttp->response.headers->free(ahttp->response.headers);

		if (ahttp->response.reason)		free(ahttp->response.reason);

		free(ahttp);
	}
}

static void httpFreeCallback(connection* conn, void* userdata) {

	httpFree((http*) userdata);
}

static size_t httpAddInbuf(struct evbuffer *buffer, http* ahttp, size_t maxsize) {

	if (maxsize == 0 || evbuffer_get_length(buffer) == 0) {
		return 0;
	}

	return evbuffer_remove_buffer(buffer, ahttp->request.inbuf, maxsize);
}

static int httpParser(http* ahttp, struct evbuffer* in) {

	ASSERT(http != NULL && in != NULL);

	if (ahttp->request.status == HTTP_REQ_INIT) {

		char* line = evbuffer_readln(in, NULL, EVBUFFER_EOL_CRLF);
		if (line == NULL) return ahttp->request.status;

		ahttp->request.status = parse_requestline(http, line);
		free(line);

		// Do not call user callbacks until I reach the next state.
		if (ahttp->request.status == HTTP_REQ_INIT) {
			return TAKEOVER;
		}
	}

	if (ahttp->request.status == HTTP_REQ_REQUESTLINE_DONE) {

		ahttp->request.status = parse_headers(http, in);

		// Do not call user callbacks until I reach the next state.
		if (ahttp->request.status == HTTP_REQ_REQUESTLINE_DONE) {
			return TAKEOVER;
		}
	}

	if (ahttp->request.status == HTTP_REQ_HEADER_DONE) {

		ahttp->request.status = parse_body(http, in);

		// Do not call user callbacks until I reach the next state.
		if (ahttp->request.status == HTTP_REQ_HEADER_DONE) {
			return TAKEOVER;
		}
	}

	if (ahttp->request.status == HTTP_REQ_DONE) {
		return OK;
	}

	if (ahttp->request.status == HTTP_ERROR) {
		return CLOSE;
	}

	BUG_EXIT();

	return CLOSE;
}

static int parseRequestLine(http* ahttp, char* line) {

	// parse request line.
	char* saveptr;
	char* method	= strtok_r(line, " ", &saveptr);
	char* uri		= strtok_r(NULL, " ", &saveptr);
	char* httpver	= strtok_r(NULL, " ", &saveptr);
	char* tmp		= strtok_r(NULL, " ", &saveptr);

	if (method == NULL || uri == NULL || httpver == NULL || tmp != NULL) {
		DEBUG("Invalid request line. %s", line);
		return HTTP_ERROR;
	}

	// set request method
	ahttp->request.method = qstrupper(strdup(method));

	// set HTTP version
	ahttp->request.httpver = qstrupper(strdup(httpver));

	if (strcmp(ahttp->request.httpver, HTTP_PROTOCOL_09)
	&& strcmp(ahttp->request.httpver, HTTP_PROTOCOL_10)
	&& strcmp(ahttp->request.httpver, HTTP_PROTOCOL_11)) {
		DEBUG("Unknown protocol: %s", ahttp->request.httpver);
		return HTTP_ERROR;
	}

	// set URI
	if (uri[0] == '/') {

		ahttp->request.uri = strdup(uri);

	} else if ((tmp = strstr(uri, "://"))) {

		// divide URI into host and path
		char* path = strstr(tmp + STRLEN("://"), "/");

		if (path == NULL) { // URI has no path ex) http://domain.com:80

			ahttp->request.headers->putstr(ahttp->request.headers, "Host",
			tmp + STRLEN("://"));
			ahttp->request.uri = strdup("/");

		} else { // URI has path, ex) http://domain.com:80/path
			*path = '\0';
			ahttp->request.headers->putstr(ahttp->request.headers, "Host",
			tmp + STRLEN("://"));
			*path = '/';
			ahttp->request.uri = strdup(path);
		}
	} else {
		DEBUG("Invalid URI format. %s", uri);
		return HTTP_ERROR;
	}

	// Set request path. Only path part from URI.
	ahttp->request.path = strdup(ahttp->request.uri);
	tmp = strstr(ahttp->request.path, "?");

	if (tmp) {

		*tmp = '\0';
		ahttp->request.query = strdup(tmp + 1);

	} else {

		ahttp->request.query = strdup("");
	}

	urlDecode(ahttp->request.path);

	// check path
	if (isValidPathname(ahttp->request.path) == false) {

		DEBUG("Invalid URI format : %s", ahttp->request.uri);
		return HTTP_ERROR;
	}

	correctPathname(ahttp->request.path);
	DEBUG("Method=%s, URI=%s, VER=%s", ahttp->request.method,ahttp->request.uri, ahttp->request.httpver);

	return HTTP_REQ_REQUESTLINE_DONE;
}

static int parseHeaders(http* ahttp, struct evbuffer* in) {

	char* line;

	while ((line = evbuffer_readln(in, NULL, EVBUFFER_EOL_CRLF))) {

		if (IS_EMPTY_STR(line)) {

			const char* clen = ahttp->request.headers->getstr(ahttp->request.headers, "Content-Length", false);
			ahttp->request.contentlength = (clen) ? atol(clen) : -1;
			free(line);
			return HTTP_REQ_HEADER_DONE;
		}

		// parse
		char* name;
		char* value;
		char* tmp = strstr(line, ":");

		if (tmp) {

			*tmp	= '\0';
			name	= qstrtrim(line);
			value	= qstrtrim(tmp + 1);

		} else {

			name = qstrtrim(line);
			value = "";
		}

		// add
		ahttp->request.headers->putstr(ahttp->request.headers, name, value);
		free(line);
	}

	return ahttp->request.status;
}

static int parseBody(http* ahttp, struct evbuffer* in) {

	// handle static data case
	if (ahttp->request.contentlength == 0) {

		return HTTP_REQ_DONE;

	} else if (ahttp->request.contentlength > 0) {

		if (ahttp->request.contentlength > ahttp->request.bodyin) {
			size_t maxread = ahttp->request.contentlength - ahttp->request.bodyin;
			if (maxread > 0 && evbuffer_get_length(in) > 0) {
				ahttp->request.bodyin += httpAddInbuf(in, http, maxread);
			}
		}

		if (ahttp->request.contentlength == ahttp->request.bodyin) {
			return HTTP_REQ_DONE;
		}

	} else {

		// check if Transfer-Encoding is chunked
		const char* tranenc = ahttp->request.headers->getstr(ahttp->request.headers, "Transfer-Encoding", false);

		if (tranenc != NULL && !strcmp(tranenc, "chunked")) {
			// TODO: handle chunked encoding
			for (;;) {

				ssize_t chunksize = parseChunkedBody(http, in);

				if (chunksize > 0) {
					continue;
				} else if (chunksize == 0) {
					return HTTP_REQ_DONE;
				} else if (chunksize == -1) {
					return ahttp->request.status;
				} else {
					return HTTP_ERROR;
				}
			}
		} else {
			return HTTP_REQ_DONE;
		}
	}

	return ahttp->request.status;
}

/**
* parse chunked body and append it to inbuf
*
* @return number of bytes in a chunk. so 0 for the ending chunk. -1 for not enough data, -2 format error.
*/
static ssize_t parseChunkedBody(http* ahttp, struct evbuffer* in) {

	// peek chunk size.
	size_t crlf_len = 0;
	char* line		= evbufferPeekln(in, &crlf_len, EVBUFFER_EOL_CRLF);

	if (line == NULL)return -1; // not enough data.

	size_t linelen = strlen(line);

	// parse chunk size
	int chunksize = -1;
	sscanf(line, "%x", &chunksize);
	free(line);

	if (chunksize < 0)return -2; // format error

	// check if we've received whole data of this chunk.
	size_t datalen	= linelen + crlf_len + chunksize + crlf_len;
	size_t inbuflen = evbuffer_get_length(in);

	if (inbuflen < datalen) {
		return -1; // not enough data.
	}

	// copy chunk body
	evbufferDrainln(in, NULL, EVBUFFER_EOL_CRLF);
	httpAddInbuf(in, ahttp, chunksize);
	evbufferDrainln(in, NULL, EVBUFFER_EOL_CRLF);

	return chunksize;
}

/**
* validate file path
*/
static bool isValidPathname(const char* path) {

	if (path == NULL) return false;

	int len = strlen(path);
	if (len == 0 || len >= PATH_MAX) return false;
	else if (path[0] != '/') return false;
	else if (strpbrk(path, "\\:*?\"<>|") != NULL) return false;

	// check folder name length
	int n;
	char* t;

	for (n = 0, t = (char *) path; *t != '\0'; t++) {
		if (*t == '/') {
			n = 0;
			continue;
		}

		if (n >= FILENAME_MAX) {
			DEBUG("Filename too long.");
			return false;
		}

		n++;
	}

	return true;
}

/**
* Correct pathname.
*
* @note
* remove : heading & tailing white spaces, double slashes, tailing slash
*/
static void correctPathname(char* path) {

	// take care of head & tail white spaces
	strTrim(path);

	// take care of double slashes
	while (strstr(path, "//") != NULL) strReplace("sr", path, "//", "/");

	// take care of tailing slash
	int len = strlen(path);

	if (len <= 1) return;

	if (path[len - 1] == '/') path[len - 1] = '\0';
}

static char* evbufferPeekln(struct evbuffer* buffer, size_t* n_read_out, enum evbuffer_eol_style eol_style) {

	// Check if first line has arrived.
	struct evbuffer_ptr ptr = evbuffer_search_eol(buffer, NULL, n_read_out,eol_style);

	if (ptr.pos == -1) return NULL;

	char* line = (char*) malloc(ptr.pos + 1);

	if (line == NULL) return NULL;

	// Linearizes buffer
	if (ptr.pos > 0) {
		char* bufferptr = (char*) evbuffer_pullup(buffer, ptr.pos);
		ASSERT(bufferptr != NULL);
		strncpy(line, bufferptr, ptr.pos);
	}

	line[ptr.pos] = '\0';

	return line;
}

static ssize_t evbufferDrainln(struct evbuffer* buffer, size_t* n_read_out, enum evbuffer_eol_style eol_style) {

	char *line = evbuffer_readln(buffer, n_read_out, eol_style);
	if (line == NULL)
	return -1;
	size_t linelen = strlen(line);
	free(line);
	return linelen;
}
