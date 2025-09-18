/* curb_multi.c - Curl multi mode
 * Copyright (c)2008 Todd A. Fisher.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 */
#include "curb_config.h"
#include <ruby.h>
#ifdef HAVE_RUBY_IO_H
#include <ruby/io.h>
#endif
#ifdef HAVE_RUBY_ST_H
  #include <ruby/st.h>
#else
  #include <st.h>
#endif

#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
  #include <ruby/thread.h>
#endif
#ifdef HAVE_RUBY_FIBER_SCHEDULER_H
  #include <ruby/fiber/scheduler.h>
#endif

#include "curb_easy.h"
#include "curb_errors.h"
#include "curb_postfield.h"
#include "curb_multi.h"

#include <errno.h>
#include <stdarg.h>

/*
 * Optional socket-action debug logging. Enabled by defining CURB_SOCKET_DEBUG=1
 * at compile time (e.g. via environment variable passed to extconf.rb).
 */
#ifndef CURB_SOCKET_DEBUG
#define CURB_SOCKET_DEBUG 0
#endif
#if !CURB_SOCKET_DEBUG
#define curb_debugf(...) ((void)0)
#endif

#ifdef _WIN32
  // for O_RDWR and O_BINARY
  #include <fcntl.h>
#endif

#if 0 /* disabled curl_multi_wait in favor of scheduler-aware fdsets */
#include <stdint.h>  /* for intptr_t */

struct wait_args {
  CURLM *handle;
  long timeout_ms;
  int numfds;
};
static void *curl_multi_wait_wrapper(void *p) {
  struct wait_args *args = p;
  CURLMcode code = curl_multi_wait(args->handle, NULL, 0, args->timeout_ms, &args->numfds);
  return (void *)(intptr_t)code;
}
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

static int detach_easy_entry(st_data_t key, st_data_t val, st_data_t arg);
static void rb_curl_multi_detach_all(ruby_curl_multi *rbcm);
static void curl_multi_mark(void *ptr);

static VALUE callback_exception(VALUE did_raise, VALUE exception) {
  // TODO: we could have an option to enable exception reporting
/*  VALUE ret = rb_funcall(exception, rb_intern("message"), 0);
  VALUE trace = rb_funcall(exception, rb_intern("backtrace"), 0);
  if (RB_TYPE_P(trace, T_ARRAY) && RARRAY_LEN(trace) > 0) {
    printf("we got an exception: %s:%d\n", StringValueCStr(ret), RARRAY_LEN(trace));
    VALUE sep = rb_str_new_cstr("\n");
    VALUE trace_lines = rb_ary_join(trace, sep);
    if (RB_TYPE_P(trace_lines, T_STRING)) {
      printf("%s\n", StringValueCStr(trace_lines));
    } else {
      printf("trace is not a string??\n");
    }
  } else {
    printf("we got an exception: %s\nno stack available\n", StringValueCStr(ret));
  }
  */
  rb_hash_aset(did_raise, rb_easy_hkey("error"), exception);
  return exception;
}

static int detach_easy_entry(st_data_t key, st_data_t val, st_data_t arg) {
  ruby_curl_multi *rbcm = (ruby_curl_multi *)arg;
  VALUE easy = (VALUE)val;
  ruby_curl_easy *rbce = NULL;

  if (RB_TYPE_P(easy, T_DATA)) {
    Data_Get_Struct(easy, ruby_curl_easy, rbce);
  }

  if (!rbce) {
    return ST_CONTINUE;
  }

  if (rbcm && rbcm->handle && rbce->curl) {
    curl_multi_remove_handle(rbcm->handle, rbce->curl);
  }

  rbce->multi = Qnil;

  return ST_CONTINUE;
}

void rb_curl_multi_forget_easy(ruby_curl_multi *rbcm, void *rbce_ptr) {
  ruby_curl_easy *rbce = (ruby_curl_easy *)rbce_ptr;

  if (!rbcm || !rbce || !rbcm->attached) {
    return;
  }

  st_data_t key = (st_data_t)rbce;
  st_delete(rbcm->attached, &key, NULL);
}

static void rb_curl_multi_detach_all(ruby_curl_multi *rbcm) {
  if (!rbcm || !rbcm->attached) {
    return;
  }

  st_table *attached = rbcm->attached;
  rbcm->attached = NULL;

  st_foreach(attached, detach_easy_entry, (st_data_t)rbcm);

  st_free_table(attached);

  rbcm->active = 0;
  rbcm->running = 0;
}

void curl_multi_free(ruby_curl_multi *rbcm) {
  if (!rbcm) {
    return;
  }

  rb_curl_multi_detach_all(rbcm);

  if (rbcm->handle) {
    curl_multi_cleanup(rbcm->handle);
    rbcm->handle = NULL;
  }

  free(rbcm);
}

static void ruby_curl_multi_init(ruby_curl_multi *rbcm) {
  rbcm->handle = curl_multi_init();
  if (!rbcm->handle) {
    rb_raise(mCurlErrFailedInit, "Failed to initialize multi handle");
  }

  rbcm->active = 0;
  rbcm->running = 0;

  if (rbcm->attached) {
    st_free_table(rbcm->attached);
    rbcm->attached = NULL;
  }

  rbcm->attached = st_init_numtable();
  if (!rbcm->attached) {
    curl_multi_cleanup(rbcm->handle);
    rbcm->handle = NULL;
    rb_raise(rb_eNoMemError, "Failed to allocate multi attachment table");
  }
}

