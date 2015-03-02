/* curb_multi.c - Curl multi mode
 * Copyright (c)2008 Todd A. Fisher.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 */
#include "curb_config.h"
#include <ruby.h>
#ifdef HAVE_RUBY_ST_H
  #include <ruby/st.h>
#else
  #include <st.h>
#endif

#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
  #include <ruby/thread.h>
#endif

#include "curb_easy.h"
#include "curb_errors.h"
#include "curb_postfield.h"
#include "curb_multi.h"

#include <errno.h>

#ifdef _WIN32
  // for O_RDWR and O_BINARY
  #include <fcntl.h>
#endif

extern VALUE mCurl;
static VALUE idCall;

#ifdef RDOC_NEVER_DEFINED
  mCurl = rb_define_module("Curl");
#endif

VALUE cCurlMulti;

static long cCurlMutiDefaulttimeout = 100; /* milliseconds */

static void rb_curl_multi_remove(ruby_curl_multi *rbcm, VALUE easy);
static void rb_curl_multi_read_info(VALUE self, CURLM *mptr);
static void rb_curl_multi_run(VALUE self, CURLM *multi_handle, int *still_running);

static VALUE callback_exception(VALUE unused) {
  return Qfalse;
} 

static void curl_multi_mark(ruby_curl_multi *rbcm) {
  rb_gc_mark(rbcm->requests);
}

static void curl_multi_flush_easy(VALUE key, VALUE easy, ruby_curl_multi *rbcm) {
  CURLMcode result;
  ruby_curl_easy *rbce;

  Data_Get_Struct(easy, ruby_curl_easy, rbce);
  result = curl_multi_remove_handle(rbcm->handle, rbce->curl);
  if (result != 0) {
    raise_curl_multi_error_exception(result);
  }
}

static int
rb_hash_clear_i(VALUE key, VALUE value, VALUE dummy) {
  return ST_DELETE;
}

static void curl_multi_free(ruby_curl_multi *rbcm) {

  if (rbcm && !rbcm->requests == Qnil && rb_type(rbcm->requests) == T_HASH && RHASH_SIZE(rbcm->requests) > 0) {

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
  if (!rbcm->handle) {
    rb_raise(mCurlErrFailedInit, "Failed to initialize multi handle");
  }

  rbcm->requests = rb_hash_new();

  rbcm->active = 0;
  rbcm->running = 0;

  new_curlm = Data_Wrap_Struct(klass, curl_multi_mark, curl_multi_free, rbcm);

  return new_curlm;
}

/*
 * call-seq:
 *   Curl::Multi.default_timeout = 4 => 4
 *
 * Set the global default time out for all Curl::Multi Handles.  This value is used
 * when libcurl cannot determine a timeout value when calling curl_multi_timeout.
 *
 */
VALUE ruby_curl_multi_set_default_timeout(VALUE klass, VALUE timeout) {
  cCurlMutiDefaulttimeout = FIX2LONG(timeout);
  return timeout;
}

/*
 * call-seq:
 *   Curl::Multi.default_timeout = 4 => 4
 *
 * Get the global default time out for all Curl::Multi Handles.
 *
 */
VALUE ruby_curl_multi_get_default_timeout(VALUE klass) {
  return INT2FIX(cCurlMutiDefaulttimeout);
}

/* Hash#foreach callback for ruby_curl_multi_requests */
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
  VALUE result_array;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  
  result_array = rb_ary_new();
  
  /* iterate over the requests hash, and stuff references into the array. */
  rb_hash_foreach(rbcm->requests, ruby_curl_multi_requests_callback, result_array);
  
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

  /* setup the easy handle */
  ruby_curl_easy_setup( rbce );

  mcode = curl_multi_add_handle(rbcm->handle, rbce->curl);
  if (mcode != CURLM_CALL_MULTI_PERFORM && mcode != CURLM_OK) {
    raise_curl_multi_error_exception(mcode);
  }

  rbcm->active++;

  /* Increase the running count, so that the perform loop keeps running.
   * If this number is not correct, the next call to curl_multi_perform will correct it. */
  rbcm->running++;

  rb_hash_aset( rbcm->requests, easy, easy );

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

  rb_curl_multi_remove(rbcm,easy);

  return self;
}
static void rb_curl_multi_remove(ruby_curl_multi *rbcm, VALUE easy) {
  CURLMcode result;
  ruby_curl_easy *rbce;
  VALUE r;

  Data_Get_Struct(easy, ruby_curl_easy, rbce);

  result = curl_multi_remove_handle(rbcm->handle, rbce->curl);
  if (result != 0) {
    raise_curl_multi_error_exception(result);
  }

  rbcm->active--;

  ruby_curl_easy_cleanup( easy, rbce );

  // active should equal INT2FIX(RHASH(rbcm->requests)->tbl->num_entries)
  r = rb_hash_delete( rbcm->requests, easy );
  if( r != easy || r == Qnil ) {
    rb_warn("Possibly lost track of Curl::Easy VALUE, it may not be reclaimed by GC");
  }
}

