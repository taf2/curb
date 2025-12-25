/* Curb - Libcurl(3) bindings for Ruby.
 * Copyright (c)2006 Ross Bamford.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id: curb.c 35 2006-12-23 15:22:19Z roscopeco $
 */

#include "curb.h"
#include "curb_upload.h"

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
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);
#ifdef HAVE_CURL_VERSION_SPNEGO
  if (ver->features & CURL_VERSION_SPNEGO) return Qtrue;
#endif
#ifdef HAVE_CURL_VERSION_GSSNEGOTIATE
  if (ver->features & CURL_VERSION_GSSNEGOTIATE) return Qtrue;
#endif
  return Qfalse;
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

/*
 * call-seq:
 *   Curl.http2?                                       => true or false
 *
 * Returns true if the installed libcurl was built with support for HTTP2.
 * For libcurl versions < 7.33.0, always returns false.
 */
static VALUE ruby_curl_http2_q(VALUE mod) {
#ifdef HAVE_CURL_VERSION_HTTP2
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);
  return((ver->features & CURL_VERSION_HTTP2) ? Qtrue : Qfalse);
#else
  return Qfalse;
#endif
}

static void finalize_curb_core(VALUE data) {
  curl_global_cleanup();
}

void Init_curb_core() {
  curl_version_info_data *ver;
  VALUE curlver, curllongver, curlvernum;

  curl_global_init(CURL_GLOBAL_ALL);
  rb_set_end_proc(finalize_curb_core, Qnil);
  ver = curl_version_info(CURLVERSION_NOW);

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
  rb_define_const(mCurl, "CURLINFO_TEXT", LONG2NUM(CURLINFO_TEXT));

  /* Passed to on_debug handler to indicate that the data is header (or header-like) data received from the peer. */
  rb_define_const(mCurl, "CURLINFO_HEADER_IN", LONG2NUM(CURLINFO_HEADER_IN));

  /* Passed to on_debug handler to indicate that the data is header (or header-like) data sent to the peer. */
  rb_define_const(mCurl, "CURLINFO_HEADER_OUT", LONG2NUM(CURLINFO_HEADER_OUT));

  /* Passed to on_debug handler to indicate that the data is protocol data received from the peer. */
  rb_define_const(mCurl, "CURLINFO_DATA_IN", LONG2NUM(CURLINFO_DATA_IN));

  /* Passed to on_debug handler to indicate that the data is protocol data sent to the peer. */
  rb_define_const(mCurl, "CURLINFO_DATA_OUT", LONG2NUM(CURLINFO_DATA_OUT));

#ifdef HAVE_CURLFTPMETHOD_MULTICWD
  rb_define_const(mCurl, "CURL_MULTICWD",  LONG2NUM(CURLFTPMETHOD_MULTICWD));
#endif

#ifdef HAVE_CURLFTPMETHOD_NOCWD
  rb_define_const(mCurl, "CURL_NOCWD",     LONG2NUM(CURLFTPMETHOD_NOCWD));
#endif

#ifdef HAVE_CURLFTPMETHOD_SINGLECWD
  rb_define_const(mCurl, "CURL_SINGLECWD", LONG2NUM(CURLFTPMETHOD_SINGLECWD));
#endif

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is an HTTP proxy. (libcurl >= 7.10) */
#ifdef HAVE_CURLPROXY_HTTP
  rb_define_const(mCurl, "CURLPROXY_HTTP", LONG2NUM(CURLPROXY_HTTP));
#else
  rb_define_const(mCurl, "CURLPROXY_HTTP", LONG2NUM(-1));
#endif

#ifdef CURL_VERSION_SSL
  rb_define_const(mCurl, "CURL_SSLVERSION_DEFAULT",     LONG2NUM(CURL_SSLVERSION_DEFAULT));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_DEFAULT", LONG2NUM(CURL_SSLVERSION_MAX_DEFAULT));
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1",   LONG2NUM(CURL_SSLVERSION_TLSv1));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv2",   LONG2NUM(CURL_SSLVERSION_SSLv2));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv3",   LONG2NUM(CURL_SSLVERSION_SSLv3));
#ifdef HAVE_CURL_SSLVERSION_TLSV1_0
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_0",     LONG2NUM(CURL_SSLVERSION_TLSv1_0));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_TLSv1_0", LONG2NUM(CURL_SSLVERSION_MAX_TLSv1_0));
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSV1_1
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_1",     LONG2NUM(CURL_SSLVERSION_TLSv1_1));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_TLSv1_1", LONG2NUM(CURL_SSLVERSION_MAX_TLSv1_1));
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSV1_2
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_2",     LONG2NUM(CURL_SSLVERSION_TLSv1_2));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_TLSv1_2", LONG2NUM(CURL_SSLVERSION_MAX_TLSv1_2));
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSV1_3
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_3",     LONG2NUM(CURL_SSLVERSION_TLSv1_3));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_TLSv1_3", LONG2NUM(CURL_SSLVERSION_MAX_TLSv1_3));
#endif

  rb_define_const(mCurl, "CURL_USESSL_CONTROL", LONG2NUM(CURB_FTPSSL_CONTROL));
  rb_define_const(mCurl, "CURL_USESSL_NONE", LONG2NUM(CURB_FTPSSL_NONE));
  rb_define_const(mCurl, "CURL_USESSL_TRY", LONG2NUM(CURB_FTPSSL_TRY));
  rb_define_const(mCurl, "CURL_USESSL_ALL", LONG2NUM(CURB_FTPSSL_ALL));
#else
  rb_define_const(mCurl, "CURL_SSLVERSION_DEFAULT", LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1",   LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv2",   LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv3",   LONG2NUM(-1));
#ifdef HAVE_CURL_SSLVERSION_TLSv1_0
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_0",     LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_TLSv1_0", LONG2NUM(-1));
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSv1_1
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_1",     LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_TLSv1_1", LONG2NUM(-1));
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSv1_2
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_2",     LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_TLSv1_2", LONG2NUM(-1));
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSv1_3
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_3",     LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_MAX_TLSv1_3", LONG2NUM(-1));
#endif

  rb_define_const(mCurl, "CURL_USESSL_CONTROL", LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_USESSL_NONE", LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_USESSL_TRY", LONG2NUM(-1));
  rb_define_const(mCurl, "CURL_USESSL_ALL", LONG2NUM(-1));
#endif

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is a SOCKS4 proxy. (libcurl >= 7.15.2) */
#ifdef HAVE_CURLPROXY_SOCKS4
  rb_define_const(mCurl, "CURLPROXY_SOCKS4", LONG2NUM(CURLPROXY_SOCKS4));