/*
 * call-seq:
 *   Curl::Multi.new                                   => #<Curl::Easy...>
 *
 * Create a new Curl::Multi instance
 */
VALUE ruby_curl_multi_new(VALUE klass) {
  ruby_curl_multi *rbcm = ALLOC(ruby_curl_multi);
  if (!rbcm) {
    rb_raise(rb_eNoMemError, "Failed to allocate memory for Curl::Multi");
  }

  MEMZERO(rbcm, ruby_curl_multi, 1);

  ruby_curl_multi_init(rbcm);

  /*
   * The mark routine will be called by the garbage collector during its ``mark'' phase.
   * If your structure references other Ruby objects, then your mark function needs to
   * identify these objects using rb_gc_mark(value). If the structure doesn't reference
   * other Ruby objects, you can simply pass 0 as a function pointer.
   */
  return Data_Wrap_Struct(klass, curl_multi_mark, curl_multi_free, rbcm);
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
 *   multi.requests                                   => [#<Curl::Easy...>, ...]
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
 * multi.max_host_connections = 1
 *
 * Set the max number of connections per host
 */
static VALUE ruby_curl_multi_max_host_connections(VALUE self, VALUE count) {
#ifdef HAVE_CURLMOPT_MAX_HOST_CONNECTIONS
  ruby_curl_multi *rbcm;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  curl_multi_setopt(rbcm->handle, CURLMOPT_MAX_HOST_CONNECTIONS, NUM2LONG(count));
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
    ruby_curl_easy_cleanup(easy, rbce);

    raise_curl_multi_error_exception(mcode);
  }

  rbcm->active++;

  /* Increase the running count, so that the perform loop keeps running.
   * If this number is not correct, the next call to curl_multi_perform will correct it. */
  rbcm->running++;

  if (!rbcm->attached) {
    rbcm->attached = st_init_numtable();
    if (!rbcm->attached) {
      curl_multi_remove_handle(rbcm->handle, rbce->curl);
      ruby_curl_easy_cleanup(easy, rbce);
      rb_raise(rb_eNoMemError, "Failed to allocate multi attachment table");
    }
  }

  st_insert(rbcm->attached, (st_data_t)rbce, (st_data_t)easy);

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

  if (rbcm->active > 0) {
    rbcm->active--;
  }

  ruby_curl_easy_cleanup( easy, rbce );

  rb_curl_multi_forget_easy(rbcm, rbce);
}

// on_success, on_failure, on_complete
static VALUE call_status_handler1(VALUE ary) {
  return rb_funcall(rb_ary_entry(ary, 0), idCall, 1, rb_ary_entry(ary, 1));
}
static VALUE call_status_handler2(VALUE ary) {
  return rb_funcall(rb_ary_entry(ary, 0), idCall, 2, rb_ary_entry(ary, 1), rb_ary_entry(ary, 2));
}

static void flush_stderr_if_any(ruby_curl_easy *rbce) {
  VALUE stderr_io = rb_easy_get("stderr_io");
  if (stderr_io != Qnil) {
    /* Flush via Ruby IO API */
    rb_funcall(stderr_io, rb_intern("flush"), 0);
#ifdef HAVE_RUBY_IO_H
    /* Additionally flush underlying FILE* to be extra safe. */
    rb_io_t *open_f_ptr;
    if (RB_TYPE_P(stderr_io, T_FILE)) {
      GetOpenFile(stderr_io, open_f_ptr);
      FILE *fp = rb_io_stdio_file(open_f_ptr);
      if (fp) fflush(fp);
    }
#endif
  }
}

/* Helper to locate the Ruby Easy VALUE from the attached table using the
 * underlying CURL* handle when CURLINFO_PRIVATE is unavailable or stale. */
struct find_easy_ctx { CURL *handle; VALUE easy; };
static int find_easy_by_handle_i(st_data_t key, st_data_t val, st_data_t arg) {
  ruby_curl_easy *rbce = (ruby_curl_easy *)key;
  struct find_easy_ctx *ctx = (struct find_easy_ctx *)arg;
  if (rbce && rbce->curl == ctx->handle) {
    ctx->easy = (VALUE)val;
    return ST_STOP;
  }
  return ST_CONTINUE;
}

static VALUE find_easy_by_handle(ruby_curl_multi *rbcm, CURL *easy_handle) {
  if (!rbcm || !rbcm->attached) return Qnil;
  struct find_easy_ctx ctx; ctx.handle = easy_handle; ctx.easy = Qnil;
  st_foreach(rbcm->attached, find_easy_by_handle_i, (st_data_t)&ctx);
  return ctx.easy;
}

static void rb_curl_mutli_handle_complete(VALUE self, CURL *easy_handle, int result) {
  long response_code = -1;
  VALUE easy = Qnil;
  ruby_curl_easy *rbce = NULL;
  VALUE callargs;
  ruby_curl_multi *rbcm = NULL;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  /* Try to recover the ruby_curl_easy pointer stored via CURLOPT_PRIVATE. */
  CURLcode private_rc = curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, (char**)&rbce);
  if (private_rc == CURLE_OK && rbce) {
    easy = rbce->self;
  }

  /* If PRIVATE is unavailable or invalid, fall back to scanning attachments. */
  if (NIL_P(easy) || !RB_TYPE_P(easy, T_DATA)) {
    easy = find_easy_by_handle(rbcm, easy_handle);
    if (!NIL_P(easy) && RB_TYPE_P(easy, T_DATA)) {
      Data_Get_Struct(easy, ruby_curl_easy, rbce);
    }
  }

  /* If we still cannot identify the easy handle, remove it and bail. */
  if (NIL_P(easy) || !RB_TYPE_P(easy, T_DATA) || !rbce) {
    if (rbcm && rbcm->handle && easy_handle) {
      curl_multi_remove_handle(rbcm->handle, easy_handle);
    }
    return;
  }

  rbce->last_result = result; /* save the last easy result code */

  /* Ensure any verbose output redirected via CURLOPT_STDERR is flushed
   * before we tear down handler state. */
  flush_stderr_if_any(rbce);

  // remove the easy handle from multi on completion so it can be reused again
  rb_funcall(self, rb_intern("remove"), 1, easy);

  /* after running a request cleanup the headers, these are set before each request */
  if (rbce->curl_headers) {
    curl_slist_free_all(rbce->curl_headers);
    rbce->curl_headers = NULL;
  }

  /* Flush again after removal to cover any last buffered data. */
  flush_stderr_if_any(rbce);

  VALUE did_raise = rb_hash_new();

  if (!rb_easy_nil("complete_proc")) {
    callargs = rb_ary_new3(2, rb_easy_get("complete_proc"), easy);
    rbce->callback_active = 1;
    rb_rescue(call_status_handler1, callargs, callback_exception, did_raise);
    rbce->callback_active = 0;
    CURB_CHECK_RB_CALLBACK_RAISE(did_raise);
  }

#ifdef HAVE_CURLINFO_RESPONSE_CODE
  curl_easy_getinfo(rbce->curl, CURLINFO_RESPONSE_CODE, &response_code);
#else /* use fdsets path for waiting */
  // old libcurl
  curl_easy_getinfo(rbce->curl, CURLINFO_HTTP_CODE, &response_code);
#endif
  long redirect_count;
  curl_easy_getinfo(rbce->curl, CURLINFO_REDIRECT_COUNT, &redirect_count);

  if (result != 0) {
    if (!rb_easy_nil("failure_proc")) {
      callargs = rb_ary_new3(3, rb_easy_get("failure_proc"), easy, rb_curl_easy_error(result));
      rbce->callback_active = 1;
      rb_rescue(call_status_handler2, callargs, callback_exception, did_raise);
      rbce->callback_active = 0;
      CURB_CHECK_RB_CALLBACK_RAISE(did_raise);
    }
  } else if (!rb_easy_nil("success_proc") &&
          ((response_code >= 200 && response_code < 300) || response_code == 0)) {
    /* NOTE: we allow response_code == 0, in the case of non http requests e.g. reading from disk */
    callargs = rb_ary_new3(2, rb_easy_get("success_proc"), easy);
    rbce->callback_active = 1;
    rb_rescue(call_status_handler1, callargs, callback_exception, did_raise);
    rbce->callback_active = 0;
    CURB_CHECK_RB_CALLBACK_RAISE(did_raise);

  } else if (!rb_easy_nil("redirect_proc") && ((response_code >= 300 && response_code < 400) || redirect_count > 0) ) {
    /* Skip on_redirect callback if follow_location is false AND max_redirects is 0 */
    if (!rbce->follow_location && rbce->max_redirs == 0) {
      // Do nothing - skip the callback
    } else {
      rbce->callback_active = 1;
      callargs = rb_ary_new3(3, rb_easy_get("redirect_proc"), easy, rb_curl_easy_error(result));
      rbce->callback_active = 0;
      rb_rescue(call_status_handler2, callargs, callback_exception, did_raise);
      CURB_CHECK_RB_CALLBACK_RAISE(did_raise);
    }
  } else if (!rb_easy_nil("missing_proc") &&
          (response_code >= 400 && response_code < 500)) {
    rbce->callback_active = 1;
    callargs = rb_ary_new3(3, rb_easy_get("missing_proc"), easy, rb_curl_easy_error(result));
    rbce->callback_active = 0;
    rb_rescue(call_status_handler2, callargs, callback_exception, did_raise);
    CURB_CHECK_RB_CALLBACK_RAISE(did_raise);
  } else if (!rb_easy_nil("failure_proc") &&
          (response_code >= 500 && response_code <= 999)) {
    callargs = rb_ary_new3(3, rb_easy_get("failure_proc"), easy, rb_curl_easy_error(result));
    rbce->callback_active = 1;
    rb_rescue(call_status_handler2, callargs, callback_exception, did_raise);
    rbce->callback_active = 0;
    CURB_CHECK_RB_CALLBACK_RAISE(did_raise);
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

#if defined(HAVE_CURL_MULTI_SOCKET_ACTION) && defined(HAVE_CURLMOPT_SOCKETFUNCTION) && defined(HAVE_CURLMOPT_TIMERFUNCTION) && defined(HAVE_RB_THREAD_FD_SELECT) && !defined(_WIN32)
/* ---- socket-action implementation (scheduler-friendly) ---- */
typedef struct {
  st_table *sock_map;     /* key: int fd, value: int 'what' (CURL_POLL_*) */
  long timeout_ms;        /* last timeout set by libcurl timer callback */
} multi_socket_ctx;

#if CURB_SOCKET_DEBUG
static void curb_debugf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  fflush(stderr);
  va_end(ap);
}

static const char *poll_what_str(int what, char *buf, size_t n) {
  /* what is one of CURL_POLL_*, not a bitmask except INOUT */
  if (what == CURL_POLL_REMOVE) snprintf(buf, n, "REMOVE");
  else if (what == CURL_POLL_IN) snprintf(buf, n, "IN");
  else if (what == CURL_POLL_OUT) snprintf(buf, n, "OUT");
  else if (what == CURL_POLL_INOUT) snprintf(buf, n, "INOUT");
  else snprintf(buf, n, "WHAT=%d", what);
  return buf;
}

static const char *cselect_flags_str(int flags, char *buf, size_t n) {
  char tmp[32]; tmp[0] = 0;
  int off = 0;
  if (flags & CURL_CSELECT_IN)  off += snprintf(tmp+off, (size_t)(sizeof(tmp)-off), "%sIN",  off?"|":"");
  if (flags & CURL_CSELECT_OUT) off += snprintf(tmp+off, (size_t)(sizeof(tmp)-off), "%sOUT", off?"|":"");
  if (flags & CURL_CSELECT_ERR) off += snprintf(tmp+off, (size_t)(sizeof(tmp)-off), "%sERR", off?"|":"");
  if (off == 0) snprintf(tmp, sizeof(tmp), "0");
  snprintf(buf, n, "%s", tmp);
  return buf;
}
#else
#define poll_what_str(...) ""
#define cselect_flags_str(...) ""
#endif

/* Protected call to rb_fiber_scheduler_io_wait to avoid unwinding into C on TypeError. */
struct fiber_io_wait_args { VALUE scheduler; VALUE io; int events; VALUE timeout; };
static VALUE fiber_io_wait_protected(VALUE argp) {
  struct fiber_io_wait_args *a = (struct fiber_io_wait_args *)argp;
  return rb_fiber_scheduler_io_wait(a->scheduler, a->io, a->events, a->timeout);
}

static int multi_socket_cb(CURL *easy, curl_socket_t s, int what, void *userp, void *socketp) {
  multi_socket_ctx *ctx = (multi_socket_ctx *)userp;
  (void)easy; (void)socketp;
  int fd = (int)s;

  if (!ctx || !ctx->sock_map) return 0;

  if (what == CURL_POLL_REMOVE) {
    st_data_t k = (st_data_t)fd;
    st_data_t rec;
    st_delete(ctx->sock_map, &k, &rec);
    {
      char b[16];
      curb_debugf("[curb.socket] sock_cb fd=%d what=%s (removed)", fd, poll_what_str(what, b, sizeof(b)));
    }
  } else {
    /* store current interest mask for this fd */
    st_insert(ctx->sock_map, (st_data_t)fd, (st_data_t)what);
    {
      char b[16];
      curb_debugf("[curb.socket] sock_cb fd=%d what=%s (tracked)", fd, poll_what_str(what, b, sizeof(b)));
    }
  }
  return 0;
}

static int multi_timer_cb(CURLM *multi, long timeout_ms, void *userp) {
  (void)multi;
  multi_socket_ctx *ctx = (multi_socket_ctx *)userp;
  if (ctx) ctx->timeout_ms = timeout_ms;
  curb_debugf("[curb.socket] timer_cb timeout_ms=%ld", timeout_ms);
  return 0;
}

struct build_fdset_args { rb_fdset_t *r; rb_fdset_t *w; rb_fdset_t *e; int maxfd; };
static int rb_fdset_from_sockmap_i(st_data_t key, st_data_t val, st_data_t argp) {
  struct build_fdset_args *a = (struct build_fdset_args *)argp;
  int fd = (int)key;
  int what = (int)val;
  if (what & CURL_POLL_IN) rb_fd_set(fd, a->r);
  if (what & CURL_POLL_OUT) rb_fd_set(fd, a->w);
  rb_fd_set(fd, a->e);
  if (fd > a->maxfd) a->maxfd = fd;
  return ST_CONTINUE;
}
static void rb_fdset_from_sockmap(st_table *map, rb_fdset_t *rfds, rb_fdset_t *wfds, rb_fdset_t *efds, int *maxfd_out) {
  if (!map) { *maxfd_out = -1; return; }
  struct build_fdset_args a; a.r = rfds; a.w = wfds; a.e = efds; a.maxfd = -1;
  st_foreach(map, rb_fdset_from_sockmap_i, (st_data_t)&a);
  *maxfd_out = a.maxfd;
}

struct dispatch_args { CURLM *mh; int *running; CURLMcode mrc; rb_fdset_t *r; rb_fdset_t *w; rb_fdset_t *e; };
static int dispatch_ready_fd_i(st_data_t key, st_data_t val, st_data_t argp) {
  (void)val;
  struct dispatch_args *dp = (struct dispatch_args *)argp;
  int fd = (int)key;
  int flags = 0;
  if (rb_fd_isset(fd, dp->r)) flags |= CURL_CSELECT_IN;
  if (rb_fd_isset(fd, dp->w)) flags |= CURL_CSELECT_OUT;
  if (rb_fd_isset(fd, dp->e)) flags |= CURL_CSELECT_ERR;
  if (flags) {
    dp->mrc = curl_multi_socket_action(dp->mh, (curl_socket_t)fd, flags, dp->running);
    if (dp->mrc != CURLM_OK) return ST_STOP;
  }
  return ST_CONTINUE;
}

/* Helpers used with st_foreach to avoid compiler-specific nested functions. */
struct pick_one_state { int fd; int what; int found; };
static int st_pick_one_i(st_data_t key, st_data_t val, st_data_t argp) {
  struct pick_one_state *s = (struct pick_one_state *)argp;
  s->fd = (int)key;
  s->what = (int)val;
  s->found = 1;
  return ST_STOP;
}
struct counter_state { int count; };
static int st_count_i(st_data_t k, st_data_t v, st_data_t argp) {
  (void)k; (void)v;
  struct counter_state *c = (struct counter_state *)argp;
  c->count++;
  return ST_CONTINUE;
}

static void rb_curl_multi_socket_drive(VALUE self, ruby_curl_multi *rbcm, multi_socket_ctx *ctx, VALUE block) {
  /* prime the state: let libcurl act on timeouts to setup sockets */
  CURLMcode mrc = curl_multi_socket_action(rbcm->handle, CURL_SOCKET_TIMEOUT, 0, &rbcm->running);
  if (mrc != CURLM_OK) raise_curl_multi_error_exception(mrc);
  curb_debugf("[curb.socket] drive: initial socket_action timeout -> mrc=%d running=%d", mrc, rbcm->running);
  rb_curl_multi_read_info(self, rbcm->handle);
  if (block != Qnil) rb_funcall(block, rb_intern("call"), 1, self);

  while (rbcm->running) {
    struct timeval tv = {0, 0};
    if (ctx->timeout_ms < 0) {
      tv.tv_sec = cCurlMutiDefaulttimeout / 1000;
      tv.tv_usec = (cCurlMutiDefaulttimeout % 1000) * 1000;
    } else {
      long t = ctx->timeout_ms;
      if (t > cCurlMutiDefaulttimeout) t = cCurlMutiDefaulttimeout;
      if (t < 0) t = 0;
      tv.tv_sec = t / 1000;
      tv.tv_usec = (t % 1000) * 1000;
    }

    /* Find a representative fd to wait on (if any). */
    int wait_fd = -1;
    int wait_what = 0;
    if (ctx->sock_map) {
      struct pick_one_state st = { -1, 0, 0 };
      st_foreach(ctx->sock_map, st_pick_one_i, (st_data_t)&st);
      if (st.found) { wait_fd = st.fd; wait_what = st.what; }
    }

    /* Count tracked fds for logging */
    int count_tracked = 0;
    if (ctx->sock_map) {
      struct counter_state cs = { 0 };
      st_foreach(ctx->sock_map, st_count_i, (st_data_t)&cs);
      count_tracked = cs.count;
    }

    curb_debugf("[curb.socket] wait phase: tracked_fds=%d fd=%d what=%d tv=%ld.%06ld", count_tracked, wait_fd, wait_what, (long)tv.tv_sec, (long)tv.tv_usec);

    int did_timeout = 0;
    int any_ready = 0;

    int handled_wait = 0;
    if (count_tracked > 1) {
      /* Multi-fd wait using scheduler-aware rb_thread_fd_select. */
      rb_fdset_t rfds, wfds, efds;
      rb_fd_init(&rfds); rb_fd_init(&wfds); rb_fd_init(&efds);
      int maxfd = -1;
      struct build_fdset_args a2; a2.r = &rfds; a2.w = &wfds; a2.e = &efds; a2.maxfd = -1;
      st_foreach(ctx->sock_map, rb_fdset_from_sockmap_i, (st_data_t)&a2);
      maxfd = a2.maxfd;
      int rc = rb_thread_fd_select(maxfd + 1, &rfds, &wfds, &efds, &tv);
      curb_debugf("[curb.socket] rb_thread_fd_select(multi) rc=%d maxfd=%d", rc, maxfd);
      if (rc < 0) {
        rb_fd_term(&rfds); rb_fd_term(&wfds); rb_fd_term(&efds);
        if (errno != EINTR) rb_raise(rb_eRuntimeError, "select(): %s", strerror(errno));
        continue;
      }
      any_ready = (rc > 0);
      did_timeout = (rc == 0);
      if (any_ready) {
        struct dispatch_args d; d.mh = rbcm->handle; d.running = &rbcm->running; d.mrc = CURLM_OK; d.r = &rfds; d.w = &wfds; d.e = &efds;
        st_foreach(ctx->sock_map, dispatch_ready_fd_i, (st_data_t)&d);
        if (d.mrc != CURLM_OK) {
          rb_fd_term(&rfds); rb_fd_term(&wfds); rb_fd_term(&efds);
          raise_curl_multi_error_exception(d.mrc);
        }
      }
      rb_fd_term(&rfds); rb_fd_term(&wfds); rb_fd_term(&efds);
      handled_wait = 1;
    } else if (count_tracked == 1) {
#if defined(HAVE_RB_WAIT_FOR_SINGLE_FD)
      if (wait_fd >= 0) {
        int ev = 0;
        if (wait_what == CURL_POLL_IN) ev = RB_WAITFD_IN;
        else if (wait_what == CURL_POLL_OUT) ev = RB_WAITFD_OUT;
        else if (wait_what == CURL_POLL_INOUT) ev = RB_WAITFD_IN|RB_WAITFD_OUT;
        int rc = rb_wait_for_single_fd(wait_fd, ev, &tv);
        curb_debugf("[curb.socket] rb_wait_for_single_fd rc=%d fd=%d ev=%d", rc, wait_fd, ev);
        if (rc < 0) {
          if (errno != EINTR) rb_raise(rb_eRuntimeError, "wait_for_single_fd(): %s", strerror(errno));
          continue;
        }
        any_ready = (rc != 0);
        did_timeout = (rc == 0);
        handled_wait = 1;
      }
#endif
#if defined(HAVE_RB_FIBER_SCHEDULER_IO_WAIT) && defined(HAVE_RB_FIBER_SCHEDULER_CURRENT)
      if (!handled_wait) {
        VALUE scheduler = rb_fiber_scheduler_current();
        if (scheduler != Qnil) {
          int events = 0;
          if (wait_fd >= 0) {
            if (wait_what == CURL_POLL_IN) events = RB_WAITFD_IN;
            else if (wait_what == CURL_POLL_OUT) events = RB_WAITFD_OUT;
            else if (wait_what == CURL_POLL_INOUT) events = RB_WAITFD_IN|RB_WAITFD_OUT;
            else events = RB_WAITFD_IN|RB_WAITFD_OUT;
          }
          double timeout_s = (double)tv.tv_sec + ((double)tv.tv_usec / 1e6);
          VALUE timeout = rb_float_new(timeout_s);
          if (wait_fd < 0) {
            rb_thread_wait_for(tv);
            did_timeout = 1;
          } else {
            const char *mode = (wait_what == CURL_POLL_IN) ? "r" : (wait_what == CURL_POLL_OUT) ? "w" : "r+";
            VALUE io = rb_funcall(rb_cIO, rb_intern("for_fd"), 2, INT2NUM(wait_fd), rb_str_new_cstr(mode));
            rb_funcall(io, rb_intern("autoclose="), 1, Qfalse);
            struct fiber_io_wait_args args = { scheduler, io, events, timeout };
            int state = 0;
            VALUE ready = rb_protect(fiber_io_wait_protected, (VALUE)&args, &state);
            if (state) {
              did_timeout = 1; any_ready = 0;
            } else {
              any_ready = (ready != Qfalse);
              did_timeout = !any_ready;
            }
          }
          handled_wait = 1;
        }
      }
#endif
      if (!handled_wait) {
        /* Fallback: single-fd select. */
        rb_fdset_t rfds, wfds, efds;
        rb_fd_init(&rfds); rb_fd_init(&wfds); rb_fd_init(&efds);
        int maxfd = -1;
        if (wait_fd >= 0) {
          if (wait_what == CURL_POLL_IN || wait_what == CURL_POLL_INOUT) rb_fd_set(wait_fd, &rfds);
          if (wait_what == CURL_POLL_OUT || wait_what == CURL_POLL_INOUT) rb_fd_set(wait_fd, &wfds);
          rb_fd_set(wait_fd, &efds);
          maxfd = wait_fd;
        }
        int rc = rb_thread_fd_select(maxfd + 1, &rfds, &wfds, &efds, &tv);
        curb_debugf("[curb.socket] rb_thread_fd_select(single) rc=%d fd=%d", rc, wait_fd);
        if (rc < 0) {
          rb_fd_term(&rfds); rb_fd_term(&wfds); rb_fd_term(&efds);
          if (errno != EINTR) rb_raise(rb_eRuntimeError, "select(): %s", strerror(errno));
          continue;
        }
        any_ready = (rc > 0);
        did_timeout = (rc == 0);
        rb_fd_term(&rfds); rb_fd_term(&wfds); rb_fd_term(&efds);
      }
    } else { /* count_tracked == 0 */
      rb_thread_wait_for(tv);
      did_timeout = 1;
    }

    if (did_timeout) {
      mrc = curl_multi_socket_action(rbcm->handle, CURL_SOCKET_TIMEOUT, 0, &rbcm->running);
      curb_debugf("[curb.socket] socket_action timeout -> mrc=%d running=%d", mrc, rbcm->running);
      if (mrc != CURLM_OK) raise_curl_multi_error_exception(mrc);
    } else if (any_ready) {
      if (count_tracked == 1 && wait_fd >= 0) {
        int flags = 0;
        if (wait_what == CURL_POLL_IN || wait_what == CURL_POLL_INOUT) flags |= CURL_CSELECT_IN;
        if (wait_what == CURL_POLL_OUT || wait_what == CURL_POLL_INOUT) flags |= CURL_CSELECT_OUT;
        flags |= CURL_CSELECT_ERR;
        char b[32];
        curb_debugf("[curb.socket] socket_action fd=%d flags=%s", wait_fd, cselect_flags_str(flags, b, sizeof(b)));
        mrc = curl_multi_socket_action(rbcm->handle, (curl_socket_t)wait_fd, flags, &rbcm->running);
        curb_debugf("[curb.socket] socket_action -> mrc=%d running=%d", mrc, rbcm->running);
        if (mrc != CURLM_OK) raise_curl_multi_error_exception(mrc);
      }
    }

    rb_curl_multi_read_info(self, rbcm->handle);
    curb_debugf("[curb.socket] processed completions; running=%d", rbcm->running);
    if (block != Qnil) rb_funcall(block, rb_intern("call"), 1, self);
  }
}

struct socket_drive_args { VALUE self; ruby_curl_multi *rbcm; multi_socket_ctx *ctx; VALUE block; };
static VALUE ruby_curl_multi_socket_drive_body(VALUE argp) {
  struct socket_drive_args *a = (struct socket_drive_args *)argp;
  rb_curl_multi_socket_drive(a->self, a->rbcm, a->ctx, a->block);
  return Qtrue;
}
struct socket_cleanup_args { ruby_curl_multi *rbcm; multi_socket_ctx *ctx; };
static VALUE ruby_curl_multi_socket_drive_ensure(VALUE argp) {
  struct socket_cleanup_args *c = (struct socket_cleanup_args *)argp;
  if (c->rbcm && c->rbcm->handle) {
    curl_multi_setopt(c->rbcm->handle, CURLMOPT_SOCKETFUNCTION, NULL);
    curl_multi_setopt(c->rbcm->handle, CURLMOPT_SOCKETDATA, NULL);
    curl_multi_setopt(c->rbcm->handle, CURLMOPT_TIMERFUNCTION, NULL);
    curl_multi_setopt(c->rbcm->handle, CURLMOPT_TIMERDATA, NULL);
  }
  if (c->ctx && c->ctx->sock_map) {
    st_free_table(c->ctx->sock_map);
    c->ctx->sock_map = NULL;
  }
  return Qnil;
}

VALUE ruby_curl_multi_socket_perform(int argc, VALUE *argv, VALUE self) {
  ruby_curl_multi *rbcm;
  VALUE block = Qnil;
  rb_scan_args(argc, argv, "0&", &block);

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  multi_socket_ctx ctx;
  ctx.sock_map = st_init_numtable();
  ctx.timeout_ms = -1;

  /* install socket/timer callbacks */
  curl_multi_setopt(rbcm->handle, CURLMOPT_SOCKETFUNCTION, multi_socket_cb);
  curl_multi_setopt(rbcm->handle, CURLMOPT_SOCKETDATA, &ctx);
  curl_multi_setopt(rbcm->handle, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
  curl_multi_setopt(rbcm->handle, CURLMOPT_TIMERDATA, &ctx);

  /* run using socket action loop with ensure-cleanup */
  struct socket_drive_args body_args = { self, rbcm, &ctx, block };
  struct socket_cleanup_args ensure_args = { rbcm, &ctx };
  rb_ensure(ruby_curl_multi_socket_drive_body, (VALUE)&body_args, ruby_curl_multi_socket_drive_ensure, (VALUE)&ensure_args);

  /* finalize */
  rb_curl_multi_read_info(self, rbcm->handle);
  if (block != Qnil) rb_funcall(block, rb_intern("call"), 1, self);
  if (cCurlMutiAutoClose == 1) rb_funcall(self, rb_intern("close"), 0);

  return Qtrue;
}
#endif /* socket-action implementation */

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

#if defined(HAVE_CURL_MULTI_WAIT) && !defined(HAVE_RB_THREAD_FD_SELECT)
      {
        struct wait_args wait_args;
        wait_args.handle     = rbcm->handle;
        wait_args.timeout_ms = timeout_milliseconds;
        wait_args.numfds     = 0;
        /*
         * When a Fiber scheduler is available (Ruby >= 3.x), rb_thread_fd_select
         * integrates with it. If we have rb_thread_fd_select available at build
         * time, we avoid curl_multi_wait entirely (see preprocessor guard above)
         * and use the fdset branch below. Otherwise, we use curl_multi_wait and
         * release the GVL so Ruby threads can continue to run.
         */
        CURLMcode wait_rc;
#if defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL)
        wait_rc = (CURLMcode)(intptr_t)rb_thread_call_without_gvl(
          curl_multi_wait_wrapper, &wait_args, RUBY_UBF_IO, NULL
        );
#else
        wait_rc = curl_multi_wait(rbcm->handle, NULL, 0, timeout_milliseconds, &wait_args.numfds);
#endif
        if (wait_rc != CURLM_OK) {
          raise_curl_multi_error_exception(wait_rc);
        }
        if (wait_args.numfds == 0) {
#ifdef HAVE_RB_THREAD_FD_SELECT
          struct timeval tv_sleep = tv_100ms;
          /* Sleep in a scheduler-aware way. */
          rb_thread_fd_select(0, NULL, NULL, NULL, &tv_sleep);
#else
          rb_thread_wait_for(tv_100ms);
#endif
        }
        /* Process pending transfers after waiting */
        rb_curl_multi_run(self, rbcm->handle, &(rbcm->running));
        rb_curl_multi_read_info(self, rbcm->handle);
        if (block != Qnil) { rb_funcall(block, rb_intern("call"), 1, self); }
      }
#else

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
#if HAVE_RB_THREAD_FD_SELECT
        struct timeval tv_sleep = tv_100ms;
        rb_thread_fd_select(0, NULL, NULL, NULL, &tv_sleep);
#else
        rb_thread_wait_for(tv_100ms);
#endif
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

#if HAVE_RB_THREAD_FD_SELECT
      /* Prefer scheduler-aware waiting when available. Build rb_fdset_t sets. */
      {
        rb_fdset_t rfds, wfds, efds;
        rb_fd_init(&rfds);
        rb_fd_init(&wfds);
        rb_fd_init(&efds);
#ifdef _WIN32
        /* On Windows, iterate explicit fd arrays for CRT fds. */
        int i;
        for (i = 0; i < crt_fdread.fd_count; i++) rb_fd_set(crt_fdread.fd_array[i], &rfds);
        for (i = 0; i < crt_fdwrite.fd_count; i++) rb_fd_set(crt_fdwrite.fd_array[i], &wfds);
        for (i = 0; i < crt_fdexcep.fd_count; i++) rb_fd_set(crt_fdexcep.fd_array[i], &efds);
        rc = rb_thread_fd_select(0, &rfds, &wfds, &efds, &tv);
#else
        int fd;
        for (fd = 0; fd <= maxfd; fd++) {
          if (FD_ISSET(fd, &fdread)) rb_fd_set(fd, &rfds);
          if (FD_ISSET(fd, &fdwrite)) rb_fd_set(fd, &wfds);
          if (FD_ISSET(fd, &fdexcep)) rb_fd_set(fd, &efds);
        }
        rc = rb_thread_fd_select(maxfd+1, &rfds, &wfds, &efds, &tv);
#endif
        rb_fd_term(&rfds);
        rb_fd_term(&wfds);
        rb_fd_term(&efds);
      }
#elif defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL)
      rc = (int)(VALUE) rb_thread_call_without_gvl((void *(*)(void *))curb_select, &fdset_args, RUBY_UBF_IO, 0);
#elif HAVE_RB_THREAD_BLOCKING_REGION
      rc = rb_thread_blocking_region(curb_select, &fdset_args, RUBY_UBF_IO, 0);
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
#endif /* disabled curl_multi_wait: use fdsets */
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
  rb_curl_multi_detach_all(rbcm);

  if (rbcm->handle) {
    curl_multi_cleanup(rbcm->handle);
    rbcm->handle = NULL;
  }

  ruby_curl_multi_init(rbcm);
  return self;
}

/* GC mark: keep attached easy VALUEs alive while associated. */
static int mark_attached_i(st_data_t key, st_data_t val, st_data_t arg) {
  VALUE easy = (VALUE)val;
  if (!NIL_P(easy)) rb_gc_mark(easy);
  return ST_CONTINUE;
}

static void curl_multi_mark(void *ptr) {
  ruby_curl_multi *rbcm = (ruby_curl_multi *)ptr;
  if (!rbcm) return;
  if (rbcm->attached) {
    st_foreach(rbcm->attached, mark_attached_i, (st_data_t)0);
  }
}


/* =================== INIT LIB =====================*/
void init_curb_multi() {
  idCall = rb_intern("call");
  cCurlMulti = rb_define_class_under(mCurl, "Multi", rb_cObject);

  rb_undef_alloc_func(cCurlMulti);

  /* Class methods */
  rb_define_singleton_method(cCurlMulti, "new", ruby_curl_multi_new, 0);
  rb_define_singleton_method(cCurlMulti, "default_timeout=", ruby_curl_multi_set_default_timeout, 1);
  rb_define_singleton_method(cCurlMulti, "default_timeout", ruby_curl_multi_get_default_timeout, 0);
  rb_define_singleton_method(cCurlMulti, "autoclose=", ruby_curl_multi_set_autoclose, 1);
  rb_define_singleton_method(cCurlMulti, "autoclose", ruby_curl_multi_get_autoclose, 0);
  /* Instance methods */
  rb_define_method(cCurlMulti, "max_connects=", ruby_curl_multi_max_connects, 1);
  rb_define_method(cCurlMulti, "max_host_connections=", ruby_curl_multi_max_host_connections, 1);
  rb_define_method(cCurlMulti, "pipeline=", ruby_curl_multi_pipeline, 1);
  rb_define_method(cCurlMulti, "_add", ruby_curl_multi_add, 1);
  rb_define_method(cCurlMulti, "_remove", ruby_curl_multi_remove, 1);
  /* Prefer a socket-action based perform when supported and scheduler-aware. */
#if defined(HAVE_CURL_MULTI_SOCKET_ACTION) && defined(HAVE_CURLMOPT_SOCKETFUNCTION) && defined(HAVE_RB_THREAD_FD_SELECT) && !defined(_WIN32)
  extern VALUE ruby_curl_multi_socket_perform(int argc, VALUE *argv, VALUE self);
  rb_define_method(cCurlMulti, "perform", ruby_curl_multi_socket_perform, -1);
#else
  rb_define_method(cCurlMulti, "perform", ruby_curl_multi_perform, -1);
#endif
  rb_define_method(cCurlMulti, "_close", ruby_curl_multi_close, 0);
}
