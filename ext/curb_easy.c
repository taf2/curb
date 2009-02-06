/* curb_easy.c - Curl easy mode
 * Copyright (c)2006 Ross Bamford. 
 * Licensed under the Ruby License. See LICENSE for details.
 * 
 * $Id: curb_easy.c 30 2006-12-09 12:30:24Z roscopeco $
 */
#include "curb_easy.h"
#include "curb_errors.h"
#include "curb_postfield.h"

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

/* These handle both body and header data */
static size_t default_data_handler(char *stream, 
                                   size_t size, 
                                   size_t nmemb, 
                                   VALUE out) {
  rb_str_buf_cat(out, stream, size * nmemb);
  return size * nmemb;
}

// size_t function( void *ptr, size_t size, size_t nmemb, void *stream);
static size_t read_data_handler(char *stream,
                                size_t size, 
                                size_t nmemb, 
                                char **buffer) {
    size_t result = 0;

    if (buffer != NULL && *buffer != NULL) {
        int len = size * nmemb;
        char *s1 = strncpy(stream, *buffer, len);
        result = strlen(s1);
        *buffer += result;
    }

    return result;
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

static int proc_progress_handler(VALUE proc,
                                 double dltotal,
                                 double dlnow,
                                 double ultotal,
                                 double ulnow) {
  VALUE procret;
  
  procret = rb_funcall(proc, idCall, 4, rb_float_new(dltotal), 
                                        rb_float_new(dlnow), 
                                        rb_float_new(ultotal),
                                        rb_float_new(ulnow));

  return(((procret == Qfalse) || (procret == Qnil)) ? -1 : 0);
}                                  

static int proc_debug_handler(CURL *curl, 
                              curl_infotype type,
                              char *data, 
                              size_t data_len, 
                              VALUE proc) {
  rb_funcall(proc, idCall, 2, INT2FIX(type), rb_str_new(data, data_len));
  return 0;  
}

/* ================== MARK/FREE FUNC ==================*/
void curl_easy_mark(ruby_curl_easy *rbce) {
  rb_gc_mark(rbce->url);
  rb_gc_mark(rbce->proxy_url);
  rb_gc_mark(rbce->body_proc);
  rb_gc_mark(rbce->body_data);
  rb_gc_mark(rbce->header_proc);
  rb_gc_mark(rbce->header_data);
  rb_gc_mark(rbce->progress_proc);
  rb_gc_mark(rbce->debug_proc);
  rb_gc_mark(rbce->interface);
  rb_gc_mark(rbce->userpwd);
  rb_gc_mark(rbce->proxypwd);  
  rb_gc_mark(rbce->headers);
  rb_gc_mark(rbce->cookiejar);
  rb_gc_mark(rbce->cert);
  rb_gc_mark(rbce->encoding);
  rb_gc_mark(rbce->success_proc);
  rb_gc_mark(rbce->failure_proc);
  rb_gc_mark(rbce->complete_proc);

  rb_gc_mark(rbce->postdata_buffer);
  rb_gc_mark(rbce->bodybuf);
  rb_gc_mark(rbce->headerbuf);
}

void curl_easy_free(ruby_curl_easy *rbce) {
  if (rbce->curl_headers) {
    curl_slist_free_all(rbce->curl_headers);
  }
  curl_easy_cleanup(rbce->curl);
  free(rbce);
}


/* ================= ALLOC METHODS ====================*/

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

  rb_scan_args(argc, argv, "01&", &url, &blk);    

  ruby_curl_easy *rbce = ALLOC(ruby_curl_easy);
  
  /* handler */  
  rbce->curl = curl_easy_init();  
  
  /* assoc objects */
  rbce->url = url;
  rbce->proxy_url = Qnil;
  rbce->body_data = Qnil;
  rbce->body_proc = Qnil;
  rbce->header_data = Qnil;
  rbce->header_proc = Qnil;
  rbce->progress_proc = Qnil;
  rbce->debug_proc = Qnil;
  rbce->interface = Qnil;
  rbce->userpwd = Qnil;
  rbce->proxypwd = Qnil;
  rbce->headers = rb_hash_new();
  rbce->cookiejar = Qnil;
  rbce->cert = Qnil;
  rbce->encoding = Qnil;
  rbce->success_proc = Qnil;
  rbce->failure_proc = Qnil;
  rbce->complete_proc = Qnil;
  
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

  /* bool opts */
  rbce->proxy_tunnel = 0;
  rbce->fetch_file_time = 0;
  rbce->ssl_verify_peer = 1;
  rbce->ssl_verify_host = 1;
  rbce->header_in_body = 0;
  rbce->use_netrc = 0;
  rbce->follow_location = 0;
  rbce->unrestricted_auth = 0;
  rbce->verbose = 0;
  rbce->multipart_form_post = 0;
  rbce->enable_cookies = 0;
  
  /* buffers */
  rbce->postdata_buffer = Qnil;
  rbce->bodybuf = Qnil;
  rbce->headerbuf = Qnil;
  rbce->curl_headers = NULL;
  
  new_curl = Data_Wrap_Struct(cCurlEasy, curl_easy_mark, curl_easy_free, rbce);
  
  if (blk != Qnil) {
    rb_funcall(blk, idCall, 1, new_curl);
  }

  /* set the rbce pointer to the curl handle */
  ecode = curl_easy_setopt(rbce->curl, CURLOPT_PRIVATE, (void*)rbce);
  if (ecode != CURLE_OK) {
    raise_curl_easy_error_exception(ecode);
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
  
  return Data_Wrap_Struct(cCurlEasy, curl_easy_mark, curl_easy_free, newrbce);
}


/* ================ OBJ ATTRIBUTES ==================*/

/*
 * call-seq:
 *   easy.url = "http://some.url/"                    => "http://some.url/"
 * 
 * Set the URL for subsequent calls to +perform+. It is acceptable
 * (and even recommended) to reuse Curl::Easy instances by reassigning
 * the URL between calls to +perform+.
 */
static VALUE ruby_curl_easy_url_set(VALUE self, VALUE url) {
  CURB_OBJECT_SETTER(ruby_curl_easy, url);
}

/*
 * call-seq:
 *   easy.url                                         => "http://some.url/"
 * 
 * Obtain the URL that will be used by subsequent calls to +perform+.
 */ 
static VALUE ruby_curl_easy_url_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, url);
}

