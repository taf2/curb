/* curb_easy.h - Curl easy mode
 * Copyright (c)2006 Ross Bamford.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id: curb_easy.h 25 2006-12-07 23:38:25Z roscopeco $
 */
#ifndef __CURB_EASY_H
#define __CURB_EASY_H

#include "curb.h"

#include <curl/easy.h>

/* a lot of this *could* be kept in the handler itself,
 * but then we lose the ability to query it's status.
 */
typedef struct {
  /* The handler */
  CURL *curl;

  VALUE opts; /* rather then allocate everything we might need to store, allocate a Hash and only store objects we actually use... */

  /* Other opts */
  unsigned short local_port;       // 0 is no port
  unsigned short local_port_range; // "  "  " "
  unsigned short proxy_port;       // "  "  " "
  int proxy_type;
  long http_auth_types;
  long proxy_auth_types;
  long max_redirs;
  unsigned long timeout;
  unsigned long connect_timeout;
  long dns_cache_timeout;
  unsigned long ftp_response_timeout;

  /* bool flags */
  char proxy_tunnel;
  char fetch_file_time;
  char ssl_verify_peer;
  char ssl_verify_host;
  char header_in_body;
  char use_netrc;
  char follow_location;
  char unrestricted_auth;
  char verbose;
  char multipart_form_post;
  char enable_cookies;
  struct curl_slist *curl_headers;

  int last_result; /* last result code from multi loop */

} ruby_curl_easy;

extern VALUE cCurlEasy;

VALUE ruby_curl_easy_setup(ruby_curl_easy *rbce, struct curl_slist **headers);
VALUE ruby_curl_easy_cleanup(VALUE self, ruby_curl_easy *rbce, struct curl_slist *headers);

void init_curb_easy();

#endif
