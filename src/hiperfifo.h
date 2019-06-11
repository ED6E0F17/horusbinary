#include <curl/curl.h>

volatile int	  curl_terminate;
struct curl_slist *slist_headers;

void curlInit();
void curlPush();
void curlClean();
void curlQueue( CURL *easy_handle );
int curlUploads();
int curlRetries();
int curlConflicts();