/*
 * call-seq:
 *   easy.proxy_url = "some.url"                      => "some.url"
 * 
 * Set the URL of the HTTP proxy to use for subsequent calls to +perform+.
 * The URL should specify the the host name or dotted IP address. To specify 
 * port number in this string, append :[port] to the end of the host name. 
 * The proxy string may be prefixed with [protocol]:// since any such prefix
 * will be ignored. The proxy's port number may optionally be specified with 
 * the separate option proxy_port .
 * 
 * When you tell the library to use an HTTP proxy, libcurl will transparently 
 * convert operations to HTTP even if you specify an FTP URL etc. This may have
 * an impact on what other features of the library you can use, such as 
 * FTP specifics that don't work unless you tunnel through the HTTP proxy. Such 
 * tunneling is activated with proxy_tunnel = true.
 * 
 * libcurl respects the environment variables *http_proxy*, *ftp_proxy*, 
 * *all_proxy* etc, if any of those is set. The proxy_url option does however 
 * override any possibly set environment variables. 
 * 
 * Starting with libcurl 7.14.1, the proxy host string given in environment 
 * variables can be specified the exact same way as the proxy can be set with 
 * proxy_url, including protocol prefix (http://) and embedded user + password.
  */
static VALUE ruby_curl_easy_proxy_url_set(VALUE self, VALUE proxy_url) {
  CURB_OBJECT_SETTER(ruby_curl_easy, proxy_url);
}

/*
 * call-seq:
 *   easy.proxy_url                                   => "some.url"
 * 
 * Obtain the HTTP Proxy URL that will be used by subsequent calls to +perform+.
 */ 
static VALUE ruby_curl_easy_proxy_url_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, proxy_url);
}

/*
 * call-seq:
 *   easy.headers = "Header: val"                              => ["Header: val", ...]
 *   easy.headers = {"Header" => "val" ..., "Header" => "val"} => ["Header: val", ...]
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
 *    easy.headers["Expect:"] = ''
 * 
 * Anything passed to libcurl as a header will be converted to a string during
 * the perform step.
 */
static VALUE ruby_curl_easy_headers_set(VALUE self, VALUE headers) {
  CURB_OBJECT_SETTER(ruby_curl_easy, headers);
}

/*
 * call-seq:
 *   easy.headers                                              => Hash, Array or Str
 * 
 * Obtain the custom HTTP headers for following requests.
 */ 
static VALUE ruby_curl_easy_headers_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, headers);
}

/*
 * call-seq:
 *   easy.interface = "interface"                     => "interface"
 * 
 * Set the interface name to use as the outgoing network interface. 
 * The name can be an interface name, an IP address or a host name.
 */
static VALUE ruby_curl_easy_interface_set(VALUE self, VALUE interface) {
  CURB_OBJECT_SETTER(ruby_curl_easy, interface);
}

/*
 * call-seq:
 *   easy.interface                                   => "interface"
 * 
 * Obtain the interface name that is used as the outgoing network interface. 
 * The name can be an interface name, an IP address or a host name.
 */ 
static VALUE ruby_curl_easy_interface_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, interface);
}

/*
 * call-seq:
 *   easy.userpwd = "pwd string"                      => "pwd string"
 * 
 * Set the username/password string to use for subsequent calls to +perform+.
 * The supplied string should have the form "username:password"
 */
static VALUE ruby_curl_easy_userpwd_set(VALUE self, VALUE userpwd) {
  CURB_OBJECT_SETTER(ruby_curl_easy, userpwd);
}

/*
 * call-seq:
 *   easy.userpwd                                     => "pwd string"
 * 
 * Obtain the username/password string that will be used for subsequent 
 * calls to +perform+.
 */ 
static VALUE ruby_curl_easy_userpwd_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, userpwd);
}

/*
 * call-seq:
 *   easy.proxypwd = "pwd string"                     => "pwd string"
 * 
 * Set the username/password string to use for proxy connection during 
 * subsequent calls to +perform+. The supplied string should have the 
 * form "username:password"
 */
static VALUE ruby_curl_easy_proxypwd_set(VALUE self, VALUE proxypwd) {
  CURB_OBJECT_SETTER(ruby_curl_easy, proxypwd);
}

/*
 * call-seq:
 *   easy.proxypwd                                    => "pwd string"
 * 
 * Obtain the username/password string that will be used for proxy 
 * connection during subsequent calls to +perform+. The supplied string 
 * should have the form "username:password"
 */ 
static VALUE ruby_curl_easy_proxypwd_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, proxypwd);
}

/*
 * call-seq:
 *   easy.cookiejar = "cookiejar.file"                => "pwd string"
 * 
 * Set a cookiejar file to use for this Curl::Easy instance. This file
 * will be used to persist cookies.
 * 
 * *Note* that you must set enable_cookies true to enable the cookie
 * engine, or this option will be ignored.
 */
static VALUE ruby_curl_easy_cookiejar_set(VALUE self, VALUE cookiejar) {
  CURB_OBJECT_SETTER(ruby_curl_easy, cookiejar);
}

/*
 * call-seq:
 *   easy.cookiejar                                   => "cookiejar.file"
 * 
 * Obtain the cookiejar file to use for this Curl::Easy instance. 
 */ 
static VALUE ruby_curl_easy_cookiejar_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, cookiejar);
}

/*
 * call-seq:
 *   easy.cert = "cert.file"                          => ""
 * 
 * Set a cert file to use for this Curl::Easy instance. This file
 * will be used to validate SSL connections.
 * 
 */
static VALUE ruby_curl_easy_cert_set(VALUE self, VALUE cert) {
  CURB_OBJECT_SETTER(ruby_curl_easy, cert);
}

/*
 * call-seq:
 *   easy.cert                                        => "cert.file"
 * 
 * Obtain the cert file to use for this Curl::Easy instance. 
 */ 
