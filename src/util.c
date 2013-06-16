
/*
 *  Boa, an http server
 *  Copyright (C) 1995 Paul Phillips <psp@well.com>
 *  Some changes Copyright (C) 1996,97 Larry Doolittle <ldoolitt@jlab.org>
 *  Some changes Copyright (C) 1996 Charles F. Randall <crandall@goldsys.com>
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

/* boa: util.c */

#include "boa.h"
#include <ctype.h>

#define HEX_TO_DECIMAL(char1, char2)	\
  (((char1 >= 'A') ? (((char1 & 0xdf) - 'A') + 10) : (char1 - '0')) * 16) + \
  (((char2 >= 'A') ? (((char2 & 0xdf) - 'A') + 10) : (char2 - '0')))

#define INT_TO_HEX(x) \
  ((((x)-10)>=0)?('A'+((x)-10)):('0'+(x)))

static time_t cur_time;
const char month_tab[48] = "Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec ";
const char day_tab[] = "Sun,Mon,Tue,Wed,Thu,Fri,Sat,";

#include "escape.h"


/*
 * Name: clean_pathname
 * 
 * Description: Replaces unsafe/incorrect instances of:
 *  //[...] with /
 *  /./ with /
 *  /../ with / (technically not what we want, but browsers should deal 
 *   with this, not servers)
 *
 * Stops parsing at '?'
 */

void clean_pathname(char *pathname)
{
	char *cleanpath, c;
	int cgiarg = 0;

	cleanpath = pathname;
	while ((c = *pathname++)) {
		if (c == '/' && !cgiarg) {
			while (1) {
				if (*pathname == '/')
					pathname++;
				else if (*pathname == '.' && *(pathname + 1) == '/')
					pathname += 2;
				else if (*pathname == '.' && *(pathname + 1) == '.' &&
						 *(pathname + 2) == '/') {
					pathname += 3;
				} else
					break;
			}
			*cleanpath++ = '/';
		} else {
			*cleanpath++ = c;
			cgiarg |= (c == '?');
		}
	}

	*cleanpath = '\0';
}


/*
 * Name: modified_since
 * Description: Decides whether a file's mtime is newer than the 
 * If-Modified-Since header of a request.
 * 
 * RETURN VALUES:
 *  0: File has not been modified since specified time.
 *  1: File has been.
 */

int modified_since(time_t * mtime, char *if_modified_since)
{
	struct tm *file_gmt;
	char *ims_info;
	char monthname[MAX_HEADER_LENGTH + 1];
	int day, month, year, hour, minute, second;
	int comp;

	ims_info = if_modified_since;

	/* the pre-space in the third scanf skips whitespace for the string */
	if (sscanf(ims_info, "%d %3s %d %d:%d:%d GMT",	/* RFC 1123 */
			   &day, monthname, &year, &hour, &minute, &second) == 6);
	else if (sscanf(ims_info, "%d-%3s-%d %d:%d:%d GMT",		/* RFC 1036 */
					&day, monthname, &year, &hour, &minute, &second) == 6)
		year += 1900;
	else if (sscanf(ims_info, " %3s %d %d:%d:%d %d",	/* asctime() format */
				  monthname, &day, &hour, &minute, &second, &year) == 6);
	else
		return -1;				/* error */

	file_gmt = gmtime(mtime);
	month = month2int(monthname);

	/* Go through from years to seconds -- if they are ever unequal,
	   we know which one is newer and can return */

	if ((comp = 1900 + file_gmt->tm_year - year))
		return (comp > 0);
	if ((comp = file_gmt->tm_mon - month))
		return (comp > 0);
	if ((comp = file_gmt->tm_mday - day))
		return (comp > 0);
	if ((comp = file_gmt->tm_hour - hour))
		return (comp > 0);
	if ((comp = file_gmt->tm_min - minute))
		return (comp > 0);
	if ((comp = file_gmt->tm_sec - second))
		return (comp > 0);

	return 0;					/* this person must really be into the latest/greatest */
}

/*
 * Name: month2int
 * 
 * Description: Turns a three letter month into a 0-11 int
 * 
 * Note: This function is from wn-v1.07 -- it's clever and fast
 */

int month2int(char *monthname)
{
	switch (*monthname) {
	case 'A':
		return (*++monthname == 'p' ? 3 : 7);
	case 'D':
		return (11);
	case 'F':
		return (1);
	case 'J':
		if (*++monthname == 'a')
			return (0);
		return (*++monthname == 'n' ? 5 : 6);
	case 'M':
		return (*(monthname + 2) == 'r' ? 2 : 4);
	case 'N':
		return (10);
	case 'O':
		return (9);
	case 'S':
		return (8);
	default:
		return (-1);
	}
}


/*
 * Name: to_upper
 * 
 * Description: Turns a string into all upper case (for HTTP_ header forming)
 * AND changes - into _ 
 */

