/* Curb - Libcurl(3) bindings for Ruby.
 * Copyright (c)2006 Ross Bamford.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id: curb.h 39 2006-12-23 15:28:45Z roscopeco $
 */

#ifndef __CURB_H
#define __CURB_H

#include <ruby.h>
#include <curl/curl.h>

#include "curb_config.h"
#include "curb_easy.h"
#include "curb_errors.h"
#include "curb_postfield.h"
#include "curb_multi.h"

#include "curb_macros.h"

// These should be managed from the Rake 'release' task.
#define CURB_VERSION   "0.4.6.0"
#define CURB_VER_NUM   460
#define CURB_VER_MAJ   0
#define CURB_VER_MIN   4
#define CURB_VER_MIC   6
#define CURB_VER_PATCH 0


// Maybe not yet defined in Ruby
#ifndef RSTRING_LEN
#define RSTRING_LEN(x) RSTRING(x)->len
#endif
#ifndef RSTRING_PTR
#define RSTRING_PTR(x) RSTRING(x)->ptr
#endif

#ifdef HAVE_RUBY19_HASH
  #define RHASH_LEN(hash) RHASH(hash)->ntbl->num_entries
#else
  #define RHASH_LEN(hash) RHASH(hash)->tbl->num_entries
#endif

#ifdef HAVE_RUBY_THREAD_BLOCKING_REGION
  /* define internal ruby blocking/unblocking methods */
  struct rb_blocking_region_buffer *rb_thread_blocking_region_begin(void);
  void rb_thread_blocking_region_end(struct rb_blocking_region_buffer *region);
  VALUE rb_thread_blocking_region( rb_blocking_function_t *func, void *data1, rb_unblock_function_t *ubf, void *data2);

  #define RB_UNLOCK_BEGIN() do { struct rb_blocking_region_buffer *__curb__lock = rb_thread_blocking_region_begin();
  #define RB_UNLOCK_RESUME() __curb__lock = rb_thread_blocking_region_begin()
  #define RB_UNLOCK_PAUSE()  rb_thread_blocking_region_end(__curb__lock)
  #define RB_UNLOCK_END()  rb_thread_blocking_region_end(__curb__lock); } while(0)
  #define RB_SELECT select
#else
  #define RB_UNLOCK_BEGIN()
  #define RB_UNLOCK_RESUME()
  #define RB_UNLOCK_PAUSE()
  #define RB_UNLOCK_END()
  #define RB_SELECT rb_thread_select
#endif

extern VALUE mCurl;

extern void Init_curb_core();

#endif