static VALUE ruby_curl_easy_cert_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, cert);
}

/*
 * call-seq:
 *   easy.encoding=                                     => "string"
 * 
 * Set the accepted encoding types, curl will handle all of the decompression
 * 
 */ 
static VALUE ruby_curl_easy_encoding_set(VALUE self, VALUE encoding) {
  CURB_OBJECT_SETTER(ruby_curl_easy, encoding);
}
/*
 * call-seq:
 *   easy.encoding                                     => "string"
 * 
 * Get the set encoding types
 * 
*/ 
static VALUE ruby_curl_easy_encoding_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_easy, encoding);
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

/*
 * call-seq:
 *   easy.http_auth_types = fixnum or nil             => fixnum or nil
 * 
 * Set the HTTP authentication types that may be used for the following 
 * +perform+ calls. This is a bitmap made by ORing together the
 * Curl::CURLAUTH constants.
 */ 
static VALUE ruby_curl_easy_http_auth_types_set(VALUE self, VALUE http_auth_types) {
  CURB_IMMED_SETTER(ruby_curl_easy, http_auth_types, 0);
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

/* ================== BOOL ATTRS ===================*/

/*
 * call-seq:
 *   proxy_tunnel = boolean                           => boolean
 * 
 * Configure whether this Curl instance will use proxy tunneling.
 */
static VALUE ruby_curl_easy_proxy_tunnel_set(VALUE self, VALUE proxy_tunnel) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, proxy_tunnel);
}

/*
 * call-seq:
 *   proxy_tunnel?                                    => boolean
 * 
 * Determine whether this Curl instance will use proxy tunneling.
 */
static VALUE ruby_curl_easy_proxy_tunnel_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, proxy_tunnel);
}

/*
 * call-seq:
 *   fetch_file_time = boolean                        => boolean
 * 
 * Configure whether this Curl instance will fetch remote file
 * times, if available.
 */
static VALUE ruby_curl_easy_fetch_file_time_set(VALUE self, VALUE fetch_file_time) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, fetch_file_time);
}

/*
 * call-seq:
 *   fetch_file_time?                                 => boolean
 * 
 * Determine whether this Curl instance will fetch remote file
 * times, if available.
 */
static VALUE ruby_curl_easy_fetch_file_time_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, fetch_file_time);
}

/*
 * call-seq:
 *   ssl_verify_peer = boolean                        => boolean
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
 *   ssl_verify_peer?                                 => boolean
 * 
 * Determine whether this Curl instance will verify the SSL peer
 * certificate.
 */
static VALUE ruby_curl_easy_ssl_verify_peer_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, ssl_verify_peer);
}

/*
 * call-seq:
 *   ssl_verify_host = boolean                        => boolean
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
  CURB_BOOLEAN_SETTER(ruby_curl_easy, ssl_verify_host);
}

/*
 * call-seq:
 *   ssl_verify_host?                                 => boolean
 * 
 * Determine whether this Curl instance will verify that the server cert 
 * is for the server it is known as.
 */
static VALUE ruby_curl_easy_ssl_verify_host_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, ssl_verify_host);
}

/*
 * call-seq:
 *   header_in_body = boolean                         => boolean
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
 *   header_in_body?                                  => boolean
 * 
 * Determine whether this Curl instance will verify the SSL peer
 * certificate.
 */
static VALUE ruby_curl_easy_header_in_body_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, header_in_body);
}

/*
 * call-seq:
 *   use_netrc = boolean                              => boolean
 * 
 * Configure whether this Curl instance will use data from the user's
 * .netrc file for FTP connections.
 */
static VALUE ruby_curl_easy_use_netrc_set(VALUE self, VALUE use_netrc) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, use_netrc);
}

/*
 * call-seq:
 *   use_netrc?                                       => boolean
 * 
 * Determine whether this Curl instance will use data from the user's
 * .netrc file for FTP connections.
 */
static VALUE ruby_curl_easy_use_netrc_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, use_netrc);
}

/*
 * call-seq:
 *   follow_location = boolean                        => boolean
 * 
 * Configure whether this Curl instance will follow Location: headers
 * in HTTP responses. Redirects will only be followed to the extent
 * specified by +max_redirects+.
 */
static VALUE ruby_curl_easy_follow_location_set(VALUE self, VALUE follow_location) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, follow_location);
}

/*
 * call-seq:
 *   follow_location?                                 => boolean
 * 
 * Determine whether this Curl instance will follow Location: headers
 * in HTTP responses.
 */
static VALUE ruby_curl_easy_follow_location_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, follow_location);
}

/*
 * call-seq:
 *   unrestricted_auth = boolean                      => boolean
 * 
 * Configure whether this Curl instance may use any HTTP authentication
 * method available when necessary.
 */
static VALUE ruby_curl_easy_unrestricted_auth_set(VALUE self, VALUE unrestricted_auth) {
  CURB_BOOLEAN_SETTER(ruby_curl_easy, unrestricted_auth);
}

/*
 * call-seq:
 *   unrestricted_auth?                               => boolean
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
 *   easy.enable_cookies?                        => boolean
 * 
 * Determine whether the libcurl cookie engine is enabled for this 
 * Curl::Easy instance.
 */
static VALUE ruby_curl_easy_enable_cookies_q(VALUE self) {
  CURB_BOOLEAN_GETTER(ruby_curl_easy, enable_cookies);
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
  CURB_HANDLER_PROC_SETTER(ruby_curl_easy, body_proc);
}

/*
 * call-seq:
 *   easy.on_success { ... }                 => &lt;old handler&gt;
 * 
 * Assign or remove the +on_success+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 * 
 * The +on_success+ handler is called when the request is finished with a
 * status of 20x
 */
static VALUE ruby_curl_easy_on_success_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_SETTER(ruby_curl_easy, success_proc);
}

/*
 * call-seq:
 *   easy.on_failure { ... }                 => &lt;old handler&gt;
 * 
 * Assign or remove the +on_failure+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 * 
 * The +on_failure+ handler is called when the request is finished with a
 * status of 50x
 */
