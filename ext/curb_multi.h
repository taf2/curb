/* curb_multi.h - Curl easy mode
 * Copyright (c)2008 Todd A. Fisher.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id$
 */
#ifndef __CURB_MULTI_H
#define __CURB_MULTI_H

#include "curb.h"
#include "curb_easy.h"
#include <curl/multi.h>

#if 0
typedef struct {
  int active;
  int running;
  VALUE requests; /* hash of handles currently added */
  CURLM *handle;
} ruby_curl_multi;
#endif

extern VALUE cCurlMulti;
void init_curb_multi();


#endif