/* Hash#foreach callback for ruby_curl_multi_cancel */
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
  
  /* for chaining */
  return self;
}

// on_success, on_failure, on_complete
static VALUE call_status_handler1(VALUE ary) {
  return rb_funcall(rb_ary_entry(ary, 0), idCall, 1, rb_ary_entry(ary, 1));
}
static VALUE call_status_handler2(VALUE ary) {
  return rb_funcall(rb_ary_entry(ary, 0), idCall, 2, rb_ary_entry(ary, 1), rb_ary_entry(ary, 2));
}

static void rb_curl_mutli_handle_complete(VALUE self, CURL *easy_handle, int result) {

  long response_code = -1;
  VALUE easy;
  ruby_curl_easy *rbce = NULL;
  VALUE callargs, val = Qtrue;

  CURLcode ecode = curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, (char**)&easy);

  Data_Get_Struct(easy, ruby_curl_easy, rbce);

  rbce->last_result = result; /* save the last easy result code */

  ruby_curl_multi_remove( self, easy );

  /* after running a request cleanup the headers, these are set before each request */
  if (rbce->curl_headers) {
    curl_slist_free_all(rbce->curl_headers);
    rbce->curl_headers = NULL;
  }

  if (ecode != 0) {
    raise_curl_easy_error_exception(ecode);
  }

  if (!rb_easy_nil("complete_proc")) {
    callargs = rb_ary_new3(2, rb_easy_get("complete_proc"), easy);
    rbce->callback_active = 1;
    val = rb_rescue(call_status_handler1, callargs, callback_exception, Qnil);
    rbce->callback_active = 0;
    //rb_funcall( rb_easy_get("complete_proc"), idCall, 1, easy );
  }

  curl_easy_getinfo(rbce->curl, CURLINFO_RESPONSE_CODE, &response_code);

  if (result != 0) {
    if (!rb_easy_nil("failure_proc")) {
      callargs = rb_ary_new3(3, rb_easy_get("failure_proc"), easy, rb_curl_easy_error(result));
      rbce->callback_active = 1;
      val = rb_rescue(call_status_handler2, callargs, callback_exception, Qnil);
      rbce->callback_active = 0;
      //rb_funcall( rb_easy_get("failure_proc"), idCall, 2, easy, rb_curl_easy_error(result) );
    }
  }
  else if (!rb_easy_nil("success_proc") &&
          ((response_code >= 200 && response_code < 300) || response_code == 0)) {
    /* NOTE: we allow response_code == 0, in the case of non http requests e.g. reading from disk */
    callargs = rb_ary_new3(2, rb_easy_get("success_proc"), easy);
    rbce->callback_active = 1;
    val = rb_rescue(call_status_handler1, callargs, callback_exception, Qnil);
    rbce->callback_active = 0;
    //rb_funcall( rb_easy_get("success_proc"), idCall, 1, easy );
  }
  else if (!rb_easy_nil("redirect_proc") &&
          (response_code >= 300 && response_code < 400)) {
    rbce->callback_active = 1;
    callargs = rb_ary_new3(3, rb_easy_get("redirect_proc"), easy, rb_curl_easy_error(result));
    rbce->callback_active = 0;
    val = rb_rescue(call_status_handler2, callargs, callback_exception, Qnil);
  }
  else if (!rb_easy_nil("missing_proc") &&
          (response_code >= 400 && response_code < 500)) {
    rbce->callback_active = 1;
    callargs = rb_ary_new3(3, rb_easy_get("missing_proc"), easy, rb_curl_easy_error(result));
    rbce->callback_active = 0;
    val = rb_rescue(call_status_handler2, callargs, callback_exception, Qnil);
  }
  else if (!rb_easy_nil("failure_proc") &&
          (response_code >= 500 && response_code <= 999)) {
    callargs = rb_ary_new3(3, rb_easy_get("failure_proc"), easy, rb_curl_easy_error(result));
    rbce->callback_active = 1;
    val = rb_rescue(call_status_handler2, callargs, callback_exception, Qnil);
    rbce->callback_active = 0;
    //rb_funcall( rb_easy_get("failure_proc"), idCall, 2, easy, rb_curl_easy_error(result) );
  }

  if (val == Qfalse) {
    rb_warn("uncaught exception from callback");
      // exception was raised?
    //fprintf(stderr, "exception raised from callback\n");
  }

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
  
}

