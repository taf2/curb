/* curb_easy.c - Curl easy mode
 * Copyright (c)2006 Ross Bamford.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id: curb_easy.c 30 2006-12-09 12:30:24Z roscopeco $
 */
#include "curb_easy.h"
#include "curb_errors.h"
#include "curb_postfield.h"
#include "curb_upload.h"
#include "curb_multi.h"

#include <errno.h>
#include <string.h>

extern VALUE mCurl;

static VALUE idCall;
static VALUE idJoin;
static VALUE rbstrAmp;

#ifdef RDOC_NEVER_DEFINED
  mCurl = rb_define_module("Curl");
#endif

VALUE cCurlEasy;


/* ================== CURL HANDLER FUNCS ==============*/

static VALUE callback_exception(VALUE unused) {
  return Qfalse;
} 

/* These handle both body and header data */
static size_t default_data_handler(char *stream,
                                   size_t size,
                                   size_t nmemb,
                                   VALUE out) {
  rb_str_buf_cat(out, stream, size * nmemb);
  return size * nmemb;
}

// size_t function( void *ptr, size_t size, size_t nmemb, void *stream);
static size_t read_data_handler(void *ptr,
                                size_t size,
                                size_t nmemb,
                                ruby_curl_easy *rbce) {
  VALUE upload = rb_easy_get("upload");
  size_t read_bytes = (size*nmemb);
  VALUE stream = ruby_curl_upload_stream_get(upload);

  if (rb_respond_to(stream, rb_intern("read"))) {//if (rb_respond_to(stream, rb_intern("to_s"))) {
    /* copy read_bytes from stream into ptr */
    VALUE str = rb_funcall(stream, rb_intern("read"), 1, rb_int_new(read_bytes) );
    if( str != Qnil ) {
      memcpy(ptr, RSTRING_PTR(str), RSTRING_LEN(str));
      return RSTRING_LEN(str);
    }
    else {
      return 0;
    }
  }
  else if (rb_respond_to(stream, rb_intern("to_s"))) {
    ruby_curl_upload *rbcu;
    VALUE str;
    size_t len;
    size_t remaining;
    char *str_ptr;
    Data_Get_Struct(upload, ruby_curl_upload, rbcu);
    str = rb_funcall(stream, rb_intern("to_s"), 0);
    len = RSTRING_LEN(str);
    remaining = len - rbcu->offset;
    str_ptr = RSTRING_PTR(str);

    if( remaining < read_bytes ) {
      if( remaining > 0 ) {
        memcpy(ptr, str_ptr+rbcu->offset, remaining);
        read_bytes = remaining;
        rbcu->offset += remaining;
      }
      return remaining;
    }
    else if( remaining > read_bytes ) { // read_bytes <= remaining - send what we can fit in the buffer(ptr)
      memcpy(ptr, str_ptr+rbcu->offset, read_bytes);
      rbcu->offset += read_bytes;
    }
    else { // they're equal
      memcpy(ptr, str_ptr+rbcu->offset, --read_bytes);
      rbcu->offset += read_bytes;
    }
    return read_bytes;
  }
  else {
    return 0;
  }
}

int seek_data_handler(ruby_curl_easy *rbce,
                      curl_off_t offset,
                      int origin) {

  VALUE upload = rb_easy_get("upload");
  VALUE stream = ruby_curl_upload_stream_get(upload);

  if (rb_respond_to(stream, rb_intern("seek"))) {
    rb_funcall(stream, rb_intern("seek"), 2, SEEK_SET, offset);
  } else {
    ruby_curl_upload *rbcu;
    Data_Get_Struct(upload, ruby_curl_upload, rbcu);
    // This OK because curl only uses SEEK_SET as per the documentation
    rbcu->offset = offset;
  }

  return 0;
}

static size_t proc_data_handler(char *stream,
                                size_t size,
                                size_t nmemb,
                                VALUE proc) {
  VALUE procret;

  procret = rb_funcall(proc, idCall, 1, rb_str_new(stream, size * nmemb));

  switch (rb_type(procret)) {
    case T_FIXNUM:
      return FIX2LONG(procret);
    case T_BIGNUM:
      return NUM2LONG(procret);
    default:
      rb_warn("Curl data handlers should return the number of bytes read as an Integer");
      return size * nmemb;
  }
}

static size_t proc_data_handler_body(char *stream,
                                     size_t size,
                                     size_t nmemb,
                                     ruby_curl_easy *rbce)
{
  size_t ret;
  rbce->callback_active = 1;
  ret = proc_data_handler(stream, size, nmemb, rb_easy_get("body_proc"));
  rbce->callback_active = 0;
  return ret;
}
static size_t proc_data_handler_header(char *stream,
                                       size_t size,
                                       size_t nmemb,
                                       ruby_curl_easy *rbce)
{
  size_t ret;
  rbce->callback_active = 1;
  ret = proc_data_handler(stream, size, nmemb, rb_easy_get("header_proc"));
  rbce->callback_active = 0;
  return ret;
}


static VALUE call_progress_handler(VALUE ary) {
  return rb_funcall(rb_ary_entry(ary, 0), idCall, 4,
                    rb_ary_entry(ary, 1), // rb_float_new(dltotal),
                    rb_ary_entry(ary, 2), // rb_float_new(dlnow),
                    rb_ary_entry(ary, 3), // rb_float_new(ultotal),
                    rb_ary_entry(ary, 4)); // rb_float_new(ulnow));
}

static int proc_progress_handler(VALUE proc,
                                 double dltotal,
                                 double dlnow,
                                 double ultotal,
                                 double ulnow) {
  VALUE procret;
  VALUE callargs = rb_ary_new2(5);

  rb_ary_store(callargs, 0, proc);
  rb_ary_store(callargs, 1, rb_float_new(dltotal));
  rb_ary_store(callargs, 2, rb_float_new(dlnow));
  rb_ary_store(callargs, 3, rb_float_new(ultotal));
  rb_ary_store(callargs, 4, rb_float_new(ulnow));

	//v = rb_rescue(range_check, (VALUE)args, range_failed, 0);
  //procret = rb_funcall(proc, idCall, 4, rb_float_new(dltotal),
  //                                      rb_float_new(dlnow),
  //                                      rb_float_new(ultotal),
  //                                      rb_float_new(ulnow));
  procret = rb_rescue(call_progress_handler, callargs, callback_exception, Qnil);

  return(((procret == Qfalse) || (procret == Qnil)) ? -1 : 0);
}

static VALUE call_debug_handler(VALUE ary) {
  return rb_funcall(rb_ary_entry(ary, 0), idCall, 2,
                    rb_ary_entry(ary, 1), // INT2FIX(type),
                    rb_ary_entry(ary, 2)); // rb_str_new(data, data_len)
}
static int proc_debug_handler(CURL *curl,
                              curl_infotype type,
                              char *data,
                              size_t data_len,
                              VALUE proc) {
  VALUE callargs = rb_ary_new2(3);
  rb_ary_store(callargs, 0, proc);
  rb_ary_store(callargs, 1, INT2FIX(type));
  rb_ary_store(callargs, 2, rb_str_new(data, data_len));
  rb_rescue(call_debug_handler, callargs, callback_exception, Qnil);
  /* no way to indicate to libcurl that we should break out given an exception in the on_debug handler...
   * this means exceptions will be swallowed
   */
  //rb_funcall(proc, idCall, 2, INT2FIX(type), rb_str_new(data, data_len));
  return 0;
}

/* ================== MARK/FREE FUNC ==================*/
void curl_easy_mark(ruby_curl_easy *rbce) {
  if (!NIL_P(rbce->opts)) { rb_gc_mark(rbce->opts); }
  if (!NIL_P(rbce->multi)) { rb_gc_mark(rbce->multi); }
}

static void ruby_curl_easy_free(ruby_curl_easy *rbce) {
  if (rbce->curl_headers) {
    curl_slist_free_all(rbce->curl_headers);
  }

  if (rbce->curl_ftp_commands) {
    curl_slist_free_all(rbce->curl_ftp_commands);
  }

  if (rbce->curl) {
    curl_easy_cleanup(rbce->curl);
  }
}

void curl_easy_free(ruby_curl_easy *rbce) {
  ruby_curl_easy_free(rbce);
  free(rbce);
}


/* ================= ALLOC METHODS ====================*/

static void ruby_curl_easy_zero(ruby_curl_easy *rbce) {
  rbce->opts = rb_hash_new();

  rbce->curl_headers = NULL;
  rbce->curl_ftp_commands = NULL;

  /* various-typed opts */
  rbce->local_port = 0;
  rbce->local_port_range = 0;
  rbce->proxy_port = 0;
  rbce->proxy_type = -1;
  rbce->http_auth_types = 0;
  rbce->proxy_auth_types = 0;
  rbce->max_redirs = -1;
  rbce->timeout = 0;
  rbce->connect_timeout = 0;
  rbce->dns_cache_timeout = 60;
  rbce->ftp_response_timeout = 0;
  rbce->low_speed_limit = 0;
  rbce->low_speed_time = 0;
  rbce->ssl_version = -1;
  rbce->use_ssl = -1;
  rbce->ftp_filemethod = -1;
  rbce->resolve_mode = CURL_IPRESOLVE_WHATEVER;

  /* bool opts */
  rbce->proxy_tunnel = 0;
  rbce->fetch_file_time = 0;
  rbce->ssl_verify_peer = 1;
  rbce->ssl_verify_host = 2;
  rbce->header_in_body = 0;
  rbce->use_netrc = 0;
  rbce->follow_location = 0;
  rbce->unrestricted_auth = 0;
  rbce->verbose = 0;
  rbce->multipart_form_post = 0;
  rbce->enable_cookies = 0;
  rbce->ignore_content_length = 0;
  rbce->callback_active = 0;
}

/*
 * call-seq:
 *   Curl::Easy.new                                   => #&lt;Curl::Easy...&gt;
 *   Curl::Easy.new(url = nil)                        => #&lt;Curl::Easy...&gt;
 *   Curl::Easy.new(url = nil) { |self| ... }         => #&lt;Curl::Easy...&gt;
 *
 * Create a new Curl::Easy instance, optionally supplying the URL.
 * The block form allows further configuration to be supplied before
 * the instance is returned.
 */
static VALUE ruby_curl_easy_new(int argc, VALUE *argv, VALUE klass) {
  CURLcode ecode;
  VALUE url, blk;
  VALUE new_curl;
  ruby_curl_easy *rbce;

  rb_scan_args(argc, argv, "01&", &url, &blk);

  rbce = ALLOC(ruby_curl_easy);

  /* handler */
  rbce->curl = curl_easy_init();
  if (!rbce->curl) {
    rb_raise(eCurlErrFailedInit, "Failed to initialize easy handle");
  }

  new_curl = Data_Wrap_Struct(klass, curl_easy_mark, curl_easy_free, rbce);

  rbce->multi = Qnil;
  rbce->opts  = Qnil;

  ruby_curl_easy_zero(rbce);

  rb_easy_set("url", url);

  /* set the new_curl pointer to the curl handle */
  ecode = curl_easy_setopt(rbce->curl, CURLOPT_PRIVATE, (void*)new_curl);
  if (ecode != CURLE_OK) {
    raise_curl_easy_error_exception(ecode);
  }

  if (blk != Qnil) {
    rb_funcall(blk, idCall, 1, new_curl);
  }

  return new_curl;
}

/*
 * call-seq:
 *   easy.clone                                       => #&lt;easy clone&gt;
 *   easy.dup                                         => #&lt;easy clone&gt;
 *
 * Clone this Curl::Easy instance, creating a new instance.
 * This method duplicates the underlying CURL* handle.
 */
static VALUE ruby_curl_easy_clone(VALUE self) {
  ruby_curl_easy *rbce, *newrbce;

  Data_Get_Struct(self, ruby_curl_easy, rbce);

  newrbce = ALLOC(ruby_curl_easy);
  memcpy(newrbce, rbce, sizeof(ruby_curl_easy));
  newrbce->curl = curl_easy_duphandle(rbce->curl);
  newrbce->curl_headers = NULL;
  newrbce->curl_ftp_commands = NULL;

  return Data_Wrap_Struct(cCurlEasy, curl_easy_mark, curl_easy_free, newrbce);
}

/*
 * call-seq:
 *   easy.close                                      => nil
 *
 * Close the Curl::Easy instance. Any open connections are closed
 * The easy handle is reinitialized.  If a previous multi handle was 
 * open it is set to nil and will be cleared after a GC.
 */
static VALUE ruby_curl_easy_close(VALUE self) {
  CURLcode ecode;
  ruby_curl_easy *rbce;

  Data_Get_Struct(self, ruby_curl_easy, rbce);

  if (rbce->callback_active) {
    rb_raise(rb_eRuntimeError, "Cannot close an active curl handle within a callback");
  }

  ruby_curl_easy_free(rbce);

  /* reinit the handle */
  rbce->curl = curl_easy_init();
  if (!rbce->curl) {
    rb_raise(eCurlErrFailedInit, "Failed to initialize easy handle");
  }

  rbce->multi = Qnil;

  ruby_curl_easy_zero(rbce);

  /* give the new curl handle a reference back to the ruby object */
  ecode = curl_easy_setopt(rbce->curl, CURLOPT_PRIVATE, (void*)self);
  if (ecode != CURLE_OK) {
    raise_curl_easy_error_exception(ecode);
  }

  return Qnil;
}

/*
 * call-seq:
 *   easy.reset                                      => Hash
 *
 * Reset the Curl::Easy instance, clears out all settings.
 *
 * from http://curl.haxx.se/libcurl/c/curl_easy_reset.html
 * Re-initializes all options previously set on a specified CURL handle to the default values. This puts back the handle to the same state as it was in when it was just created with curl_easy_init(3).
 * It does not change the following information kept in the handle: live connections, the Session ID cache, the DNS cache, the cookies and shares.
 *
 * The return value contains all settings stored.
 */
static VALUE ruby_curl_easy_reset(VALUE self) {
  CURLcode ecode;
  ruby_curl_easy *rbce;
  VALUE opts_dup;
  Data_Get_Struct(self, ruby_curl_easy, rbce);

  if (rbce->callback_active) {
    rb_raise(rb_eRuntimeError, "Cannot close an active curl handle within a callback");
  }

  opts_dup = rb_funcall(rbce->opts, rb_intern("dup"), 0);

  curl_easy_reset(rbce->curl);
  ruby_curl_easy_zero(rbce);

  /* rest clobbers the private setting, so reset it to self */
  ecode = curl_easy_setopt(rbce->curl, CURLOPT_PRIVATE, (void*)self);
  if (ecode != CURLE_OK) {
    raise_curl_easy_error_exception(ecode);
  }

  /* Free everything up */
  if (rbce->curl_headers) {
    curl_slist_free_all(rbce->curl_headers);
    rbce->curl_headers = NULL;
  }

  return opts_dup;
}


/* ================ OBJ ATTRIBUTES ==================*/

/*
 * call-seq:
 *   easy.url                                         => string
 *
 * Obtain the URL that will be used by subsequent calls to +perform+.
 */
static VALUE ruby_curl_easy_url_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, url);
}

/*
 * call-seq:
 *   easy.proxy_url                                   => string
 *
 * Obtain the HTTP Proxy URL that will be used by subsequent calls to +perform+.
 */
static VALUE ruby_curl_easy_proxy_url_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, proxy_url);
}

