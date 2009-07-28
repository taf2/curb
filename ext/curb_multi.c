/* curb_multi.c - Curl multi mode
 * Copyright (c)2008 Todd A. Fisher.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 */

#include "curb_config.h"
#ifdef HAVE_RUBY19_ST_H
  #include <ruby.h>
  #include <ruby/st.h>
#else
  #include <ruby.h>
  #include <st.h>
#endif
#include "curb_easy.h"
#include "curb_errors.h"
#include "curb_postfield.h"
#include "curb_multi.h"

#include <errno.h>

extern VALUE mCurl;
static VALUE idCall;

#ifdef RDOC_NEVER_DEFINED
  mCurl = rb_define_module("Curl");
#endif

VALUE cCurlMulti;

static void rb_curl_multi_remove(ruby_curl_multi *rbcm, VALUE easy);
static void rb_curl_multi_read_info(VALUE self, CURLM *mptr);

static void rb_curl_multi_mark_all_easy(VALUE key, VALUE rbeasy, ruby_curl_multi *rbcm) {
  //printf( "mark easy: 0x%X\n", (long)rbeasy );
  rb_gc_mark(rbeasy);
}

static void curl_multi_mark(ruby_curl_multi *rbcm) {
  rb_gc_mark(rbcm->requests);
  rb_hash_foreach( rbcm->requests, (int (*)())rb_curl_multi_mark_all_easy, (VALUE)rbcm );
}

static void curl_multi_flush_easy(VALUE key, VALUE easy, ruby_curl_multi *rbcm) {
  //rb_curl_multi_remove(rbcm, easy);
  CURLMcode result;
  ruby_curl_easy *rbce;
  Data_Get_Struct(easy, ruby_curl_easy, rbce);
  result = curl_multi_remove_handle(rbcm->handle, rbce->curl);
  if (result != 0) {
    raise_curl_multi_error_exception(result);
  }
  // XXX: easy handle may not be finished yet... so don't clean it GC pass will get it next time
  VALUE r = rb_hash_delete( rbcm->requests, easy );
  if( r != easy || r == Qnil ) {
    rb_raise(rb_eRuntimeError, "Critical:: Unable to remove easy from requests");
  }
}

static int
rb_hash_clear_i(VALUE key, VALUE value, VALUE dummy) {
  return ST_DELETE;
}

static void curl_multi_free(ruby_curl_multi *rbcm) {

  //printf("hash entries: %d\n", RHASH(rbcm->requests)->tbl->num_entries );
  if (rbcm && !rbcm->requests == Qnil && rb_type(rbcm->requests) == T_HASH && RHASH_LEN(rbcm->requests) > 0) {

    rb_hash_foreach( rbcm->requests, (int (*)())curl_multi_flush_easy, (VALUE)rbcm );

    rb_hash_foreach(rbcm->requests, rb_hash_clear_i, 0); //rb_hash_clear(rbcm->requests);
    rbcm->requests = Qnil;
  }
  curl_multi_cleanup(rbcm->handle);
  free(rbcm);
}

/*
 * call-seq:
 *   Curl::Multi.new                                   => #&lt;Curl::Easy...&gt;
 *
 * Create a new Curl::Multi instance
 */
VALUE ruby_curl_multi_new(VALUE klass) {
  VALUE new_curlm;

  ruby_curl_multi *rbcm = ALLOC(ruby_curl_multi);

  rbcm->handle = curl_multi_init();

  rbcm->requests = rb_hash_new();

  rbcm->active = 0;
  rbcm->running = 0;

  new_curlm = Data_Wrap_Struct(klass, curl_multi_mark, curl_multi_free, rbcm);

  return new_curlm;
}

// Hash#foreach callback for ruby_curl_multi_requests
static int ruby_curl_multi_requests_callback(VALUE key, VALUE value, VALUE result_array) {
  rb_ary_push(result_array, value);
  
  return ST_CONTINUE;
}

/*
 * call-seq:
 *   multi.requests                                   => [#&lt;Curl::Easy...&gt;, ...]
 * 
 * Returns an array containing all the active requests on this Curl::Multi object.
 */
