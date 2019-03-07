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

void Init_curb_core() {
  // TODO we need to call curl_global_cleanup at exit!
  curl_version_info_data *ver;
  VALUE curlver, curllongver, curlvernum;

  curl_global_init(CURL_GLOBAL_ALL);
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
  rb_define_const(mCurl, "CURL_SSLVERSION_DEFAULT", LONG2NUM(CURL_SSLVERSION_DEFAULT));
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1",   LONG2NUM(CURL_SSLVERSION_TLSv1));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv2",   LONG2NUM(CURL_SSLVERSION_SSLv2));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv3",   LONG2NUM(CURL_SSLVERSION_SSLv3));
#if HAVE_CURL_SSLVERSION_TLSV1_0
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_0",   LONG2NUM(CURL_SSLVERSION_TLSv1_0));
#endif
#if HAVE_CURL_SSLVERSION_TLSV1_1
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_1",   LONG2NUM(CURL_SSLVERSION_TLSv1_1));
#endif
#if HAVE_CURL_SSLVERSION_TLSV1_2
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_2",   LONG2NUM(CURL_SSLVERSION_TLSv1_2));
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
#if HAVE_CURL_SSLVERSION_TLSv1_0
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_0", LONG2NUM(-1));
#endif
#if HAVE_CURL_SSLVERSION_TLSv1_1
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_1", LONG2NUM(-1));
#endif
#if HAVE_CURL_SSLVERSION_TLSv1_2
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1_2", LONG2NUM(-1));
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
#if HAVE_CURLOPT_PATH_AS_IS
  CURB_DEFINE(CURLOPT_PATH_AS_IS);
#endif
  CURB_DEFINE(CURLOPT_WRITEFUNCTION);
  CURB_DEFINE(CURLOPT_WRITEDATA);
  CURB_DEFINE(CURLOPT_READFUNCTION);
  CURB_DEFINE(CURLOPT_READDATA);
  CURB_DEFINE(CURLOPT_IOCTLFUNCTION);
  CURB_DEFINE(CURLOPT_IOCTLDATA);
#if HAVE_CURLOPT_SEEKFUNCTION
  CURB_DEFINE(CURLOPT_SEEKFUNCTION);
#endif
#if HAVE_CURLOPT_SEEKDATA
  CURB_DEFINE(CURLOPT_SEEKDATA);
#endif
#if HAVE_CURLOPT_SOCKOPTFUNCTION
  CURB_DEFINE(CURLOPT_SOCKOPTFUNCTION);
#endif
#if HAVE_CURLOPT_SOCKOPTDATA
  CURB_DEFINE(CURLOPT_SOCKOPTDATA);
#endif
#if HAVE_CURLOPT_OPENSOCKETFUNCTION
  CURB_DEFINE(CURLOPT_OPENSOCKETFUNCTION);
#endif
#if HAVE_CURLOPT_OPENSOCKETDATA
  CURB_DEFINE(CURLOPT_OPENSOCKETDATA);
#endif
  CURB_DEFINE(CURLOPT_PROGRESSFUNCTION);
  CURB_DEFINE(CURLOPT_PROGRESSDATA);
  CURB_DEFINE(CURLOPT_HEADERFUNCTION);
  CURB_DEFINE(CURLOPT_WRITEHEADER);
  CURB_DEFINE(CURLOPT_DEBUGFUNCTION);
  CURB_DEFINE(CURLOPT_DEBUGDATA);
  CURB_DEFINE(CURLOPT_SSL_CTX_FUNCTION);
  CURB_DEFINE(CURLOPT_SSL_CTX_DATA);
  CURB_DEFINE(CURLOPT_CONV_TO_NETWORK_FUNCTION);
  CURB_DEFINE(CURLOPT_CONV_FROM_NETWORK_FUNCTION);
  CURB_DEFINE(CURLOPT_CONV_FROM_UTF8_FUNCTION);

#if HAVE_CURLOPT_INTERLEAVEFUNCTION
  CURB_DEFINE(CURLOPT_INTERLEAVEFUNCTION);
#endif
#if HAVE_CURLOPT_INTERLEAVEDATA
  CURB_DEFINE(CURLOPT_INTERLEAVEDATA);