/*
 * call-seq:
 *   easy.headers = "Header: val"                              => "Header: val"
 *   easy.headers = {"Header" => "val" ..., "Header" => "val"} => {"Header: val", ...}
 *   easy.headers = ["Header: val" ..., "Header: val"]         => ["Header: val", ...]
 *
 * Set custom HTTP headers for following requests. This can be used to add
 * custom headers, or override standard headers used by libcurl. It defaults to a
 * Hash.
 *
 * For example to set a standard or custom header:
 *
 *    easy.headers["MyHeader"] = "myval"
 *
 * To remove a standard header (this is useful when removing libcurls default
 * 'Expect: 100-Continue' header when using HTTP form posts):
 *
 *    easy.headers["Expect"] = ''
 *
 * Anything passed to libcurl as a header will be converted to a string during
 * the perform step.
 */
static VALUE ruby_curl_easy_headers_set(VALUE self, VALUE headers) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, headers);
}

/*
 * call-seq:
 *   easy.headers                                     => Hash, Array or Str
 *
 * Obtain the custom HTTP headers for following requests.
 */
static VALUE ruby_curl_easy_headers_get(VALUE self) {
  ruby_curl_easy *rbce;
  VALUE headers;
  Data_Get_Struct(self, ruby_curl_easy, rbce);
  headers = rb_easy_get("headers");//rb_hash_aref(rbce->opts, rb_intern("headers")); 
  if (headers == Qnil) { headers = rb_easy_set("headers", rb_hash_new()); }
  return headers;
}

/*
 * call-seq:
 *   easy.interface                                   => string
 *
 * Obtain the interface name that is used as the outgoing network interface.
 * The name can be an interface name, an IP address or a host name.
 */
static VALUE ruby_curl_easy_interface_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, interface_hm);
}

/*
 * call-seq:
 *   easy.userpwd                                     => string
 *
 * Obtain the username/password string that will be used for subsequent
 * calls to +perform+.
 */
static VALUE ruby_curl_easy_userpwd_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, userpwd);
}

/*
 * call-seq:
 *   easy.proxypwd                                    => string
 *
 * Obtain the username/password string that will be used for proxy
 * connection during subsequent calls to +perform+. The supplied string
 * should have the form "username:password"
 */
static VALUE ruby_curl_easy_proxypwd_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, proxypwd);
}

/*
 * call-seq:
 *   easy.cookies                                     => "name1=content1; name2=content2;"
 *
 * Obtain the cookies for this Curl::Easy instance.
 */
static VALUE ruby_curl_easy_cookies_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, cookies);
}

/*
 * call-seq:
 *   easy.cookiefile                                  => string
 *
 * Obtain the cookiefile file for this Curl::Easy instance.
 */
static VALUE ruby_curl_easy_cookiefile_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, cookiefile);
}

/*
 * call-seq:
 *   easy.cookiejar                                   => string
 *
 * Obtain the cookiejar file to use for this Curl::Easy instance.
 */
static VALUE ruby_curl_easy_cookiejar_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, cookiejar);
}

/*
 * call-seq:
 *   easy.cert = string                               => ""
 *
 * Set a cert file to use for this Curl::Easy instance. This file
 * will be used to validate SSL connections.
 *
 */
static VALUE ruby_curl_easy_cert_set(VALUE self, VALUE cert) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, cert);
}

/*
 * call-seq:
 *   easy.cert                                        => string
 *
 * Obtain the cert file to use for this Curl::Easy instance.
 */
static VALUE ruby_curl_easy_cert_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, cert);
}

/*
 * call-seq:
 *   easy.cert_key = "cert_key.file"                  => ""
 *
 * Set a cert key to use for this Curl::Easy instance. This file
 * will be used to validate SSL certificates.
 *
 */
static VALUE ruby_curl_easy_cert_key_set(VALUE self, VALUE cert_key) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, cert_key);
}

/*
 * call-seq:
 *   easy.cert_key                                    => "cert_key.file"
 *
 * Obtain the cert key file to use for this Curl::Easy instance.
 */
static VALUE ruby_curl_easy_cert_key_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, cert_key);
}

/*
 * call-seq:
 *   easy.cacert = string                             => ""
 *
 * Set a cacert bundle to use for this Curl::Easy instance. This file
 * will be used to validate SSL certificates.
 *
 */
static VALUE ruby_curl_easy_cacert_set(VALUE self, VALUE cacert) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, cacert);
}

/*
 * call-seq:
 *   easy.cacert                                      => string
 *
 * Obtain the cacert file to use for this Curl::Easy instance.
 */
static VALUE ruby_curl_easy_cacert_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, cacert);
}

/*
 * call-seq:
 *   easy.certpassword = string                       => ""
 *
 * Set a password used to open the specified cert
 */
static VALUE ruby_curl_easy_certpassword_set(VALUE self, VALUE certpassword) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, certpassword);
}

/*
 * call-seq:
 *   easy.certtype = "PEM|DER"                        => ""
 *
 * Set a cert type to use for this Curl::Easy instance.
 * Default is PEM
 *
 */
static VALUE ruby_curl_easy_certtype_set(VALUE self, VALUE certtype) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, certtype);
}

/*
 * call-seq:
 *   easy.certtype                                    => string
 *
 * Obtain the cert type used for this Curl::Easy instance
 */
static VALUE ruby_curl_easy_certtype_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, certtype);
}

/*
 * call-seq:
 *   easy.encoding = string                           => string
 *
 * Set the accepted encoding types, curl will handle all of the decompression
 *
 */
static VALUE ruby_curl_easy_encoding_set(VALUE self, VALUE encoding) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, encoding);
}
/*
 * call-seq:
 *   easy.encoding                                    => string
 *
 * Get the set encoding types
 *
*/
static VALUE ruby_curl_easy_encoding_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, encoding);
}

/*
 * call-seq:
 *   easy.useragent = "Ruby/Curb"                     => ""
 *
 * Set the user agent string for this Curl::Easy instance
 *
 */
static VALUE ruby_curl_easy_useragent_set(VALUE self, VALUE useragent) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, useragent);
}

/*
 * call-seq:
 *   easy.useragent                                  => "Ruby/Curb"
 *
 * Obtain the user agent string used for this Curl::Easy instance
 */
static VALUE ruby_curl_easy_useragent_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, useragent);
}

/*
 * call-seq:
 *   easy.post_body = "some=form%20data&to=send"      => string or nil
 * 
 * Sets the POST body of this Curl::Easy instance.  This is expected to be
 * URL encoded; no additional processing or encoding is done on the string.
 * The content-type header will be set to application/x-www-form-urlencoded.
 * 
 * This is handy if you want to perform a POST against a Curl::Multi instance.
 */
static VALUE ruby_curl_easy_post_body_set(VALUE self, VALUE post_body) {
  ruby_curl_easy *rbce;
  CURL *curl;
  
  char *data;
  long len;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  
  curl = rbce->curl;
  
  if ( post_body == Qnil ) {
    rb_easy_del("postdata_buffer");
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    
  } else {  
    if (rb_type(post_body) == T_STRING) {
      data = StringValuePtr(post_body);
      len = RSTRING_LEN(post_body);
    }
    else if (rb_respond_to(post_body, rb_intern("to_s"))) {
      VALUE str_body = rb_funcall(post_body, rb_intern("to_s"), 0);
      data = StringValuePtr(str_body);
      len = RSTRING_LEN(post_body);
    }
    else {
      rb_raise(rb_eRuntimeError, "post data must respond_to .to_s");
    }
  
    // Store the string, since it has to hang around for the duration of the 
    // request.  See CURLOPT_POSTFIELDS in the libcurl docs.
    //rbce->postdata_buffer = post_body;
    rb_easy_set("postdata_buffer", post_body);
  
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, len);
    
    return post_body;
  }
  
  return Qnil;
}

/*
 * call-seq:
 *   easy.post_body                                  => string or nil
 *
 * Obtain the POST body used in this Curl::Easy instance.
 */
static VALUE ruby_curl_easy_post_body_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, postdata_buffer);
}

/*
 * call-seq:
 *   easy.put_data = data                             => ""
 * 
 * Points this Curl::Easy instance to data to be uploaded via PUT.  This
 * sets the request to a PUT type request - useful if you want to PUT via
 * a multi handle.
 */
static VALUE ruby_curl_easy_put_data_set(VALUE self, VALUE data) {
  ruby_curl_easy *rbce;
  CURL *curl;
  VALUE upload;
  VALUE headers;

  Data_Get_Struct(self, ruby_curl_easy, rbce);

  upload = ruby_curl_upload_new(cCurlUpload);
  ruby_curl_upload_stream_set(upload,data);

  curl = rbce->curl;
  rb_easy_set("upload", upload); /* keep the upload object alive as long as
                                    the easy handle is active or until the upload
                                    is complete or terminated... */

  curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, (curl_read_callback)read_data_handler);
#if HAVE_CURLOPT_SEEKFUNCTION
  curl_easy_setopt(curl, CURLOPT_SEEKFUNCTION, (curl_seek_callback)seek_data_handler);
#endif
  curl_easy_setopt(curl, CURLOPT_READDATA, rbce);
#if HAVE_CURLOPT_SEEKDATA
  curl_easy_setopt(curl, CURLOPT_SEEKDATA, rbce);
#endif

  /* 
   * we need to set specific headers for the PUT to work... so
   * convert the internal headers structure to a HASH if one is set
   */
  if (!rb_easy_nil("headers")) {
    if (rb_easy_type_check("headers", T_ARRAY) || rb_easy_type_check("headers", T_STRING)) {
      rb_raise(rb_eRuntimeError, "Must set headers as a HASH to modify the headers in an PUT request");
    }
  }

  // exit fast if the payload is empty
  if (NIL_P(data)) { return data; }

  headers = rb_easy_get("headers");
  if( headers == Qnil ) { 
    headers = rb_hash_new();
  }

  if (rb_respond_to(data, rb_intern("read"))) {
    VALUE stat = rb_funcall(data, rb_intern("stat"), 0);
    if( stat && rb_hash_aref(headers, rb_str_new2("Content-Length")) == Qnil) {
      VALUE size;
      if( rb_hash_aref(headers, rb_str_new2("Expect")) == Qnil ) {
        rb_hash_aset(headers, rb_str_new2("Expect"), rb_str_new2(""));
      }
      size = rb_funcall(stat, rb_intern("size"), 0);
      curl_easy_setopt(curl, CURLOPT_INFILESIZE, FIX2LONG(size));
    }
    else if( rb_hash_aref(headers, rb_str_new2("Content-Length")) == Qnil && rb_hash_aref(headers, rb_str_new2("Transfer-Encoding")) == Qnil ) {
      rb_hash_aset(headers, rb_str_new2("Transfer-Encoding"), rb_str_new2("chunked"));
    }
    else if( rb_hash_aref(headers, rb_str_new2("Content-Length")) ) {
      VALUE size = rb_funcall(rb_hash_aref(headers, rb_str_new2("Content-Length")), rb_intern("to_i"), 0);
      curl_easy_setopt(curl, CURLOPT_INFILESIZE, FIX2LONG(size));
    }
  }
  else if (rb_respond_to(data, rb_intern("to_s"))) {
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, RSTRING_LEN(data));
    if( rb_hash_aref(headers, rb_str_new2("Expect")) == Qnil ) {
      rb_hash_aset(headers, rb_str_new2("Expect"), rb_str_new2(""));
    }
  }
  else {
    rb_raise(rb_eRuntimeError, "PUT data must respond to read or to_s");
  }
  rb_easy_set("headers",headers);

  // if we made it this far, all should be well.
  return data;
}

/*
 * call-seq:
 *   easy.ftp_commands = ["CWD /", "MKD directory"]   => ["CWD /", ...]
 *
 * Explicitly sets the list of commands to execute on the FTP server when calling perform
 */
static VALUE ruby_curl_easy_ftp_commands_set(VALUE self, VALUE ftp_commands) {
  CURB_OBJECT_HSETTER(ruby_curl_easy, ftp_commands);
}

/*
 * call-seq
 *   easy.ftp_commands                                => array or nil
 */
static VALUE ruby_curl_easy_ftp_commands_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, ftp_commands);
}

/* ================== IMMED ATTRS ==================*/

/*
 * call-seq:
 *   easy.local_port = fixnum or nil                  => fixnum or nil
 *
 * Set the local port that will be used for the following +perform+ calls.
 *
 * Passing +nil+ will return to the default behaviour (no local port
 * preference).
 *
 * This option is ignored if compiled against libcurl < 7.15.2.
 */
static VALUE ruby_curl_easy_local_port_set(VALUE self, VALUE local_port) {
  CURB_IMMED_PORT_SETTER(ruby_curl_easy, local_port, "port");
}

/*
 * call-seq:
 *   easy.local_port                                  => fixnum or nil
 *
 * Obtain the local port that will be used for the following +perform+ calls.
 *
 * This option is ignored if compiled against libcurl < 7.15.2.
 */
static VALUE ruby_curl_easy_local_port_get(VALUE self) {
  CURB_IMMED_PORT_GETTER(ruby_curl_easy, local_port);
}

/*
 * call-seq:
 *   easy.local_port_range = fixnum or nil            => fixnum or nil
 *
 * Set the local port range that will be used for the following +perform+
 * calls. This is a number (between 0 and 65535) that determines how far
 * libcurl may deviate from the supplied +local_port+ in order to find
 * an available port.
 *
 * If you set +local_port+ it's also recommended that you set this, since
 * it is fairly likely that your specified port will be unavailable.
 *
 * This option is ignored if compiled against libcurl < 7.15.2.
 */
static VALUE ruby_curl_easy_local_port_range_set(VALUE self, VALUE local_port_range) {
  CURB_IMMED_PORT_SETTER(ruby_curl_easy, local_port_range, "port range");
}

/*
 * call-seq:
 *   easy.local_port_range                            => fixnum or nil
 *
 * Obtain the local port range that will be used for the following +perform+
 * calls.
 *
 * This option is ignored if compiled against libcurl < 7.15.2.
 */
static VALUE ruby_curl_easy_local_port_range_get(VALUE self) {
  CURB_IMMED_PORT_GETTER(ruby_curl_easy, local_port_range);
}

/*
 * call-seq:
 *   easy.proxy_port = fixnum or nil                  => fixnum or nil
 *
 * Set the proxy port that will be used for the following +perform+ calls.
 */
static VALUE ruby_curl_easy_proxy_port_set(VALUE self, VALUE proxy_port) {
  CURB_IMMED_PORT_SETTER(ruby_curl_easy, proxy_port, "port");
}

/*
 * call-seq:
 *   easy.proxy_port                                  => fixnum or nil
 *
 * Obtain the proxy port that will be used for the following +perform+ calls.
 */
static VALUE ruby_curl_easy_proxy_port_get(VALUE self) {
  CURB_IMMED_PORT_GETTER(ruby_curl_easy, proxy_port);
}

/*
 * call-seq:
 *   easy.proxy_type = fixnum or nil                  => fixnum or nil
 *
 * Set the proxy type that will be used for the following +perform+ calls.
 * This should be one of the Curl::CURLPROXY constants.
 */
static VALUE ruby_curl_easy_proxy_type_set(VALUE self, VALUE proxy_type) {
  CURB_IMMED_SETTER(ruby_curl_easy, proxy_type, -1);
}

/*
 * call-seq:
 *   easy.proxy_type                                  => fixnum or nil
 *
 * Obtain the proxy type that will be used for the following +perform+ calls.
 */