#ifdef _WIN32
void create_crt_fd(fd_set *os_set, fd_set *crt_set)
{
  int i;
  crt_set->fd_count = os_set->fd_count;
  for (i = 0; i < os_set->fd_count; i++) {
    WSAPROTOCOL_INFO wsa_pi;
    // dupicate the SOCKET
    int r = WSADuplicateSocket(os_set->fd_array[i], GetCurrentProcessId(), &wsa_pi);
    SOCKET s = WSASocket(wsa_pi.iAddressFamily, wsa_pi.iSocketType, wsa_pi.iProtocol, &wsa_pi, 0, 0);
    // create the CRT fd so ruby can get back to the SOCKET
    int fd = _open_osfhandle(s, O_RDWR|O_BINARY);
    os_set->fd_array[i] = s;
    crt_set->fd_array[i] = fd;
  }
}

void cleanup_crt_fd(fd_set *os_set, fd_set *crt_set)
{
  int i;
  for (i = 0; i < os_set->fd_count; i++) {
    // cleanup the CRT fd
    _close(crt_set->fd_array[i]);
    // cleanup the duplicated SOCKET
    closesocket(os_set->fd_array[i]);
  }
}
#endif

#if defined(HAVE_RB_THREAD_BLOCKING_REGION) || defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL)
struct _select_set {
  int maxfd;
  fd_set *fdread, *fdwrite, *fdexcep;
  struct timeval *tv;
};

static VALUE curb_select(void *args) {
  struct _select_set* set = args;
  int rc = select(set->maxfd, set->fdread, set->fdwrite, set->fdexcep, set->tv);
  return INT2FIX(rc);
}
#endif

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
#ifdef _WIN32
  fd_set crt_fdread, crt_fdwrite, crt_fdexcep;
#endif
  long timeout_milliseconds;
  struct timeval tv = {0, 0};
  VALUE block = Qnil;
#if defined(HAVE_RB_THREAD_BLOCKING_REGION) || defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL)
  struct _select_set fdset_args;
