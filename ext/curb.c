/* Curb - Libcurl(3) bindings for Ruby.
 * Copyright (c)2006 Ross Bamford. 
 * Licensed under the Ruby License. See LICENSE for details.
 * 
 * $Id: curb.c 35 2006-12-23 15:22:19Z roscopeco $
 */
 
#include "curb.h"

VALUE mCurl;

/* ================== VER QUERY FUNCS ==============*/

/*
 * call-seq:
 *   Curl.ipv6?                                       => true or false
 * 
 * Returns true if the installed libcurl supports IPv6. 
 */
static VALUE ruby_curl_ipv6_q(VALUE mod) {
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_IPV6) ? Qtrue : Qfalse);
}

/*
 * call-seq:
 *   Curl.kerberos4?                                  => true or false
 * 
 * Returns true if the installed libcurl supports Kerberos4 authentication
 * with FTP connections. 
 */
static VALUE ruby_curl_kerberos4_q(VALUE mod) {
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_KERBEROS4) ? Qtrue : Qfalse);
}

/*
 * call-seq:
 *   Curl.ssl?                                        => true or false
 * 
 * Returns true if the installed libcurl supports SSL connections.
 * For libcurl versions < 7.10, always returns false.
 */
static VALUE ruby_curl_ssl_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_SSL
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_SSL) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.libz?                                       => true or false
 * 
 * Returns true if the installed libcurl supports HTTP deflate
 * using libz. For libcurl versions < 7.10, always returns false.
 */
static VALUE ruby_curl_libz_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_LIBZ
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_LIBZ) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.ntlm?                                       => true or false
 * 
 * Returns true if the installed libcurl supports HTTP NTLM.
 * For libcurl versions < 7.10.6, always returns false.
 */
static VALUE ruby_curl_ntlm_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_NTLM
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_NTLM) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.gssnegotiate?                               => true or false
 * 
 * Returns true if the installed libcurl supports HTTP GSS-Negotiate.
 * For libcurl versions < 7.10.6, always returns false.
 */
static VALUE ruby_curl_gssnegotiate_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_GSSNEGOTIATE
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_GSSNEGOTIATE) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.debug?                                      => true or false
 * 
 * Returns true if the installed libcurl was built with extra debug 
 * capabilities built-in. For libcurl versions < 7.10.6, always returns
 * false.
 */
static VALUE ruby_curl_debug_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_DEBUG
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_DEBUG) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.asyncdns?                                   => true or false
 * 
 * Returns true if the installed libcurl was built with support for
 * asynchronous name lookups, which allows more exact timeouts (even
 * on Windows) and less blocking when using the multi interface.
 * For libcurl versions < 7.10.7, always returns false.
 */
static VALUE ruby_curl_asyncdns_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_ASYNCHDNS
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_ASYNCHDNS) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.spnego?                                     => true or false
 * 
 * Returns true if the installed libcurl was built with support for SPNEGO
 * authentication (Simple and Protected GSS-API Negotiation Mechanism, defined
 * in RFC 2478). For libcurl versions < 7.10.8, always returns false.
 */
static VALUE ruby_curl_spnego_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_SPNEGO
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_SPNEGO) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.largefile?                                  => true or false
 * 
 * Returns true if the installed libcurl was built with support for large
 * files. For libcurl versions < 7.11.1, always returns false.
 */
static VALUE ruby_curl_largefile_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_LARGEFILE
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_LARGEFILE) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.idn?                                        => true or false
 * 
 * Returns true if the installed libcurl was built with support for IDNA, 
 * domain names with international letters. For libcurl versions < 7.12.0, 
 * always returns false.
 */
static VALUE ruby_curl_idn_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_IDN
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_IDN) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.sspi?                                       => true or false
 * 
 * Returns true if the installed libcurl was built with support for SSPI.
 * This is only available on Windows and makes libcurl use Windows-provided
 * functions for NTLM authentication. It also allows libcurl to use the current
 * user and the current user's password without the app having to pass them on. 
 * For libcurl versions < 7.13.2, always returns false.
 */
static VALUE ruby_curl_sspi_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_SSPI
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_SSPI) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

/*
 * call-seq:
 *   Curl.conv?                                       => true or false
 * 
 * Returns true if the installed libcurl was built with support for character 
 * conversions. For libcurl versions < 7.15.4, always returns false.
 */