static VALUE ruby_curl_easy_proxy_type_get(VALUE self) {
  CURB_IMMED_GETTER(ruby_curl_easy, proxy_type, -1);
}

#if defined(HAVE_CURLAUTH_DIGEST_IE)
#define CURL_HTTPAUTH_STR_TO_NUM(node) \
  (!strncmp("basic",node,5)) ? CURLAUTH_BASIC : \
  (!strncmp("digest_ie",node,9)) ? CURLAUTH_DIGEST_IE : \
  (!strncmp("digest",node,6)) ? CURLAUTH_DIGEST : \
  (!strncmp("gssnegotiate",node,12)) ? CURLAUTH_GSSNEGOTIATE : \
  (!strncmp("ntlm",node,4)) ? CURLAUTH_NTLM : \
  (!strncmp("any",node,3)) ? CURLAUTH_ANY : \
  (!strncmp("anysafe",node,7)) ? CURLAUTH_ANYSAFE : 0
#else 
#define CURL_HTTPAUTH_STR_TO_NUM(node) \
  (!strncmp("basic",node,5)) ? CURLAUTH_BASIC : \
  (!strncmp("digest",node,6)) ? CURLAUTH_DIGEST : \
  (!strncmp("gssnegotiate",node,12)) ? CURLAUTH_GSSNEGOTIATE : \
  (!strncmp("ntlm",node,4)) ? CURLAUTH_NTLM : \
  (!strncmp("any",node,3)) ? CURLAUTH_ANY : \
  (!strncmp("anysafe",node,7)) ? CURLAUTH_ANYSAFE : 0
#endif
/*
 * call-seq:
 *   easy.http_auth_types = fixnum or nil             => fixnum or nil
 *   easy.http_auth_types = [:basic,:digest,:digest_ie,:gssnegotiate, :ntlm, :any, :anysafe]
 *
 * Set the HTTP authentication types that may be used for the following
 * +perform+ calls. This is a bitmap made by ORing together the
 * Curl::CURLAUTH constants.
 */
static VALUE ruby_curl_easy_http_auth_types_set(int argc, VALUE *argv, VALUE self) {//VALUE self, VALUE http_auth_types) {
  ruby_curl_easy *rbce;
  VALUE args_ary;
  int i, len;
  char* node = NULL;
  long mask = 0x000000;

  rb_scan_args(argc, argv, "*", &args_ary);
  Data_Get_Struct(self, ruby_curl_easy, rbce);

  len = (int)RARRAY_LEN(args_ary);

  if (len == 1 && (TYPE(rb_ary_entry(args_ary,0)) == T_FIXNUM || rb_ary_entry(args_ary,0) == Qnil)) {
    if (rb_ary_entry(args_ary,0) == Qnil) { 
      rbce->http_auth_types = 0;
    }
    else {
      rbce->http_auth_types = NUM2INT(rb_ary_entry(args_ary,0));
    }
  }
  else {
    // we could have multiple values, but they should be symbols
    node = RSTRING_PTR(rb_funcall(rb_ary_entry(args_ary,0),rb_intern("to_s"),0));
    mask = CURL_HTTPAUTH_STR_TO_NUM(node);
    for( i = 1; i < len; ++i ) {
      node = RSTRING_PTR(rb_funcall(rb_ary_entry(args_ary,i),rb_intern("to_s"),0));
      mask |= CURL_HTTPAUTH_STR_TO_NUM(node);
    }
    rbce->http_auth_types = mask;
  }
  return INT2NUM(rbce->http_auth_types);
}

/*
 * call-seq:
 *   easy.http_auth_types                             => fixnum or nil
 *
 * Obtain the HTTP authentication types that may be used for the following
 * +perform+ calls.
 */
static VALUE ruby_curl_easy_http_auth_types_get(VALUE self) {
  CURB_IMMED_GETTER(ruby_curl_easy, http_auth_types, 0);
}

/*
 * call-seq:
 *   easy.proxy_auth_types = fixnum or nil            => fixnum or nil
 *
 * Set the proxy authentication types that may be used for the following
 * +perform+ calls. This is a bitmap made by ORing together the
 * Curl::CURLAUTH constants.
 */
static VALUE ruby_curl_easy_proxy_auth_types_set(VALUE self, VALUE proxy_auth_types) {
  CURB_IMMED_SETTER(ruby_curl_easy, proxy_auth_types, 0);
}

/*
 * call-seq:
 *   easy.proxy_auth_types                            => fixnum or nil
 *
 * Obtain the proxy authentication types that may be used for the following
 * +perform+ calls.
 */
static VALUE ruby_curl_easy_proxy_auth_types_get(VALUE self) {
  CURB_IMMED_GETTER(ruby_curl_easy, proxy_auth_types, 0);
}

/*
 * call-seq:
 *   easy.max_redirects = fixnum or nil               => fixnum or nil
 *
 * Set the maximum number of redirections to follow in the following +perform+
 * calls. Set to nil or -1 allow an infinite number (the default). Setting this
 * option only makes sense if +follow_location+ is also set true.
 *
 * With libcurl >= 7.15.1, setting this to 0 will cause libcurl to refuse any
 * redirect.
 */
static VALUE ruby_curl_easy_max_redirects_set(VALUE self, VALUE max_redirs) {
  CURB_IMMED_SETTER(ruby_curl_easy, max_redirs, -1);
}

/*
 * call-seq:
 *   easy.max_redirects                               => fixnum or nil
 *
 * Obtain the maximum number of redirections to follow in the following
 * +perform+ calls.
 */
static VALUE ruby_curl_easy_max_redirects_get(VALUE self) {
  CURB_IMMED_GETTER(ruby_curl_easy, max_redirs, -1);
}

/*
 * call-seq:
 *   easy.timeout = fixnum or nil                     => fixnum or nil
 *
 * Set the maximum time in seconds that you allow the libcurl transfer
 * operation to take. Normally, name lookups can take a considerable time
 * and limiting operations to less than a few minutes risk aborting
 * perfectly normal operations.
 *
 * Set to nil (or zero) to disable timeout (it will then only timeout
 * on the system's internal timeouts).
 */
static VALUE ruby_curl_easy_timeout_set(VALUE self, VALUE timeout) {
  CURB_IMMED_SETTER(ruby_curl_easy, timeout, 0);
}

/*
 * call-seq:
 *   easy.timeout                                     => fixnum or nil
 *
 * Obtain the maximum time in seconds that you allow the libcurl transfer
 * operation to take.
 */
static VALUE ruby_curl_easy_timeout_get(VALUE self, VALUE timeout) {
  CURB_IMMED_GETTER(ruby_curl_easy, timeout, 0);
}

/*
 * call-seq:
 *   easy.connect_timeout = fixnum or nil             => fixnum or nil
 *
 * Set the maximum time in seconds that you allow the connection to the
 * server to take. This only limits the connection phase, once it has
 * connected, this option is of no more use.
 *
 * Set to nil (or zero) to disable connection timeout (it will then only
 * timeout on the system's internal timeouts).
 */
static VALUE ruby_curl_easy_connect_timeout_set(VALUE self, VALUE connect_timeout) {
  CURB_IMMED_SETTER(ruby_curl_easy, connect_timeout, 0);
}

/*
 * call-seq:
 *   easy.connect_timeout                             => fixnum or nil
 *
 * Obtain the maximum time in seconds that you allow the connection to the
 * server to take.
 */
static VALUE ruby_curl_easy_connect_timeout_get(VALUE self, VALUE connect_timeout) {
  CURB_IMMED_GETTER(ruby_curl_easy, connect_timeout, 0);
}

/*
 * call-seq:
 *   easy.dns_cache_timeout = fixnum or nil           => fixnum or nil
 *
 * Set the dns cache timeout in seconds. Name resolves will be kept in
 * memory for this number of seconds. Set to zero (0) to completely disable
 * caching, or set to nil (or -1) to make the cached entries remain forever.
 * By default, libcurl caches this info for 60 seconds.
 */
static VALUE ruby_curl_easy_dns_cache_timeout_set(VALUE self, VALUE dns_cache_timeout) {
  CURB_IMMED_SETTER(ruby_curl_easy, dns_cache_timeout, -1);
}

/*
 * call-seq:
 *   easy.dns_cache_timeout                           => fixnum or nil
 *
 * Obtain the dns cache timeout in seconds.
 */
static VALUE ruby_curl_easy_dns_cache_timeout_get(VALUE self, VALUE dns_cache_timeout) {
  CURB_IMMED_GETTER(ruby_curl_easy, dns_cache_timeout, -1);
}

/*
 * call-seq:
 *   easy.ftp_response_timeout = fixnum or nil        => fixnum or nil
 *
 * Set a timeout period (in seconds) on the amount of time that the server
 * is allowed to take in order to generate a response message for a command
 * before the session is considered hung. While curl is waiting for a
 * response, this value overrides +timeout+. It is recommended that if used
 * in conjunction with +timeout+, you set +ftp_response_timeout+ to a value
 * smaller than +timeout+.
 *
 * Ignored if libcurl version is < 7.10.8.
 */
static VALUE ruby_curl_easy_ftp_response_timeout_set(VALUE self, VALUE ftp_response_timeout) {
  CURB_IMMED_SETTER(ruby_curl_easy, ftp_response_timeout, 0);
}

/*
 * call-seq:
 *   easy.ftp_response_timeout                        => fixnum or nil
 *
 * Obtain the maximum time that libcurl will wait for FTP command responses.
 */
static VALUE ruby_curl_easy_ftp_response_timeout_get(VALUE self, VALUE ftp_response_timeout) {
  CURB_IMMED_GETTER(ruby_curl_easy, ftp_response_timeout, 0);
}

/*
 * call-seq:
 *   easy.low_speed_limit = fixnum or nil        => fixnum or nil
 *
 * Set the transfer speed (in bytes per second) that the transfer should be
 * below during +low_speed_time+ seconds for the library to consider it too
 * slow and abort.
 */
static VALUE ruby_curl_easy_low_speed_limit_set(VALUE self, VALUE low_speed_limit) {
  CURB_IMMED_SETTER(ruby_curl_easy, low_speed_limit, 0);
}

/*
 * call-seq:
 *   easy.low_speed_limit                        => fixnum or nil
 *
 * Obtain the minimum transfer speed over +low_speed+time+ below which the
 * transfer will be aborted.
 */
static VALUE ruby_curl_easy_low_speed_limit_get(VALUE self, VALUE low_speed_limit) {
  CURB_IMMED_GETTER(ruby_curl_easy, low_speed_limit, 0);
}

/*
 * call-seq:
 *   easy.low_speed_time = fixnum or nil        => fixnum or nil
 *
 * Set the time (in seconds) that the transfer should be below the
 * +low_speed_limit+ for the library to consider it too slow and abort.
 */
static VALUE ruby_curl_easy_low_speed_time_set(VALUE self, VALUE low_speed_time) {
  CURB_IMMED_SETTER(ruby_curl_easy, low_speed_time, 0);
}

/*
 * call-seq:
 *   easy.low_speed_time                        => fixnum or nil
 *
 * Obtain the time that the transfer should be below +low_speed_limit+ for
 * the library to abort it.
 */
static VALUE ruby_curl_easy_low_speed_time_get(VALUE self, VALUE low_speed_time) {
  CURB_IMMED_GETTER(ruby_curl_easy, low_speed_time, 0);
}

/*
 * call-seq:
 *   easy.username = string                           => string
 *
 * Set the HTTP Authentication username.
 */
static VALUE ruby_curl_easy_username_set(VALUE self, VALUE username) {
#if HAVE_CURLOPT_USERNAME
  CURB_OBJECT_HSETTER(ruby_curl_easy, username);
#else
  return Qnil;
#endif
}

/*
 * call-seq:
 *   easy.username                                    => string
 * 
 * Get the current username
 */
static VALUE ruby_curl_easy_username_get(VALUE self, VALUE username) {
#if HAVE_CURLOPT_USERNAME
  CURB_OBJECT_HGETTER(ruby_curl_easy, username);
#else
  return Qnil;
#endif
}

/*
 * call-seq:
 *   easy.password = string                           => string
 *
 * Set the HTTP Authentication password.
 */
static VALUE ruby_curl_easy_password_set(VALUE self, VALUE password) {
#if HAVE_CURLOPT_PASSWORD
  CURB_OBJECT_HSETTER(ruby_curl_easy, password);
#else
  return Qnil;
#endif
}

/*
 * call-seq:
 *   easy.password                                    => string
 * 
 * Get the current password
 */
static VALUE ruby_curl_easy_password_get(VALUE self, VALUE password) {
#if HAVE_CURLOPT_PASSWORD
  CURB_OBJECT_HGETTER(ruby_curl_easy, password);
#else
  return Qnil;
#endif
}

/*
 * call-seq:
 *   easy.ssl_version = value                         => fixnum or nil
 *
 * Sets the version of SSL/TLS that libcurl will attempt to use. Valid
 * options are Curl::CURL_SSLVERSION_TLSv1, Curl::CURL_SSLVERSION::SSLv2,
 * Curl::CURL_SSLVERSION_SSLv3 and Curl::CURL_SSLVERSION_DEFAULT
 */
static VALUE ruby_curl_easy_ssl_version_set(VALUE self, VALUE ssl_version) {
  CURB_IMMED_SETTER(ruby_curl_easy, ssl_version, -1);
}

/*
 * call-seq:
 *   easy.ssl_version                                 => fixnum
 *
 * Get the version of SSL/TLS that libcurl will attempt to use.
 */
static VALUE ruby_curl_easy_ssl_version_get(VALUE self, VALUE ssl_version) {
  CURB_IMMED_GETTER(ruby_curl_easy, ssl_version, -1);
}

/*
 * call-seq:
 *   easy.use_ssl = value                             => fixnum or nil
 * 
 * Ensure libcurl uses SSL for FTP connections. Valid options are Curl::CURL_USESSL_NONE,
 * Curl::CURL_USESSL_TRY, Curl::CURL_USESSL_CONTROL, and Curl::CURL_USESSL_ALL.
 */
static VALUE ruby_curl_easy_use_ssl_set(VALUE self, VALUE use_ssl) {
  CURB_IMMED_SETTER(ruby_curl_easy, use_ssl, -1);
}

/*
 * call-seq:
 *   easy.use_ssl                                     => fixnum
 *
 * Get the desired level for using SSL on FTP connections.
 */
static VALUE ruby_curl_easy_use_ssl_get(VALUE self, VALUE use_ssl) {
  CURB_IMMED_GETTER(ruby_curl_easy, use_ssl, -1);
}

/*
 * call-seq:
 *   easy.ftp_filemethod = value                      => fixnum or nil
 *
 * Controls how libcurl reaches files on the server. Valid options are Curl::CURL_MULTICWD,
 * Curl::CURL_NOCWD, and Curl::CURL_SINGLECWD (see libcurl docs for CURLOPT_FTP_METHOD).
 */
static VALUE ruby_curl_easy_ftp_filemethod_set(VALUE self, VALUE ftp_filemethod) {
  CURB_IMMED_SETTER(ruby_curl_easy, ftp_filemethod, -1);
}

/*
 * call-seq
 *   easy.ftp_filemethod                              => fixnum
 *
 * Get the configuration for how libcurl will reach files on the server.
 */
