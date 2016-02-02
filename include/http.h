/**
 * @abstruct HTTP library
 * @author rockmetoo <rockmetoo@gmail.com>
*/

#ifndef __http_h__
#define __http_h__

#include "common.h"
#include "hashtable.h"
#include "list.h"
#include "listtable.h"

#ifdef __cplusplus
extern "C" {
#endif

// HTTP PROTOCOL CODES
#define HTTP_PROTOCOL_09 "HTTP/0.9"
#define HTTP_PROTOCOL_10 "HTTP/1.0"
#define HTTP_PROTOCOL_11 "HTTP/1.1"

// HTTP RESPONSE CODES
#define HTTP_NO_RESPONSE				(0)
#define HTTP_CODE_CONTINUE				(100)
#define HTTP_CODE_OK					(200)
#define HTTP_CODE_CREATED				(201)
#define HTTP_CODE_NO_CONTENT			(204)
#define HTTP_CODE_PARTIAL_CONTENT		(206)
#define HTTP_CODE_MULTI_STATUS			(207)
#define HTTP_CODE_MOVED_TEMPORARILY		(302)
#define HTTP_CODE_NOT_MODIFIED			(304)
#define HTTP_CODE_BAD_REQUEST			(400)
#define HTTP_CODE_UNAUTHORIZED			(401)
#define HTTP_CODE_FORBIDDEN				(403)
#define HTTP_CODE_NOT_FOUND				(404)
#define HTTP_CODE_METHOD_NOT_ALLOWED	(405)
#define HTTP_CODE_REQUEST_TIME_OUT		(408)
#define HTTP_CODE_GONE					(410)
#define HTTP_CODE_REQUEST_URI_TOO_LONG	(414)
#define HTTP_CODE_LOCKED				(423)
#define HTTP_CODE_INTERNAL_SERVER_ERROR (500)
#define HTTP_CODE_NOT_IMPLEMENTED		(501)
#define HTTP_CODE_SERVICE_UNAVAILABLE	(503)

// DEFAULT BEHAVIORS
#define HTTP_CRLF "\r\n"
#define HTTP_DEF_CONTENT_TYPE "application/octet-stream"

// Hook type
#define HOOK_ALL				(0)			// call on each and every phases
#define HOOK_ON_CONNECT			(1)			// call right after the establishment of connection
#define HOOK_AFTER_REQUESTLINE	(1 << 2)	// call after parsing request line
#define HOOK_AFTER_HEADER		(1 << 3)	// call after parsing all headers
#define HOOK_ON_BODY			(1 << 4)	// call on every time body data received
#define HOOK_ON_REQUEST			(1 << 5)	// call with complete request
#define HOOK_ON_CLOSE			(1 << 6)	// call right before closing or next request

enum http_request_status_e {
	HTTP_REQ_INIT = 0,			// initial state
	HTTP_REQ_REQUESTLINE_DONE,	// received 1st line
	HTTP_REQ_HEADER_DONE,		// received headers completely
	HTTP_REQ_DONE,				// received body completely. no more data expected
	HTTP_ERROR,					// unrecoverable error found
};

struct http_t {

	// HTTP Request
	struct {

		enum http_request_status_e status;	// request status.
		struct evbuffer* inbuf;				// input data buffer.
		// request line - available on REQ_REQUESTLINE_DONE.
		char* method;						// request method ex) GET
		char* uri;							// url+query ex) /data%20path?query=the%20value
		char* httpver;						// version ex) HTTP/1.1
		char* path;							// decoded path ex) /data path
		char* query;						// query string ex) query=the%20value
		// request header - available on REQ_HEADER_DONE.
		listtable* headers;					// parsed request header entries
		char* host;							// host ex) www.domain.com or www.domain.com:8080
		char* domain;						// domain name ex) www.domain.com (no port number)
		off_t contentlength;				// value of Content-Length header.*/
		size_t bodyin;						// bytes moved to in-buff
	} request;

	// HTTP Response
	struct {
		struct evbuffer* outbuf;			// output data buffer.
		bool frozen_header;					// indicator whether we sent header out or not
		// response headers
		int code;							// response status-code
		char* reason;						// reason-phrase
		listtable* headers;					// response header entries
		off_t contentlength;				// content length in response
		size_t bodyout;						// bytes added to out-buffer
	} response;
};

typedef struct http_t http;

// public functions
extern int							httpHandler(short event, connection* conn, void* userdata);
extern enum http_request_status_e	httpGetStatus(connection* conn);
extern struct evbuffer*				httpGetInbuf(connection* conn);
extern struct evbuffer*				httpGetOutbuf(connection* conn);
extern const char*					httpGetRequestHeader(connection* conn, const char* name);
extern off_t						httpGetContentLength(connection* conn);
extern void*						httpGetContent(connection* conn, size_t maxsize, size_t* storedsize);
extern int							httpIsKeepaliveRequest(connection* conn);
extern int							httpSetResponseHeader(connection* conn, const char* name, const char* value);
extern const char*					httpGetResponseHeader(connection* conn, const char* name);
extern int							httpSetResponseCode(connection* conn, int code, const char* reason);
extern int							httpSetResponseContent(connection* conn, const char* contenttype, off_t size);
extern size_t						httpResponse(connection* conn, int code, const char* contenttype, const void* data, off_t size);
extern size_t						httpSendHeader(connection* conn);
extern size_t						httpSendData(connection* conn, const void* data, size_t size);
extern size_t						httpSendChunk(connection* conn, const void* data, size_t size);
extern const char*					httpGetReason(int code);

#ifdef __cplusplus
}
#endif
#endif
