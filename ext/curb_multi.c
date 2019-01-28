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
static char cCurlMutiAutoClose = 0;

static void rb_curl_multi_remove(ruby_curl_multi *rbcm, VALUE easy);
static void rb_curl_multi_read_info(VALUE self, CURLM *mptr);
static void rb_curl_multi_run(VALUE self, CURLM *multi_handle, int *still_running);

static VALUE callback_exception(VALUE unused) {
  return Qfalse;
}

void curl_multi_free(ruby_curl_multi *rbcm) {
  curl_multi_cleanup(rbcm->handle);
  free(rbcm);
}

static void ruby_curl_multi_init(ruby_curl_multi *rbcm) {
  rbcm->handle = curl_multi_init();
  if (!rbcm->handle) {
    rb_raise(mCurlErrFailedInit, "Failed to initialize multi handle");
  }

  rbcm->active = 0;
  rbcm->running = 0;
}

/*
 * call-seq:
 *   Curl::Multi.new                                   => #&lt;Curl::Easy...&gt;
 *
 * Create a new Curl::Multi instance
 */
VALUE ruby_curl_multi_new(VALUE klass) {
  ruby_curl_multi *rbcm = ALLOC(ruby_curl_multi);

  ruby_curl_multi_init(rbcm);

  /*
   * The mark routine will be called by the garbage collector during its ``mark'' phase.
   * If your structure references other Ruby objects, then your mark function needs to
   * identify these objects using rb_gc_mark(value). If the structure doesn't reference
   * other Ruby objects, you can simply pass 0 as a function pointer.
  */
  return Data_Wrap_Struct(klass, 0, curl_multi_free, rbcm);
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
  cCurlMutiDefaulttimeout = NUM2LONG(timeout);
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
  return LONG2NUM(cCurlMutiDefaulttimeout);
}

/*
 * call-seq:
 *   Curl::Multi.autoclose = true => true
 *
 * Automatically close open connections after each request. Otherwise, the connection will remain open 
 * for reuse until the next GC
 *
 */
VALUE ruby_curl_multi_set_autoclose(VALUE klass, VALUE onoff) {
  cCurlMutiAutoClose = ((onoff == Qtrue) ? 1 : 0);
  return onoff;
}

/*
 * call-seq:
 *   Curl::Multi.autoclose => true|false
 *
 * Get the global default autoclose setting for all Curl::Multi Handles.
 *
 */