static VALUE ruby_curl_easy_ftp_filemethod_get(VALUE self, VALUE ftp_filemethod) {
  CURB_IMMED_GETTER(ruby_curl_easy, ftp_filemethod, -1);
}

/* ================== BOOL ATTRS ===================*/

/*
 * call-seq:
 *   easy.proxy_tunnel = boolean                      => boolean
 *
 * Configure whether this Curl instance will use proxy tunneling.
 */
static VALUE ruby_curl_easy_proxy_tunnel_set(VALUE self, VALUE proxy_tunnel) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, proxy_tunnel);
}

/*
 * call-seq:
 *   easy.proxy_tunnel?                               => boolean
 *
 * Determine whether this Curl instance will use proxy tunneling.
 */
static VALUE ruby_curl_easy_proxy_tunnel_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, proxy_tunnel);
}

/*
 * call-seq:
 *   easy.fetch_file_time = boolean                   => boolean
 *
 * Configure whether this Curl instance will fetch remote file
 * times, if available.
 */
static VALUE ruby_curl_easy_fetch_file_time_set(VALUE self, VALUE fetch_file_time) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, fetch_file_time);
}

/*
 * call-seq:
 *   easy.fetch_file_time?                            => boolean
 *
 * Determine whether this Curl instance will fetch remote file
 * times, if available.
 */
static VALUE ruby_curl_easy_fetch_file_time_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, fetch_file_time);
}

/*
 * call-seq:
 *   easy.ssl_verify_peer = boolean                   => boolean
 *
 * Configure whether this Curl instance will verify the SSL peer
 * certificate. When true (the default), and the verification fails to
 * prove that the certificate is authentic, the connection fails. When
 * false, the connection succeeds regardless.
 *
 * Authenticating the certificate is not by itself very useful. You
 * typically want to ensure that the server, as authentically identified
 * by its certificate, is the server you mean to be talking to.
 * The ssl_verify_host? options controls that.
 */
static VALUE ruby_curl_easy_ssl_verify_peer_set(VALUE self, VALUE ssl_verify_peer) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, ssl_verify_peer);
}

/*
 * call-seq:
 *   easy.ssl_verify_peer?                            => boolean
 *
 * Determine whether this Curl instance will verify the SSL peer
 * certificate.
 */
static VALUE ruby_curl_easy_ssl_verify_peer_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, ssl_verify_peer);
}

/*
 * call-seq:
 *   easy.ssl_verify_host = [0, 1, 2]                   => [0, 1, 2]
 *
 * Configure whether this Curl instance will verify that the server cert
 * is for the server it is known as. When true (the default) the server
 * certificate must indicate that the server is the server to which you
 * meant to connect, or the connection fails. When false, the connection
 * will succeed regardless of the names in the certificate.
 *
 * this option controls is of the identity that the server claims.
 * The server could be lying. To control lying, see ssl_verify_peer? .
 */
static VALUE ruby_curl_easy_ssl_verify_host_set(VALUE self, VALUE ssl_verify_host) {
  CURB_IMMED_SETTER(ruby_curl_easy, ssl_verify_host, 0);
}

/*
 * call-seq:
 *   easy.ssl_verify_host                            => number
 *
 * Determine whether this Curl instance will verify that the server cert
 * is for the server it is known as.
 */
static VALUE ruby_curl_easy_ssl_verify_host_get(VALUE self) {
  CURB_IMMED_GETTER(ruby_curl_easy, ssl_verify_host, 0);
}

/*
 * call-seq:
 *   easy.header_in_body = boolean                    => boolean
 *
 * Configure whether this Curl instance will return HTTP headers
 * combined with body data. If this option is set true, both header
 * and body data will go to +body_str+ (or the configured +on_body+ handler).
 */
static VALUE ruby_curl_easy_header_in_body_set(VALUE self, VALUE header_in_body) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, header_in_body);
}

/*
 * call-seq:
 *   easy.header_in_body?                             => boolean
 *
 * Determine whether this Curl instance will return HTTP headers
 * combined with body data.
 */
static VALUE ruby_curl_easy_header_in_body_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, header_in_body);
}

/*
 * call-seq:
 *   easy.use_netrc = boolean                         => boolean
 *
 * Configure whether this Curl instance will use data from the user's
 * .netrc file for FTP connections.
 */
static VALUE ruby_curl_easy_use_netrc_set(VALUE self, VALUE use_netrc) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, use_netrc);
}

/*
 * call-seq:
 *   easy.use_netrc?                                  => boolean
 *
 * Determine whether this Curl instance will use data from the user's
 * .netrc file for FTP connections.
 */
static VALUE ruby_curl_easy_use_netrc_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, use_netrc);
}

/*
 * call-seq:
 *
 * easy = Curl::Easy.new
 * easy.autoreferer=true
 */
static VALUE ruby_curl_easy_autoreferer_set(VALUE self, VALUE autoreferer) {
  ruby_curl_easy *rbce;
  Data_Get_Struct(self, ruby_curl_easy, rbce);

  if (Qtrue == autoreferer) {
    curl_easy_setopt(rbce->curl, CURLOPT_AUTOREFERER, 1);
  }
  else {
    curl_easy_setopt(rbce->curl, CURLOPT_AUTOREFERER, 0);
  }

  return autoreferer;
}

/*
 * call-seq:
 *   easy.follow_location?                            => boolean
 *
 * Determine whether this Curl instance will follow Location: headers
 * in HTTP responses.
 */
static VALUE ruby_curl_easy_follow_location_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, follow_location);
}

/*
 * call-seq:
 *   easy.unrestricted_auth = boolean                 => boolean
 *
 * Configure whether this Curl instance may use any HTTP authentication
 * method available when necessary.
 */
static VALUE ruby_curl_easy_unrestricted_auth_set(VALUE self, VALUE unrestricted_auth) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, unrestricted_auth);
}

/*
 * call-seq:
 *   easy.unrestricted_auth?                          => boolean
 *
 * Determine whether this Curl instance may use any HTTP authentication
 * method available when necessary.
 */
static VALUE ruby_curl_easy_unrestricted_auth_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, unrestricted_auth);
}

/*
 * call-seq:
 *   easy.verbose = boolean                           => boolean
 *
 * Configure whether this Curl instance gives verbose output to STDERR
 * during transfers. Ignored if this instance has an on_debug handler.
 */
static VALUE ruby_curl_easy_verbose_set(VALUE self, VALUE verbose) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, verbose);
}

/*
 * call-seq:
 *   easy.verbose?                                    => boolean
 *
 * Determine whether this Curl instance gives verbose output to STDERR
 * during transfers.
 */
static VALUE ruby_curl_easy_verbose_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, verbose);
}

/*
 * call-seq:
 *   easy.multipart_form_post = boolean               => boolean
 *
 * Configure whether this Curl instance uses multipart/formdata content
 * type for HTTP POST requests. If this is false (the default), then the
 * application/x-www-form-urlencoded content type is used for the form
 * data.
 *
 * If this is set true, you must pass one or more PostField instances
 * to the http_post method - no support for posting multipart forms from
 * a string is provided.
 */
static VALUE ruby_curl_easy_multipart_form_post_set(VALUE self, VALUE multipart_form_post)
{
  CURB_BOOLEAN_SETTER(ruby_curl_easy, multipart_form_post);
}

/*
 * call-seq:
 *   easy.multipart_form_post?                        => boolean
 *
 * Determine whether this Curl instance uses multipart/formdata content
 * type for HTTP POST requests.
 */
static VALUE ruby_curl_easy_multipart_form_post_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, multipart_form_post);
}

/*
 * call-seq:
 *   easy.enable_cookies = boolean                    => boolean
 *
 * Configure whether the libcurl cookie engine is enabled for this Curl::Easy
 * instance.
 */
static VALUE ruby_curl_easy_enable_cookies_set(VALUE self, VALUE enable_cookies)
{
  CURB_BOOLEAN_SETTER(ruby_curl_easy, enable_cookies);
}

/*
 * call-seq:
 *   easy.enable_cookies?                             => boolean
 *
 * Determine whether the libcurl cookie engine is enabled for this
 * Curl::Easy instance.
 */
static VALUE ruby_curl_easy_enable_cookies_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, enable_cookies);
}

/*
 * call-seq:
 *   easy.ignore_content_length = boolean
 *
 * Configure whether this Curl::Easy instance should ignore the content
 * length header.
 */
static VALUE ruby_curl_easy_ignore_content_length_set(VALUE self, VALUE ignore_content_length)
{
  CURB_BOOLEAN_SETTER(ruby_curl_easy, ignore_content_length);
}

/*
 * call-seq:
 *   easy.ignore_content_length?                             => boolean
 *
 * Determine whether this Curl::Easy instance ignores the content
 * length header.
 */
static VALUE ruby_curl_easy_ignore_content_length_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, ignore_content_length);
}

/*
 * call-seq:
 *   easy.resolve_mode                                      => symbol
 *
 * Determines what type of IP address this Curl::Easy instance
 * resolves DNS names to.
 */
static VALUE ruby_curl_easy_resolve_mode(VALUE self) {
  ruby_curl_easy *rbce;
  unsigned short rm;
  Data_Get_Struct(self, ruby_curl_easy, rbce);

  rm = rbce->resolve_mode;

  switch(rm) {
    case CURL_IPRESOLVE_V4:
      return rb_easy_sym("ipv4");
    case CURL_IPRESOLVE_V6:
      return rb_easy_sym("ipv6");
    default:
      return rb_easy_sym("auto");
  }
}

/*
 * call-seq:
 *   easy.resolve_mode = symbol                             => symbol
 *
 * Configures what type of IP address this Curl::Easy instance
 * resolves DNS names to. Valid options are:
 *
 * [:auto]  resolves DNS names to all IP versions your system allows
 * [:ipv4]  resolves DNS names to IPv4 only
 * [:ipv6]  resolves DNS names to IPv6 only
 */
static VALUE ruby_curl_easy_resolve_mode_set(VALUE self, VALUE resolve_mode) {
  if (TYPE(resolve_mode) != T_SYMBOL) {
    rb_raise(rb_eTypeError, "Must pass a symbol");
    return Qnil;
  } else {
    ruby_curl_easy *rbce;
    ID resolve_mode_id;
    Data_Get_Struct(self, ruby_curl_easy, rbce);

    resolve_mode_id = rb_to_id(resolve_mode);

    if (resolve_mode_id == rb_intern("auto")) {
      rbce->resolve_mode = CURL_IPRESOLVE_WHATEVER;
      return resolve_mode;
    } else if (resolve_mode_id == rb_intern("ipv4")) {
      rbce->resolve_mode = CURL_IPRESOLVE_V4;
      return resolve_mode;
    } else if (resolve_mode_id == rb_intern("ipv6")) {
      rbce->resolve_mode = CURL_IPRESOLVE_V6;
      return resolve_mode;
    } else {
      rb_raise(rb_eArgError, "Must set to one of :auto, :ipv4, :ipv6");
      return Qnil;
    }
  }
}


/* ================= EVENT PROCS ================== */

/*
 * call-seq:
 *   easy.on_body { |body_data| ... }                 => &lt;old handler&gt;
 *
 * Assign or remove the +on_body+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 *
 * The +on_body+ handler is called for each chunk of response body passed back
 * by libcurl during +perform+. It should perform any processing necessary,
 * and return the actual number of bytes handled. Normally, this will
 * equal the length of the data string, and CURL will continue processing.
 * If the returned length does not equal the input length, CURL will abort
 * the processing with a Curl::Err::AbortedByCallbackError.
 */
static VALUE ruby_curl_easy_on_body_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, body_proc);
}

/*
 * call-seq:
 *   easy.on_success { |easy| ... }                   => &lt;old handler&gt;
 *
 * Assign or remove the +on_success+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 *
 * The +on_success+ handler is called when the request is finished with a
 * status of 20x
 */
static VALUE ruby_curl_easy_on_success_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, success_proc);
}

/*
 * call-seq:
 *   easy.on_failure {|easy,code| ... }               => &lt;old handler&gt;
 *
 * Assign or remove the +on_failure+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 *
 * The +on_failure+ handler is called when the request is finished with a
 * status of 50x
 */
static VALUE ruby_curl_easy_on_failure_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, failure_proc);
}

/*
 * call-seq:
 *  easy.on_missing {|easy,code| ... }                => &lt;old handler;&gt;
 *
 *  Assign or remove the on_missing handler for this Curl::Easy instance.
 *  To remove a previously-supplied handler, call this method with no attached
 *  block.
 *
 *  The +on_missing+ handler is called when request is finished with a 
 *  status of 40x
 */
static VALUE ruby_curl_easy_on_missing_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, missing_proc);
}

/*
 * call-seq:
 *  easy.on_redirect {|easy,code| ... }                => &lt;old handler;&gt;
 *
 *  Assign or remove the on_redirect handler for this Curl::Easy instance.
 *  To remove a previously-supplied handler, call this method with no attached
 *  block.
 *
 *  The +on_redirect+ handler is called when request is finished with a 
 *  status of 30x
 */
static VALUE ruby_curl_easy_on_redirect_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, redirect_proc);
}

/*
 * call-seq:
 *   easy.on_complete {|easy| ... }                   => &lt;old handler&gt;
 *
 * Assign or remove the +on_complete+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 *
 * The +on_complete+ handler is called when the request is finished.
 */
static VALUE ruby_curl_easy_on_complete_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, complete_proc);
}

/*
 * call-seq:
 *   easy.on_header { |header_data| ... }             => &lt;old handler&gt;
 *
 * Assign or remove the +on_header+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 *
 * The +on_header+ handler is called for each chunk of response header passed
 * back by libcurl during +perform+. The semantics are the same as for the
 * block supplied to +on_body+.
 */
static VALUE ruby_curl_easy_on_header_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, header_proc);
}

/*
 * call-seq:
 *   easy.on_progress { |dl_total, dl_now, ul_total, ul_now| ... } => &lt;old handler&gt;
 *
 * Assign or remove the +on_progress+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 *
 * The +on_progress+ handler is called regularly by libcurl (approximately once
 * per second) during transfers to allow the application to receive progress
 * information. There is no guarantee that the reported progress will change
 * between calls.
 *
 * The result of the block call determines whether libcurl continues the transfer.
 * Returning a non-true value (i.e. nil or false) will cause the transfer to abort,
 * throwing a Curl::Err::AbortedByCallbackError.
 */
static VALUE ruby_curl_easy_on_progress_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, progress_proc);
}

/*
 * call-seq:
 *   easy.on_debug { |type, data| ... }               => &lt;old handler&gt;
 *
 * Assign or remove the +on_debug+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 *
 * The +on_debug+ handler, if configured, will receive detailed information
 * from libcurl during the perform call. This can be useful for debugging.
 * Setting a debug handler overrides libcurl's internal handler, disabling
 * any output from +verbose+, if set.
 *
 * The type argument will match one of the Curl::Easy::CURLINFO_XXXX
 * constants, and specifies the kind of information contained in the
 * data. The data is passed as a String.
 */
static VALUE ruby_curl_easy_on_debug_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_HSETTER(ruby_curl_easy, debug_proc);
}


/* =================== PERFORM =====================*/

/***********************************************
 * This is an rb_iterate callback used to set up http headers.
 */