static VALUE ruby_curl_multi_requests(VALUE self) {
  ruby_curl_multi *rbcm;
  
  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  
  VALUE result_array = rb_ary_new();
  
  // iterate over the requests hash, and stuff references into the array.
  rb_hash_foreach( rbcm->requests, ruby_curl_multi_requests_callback, result_array );
  
  return result_array;
}

/*
 * call-seq:
 *   multi.idle?                                      => true or false
 * 
 * Returns whether or not this Curl::Multi handle is processing any requests.  E.g. this returns
 * true when multi.requests.length == 0.
 */
static VALUE ruby_curl_multi_idle(VALUE self) {
  ruby_curl_multi *rbcm;
  
  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  
  if ( FIX2INT( rb_funcall(rbcm->requests, rb_intern("length"), 0) ) == 0 ) {
    return Qtrue;
  } else {
    return Qfalse;
  }
}

/*
 * call-seq:
 * multi = Curl::Multi.new
 * multi.max_connects = 800
 *
 * Set the max connections in the cache for a multi handle
 */
static VALUE ruby_curl_multi_max_connects(VALUE self, VALUE count) {
#ifdef HAVE_CURLMOPT_MAXCONNECTS
  ruby_curl_multi *rbcm;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  curl_multi_setopt(rbcm->handle, CURLMOPT_MAXCONNECTS, NUM2INT(count));
#endif

  return count;
}

/*
 * call-seq:
 * multi = Curl::Multi.new
 * multi.pipeline = true
 *
 * Pass a long set to 1 to enable or 0 to disable. Enabling pipelining on a multi handle will make it
 * attempt to perform HTTP Pipelining as far as possible for transfers using this handle. This means
 * that if you add a second request that can use an already existing connection, the second request will
 * be "piped" on the same connection rather than being executed in parallel. (Added in 7.16.0)
 *
 */
static VALUE ruby_curl_multi_pipeline(VALUE self, VALUE onoff) {
#ifdef HAVE_CURLMOPT_PIPELINING
  ruby_curl_multi *rbcm;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  curl_multi_setopt(rbcm->handle, CURLMOPT_PIPELINING, onoff == Qtrue ? 1 : 0);
#endif
  return onoff;
}

/*
 * call-seq:
 * multi = Curl::Multi.new
 * easy = Curl::Easy.new('url')
 *
 * multi.add(easy)
 *
 * Add an easy handle to the multi stack
 */
VALUE ruby_curl_multi_add(VALUE self, VALUE easy) {
  CURLMcode mcode;
  ruby_curl_easy *rbce;
  ruby_curl_multi *rbcm;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  Data_Get_Struct(easy, ruby_curl_easy, rbce);

  mcode = curl_multi_add_handle(rbcm->handle, rbce->curl);
  if (mcode != CURLM_CALL_MULTI_PERFORM && mcode != CURLM_OK) {
    raise_curl_multi_error_exception(mcode);
  }

  /* save a pointer to self */
  rbce->self = easy;

  /* setup the easy handle */
  ruby_curl_easy_setup( rbce, &(rbce->bodybuf), &(rbce->headerbuf), &(rbce->curl_headers) );

  rbcm->active++;
  if (mcode == CURLM_CALL_MULTI_PERFORM) {
    curl_multi_perform(rbcm->handle, &(rbcm->running));
  }

  rb_hash_aset( rbcm->requests, easy, easy );
  // active should equal INT2FIX(RHASH(rbcm->requests)->tbl->num_entries)

  if (rbcm->active > rbcm->running) {
    rb_curl_multi_read_info(self, rbcm->handle);
  }

  return self;
}

/*
 * call-seq:
 * multi = Curl::Multi.new
 * easy = Curl::Easy.new('url')
 *
 * multi.add(easy)
 *
 * # sometime later
 * multi.remove(easy)
 *
 * Remove an easy handle from a multi stack.
 *
 * Will raise an exception if the easy handle is not found
 */
