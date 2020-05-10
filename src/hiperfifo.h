#include <curl/curl.h>

extern struct curl_slist *slist_headers;

int curl_terminated();
void curlInit();
void curlPush();
void curlClean();
void curlQueue( CURL *easy_handle );
int curlUploads();
int curlRetries();
int curlConflicts();
