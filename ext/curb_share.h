/* curb_share.h - Curl share interface
 * Wraps the libcurl share interface (curl_share_init / curl_share_setopt),
 * allowing multiple Curl::Easy handles across Ruby threads to share a
 * connection cache, DNS cache, SSL session cache, and cookie jar.
 */
#ifndef CURB_SHARE_H
#define CURB_SHARE_H

#include <curl/curl.h>
#include <pthread.h>
#include "ruby.h"

extern VALUE cCurlShare;

typedef struct {
    CURLSH          *share;
    pthread_mutex_t  mutexes[CURL_LOCK_DATA_LAST]; /* one per curl_lock_data slot */
    int              closed;
} ruby_curl_share;

void init_curb_share(void);

#endif