static VALUE cb_each_http_header(VALUE header, VALUE wrap) {
  struct curl_slist **list;
  VALUE header_str = Qnil;

  Data_Get_Struct(wrap, struct curl_slist *, list);

  //rb_p(header);

  if (rb_type(header) == T_ARRAY) {
    // we're processing a hash, header is [name, val]
    VALUE name, value;

    name = rb_obj_as_string(rb_ary_entry(header, 0));
    value = rb_obj_as_string(rb_ary_entry(header, 1));

    // This is a bit inefficient, but we don't want to be modifying
    // the actual values in the original hash.
    header_str = rb_str_plus(name, rb_str_new2(": "));
    header_str = rb_str_plus(header_str, value);
  } else {
    header_str = rb_obj_as_string(header);
  }

  //rb_p(header_str);

  *list = curl_slist_append(*list, StringValuePtr(header_str));
  return header_str;
}

/***********************************************
 * This is an rb_iterate callback used to set up ftp commands.
 */
static VALUE cb_each_ftp_command(VALUE ftp_command, VALUE wrap) {
  struct curl_slist **list;
  VALUE ftp_command_string;
  Data_Get_Struct(wrap, struct curl_slist *, list);

  ftp_command_string = rb_obj_as_string(ftp_command);
  *list = curl_slist_append(*list, StringValuePtr(ftp_command));

  return ftp_command_string;
}

/***********************************************
 *
 * Setup a connection
 *
 * Always returns Qtrue, rb_raise on error.
 */
VALUE ruby_curl_easy_setup(ruby_curl_easy *rbce) {
  // TODO this could do with a bit of refactoring...
  CURL *curl;
  VALUE url, _url = rb_easy_get("url");
  struct curl_slist **hdrs = &(rbce->curl_headers);
  struct curl_slist **cmds = &(rbce->curl_ftp_commands);

  curl = rbce->curl;

  if (_url == Qnil) {
    rb_raise(eCurlErrError, "No URL supplied");
  }

  url = rb_check_string_type(_url);

  curl_easy_setopt(curl, CURLOPT_URL, StringValuePtr(url));

  // network stuff and auth
  if (!rb_easy_nil("interface_hm")) {
    curl_easy_setopt(curl, CURLOPT_INTERFACE, rb_easy_get_str("interface_hm"));
  } else {
    curl_easy_setopt(curl, CURLOPT_INTERFACE, NULL);
  }

#if HAVE_CURLOPT_USERNAME == 1 && HAVE_CURLOPT_PASSWORD == 1
  if (!rb_easy_nil("username")) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, rb_easy_get_str("username"));
  } else {
    curl_easy_setopt(curl, CURLOPT_USERNAME, NULL);
  }
  if (!rb_easy_nil("password")) {
    curl_easy_setopt(curl, CURLOPT_PASSWORD, rb_easy_get_str("password"));
  }
  else {
    curl_easy_setopt(curl, CURLOPT_PASSWORD, NULL);
  }
#endif

  if (!rb_easy_nil("userpwd")) {
    curl_easy_setopt(curl, CURLOPT_USERPWD, rb_easy_get_str("userpwd"));
#if HAVE_CURLOPT_USERNAME == 1
  } else if (rb_easy_nil("username") && rb_easy_nil("password")) { /* don't set this even to NULL if we have set username and password */
#else
  } else {
#endif
    curl_easy_setopt(curl, CURLOPT_USERPWD, NULL);
  }

  if (rb_easy_nil("proxy_url")) {
    curl_easy_setopt(curl, CURLOPT_PROXY, NULL);
  } else {
    curl_easy_setopt(curl, CURLOPT_PROXY, rb_easy_get_str("proxy_url"));
  }

  if (rb_easy_nil("proxypwd")) {
    curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, NULL);
  } else {
    curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, rb_easy_get_str("proxypwd"));
  }

  // body/header procs
  if (!rb_easy_nil("body_proc")) {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)&proc_data_handler_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, rbce);
    /* clear out the body_data if it was set */
    rb_easy_del("body_data");
  } else {
    VALUE body_buffer = rb_easy_set("body_data", rb_str_buf_new(32768));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)&default_data_handler);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, body_buffer);
  }

  if (!rb_easy_nil("header_proc")) {
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)&proc_data_handler_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, rbce);
    /* clear out the header_data if it was set */
    rb_easy_del("header_data");
  } else {
    VALUE header_buffer = rb_easy_set("header_data", rb_str_buf_new(16384));
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)&default_data_handler);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, header_buffer);
  }

  /* encoding */
  if (!rb_easy_nil("encoding")) {
    curl_easy_setopt(curl, CURLOPT_ENCODING, rb_easy_get_str("encoding"));
  }

  // progress and debug procs
  if (!rb_easy_nil("progress_proc")) {
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, (curl_progress_callback)&proc_progress_handler);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, rb_easy_get("progress_proc"));
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
  } else {
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
  }

  if (!rb_easy_nil("debug_proc")) {
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, (curl_debug_callback)&proc_debug_handler);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, rb_easy_get("debug_proc"));
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
  } else {
    // have to remove handler to re-enable standard verbosity
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, rbce->verbose);
  }

  /* general opts */

  curl_easy_setopt(curl, CURLOPT_HEADER, rbce->header_in_body);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, rbce->follow_location);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, rbce->max_redirs);

  curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, rbce->proxy_tunnel);
  curl_easy_setopt(curl, CURLOPT_FILETIME, rbce->fetch_file_time);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, rbce->ssl_verify_peer);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, rbce->ssl_verify_host);

  if ((rbce->use_netrc != Qnil) && (rbce->use_netrc != Qfalse)) {
    curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
  } else {
    curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_IGNORED);
  }

  curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, rbce->unrestricted_auth);

  curl_easy_setopt(curl, CURLOPT_TIMEOUT, rbce->timeout);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, rbce->connect_timeout);
  curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, rbce->dns_cache_timeout);

  curl_easy_setopt(curl, CURLOPT_IGNORE_CONTENT_LENGTH, rbce->ignore_content_length);

  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, rbce->resolve_mode);


#if LIBCURL_VERSION_NUM >= 0x070a08
  curl_easy_setopt(curl, CURLOPT_FTP_RESPONSE_TIMEOUT, rbce->ftp_response_timeout);
#else
  if (rbce->ftp_response_timeout > 0) {
    rb_warn("Installed libcurl is too old to support ftp_response_timeout");
  }
#endif

  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, rbce->low_speed_limit);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, rbce->low_speed_time);

  // Set up localport / proxy port
  // FIXME these won't get returned to default if they're unset Ruby
  if (rbce->proxy_port > 0) {
    curl_easy_setopt(curl, CURLOPT_PROXYPORT, rbce->proxy_port);
  }

  if (rbce->local_port > 0) {
#if LIBCURL_VERSION_NUM >= 0x070f02
    curl_easy_setopt(curl, CURLOPT_LOCALPORT, rbce->local_port);

    if (rbce->local_port_range > 0) {
      curl_easy_setopt(curl, CURLOPT_LOCALPORTRANGE, rbce->local_port_range);
    }
#else
    rb_warn("Installed libcurl is too old to support local_port");
#endif
  }

  if (rbce->proxy_type != -1) {
#if LIBCURL_VERSION_NUM >= 0x070a00
    if (rbce->proxy_type == -2) {
      rb_warn("Installed libcurl is too old to support the selected proxy type");
    } else {
      curl_easy_setopt(curl, CURLOPT_PROXYTYPE, rbce->proxy_type);
    }
  } else {
    curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
#else
    rb_warn("Installed libcurl is too old to support proxy_type");
#endif
  }

  if (rbce->http_auth_types > 0) {
#if LIBCURL_VERSION_NUM >= 0x070a06
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, rbce->http_auth_types);
  } else {
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
#else
    rb_warn("Installed libcurl is too old to support http_auth_types");
#endif
  }

  if (rbce->proxy_auth_types > 0) {
#if LIBCURL_VERSION_NUM >= 0x070a07
    curl_easy_setopt(curl, CURLOPT_PROXYAUTH, rbce->proxy_auth_types);
  } else {
    curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
#else
    rb_warn("Installed libcurl is too old to support proxy_auth_types");
#endif
  }

  /* Set up HTTP cookie handling if necessary
     FIXME this may not get disabled if it's enabled, the disabled again from ruby.
     */
  if (rbce->enable_cookies) {
    if (!rb_easy_nil("cookiejar")) {
      curl_easy_setopt(curl, CURLOPT_COOKIEJAR, rb_easy_get_str("cookiejar"));
    }

    if (!rb_easy_nil("cookiefile")) {
      curl_easy_setopt(curl, CURLOPT_COOKIEFILE, rb_easy_get_str("cookiefile"));
    } else {
      curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); /* "" = magic to just enable */
    }
  }

  if (!rb_easy_nil("cookies")) {
    curl_easy_setopt(curl, CURLOPT_COOKIE, rb_easy_get_str("cookies"));
  }

  /* Set up HTTPS cert handling if necessary */
  if (!rb_easy_nil("cert")) {
    if (!rb_easy_nil("certtype")) {
      curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, rb_easy_get_str("certtype"));
    }
    curl_easy_setopt(curl, CURLOPT_SSLCERT, rb_easy_get_str("cert"));
    if (!rb_easy_nil("certpassword")) {
      curl_easy_setopt(curl, CURLOPT_SSLCERTPASSWD, rb_easy_get_str("certpassword"));
    }
    if (!rb_easy_nil("cert_key")) {
      curl_easy_setopt(curl, CURLOPT_SSLKEY, rb_easy_get_str("cert_key"));
    }
  }

  if (!rb_easy_nil("cacert")) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, rb_easy_get_str("cacert"));
  }
#ifdef HAVE_CURL_CONFIG_CA
  else {
    curl_easy_setopt(curl, CURLOPT_CAINFO, CURL_CONFIG_CA);
  }
#endif

#ifdef CURL_VERSION_SSL
  if (rbce->ssl_version > 0) {
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, rbce->ssl_version);
  }

  if (rbce->use_ssl > 0) {
    curl_easy_setopt(curl, CURB_FTPSSL, rbce->use_ssl);
  }
#else
  if (rbce->ssl_version > 0 || rbce->use_ssl > 0) {
    rb_warn("libcurl is not configured with SSL support");
  }
#endif
  
  if (rbce->ftp_filemethod > 0) {
    curl_easy_setopt(curl, CURLOPT_FTP_FILEMETHOD, rbce->ftp_filemethod);
  }

  /* Set the user-agent string if specified */
  if (!rb_easy_nil("useragent")) {
    curl_easy_setopt(curl, CURLOPT_USERAGENT, rb_easy_get_str("useragent"));
  }

  /* Setup HTTP headers if necessary */
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);   // XXX: maybe we shouldn't be clearing this?

  if (!rb_easy_nil("headers")) {
    if (rb_easy_type_check("headers", T_ARRAY) || rb_easy_type_check("headers", T_HASH)) {
      VALUE wrap = Data_Wrap_Struct(rb_cObject, 0, 0, hdrs);
      rb_iterate(rb_each, rb_easy_get("headers"), cb_each_http_header, wrap);
    } else {
      VALUE headers_str = rb_obj_as_string(rb_easy_get("headers"));
      *hdrs = curl_slist_append(*hdrs, StringValuePtr(headers_str));
    }

    if (*hdrs) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *hdrs);
    }
  }

  /* Setup FTP commands if necessary */
  if (!rb_easy_nil("ftp_commands")) {
    if (rb_easy_type_check("ftp_commands", T_ARRAY)) {
      VALUE wrap = Data_Wrap_Struct(rb_cObject, 0, 0, cmds);
      rb_iterate(rb_each, rb_easy_get("ftp_commands"), cb_each_ftp_command, wrap);
    }

    if (*cmds) {
      curl_easy_setopt(curl, CURLOPT_QUOTE, *cmds);
    }
  }

  return Qnil;
}
/***********************************************
 *
 * Clean up a connection
 *
 * Always returns Qtrue.
 */
VALUE ruby_curl_easy_cleanup( VALUE self, ruby_curl_easy *rbce ) {

  CURL *curl = rbce->curl;
  struct curl_slist *ftp_commands;

  /* Free everything up */
  if (rbce->curl_headers) {
    curl_slist_free_all(rbce->curl_headers);
    rbce->curl_headers = NULL;
  }

  ftp_commands = rbce->curl_ftp_commands;
  if (ftp_commands) {
    curl_slist_free_all(ftp_commands);
    rbce->curl_ftp_commands = NULL;
  }

  /* clean up a PUT request's curl options. */
  if (!rb_easy_nil("upload")) {
    rb_easy_del("upload"); // set the upload object to Qnil to let the GC clean up
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 0);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_READDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, 0);
  }

  return Qnil;
}

/*
 * Common implementation of easy.http(verb) and easy.http_delete
 */
static VALUE ruby_curl_easy_perform_verb_str(VALUE self, const char *verb) {
  ruby_curl_easy *rbce;
  CURL *curl;
  VALUE retval;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl = rbce->curl;

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, verb);

  retval = rb_funcall(self, rb_intern("perform"), 0);

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);

  return retval;
}

/*
 * call-seq:
 *   easy.http(verb)
 *
 * Send an HTTP request with method set to verb, using the current options set for this Curl::Easy instance.
 * This method always returns true or raises an exception (defined under Curl::Err) on error.
 */
static VALUE ruby_curl_easy_perform_verb(VALUE self, VALUE verb) {
  VALUE str_verb;
  if (rb_type(verb) == T_STRING) {
    return ruby_curl_easy_perform_verb_str(self, StringValueCStr(verb));
  }
  else if (rb_respond_to(verb,rb_intern("to_s"))) {
    str_verb = rb_funcall(verb, rb_intern("to_s"), 0);
    return ruby_curl_easy_perform_verb_str(self, StringValueCStr(str_verb));
  }
  else {
    rb_raise(rb_eRuntimeError, "Invalid HTTP VERB, must response to 'to_s'");
  }
}

/*
 * call-seq:
 *   easy.http_post("url=encoded%20form%20data;and=so%20on") => true
 *   easy.http_post("url=encoded%20form%20data", "and=so%20on", ...) => true
 *   easy.http_post("url=encoded%20form%20data", Curl::PostField, "and=so%20on", ...) => true
 *   easy.http_post(Curl::PostField, Curl::PostField ..., Curl::PostField) => true
 *
 * POST the specified formdata to the currently configured URL using
 * the current options set for this Curl::Easy instance. This method
 * always returns true, or raises an exception (defined under
 * Curl::Err) on error.
 *
 * The Content-type of the POST is determined by the current setting
 * of multipart_form_post? , according to the following rules:
 * * When false (the default): the form will be POSTed with a
 *   content-type of 'application/x-www-form-urlencoded', and any of the
 *   four calling forms may be used.
 * * When true: the form will be POSTed with a content-type of
 *   'multipart/formdata'. Only the last calling form may be used,
 *   i.e. only PostField instances may be POSTed. In this mode,
 *   individual fields' content-types are recognised, and file upload
 *   fields are supported.
 *
 */
