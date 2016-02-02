#include "server.h"

int my_http_get_handler(short event, connection* conn, void* userdata) {

	if (event & EVENT_READ) {

		if (httpGetStatus(conn) == HTTP_REQ_DONE) {

			httpResponse(conn, 200, "text/html", "Hello World", 11);

			return httpIsKeepaliveRequest(conn) ? DONE : CLOSE;
		}
	}

	return OK;
}

int my_http_default_handler(short event, connection* conn, void* userdata) {

	if (event & EVENT_READ) {

		if (httpGetStatus(conn) == HTTP_REQ_DONE) {

			httpResponse(conn, 501, "text/html", "Not implemented", 15);

			// close connection
			return CLOSE;
		}
	}

	return OK;
}


int main(int argc, char** argv) {

	logLevel(LOG_DEBUG);
	server* webserver = serverNew();
	serverSetOption(webserver, "server.port", "8888");
	// HTTP Parser is also a hook.
	serverRegisterHook(webserver, httpHandler, NULL);
	serverRegisterHookOnMethod(webserver, "GET", my_http_get_handler, NULL);
	serverRegisterHook(webserver, my_http_default_handler, NULL);

	return serverStart(webserver);
}
