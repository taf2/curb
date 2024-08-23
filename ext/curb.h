/* Curb - Libcurl(3) bindings for Ruby.
 * Copyright (c)2006 Ross Bamford.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id: curb.h 39 2006-12-23 15:28:45Z roscopeco $
 */

#ifndef __CURB_H
#define __CURB_H

#include <ruby.h>

#ifdef HAVE_RUBY_IO_H
#include "ruby/io.h"
#else
#include "rubyio.h" // ruby 1.8
#endif

#include <curl/curl.h>

#include "banned.h"
#include "curb_config.h"
#include "curb_easy.h"
#include "curb_errors.h"
#include "curb_postfield.h"
#include "curb_multi.h"

#include "curb_macros.h"

// These should be managed from the Rake 'release' task.
#define CURB_VERSION   "1.0.6"
#define CURB_VER_NUM   1006
#define CURB_VER_MAJ   1
#define CURB_VER_MIN   0
#define CURB_VER_MIC   6
#define CURB_VER_PATCH 0


// Maybe not yet defined in Ruby
#ifndef RSTRING_LEN
  #define RSTRING_LEN(x) RSTRING(x)->len
#endif

#ifndef RSTRING_PTR
  #define RSTRING_PTR(x) RSTRING(x)->ptr
#endif

#ifndef RHASH_SIZE
  #define RHASH_SIZE(hash) RHASH(hash)->tbl->num_entries
#endif

// ruby 1.8 does not provide the macro
#ifndef DBL2NUM
  #define DBL2NUM(dbl) rb_float_new(dbl)
#endif

extern VALUE mCurl;

extern void Init_curb_core();

#endif