static VALUE ruby_curl_easy_perform_post(int argc, VALUE *argv, VALUE self) {
  ruby_curl_easy *rbce;
  CURL *curl;
  int i;
  VALUE args_ary;

  rb_scan_args(argc, argv, "*", &args_ary);

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl = rbce->curl;

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);

  if (rbce->multipart_form_post) {
    VALUE ret;
    struct curl_httppost *first = NULL, *last = NULL;

    // Make the multipart form
    for (i = 0; i < argc; i++) {
      if (rb_obj_is_instance_of(argv[i], cCurlPostField)) {
        append_to_form(argv[i], &first, &last);
      } else if (rb_type(argv[i]) == T_ARRAY) {
        // see: https://github.com/rvanlieshout/curb/commit/8bcdefddc0162484681ebd1a92d52a642666a445
        int c = 0, argv_len = (int)RARRAY_LEN(argv[i]);
        for (; c < argv_len; ++c) {
          if (rb_obj_is_instance_of(rb_ary_entry(argv[i],c), cCurlPostField)) {
            append_to_form(rb_ary_entry(argv[i],c), &first, &last);
          } else {
            rb_raise(eCurlErrInvalidPostField, "You must use PostFields only with multipart form posts");
            return Qnil;
          }
        }
      } else {
        rb_raise(eCurlErrInvalidPostField, "You must use PostFields only with multipart form posts");
        return Qnil;
      }
    }

    curl_easy_setopt(curl, CURLOPT_POST, 0);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, first);
    ret = rb_funcall(self, rb_intern("perform"), 0);
    curl_formfree(first);

    return ret;
  } else {
    VALUE post_body = Qnil;
    /* TODO: check for PostField.file and raise error before to_s fails */
    if ((post_body = rb_funcall(args_ary, idJoin, 1, rbstrAmp)) == Qnil) {
      rb_raise(eCurlErrError, "Failed to join arguments");
      return Qnil;
    } else {
      /* if the function call above returns an empty string because no additional arguments were passed this makes sure
         a previously set easy.post_body = "arg=foo&bar=bin"  will be honored */
      if( post_body != Qnil && rb_type(post_body) == T_STRING && RSTRING_LEN(post_body) > 0 ) {
        ruby_curl_easy_post_body_set(self, post_body);
      }

      /* if post body is not defined, set it so we enable POST header, even though the request body is empty */
      if( rb_easy_nil("postdata_buffer") ) {
        ruby_curl_easy_post_body_set(self, post_body);
      }

      return rb_funcall(self, rb_intern("perform"), 0);
    }
  }
}

/*
 * call-seq:
 *   easy.http_put(data)                              => true
 *
 * PUT the supplied data to the currently configured URL using the
 * current options set for this Curl::Easy instance. This method always
 * returns true, or raises an exception (defined under Curl::Err) on error.
 */
static VALUE ruby_curl_easy_perform_put(VALUE self, VALUE data) {
  ruby_curl_easy *rbce;
  CURL *curl;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl = rbce->curl;

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
  ruby_curl_easy_put_data_set(self, data);

  return rb_funcall(self, rb_intern("perform"), 0);
}


/* =================== DATA FUNCS =============== */

/*
 * call-seq:
 *   easy.body_str                                    => "response body"
 *
 * Return the response body from the previous call to +perform+. This
 * is populated by the default +on_body+ handler - if you supply
 * your own body handler, this string will be empty.
 */
static VALUE ruby_curl_easy_body_str_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, body_data);
}

/*
 * call-seq:
 *   easy.header_str                                  => "response header"
 *
 * Return the response header from the previous call to +perform+. This
 * is populated by the default +on_header+ handler - if you supply
 * your own header handler, this string will be empty.
 */
static VALUE ruby_curl_easy_header_str_get(VALUE self) {
  CURB_OBJECT_HGETTER(ruby_curl_easy, header_data);
}


/* ============== LASTCONN INFO FUNCS ============ */

/*
 * call-seq:
 *   easy.last_effective_url                          => "http://some.url" or nil
 *
 * Retrieve the last effective URL used by this instance.
 * This is the URL used in the last +perform+ call,
 * and may differ from the value of easy.url.
 */
static VALUE ruby_curl_easy_last_effective_url_get(VALUE self) {
  ruby_curl_easy *rbce;
  char* url;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_EFFECTIVE_URL, &url);

  if (url && url[0]) {    // curl returns empty string if none
    return rb_str_new2(url);
  } else {
    return Qnil;
  }
}

/*
 * call-seq:
 *   easy.response_code                               => fixnum
 *
 * Retrieve the last received HTTP or FTP code. This will be zero
 * if no server response code has been received. Note that a proxy's
 * CONNECT response should be read with +http_connect_code+
 * and not this method.
 */
static VALUE ruby_curl_easy_response_code_get(VALUE self) {
  ruby_curl_easy *rbce;
  long code;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
#ifdef HAVE_CURLINFO_RESPONSE_CODE
  curl_easy_getinfo(rbce->curl, CURLINFO_RESPONSE_CODE, &code);
#else
  // old libcurl
  curl_easy_getinfo(rbce->curl, CURLINFO_HTTP_CODE, &code);
#endif

  return LONG2NUM(code);
}

#if defined(HAVE_CURLINFO_PRIMARY_IP)
/*
 * call-seq:
 *   easy.primary_ip                                  => "xx.xx.xx.xx" or nil
 *
 *   Retrieve the resolved IP of the most recent connection
 *   done with this curl handle. This string may be  IPv6 if
 *   that's enabled. This feature requires curl 7.19.x and above
 */
static VALUE ruby_curl_easy_primary_ip_get(VALUE self) {
  ruby_curl_easy *rbce;
  char* ip;
  
  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_PRIMARY_IP, &ip);

  if (ip && ip[0]) {    // curl returns empty string if none
    return rb_str_new2(ip);
  } else {
    return Qnil;
  }
}
#endif

/*
 * call-seq:
 *   easy.http_connect_code                           => fixnum
 *
 * Retrieve the last received proxy response code to a CONNECT request.
 */
static VALUE ruby_curl_easy_http_connect_code_get(VALUE self) {
  ruby_curl_easy *rbce;
  long code;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_HTTP_CONNECTCODE, &code);

  return LONG2NUM(code);
}

/*
 * call-seq:
 *   easy.file_time                                   => fixnum
 *
 * Retrieve the remote time of the retrieved document (in number of
 * seconds since 1 jan 1970 in the GMT/UTC time zone). If you get -1,
 * it can be because of many reasons (unknown, the server hides it
 * or the server doesn't support the command that tells document time
 * etc) and the time of the document is unknown.
 *
 * Note that you must tell the server to collect this information
 * before the transfer is made, by setting +fetch_file_time?+ to true,
 * or you will unconditionally get a -1 back.
 *
 * This requires libcurl 7.5 or higher - otherwise -1 is unconditionally
 * returned.
 */
static VALUE ruby_curl_easy_file_time_get(VALUE self) {
#ifdef HAVE_CURLINFO_FILETIME
  ruby_curl_easy *rbce;
  long time;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_FILETIME, &time);

  return LONG2NUM(time);
#else
  rb_warn("Installed libcurl is too old to support file_time");
  return INT2FIX(0);
#endif
}

/*
 * call-seq:
 *   easy.total_time                                  => float
 *
 * Retrieve the total time in seconds for the previous transfer,
 * including name resolving, TCP connect etc.
 */
static VALUE ruby_curl_easy_total_time_get(VALUE self) {
  ruby_curl_easy *rbce;
  double time;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_TOTAL_TIME, &time);

  return rb_float_new(time);
}

/*
 * call-seq:
 *   easy.name_lookup_time                            => float
 *
 * Retrieve the time, in seconds, it took from the start until the
 * name resolving was completed.
 */
static VALUE ruby_curl_easy_name_lookup_time_get(VALUE self) {
  ruby_curl_easy *rbce;
  double time;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_NAMELOOKUP_TIME, &time);

  return rb_float_new(time);
}

/*
 * call-seq:
 *   easy.connect_time                                => float
 *
 * Retrieve the time, in seconds, it took from the start until the
 * connect to the remote host (or proxy) was completed.
 */
static VALUE ruby_curl_easy_connect_time_get(VALUE self) {
  ruby_curl_easy *rbce;
  double time;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_CONNECT_TIME, &time);

  return rb_float_new(time);
}

/*
 * call-seq:
 *   easy.app_connect_time                         => float
 *
 * Retrieve the time, in seconds, it took from the start until the SSL/SSH
 * connect/handshake to the remote host was completed. This time is most often
 * very near to the pre transfer time, except for cases such as HTTP
 * pippelining where the pretransfer time can be delayed due to waits in line
 * for the pipeline and more.
 */
#if defined(HAVE_CURLINFO_APPCONNECT_TIME)
static VALUE ruby_curl_easy_app_connect_time_get(VALUE self) {
  ruby_curl_easy *rbce;
  double time;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_APPCONNECT_TIME, &time);

  return rb_float_new(time);
}
#endif


/*
 * call-seq:
 *   easy.pre_transfer_time                           => float
 *
 * Retrieve the time, in seconds, it took from the start until the
 * file transfer is just about to begin. This includes all pre-transfer
 * commands and negotiations that are specific to the particular protocol(s)
 * involved.
 */
static VALUE ruby_curl_easy_pre_transfer_time_get(VALUE self) {
  ruby_curl_easy *rbce;
  double time;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_PRETRANSFER_TIME, &time);

  return rb_float_new(time);
}

/*
 * call-seq:
 *   easy.start_transfer_time                         => float
 *
 * Retrieve the time, in seconds, it took from the start until the first byte
 * is just about to be transferred. This includes the +pre_transfer_time+ and
 * also the time the server needs to calculate the result.
 */
static VALUE ruby_curl_easy_start_transfer_time_get(VALUE self) {
  ruby_curl_easy *rbce;
  double time;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_STARTTRANSFER_TIME, &time);

  return rb_float_new(time);
}

/*
 * call-seq:
 *   easy.redirect_time                               => float
 *
 * Retrieve the total time, in seconds, it took for all redirection steps
 * include name lookup, connect, pretransfer and transfer before final
 * transaction was started. +redirect_time+ contains the complete
 * execution time for multiple redirections.
 *
 * Requires libcurl 7.9.7 or higher, otherwise -1 is always returned.
 */
static VALUE ruby_curl_easy_redirect_time_get(VALUE self) {
#ifdef HAVE_CURLINFO_REDIRECT_TIME
  ruby_curl_easy *rbce;
  double time;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_REDIRECT_TIME, &time);

  return rb_float_new(time);
#else
  rb_warn("Installed libcurl is too old to support redirect_time");
  return rb_float_new(-1);
#endif
}

/*
 * call-seq:
 *   easy.redirect_count                            => integer
 *
 * Retrieve the total number of redirections that were actually followed.
 *
 * Requires libcurl 7.9.7 or higher, otherwise -1 is always returned.
 */
static VALUE ruby_curl_easy_redirect_count_get(VALUE self) {
#ifdef HAVE_CURLINFO_REDIRECT_COUNT
  ruby_curl_easy *rbce;
  long count;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_REDIRECT_COUNT, &count);

  return LONG2NUM(count);
#else
  rb_warn("Installed libcurl is too old to support redirect_count");
  return INT2FIX(-1);
#endif

}

/*
 * call-seq:
 *   easy.redirect_url                               => "http://some.url" or nil
 *
 * Retrieve  the URL a redirect would take you to if you 
 * would enable CURLOPT_FOLLOWLOCATION.
 *
 * Requires libcurl 7.18.2 or higher, otherwise -1 is always returned.
 */
static VALUE ruby_curl_easy_redirect_url_get(VALUE self) {
#ifdef HAVE_CURLINFO_REDIRECT_URL
  ruby_curl_easy *rbce;
  char* url;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_REDIRECT_URL, &url);

  if (url && url[0]) {    // curl returns empty string if none
    return rb_str_new2(url);
  } else {
    return Qnil;
  }
#else
  rb_warn("Installed libcurl is too old to support redirect_url");
  return INT2FIX(-1);
#endif
}



/*
 * call-seq:
 *   easy.uploaded_bytes                              => float
 *
 * Retrieve the total amount of bytes that were uploaded in the
 * preceeding transfer.
 */
static VALUE ruby_curl_easy_uploaded_bytes_get(VALUE self) {
  ruby_curl_easy *rbce;
  double bytes;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_SIZE_UPLOAD, &bytes);

  return rb_float_new(bytes);
}

/*
 * call-seq:
 *   easy.downloaded_bytes                            => float
 *
 * Retrieve the total amount of bytes that were downloaded in the
 * preceeding transfer.
 */
static VALUE ruby_curl_easy_downloaded_bytes_get(VALUE self) {
  ruby_curl_easy *rbce;
  double bytes;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_SIZE_DOWNLOAD, &bytes);

  return rb_float_new(bytes);
}

/*
 * call-seq:
 *   easy.upload_speed                                => float
 *
 * Retrieve the average upload speed that curl measured for the
 * preceeding complete upload.
 */
static VALUE ruby_curl_easy_upload_speed_get(VALUE self) {
  ruby_curl_easy *rbce;
  double bytes;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_SPEED_UPLOAD, &bytes);

  return rb_float_new(bytes);
}

/*
 * call-seq:
 *   easy.download_speed                              => float
 *
 * Retrieve the average download speed that curl measured for
 * the preceeding complete download.
 */
static VALUE ruby_curl_easy_download_speed_get(VALUE self) {
  ruby_curl_easy *rbce;
  double bytes;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_SPEED_DOWNLOAD, &bytes);

  return rb_float_new(bytes);
}

/*
 * call-seq:
 *   easy.header_size                                 => fixnum
 *
 * Retrieve the total size of all the headers received in the
 * preceeding transfer.
 */
static VALUE ruby_curl_easy_header_size_get(VALUE self) {
  ruby_curl_easy *rbce;
  long size;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_HEADER_SIZE, &size);

  return LONG2NUM(size);
}

/*
 * call-seq:
 *   easy.request_size                                => fixnum
 *
 * Retrieve the total size of the issued requests. This is so far
 * only for HTTP requests. Note that this may be more than one request
 * if +follow_location?+ is true.
 */
static VALUE ruby_curl_easy_request_size_get(VALUE self) {
  ruby_curl_easy *rbce;
  long size;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_REQUEST_SIZE, &size);

  return LONG2NUM(size);
}

/*
 * call-seq:
 *   easy.ssl_verify_result                           => integer
 *
 * Retrieve the result of the certification verification that was requested
 * (by setting +ssl_verify_peer?+ to +true+).
 */
static VALUE ruby_curl_easy_ssl_verify_result_get(VALUE self) {
  ruby_curl_easy *rbce;
  long result;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_SSL_VERIFYRESULT, &result);

  return LONG2NUM(result);
}

/* TODO CURLINFO_SSL_ENGINES

Pass the address of a 'struct curl_slist *' to receive a linked-list of OpenSSL crypto-engines supported.
Note that engines are normally implemented in separate dynamic libraries.
Hence not all the returned engines may be available at run-time.
NOTE: you must call curl_slist_free_all(3) on the list pointer once you're done with it, as libcurl will not free the data for you. (Added in 7.12.3)
*/

/*
 * call-seq:
 *   easy.downloaded_content_length                   => float
 *
 * Retrieve the content-length of the download. This is the value read
 * from the Content-Length: field.
 */
static VALUE ruby_curl_easy_downloaded_content_length_get(VALUE self) {
  ruby_curl_easy *rbce;
  double bytes;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &bytes);

  return rb_float_new(bytes);
}

/*
 * call-seq:
 *   easy.uploaded_content_length                     => float
 *
 * Retrieve the content-length of the upload.
 */
static VALUE ruby_curl_easy_uploaded_content_length_get(VALUE self) {
  ruby_curl_easy *rbce;
  double bytes;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_CONTENT_LENGTH_UPLOAD, &bytes);

  return rb_float_new(bytes);
}