#endif
#if HAVE_CURLOPT_CHUNK_BGN_FUNCTION
  CURB_DEFINE(CURLOPT_CHUNK_BGN_FUNCTION);
#endif
#if HAVE_CURLOPT_CHUNK_END_FUNCTION
  CURB_DEFINE(CURLOPT_CHUNK_END_FUNCTION);
#endif
#if HAVE_CURLOPT_CHUNK_DATA
  CURB_DEFINE(CURLOPT_CHUNK_DATA);
#endif
#if HAVE_CURLOPT_FNMATCH_FUNCTION
  CURB_DEFINE(CURLOPT_FNMATCH_FUNCTION);
#endif
#if HAVE_CURLOPT_FNMATCH_DATA
  CURB_DEFINE(CURLOPT_FNMATCH_DATA);
#endif
#if HAVE_CURLOPT_ERRORBUFFER
  CURB_DEFINE(CURLOPT_ERRORBUFFER);
#endif
#if HAVE_CURLOPT_STDERR
  CURB_DEFINE(CURLOPT_STDERR);
#endif
#if HAVE_CURLOPT_FAILONERROR
  CURB_DEFINE(CURLOPT_FAILONERROR);
#endif
  CURB_DEFINE(CURLOPT_URL);
#if HAVE_CURLOPT_PROTOCOLS
  CURB_DEFINE(CURLOPT_PROTOCOLS);
#endif
#if HAVE_CURLOPT_REDIR_PROTOCOLS
  CURB_DEFINE(CURLOPT_REDIR_PROTOCOLS);
#endif
  CURB_DEFINE(CURLOPT_PROXY);
  CURB_DEFINE(CURLOPT_PROXYPORT);
#if HAVE_CURLOPT_PROXYTYPE
  CURB_DEFINE(CURLOPT_PROXYTYPE);
#endif
#if HAVE_CURLOPT_NOPROXY
  CURB_DEFINE(CURLOPT_NOPROXY);
#endif
  CURB_DEFINE(CURLOPT_HTTPPROXYTUNNEL);
#if HAVE_CURLOPT_SOCKS5_GSSAPI_SERVICE
  CURB_DEFINE(CURLOPT_SOCKS5_GSSAPI_SERVICE);
#endif
#if HAVE_CURLOPT_SOCKS5_GSSAPI_NEC
  CURB_DEFINE(CURLOPT_SOCKS5_GSSAPI_NEC);
#endif
  CURB_DEFINE(CURLOPT_INTERFACE);
#if HAVE_CURLOPT_LOCALPORT
  CURB_DEFINE(CURLOPT_LOCALPORT);
#endif
  CURB_DEFINE(CURLOPT_DNS_CACHE_TIMEOUT);
  CURB_DEFINE(CURLOPT_DNS_USE_GLOBAL_CACHE);
  CURB_DEFINE(CURLOPT_BUFFERSIZE);
  CURB_DEFINE(CURLOPT_PORT);
  CURB_DEFINE(CURLOPT_TCP_NODELAY);
#if HAVE_CURLOPT_ADDRESS_SCOPE
  CURB_DEFINE(CURLOPT_ADDRESS_SCOPE);
#endif
  CURB_DEFINE(CURLOPT_NETRC);
    CURB_DEFINE(CURL_NETRC_OPTIONAL);
    CURB_DEFINE(CURL_NETRC_IGNORED);
    CURB_DEFINE(CURL_NETRC_REQUIRED);
#if HAVE_CURLOPT_NETRC_FILE
  CURB_DEFINE(CURLOPT_NETRC_FILE);
#endif
  CURB_DEFINE(CURLOPT_USERPWD);
  CURB_DEFINE(CURLOPT_PROXYUSERPWD);
#if HAVE_CURLOPT_USERNAME
  CURB_DEFINE(CURLOPT_USERNAME);
#endif
#if HAVE_CURLOPT_PASSWORD
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif
#if HAVE_CURLOPT_PROXYUSERNAME
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif
#if HAVE_CURLOPT_PROXYPASSWORD
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif

#if HAVE_CURLOPT_HTTPAUTH
  CURB_DEFINE(CURLOPT_HTTPAUTH);
#endif
#if HAVE_CURLAUTH_DIGEST_IE
    CURB_DEFINE(CURLAUTH_DIGEST_IE);
#endif
#if HAVE_CURLAUTH_ONLY
    CURB_DEFINE(CURLAUTH_ONLY);
#endif
#if HAVE_CURLOPT_TLSAUTH_TYPE
  CURB_DEFINE(CURLOPT_TLSAUTH_TYPE);
#endif
#if HAVE_CURLOPT_TLSAUTH_SRP
  CURB_DEFINE(CURLOPT_TLSAUTH_SRP);
#endif
#if HAVE_CURLOPT_TLSAUTH_USERNAME
  CURB_DEFINE(CURLOPT_TLSAUTH_USERNAME);
#endif
#if HAVE_CURLOPT_TLSAUTH_PASSWORD
  CURB_DEFINE(CURLOPT_TLSAUTH_PASSWORD);
#endif
#if HAVE_CURLOPT_PROXYAUTH
  CURB_DEFINE(CURLOPT_PROXYAUTH);
#endif
#if HAVE_CURLOPT_AUTOREFERER
  CURB_DEFINE(CURLOPT_AUTOREFERER);
#endif
#if HAVE_CURLOPT_ENCODING
  CURB_DEFINE(CURLOPT_ENCODING);
#endif
#if HAVE_CURLOPT_FOLLOWLOCATION
  CURB_DEFINE(CURLOPT_FOLLOWLOCATION);
#endif
#if HAVE_CURLOPT_UNRESTRICTED_AUTH
  CURB_DEFINE(CURLOPT_UNRESTRICTED_AUTH);
#endif
#if HAVE_CURLOPT_MAXREDIRS
  CURB_DEFINE(CURLOPT_MAXREDIRS);
#endif
#if HAVE_CURLOPT_POSTREDIR
  CURB_DEFINE(CURLOPT_POSTREDIR);
#endif
#if HAVE_CURLOPT_PUT
  CURB_DEFINE(CURLOPT_PUT);
#endif
#if HAVE_CURLOPT_POST
  CURB_DEFINE(CURLOPT_POST);
#endif
  CURB_DEFINE(CURLOPT_POSTFIELDS);
  CURB_DEFINE(CURLOPT_POSTFIELDSIZE);
#if HAVE_CURLOPT_POSTFIELDSIZE_LARGE
  CURB_DEFINE(CURLOPT_POSTFIELDSIZE_LARGE);
#endif
#if HAVE_CURLOPT_COPYPOSTFIELDS
  CURB_DEFINE(CURLOPT_COPYPOSTFIELDS);
#endif
#if HAVE_CURLOPT_HTTPPOST
  CURB_DEFINE(CURLOPT_HTTPPOST);
#endif
  CURB_DEFINE(CURLOPT_REFERER);
  CURB_DEFINE(CURLOPT_USERAGENT);
  CURB_DEFINE(CURLOPT_HTTPHEADER);
#if HAVE_CURLOPT_PROXYHEADER
  CURB_DEFINE(CURLOPT_PROXYHEADER);
#endif
#if HAVE_CURLOPT_HTTP200ALIASES
  CURB_DEFINE(CURLOPT_HTTP200ALIASES);
#endif

  CURB_DEFINE(CURLOPT_COOKIE);
  CURB_DEFINE(CURLOPT_COOKIEFILE);
  CURB_DEFINE(CURLOPT_COOKIEJAR);

#if HAVE_CURLOPT_COOKIESESSION
  CURB_DEFINE(CURLOPT_COOKIESESSION);
#endif
#if HAVE_CURLOPT_COOKIELIST
  CURB_DEFINE(CURLOPT_COOKIELIST);
#endif
#if HAVE_CURLOPT_HTTPGET
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
#if HAVE_CURLOPT_IGNORE_CONTENT_LENGTH
  CURB_DEFINE(CURLOPT_IGNORE_CONTENT_LENGTH);
#endif
#if HAVE_CURLOPT_HTTP_CONTENT_DECODING
  CURB_DEFINE(CURLOPT_HTTP_CONTENT_DECODING);