char *to_upper(char *str)
{
	char *start = str;

	while (*str) {
		if (*str == '-')
			*str = '_';
		else
			*str = toupper(*str);

		str++;
	}

	return start;
}

/*
 * Name: unescape_uri
 *
 * Description: Decodes a uri, changing %xx encodings with the actual 
 * character.  The query_string should already be gone.
 * 
 * Return values:
 *  1: success
 *  0: illegal string
 */

int unescape_uri(char *uri)
{
	char c, d;
	char *uri_old;

	uri_old = uri;

	while ((c = *uri_old)) {
		if (c == '%') {
			uri_old++;
			if ((c = *uri_old++) && (d = *uri_old++))
				*uri++ = HEX_TO_DECIMAL(c, d);
			else
				return 0;		/* NULL in chars to be decoded */
		} else {
			*uri++ = c;
			uri_old++;
		}
	}

	*uri = '\0';
	return 1;
}

/*
 * Name: close_unused_fds
 *
 * Description: Closes child's unused file descriptors, in particular
 * all the active transaction channels.  Leaves std* untouched.
 * Call once for request_block, and once for request_ready.
 *
 */

void close_unused_fds(request * head)
{
	request *current;
	for (current = head; current; current = current->next) {
		close(current->fd);
	}
}

/*
 * Name: fixup_server_root
 *
 * Description: Makes sure the server root is valid.
 *
 */

void fixup_server_root()
{
	char *dirbuf;
	int dirbuf_size;

	if (!server_root) {
#ifdef SERVER_ROOT
		server_root = strdup(SERVER_ROOT);
#else
		fputs(
				 "boa: don't know where server root is.  Please #define "
				 "SERVER_ROOT in boa.h\n"
				 "and recompile, or use the -c command line option to "
				 "specify it.\n", stderr);
		exit(1);
#endif
	}
	if (chdir(server_root) == -1) {
		fprintf(stderr, "Could not chdir to %s: aborting\n", server_root);
		exit(1);
	}
	if (server_root[0] == '/')
		return;

	/* if here, server_root (as specified on the command line) is
	 * a relative path name. CGI programs require SERVER_ROOT
	 * to be absolute.
	 */

	dirbuf_size = MAX_PATH_LENGTH * 2 + 1;
	if ((dirbuf = (char *) malloc(dirbuf_size)) == NULL) {
		fprintf(stderr,
				"boa: Cannot malloc %d bytes of memory. Aborting.\n",
				dirbuf_size);
		exit(1);
	}
	if (getcwd(dirbuf, dirbuf_size) == NULL) {
		if (errno == ERANGE)
			perror("boa: getcwd() failed - unable to get working directory. "
				   "Aborting.");
		else if (errno == EACCES)
			perror("boa: getcwd() failed - No read access in current "
				   "directory. Aborting.");
		else
			perror("boa: getcwd() failed - unknown error. Aborting.");
		exit(1);
	}
	fprintf(stderr,
			"boa: Warning, the server_root directory specified"
			" on the command line,\n"
			"%s, is relative. CGI programs expect the environment\n"
			"variable SERVER_ROOT to be an absolute path.\n"
		 "Setting SERVER_ROOT to %s to conform.\n", server_root, dirbuf);
	free(server_root);
	server_root = dirbuf;
}

/* rfc822 (1123) time is exactly 29 characters long
 * "Sun, 06 Nov 1994 08:49:37 GMT"
 */

int req_write_rfc822_time(request * req, time_t s)
{
	struct tm *t;
	char *p;
	unsigned int a;

	if (req->buffer_end + 29 > BUFFER_SIZE)
		return 0;

	if (!s) {
		time(&cur_time);
		t = gmtime(&cur_time);
	} else
		t = gmtime(&s);

	p = req->buffer + req->buffer_end + 28;
	/* p points to the last char in the buf */

	p -= 3;						/* p points to where the ' ' will go */
	memcpy(p--, " GMT", 4);

	a = t->tm_sec;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_min;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_hour;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ' ';
	a = 1900 + t->tm_year;
	while (a) {
		*p-- = '0' + a % 10;
		a /= 10;
	}
	/* p points to an unused spot to where the space will go */
	p -= 3;
	/* p points to where the first char of the month will go */
	memcpy(p--, month_tab + 4 * (t->tm_mon), 4);
	*p-- = ' ';
	a = t->tm_mday;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ' ';
	p -= 3;
	memcpy(p, day_tab + t->tm_wday * 4, 4);

	req->buffer_end += 29;
	req_flush(req);
	return 29;
}

/* rfc822 (1123) time is exactly 29 characters long
 * "Sun, 06 Nov 1994 08:49:37 GMT"
 */