VALUE ruby_curl_multi_get_autoclose(VALUE klass) {
  return cCurlMutiAutoClose == 1 ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *   multi.requests                                   => [#&lt;Curl::Easy...&gt;, ...]
 * 
 * Returns an array containing all the active requests on this Curl::Multi object.
 */
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

  curl_multi_setopt(rbcm->handle, CURLMOPT_MAXCONNECTS, NUM2LONG(count));
#endif

  return count;
}

/*
 * call-seq:
 * multi = Curl::Multi.new
 * multi.pipeline = true
 *
 * Pass a long set to 1 for HTTP/1.1 pipelining, 2 for HTTP/2 multiplexing, or 0 to disable.
 *  Enabling pipelining on a multi handle will make it attempt to perform HTTP Pipelining as 
 * far as possible for transfers using this handle. This means that if you add a second request 
 * that can use an already existing connection, the second request will be "piped" on the same
 * connection rather than being executed in parallel. (Added in 7.16.0, multiplex added in 7.43.0)
 *
 */
static VALUE ruby_curl_multi_pipeline(VALUE self, VALUE method) {
#ifdef HAVE_CURLMOPT_PIPELINING
  ruby_curl_multi *rbcm;

  long value;

  if (method == Qtrue) {
    value = 1;
  } else if (method == Qfalse) {
    value  = 0;
  } else {
    value = NUM2LONG(method);
  } 

  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  curl_multi_setopt(rbcm->handle, CURLMOPT_PIPELINING, value);
#endif
  return method == Qtrue ? 1 : 0;
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

  /* track a reference to associated multi handle */
  rbce->multi = self;

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
VALUE ruby_curl_multi_remove(VALUE self, VALUE rb_easy_handle) {
  ruby_curl_multi *rbcm;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  rb_curl_multi_remove(rbcm, rb_easy_handle);

  return self;
}
static void rb_curl_multi_remove(ruby_curl_multi *rbcm, VALUE easy) {
  CURLMcode result;
  ruby_curl_easy *rbce;

  Data_Get_Struct(easy, ruby_curl_easy, rbce);
  result = curl_multi_remove_handle(rbcm->handle, rbce->curl);
  if (result != 0) {
    raise_curl_multi_error_exception(result);
  }

  rbcm->active--;

  ruby_curl_easy_cleanup( easy, rbce );
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

  // remove the easy handle from multi on completion so it can be reused again
  rb_funcall(self, rb_intern("remove"), 1, easy);

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

}

static void rb_curl_multi_read_info(VALUE self, CURLM *multi_handle) {
  int msgs_left;

  CURLcode c_easy_result;
  CURLMsg *c_multi_result; // for picking up messages with the transfer status
  CURL *c_easy_handle;

  /* Check for finished easy handles and remove from the multi handle.
   * curl_multi_info_read will query for messages from individual handles.
   *
   * The messages fetched with this function are removed from the curl internal
   * queue and when there are no messages left it will return NULL (and break
   * the loop effectively).
   */
  while ((c_multi_result = curl_multi_info_read(multi_handle, &msgs_left))) {
    // A message is there, but we really care only about transfer completetion.
    if (c_multi_result->msg != CURLMSG_DONE) continue;

    c_easy_handle = c_multi_result->easy_handle;
    c_easy_result = c_multi_result->data.result; /* return code for transfer */

    rb_curl_mutli_handle_complete(self, c_easy_handle, c_easy_result);
  }
}

/* called within ruby_curl_multi_perform */
static void rb_curl_multi_run(VALUE self, CURLM *multi_handle, int *still_running) {
  CURLMcode mcode;

  /*
   * curl_multi_perform will return CURLM_CALL_MULTI_PERFORM only when it wants to be called again immediately.
   * When things are fine and there is nothing immediate it wants done, it'll return CURLM_OK.
   *
   * It will perform all pending actions on all added easy handles attached to this multi handle. We will loop
   * here as long as mcode is CURLM_CALL_MULTIPERFORM.
   */
  do {
    mcode = curl_multi_perform(multi_handle, still_running);
  } while (mcode == CURLM_CALL_MULTI_PERFORM);

  /*
   * Nothing more to do, check if an error occured in the loop above and raise an exception if necessary.
   */

  if (mcode != CURLM_OK) {
    raise_curl_multi_error_exception(mcode);
  }

  /*
   * Everything is ok, but this does not mean all the transfers are completed.
   * There is no data to read or write available for Curl at the moment.
   *
   * At this point we can return control to the caller to do something else while
   * curl is waiting for more actions to queue.
   */
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
  int maxfd, rc = -1;
  fd_set fdread, fdwrite, fdexcep;
#ifdef _WIN32
  fd_set crt_fdread, crt_fdwrite, crt_fdexcep;
#endif
  long timeout_milliseconds;
  struct timeval tv = {0, 0};
  struct timeval tv_100ms = {0, 100000};
  VALUE block = Qnil;
#if defined(HAVE_RB_THREAD_BLOCKING_REGION) || defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL)
  struct _select_set fdset_args;
#endif

  rb_scan_args(argc, argv, "0&", &block);

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  timeout_milliseconds = cCurlMutiDefaulttimeout;

  // Run curl_multi_perform for the first time to get the ball rolling
  rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );

  // Check the easy handles for new messages one more time before yielding
  // control to passed ruby block.
  //
  // This call will block until all queued messages are processed and if any
  // handle completed the transfer we will run the on_complete callback here too.
  rb_curl_multi_read_info( self, rbcm->handle );

  // There are no more messages to handle by curl and we can run the ruby block
  // passed to perform method.
  // When the block completes curl will resume.
  if (block != Qnil) {
    rb_funcall(block, rb_intern("call"), 1, self);
  }

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

      if (maxfd == -1) {
        /* libcurl recommends sleeping for 100ms */
        rb_thread_wait_for(tv_100ms);
        rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );
        rb_curl_multi_read_info( self, rbcm->handle );
        if (block != Qnil) { rb_funcall(block, rb_intern("call"), 1, self);  }
        continue;
      }

#ifdef _WIN32
      create_crt_fd(&fdread, &crt_fdread);
      create_crt_fd(&fdwrite, &crt_fdwrite);
      create_crt_fd(&fdexcep, &crt_fdexcep);
#endif


#if (defined(HAVE_RB_THREAD_BLOCKING_REGION) || defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL))
      fdset_args.maxfd = maxfd+1;
      fdset_args.fdread = &fdread;
      fdset_args.fdwrite = &fdwrite;
      fdset_args.fdexcep = &fdexcep;
      fdset_args.tv = &tv;
#endif

#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
      rc = (int)(VALUE) rb_thread_call_without_gvl((void *(*)(void *))curb_select, &fdset_args, RUBY_UBF_IO, 0);
#elif HAVE_RB_THREAD_BLOCKING_REGION
      rc = rb_thread_blocking_region(curb_select, &fdset_args, RUBY_UBF_IO, 0);
#elif HAVE_RB_THREAD_FD_SELECT
      rc = rb_thread_fd_select(maxfd+1, &fdread, &fdwrite, &fdexcep, &tv);
#else
      rc = rb_thread_select(maxfd+1, &fdread, &fdwrite, &fdexcep, &tv);
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
  if (cCurlMutiAutoClose  == 1) {
    rb_funcall(self, rb_intern("close"), 0);
  }
  return Qtrue;
}