static VALUE ruby_curl_easy_on_failure_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_SETTER(ruby_curl_easy, failure_proc);
}

/*
 * call-seq:
 *   easy.on_complete { ... }                 => &lt;old handler&gt;
 * 
 * Assign or remove the +on_complete+ handler for this Curl::Easy instance.
 * To remove a previously-supplied handler, call this method with no
 * attached block.
 * 
 * The +on_complete+ handler is called when the request is finished.
 */
static VALUE ruby_curl_easy_on_complete_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_SETTER(ruby_curl_easy, complete_proc);
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
  CURB_HANDLER_PROC_SETTER(ruby_curl_easy, header_proc);
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
  CURB_HANDLER_PROC_SETTER(ruby_curl_easy, progress_proc);
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
  CURB_HANDLER_PROC_SETTER(ruby_curl_easy, debug_proc);
}


/* =================== PERFORM =====================*/

/***********************************************
 * This is an rb_iterate callback used to set up http headers.
 */
static VALUE cb_each_http_header(VALUE header, struct curl_slist **list) {
  VALUE header_str = Qnil;
  
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
 *
 * Setup a connection
 *
 * Always returns Qtrue, rb_raise on error.
 */
VALUE ruby_curl_easy_setup( ruby_curl_easy *rbce, VALUE *body_buffer, VALUE *header_buffer, struct curl_slist **hdrs ) {
  // TODO this could do with a bit of refactoring...
  CURL *curl;

  curl = rbce->curl;
  
  if (rbce->url == Qnil) {  
    rb_raise(eCurlErrError, "No URL supplied");
  }

  VALUE url = rb_check_string_type(rbce->url);

  // Need to configure the handler as per settings in rbce
  curl_easy_setopt(curl, CURLOPT_URL, StringValuePtr(url));
  
  // network stuff and auth
  if (rbce->interface != Qnil) {
    curl_easy_setopt(curl, CURLOPT_INTERFACE, StringValuePtr(rbce->interface));
  } else {
    curl_easy_setopt(curl, CURLOPT_INTERFACE, NULL);
  }      
  
  if (rbce->userpwd != Qnil) {
    curl_easy_setopt(curl, CURLOPT_USERPWD, StringValuePtr(rbce->userpwd));
  } else {
    curl_easy_setopt(curl, CURLOPT_USERPWD, NULL);
  }      
  
  if (rbce->proxy_url != Qnil) {
    curl_easy_setopt(curl, CURLOPT_PROXY, StringValuePtr(rbce->proxy_url));
  } else {
    curl_easy_setopt(curl, CURLOPT_PROXY, NULL);
  }      
  
  if (rbce->proxypwd != Qnil) {
    curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, StringValuePtr(rbce->proxypwd));
  } else {
    curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, NULL);
  }      
  
  // body/header procs
  if (rbce->body_proc != Qnil) {      
    *body_buffer = Qnil;      
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)&proc_data_handler);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, rbce->body_proc);
  } else {
    *body_buffer = rb_str_buf_new(32768);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)&default_data_handler);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, *body_buffer);
  }
      
  if (rbce->header_proc != Qnil) {
    *header_buffer = Qnil;      
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)&proc_data_handler);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, rbce->header_proc);
  } else {
    *header_buffer = rb_str_buf_new(32768);      
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)&default_data_handler);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, *header_buffer);
  }

  /* encoding */
  if (rbce->encoding != Qnil) {
    curl_easy_setopt(curl, CURLOPT_ENCODING, StringValuePtr(rbce->encoding)); 
  }

  // progress and debug procs    
  if (rbce->progress_proc != Qnil) {
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, (curl_progress_callback)&proc_progress_handler);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, rbce->progress_proc);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
  } else {
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
  }
  
  if (rbce->debug_proc != Qnil) {
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, (curl_debug_callback)&proc_debug_handler);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, rbce->debug_proc);
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
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, rbce->ssl_verify_peer);
          
  if ((rbce->use_netrc != Qnil) && (rbce->use_netrc != Qfalse)) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, CURL_NETRC_OPTIONAL);
  } else {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, CURL_NETRC_IGNORED);
  }      

  curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, rbce->unrestricted_auth);
  
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, rbce->timeout);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, rbce->connect_timeout);
  curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, rbce->dns_cache_timeout);

#if LIBCURL_VERSION_NUM >= 0x070a08
  curl_easy_setopt(curl, CURLOPT_FTP_RESPONSE_TIMEOUT, rbce->ftp_response_timeout);
#else
  if (rbce->ftp_response_timeout > 0) {
    rb_warn("Installed libcurl is too old to support ftp_response_timeout");
  }
#endif
  
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
    if (rbce->cookiejar != Qnil) {
      curl_easy_setopt(curl, CURLOPT_COOKIEJAR, StringValuePtr(rbce->cookiejar));
    } else {
      curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); /* "" = magic to just enable */
    }
  }

  /* Set up HTTPS cert handling if necessary */
  if (rbce->cert != Qnil) {
    curl_easy_setopt(curl, CURLOPT_SSLCERT, StringValuePtr(rbce->cert));
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/usr/local/share/curl/curl-ca-bundle.crt");
  }
  
  /* Setup HTTP headers if necessary */
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);   // clear
  
  if (rbce->headers != Qnil) {      
    if ((rb_type(rbce->headers) == T_ARRAY) || (rb_type(rbce->headers) == T_HASH)) {
      rb_iterate(rb_each, rbce->headers, cb_each_http_header, (VALUE)hdrs);        
    } else {
      VALUE headers_str = rb_obj_as_string(rbce->headers);
      *hdrs = curl_slist_append(*hdrs, StringValuePtr(headers_str));
    }
    
    if (*hdrs) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *hdrs);
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
VALUE ruby_curl_easy_cleanup( VALUE self, ruby_curl_easy *rbce, VALUE bodybuf, VALUE headerbuf, struct curl_slist *headers ) {

  // Free everything up
  if (headers) {
    curl_slist_free_all(headers);
    rbce->curl_headers = NULL;
  }
  
  // Sort out the built-in body/header data.
  if (bodybuf != Qnil) {
    if (TYPE(bodybuf) == T_STRING) {
      rbce->body_data = rb_str_to_str(bodybuf);
    }
    else if (rb_respond_to(bodybuf, rb_intern("to_str"))) {
      rbce->body_data = rb_funcall(bodybuf, rb_intern("to_str"), 0);
    }
    else {
      rbce->body_data = Qnil;
    }
  } else {
    rbce->body_data = Qnil;
  }

  if (headerbuf != Qnil) {
    if (TYPE(headerbuf) == T_STRING) {
      rbce->header_data = rb_str_to_str(headerbuf);
    }
    else if (rb_respond_to(headerbuf, rb_intern("to_str"))) {
      rbce->header_data = rb_funcall(headerbuf, rb_intern("to_str"), 0);
    }
    else {
      rbce->header_data = Qnil;
    }
  } else {
    rbce->header_data = Qnil;
  }

  return Qnil;
}

