#include "server.h"

int my_http_get_handler(short event, ad_conn_t *conn, void *userdata) {

	if (event & AD_EVENT_READ) {

		if (ad_http_get_status(conn) == AD_HTTP_REQ_DONE) {

			ad_http_response(conn, 200, "text/html", "Hello World", 11);
			return ad_http_is_keepalive_request(conn) ? AD_DONE : AD_CLOSE;
		}
	}

	return AD_OK;
}

int my_http_default_handler(short event, ad_conn_t *conn, void *userdata) {
	if (event & AD_EVENT_READ) {
		if (ad_http_get_status(conn) == AD_HTTP_REQ_DONE) {
			ad_http_response(conn, 501, "text/html", "Not implemented", 15);
			return AD_CLOSE; // Close connection.
		}
	}

	return AD_OK;
}


int main(int argc, char **argv) {

	logLevel(LOG_DEBUG);
	server* webserver = serverNew();
	serverSetOption(webserver, "server.port", "8888");
	// HTTP Parser is also a hook.
	serverRegisterHook(webserver, ad_http_handler, NULL);
	serverRegisterHookOnMethod(webserver, "GET", my_http_get_handler, NULL);
	serverRegisterHook(webserver, my_http_default_handler, NULL);

	return serverStart(webserver);
}