#endif
#if HAVE_CURLOPT_HTTP_TRANSFER_DECODING
  CURB_DEFINE(CURLOPT_HTTP_TRANSFER_DECODING);
#endif
#if HAVE_CURLOPT_MAIL_FROM
  CURB_DEFINE(CURLOPT_MAIL_FROM);
#endif
#if HAVE_CURLOPT_MAIL_RCPT
  CURB_DEFINE(CURLOPT_MAIL_RCPT);
#endif
#if HAVE_CURLOPT_TFTP_BLKSIZE
  CURB_DEFINE(CURLOPT_TFTP_BLKSIZE);
#endif
#if HAVE_CURLOPT_FTPPORT
  CURB_DEFINE(CURLOPT_FTPPORT);
#endif
#if HAVE_CURLOPT_QUOTE
  CURB_DEFINE(CURLOPT_QUOTE);
#endif
#if HAVE_CURLOPT_POSTQUOTE
  CURB_DEFINE(CURLOPT_POSTQUOTE);
#endif
#if HAVE_CURLOPT_PREQUOTE
  CURB_DEFINE(CURLOPT_PREQUOTE);
#endif
#if HAVE_CURLOPT_DIRLISTONLY
  CURB_DEFINE(CURLOPT_DIRLISTONLY);
#endif
#if HAVE_CURLOPT_APPEND
  CURB_DEFINE(CURLOPT_APPEND);
#endif
#if HAVE_CURLOPT_FTP_USE_EPRT
  CURB_DEFINE(CURLOPT_FTP_USE_EPRT);
#endif
#if HAVE_CURLOPT_FTP_USE_EPSV
  CURB_DEFINE(CURLOPT_FTP_USE_EPSV);
#endif
#if HAVE_CURLOPT_FTP_USE_PRET
  CURB_DEFINE(CURLOPT_FTP_USE_PRET);
#endif
#if HAVE_CURLOPT_FTP_CREATE_MISSING_DIRS
  CURB_DEFINE(CURLOPT_FTP_CREATE_MISSING_DIRS);
#endif
#if HAVE_CURLOPT_FTP_RESPONSE_TIMEOUT
  CURB_DEFINE(CURLOPT_FTP_RESPONSE_TIMEOUT);
#endif
#if HAVE_CURLOPT_FTP_ALTERNATIVE_TO_USER
  CURB_DEFINE(CURLOPT_FTP_ALTERNATIVE_TO_USER);
#endif
#if HAVE_CURLOPT_FTP_SKIP_PASV_IP
  CURB_DEFINE(CURLOPT_FTP_SKIP_PASV_IP);
#endif
#if HAVE_CURLOPT_FTPSSLAUTH
  CURB_DEFINE(CURLOPT_FTPSSLAUTH);
#endif
#if HAVE_CURLFTPAUTH_DEFAULT
  CURB_DEFINE(CURLFTPAUTH_DEFAULT);
#endif
#if HAVE_CURLFTPAUTH_SSL
  CURB_DEFINE(CURLFTPAUTH_SSL);
#endif
#if HAVE_CURLFTPAUTH_TLS
  CURB_DEFINE(CURLFTPAUTH_TLS);
#endif
#if HAVE_CURLOPT_FTP_SSL_CCC
  CURB_DEFINE(CURLOPT_FTP_SSL_CCC);
#endif
#if HAVE_CURLFTPSSL_CCC_NONE
  CURB_DEFINE(CURLFTPSSL_CCC_NONE);
#endif
#if HAVE_CURLFTPSSL_CCC_PASSIVE
  CURB_DEFINE(CURLFTPSSL_CCC_PASSIVE);
#endif
#if HAVE_CURLFTPSSL_CCC_ACTIVE
  CURB_DEFINE(CURLFTPSSL_CCC_ACTIVE);
#endif
#if HAVE_CURLOPT_FTP_ACCOUNT
  CURB_DEFINE(CURLOPT_FTP_ACCOUNT);
#endif
#if HAVE_CURLOPT_FTP_FILEMETHOD
  CURB_DEFINE(CURLOPT_FTP_FILEMETHOD);