/***********************************************
 * 
 * This is the main worker for the perform methods (get, post, head, put).
 * It's not surfaced as a Ruby method - instead, the individual request
 * methods are responsible for setting up stuff specific to that type,
 * then calling this to handle common stuff and do the perform.
 * 
 * Always returns Qtrue, rb_raise on error.
 * 
 */
static VALUE handle_perform(VALUE self, ruby_curl_easy *rbce) {  

  int msgs;
  int still_running = 1;
  CURLcode result = -1;
  CURLMcode mcode = -1;
  CURLM *multi_handle = curl_multi_init();
  struct curl_slist *headers = NULL;
  VALUE bodybuf = Qnil, headerbuf = Qnil;
//  char errors[CURL_ERROR_SIZE*2];

  ruby_curl_easy_setup(rbce, &bodybuf, &headerbuf, &headers);
//  curl_easy_setopt(rbce->curl, CURLOPT_ERRORBUFFER, errors);
//  curl_easy_setopt(rbce->curl, CURLOPT_VERBOSE, 1);

//  if( rb_thread_alone() ) {
//    result = curl_easy_perform(rbce->curl);
//  }
//  else {

    /* NOTE:
     * We create an Curl multi handle here and use rb_thread_select allowing other ruby threads to 
     * perform actions... ideally we'd have just 1 shared multi handle per all curl easy handles globally
     */
    mcode = curl_multi_add_handle(multi_handle, rbce->curl);
    if (mcode != CURLM_CALL_MULTI_PERFORM && mcode != CURLM_OK) {
      raise_curl_multi_error_exception(mcode);
    }

    while(CURLM_CALL_MULTI_PERFORM == (mcode=curl_multi_perform(multi_handle, &still_running)) ) ;

    if (mcode != CURLM_CALL_MULTI_PERFORM && mcode != CURLM_OK) {
      raise_curl_multi_error_exception(mcode);
    }

    while(still_running) {
      struct timeval timeout;
      int rc; /* select() return code */
      int maxfd;

      fd_set fdread;
      fd_set fdwrite;
      fd_set fdexcep;

      FD_ZERO(&fdread);
      FD_ZERO(&fdwrite);
      FD_ZERO(&fdexcep);

      /* set a suitable timeout to play around with */
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      /* get file descriptors from the transfers */
      mcode = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
      if (mcode != CURLM_CALL_MULTI_PERFORM && mcode != CURLM_OK) {
        raise_curl_multi_error_exception(mcode);
      }

      rc = rb_thread_select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
      if (rc < 0) {
        rb_raise(rb_eRuntimeError, "select(): %s", strerror(errno));
      }
   
      switch(rc) {
      case 0:
      default:
        /* timeout or readable/writable sockets */
        while(CURLM_CALL_MULTI_PERFORM == (mcode=curl_multi_perform(multi_handle, &still_running)) );
      break;
      }
   
      if (mcode != CURLM_CALL_MULTI_PERFORM && mcode != CURLM_OK) {
        raise_curl_multi_error_exception(mcode);
      }

    }
 
    /* check for errors */
    CURLMsg *msg = curl_multi_info_read(multi_handle, &msgs);
    if (msg && msg->msg == CURLMSG_DONE) {
      result = msg->data.result;
    }

    curl_multi_remove_handle(multi_handle, rbce->curl);
    curl_multi_cleanup(multi_handle);
//  }

  ruby_curl_easy_cleanup(self, rbce, bodybuf, headerbuf, headers);
  
  if (rbce->complete_proc != Qnil) {
    rb_funcall( rbce->complete_proc, idCall, 1, self );
  }

  /* check the request status and determine if on_success or on_failure should be called */
  long response_code = -1;
  curl_easy_getinfo(rbce->curl, CURLINFO_RESPONSE_CODE, &response_code);
  if (result != 0) {
//    printf("error: %s\n", errors);
    if (rbce->failure_proc != Qnil) {
      rb_funcall( rbce->failure_proc, idCall, 1, self );
    } else {
      raise_curl_easy_error_exception(result);
    }
  }
  else if (rbce->success_proc != Qnil &&
           /* NOTE: we allow response_code == 0, in the case the file is being read from disk */
           ((response_code >= 200 && response_code < 300) || response_code == 0)) {
    rb_funcall( rbce->success_proc, idCall, 1, self );
  }
  else if (rbce->failure_proc != Qnil && 
           (response_code >= 300 && response_code < 600)) {
    rb_funcall( rbce->failure_proc, idCall, 1, self );
  }

  return Qtrue;
}

/*
 * call-seq:
 *   easy.http_get                                    => true
 * 
 * GET the currently configured URL using the current options set for
 * this Curl::Easy instance. This method always returns true, or raises
 * an exception (defined under Curl::Err) on error.
 */
static VALUE ruby_curl_easy_perform_get(VALUE self) {  
  ruby_curl_easy *rbce;
  CURL *curl;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl = rbce->curl;
  
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
  
  return handle_perform(self,rbce);
}

