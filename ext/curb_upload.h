/* curb_upload.h - Curl upload handle
 * Copyright (c)2009 Todd A Fisher. 
 * Licensed under the Ruby License. See LICENSE for details.
 */
#ifndef __CURB_UPLOAD_H
#define __CURB_UPLOAD_H

#include "curb.h"

#include <curl/easy.h>

/*
 * Maintain the state of an upload e.g. for putting large streams with very little memory
 * out to a server. via PUT requests
 */
typedef struct {
  VALUE stream;
  size_t offset;
} ruby_curl_upload;

extern VALUE cCurlUpload;
void init_curb_upload();

VALUE ruby_curl_upload_new(VALUE klass);
VALUE ruby_curl_upload_stream_set(VALUE self, VALUE stream);
VALUE ruby_curl_upload_stream_get(VALUE self);
VALUE ruby_curl_upload_offset_set(VALUE self, VALUE offset);
VALUE ruby_curl_upload_offset_get(VALUE self);

#endif