static VALUE ruby_curl_conv_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_CONV
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);  
  return((ver->features & CURL_VERSION_CONV) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

void Init_curb_core() {
  // TODO we need to call curl_global_cleanup at exit!
  curl_global_init(CURL_GLOBAL_ALL);
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);
  VALUE curlver, curllongver, curlvernum;

  mCurl = rb_define_module("Curl");  
  
  curlver = rb_str_new2(ver->version);
  curllongver = rb_str_new2(curl_version());
  curlvernum = LONG2NUM(LIBCURL_VERSION_NUM);
  
  rb_define_const(mCurl, "CURB_VERSION", rb_str_new2(CURB_VERSION));    
  rb_define_const(mCurl, "VERSION", curlver);
  rb_define_const(mCurl, "CURL_VERSION", curlver);
  rb_define_const(mCurl, "VERNUM", curlvernum);
  rb_define_const(mCurl, "CURL_VERNUM", curlvernum);
  rb_define_const(mCurl, "LONG_VERSION", curllongver);  
  rb_define_const(mCurl, "CURL_LONG_VERSION", curllongver);  

  /* Passed to on_debug handler to indicate that the data is informational text. */
  rb_define_const(mCurl, "CURLINFO_TEXT", INT2FIX(CURLINFO_TEXT));
  
  /* Passed to on_debug handler to indicate that the data is header (or header-like) data received from the peer. */
  rb_define_const(mCurl, "CURLINFO_HEADER_IN", INT2FIX(CURLINFO_HEADER_IN));
  
  /* Passed to on_debug handler to indicate that the data is header (or header-like) data sent to the peer. */
  rb_define_const(mCurl, "CURLINFO_HEADER_OUT", INT2FIX(CURLINFO_HEADER_OUT));
  
  /* Passed to on_debug handler to indicate that the data is protocol data received from the peer. */
  rb_define_const(mCurl, "CURLINFO_DATA_IN", INT2FIX(CURLINFO_DATA_IN));
  
  /* Passed to on_debug handler to indicate that the data is protocol data sent to the peer. */
  rb_define_const(mCurl, "CURLINFO_DATA_OUT", INT2FIX(CURLINFO_DATA_OUT));

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is an HTTP proxy. (libcurl >= 7.10) */
#ifdef HAVE_CURLPROXY_HTTP
  rb_define_const(mCurl, "CURLPROXY_HTTP", INT2FIX(CURLPROXY_HTTP));
#else
  rb_define_const(mCurl, "CURLPROXY_HTTP", INT2FIX(-1));
#endif

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is a SOCKS4 proxy. (libcurl >= 7.15.2) */
#ifdef HAVE_CURLPROXY_SOCKS4  
  rb_define_const(mCurl, "CURLPROXY_SOCKS4", INT2FIX(CURLPROXY_SOCKS4));
#else
  rb_define_const(mCurl, "CURLPROXY_SOCKS4", INT2FIX(-2));
#endif

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is a SOCKS5 proxy. (libcurl >= 7.10) */
#ifdef HAVE_CURLPROXY_SOCKS5    
  rb_define_const(mCurl, "CURLPROXY_SOCKS5", INT2FIX(CURLPROXY_SOCKS5));
#else
  rb_define_const(mCurl, "CURLPROXY_SOCKS5", INT2FIX(-2));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, directs libcurl to use Basic authentication. */
#ifdef HAVE_CURLAUTH_BASIC
  rb_define_const(mCurl, "CURLAUTH_BASIC", INT2FIX(CURLAUTH_BASIC));
#else
  rb_define_const(mCurl, "CURLAUTH_BASIC", INT2FIX(0);
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, directs libcurl to use Digest authentication. */
#ifdef HAVE_CURLAUTH_DIGEST
  rb_define_const(mCurl, "CURLAUTH_DIGEST", INT2FIX(CURLAUTH_DIGEST));
#else
  rb_define_const(mCurl, "CURLAUTH_DIGEST", INT2FIX(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, directs libcurl to use GSS Negotiate authentication. Requires a suitable GSS-API library. */
#ifdef HAVE_CURLAUTH_GSSNEGOTIATE
  rb_define_const(mCurl, "CURLAUTH_GSSNEGOTIATE", INT2FIX(CURLAUTH_GSSNEGOTIATE));
#else
  rb_define_const(mCurl, "CURLAUTH_GSSNEGOTIATE", INT2FIX(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, directs libcurl to use HTTP NTLM authentication. Requires MS Windows or OpenSSL support. */
#ifdef HAVE_CURLAUTH_NTLM  
  rb_define_const(mCurl, "CURLAUTH_NTLM", INT2FIX(CURLAUTH_NTLM));
#else
  rb_define_const(mCurl, "CURLAUTH_NTLM", INT2FIX(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, allows libcurl to select any suitable authentication method except basic. */
#ifdef HAVE_CURLAUTH_ANYSAFE  
  rb_define_const(mCurl, "CURLAUTH_ANYSAFE", INT2FIX(CURLAUTH_ANYSAFE));
#else
  rb_define_const(mCurl, "CURLAUTH_ANYSAFE", INT2FIX(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, allows libcurl to select any suitable authentication method. */
#ifdef HAVE_CURLAUTH_ANY
  rb_define_const(mCurl, "CURLAUTH_ANY", INT2FIX(CURLAUTH_ANY));
#else
  rb_define_const(mCurl, "CURLAUTH_ANY", INT2FIX(0));
#endif
  
  rb_define_singleton_method(mCurl, "ipv6?", ruby_curl_ipv6_q, 0);
  rb_define_singleton_method(mCurl, "kerberos4?", ruby_curl_kerberos4_q, 0);
  rb_define_singleton_method(mCurl, "ssl?", ruby_curl_ssl_q, 0);
  rb_define_singleton_method(mCurl, "libz?", ruby_curl_libz_q, 0);
  rb_define_singleton_method(mCurl, "ntlm?", ruby_curl_ntlm_q, 0);
  rb_define_singleton_method(mCurl, "gssnegotiate?", ruby_curl_gssnegotiate_q, 0);
  rb_define_singleton_method(mCurl, "debug?", ruby_curl_debug_q, 0);
  rb_define_singleton_method(mCurl, "asyncdns?", ruby_curl_asyncdns_q, 0);
  rb_define_singleton_method(mCurl, "spnego?", ruby_curl_spnego_q, 0);
  rb_define_singleton_method(mCurl, "largefile?", ruby_curl_largefile_q, 0);
  rb_define_singleton_method(mCurl, "idn?", ruby_curl_idn_q, 0);
  rb_define_singleton_method(mCurl, "sspi?", ruby_curl_sspi_q, 0);
  rb_define_singleton_method(mCurl, "conv?", ruby_curl_conv_q, 0);

  init_curb_errors();
  init_curb_easy();
  init_curb_postfield();
  init_curb_multi();
}