/*
 * call-seq:
 *   easy.http_delete
 *
 * DELETE the currently configured URL using the current options set for
 * this Curl::Easy instance. This method always returns true, or raises
 * an exception (defined under Curl::Err) on error.
 */
static VALUE ruby_curl_easy_perform_delete(VALUE self) {
  ruby_curl_easy *rbce;
  CURL *curl;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl = rbce->curl;
  
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

  VALUE retval = handle_perform(self,rbce);

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);

  return retval;
}

/*
 * call-seq:
 *   easy.perform                                     => true
 * 
 * Transfer the currently configured URL using the options set for this
 * Curl::Easy instance. If this is an HTTP URL, it will be transferred via
 * the GET request method (i.e. this method is a synonym for +http_get+ 
 * when using HTTP URLs).
 */
static VALUE ruby_curl_easy_perform(VALUE self) {  
  return ruby_curl_easy_perform_get(self);
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
  curl = curl_easy_duphandle(rbce->curl);
  curl_easy_cleanup(rbce->curl);
  rbce->curl = curl;
  
  if (rbce->multipart_form_post) {
    VALUE ret;
    struct curl_httppost *first = NULL, *last = NULL;

    // Make the multipart form
    for (i = 0; i < argc; i++) {
      if (rb_obj_is_instance_of(argv[i], cCurlPostField)) {
        append_to_form(argv[i], &first, &last);
      } else {
        rb_raise(eCurlErrInvalidPostField, "You must use PostFields only with multipart form posts");
        return Qnil;
      }
    }

    curl_easy_setopt(curl, CURLOPT_POST, 0);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, first);      
    ret = handle_perform(self,rbce);    
    curl_formfree(first);
    
    return ret;    
  } else {
    long len;
    char *data;
    
    if ((rbce->postdata_buffer = rb_funcall(args_ary, idJoin, 1, rbstrAmp)) == Qnil) {
      rb_raise(eCurlErrError, "Failed to join arguments");
      return Qnil;
    } else { 
      data = StringValuePtr(rbce->postdata_buffer);   
      len = RSTRING_LEN(rbce->postdata_buffer);
      
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, len);

      return handle_perform(self,rbce);
    }
  }
}

/*
 * call-seq:
 *   easy.http_head                                   => true
 * 
 * Request headers from the currently configured URL using the HEAD
 * method and current options set for this Curl::Easy instance. This 
 * method always returns true, or raises an exception (defined under 
 * Curl::Err) on error.
 * 
 */
static VALUE ruby_curl_easy_perform_head(VALUE self) {
  ruby_curl_easy *rbce;
  CURL *curl;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl = rbce->curl;

  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);

  return handle_perform(self,rbce);
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
  char *buffer;
  int len;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  curl = rbce->curl;

  buffer = StringValuePtr(data);
  len = RSTRING_LEN(data);

  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, (curl_read_callback)read_data_handler);
  curl_easy_setopt(curl, CURLOPT_READDATA, &buffer);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE, len);

  return handle_perform(self, rbce);
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
  CURB_OBJECT_GETTER(ruby_curl_easy, body_data);
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
  CURB_OBJECT_GETTER(ruby_curl_easy, header_data);
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
 *   easy.redirect_count                           => integer
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

Pass the address of a 'struct curl_slist *' to receive a linked-list of OpenSSL crypto-engines supported. Note that engines are normally implemented in separate dynamic libraries. Hence not all the returned engines may be available at run-time. NOTE: you must call curl_slist_free_all(3) on the list pointer once you're done with it, as libcurl will not free the data for you. (Added in 7.12.3) 
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
 *   easy.num_connects                                    => integer
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
 *   easy.content_type                                => "content/type" or nil
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
static VALUE ruby_curl_easy_escape(VALUE self, VALUE str) {
  ruby_curl_easy *rbce;
  char *result;
  VALUE rresult;

  Data_Get_Struct(self, ruby_curl_easy, rbce);
  
#if (LIBCURL_VERSION_NUM >= 0x070f04)
  result = (char*)curl_easy_escape(rbce->curl, StringValuePtr(str), RSTRING_LEN(str));
#else
  result = (char*)curl_escape(StringValuePtr(str), RSTRING_LEN(str));
#endif

  rresult = rb_str_new2(result);
  curl_free(result); 
  
  return rresult;
}

/*
 * call-seq:
 *   easy.unescape("some text")                       => "some%20text"
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
  result = (char*)curl_easy_unescape(rbce->curl, StringValuePtr(str), RSTRING_LEN(str), &rlen);
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
 *   Curl::Easy.perform(url) { |easy| ... }           => #&lt;Curl::Easy...&gt;
 *   
 * Convenience method that creates a new Curl::Easy instance with 
 * the specified URL and calls the general +perform+ method, before returning 
 * the new instance. For HTTP URLs, this is equivalent to calling +http_get+.
 * 
 * If a block is supplied, the new instance will be yielded just prior to
 * the +http_get+ call.
 */
static VALUE ruby_curl_easy_class_perform(int argc, VALUE *argv, VALUE klass) {
  VALUE c = ruby_curl_easy_new(argc, argv, klass);  
  
  if (rb_block_given_p()) {
    rb_yield(c);
  }
  
  ruby_curl_easy_perform(c);
  return c;  
}

/*
 * call-seq:
 *   Curl::Easy.http_get(url) { |easy| ... }          => #&lt;Curl::Easy...&gt;
 *   
 * Convenience method that creates a new Curl::Easy instance with 
 * the specified URL and calls +http_get+, before returning the new instance.
 * 
 * If a block is supplied, the new instance will be yielded just prior to
 * the +http_get+ call.
 */
static VALUE ruby_curl_easy_class_perform_get(int argc, VALUE *argv, VALUE klass) {
  VALUE c = ruby_curl_easy_new(argc, argv, klass);  
  
  if (rb_block_given_p()) {
    rb_yield(c);
  }
  
  ruby_curl_easy_perform_get(c);
  return c;  
}