#endif

  rb_scan_args(argc, argv, "0&", &block);

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  timeout_milliseconds = cCurlMutiDefaulttimeout;

  rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );
  rb_curl_multi_read_info( self, rbcm->handle );
  if (block != Qnil) { rb_funcall(block, rb_intern("call"), 1, self);  }
 
  do {
    while (rbcm->running) {

#ifdef HAVE_CURL_MULTI_TIMEOUT
      /* get the curl suggested time out */
      mcode = curl_multi_timeout(rbcm->handle, &timeout_milliseconds);
      if (mcode != CURLM_OK) {
        raise_curl_multi_error_exception(mcode);
      }
#else
      /* libcurl doesn't have a timeout method defined, initialize to -1 we'll pick up the default later */
      timeout_milliseconds = -1;
#endif

      if (timeout_milliseconds == 0) { /* no delay */
        rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );
        rb_curl_multi_read_info( self, rbcm->handle );
        if (block != Qnil) { rb_funcall(block, rb_intern("call"), 1, self);  }
        continue;
      }

      if (timeout_milliseconds < 0 || timeout_milliseconds > cCurlMutiDefaulttimeout) {
        timeout_milliseconds = cCurlMutiDefaulttimeout; /* libcurl doesn't know how long to wait, use a default timeout */
                                                        /* or buggy versions libcurl sometimes reports huge timeouts... let's cap it */
      }

      tv.tv_sec  = 0; /* never wait longer than 1 second */
      tv.tv_usec = (int)(timeout_milliseconds * 1000); /* XXX: int is the right type for OSX, what about linux? */

      FD_ZERO(&fdread);
      FD_ZERO(&fdwrite);
      FD_ZERO(&fdexcep);

      /* load the fd sets from the multi handle */
      mcode = curl_multi_fdset(rbcm->handle, &fdread, &fdwrite, &fdexcep, &maxfd);
      if (mcode != CURLM_OK) {
        raise_curl_multi_error_exception(mcode);
      }

#ifdef _WIN32
      create_crt_fd(&fdread, &crt_fdread);
      create_crt_fd(&fdwrite, &crt_fdwrite);
      create_crt_fd(&fdexcep, &crt_fdexcep);
#endif

#if defined(HAVE_RB_THREAD_BLOCKING_REGION) || defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL)
      fdset_args.maxfd = maxfd+1;
      fdset_args.fdread = &fdread;
      fdset_args.fdwrite = &fdwrite;
      fdset_args.fdexcep = &fdexcep;
      fdset_args.tv = &tv;
#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
      rc = (int)(VALUE) rb_thread_call_without_gvl((void *(*)(void *))curb_select, &fdset_args, RUBY_UBF_IO, 0);
#elif HAVE_RB_THREAD_BLOCKING_REGION
      rc = rb_thread_blocking_region(curb_select, &fdset_args, RUBY_UBF_IO, 0);
#elif HAVE_RB_THREAD_FD_SELECT
      rc = rb_thread_fd_select(maxfd+1, &fdread, &fdwrite, &fdexcep, &tv);
#else
      rc = rb_thread_select(maxfd+1, &fdread, &fdwrite, &fdexcep, &tv);
#endif

#endif

#ifdef _WIN32
      cleanup_crt_fd(&fdread, &crt_fdread);
      cleanup_crt_fd(&fdwrite, &crt_fdwrite);
      cleanup_crt_fd(&fdexcep, &crt_fdexcep);
#endif

      switch(rc) {
      case -1:
        if(errno != EINTR) {
          rb_raise(rb_eRuntimeError, "select(): %s", strerror(errno));
          break;
        }
      case 0: /* timeout */
      default: /* action */
        rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );
        rb_curl_multi_read_info( self, rbcm->handle );
        if (block != Qnil) { rb_funcall(block, rb_intern("call"), 1, self);  }
        break;
      }
    }

  } while( rbcm->running );

  rb_curl_multi_read_info( self, rbcm->handle );
  if (block != Qnil) { rb_funcall(block, rb_intern("call"), 1, self);  }
    
  return Qtrue;
}

/* =================== INIT LIB =====================*/
void init_curb_multi() {
  idCall = rb_intern("call");

  cCurlMulti = rb_define_class_under(mCurl, "Multi", rb_cObject);

  /* Class methods */
  rb_define_singleton_method(cCurlMulti, "new", ruby_curl_multi_new, 0);
  rb_define_singleton_method(cCurlMulti, "default_timeout=", ruby_curl_multi_set_default_timeout, 1);
  rb_define_singleton_method(cCurlMulti, "default_timeout", ruby_curl_multi_get_default_timeout, 0);
  
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
