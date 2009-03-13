/* curb_easy.c - Curl easy mode
 * Copyright (c)2008 Todd A. Fisher. 
 * Licensed under the Ruby License. See LICENSE for details.
 * 
 * $Id$
 */
#include <st.h>
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

static VALUE ruby_curl_multi_remove(VALUE , VALUE );
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
  rb_curl_multi_remove(rbcm, easy);
}

static void curl_multi_free(ruby_curl_multi *rbcm) {
  //printf("hash entries: %d\n", RHASH(rbcm->requests)->tbl->num_entries );
  if (RHASH_LEN(rbcm->requests) > 0) {
    rb_hash_foreach( rbcm->requests, (int (*)())curl_multi_flush_easy, (VALUE)rbcm );

    curl_multi_cleanup(rbcm->handle);
    //rb_hash_clear(rbcm->requests)
    rbcm->requests = rb_hash_new();
  }
}

/*
 * call-seq:
 *   Curl::Multi.new                                   => #&lt;Curl::Easy...&gt;
 * 
 * Create a new Curl::Multi instance
 */
static VALUE ruby_curl_multi_new(VALUE self) {
  VALUE new_curlm;
  
  ruby_curl_multi *rbcm = ALLOC(ruby_curl_multi);

  rbcm->handle = curl_multi_init();

  rbcm->requests = rb_hash_new();

  rbcm->active = 0;
  rbcm->running = 0;
  
  new_curlm = Data_Wrap_Struct(cCurlMulti, curl_multi_mark, curl_multi_free, rbcm);

  return new_curlm;
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
static VALUE ruby_curl_multi_add(VALUE self, VALUE easy) {
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

  rb_hash_aset( rbcm->requests, rb_int_new((long)rbce->curl), easy );
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
 * Remove an easy handle from a multi stack
 *
 * Will raise an exception if the easy handle is not found
 */
static VALUE ruby_curl_multi_remove(VALUE self, VALUE easy) {
  ruby_curl_multi *rbcm;

  Data_Get_Struct(self, ruby_curl_multi, rbcm);

  rb_curl_multi_remove(rbcm,easy);
  // active should equal INT2FIX(RHASH(rbcm->requests)->tbl->num_entries)

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
  rb_hash_delete( rbcm->requests, rb_int_new((long)rbce->curl) );
}

static void rb_curl_multi_read_info(VALUE self, CURLM *multi_handle) {
  int msgs_left, result;
  CURLMsg *msg;
  CURLcode ecode;
  CURL *easy_handle;
  ruby_curl_easy *rbce = NULL;
//  VALUE finished = rb_ary_new();

  /* check for finished easy handles and remove from the multi handle */
  while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {

    if (msg->msg != CURLMSG_DONE) {
      continue;
    }

    easy_handle = msg->easy_handle;
    result = msg->data.result;
    if (easy_handle) {
      ecode = curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, (char**)&rbce);
      if (ecode != 0) {
        raise_curl_easy_error_exception(ecode);
      }
      //printf( "finished: 0x%X\n", (long)rbce->self );
      //rb_ary_push(finished, rbce->self);
      ruby_curl_multi_remove( self, rbce->self );

      if (rbce->complete_proc != Qnil) {
        rb_funcall( rbce->complete_proc, idCall, 1, self );
      }

      long response_code = -1;
      curl_easy_getinfo(rbce->curl, CURLINFO_RESPONSE_CODE, &response_code);
 
      if (result != 0) {
        if (rbce->failure_proc != Qnil) {
          rb_funcall( rbce->failure_proc, idCall, 1, rbce->self );
        }
      }
      else if (rbce->success_proc != Qnil &&
              ((response_code >= 200 && response_code < 300) || response_code == 0)) {
        /* NOTE: we allow response_code == 0, in the case the file is being read from disk */
        rb_funcall( rbce->success_proc, idCall, 1, rbce->self );
      }
      else if (rbce->failure_proc != Qnil &&
              (response_code >= 300 && response_code < 600)) {
        rb_funcall( rbce->failure_proc, idCall, 1, rbce->self );
      }
    }
    else {
      //printf( "missing easy handle\n" );
    }
  }

  /*
  while (RARRAY(finished)->len > 0) {
    //printf( "finished handle\n" );
    ruby_curl_multi_remove( self, rb_ary_pop(finished) );
  }
   */
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
static VALUE ruby_curl_multi_perform(VALUE self) {
  CURLMcode mcode;
  ruby_curl_multi *rbcm;
  int maxfd, rc;
  fd_set fdread, fdwrite, fdexcep;

  long timeout;
  struct timeval tv = {0, 0};

  Data_Get_Struct(self, ruby_curl_multi, rbcm);
  //rb_gc_mark(self);

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

    /* get the curl suggested time out */
    mcode = curl_multi_timeout(rbcm->handle, &timeout);
    if (mcode != CURLM_OK) {
      raise_curl_multi_error_exception(mcode);
    }

    if (timeout == 0) { /* no delay */
      rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );
      continue;
    }
    else if (timeout == -1) {
      timeout = 1; /* You must not wait too long 
                     (more than a few seconds perhaps) before 
                     you call curl_multi_perform() again */
    }

    if (rb_block_given_p()) {
      rb_yield(self);
    }
 
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout * 1000) % 1000000;

    rc = rb_thread_select(maxfd+1, &fdread, &fdwrite, &fdexcep, &tv);
    if (rc < 0) {
      rb_raise(rb_eRuntimeError, "select(): %s", strerror(errno));
    }

    rb_curl_multi_run( self, rbcm->handle, &(rbcm->running) );

  }

  return Qnil;
}

/* =================== INIT LIB =====================*/
void init_curb_multi() {
  idCall = rb_intern("call");

  cCurlMulti = rb_define_class_under(mCurl, "Multi", rb_cObject);

  /* Class methods */
  rb_define_singleton_method(cCurlMulti, "new", ruby_curl_multi_new, -1);

  /* Instnace methods */
  rb_define_method(cCurlMulti, "add", ruby_curl_multi_add, 1);
  rb_define_method(cCurlMulti, "remove", ruby_curl_multi_remove, 1);
  rb_define_method(cCurlMulti, "perform", ruby_curl_multi_perform, 0);
}
