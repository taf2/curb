/* curb_multi.h - Curl easy mode
 * Copyright (c)2008 Todd A. Fisher.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id$
 */
#ifndef __CURB_MULTI_H
#define __CURB_MULTI_H

#include "curb.h"
#include <curl/multi.h>

struct st_table;

typedef struct {
  int active;
  int running;
  char closed;
  CURLM *handle;
  struct st_table *attached;
} ruby_curl_multi;

extern VALUE cCurlMulti;
extern const rb_data_type_t ruby_curl_multi_data_type;

void init_curb_multi();
void rb_curl_multi_forget_easy(ruby_curl_multi *rbcm, void *rbce_ptr);


#endif