/*
 * call-seq:
 *
 * multi.close
 * after closing the multi handle all connections will be closed and the handle will no longer be usable
 *
 */
VALUE ruby_curl_multi_close(VALUE self) {
  ruby_curl_multi *rbcm;
  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  curl_multi_cleanup(rbcm->handle);
  ruby_curl_multi_init(rbcm);
  return self;
}


/* =================== INIT LIB =====================*/
void init_curb_multi() {
  idCall = rb_intern("call");
  cCurlMulti = rb_define_class_under(mCurl, "Multi", rb_cObject);

  /* Class methods */
  rb_define_singleton_method(cCurlMulti, "new", ruby_curl_multi_new, 0);
  rb_define_singleton_method(cCurlMulti, "default_timeout=", ruby_curl_multi_set_default_timeout, 1);
  rb_define_singleton_method(cCurlMulti, "default_timeout", ruby_curl_multi_get_default_timeout, 0);
  rb_define_singleton_method(cCurlMulti, "autoclose=", ruby_curl_multi_set_autoclose, 1);
  rb_define_singleton_method(cCurlMulti, "autoclose", ruby_curl_multi_get_autoclose, 0);
  /* Instance methods */
  rb_define_method(cCurlMulti, "max_connects=", ruby_curl_multi_max_connects, 1);
  rb_define_method(cCurlMulti, "pipeline=", ruby_curl_multi_pipeline, 1);
  rb_define_method(cCurlMulti, "_add", ruby_curl_multi_add, 1);
  rb_define_method(cCurlMulti, "_remove", ruby_curl_multi_remove, 1);
  rb_define_method(cCurlMulti, "perform", ruby_curl_multi_perform, -1);
  rb_define_method(cCurlMulti, "_close", ruby_curl_multi_close, 0);
}