VALUE ruby_curl_multi_remove(VALUE self, VALUE easy) {
  ruby_curl_multi *rbcm;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  ruby_curl_easy *rbce;
  Data_Get_Struct(easy, ruby_curl_easy, rbce);

  rb_curl_multi_remove(rbcm,easy);

  return self;
}
static void rb_curl_multi_remove(ruby_curl_multi *rbcm, VALUE easy) {
  CURLMcode result;
  ruby_curl_easy *rbce;
  Data_Get_Struct(easy, ruby_curl_easy, rbce);

  rbcm->active--;

  //printf( "calling rb_curl_multi_remove: 0x%X, active: %d\n", (long)easy, rbcm->active );

  result = curl_multi_remove_handle(rbcm->handle, rbce->curl);
  if (result != 0) {
    raise_curl_multi_error_exception(result);
  }

  ruby_curl_easy_cleanup( easy, rbce, rbce->bodybuf, rbce->headerbuf, rbce->curl_headers );
  rbce->headerbuf = Qnil;
  rbce->bodybuf = Qnil;

  // active should equal INT2FIX(RHASH(rbcm->requests)->tbl->num_entries)
  VALUE r = rb_hash_delete( rbcm->requests, easy );
  if( r != easy || r == Qnil ) {
    rb_raise(rb_eRuntimeError, "Critical:: Unable to remove easy from requests");
  }
}

// Hash#foreach callback for ruby_curl_multi_cancel
static int ruby_curl_multi_cancel_callback(VALUE key, VALUE value, ruby_curl_multi *rbcm) {
  rb_curl_multi_remove(rbcm, value);
  
  return ST_CONTINUE;
}

/*
 * call-seq:
 *   multi.cancel!
 * 
 * Cancels all requests currently being made on this Curl::Multi handle.  
 */
static VALUE ruby_curl_multi_cancel(VALUE self) {
  ruby_curl_multi *rbcm;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  
  rb_hash_foreach( rbcm->requests, ruby_curl_multi_cancel_callback, (VALUE)rbcm );
  
  // for chaining
  return self;
}

static void rb_curl_mutli_handle_complete(VALUE self, CURL *easy_handle, int result) {

  long response_code = -1;
  ruby_curl_easy *rbce = NULL;

  CURLcode ecode = curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, (char**)&rbce);

  if (ecode != 0) {
    raise_curl_easy_error_exception(ecode);
  }

  rbce->last_result = result; /* save the last easy result code */

  ruby_curl_multi_remove( self, rbce->self );

  if (rbce->complete_proc != Qnil) {
    rb_funcall( rbce->complete_proc, idCall, 1, rbce->self );
  }

  curl_easy_getinfo(rbce->curl, CURLINFO_RESPONSE_CODE, &response_code);

  if (result != 0) {
    if (rbce->failure_proc != Qnil) {
      rb_funcall( rbce->failure_proc, idCall, 2, rbce->self, rb_curl_easy_error(result) );
    }
  }
  else if (rbce->success_proc != Qnil &&
          ((response_code >= 200 && response_code < 300) || response_code == 0)) {
    /* NOTE: we allow response_code == 0, in the case of non http requests e.g. reading from disk */
    rb_funcall( rbce->success_proc, idCall, 1, rbce->self );
  }
  else if (rbce->failure_proc != Qnil &&
          (response_code >= 300 && response_code <= 999)) {
    rb_funcall( rbce->failure_proc, idCall, 2, rbce->self, rb_curl_easy_error(result) );
  }
  rbce->self = Qnil;
}

static void rb_curl_multi_read_info(VALUE self, CURLM *multi_handle) {
  int msgs_left, result;
  CURLMsg *msg;
  CURL *easy_handle;

  /* check for finished easy handles and remove from the multi handle */
  while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      easy_handle = msg->easy_handle;
      result = msg->data.result;
      if (easy_handle) {
        rb_curl_mutli_handle_complete(self, easy_handle, result);
      }
    }
  }
}

/* called within ruby_curl_multi_perform */
static void rb_curl_multi_run(VALUE self, CURLM *multi_handle, int *still_running) {
  CURLMcode mcode;

  do {
    mcode = curl_multi_perform(multi_handle, still_running);
  } while (mcode == CURLM_CALL_MULTI_PERFORM);

  if (mcode != CURLM_OK) {
    raise_curl_multi_error_exception(mcode);
  }

  rb_curl_multi_read_info( self, multi_handle );
}

