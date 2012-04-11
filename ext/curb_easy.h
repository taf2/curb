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

#ifdef CURL_VERSION_SSL
#if LIBCURL_VERSION_NUM >= 0x070b00
#  if LIBCURL_VERSION_NUM <= 0x071004
#    define CURB_FTPSSL         CURLOPT_FTP_SSL
#    define CURB_FTPSSL_ALL     CURLFTPSSL_ALL
#    define CURB_FTPSSL_TRY     CURLFTPSSL_TRY
#    define CURB_FTPSSL_CONTROL CURLFTPSSL_CONTROL
#    define CURB_FTPSSL_NONE    CURLFTPSSL_NONE
#  else
#    define CURB_FTPSSL         CURLOPT_USE_SSL
#    define CURB_FTPSSL_ALL     CURLUSESSL_ALL
#    define CURB_FTPSSL_TRY     CURLUSESSL_TRY
#    define CURB_FTPSSL_CONTROL CURLUSESSL_CONTROL
#    define CURB_FTPSSL_NONE    CURLUSESSL_NONE
#  endif
#endif
#endif

/* a lot of this *could* be kept in the handler itself,
 * but then we lose the ability to query it's status.
 */
typedef struct {
  /* The handler */
  CURL *curl;

  VALUE opts; /* rather then allocate everything we might need to store, allocate a Hash and only store objects we actually use... */
  VALUE multi; /* keep a multi handle alive for each easy handle not being used by a multi handle.  This improves easy performance when not within a multi context */

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
  long low_speed_limit;
  long low_speed_time;
  long ssl_version;
  long use_ssl;
  long ftp_filemethod;
  unsigned short resolve_mode;

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
  char ignore_content_length;
  char callback_active;

  struct curl_slist *curl_headers;
  struct curl_slist *curl_ftp_commands;

  int last_result; /* last result code from multi loop */

} ruby_curl_easy;

extern VALUE cCurlEasy;

VALUE ruby_curl_easy_setup(ruby_curl_easy *rbce);
VALUE ruby_curl_easy_cleanup(VALUE self, ruby_curl_easy *rbce);

void init_curb_easy();

#endif