#endif
#if HAVE_CURLFTPMETHOD_MULTICWD
  CURB_DEFINE(CURLFTPMETHOD_MULTICWD);
#endif
#if HAVE_CURLFTPMETHOD_NOCWD
  CURB_DEFINE(CURLFTPMETHOD_NOCWD);
#endif
#if HAVE_CURLFTPMETHOD_SINGLECWD
  CURB_DEFINE(CURLFTPMETHOD_SINGLECWD);
#endif
#if HAVE_CURLOPT_RTSP_REQUEST
  CURB_DEFINE(CURLOPT_RTSP_REQUEST);
#endif
#if HAVE_CURL_RTSPREQ_OPTIONS
  CURB_DEFINE(CURL_RTSPREQ_OPTIONS);
#endif
#if HAVE_CURL_RTSPREQ_DESCRIBE
  CURB_DEFINE(CURL_RTSPREQ_DESCRIBE);
#endif
#if HAVE_CURL_RTSPREQ_ANNOUNCE
  CURB_DEFINE(CURL_RTSPREQ_ANNOUNCE);
#endif
#if HAVE_CURL_RTSPREQ_SETUP
  CURB_DEFINE(CURL_RTSPREQ_SETUP);
#endif
#if HAVE_CURL_RTSPREQ_PLAY
  CURB_DEFINE(CURL_RTSPREQ_PLAY);
#endif
#if HAVE_CURL_RTSPREQ_PAUSE
  CURB_DEFINE(CURL_RTSPREQ_PAUSE);
#endif
#if HAVE_CURL_RTSPREQ_TEARDOWN
  CURB_DEFINE(CURL_RTSPREQ_TEARDOWN);
#endif
#if HAVE_CURL_RTSPREQ_GET_PARAMETER
  CURB_DEFINE(CURL_RTSPREQ_GET_PARAMETER);
#endif
#if HAVE_CURL_RTSPREQ_SET_PARAMETER
  CURB_DEFINE(CURL_RTSPREQ_SET_PARAMETER);
#endif
#if HAVE_CURL_RTSPREQ_RECORD
  CURB_DEFINE(CURL_RTSPREQ_RECORD);
#endif
#if HAVE_CURL_RTSPREQ_RECEIVE
  CURB_DEFINE(CURL_RTSPREQ_RECEIVE);
#endif
#if HAVE_CURLOPT_RTSP_SESSION_ID
  CURB_DEFINE(CURLOPT_RTSP_SESSION_ID);
#endif
#if HAVE_CURLOPT_RTSP_STREAM_URI
  CURB_DEFINE(CURLOPT_RTSP_STREAM_URI);
#endif
#if HAVE_CURLOPT_RTSP_TRANSPORT
  CURB_DEFINE(CURLOPT_RTSP_TRANSPORT);
#endif
#if HAVE_CURLOPT_RTSP_HEADER
  CURB_DEFINE(CURLOPT_RTSP_HEADER);
#endif
#if HAVE_CURLOPT_RTSP_CLIENT_CSEQ
  CURB_DEFINE(CURLOPT_RTSP_CLIENT_CSEQ);
#endif
#if HAVE_CURLOPT_RTSP_SERVER_CSEQ
  CURB_DEFINE(CURLOPT_RTSP_SERVER_CSEQ);
#endif

  CURB_DEFINE(CURLOPT_TRANSFERTEXT);
#if HAVE_CURLOPT_PROXY_TRANSFER_MODE
  CURB_DEFINE(CURLOPT_PROXY_TRANSFER_MODE);
#endif
#if HAVE_CURLOPT_CRLF
  CURB_DEFINE(CURLOPT_CRLF);
#endif
#if HAVE_CURLOPT_RANGE
  CURB_DEFINE(CURLOPT_RANGE);
#endif
#if HAVE_CURLOPT_RESUME_FROM
  CURB_DEFINE(CURLOPT_RESUME_FROM);
#endif
#if HAVE_CURLOPT_RESUME_FROM_LARGE
  CURB_DEFINE(CURLOPT_RESUME_FROM_LARGE);