/*
 * call-seq:
 *   easy.content_type                                => "content/type" or nil
 *
 * Retrieve the content-type of the downloaded object. This is the value read
 * from the Content-Type: field. If you get +nil+, it means that the server
 * didn't send a valid Content-Type header or that the protocol used doesn't
 * support this.
 */
static VALUE ruby_curl_easy_content_type_get(VALUE self) {
  ruby_curl_easy *rbce;
  char* type;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_CONTENT_TYPE, &type);

  if (type && type[0]) {    // curl returns empty string if none
    return rb_str_new2(type);
  } else {
    return Qnil;
  }
}


/* NOT REQUIRED?
CURLINFO_PRIVATE

Pass a pointer to a 'char *' to receive the pointer to the private data associated with the curl handle (set with the CURLOPT_PRIVATE option to curl_easy_setopt(3)). (Added in 7.10.3)
*/

/* TODO these will need constants setting up too for checking the bits.
 *
 * Alternatively, could return an object that wraps the long, and has
 * question methods to query the auth types. Could return long from to_i(nt)
 *
CURLINFO_HTTPAUTH_AVAIL

Pass a pointer to a long to receive a bitmask indicating the authentication method(s) available. The meaning of the bits is explained in the CURLOPT_HTTPAUTH option for curl_easy_setopt(3). (Added in 7.10.8)

CURLINFO_PROXYAUTH_AVAIL

Pass a pointer to a long to receive a bitmask indicating the authentication method(s) available for your proxy authentication. (Added in 7.10.8)
*/

/*
 * call-seq:
 *   easy.os_errno                                    => integer
 *
 * Retrieve the errno variable from a connect failure (requires
 * libcurl 7.12.2 or higher, otherwise 0 is always returned).
 */
static VALUE ruby_curl_easy_os_errno_get(VALUE self) {
#ifdef HAVE_CURLINFO_OS_ERRNO
  ruby_curl_easy *rbce;
  long result;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_OS_ERRNO, &result);

  return LONG2NUM(result);
#else
  rb_warn("Installed libcurl is too old to support os_errno");
  return INT2FIX(0);
#endif
}

/*
 * call-seq:
 *   easy.num_connects                                => integer
 *
 * Retrieve the number of new connections libcurl had to create to achieve
 * the previous transfer (only the successful connects are counted).
 * Combined with +redirect_count+ you are able to know how many times libcurl
 * successfully reused existing connection(s) or not.
 *
 * See the Connection Options of curl_easy_setopt(3) to see how libcurl tries
 * to make persistent connections to save time.
 *
 * (requires libcurl 7.12.3 or higher, otherwise -1 is always returned).
 */
static VALUE ruby_curl_easy_num_connects_get(VALUE self) {
#ifdef HAVE_CURLINFO_NUM_CONNECTS
  ruby_curl_easy *rbce;
  long result;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_NUM_CONNECTS, &result);

  return LONG2NUM(result);
#else
  rb_warn("Installed libcurl is too old to support num_connects");
  return INT2FIX(-1);
#endif
}


/* TODO this needs to be implemented.

CURLINFO_COOKIELIST

Pass a pointer to a 'struct curl_slist *' to receive a linked-list of all cookies cURL knows (expired ones, too). Don't forget to curl_slist_free_all(3) the list after it has been used. If there are no cookies (cookies for the handle have not been enabled or simply none have been received) 'struct curl_slist *' will be set to point to NULL. (Added in 7.14.1)
*/

/* TODO this needs to be implemented. Could probably support CONNECT_ONLY by having this
 *      return an open Socket or something.
 *
CURLINFO_LASTSOCKET

Pass a pointer to a long to receive the last socket used by this curl session. If the socket is no longer valid, -1 is returned. When you finish working with the socket, you must call curl_easy_cleanup() as usual and let libcurl close the socket and cleanup other resources associated with the handle. This is typically used in combination with CURLOPT_CONNECT_ONLY. (Added in 7.15.2)
*/

/*
 * call-seq:
 *   easy.ftp_entry_path                                => "C:\ftp\root\" or nil
 *
 * Retrieve the path of the entry path. That is the initial path libcurl ended
 * up in when logging on to the remote FTP server. This returns +nil+ if
 * something is wrong.
 *
 * (requires libcurl 7.15.4 or higher, otherwise +nil+ is always returned).
 */
static VALUE ruby_curl_easy_ftp_entry_path_get(VALUE self) {
#ifdef HAVE_CURLINFO_FTP_ENTRY_PATH
  ruby_curl_easy *rbce;
  char* path = NULL;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl_easy_getinfo(rbce->curl, CURLINFO_FTP_ENTRY_PATH, &path);

  if (path && path[0]) {    // curl returns NULL or empty string if none
    return rb_str_new2(path);
  } else {
    return Qnil;
  }
#else
  rb_warn("Installed libcurl is too old to support num_connects");
  return Qnil;
#endif
}

/*
 * call-seq:
 *   easy.multi                                     => "#<Curl::Multi>"
 */
static VALUE ruby_curl_easy_multi_get(VALUE self) {
  ruby_curl_easy *rbce;
  Data_Get_Struct(self, ruby_curl_easy, rbce);
  return rbce->multi;
}

/*
 * call-seq:
 *   easy.multi=multi                                    => "#<Curl::Multi>"
 */
static VALUE ruby_curl_easy_multi_set(VALUE self, VALUE multi) {
  ruby_curl_easy *rbce;
  Data_Get_Struct(self, ruby_curl_easy, rbce);
  rbce->multi = multi;
  return rbce->multi;
}

/*
 * call-seq:
 *   easy.last_result                                    => 0
 */
static VALUE ruby_curl_easy_last_result(VALUE self) {
  ruby_curl_easy *rbce;
  Data_Get_Struct(self, ruby_curl_easy, rbce);
  return INT2FIX(rbce->last_result);
}

/*
 * call-seq:
 *   easy.setopt Fixnum, value  => value
 *
 * Iniital access to libcurl curl_easy_setopt
 */
static VALUE ruby_curl_easy_set_opt(VALUE self, VALUE opt, VALUE val) {
  ruby_curl_easy *rbce;
  long option = FIX2LONG(opt);

  Data_Get_Struct(self, ruby_curl_easy, rbce);

  switch (option) {
  /* BEHAVIOR OPTIONS */
  case CURLOPT_VERBOSE: {
    VALUE verbose = val;
    CURB_BOOLEAN_SETTER(ruby_curl_easy, verbose);
    } break;
  case CURLOPT_FOLLOWLOCATION: {
    VALUE follow_location = val;
    CURB_BOOLEAN_SETTER(ruby_curl_easy, follow_location);
    } break;
  /* TODO: CALLBACK OPTIONS */
  /* TODO: ERROR OPTIONS */
  /* NETWORK OPTIONS */
  case CURLOPT_URL: {
    VALUE url = val;
    CURB_OBJECT_HSETTER(ruby_curl_easy, url);
    } break;
  case CURLOPT_CUSTOMREQUEST:
    curl_easy_setopt(rbce->curl, CURLOPT_CUSTOMREQUEST, NIL_P(val) ? NULL : StringValueCStr(val));
    break;
  case CURLOPT_HTTP_VERSION:
    curl_easy_setopt(rbce->curl, CURLOPT_HTTP_VERSION, FIX2INT(val));
    break;
  case CURLOPT_PROXY: {
    VALUE proxy_url = val;
    CURB_OBJECT_HSETTER(ruby_curl_easy, proxy_url);
    } break;
  case CURLOPT_INTERFACE: {
    VALUE interface_hm = val;
    CURB_OBJECT_HSETTER(ruby_curl_easy, interface_hm);
    } break;
  case CURLOPT_HEADER:
  case CURLOPT_NOPROGRESS:
  case CURLOPT_NOSIGNAL:
  case CURLOPT_HTTPGET:
    break;
  case CURLOPT_POST: {
    curl_easy_setopt(rbce->curl, CURLOPT_POST, rb_type(val) == T_TRUE);
  } break;
  case CURLOPT_POSTFIELDS: {
    curl_easy_setopt(rbce->curl, CURLOPT_POSTFIELDS, NIL_P(val) ? NULL : StringValueCStr(val));
  } break;
  case CURLOPT_NOBODY: {
    int type = rb_type(val);
    VALUE value;
    if (type == T_TRUE) {
      value = rb_int_new(1);
    } else if (type == T_FALSE) {
      value = rb_int_new(0);
    } else {
      value = rb_funcall(val, rb_intern("to_i"), 0);
    }
    curl_easy_setopt(rbce->curl, option, FIX2INT(value));
    } break;
  case CURLOPT_USERPWD: {
    VALUE userpwd = val;
    CURB_OBJECT_HSETTER(ruby_curl_easy, userpwd);
    } break;
  case CURLOPT_PROXYUSERPWD: {
    VALUE proxypwd = val;
    CURB_OBJECT_HSETTER(ruby_curl_easy, proxypwd);
    } break;
  case CURLOPT_COOKIE: {
    VALUE cookies = val;
    CURB_OBJECT_HSETTER(ruby_curl_easy, cookies);
    } break;
  case CURLOPT_COOKIEFILE: {
    VALUE cookiefile = val;
    CURB_OBJECT_HSETTER(ruby_curl_easy, cookiefile);
    } break;
  case CURLOPT_COOKIEJAR: {
    VALUE cookiejar = val;
    CURB_OBJECT_HSETTER(ruby_curl_easy, cookiejar);
    } break;
  case CURLOPT_TCP_NODELAY: {
    curl_easy_setopt(rbce->curl, CURLOPT_TCP_NODELAY, FIX2LONG(val));
    } break;
  case CURLOPT_RESUME_FROM: {
    curl_easy_setopt(rbce->curl, CURLOPT_RESUME_FROM, FIX2LONG(val));
    } break;
  case CURLOPT_FAILONERROR: {
    curl_easy_setopt(rbce->curl, CURLOPT_FAILONERROR, FIX2LONG(val));
    } break;
  case CURLOPT_SSL_CIPHER_LIST: {
    curl_easy_setopt(rbce->curl, CURLOPT_SSL_CIPHER_LIST, StringValueCStr(val));
    } break;
#if HAVE_CURLOPT_GSSAPI_DELEGATION
  case CURLOPT_GSSAPI_DELEGATION: {
    curl_easy_setopt(rbce->curl, CURLOPT_GSSAPI_DELEGATION, FIX2LONG(val));
    } break;
#endif
  default:
    rb_raise(rb_eTypeError, "Curb unsupported option");
  }

  return val;
}

/*
 * call-seq:
 *   easy.getinfo Fixnum => value
 *
 * Iniital access to libcurl curl_easy_getinfo, remember getinfo doesn't return the same values as setopt
 */
static VALUE ruby_curl_easy_get_opt(VALUE self, VALUE opt) {
  return Qnil;
}

/*
 * call-seq:
 *   easy.inspect                                     => "#<Curl::Easy http://google.com/>"
 */
static VALUE ruby_curl_easy_inspect(VALUE self) {
  char buf[64];
  ruby_curl_easy *rbce;
  Data_Get_Struct(self, ruby_curl_easy, rbce);
  /* if we don't have a url set... we'll crash... */
  if( !rb_easy_nil("url") && rb_easy_type_check("url", T_STRING)) {
    VALUE url = rb_easy_get("url");
    size_t len = 13+((RSTRING_LEN(url) > 50) ? 50 : RSTRING_LEN(url));
    /* "#<Net::HTTP http://www.google.com/:80 open=false>" */
    memcpy(buf,"#<Curl::Easy ", 13);
    memcpy(buf+13,StringValueCStr(url), (len - 13));
    buf[len++] = '>';
    return rb_str_new(buf,len);
  }
  return rb_str_new2("#<Curl::Easy>");
}


/* ================== ESCAPING FUNCS ==============*/

/*
 * call-seq:
 *   easy.escape("some text")                         => "some%20text"
 *
 * Convert the given input string to a URL encoded string and return
 * the result. All input characters that are not a-z, A-Z or 0-9 are
 * converted to their "URL escaped" version (%NN where NN is a
 * two-digit hexadecimal number).
 */
static VALUE ruby_curl_easy_escape(VALUE self, VALUE svalue) {
  ruby_curl_easy *rbce;
  char *result;
  VALUE rresult;
  VALUE str = svalue;

  Data_Get_Struct(self, ruby_curl_easy, rbce);

  /* NOTE: make sure the value is a string, if not call to_s */
  if( rb_type(str) != T_STRING ) { str = rb_funcall(str,rb_intern("to_s"),0); }

#if (LIBCURL_VERSION_NUM >= 0x070f04)
  result = (char*)curl_easy_escape(rbce->curl, StringValuePtr(str), (int)RSTRING_LEN(str));
#else
  result = (char*)curl_escape(StringValuePtr(str), (int)RSTRING_LEN(str));
#endif

  rresult = rb_str_new2(result);
  curl_free(result);

  return rresult;
}

/*
 * call-seq:
 *   easy.unescape("some%20text")                     => "some text"
 *
 * Convert the given URL encoded input string to a "plain string" and return
 * the result. All input characters that are URL encoded (%XX where XX is a
 * two-digit hexadecimal number) are converted to their binary versions.
 */
static VALUE ruby_curl_easy_unescape(VALUE self, VALUE str) {
  ruby_curl_easy *rbce;
  int rlen;
  char *result;
  VALUE rresult;

  Data_Get_Struct(self, ruby_curl_easy, rbce);

#if (LIBCURL_VERSION_NUM >= 0x070f04)
  result = (char*)curl_easy_unescape(rbce->curl, StringValuePtr(str), (int)RSTRING_LEN(str), &rlen);
#else
  result = (char*)curl_unescape(StringValuePtr(str), RSTRING_LEN(str));
  rlen = strlen(result);
#endif

  rresult = rb_str_new(result, rlen);
  curl_free(result);

  return rresult;
}


/* ================= CLASS METHODS ==================*/

/*
 * call-seq:
 *   Curl::Easy.error(code)    => [ErrCode, String]
 *
 * translate an internal libcurl error to ruby error class
 */
static VALUE ruby_curl_easy_error_message(VALUE klass, VALUE code) {
  return rb_curl_easy_error(FIX2INT(code));
}

