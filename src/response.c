/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996,97 Jon Nelson <nels0988@tc.umn.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* boa: response.c */

#include "boa.h"
#include <sys/uio.h>

static char e_s[MAX_HEADER_LENGTH * 3];
static char ka_phrase[75];

void print_content_type(request * req)
{
	req_write(req, "Content-Type: ");
	req_write(req, get_mime_type(req->request_uri));
	req_write(req, "\r\n");
}

void print_content_length(request * req)
{
	req_write(req, "Content-Length: ");
	req_write(req, simple_itoa(req->filesize));
	req_write(req, "\r\n");
}

void print_last_modified(request * req)
{
	static char lm[] = "Last-Modified: "
	"                             "
	"\r\n";
	rfc822_time_buf(lm + 15, req->last_modified);
	req_write(req, lm);
}

void init_ka_phrase(void)
{
	sprintf(ka_phrase, "Connection: Keep-Alive\r\n"
			"Keep-Alive: timeout=%d, max=%d\r\n",
			ka_timeout, ka_max);

}

void print_ka_phrase(void)
{
	fprintf(stderr, "ka_phrase: \"%s\"\n", ka_phrase);
}

void print_http_headers(request * req)
{
	static char stuff[] = "Date: "
	"                             "
	"\r\nServer: "
	SERVER_VERSION
	"\r\n";

	rfc822_time_buf(stuff + 6, 0);
	req_write(req, stuff);

	if (req->keepalive == KA_ACTIVE) {
		req_write(req, ka_phrase);
	} else
		req_write(req, "Connection: close\r\n");
}

/* The routines above are only called by the routines below.
 * The rest of Boa only enters through the routines below.
 */

/* R_REQUEST_OK: 200 */
void send_r_request_ok(request * req)
{
	req->response_status = R_REQUEST_OK;
	if (req->simple)
		return;

	req_write(req, "HTTP/1.0 200 OK\r\n");
	print_http_headers(req);
 
	if (!req->is_cgi) {
		print_content_length(req);
		print_last_modified(req);
		print_content_type(req);
		req_write(req, "\r\n");
	}
}

/* R_MOVED_PERM: 301 */
void send_redirect_perm(request * req, char *url)
{
	req->response_status = R_MOVED_PERM;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 301 Moved Permanently\r\n");
		print_http_headers(req);
		req_write(req, "Content-Type: text/html\r\n");

		req_write(req, "Location: ");
		req_write(req, escape_string(url, e_s));
		req_write(req, "\r\n\r\n");
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>301 Moved Permanently</TITLE></HEAD>\n"
				  "<BODY>\n<H1>301 Moved</H1>The document has moved\n"
				  "<A HREF=\"");
		req_write(req, escape_string(url, e_s));
		req_write(req, "\">here</A>.\n</BODY></HTML>\n");
	}
	req_flush(req);
}

/* R_MOVED_TEMP: 302 */
void send_redirect_temp(request * req, char *url)
{

	req->response_status = R_MOVED_TEMP;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 302 Moved Temporarily\r\n");
		print_http_headers(req);
		req_write(req, "Content-Type: text/html\r\n");

		req_write(req, "Location: ");
		req_write(req, escape_string(url, e_s));
		req_write(req, "\r\n\r\n");
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>302 Moved Temporarily</TITLE></HEAD>\n"
				  "<BODY>\n<H1>302 Moved</H1>The document has moved\n"
				  "<A HREF=\"");
		req_write(req, escape_string(url, e_s));
		req_write(req, "\">here</A>.\n</BODY></HTML>\n");
	}
	req_flush(req);
}

/* R_NOT_MODIFIED: 304 */
void send_r_not_modified(request * req)
{
	req_write(req, "HTTP/1.0 304 Not Modified\r\n");
	req->response_status = R_NOT_MODIFIED;
	print_http_headers(req);
	print_content_type(req);
	req_write(req, "\r\n");		/* terminate header */
	req_flush(req);
}

