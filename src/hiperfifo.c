/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2016, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#include "hiperfifo.h"

CURLM *multi;
int running, uploads, retries, curl409;

int curlUploads( void ) {
	return uploads;
}

int curlRetries( void ) {
        return uploads; 
}

int curlConflicts( void ) {
	return curl409;
}


/* Request clean exit on first Ctrl-C, or force exit on multiple retries */
static void signal_handler( int sig ) {
	if ( curl_terminate++ > 2)
		exit(0);
}

void curlInit() {
	curl_terminate = 0;
	signal( SIGINT, signal_handler );
	signal( SIGTERM, signal_handler );

	curl_global_init( CURL_GLOBAL_ALL );
	multi = curl_multi_init();
	running = uploads = retries = curl409 = 0;

	slist_headers = NULL;
	slist_headers = curl_slist_append( slist_headers, "Accept: application/json" );
	slist_headers = curl_slist_append( slist_headers, "Content-Type: application/json" );
	slist_headers = curl_slist_append( slist_headers, "charsets: utf-8" );
}

/* Die if we get a bad CURLMcode somewhere */
void mcode_or_die( const char *where, CURLMcode code ) {
	const char *s;
	CURLMcode rc = code;

	if ( CURLM_CALL_MULTI_PERFORM == code ) {
		rc = curl_multi_perform( multi, &running );
	}

	switch ( rc ) {
	case     CURLM_CALL_MULTI_PERFORM:
	case     CURLM_OK:                 return;

	case     CURLM_BAD_SOCKET:         s = "CURLM_BAD_SOCKET";         break;
	case     CURLM_BAD_HANDLE:         s = "CURLM_BAD_HANDLE";         break;
	case     CURLM_BAD_EASY_HANDLE:    s = "CURLM_BAD_EASY_HANDLE";    break;
	case     CURLM_OUT_OF_MEMORY:      s = "CURLM_OUT_OF_MEMORY";      break;
	case     CURLM_INTERNAL_ERROR:     s = "CURLM_INTERNAL_ERROR";     break;
	case     CURLM_UNKNOWN_OPTION:     s = "CURLM_UNKNOWN_OPTION";     break;

//	case     CURLM_ADDED_ALREADY:
	case     CURLM_LAST:
	default:              printf( "\n    Unexpected curl error:  %s  \n",where );
		return;
	}

	printf( "\n\n Curl fatal error:%s,%s\n",where,s );
	exit( code );
}

static time_t lastretry = 0;
void multi_retry( CURLcode res, CURL *easy ) {
	time_t timenow;
	long responseCode;

	/* check for couchDB merge conflict */
	if (res == CURLE_HTTP_RETURNED_ERROR) {
		curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responseCode);
		if (responseCode == 409) {
			/* resubmit with no limits */
			curlQueue( easy );
			curl409++;
			return;
		}
	}

	timenow = time( NULL );
	if ( ((res == CURLE_OPERATION_TIMEDOUT) || (res == CURLE_COULDNT_RESOLVE_HOST))
								&& (timenow != lastretry)) {
		/* resubmit with rate limits */
		curlQueue( easy );
		lastretry = timenow;
		retries++;
		return;
	} 

	/* fail silently and remove handle */
	curl_easy_cleanup( easy );
}

/* Check for completed transfers, and remove their easy handles */
void check_multi_info() {
	CURLMsg *msg;
	int msgs_left;
	CURL *easy;
	CURLcode res;

	while ( ( msg = curl_multi_info_read( multi, &msgs_left ) ) ) {
		if ( msg->msg == CURLMSG_DONE ) {
			easy = msg->easy_handle;
			res = msg->data.result;
			curl_multi_remove_handle( multi, easy );

			if ( res == CURLE_OK ) {
				curl_easy_cleanup( easy );
				uploads++;
			} else {
				multi_retry( res, easy );
			}
		}
	}
}


/* Add a new easy handle to the global curl_multi */
void curlQueue( CURL *easy_handle ) {
	CURLMcode rc;

	if ( !easy_handle ) {
		return;
	}

	/* drop messages if the queue is blocked */
	if ( running >= 20 ) {
		// first check if any have recently finished
		rc = curl_multi_perform( multi, &running );
		mcode_or_die( "curlQueue: curl_multi_perform", rc );
	}
	if ( running >= 30 ) {
		// drop if still blocked
		curl_easy_cleanup( easy_handle );
		return;
	}

	rc = curl_multi_add_handle( multi, easy_handle );
	mcode_or_die( "curlQueue: curl_multi_add_handle", rc );
}

void curlPush() {
	CURLMcode rc;

	// clear completed handles
	check_multi_info();

	// start new handles
	rc = curl_multi_perform( multi, &running );
	mcode_or_die( "curlPush: curl_multi_perform", rc );
}

void curlClean() {
	check_multi_info();
	curl_multi_cleanup( multi );
	curl_slist_free_all(slist_headers);
	curl_global_cleanup();
}