#else
  rb_define_const(mCurl, "CURLPROXY_SOCKS4", LONG2NUM(-2));
#endif

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is a SOCKS4A proxy. (libcurl >= 7.18.0) */
#ifdef HAVE_CURLPROXY_SOCKS4A
  rb_define_const(mCurl, "CURLPROXY_SOCKS4A", LONG2NUM(CURLPROXY_SOCKS4A));
#else
  rb_define_const(mCurl, "CURLPROXY_SOCKS4A", LONG2NUM(-2));
#endif

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is a SOCKS5 proxy. (libcurl >= 7.10) */
#ifdef HAVE_CURLPROXY_SOCKS5
  rb_define_const(mCurl, "CURLPROXY_SOCKS5", LONG2NUM(CURLPROXY_SOCKS5));
#else
  rb_define_const(mCurl, "CURLPROXY_SOCKS5", LONG2NUM(-2));
#endif

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is a SOCKS5 proxy (and that the proxy should resolve the hostname). (libcurl >= 7.17.2) */
#ifdef HAVE_CURLPROXY_SOCKS5_HOSTNAME
  rb_define_const(mCurl, "CURLPROXY_SOCKS5_HOSTNAME", LONG2NUM(CURLPROXY_SOCKS5_HOSTNAME));
#else
  rb_define_const(mCurl, "CURLPROXY_SOCKS5_HOSTNAME", LONG2NUM(-2));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, directs libcurl to use Basic authentication. */
#ifdef HAVE_CURLAUTH_BASIC
  rb_define_const(mCurl, "CURLAUTH_BASIC", LONG2NUM(CURLAUTH_BASIC));