/* R_BAD_REQUEST: 400 */
void send_r_bad_request(request * req)
{
	req->response_status = R_BAD_REQUEST;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 400 Bad Request\r\n");
		print_http_headers(req);
		req_write(req, "Content-Type: text/html\r\n\r\n");	/* terminate header */
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n"
				"<BODY><H1>400 Bad Request</H1>\nYour client has issued "
				  "a malformed or illegal request.\n</BODY></HTML>\n");
	}
	req_flush(req);
}

/* R_UNAUTHORIZED: 401 */
void send_r_unauthorized(request * req, char *realm_name)
{
	req->response_status = R_UNAUTHORIZED;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 401 Unauthorized\r\n");
		print_http_headers(req);
		req_write(req, "WWW-Authenticate: Basic realm=\"");
		req_write(req, realm_name);
		req_write(req, "\"\r\n");
		req_write(req, "Content-Type: text/html\r\n\r\n");	/* terminate header */
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>401 Unauthorized</TITLE></HEAD>\n"
				  "<BODY><H1>401 Unauthorized</H1>\nYour client does not "
				  "have permission to get URL ");
		req_write(req, escape_string(req->request_uri, e_s));
		req_write(req, " from this server.\n</BODY></HTML>\n");
	}
	req_flush(req);
}

/* R_FORBIDDEN: 403 */
void send_r_forbidden(request * req)
{
	req->response_status = R_FORBIDDEN;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 403 Forbidden\r\n");
		print_http_headers(req);
		req_write(req, "Content-Type: text/html\r\n\r\n");	/* terminate header */
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n"
				  "<BODY><H1>403 Forbidden</H1>\nYour client does not "
				  "have permission to get URL ");
		req_write(req, escape_string(req->request_uri, e_s));
		req_write(req, " from this server.\n</BODY></HTML>\n");
	}
	req_flush(req);
}

/* R_NOT_FOUND: 404 */
void send_r_not_found(request * req)
{
	req->response_status = R_NOT_FOUND;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 404 Not Found\r\n");
		print_http_headers(req);
		req_write(req, "Content-Type: text/html\r\n\r\n");	/* terminate header */
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n"
				  "<BODY><H1>404 Not Found</H1>\nThe requested URL ");
		req_write(req, escape_string(req->request_uri, e_s));
		req_write(req, " was not found on this server.\n</BODY></HTML>\n");
	}
	req_flush(req);
}

/* R_ERROR: 500 */
void send_r_error(request * req)
{
	req->response_status = R_ERROR;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 500 Server Error\r\n");
		print_http_headers(req);
		req_write(req, "Content-Type: text/html\r\n\r\n");	/* terminate header */
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>500 Server Error</TITLE></HEAD>\n"
			   "<BODY><H1>500 Server Error</H1>\nThe server encountered "
			   "an internal error and could not complete your request.\n"
				  "</BODY></HTML>\n");
	}
	req_flush(req);
}

/* R_NOT_IMP: 501 */
void send_r_not_implemented(request * req)
{
	req->response_status = R_NOT_IMP;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 501 Not Implemented\r\n");
		print_http_headers(req);
		req_write(req, "Content-Type: text/html\r\n\r\n");	/* terminate header */
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>\n"
				"<BODY><H1>501 Not Implemented</H1>\nPOST to non-script "
				  "is not supported in Boa.\n</BODY></HTML>\n");
	}
	req_flush(req);
}

/* R_NOT_IMP: 505 */
void send_r_bad_version(request * req)
{
	req->response_status = R_BAD_VERSION;
	if (!req->simple) {
		req_write(req, "HTTP/1.0 505 HTTP Version Not Supported\r\n");
		print_http_headers(req);
		req_write(req, "Content-Type: text/html\r\n\r\n");	/* terminate header */
	}
	if (req->method != M_HEAD) {
		req_write(req, "<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n"
		  "<BODY><H1>505 HTTP Version Not Supported</H1>\nHTTP versions "
				  "other than 0.9 and 1.0 "
			   "are not supported in Boa.\n<p><p>Version encountered: ");
		req_write(req, req->http_version);
		req_write(req, "<p><p></BODY></HTML>\n");
	}
	req_flush(req);
}