/* =================== INIT LIB =====================*/
void init_curb_easy() {
  idCall = rb_intern("call");
  idJoin = rb_intern("join");

  rbstrAmp = rb_str_new2("&");
  rb_global_variable(&rbstrAmp);

  cCurlEasy = rb_define_class_under(mCurl, "Easy", rb_cObject);

  /* Class methods */
  rb_define_singleton_method(cCurlEasy, "new", ruby_curl_easy_new, -1);
  rb_define_singleton_method(cCurlEasy, "error", ruby_curl_easy_error_message, 1);

  /* Attributes for config next perform */
  rb_define_method(cCurlEasy, "url", ruby_curl_easy_url_get, 0);
  rb_define_method(cCurlEasy, "proxy_url", ruby_curl_easy_proxy_url_get, 0);
  rb_define_method(cCurlEasy, "headers=", ruby_curl_easy_headers_set, 1);
  rb_define_method(cCurlEasy, "headers", ruby_curl_easy_headers_get, 0);
  rb_define_method(cCurlEasy, "interface", ruby_curl_easy_interface_get, 0);
  rb_define_method(cCurlEasy, "userpwd", ruby_curl_easy_userpwd_get, 0);
  rb_define_method(cCurlEasy, "proxypwd", ruby_curl_easy_proxypwd_get, 0);
  rb_define_method(cCurlEasy, "cookies", ruby_curl_easy_cookies_get, 0);
  rb_define_method(cCurlEasy, "cookiefile", ruby_curl_easy_cookiefile_get, 0);
  rb_define_method(cCurlEasy, "cookiejar", ruby_curl_easy_cookiejar_get, 0);
  rb_define_method(cCurlEasy, "cert=", ruby_curl_easy_cert_set, 1);
  rb_define_method(cCurlEasy, "cert", ruby_curl_easy_cert_get, 0);
  rb_define_method(cCurlEasy, "cert_key=", ruby_curl_easy_cert_key_set, 1);
  rb_define_method(cCurlEasy, "cert_key", ruby_curl_easy_cert_key_get, 0);
  rb_define_method(cCurlEasy, "cacert=", ruby_curl_easy_cacert_set, 1);
  rb_define_method(cCurlEasy, "cacert", ruby_curl_easy_cacert_get, 0);
  rb_define_method(cCurlEasy, "certpassword=", ruby_curl_easy_certpassword_set, 1);
  rb_define_method(cCurlEasy, "certtype=", ruby_curl_easy_certtype_set, 1);
  rb_define_method(cCurlEasy, "certtype", ruby_curl_easy_certtype_get, 0);
  rb_define_method(cCurlEasy, "encoding=", ruby_curl_easy_encoding_set, 1);
  rb_define_method(cCurlEasy, "encoding", ruby_curl_easy_encoding_get, 0);
  rb_define_method(cCurlEasy, "useragent=", ruby_curl_easy_useragent_set, 1);
  rb_define_method(cCurlEasy, "useragent", ruby_curl_easy_useragent_get, 0);
  rb_define_method(cCurlEasy, "post_body=", ruby_curl_easy_post_body_set, 1);
  rb_define_method(cCurlEasy, "post_body", ruby_curl_easy_post_body_get, 0);
  rb_define_method(cCurlEasy, "put_data=", ruby_curl_easy_put_data_set, 1);
  rb_define_method(cCurlEasy, "ftp_commands=", ruby_curl_easy_ftp_commands_set, 1);
  rb_define_method(cCurlEasy, "ftp_commands", ruby_curl_easy_ftp_commands_get, 0);

  rb_define_method(cCurlEasy, "local_port=", ruby_curl_easy_local_port_set, 1);
  rb_define_method(cCurlEasy, "local_port", ruby_curl_easy_local_port_get, 0);
  rb_define_method(cCurlEasy, "local_port_range=", ruby_curl_easy_local_port_range_set, 1);
  rb_define_method(cCurlEasy, "local_port_range", ruby_curl_easy_local_port_range_get, 0);
  rb_define_method(cCurlEasy, "proxy_port=", ruby_curl_easy_proxy_port_set, 1);
  rb_define_method(cCurlEasy, "proxy_port", ruby_curl_easy_proxy_port_get, 0);
  rb_define_method(cCurlEasy, "proxy_type=", ruby_curl_easy_proxy_type_set, 1);
  rb_define_method(cCurlEasy, "proxy_type", ruby_curl_easy_proxy_type_get, 0);
  rb_define_method(cCurlEasy, "http_auth_types=", ruby_curl_easy_http_auth_types_set, -1);
  rb_define_method(cCurlEasy, "http_auth_types", ruby_curl_easy_http_auth_types_get, 0);
  rb_define_method(cCurlEasy, "proxy_auth_types=", ruby_curl_easy_proxy_auth_types_set, 1);
  rb_define_method(cCurlEasy, "proxy_auth_types", ruby_curl_easy_proxy_auth_types_get, 0);
  rb_define_method(cCurlEasy, "max_redirects=", ruby_curl_easy_max_redirects_set, 1);
  rb_define_method(cCurlEasy, "max_redirects", ruby_curl_easy_max_redirects_get, 0);
  rb_define_method(cCurlEasy, "timeout=", ruby_curl_easy_timeout_set, 1);
  rb_define_method(cCurlEasy, "timeout", ruby_curl_easy_timeout_get, 0);
  rb_define_method(cCurlEasy, "connect_timeout=", ruby_curl_easy_connect_timeout_set, 1);
  rb_define_method(cCurlEasy, "connect_timeout", ruby_curl_easy_connect_timeout_get, 0);
  rb_define_method(cCurlEasy, "dns_cache_timeout=", ruby_curl_easy_dns_cache_timeout_set, 1);
  rb_define_method(cCurlEasy, "dns_cache_timeout", ruby_curl_easy_dns_cache_timeout_get, 0);
  rb_define_method(cCurlEasy, "ftp_response_timeout=", ruby_curl_easy_ftp_response_timeout_set, 1);
  rb_define_method(cCurlEasy, "ftp_response_timeout", ruby_curl_easy_ftp_response_timeout_get, 0);
  rb_define_method(cCurlEasy, "low_speed_limit=", ruby_curl_easy_low_speed_limit_set, 1);
  rb_define_method(cCurlEasy, "low_speed_limit", ruby_curl_easy_low_speed_limit_get, 0);
  rb_define_method(cCurlEasy, "low_speed_time=", ruby_curl_easy_low_speed_time_set, 1);
  rb_define_method(cCurlEasy, "low_speed_time", ruby_curl_easy_low_speed_time_get, 0);
  rb_define_method(cCurlEasy, "ssl_version=", ruby_curl_easy_ssl_version_set, 1);
  rb_define_method(cCurlEasy, "ssl_version", ruby_curl_easy_ssl_version_get, 0);
  rb_define_method(cCurlEasy, "use_ssl=", ruby_curl_easy_use_ssl_set, 1);
  rb_define_method(cCurlEasy, "use_ssl", ruby_curl_easy_use_ssl_get, 0);
  rb_define_method(cCurlEasy, "ftp_filemethod=", ruby_curl_easy_ftp_filemethod_set, 1);
  rb_define_method(cCurlEasy, "ftp_filemethod", ruby_curl_easy_ftp_filemethod_get, 0);

  rb_define_method(cCurlEasy, "username=", ruby_curl_easy_username_set, 1);
  rb_define_method(cCurlEasy, "username", ruby_curl_easy_username_get, 0);
  rb_define_method(cCurlEasy, "password=", ruby_curl_easy_password_set, 1);
  rb_define_method(cCurlEasy, "password", ruby_curl_easy_password_get, 0);

  rb_define_method(cCurlEasy, "proxy_tunnel=", ruby_curl_easy_proxy_tunnel_set, 1);
  rb_define_method(cCurlEasy, "proxy_tunnel?", ruby_curl_easy_proxy_tunnel_q, 0);
  rb_define_method(cCurlEasy, "fetch_file_time=", ruby_curl_easy_fetch_file_time_set, 1);
  rb_define_method(cCurlEasy, "fetch_file_time?", ruby_curl_easy_fetch_file_time_q, 0);
  rb_define_method(cCurlEasy, "ssl_verify_peer=", ruby_curl_easy_ssl_verify_peer_set, 1);
  rb_define_method(cCurlEasy, "ssl_verify_peer?", ruby_curl_easy_ssl_verify_peer_q, 0);
  rb_define_method(cCurlEasy, "ssl_verify_host_integer=", ruby_curl_easy_ssl_verify_host_set, 1);
  rb_define_method(cCurlEasy, "ssl_verify_host", ruby_curl_easy_ssl_verify_host_get, 0);
  rb_define_method(cCurlEasy, "header_in_body=", ruby_curl_easy_header_in_body_set, 1);
  rb_define_method(cCurlEasy, "header_in_body?", ruby_curl_easy_header_in_body_q, 0);
  rb_define_method(cCurlEasy, "use_netrc=", ruby_curl_easy_use_netrc_set, 1);
  rb_define_method(cCurlEasy, "use_netrc?", ruby_curl_easy_use_netrc_q, 0);
  rb_define_method(cCurlEasy, "follow_location?", ruby_curl_easy_follow_location_q, 0);
  rb_define_method(cCurlEasy, "autoreferer=", ruby_curl_easy_autoreferer_set, 1);
  rb_define_method(cCurlEasy, "unrestricted_auth=", ruby_curl_easy_unrestricted_auth_set, 1);
  rb_define_method(cCurlEasy, "unrestricted_auth?", ruby_curl_easy_unrestricted_auth_q, 0);
  rb_define_method(cCurlEasy, "verbose=", ruby_curl_easy_verbose_set, 1);
  rb_define_method(cCurlEasy, "verbose?", ruby_curl_easy_verbose_q, 0);
  rb_define_method(cCurlEasy, "multipart_form_post=", ruby_curl_easy_multipart_form_post_set, 1);
  rb_define_method(cCurlEasy, "multipart_form_post?", ruby_curl_easy_multipart_form_post_q, 0);
  rb_define_method(cCurlEasy, "enable_cookies=", ruby_curl_easy_enable_cookies_set, 1);
  rb_define_method(cCurlEasy, "enable_cookies?", ruby_curl_easy_enable_cookies_q, 0);
  rb_define_method(cCurlEasy, "ignore_content_length=", ruby_curl_easy_ignore_content_length_set, 1);
  rb_define_method(cCurlEasy, "ignore_content_length?", ruby_curl_easy_ignore_content_length_q, 0);
  rb_define_method(cCurlEasy, "resolve_mode", ruby_curl_easy_resolve_mode, 0);
  rb_define_method(cCurlEasy, "resolve_mode=", ruby_curl_easy_resolve_mode_set, 1);

  rb_define_method(cCurlEasy, "on_body", ruby_curl_easy_on_body_set, -1);
  rb_define_method(cCurlEasy, "on_header", ruby_curl_easy_on_header_set, -1);
  rb_define_method(cCurlEasy, "on_progress", ruby_curl_easy_on_progress_set, -1);
  rb_define_method(cCurlEasy, "on_debug", ruby_curl_easy_on_debug_set, -1);
  rb_define_method(cCurlEasy, "on_success", ruby_curl_easy_on_success_set, -1);
  rb_define_method(cCurlEasy, "on_failure", ruby_curl_easy_on_failure_set, -1);
  rb_define_method(cCurlEasy, "on_missing", ruby_curl_easy_on_missing_set, -1);
  rb_define_method(cCurlEasy, "on_redirect", ruby_curl_easy_on_redirect_set, -1);
  rb_define_method(cCurlEasy, "on_complete", ruby_curl_easy_on_complete_set, -1);

  rb_define_method(cCurlEasy, "http", ruby_curl_easy_perform_verb, 1);
  rb_define_method(cCurlEasy, "http_post", ruby_curl_easy_perform_post, -1);
  rb_define_method(cCurlEasy, "http_put", ruby_curl_easy_perform_put, 1);

  /* Post-perform info methods */
  rb_define_method(cCurlEasy, "body_str", ruby_curl_easy_body_str_get, 0);
  rb_define_method(cCurlEasy, "header_str", ruby_curl_easy_header_str_get, 0);

  rb_define_method(cCurlEasy, "last_effective_url", ruby_curl_easy_last_effective_url_get, 0);
  rb_define_method(cCurlEasy, "response_code", ruby_curl_easy_response_code_get, 0);
#if defined(HAVE_CURLINFO_PRIMARY_IP)
  rb_define_method(cCurlEasy, "primary_ip", ruby_curl_easy_primary_ip_get, 0);
#endif
  rb_define_method(cCurlEasy, "http_connect_code", ruby_curl_easy_http_connect_code_get, 0);
  rb_define_method(cCurlEasy, "file_time", ruby_curl_easy_file_time_get, 0);
  rb_define_method(cCurlEasy, "total_time", ruby_curl_easy_total_time_get, 0);
  rb_define_method(cCurlEasy, "name_lookup_time", ruby_curl_easy_name_lookup_time_get, 0);
  rb_define_method(cCurlEasy, "connect_time", ruby_curl_easy_connect_time_get, 0);
#if defined(HAVE_CURLINFO_APPCONNECT_TIME)
  rb_define_method(cCurlEasy, "app_connect_time", ruby_curl_easy_app_connect_time_get, 0);
#endif
  rb_define_method(cCurlEasy, "pre_transfer_time", ruby_curl_easy_pre_transfer_time_get, 0);
  rb_define_method(cCurlEasy, "start_transfer_time", ruby_curl_easy_start_transfer_time_get, 0);
  rb_define_method(cCurlEasy, "redirect_time", ruby_curl_easy_redirect_time_get, 0);
  rb_define_method(cCurlEasy, "redirect_count", ruby_curl_easy_redirect_count_get, 0);
  rb_define_method(cCurlEasy, "redirect_url", ruby_curl_easy_redirect_url_get, 0);
  rb_define_method(cCurlEasy, "downloaded_bytes", ruby_curl_easy_downloaded_bytes_get, 0);
  rb_define_method(cCurlEasy, "uploaded_bytes", ruby_curl_easy_uploaded_bytes_get, 0);
  rb_define_method(cCurlEasy, "download_speed", ruby_curl_easy_download_speed_get, 0);
  rb_define_method(cCurlEasy, "upload_speed", ruby_curl_easy_upload_speed_get, 0);
  rb_define_method(cCurlEasy, "header_size", ruby_curl_easy_header_size_get, 0);
  rb_define_method(cCurlEasy, "request_size", ruby_curl_easy_request_size_get, 0);
  rb_define_method(cCurlEasy, "ssl_verify_result", ruby_curl_easy_ssl_verify_result_get, 0);
  rb_define_method(cCurlEasy, "downloaded_content_length", ruby_curl_easy_downloaded_content_length_get, 0);
  rb_define_method(cCurlEasy, "uploaded_content_length", ruby_curl_easy_uploaded_content_length_get, 0);
  rb_define_method(cCurlEasy, "content_type", ruby_curl_easy_content_type_get, 0);
  rb_define_method(cCurlEasy, "os_errno", ruby_curl_easy_os_errno_get, 0);
  rb_define_method(cCurlEasy, "num_connects", ruby_curl_easy_num_connects_get, 0);
  rb_define_method(cCurlEasy, "ftp_entry_path", ruby_curl_easy_ftp_entry_path_get, 0);

  rb_define_method(cCurlEasy, "close", ruby_curl_easy_close, 0);
  rb_define_method(cCurlEasy, "reset", ruby_curl_easy_reset, 0);

  /* Curl utils */
  rb_define_method(cCurlEasy, "escape", ruby_curl_easy_escape, 1);
  rb_define_method(cCurlEasy, "unescape", ruby_curl_easy_unescape, 1);

  /* Runtime support */
  rb_define_method(cCurlEasy, "clone", ruby_curl_easy_clone, 0);
  rb_define_alias(cCurlEasy, "dup", "clone");
  rb_define_method(cCurlEasy, "inspect", ruby_curl_easy_inspect, 0);

  rb_define_method(cCurlEasy, "multi", ruby_curl_easy_multi_get, 0);
  rb_define_method(cCurlEasy, "multi=", ruby_curl_easy_multi_set, 1);
  rb_define_method(cCurlEasy, "last_result", ruby_curl_easy_last_result, 0);

  rb_define_method(cCurlEasy, "setopt", ruby_curl_easy_set_opt, 2);
  rb_define_method(cCurlEasy, "getinfo", ruby_curl_easy_get_opt, 1);
}