#else
  rb_define_const(mCurl, "CURLAUTH_BASIC", LONG2NUM(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, directs libcurl to use Digest authentication. */
#ifdef HAVE_CURLAUTH_DIGEST
  rb_define_const(mCurl, "CURLAUTH_DIGEST", LONG2NUM(CURLAUTH_DIGEST));
#else
  rb_define_const(mCurl, "CURLAUTH_DIGEST", LONG2NUM(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, directs libcurl to use GSS Negotiate authentication. Requires a suitable GSS-API library. */
#ifdef HAVE_CURLAUTH_GSSNEGOTIATE
  rb_define_const(mCurl, "CURLAUTH_GSSNEGOTIATE", LONG2NUM(CURLAUTH_GSSNEGOTIATE));
#else
  rb_define_const(mCurl, "CURLAUTH_GSSNEGOTIATE", LONG2NUM(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, directs libcurl to use HTTP NTLM authentication. Requires MS Windows or OpenSSL support. */
#ifdef HAVE_CURLAUTH_NTLM
  rb_define_const(mCurl, "CURLAUTH_NTLM", LONG2NUM(CURLAUTH_NTLM));
#else
  rb_define_const(mCurl, "CURLAUTH_NTLM", LONG2NUM(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, allows libcurl to select any suitable authentication method except basic. */
#ifdef HAVE_CURLAUTH_ANYSAFE
  rb_define_const(mCurl, "CURLAUTH_ANYSAFE", LONG2NUM(CURLAUTH_ANYSAFE));
#else
  rb_define_const(mCurl, "CURLAUTH_ANYSAFE", LONG2NUM(0));
#endif

  /* When passed to Curl::Easy#http_auth_types or Curl::Easy#proxy_auth_types, allows libcurl to select any suitable authentication method. */
#ifdef HAVE_CURLAUTH_ANY
  rb_define_const(mCurl, "CURLAUTH_ANY", LONG2NUM(CURLAUTH_ANY));
#else
  rb_define_const(mCurl, "CURLAUTH_ANY", LONG2NUM(0));
#endif

  CURB_DEFINE(CURLOPT_VERBOSE);
  CURB_DEFINE(CURLOPT_HEADER);
  CURB_DEFINE(CURLOPT_NOPROGRESS);
  CURB_DEFINE(CURLOPT_NOSIGNAL);
#ifdef HAVE_CURLOPT_PATH_AS_IS
  CURB_DEFINE(CURLOPT_PATH_AS_IS);
#endif
  CURB_DEFINE(CURLOPT_WRITEFUNCTION);
  CURB_DEFINE(CURLOPT_WRITEDATA);
  CURB_DEFINE(CURLOPT_READFUNCTION);
  CURB_DEFINE(CURLOPT_READDATA);
  /* CURLOPT_IOCTLFUNCTION/DATA deprecated since 7.18.0, use SEEKFUNCTION/DATA */
#ifdef HAVE_CURLOPT_IOCTLFUNCTION
  CURB_DEFINE(CURLOPT_IOCTLFUNCTION);
#endif
#ifdef HAVE_CURLOPT_IOCTLDATA
  CURB_DEFINE(CURLOPT_IOCTLDATA);
#endif
#ifdef HAVE_CURLOPT_SEEKFUNCTION
  CURB_DEFINE(CURLOPT_SEEKFUNCTION);
#endif
#ifdef HAVE_CURLOPT_SEEKDATA
  CURB_DEFINE(CURLOPT_SEEKDATA);
#endif
#ifdef HAVE_CURLOPT_SOCKOPTFUNCTION
  CURB_DEFINE(CURLOPT_SOCKOPTFUNCTION);
#endif
#ifdef HAVE_CURLOPT_SOCKOPTDATA
  CURB_DEFINE(CURLOPT_SOCKOPTDATA);
#endif
#ifdef HAVE_CURLOPT_OPENSOCKETFUNCTION
  CURB_DEFINE(CURLOPT_OPENSOCKETFUNCTION);
#endif
#ifdef HAVE_CURLOPT_OPENSOCKETDATA
  CURB_DEFINE(CURLOPT_OPENSOCKETDATA);
#endif
  /* CURLOPT_PROGRESSFUNCTION deprecated since 7.32.0, use XFERINFOFUNCTION */
#ifdef HAVE_CURLOPT_PROGRESSFUNCTION
  CURB_DEFINE(CURLOPT_PROGRESSFUNCTION);
#endif
#ifdef HAVE_CURLOPT_PROGRESSDATA
  CURB_DEFINE(CURLOPT_PROGRESSDATA);
#endif
#ifdef HAVE_CURLOPT_XFERINFOFUNCTION
  CURB_DEFINE(CURLOPT_XFERINFOFUNCTION);
#endif
#ifdef HAVE_CURLOPT_XFERINFODATA
  CURB_DEFINE(CURLOPT_XFERINFODATA);
#endif
  CURB_DEFINE(CURLOPT_HEADERFUNCTION);
  CURB_DEFINE(CURLOPT_WRITEHEADER);
  CURB_DEFINE(CURLOPT_DEBUGFUNCTION);
  CURB_DEFINE(CURLOPT_DEBUGDATA);
  CURB_DEFINE(CURLOPT_SSL_CTX_FUNCTION);
  CURB_DEFINE(CURLOPT_SSL_CTX_DATA);
  /* CURLOPT_CONV_* deprecated since 7.82.0, serve no purpose anymore */
#ifdef HAVE_CURLOPT_CONV_TO_NETWORK_FUNCTION
  CURB_DEFINE(CURLOPT_CONV_TO_NETWORK_FUNCTION);
#endif
#ifdef HAVE_CURLOPT_CONV_FROM_NETWORK_FUNCTION
  CURB_DEFINE(CURLOPT_CONV_FROM_NETWORK_FUNCTION);
#endif
#ifdef HAVE_CURLOPT_CONV_FROM_UTF8_FUNCTION
  CURB_DEFINE(CURLOPT_CONV_FROM_UTF8_FUNCTION);
#endif

#ifdef HAVE_CURLOPT_INTERLEAVEFUNCTION
  CURB_DEFINE(CURLOPT_INTERLEAVEFUNCTION);
#endif
#ifdef HAVE_CURLOPT_INTERLEAVEDATA
  CURB_DEFINE(CURLOPT_INTERLEAVEDATA);
#endif
#ifdef HAVE_CURLOPT_CHUNK_BGN_FUNCTION
  CURB_DEFINE(CURLOPT_CHUNK_BGN_FUNCTION);
#endif
#ifdef HAVE_CURLOPT_CHUNK_END_FUNCTION
  CURB_DEFINE(CURLOPT_CHUNK_END_FUNCTION);
#endif
#ifdef HAVE_CURLOPT_CHUNK_DATA
  CURB_DEFINE(CURLOPT_CHUNK_DATA);
#endif
#ifdef HAVE_CURLOPT_FNMATCH_FUNCTION
  CURB_DEFINE(CURLOPT_FNMATCH_FUNCTION);
#endif
#ifdef HAVE_CURLOPT_FNMATCH_DATA
  CURB_DEFINE(CURLOPT_FNMATCH_DATA);
#endif
#ifdef HAVE_CURLOPT_ERRORBUFFER
  CURB_DEFINE(CURLOPT_ERRORBUFFER);
#endif
#ifdef HAVE_CURLOPT_STDERR
  CURB_DEFINE(CURLOPT_STDERR);
#endif
#ifdef HAVE_CURLOPT_FAILONERROR
  CURB_DEFINE(CURLOPT_FAILONERROR);
#endif
  CURB_DEFINE(CURLOPT_URL);
  /* CURLOPT_PROTOCOLS deprecated since 7.85.0, use CURLOPT_PROTOCOLS_STR */
#ifdef HAVE_CURLOPT_PROTOCOLS
  CURB_DEFINE(CURLOPT_PROTOCOLS);
#endif
#ifdef HAVE_CURLOPT_PROTOCOLS_STR
  CURB_DEFINE(CURLOPT_PROTOCOLS_STR);
#endif
  /* CURLOPT_REDIR_PROTOCOLS deprecated since 7.85.0, use CURLOPT_REDIR_PROTOCOLS_STR */
#ifdef HAVE_CURLOPT_REDIR_PROTOCOLS
  CURB_DEFINE(CURLOPT_REDIR_PROTOCOLS);
#endif
#ifdef HAVE_CURLOPT_REDIR_PROTOCOLS_STR
  CURB_DEFINE(CURLOPT_REDIR_PROTOCOLS_STR);
#endif
  CURB_DEFINE(CURLOPT_PROXY);
  CURB_DEFINE(CURLOPT_PROXYPORT);
#ifdef HAVE_CURLOPT_PROXYTYPE
  CURB_DEFINE(CURLOPT_PROXYTYPE);
#endif
#ifdef HAVE_CURLOPT_NOPROXY
  CURB_DEFINE(CURLOPT_NOPROXY);
#endif
  CURB_DEFINE(CURLOPT_HTTPPROXYTUNNEL);
  /* CURLOPT_SOCKS5_GSSAPI_SERVICE deprecated since 7.49.0, use CURLOPT_PROXY_SERVICE_NAME */
#ifdef HAVE_CURLOPT_SOCKS5_GSSAPI_SERVICE
  CURB_DEFINE(CURLOPT_SOCKS5_GSSAPI_SERVICE);
#endif
#ifdef HAVE_CURLOPT_PROXY_SERVICE_NAME
  CURB_DEFINE(CURLOPT_PROXY_SERVICE_NAME);
#endif
#ifdef HAVE_CURLOPT_SOCKS5_GSSAPI_NEC
  CURB_DEFINE(CURLOPT_SOCKS5_GSSAPI_NEC);
#endif
  CURB_DEFINE(CURLOPT_INTERFACE);
#ifdef HAVE_CURLOPT_LOCALPORT
  CURB_DEFINE(CURLOPT_LOCALPORT);
#endif
  CURB_DEFINE(CURLOPT_DNS_CACHE_TIMEOUT);
  /* CURLOPT_DNS_USE_GLOBAL_CACHE deprecated since 7.11.1, does nothing since 7.62.0 */
#ifdef HAVE_CURLOPT_DNS_USE_GLOBAL_CACHE
  CURB_DEFINE(CURLOPT_DNS_USE_GLOBAL_CACHE);
#endif
  CURB_DEFINE(CURLOPT_BUFFERSIZE);
  CURB_DEFINE(CURLOPT_PORT);
  CURB_DEFINE(CURLOPT_TCP_NODELAY);
#ifdef HAVE_CURLOPT_ADDRESS_SCOPE
  CURB_DEFINE(CURLOPT_ADDRESS_SCOPE);
#endif
  CURB_DEFINE(CURLOPT_NETRC);
    CURB_DEFINE(CURL_NETRC_OPTIONAL);
    CURB_DEFINE(CURL_NETRC_IGNORED);
    CURB_DEFINE(CURL_NETRC_REQUIRED);
#ifdef HAVE_CURLOPT_NETRC_FILE
  CURB_DEFINE(CURLOPT_NETRC_FILE);
#endif
  CURB_DEFINE(CURLOPT_USERPWD);
  CURB_DEFINE(CURLOPT_PROXYUSERPWD);
#ifdef HAVE_CURLOPT_USERNAME
  CURB_DEFINE(CURLOPT_USERNAME);
#endif
#ifdef HAVE_CURLOPT_PASSWORD
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif
#ifdef HAVE_CURLOPT_PROXYUSERNAME
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif
#ifdef HAVE_CURLOPT_PROXYPASSWORD
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif

#ifdef HAVE_CURLOPT_HTTPAUTH
  CURB_DEFINE(CURLOPT_HTTPAUTH);
#endif
#ifdef HAVE_CURLAUTH_DIGEST_IE
    CURB_DEFINE(CURLAUTH_DIGEST_IE);
#endif
#ifdef HAVE_CURLAUTH_ONLY
    CURB_DEFINE(CURLAUTH_ONLY);
#endif
#ifdef HAVE_CURLOPT_TLSAUTH_TYPE
  CURB_DEFINE(CURLOPT_TLSAUTH_TYPE);
#endif
#ifdef HAVE_CURLOPT_TLSAUTH_SRP
  CURB_DEFINE(CURLOPT_TLSAUTH_SRP);
#endif
#ifdef HAVE_CURLOPT_TLSAUTH_USERNAME
  CURB_DEFINE(CURLOPT_TLSAUTH_USERNAME);
#endif
#ifdef HAVE_CURLOPT_TLSAUTH_PASSWORD
  CURB_DEFINE(CURLOPT_TLSAUTH_PASSWORD);
#endif
#ifdef HAVE_CURLOPT_PROXYAUTH
  CURB_DEFINE(CURLOPT_PROXYAUTH);
#endif
#ifdef HAVE_CURLOPT_AUTOREFERER
  CURB_DEFINE(CURLOPT_AUTOREFERER);
#endif
#ifdef HAVE_CURLOPT_ENCODING
  CURB_DEFINE(CURLOPT_ENCODING);
#endif
#ifdef HAVE_CURLOPT_FOLLOWLOCATION
  CURB_DEFINE(CURLOPT_FOLLOWLOCATION);
#endif
#ifdef HAVE_CURLOPT_UNRESTRICTED_AUTH
  CURB_DEFINE(CURLOPT_UNRESTRICTED_AUTH);
#endif
#ifdef HAVE_CURLOPT_MAXREDIRS
  CURB_DEFINE(CURLOPT_MAXREDIRS);
#endif
#ifdef HAVE_CURLOPT_POSTREDIR
  CURB_DEFINE(CURLOPT_POSTREDIR);
#endif
  /* CURLOPT_PUT deprecated since 7.12.1, use CURLOPT_UPLOAD */
#ifdef HAVE_CURLOPT_PUT
  CURB_DEFINE(CURLOPT_PUT);
#endif
#ifdef HAVE_CURLOPT_POST
  CURB_DEFINE(CURLOPT_POST);
#endif
  CURB_DEFINE(CURLOPT_POSTFIELDS);
  CURB_DEFINE(CURLOPT_POSTFIELDSIZE);
#ifdef HAVE_CURLOPT_POSTFIELDSIZE_LARGE
  CURB_DEFINE(CURLOPT_POSTFIELDSIZE_LARGE);
#endif
#ifdef HAVE_CURLOPT_COPYPOSTFIELDS
  CURB_DEFINE(CURLOPT_COPYPOSTFIELDS);
#endif
  /* CURLOPT_HTTPPOST deprecated since 7.56.0, use CURLOPT_MIMEPOST */
#ifdef HAVE_CURLOPT_HTTPPOST
  CURB_DEFINE(CURLOPT_HTTPPOST);
#endif
#ifdef HAVE_CURLOPT_MIMEPOST
  CURB_DEFINE(CURLOPT_MIMEPOST);
#endif
  CURB_DEFINE(CURLOPT_REFERER);
  CURB_DEFINE(CURLOPT_USERAGENT);
  CURB_DEFINE(CURLOPT_HTTPHEADER);
#ifdef HAVE_CURLOPT_PROXYHEADER
  CURB_DEFINE(CURLOPT_PROXYHEADER);
#endif
#ifdef HAVE_CURLOPT_REQUEST_TARGET
  /* Allows overriding the Request-URI target used in the request line.
   * Useful for absolute-form requests or special targets like "*". */
  CURB_DEFINE(CURLOPT_REQUEST_TARGET);
#endif
#ifdef HAVE_CURLOPT_HTTP200ALIASES
  CURB_DEFINE(CURLOPT_HTTP200ALIASES);
#endif

  CURB_DEFINE(CURLOPT_COOKIE);
  CURB_DEFINE(CURLOPT_COOKIEFILE);
  CURB_DEFINE(CURLOPT_COOKIEJAR);

#ifdef HAVE_CURLOPT_COOKIESESSION
  CURB_DEFINE(CURLOPT_COOKIESESSION);
#endif
#ifdef HAVE_CURLOPT_COOKIELIST
  CURB_DEFINE(CURLOPT_COOKIELIST);
#endif
#ifdef HAVE_CURLOPT_HTTPGET
  CURB_DEFINE(CURLOPT_HTTPGET);
#endif
  CURB_DEFINE(CURLOPT_HTTP_VERSION);
    CURB_DEFINE(CURL_HTTP_VERSION_NONE);
    CURB_DEFINE(CURL_HTTP_VERSION_1_0);
    CURB_DEFINE(CURL_HTTP_VERSION_1_1);
#if LIBCURL_VERSION_NUM >= 0x072100 /* 7.33.0 */
    CURB_DEFINE(CURL_HTTP_VERSION_2_0);
#endif
#if LIBCURL_VERSION_NUM >= 0x072f00 /* 7.47.0 */
    CURB_DEFINE(CURL_HTTP_VERSION_2TLS);
#endif
#ifdef HAVE_CURLOPT_IGNORE_CONTENT_LENGTH
  CURB_DEFINE(CURLOPT_IGNORE_CONTENT_LENGTH);
#endif
#ifdef HAVE_CURLOPT_HTTP_CONTENT_DECODING
  CURB_DEFINE(CURLOPT_HTTP_CONTENT_DECODING);
#endif
#ifdef HAVE_CURLOPT_HTTP_TRANSFER_DECODING
  CURB_DEFINE(CURLOPT_HTTP_TRANSFER_DECODING);
#endif
#ifdef HAVE_CURLOPT_MAIL_FROM
  CURB_DEFINE(CURLOPT_MAIL_FROM);
#endif
#ifdef HAVE_CURLOPT_MAIL_RCPT
  CURB_DEFINE(CURLOPT_MAIL_RCPT);
#endif
#ifdef HAVE_CURLOPT_TFTP_BLKSIZE
  CURB_DEFINE(CURLOPT_TFTP_BLKSIZE);
#endif
#ifdef HAVE_CURLOPT_FTPPORT
  CURB_DEFINE(CURLOPT_FTPPORT);
#endif
#ifdef HAVE_CURLOPT_QUOTE
  CURB_DEFINE(CURLOPT_QUOTE);
#endif
#ifdef HAVE_CURLOPT_POSTQUOTE
  CURB_DEFINE(CURLOPT_POSTQUOTE);
#endif
#ifdef HAVE_CURLOPT_PREQUOTE
  CURB_DEFINE(CURLOPT_PREQUOTE);
#endif
#ifdef HAVE_CURLOPT_DIRLISTONLY
  CURB_DEFINE(CURLOPT_DIRLISTONLY);
#endif
#ifdef HAVE_CURLOPT_APPEND
  CURB_DEFINE(CURLOPT_APPEND);
#endif
#ifdef HAVE_CURLOPT_FTP_USE_EPRT
  CURB_DEFINE(CURLOPT_FTP_USE_EPRT);
#endif
#ifdef HAVE_CURLOPT_FTP_USE_EPSV
  CURB_DEFINE(CURLOPT_FTP_USE_EPSV);
#endif
#ifdef HAVE_CURLOPT_FTP_USE_PRET
  CURB_DEFINE(CURLOPT_FTP_USE_PRET);
#endif
#ifdef HAVE_CURLOPT_FTP_CREATE_MISSING_DIRS
  CURB_DEFINE(CURLOPT_FTP_CREATE_MISSING_DIRS);
#endif
#ifdef HAVE_CURLOPT_FTP_RESPONSE_TIMEOUT
  CURB_DEFINE(CURLOPT_FTP_RESPONSE_TIMEOUT);
#endif
#ifdef HAVE_CURLOPT_FTP_ALTERNATIVE_TO_USER
  CURB_DEFINE(CURLOPT_FTP_ALTERNATIVE_TO_USER);
#endif
#ifdef HAVE_CURLOPT_FTP_SKIP_PASV_IP
  CURB_DEFINE(CURLOPT_FTP_SKIP_PASV_IP);
#endif
#ifdef HAVE_CURLOPT_FTPSSLAUTH
  CURB_DEFINE(CURLOPT_FTPSSLAUTH);
#endif
#ifdef HAVE_CURLFTPAUTH_DEFAULT
  CURB_DEFINE(CURLFTPAUTH_DEFAULT);
#endif
#ifdef HAVE_CURLFTPAUTH_SSL
  CURB_DEFINE(CURLFTPAUTH_SSL);
#endif
#ifdef HAVE_CURLFTPAUTH_TLS
  CURB_DEFINE(CURLFTPAUTH_TLS);
#endif
#ifdef HAVE_CURLOPT_FTP_SSL_CCC
  CURB_DEFINE(CURLOPT_FTP_SSL_CCC);
#endif
#ifdef HAVE_CURLFTPSSL_CCC_NONE
  CURB_DEFINE(CURLFTPSSL_CCC_NONE);
#endif
#ifdef HAVE_CURLFTPSSL_CCC_PASSIVE
  CURB_DEFINE(CURLFTPSSL_CCC_PASSIVE);
#endif
#ifdef HAVE_CURLFTPSSL_CCC_ACTIVE
  CURB_DEFINE(CURLFTPSSL_CCC_ACTIVE);
#endif
#ifdef HAVE_CURLOPT_FTP_ACCOUNT
  CURB_DEFINE(CURLOPT_FTP_ACCOUNT);
#endif
#ifdef HAVE_CURLOPT_FTP_FILEMETHOD
  CURB_DEFINE(CURLOPT_FTP_FILEMETHOD);
#endif
#ifdef HAVE_CURLFTPMETHOD_MULTICWD
  CURB_DEFINE(CURLFTPMETHOD_MULTICWD);
#endif
#ifdef HAVE_CURLFTPMETHOD_NOCWD
  CURB_DEFINE(CURLFTPMETHOD_NOCWD);
#endif
#ifdef HAVE_CURLFTPMETHOD_SINGLECWD
  CURB_DEFINE(CURLFTPMETHOD_SINGLECWD);
#endif
#ifdef HAVE_CURLOPT_RTSP_REQUEST
  CURB_DEFINE(CURLOPT_RTSP_REQUEST);
#endif
#ifdef HAVE_CURL_RTSPREQ_OPTIONS
  CURB_DEFINE(CURL_RTSPREQ_OPTIONS);
#endif
#ifdef HAVE_CURL_RTSPREQ_DESCRIBE
  CURB_DEFINE(CURL_RTSPREQ_DESCRIBE);
#endif
#ifdef HAVE_CURL_RTSPREQ_ANNOUNCE
  CURB_DEFINE(CURL_RTSPREQ_ANNOUNCE);
#endif
#ifdef HAVE_CURL_RTSPREQ_SETUP
  CURB_DEFINE(CURL_RTSPREQ_SETUP);
#endif
#ifdef HAVE_CURL_RTSPREQ_PLAY
  CURB_DEFINE(CURL_RTSPREQ_PLAY);
#endif
#ifdef HAVE_CURL_RTSPREQ_PAUSE
  CURB_DEFINE(CURL_RTSPREQ_PAUSE);
#endif
#ifdef HAVE_CURL_RTSPREQ_TEARDOWN
  CURB_DEFINE(CURL_RTSPREQ_TEARDOWN);
#endif
#ifdef HAVE_CURL_RTSPREQ_GET_PARAMETER
  CURB_DEFINE(CURL_RTSPREQ_GET_PARAMETER);
#endif
#ifdef HAVE_CURL_RTSPREQ_SET_PARAMETER
  CURB_DEFINE(CURL_RTSPREQ_SET_PARAMETER);
#endif
#ifdef HAVE_CURL_RTSPREQ_RECORD
  CURB_DEFINE(CURL_RTSPREQ_RECORD);
#endif
#ifdef HAVE_CURL_RTSPREQ_RECEIVE
  CURB_DEFINE(CURL_RTSPREQ_RECEIVE);
#endif
#ifdef HAVE_CURLOPT_RTSP_SESSION_ID
  CURB_DEFINE(CURLOPT_RTSP_SESSION_ID);
#endif
#ifdef HAVE_CURLOPT_RTSP_STREAM_URI
  CURB_DEFINE(CURLOPT_RTSP_STREAM_URI);
#endif
#ifdef HAVE_CURLOPT_RTSP_TRANSPORT
  CURB_DEFINE(CURLOPT_RTSP_TRANSPORT);
#endif
#ifdef HAVE_CURLOPT_RTSP_HEADER
  CURB_DEFINE(CURLOPT_RTSP_HEADER);
#endif
#ifdef HAVE_CURLOPT_RTSP_CLIENT_CSEQ
  CURB_DEFINE(CURLOPT_RTSP_CLIENT_CSEQ);
#endif
#ifdef HAVE_CURLOPT_RTSP_SERVER_CSEQ
  CURB_DEFINE(CURLOPT_RTSP_SERVER_CSEQ);
#endif

  CURB_DEFINE(CURLOPT_TRANSFERTEXT);
#ifdef HAVE_CURLOPT_PROXY_TRANSFER_MODE
  CURB_DEFINE(CURLOPT_PROXY_TRANSFER_MODE);
#endif
#ifdef HAVE_CURLOPT_CRLF
  CURB_DEFINE(CURLOPT_CRLF);
#endif
#ifdef HAVE_CURLOPT_RANGE
  CURB_DEFINE(CURLOPT_RANGE);
#endif
#ifdef HAVE_CURLOPT_RESUME_FROM
  CURB_DEFINE(CURLOPT_RESUME_FROM);
#endif
#ifdef HAVE_CURLOPT_RESUME_FROM_LARGE
  CURB_DEFINE(CURLOPT_RESUME_FROM_LARGE);
#endif
#ifdef HAVE_CURLOPT_CUSTOMREQUEST
  CURB_DEFINE(CURLOPT_CUSTOMREQUEST);
#endif
#ifdef HAVE_CURLOPT_FILETIME
  CURB_DEFINE(CURLOPT_FILETIME);
#endif
#ifdef HAVE_CURLOPT_NOBODY
  CURB_DEFINE(CURLOPT_NOBODY);
#endif
#ifdef HAVE_CURLOPT_INFILESIZE
  CURB_DEFINE(CURLOPT_INFILESIZE);
#endif
#ifdef HAVE_CURLOPT_INFILESIZE_LARGE
  CURB_DEFINE(CURLOPT_INFILESIZE_LARGE);
#endif
#ifdef HAVE_CURLOPT_UPLOAD
  CURB_DEFINE(CURLOPT_UPLOAD);
#endif
#ifdef HAVE_CURLOPT_MAXFILESIZE
  CURB_DEFINE(CURLOPT_MAXFILESIZE);
#endif
#ifdef HAVE_CURLOPT_MAXFILESIZE_LARGE
  CURB_DEFINE(CURLOPT_MAXFILESIZE_LARGE);
#endif
#ifdef HAVE_CURLOPT_TIMECONDITION
  CURB_DEFINE(CURLOPT_TIMECONDITION);
#endif
#ifdef HAVE_CURLOPT_TIMEVALUE
  CURB_DEFINE(CURLOPT_TIMEVALUE);
#endif

#ifdef HAVE_CURLOPT_TIMEOUT
  CURB_DEFINE(CURLOPT_TIMEOUT);
#endif
#ifdef HAVE_CURLOPT_TIMEOUT_MS
  CURB_DEFINE(CURLOPT_TIMEOUT_MS);
#endif
#ifdef HAVE_CURLOPT_LOW_SPEED_LIMIT
  CURB_DEFINE(CURLOPT_LOW_SPEED_LIMIT);
#endif
#ifdef HAVE_CURLOPT_LOW_SPEED_TIME
  CURB_DEFINE(CURLOPT_LOW_SPEED_TIME);
#endif
#ifdef HAVE_CURLOPT_MAX_SEND_SPEED_LARGE
  CURB_DEFINE(CURLOPT_MAX_SEND_SPEED_LARGE);
#endif
#ifdef HAVE_CURLOPT_MAX_RECV_SPEED_LARGE
  CURB_DEFINE(CURLOPT_MAX_RECV_SPEED_LARGE);
#endif
#ifdef HAVE_CURLOPT_MAXCONNECTS
  CURB_DEFINE(CURLOPT_MAXCONNECTS);
#endif
#ifdef HAVE_CURLOPT_CLOSEPOLICY
  CURB_DEFINE(CURLOPT_CLOSEPOLICY);
#endif
#ifdef HAVE_CURLOPT_FRESH_CONNECT
  CURB_DEFINE(CURLOPT_FRESH_CONNECT);
#endif
#ifdef HAVE_CURLOPT_FORBID_REUSE
  CURB_DEFINE(CURLOPT_FORBID_REUSE);
#endif
#ifdef HAVE_CURLOPT_CONNECTTIMEOUT
  CURB_DEFINE(CURLOPT_CONNECTTIMEOUT);
#endif
#ifdef HAVE_CURLOPT_CONNECTTIMEOUT_MS
  CURB_DEFINE(CURLOPT_CONNECTTIMEOUT_MS);
#endif
#ifdef HAVE_CURLOPT_IPRESOLVE
  CURB_DEFINE(CURLOPT_IPRESOLVE);
#endif
#ifdef HAVE_CURL_IPRESOLVE_WHATEVER
  CURB_DEFINE(CURL_IPRESOLVE_WHATEVER);
#endif
#ifdef HAVE_CURL_IPRESOLVE_V4
  CURB_DEFINE(CURL_IPRESOLVE_V4);
#endif
#ifdef HAVE_CURL_IPRESOLVE_V6
  CURB_DEFINE(CURL_IPRESOLVE_V6);
#endif
#ifdef HAVE_CURLOPT_CONNECT_ONLY
  CURB_DEFINE(CURLOPT_CONNECT_ONLY);
#endif
#ifdef HAVE_CURLOPT_USE_SSL
  CURB_DEFINE(CURLOPT_USE_SSL);
#endif
#ifdef HAVE_CURLUSESSL_NONE
  CURB_DEFINE(CURLUSESSL_NONE);
#endif
#ifdef HAVE_CURLUSESSL_TRY
  CURB_DEFINE(CURLUSESSL_TRY);
#endif
#ifdef HAVE_CURLUSESSL_CONTROL
  CURB_DEFINE(CURLUSESSL_CONTROL);
#endif
#ifdef HAVE_CURLUSESSL_ALL
  CURB_DEFINE(CURLUSESSL_ALL);
#endif
#ifdef HAVE_CURLOPT_RESOLVE
  CURB_DEFINE(CURLOPT_RESOLVE);
#endif

#ifdef HAVE_CURLOPT_SSLCERT
  CURB_DEFINE(CURLOPT_SSLCERT);
#endif
#ifdef HAVE_CURLOPT_SSLCERTTYPE
  CURB_DEFINE(CURLOPT_SSLCERTTYPE);
#endif
#ifdef HAVE_CURLOPT_SSLKEY
  CURB_DEFINE(CURLOPT_SSLKEY);
#endif
#ifdef HAVE_CURLOPT_SSLKEYTYPE
  CURB_DEFINE(CURLOPT_SSLKEYTYPE);
#endif
#ifdef HAVE_CURLOPT_KEYPASSWD
  CURB_DEFINE(CURLOPT_KEYPASSWD);
#endif
#ifdef HAVE_CURLOPT_SSLENGINE
  CURB_DEFINE(CURLOPT_SSLENGINE);
#endif
#ifdef HAVE_CURLOPT_SSLENGINE_DEFAULT
  CURB_DEFINE(CURLOPT_SSLENGINE_DEFAULT);
#endif
#ifdef HAVE_CURLOPT_SSLVERSION
  CURB_DEFINE(CURLOPT_SSLVERSION);
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSv1
  CURB_DEFINE(CURL_SSLVERSION_TLSv1);
#endif
#ifdef HAVE_CURL_SSLVERSION_SSLv2
  CURB_DEFINE(CURL_SSLVERSION_SSLv2);
#endif
#ifdef HAVE_CURL_SSLVERSION_SSLv3
  CURB_DEFINE(CURL_SSLVERSION_SSLv3);
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSv1_0
  CURB_DEFINE(CURL_SSLVERSION_TLSv1_0);
  CURB_DEFINE(CURL_SSLVERSION_MAX_TLSv1_0);
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSv1_1
  CURB_DEFINE(CURL_SSLVERSION_TLSv1_1);
  CURB_DEFINE(CURL_SSLVERSION_MAX_TLSv1_1);
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSv1_2
  CURB_DEFINE(CURL_SSLVERSION_TLSv1_2);
  CURB_DEFINE(CURL_SSLVERSION_MAX_TLSv1_2);
#endif
#ifdef HAVE_CURL_SSLVERSION_TLSv1_3
  CURB_DEFINE(CURL_SSLVERSION_TLSv1_3);
  CURB_DEFINE(CURL_SSLVERSION_MAX_TLSv1_3);
#endif
#ifdef HAVE_CURLOPT_SSL_VERIFYPEER
  CURB_DEFINE(CURLOPT_SSL_VERIFYPEER);
#endif
#ifdef HAVE_CURLOPT_CAINFO
  CURB_DEFINE(CURLOPT_CAINFO);
#endif
#ifdef HAVE_CURLOPT_ISSUERCERT
  CURB_DEFINE(CURLOPT_ISSUERCERT);
#endif
#ifdef HAVE_CURLOPT_CAPATH
  CURB_DEFINE(CURLOPT_CAPATH);
#endif
#ifdef HAVE_CURLOPT_CRLFILE
  CURB_DEFINE(CURLOPT_CRLFILE);
#endif
#ifdef HAVE_CURLOPT_SSL_VERIFYHOST
  CURB_DEFINE(CURLOPT_SSL_VERIFYHOST);
#endif
#ifdef HAVE_CURLOPT_CERTINFO
  CURB_DEFINE(CURLOPT_CERTINFO);
#endif
  /* CURLOPT_RANDOM_FILE deprecated since 7.84.0, serves no purpose */
#ifdef HAVE_CURLOPT_RANDOM_FILE
  CURB_DEFINE(CURLOPT_RANDOM_FILE);
#endif
  /* CURLOPT_EGDSOCKET deprecated since 7.84.0, serves no purpose */
#ifdef HAVE_CURLOPT_EGDSOCKET
  CURB_DEFINE(CURLOPT_EGDSOCKET);
#endif
#ifdef HAVE_CURLOPT_SSL_CIPHER_LIST
  CURB_DEFINE(CURLOPT_SSL_CIPHER_LIST);
#endif
#ifdef HAVE_CURLOPT_SSL_SESSIONID_CACHE
  CURB_DEFINE(CURLOPT_SSL_SESSIONID_CACHE);
#endif
#ifdef HAVE_CURLOPT_KRBLEVEL
  CURB_DEFINE(CURLOPT_KRBLEVEL);
#endif

#ifdef HAVE_CURLOPT_SSH_AUTH_TYPES
  CURB_DEFINE(CURLOPT_SSH_AUTH_TYPES);
#endif
#ifdef HAVE_CURLOPT_SSH_HOST_PUBLIC_KEY_MD5
  CURB_DEFINE(CURLOPT_SSH_HOST_PUBLIC_KEY_MD5);
#endif
#ifdef HAVE_CURLOPT_SSH_PUBLIC_KEYFILE
  CURB_DEFINE(CURLOPT_SSH_PUBLIC_KEYFILE);
#endif
#ifdef HAVE_CURLOPT_SSH_PRIVATE_KEYFILE
  CURB_DEFINE(CURLOPT_SSH_PRIVATE_KEYFILE);
#endif
#ifdef HAVE_CURLOPT_SSH_KNOWNHOSTS
  CURB_DEFINE(CURLOPT_SSH_KNOWNHOSTS);
#endif
#ifdef HAVE_CURLOPT_SSH_KEYFUNCTION
  CURB_DEFINE(CURLOPT_SSH_KEYFUNCTION);
#endif
#ifdef HAVE_CURLKHSTAT_FINE_ADD_TO_FILE
  CURB_DEFINE(CURLKHSTAT_FINE_ADD_TO_FILE);
#endif
#ifdef HAVE_CURLKHSTAT_FINE
  CURB_DEFINE(CURLKHSTAT_FINE);
#endif
#ifdef HAVE_CURLKHSTAT_REJECT
  CURB_DEFINE(CURLKHSTAT_REJECT);
#endif
#ifdef HAVE_CURLKHSTAT_DEFER
  CURB_DEFINE(CURLKHSTAT_DEFER);
#endif
#ifdef HAVE_CURLOPT_SSH_KEYDATA
  CURB_DEFINE(CURLOPT_SSH_KEYDATA);
#endif

#ifdef HAVE_CURLOPT_PRIVATE
  CURB_DEFINE(CURLOPT_PRIVATE);
#endif
#ifdef HAVE_CURLOPT_SHARE
  CURB_DEFINE(CURLOPT_SHARE);
#endif
#ifdef HAVE_CURLOPT_NEW_FILE_PERMS
  CURB_DEFINE(CURLOPT_NEW_FILE_PERMS);
#endif
#ifdef HAVE_CURLOPT_NEW_DIRECTORY_PERMS
  CURB_DEFINE(CURLOPT_NEW_DIRECTORY_PERMS);
#endif

#ifdef HAVE_CURLOPT_TELNETOPTIONS
  CURB_DEFINE(CURLOPT_TELNETOPTIONS);
#endif

#ifdef HAVE_CURLOPT_GSSAPI_DELEGATION
  CURB_DEFINE(CURLOPT_GSSAPI_DELEGATION);
#endif

#ifdef HAVE_CURLGSSAPI_DELEGATION_FLAG
  CURB_DEFINE(CURLGSSAPI_DELEGATION_FLAG);
#endif

#ifdef HAVE_CURLGSSAPI_DELEGATION_POLICY_FLAG
  CURB_DEFINE(CURLGSSAPI_DELEGATION_POLICY_FLAG);
#endif

#ifdef HAVE_CURLOPT_UNIX_SOCKET_PATH
  CURB_DEFINE(CURLOPT_UNIX_SOCKET_PATH);
#endif

#ifdef HAVE_CURLOPT_PIPEWAIT
  CURB_DEFINE(CURLOPT_PIPEWAIT);
#endif

#ifdef HAVE_CURLOPT_TCP_KEEPALIVE
  CURB_DEFINE(CURLOPT_TCP_KEEPALIVE);
  CURB_DEFINE(CURLOPT_TCP_KEEPIDLE);
  CURB_DEFINE(CURLOPT_TCP_KEEPINTVL);
#endif

#ifdef HAVE_CURLOPT_HAPROXYPROTOCOL
  CURB_DEFINE(CURLOPT_HAPROXYPROTOCOL);
#endif

#ifdef HAVE_CURLOPT_PROXY_SSL_VERIFYHOST
  CURB_DEFINE(CURLOPT_PROXY_SSL_VERIFYHOST);
#endif

#ifdef HAVE_CURLPROTO_RTMPTE
  CURB_DEFINE(CURLPROTO_RTMPTE);
#endif

#ifdef HAVE_CURLPROTO_RTMPTS
  CURB_DEFINE(CURLPROTO_RTMPTS);
#endif

#ifdef HAVE_CURLPROTO_SMBS
  CURB_DEFINE(CURLPROTO_SMBS);
#endif

#ifdef HAVE_CURLPROTO_LDAP
  CURB_DEFINE(CURLPROTO_LDAP);
#endif

#ifdef HAVE_CURLPROTO_FTP
  CURB_DEFINE(CURLPROTO_FTP);
#endif

#ifdef HAVE_CURLPROTO_SMTPS
  CURB_DEFINE(CURLPROTO_SMTPS);
#endif

#ifdef HAVE_CURLPROTO_HTTP
  CURB_DEFINE(CURLPROTO_HTTP);
#endif

#ifdef HAVE_CURLPROTO_SMTP
  CURB_DEFINE(CURLPROTO_SMTP);
#endif

#ifdef HAVE_CURLPROTO_TFTP
  CURB_DEFINE(CURLPROTO_TFTP);
#endif

#ifdef HAVE_CURLPROTO_LDAPS
  CURB_DEFINE(CURLPROTO_LDAPS);
#endif

#ifdef HAVE_CURLPROTO_IMAPS
  CURB_DEFINE(CURLPROTO_IMAPS);
#endif

#ifdef HAVE_CURLPROTO_SCP
  CURB_DEFINE(CURLPROTO_SCP);
#endif

#ifdef HAVE_CURLPROTO_SFTP
  CURB_DEFINE(CURLPROTO_SFTP);
#endif

#ifdef HAVE_CURLPROTO_TELNET
  CURB_DEFINE(CURLPROTO_TELNET);
#endif

#ifdef HAVE_CURLPROTO_FILE
  CURB_DEFINE(CURLPROTO_FILE);
#endif

#ifdef HAVE_CURLPROTO_FTPS
  CURB_DEFINE(CURLPROTO_FTPS);
#endif

#ifdef HAVE_CURLPROTO_HTTPS
  CURB_DEFINE(CURLPROTO_HTTPS);
#endif

#ifdef HAVE_CURLPROTO_IMAP
  CURB_DEFINE(CURLPROTO_IMAP);
#endif

#ifdef HAVE_CURLPROTO_POP3
  CURB_DEFINE(CURLPROTO_POP3);
#endif

#ifdef HAVE_CURLPROTO_GOPHER
  CURB_DEFINE(CURLPROTO_GOPHER);
#endif

#ifdef HAVE_CURLPROTO_DICT
  CURB_DEFINE(CURLPROTO_DICT);
#endif

#ifdef HAVE_CURLPROTO_SMB
  CURB_DEFINE(CURLPROTO_SMB);
#endif

#ifdef HAVE_CURLPROTO_RTMP
  CURB_DEFINE(CURLPROTO_RTMP);
#endif

#ifdef HAVE_CURLPROTO_ALL
  CURB_DEFINE(CURLPROTO_ALL);
#endif

#ifdef HAVE_CURLPROTO_RTMPE
  CURB_DEFINE(CURLPROTO_RTMPE);
#endif

#ifdef HAVE_CURLPROTO_RTMPS
  CURB_DEFINE(CURLPROTO_RTMPS);
#endif

#ifdef HAVE_CURLPROTO_RTMPT
  CURB_DEFINE(CURLPROTO_RTMPT);
#endif

#ifdef HAVE_CURLPROTO_POP3S
  CURB_DEFINE(CURLPROTO_POP3S);
#endif

#ifdef HAVE_CURLPROTO_RTSP
  CURB_DEFINE(CURLPROTO_RTSP);
#endif

#if LIBCURL_VERSION_NUM >= 0x072B00 /* 7.43.0 */
  CURB_DEFINE(CURLPIPE_NOTHING);
  CURB_DEFINE(CURLPIPE_HTTP1);
  CURB_DEFINE(CURLPIPE_MULTIPLEX);

  rb_define_const(mCurl, "PIPE_NOTHING", LONG2NUM(CURLPIPE_NOTHING));
  rb_define_const(mCurl, "PIPE_HTTP1", LONG2NUM(CURLPIPE_HTTP1));
  rb_define_const(mCurl, "PIPE_MULTIPLEX", LONG2NUM(CURLPIPE_MULTIPLEX));
#endif

#if LIBCURL_VERSION_NUM >= 0x072100 /* 7.33.0 */
  rb_define_const(mCurl, "HTTP_2_0", LONG2NUM(CURL_HTTP_VERSION_2_0));
#endif
  rb_define_const(mCurl, "HTTP_1_1", LONG2NUM(CURL_HTTP_VERSION_1_1));
  rb_define_const(mCurl, "HTTP_1_0", LONG2NUM(CURL_HTTP_VERSION_1_0));
  rb_define_const(mCurl, "HTTP_NONE", LONG2NUM(CURL_HTTP_VERSION_NONE));



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
  rb_define_singleton_method(mCurl, "http2?", ruby_curl_http2_q, 0);

  init_curb_errors();
  init_curb_easy();
  init_curb_postfield();
  init_curb_multi();
  init_curb_upload();
}