#endif
#if HAVE_CURLOPT_CUSTOMREQUEST
  CURB_DEFINE(CURLOPT_CUSTOMREQUEST);
#endif
#if HAVE_CURLOPT_FILETIME
  CURB_DEFINE(CURLOPT_FILETIME);
#endif
#if HAVE_CURLOPT_NOBODY
  CURB_DEFINE(CURLOPT_NOBODY);
#endif
#if HAVE_CURLOPT_INFILESIZE
  CURB_DEFINE(CURLOPT_INFILESIZE);
#endif
#if HAVE_CURLOPT_INFILESIZE_LARGE
  CURB_DEFINE(CURLOPT_INFILESIZE_LARGE);
#endif
#if HAVE_CURLOPT_UPLOAD
  CURB_DEFINE(CURLOPT_UPLOAD);
#endif
#if HAVE_CURLOPT_MAXFILESIZE
  CURB_DEFINE(CURLOPT_MAXFILESIZE);
#endif
#if HAVE_CURLOPT_MAXFILESIZE_LARGE
  CURB_DEFINE(CURLOPT_MAXFILESIZE_LARGE);
#endif
#if HAVE_CURLOPT_TIMECONDITION
  CURB_DEFINE(CURLOPT_TIMECONDITION);
#endif
#if HAVE_CURLOPT_TIMEVALUE
  CURB_DEFINE(CURLOPT_TIMEVALUE);
#endif

#if HAVE_CURLOPT_TIMEOUT
  CURB_DEFINE(CURLOPT_TIMEOUT);
#endif
#if HAVE_CURLOPT_TIMEOUT_MS
  CURB_DEFINE(CURLOPT_TIMEOUT_MS);
#endif
#if HAVE_CURLOPT_LOW_SPEED_LIMIT
  CURB_DEFINE(CURLOPT_LOW_SPEED_LIMIT);
#endif
#if HAVE_CURLOPT_LOW_SPEED_TIME
  CURB_DEFINE(CURLOPT_LOW_SPEED_TIME);
#endif
#if HAVE_CURLOPT_MAX_SEND_SPEED_LARGE
  CURB_DEFINE(CURLOPT_MAX_SEND_SPEED_LARGE);
#endif
#if HAVE_CURLOPT_MAX_RECV_SPEED_LARGE
  CURB_DEFINE(CURLOPT_MAX_RECV_SPEED_LARGE);
#endif
#if HAVE_CURLOPT_MAXCONNECTS
  CURB_DEFINE(CURLOPT_MAXCONNECTS);
#endif
#if HAVE_CURLOPT_CLOSEPOLICY
  CURB_DEFINE(CURLOPT_CLOSEPOLICY);
#endif
#if HAVE_CURLOPT_FRESH_CONNECT
  CURB_DEFINE(CURLOPT_FRESH_CONNECT);
#endif
#if HAVE_CURLOPT_FORBID_REUSE
  CURB_DEFINE(CURLOPT_FORBID_REUSE);
#endif
#if HAVE_CURLOPT_CONNECTTIMEOUT
  CURB_DEFINE(CURLOPT_CONNECTTIMEOUT);
#endif
#if HAVE_CURLOPT_CONNECTTIMEOUT_MS
  CURB_DEFINE(CURLOPT_CONNECTTIMEOUT_MS);
#endif
#if HAVE_CURLOPT_IPRESOLVE
  CURB_DEFINE(CURLOPT_IPRESOLVE);
#endif
#if HAVE_CURL_IPRESOLVE_WHATEVER
  CURB_DEFINE(CURL_IPRESOLVE_WHATEVER);
#endif
#if HAVE_CURL_IPRESOLVE_V4
  CURB_DEFINE(CURL_IPRESOLVE_V4);
#endif
#if HAVE_CURL_IPRESOLVE_V6
  CURB_DEFINE(CURL_IPRESOLVE_V6);
#endif
#if HAVE_CURLOPT_CONNECT_ONLY
  CURB_DEFINE(CURLOPT_CONNECT_ONLY);
#endif
#if HAVE_CURLOPT_USE_SSL
  CURB_DEFINE(CURLOPT_USE_SSL);