/*
 * call-seq:
 *   Curl::Easy.http_delete(url) { |easy| ... }          => #&lt;Curl::Easy...&gt;
 *   
 * Convenience method that creates a new Curl::Easy instance with 
 * the specified URL and calls +http_delete+, before returning the new instance.
 * 
 * If a block is supplied, the new instance will be yielded just prior to
 * the +http_delete+ call.
 */
static VALUE ruby_curl_easy_class_perform_delete(int argc, VALUE *argv, VALUE klass) {
  VALUE c = ruby_curl_easy_new(argc, argv, klass);  
  
  if (rb_block_given_p()) {
    rb_yield(c);
  }
  
  ruby_curl_easy_perform_delete(c);
  return c;
}

/*
 * call-seq:
 *   Curl::Easy.http_head(url) { |easy| ... }          => #&lt;Curl::Easy...&gt;
 *
 * Convenience method that creates a new Curl::Easy instance with
 * the specified URL and calls +http_head+, before returning the new instance.
 *
 * If a block is supplied, the new instance will be yielded just prior to
 * the +http_head+ call.
 */
static VALUE ruby_curl_easy_class_perform_head(int argc, VALUE *argv, VALUE klass) {
  VALUE c = ruby_curl_easy_new(argc, argv, klass);

  if (rb_block_given_p()) {
    rb_yield(c);
  }

  ruby_curl_easy_perform_head(c);
  return c;
}

// TODO: add convenience method for http_post

/*
 * call-seq:
 *   Curl::Easy.http_post(url, "some=urlencoded%20form%20data&and=so%20on") => true
 *   Curl::Easy.http_post(url, "some=urlencoded%20form%20data", "and=so%20on", ...) => true
 *   Curl::Easy.http_post(url, "some=urlencoded%20form%20data", Curl::PostField, "and=so%20on", ...) => true
 *   Curl::Easy.http_post(url, Curl::PostField, Curl::PostField ..., Curl::PostField) => true
 * 
 * POST the specified formdata to the currently configured URL using 
 * the current options set for this Curl::Easy instance. This method 
 * always returns true, or raises an exception (defined under 
 * Curl::Err) on error.
 * 
 * If you wish to use multipart form encoding, you'll need to supply a block
 * in order to set multipart_form_post true. See #http_post for more 
 * information.
 */