/*
 * call-seq:
 * multi = Curl::Multi.new
 * easy1 = Curl::Easy.new('url')
 * easy2 = Curl::Easy.new('url')
 *
 * multi.add(easy1)
 * multi.add(easy2)
 *
 * multi.perform do
 *  # while idle other code my execute here
 * end
 *
 * Run multi handles, looping selecting when data can be transfered
 */
VALUE ruby_curl_multi_perform(int argc, VALUE *argv, VALUE self) {
  CURLMcode mcode;
  ruby_curl_multi *rbcm;
  int maxfd, rc;
  fd_set fdread, fdwrite, fdexcep;

  long timeout_milliseconds;
  struct timeval tv = {0, 0};
  VALUE block = Qnil;

  rb_scan_args(argc, argv, "0&", &block);

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );

  while(rbcm->running) {
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    /* load the fd sets from the multi handle */
    mcode = curl_multi_fdset(rbcm->handle, &fdread, &fdwrite, &fdexcep, &maxfd);
    if (mcode != CURLM_OK) {
      raise_curl_multi_error_exception(mcode);
    }

#ifdef HAVE_CURL_MULTI_TIMEOUT
    /* get the curl suggested time out */
    mcode = curl_multi_timeout(rbcm->handle, &timeout_milliseconds);
    if (mcode != CURLM_OK) {
      raise_curl_multi_error_exception(mcode);
    }
#else
    /* libcurl doesn't have a timeout method defined... make a wild guess */
    timeout_milliseconds = -1;
#endif
    //printf("libcurl says wait: %ld ms or %ld s\n", timeout_milliseconds, timeout_milliseconds/1000);

    if (timeout_milliseconds == 0) { /* no delay */
      rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );
      continue;
    }
    else if(timeout_milliseconds < 0) {
      timeout_milliseconds = 500; /* wait half a second, libcurl doesn't know how long to wait */
    }
#ifdef __APPLE_CC__
    if(timeout_milliseconds > 1000) {
      timeout_milliseconds = 1000; /* apple libcurl sometimes reports huge timeouts... let's cap it */
    }
#endif

    tv.tv_sec = timeout_milliseconds / 1000; // convert milliseconds to seconds
    tv.tv_usec = (timeout_milliseconds % 1000) * 1000; // get the remainder of milliseconds and convert to micro seconds

    rc = rb_thread_select(maxfd+1, &fdread, &fdwrite, &fdexcep, &tv);
    switch(rc) {
    case -1:
      rb_raise(rb_eRuntimeError, "select(): %s", strerror(errno));
      break;
    case 0:
      if (block != Qnil) {
        rb_funcall(block, rb_intern("call"), 1, self); 
      }
//      if (rb_block_given_p()) {
//        rb_yield(self);
//      }
    default: 
      rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );
      break;
    }

  }

  return Qtrue;
}

/* =================== INIT LIB =====================*/
void init_curb_multi() {
  idCall = rb_intern("call");

  cCurlMulti = rb_define_class_under(mCurl, "Multi", rb_cObject);

  /* Class methods */
  rb_define_singleton_method(cCurlMulti, "new", ruby_curl_multi_new, 0);
  
  /* "Attributes" */
  rb_define_method(cCurlMulti, "requests", ruby_curl_multi_requests, 0);
  rb_define_method(cCurlMulti, "idle?", ruby_curl_multi_idle, 0);
  
  /* Instnace methods */
  rb_define_method(cCurlMulti, "max_connects=", ruby_curl_multi_max_connects, 1);
  rb_define_method(cCurlMulti, "pipeline=", ruby_curl_multi_pipeline, 1);
  rb_define_method(cCurlMulti, "add", ruby_curl_multi_add, 1);
  rb_define_method(cCurlMulti, "remove", ruby_curl_multi_remove, 1);
  rb_define_method(cCurlMulti, "cancel!", ruby_curl_multi_cancel, 0);
  rb_define_method(cCurlMulti, "perform", ruby_curl_multi_perform, -1);
}