#endif
#if HAVE_CURLUSESSL_NONE
  CURB_DEFINE(CURLUSESSL_NONE);
#endif
#if HAVE_CURLUSESSL_TRY
  CURB_DEFINE(CURLUSESSL_TRY);
#endif
#if HAVE_CURLUSESSL_CONTROL
  CURB_DEFINE(CURLUSESSL_CONTROL);
#endif
#if HAVE_CURLUSESSL_ALL
  CURB_DEFINE(CURLUSESSL_ALL);
#endif
#if HAVE_CURLOPT_RESOLVE
  CURB_DEFINE(CURLOPT_RESOLVE);
#endif

#if HAVE_CURLOPT_SSLCERT
  CURB_DEFINE(CURLOPT_SSLCERT);
#endif
#if HAVE_CURLOPT_SSLCERTTYPE
  CURB_DEFINE(CURLOPT_SSLCERTTYPE);
#endif
#if HAVE_CURLOPT_SSLKEY
  CURB_DEFINE(CURLOPT_SSLKEY);
#endif
#if HAVE_CURLOPT_SSLKEYTYPE
  CURB_DEFINE(CURLOPT_SSLKEYTYPE);
#endif
#if HAVE_CURLOPT_KEYPASSWD
  CURB_DEFINE(CURLOPT_KEYPASSWD);
#endif
#if HAVE_CURLOPT_SSLENGINE
  CURB_DEFINE(CURLOPT_SSLENGINE);
#endif
#if HAVE_CURLOPT_SSLENGINE_DEFAULT
  CURB_DEFINE(CURLOPT_SSLENGINE_DEFAULT);
#endif
#if HAVE_CURLOPT_SSLVERSION
  CURB_DEFINE(CURLOPT_SSLVERSION);
#endif
#if HAVE_CURL_SSLVERSION_TLSv1
  CURB_DEFINE(CURL_SSLVERSION_TLSv1);
#endif
#if HAVE_CURL_SSLVERSION_SSLv2
  CURB_DEFINE(CURL_SSLVERSION_SSLv2);
#endif
#if HAVE_CURL_SSLVERSION_SSLv3
  CURB_DEFINE(CURL_SSLVERSION_SSLv3);
#endif
#if HAVE_CURL_SSLVERSION_TLSv1_0
  CURB_DEFINE(CURL_SSLVERSION_TLSv1_0);
#endif
#if HAVE_CURL_SSLVERSION_TLSv1_1
  CURB_DEFINE(CURL_SSLVERSION_TLSv1_1);
#endif
#if HAVE_CURL_SSLVERSION_TLSv1_2
  CURB_DEFINE(CURL_SSLVERSION_TLSv1_2);
#endif
#if HAVE_CURLOPT_SSL_VERIFYPEER
  CURB_DEFINE(CURLOPT_SSL_VERIFYPEER);
#endif
#if HAVE_CURLOPT_CAINFO
  CURB_DEFINE(CURLOPT_CAINFO);
#endif
#if HAVE_CURLOPT_ISSUERCERT
  CURB_DEFINE(CURLOPT_ISSUERCERT);
#endif
#if HAVE_CURLOPT_CAPATH
  CURB_DEFINE(CURLOPT_CAPATH);
#endif
#if HAVE_CURLOPT_CRLFILE
  CURB_DEFINE(CURLOPT_CRLFILE);
#endif
#if HAVE_CURLOPT_SSL_VERIFYHOST
  CURB_DEFINE(CURLOPT_SSL_VERIFYHOST);
#endif
#if HAVE_CURLOPT_CERTINFO
  CURB_DEFINE(CURLOPT_CERTINFO);
#endif
#if HAVE_CURLOPT_RANDOM_FILE
  CURB_DEFINE(CURLOPT_RANDOM_FILE);
#endif
#if HAVE_CURLOPT_EGDSOCKET
  CURB_DEFINE(CURLOPT_EGDSOCKET);
#endif
#if HAVE_CURLOPT_SSL_CIPHER_LIST
  CURB_DEFINE(CURLOPT_SSL_CIPHER_LIST);