static VALUE ruby_curl_easy_class_perform_post(int argc, VALUE *argv, VALUE klass) {   
  VALUE url, fields;
  
  rb_scan_args(argc, argv, "1*", &url, &fields);
  
  VALUE c = ruby_curl_easy_new(1, &url, klass);  
  
  if (argc > 1) {
    ruby_curl_easy_perform_post(argc - 1, &argv[1], c);
  } else {
    ruby_curl_easy_perform_post(0, NULL, c);
  }    
  
  return c;  
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
  rb_define_singleton_method(cCurlEasy, "perform", ruby_curl_easy_class_perform, -1);
  rb_define_singleton_method(cCurlEasy, "http_delete", ruby_curl_easy_class_perform_delete, -1);
  rb_define_singleton_method(cCurlEasy, "http_get", ruby_curl_easy_class_perform_get, -1);
  rb_define_singleton_method(cCurlEasy, "http_post", ruby_curl_easy_class_perform_post, -1);
  rb_define_singleton_method(cCurlEasy, "http_head", ruby_curl_easy_class_perform_head, -1);

  /* Attributes for config next perform */
  rb_define_method(cCurlEasy, "url=", ruby_curl_easy_url_set, 1);
  rb_define_method(cCurlEasy, "url", ruby_curl_easy_url_get, 0);  
  rb_define_method(cCurlEasy, "proxy_url=", ruby_curl_easy_proxy_url_set, 1);
  rb_define_method(cCurlEasy, "proxy_url", ruby_curl_easy_proxy_url_get, 0);
  rb_define_method(cCurlEasy, "headers=", ruby_curl_easy_headers_set, 1);
  rb_define_method(cCurlEasy, "headers", ruby_curl_easy_headers_get, 0);
  rb_define_method(cCurlEasy, "interface=", ruby_curl_easy_interface_set, 1);
  rb_define_method(cCurlEasy, "interface", ruby_curl_easy_interface_get, 0);
  rb_define_method(cCurlEasy, "userpwd=", ruby_curl_easy_userpwd_set, 1);
  rb_define_method(cCurlEasy, "userpwd", ruby_curl_easy_userpwd_get, 0);
  rb_define_method(cCurlEasy, "proxypwd=", ruby_curl_easy_proxypwd_set, 1);
  rb_define_method(cCurlEasy, "proxypwd", ruby_curl_easy_proxypwd_get, 0);
  rb_define_method(cCurlEasy, "cookiejar=", ruby_curl_easy_cookiejar_set, 1);
  rb_define_method(cCurlEasy, "cookiejar", ruby_curl_easy_cookiejar_get, 0);
  rb_define_method(cCurlEasy, "cert=", ruby_curl_easy_cert_set, 1);
  rb_define_method(cCurlEasy, "cert", ruby_curl_easy_cert_get, 0);
  rb_define_method(cCurlEasy, "encoding=", ruby_curl_easy_encoding_set, 1);
  rb_define_method(cCurlEasy, "encoding", ruby_curl_easy_encoding_get, 0);

  rb_define_method(cCurlEasy, "local_port=", ruby_curl_easy_local_port_set, 1);
  rb_define_method(cCurlEasy, "local_port", ruby_curl_easy_local_port_get, 0);
  rb_define_method(cCurlEasy, "local_port_range=", ruby_curl_easy_local_port_range_set, 1);
  rb_define_method(cCurlEasy, "local_port_range", ruby_curl_easy_local_port_range_get, 0);
  rb_define_method(cCurlEasy, "proxy_port=", ruby_curl_easy_proxy_port_set, 1);
  rb_define_method(cCurlEasy, "proxy_port", ruby_curl_easy_proxy_port_get, 0);
  rb_define_method(cCurlEasy, "proxy_type=", ruby_curl_easy_proxy_type_set, 1);
  rb_define_method(cCurlEasy, "proxy_type", ruby_curl_easy_proxy_type_get, 0);
  rb_define_method(cCurlEasy, "http_auth_types=", ruby_curl_easy_http_auth_types_set, 1);
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
  
  rb_define_method(cCurlEasy, "proxy_tunnel=", ruby_curl_easy_proxy_tunnel_set, 1);
  rb_define_method(cCurlEasy, "proxy_tunnel?", ruby_curl_easy_proxy_tunnel_q, 0);
  rb_define_method(cCurlEasy, "fetch_file_time=", ruby_curl_easy_fetch_file_time_set, 1);
  rb_define_method(cCurlEasy, "fetch_file_time?", ruby_curl_easy_fetch_file_time_q, 0);
  rb_define_method(cCurlEasy, "ssl_verify_peer=", ruby_curl_easy_ssl_verify_peer_set, 1);
  rb_define_method(cCurlEasy, "ssl_verify_peer?", ruby_curl_easy_ssl_verify_peer_q, 0);
  rb_define_method(cCurlEasy, "ssl_verify_host=", ruby_curl_easy_ssl_verify_host_set, 1);
  rb_define_method(cCurlEasy, "ssl_verify_host?", ruby_curl_easy_ssl_verify_host_q, 0);
  rb_define_method(cCurlEasy, "header_in_body=", ruby_curl_easy_header_in_body_set, 1);
  rb_define_method(cCurlEasy, "header_in_body?", ruby_curl_easy_header_in_body_q, 0);
  rb_define_method(cCurlEasy, "use_netrc=", ruby_curl_easy_use_netrc_set, 1);
  rb_define_method(cCurlEasy, "use_netrc?", ruby_curl_easy_use_netrc_q, 0);
  rb_define_method(cCurlEasy, "follow_location=", ruby_curl_easy_follow_location_set, 1);
  rb_define_method(cCurlEasy, "follow_location?", ruby_curl_easy_follow_location_q, 0);
  rb_define_method(cCurlEasy, "unrestricted_auth=", ruby_curl_easy_unrestricted_auth_set, 1);
  rb_define_method(cCurlEasy, "unrestricted_auth?", ruby_curl_easy_unrestricted_auth_q, 0);
  rb_define_method(cCurlEasy, "verbose=", ruby_curl_easy_verbose_set, 1);
  rb_define_method(cCurlEasy, "verbose?", ruby_curl_easy_verbose_q, 0);
  rb_define_method(cCurlEasy, "multipart_form_post=", ruby_curl_easy_multipart_form_post_set, 1);
  rb_define_method(cCurlEasy, "multipart_form_post?", ruby_curl_easy_multipart_form_post_q, 0);
  rb_define_method(cCurlEasy, "enable_cookies=", ruby_curl_easy_enable_cookies_set, 1);
  rb_define_method(cCurlEasy, "enable_cookies?", ruby_curl_easy_enable_cookies_q, 0);

  rb_define_method(cCurlEasy, "on_body", ruby_curl_easy_on_body_set, -1);
  rb_define_method(cCurlEasy, "on_header", ruby_curl_easy_on_header_set, -1);
  rb_define_method(cCurlEasy, "on_progress", ruby_curl_easy_on_progress_set, -1);
  rb_define_method(cCurlEasy, "on_debug", ruby_curl_easy_on_debug_set, -1);
  rb_define_method(cCurlEasy, "on_success", ruby_curl_easy_on_success_set, -1);
  rb_define_method(cCurlEasy, "on_failure", ruby_curl_easy_on_failure_set, -1);
  rb_define_method(cCurlEasy, "on_complete", ruby_curl_easy_on_complete_set, -1);

  rb_define_method(cCurlEasy, "perform", ruby_curl_easy_perform, 0);
  rb_define_method(cCurlEasy, "http_delete", ruby_curl_easy_perform_delete, 0);
  rb_define_method(cCurlEasy, "http_get", ruby_curl_easy_perform_get, 0);
  rb_define_method(cCurlEasy, "http_post", ruby_curl_easy_perform_post, -1);
  rb_define_method(cCurlEasy, "http_head", ruby_curl_easy_perform_head, 0);
  rb_define_method(cCurlEasy, "http_put", ruby_curl_easy_perform_put, 1);

  /* Post-perform info methods */
  rb_define_method(cCurlEasy, "body_str", ruby_curl_easy_body_str_get, 0);
  rb_define_method(cCurlEasy, "header_str", ruby_curl_easy_header_str_get, 0);

  rb_define_method(cCurlEasy, "last_effective_url", ruby_curl_easy_last_effective_url_get, 0);
  rb_define_method(cCurlEasy, "response_code", ruby_curl_easy_response_code_get, 0);
  rb_define_method(cCurlEasy, "http_connect_code", ruby_curl_easy_http_connect_code_get, 0);
  rb_define_method(cCurlEasy, "file_time", ruby_curl_easy_file_time_get, 0);
  rb_define_method(cCurlEasy, "total_time", ruby_curl_easy_total_time_get, 0);
  rb_define_method(cCurlEasy, "total_time", ruby_curl_easy_total_time_get, 0);
  rb_define_method(cCurlEasy, "name_lookup_time", ruby_curl_easy_name_lookup_time_get, 0);
  rb_define_method(cCurlEasy, "connect_time", ruby_curl_easy_connect_time_get, 0);
  rb_define_method(cCurlEasy, "pre_transfer_time", ruby_curl_easy_pre_transfer_time_get, 0);
  rb_define_method(cCurlEasy, "start_transfer_time", ruby_curl_easy_start_transfer_time_get, 0);
  rb_define_method(cCurlEasy, "redirect_time", ruby_curl_easy_redirect_time_get, 0);
  rb_define_method(cCurlEasy, "redirect_count", ruby_curl_easy_redirect_count_get, 0);
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

  /* Curl utils */
  rb_define_method(cCurlEasy, "escape", ruby_curl_easy_escape, 1);
  rb_define_method(cCurlEasy, "unescape", ruby_curl_easy_unescape, 1);

  /* Runtime support */
  rb_define_method(cCurlEasy, "clone", ruby_curl_easy_clone, 0);
  rb_define_alias(cCurlEasy, "dup", "clone");  
}