void rfc822_time_buf(char *buf, time_t s)
{
	struct tm *t;
	char *p;
	unsigned int a;

	if (!s) {
		time(&cur_time);
		t = gmtime(&cur_time);
	} else
		t = gmtime(&s);

	p = buf + 28;
	/* p points to the last char in the buf */

	p -= 3;		
	/* p points to where the ' ' will go */
	memcpy(p--, " GMT", 4);

	a = t->tm_sec;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_min;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_hour;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ' ';
	a = 1900 + t->tm_year;
	while (a) {
		*p-- = '0' + a % 10;
		a /= 10;
	}
	/* p points to an unused spot to where the space will go */
	p -= 3;
	/* p points to where the first char of the month will go */
	memcpy(p--, month_tab + 4 * (t->tm_mon), 4);
	*p-- = ' ';
	a = t->tm_mday;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ' ';
	p -= 3;
	memcpy(p, day_tab + t->tm_wday * 4, 4);
}


/*
 * Name: get_commonlog_time
 *
 * Description: Returns the current time in common log format in a static
 * char buffer.
 *
 * commonlog time is exactly 24 characters long
 * because this is only used in logging, we add "[" before and "] " after
 * making 27 characters
 * "27/Feb/1998:20:20:04 GMT"
 */

char *get_commonlog_time(void)
{
	struct tm *t;
	char *p;
	unsigned int a;
	static char buf[27];

	time(&cur_time);
	t = gmtime(&cur_time);

	p = buf + 27 - 1 - 5;

	memcpy(p--, " GMT] ", 6);

	a = t->tm_sec;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_min;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_hour;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = 1900 + t->tm_year;
	while (a) {
		*p-- = '0' + a % 10;
		a /= 10;
	}
	/* p points to an unused spot */
	*p-- = '/';
	p -= 2;
	memcpy(p--, month_tab + 4 * (t->tm_mon), 3);
	*p-- = '/';
	a = t->tm_mday;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p = '[';
	return p;					/* should be same as returning buf */
}

char *simple_itoa(int i)
{
	/* 21 digits plus null terminator, good for 64-bit or smaller ints */
	/* for bigger ints, use a bigger buffer! */
	static char local[22];
	char *p = &local[21];
	*p-- = '\0';
	do {
		*p-- = '0' + i % 10;
		i /= 10;
	} while (i > 0);
	return p + 1;
}

/*
 * Name: escape_string
 * 
 * Description: escapes the string inp.  Uses variable buf.  If buf is
 *  NULL when the program starts, it will attempt to dynamically allocate
 *  the space that it needs, otherwise it will assume that the user 
 *  has already allocated enough space for the variable buf, which
 *  could be up to 3 times the size of inp.  If the routine dynamically
 *  allocates the space, the user is responsible for freeing it afterwords
 * Returns: NULL on error, pointer to string otherwise.
 */

char *escape_string(char *inp, char *buf)
{
	int max;
	char *index;
	unsigned char c;

	max = strlen(inp) * 3;

	if (buf == NULL && max)
		buf = malloc(sizeof (char) * max + 1);

	if (buf == NULL)
		return NULL;

	index = buf;
	while ((c = *inp++) && max > 0) {
		if (needs_escape((unsigned int) c)) {
			*index++ = '%';
			*index++ = INT_TO_HEX(c >> 4);
			*index++ = INT_TO_HEX(c & 15);
		} else
			*index++ = c;
	}
	*index = '\0';
	return buf;
}

/*
 * Name: req_write
 * 
 * Description: Buffers data before sending to client.
 * Returns: -1 for error, otherwise how much is stored
 */

int req_write(request * req, char *msg)
{
	int msg_len;

	msg_len = strlen(msg);

	if (req->status == CLOSE || !msg_len)
		return req->buffer_end;

	if (req->buffer_end + msg_len > BUFFER_SIZE) {
		log_error_time();
		fprintf(stderr, "Ran out of Buffer space!\n");
		req->status = CLOSE;
		return -1;
	}
	memcpy(req->buffer + req->buffer_end, msg, msg_len);
	req->buffer_end += msg_len;
	return req->buffer_end;
}

/*
 * Name: flush_req
 * 
 * Description: Sends any backlogged buffer to client.
 *
 * Returns: -2 for error, -1 for blocked, otherwise how much is stored
 */

int req_flush(request * req)
{
	int bytes_to_write;

	bytes_to_write = req->buffer_end - req->buffer_start;
	if (bytes_to_write) {
		int bytes_written;

		bytes_written = write(req->fd,
							  req->buffer + req->buffer_start,
							  bytes_to_write);

		if (bytes_written == -1)
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				return -1;		/* request blocked at the pipe level, but keep going */
			else {
				req->buffer_start = req->buffer_end = 0;
				if (errno != EPIPE)
					perror("buffer flush");		/* OK to disable if your logs get too big */
				return -2;
			}
		req->buffer_start += bytes_written;
	}
	if (req->buffer_start == req->buffer_end)
		req->buffer_start = req->buffer_end = 0;
	return req->buffer_end;		/* successful */
}
