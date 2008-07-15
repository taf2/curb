/* curb_postfield.h - Field class for POST method
 * Copyright (c)2006 Ross Bamford. 
 * Licensed under the Ruby License. See LICENSE for details.
 * 
 * $Id: curb_postfield.h 4 2006-11-17 18:35:31Z roscopeco $
 */
#ifndef __CURB_POSTFIELD_H
#define __CURB_POSTFIELD_H

#include "curb.h"

/* 
 * postfield doesn't actually wrap a curl_httppost - instead,
 * it just holds together some ruby objects and has a C-side 
 * method to add it to a given form list during the perform.
 */
typedef struct {  
  /* Objects we associate */
  VALUE name;  
  VALUE content;
  VALUE content_type;
  VALUE content_proc;
  VALUE local_file;
  VALUE remote_file;
  
  /* this will sometimes hold a string, which is the result
   * of the content_proc invocation. We need it to hang around.
   */
  VALUE buffer_str;
} ruby_curl_postfield;

extern VALUE cCurlPostField;

void append_to_form(VALUE self, 
                    struct curl_httppost **first, 
                    struct curl_httppost **last);

void init_curb_postfield();

#endif