#endif
#if HAVE_CURLOPT_SSL_SESSIONID_CACHE
  CURB_DEFINE(CURLOPT_SSL_SESSIONID_CACHE);
#endif
#if HAVE_CURLOPT_KRBLEVEL
  CURB_DEFINE(CURLOPT_KRBLEVEL);
#endif

#if HAVE_CURLOPT_SSH_AUTH_TYPES
  CURB_DEFINE(CURLOPT_SSH_AUTH_TYPES);
#endif
#if HAVE_CURLOPT_SSH_HOST_PUBLIC_KEY_MD5
  CURB_DEFINE(CURLOPT_SSH_HOST_PUBLIC_KEY_MD5);
#endif
#if HAVE_CURLOPT_SSH_PUBLIC_KEYFILE
  CURB_DEFINE(CURLOPT_SSH_PUBLIC_KEYFILE);
#endif
#if HAVE_CURLOPT_SSH_PRIVATE_KEYFILE
  CURB_DEFINE(CURLOPT_SSH_PRIVATE_KEYFILE);
#endif
#if HAVE_CURLOPT_SSH_KNOWNHOSTS
  CURB_DEFINE(CURLOPT_SSH_KNOWNHOSTS);
#endif
#if HAVE_CURLOPT_SSH_KEYFUNCTION
  CURB_DEFINE(CURLOPT_SSH_KEYFUNCTION);
#endif
#if HAVE_CURLKHSTAT_FINE_ADD_TO_FILE
  CURB_DEFINE(CURLKHSTAT_FINE_ADD_TO_FILE);
#endif
#if HAVE_CURLKHSTAT_FINE
  CURB_DEFINE(CURLKHSTAT_FINE);
#endif
#if HAVE_CURLKHSTAT_REJECT
  CURB_DEFINE(CURLKHSTAT_REJECT);
#endif
#if HAVE_CURLKHSTAT_DEFER
  CURB_DEFINE(CURLKHSTAT_DEFER);
#endif
#if HAVE_CURLOPT_SSH_KEYDATA
  CURB_DEFINE(CURLOPT_SSH_KEYDATA);
#endif

#if HAVE_CURLOPT_PRIVATE
  CURB_DEFINE(CURLOPT_PRIVATE);
#endif
#if HAVE_CURLOPT_SHARE
  CURB_DEFINE(CURLOPT_SHARE);
#endif
#if HAVE_CURLOPT_NEW_FILE_PERMS
  CURB_DEFINE(CURLOPT_NEW_FILE_PERMS);
#endif
#if HAVE_CURLOPT_NEW_DIRECTORY_PERMS
  CURB_DEFINE(CURLOPT_NEW_DIRECTORY_PERMS);
#endif

#if HAVE_CURLOPT_TELNETOPTIONS
  CURB_DEFINE(CURLOPT_TELNETOPTIONS);
#endif

#if HAVE_CURLOPT_GSSAPI_DELEGATION
  CURB_DEFINE(CURLOPT_GSSAPI_DELEGATION);
#endif

#if HAVE_CURLGSSAPI_DELEGATION_FLAG
  CURB_DEFINE(CURLGSSAPI_DELEGATION_FLAG);
#endif

#if HAVE_CURLGSSAPI_DELEGATION_POLICY_FLAG
  CURB_DEFINE(CURLGSSAPI_DELEGATION_POLICY_FLAG);
#endif

#if HAVE_CURLOPT_UNIX_SOCKET_PATH
  CURB_DEFINE(CURLOPT_UNIX_SOCKET_PATH);
#endif

#if HAVE_CURLOPT_PIPEWAIT
  CURB_DEFINE(CURLOPT_PIPEWAIT);
#endif

#if HAVE_CURLOPT_TCP_KEEPALIVE
  CURB_DEFINE(CURLOPT_TCP_KEEPALIVE);
  CURB_DEFINE(CURLOPT_TCP_KEEPIDLE);
  CURB_DEFINE(CURLOPT_TCP_KEEPINTVL);
#endif

#if HAVE_CURLOPT_HAPROXYPROTOCOL
  CURB_DEFINE(CURLOPT_HAPROXYPROTOCOL);
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
