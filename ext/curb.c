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
  rb_define_const(mCurl, "CURLINFO_TEXT", INT2FIX(CURLINFO_TEXT));

  /* Passed to on_debug handler to indicate that the data is header (or header-like) data received from the peer. */
  rb_define_const(mCurl, "CURLINFO_HEADER_IN", INT2FIX(CURLINFO_HEADER_IN));

  /* Passed to on_debug handler to indicate that the data is header (or header-like) data sent to the peer. */
  rb_define_const(mCurl, "CURLINFO_HEADER_OUT", INT2FIX(CURLINFO_HEADER_OUT));

  /* Passed to on_debug handler to indicate that the data is protocol data received from the peer. */
  rb_define_const(mCurl, "CURLINFO_DATA_IN", INT2FIX(CURLINFO_DATA_IN));

  /* Passed to on_debug handler to indicate that the data is protocol data sent to the peer. */
  rb_define_const(mCurl, "CURLINFO_DATA_OUT", INT2FIX(CURLINFO_DATA_OUT));

#ifdef HAVE_CURLFTPMETHOD_MULTICWD 
  rb_define_const(mCurl, "CURL_MULTICWD",  INT2FIX(CURLFTPMETHOD_MULTICWD));
#endif

#ifdef HAVE_CURLFTPMETHOD_NOCWD 
  rb_define_const(mCurl, "CURL_NOCWD",     INT2FIX(CURLFTPMETHOD_NOCWD));
#endif

#ifdef HAVE_CURLFTPMETHOD_SINGLECWD 
  rb_define_const(mCurl, "CURL_SINGLECWD", INT2FIX(CURLFTPMETHOD_SINGLECWD));
#endif

  /* When passed to Curl::Easy#proxy_type , indicates that the proxy is an HTTP proxy. (libcurl >= 7.10) */
#ifdef HAVE_CURLPROXY_HTTP
  rb_define_const(mCurl, "CURLPROXY_HTTP", INT2FIX(CURLPROXY_HTTP));
#else
  rb_define_const(mCurl, "CURLPROXY_HTTP", INT2FIX(-1));
#endif

#ifdef CURL_VERSION_SSL
  rb_define_const(mCurl, "CURL_SSLVERSION_DEFAULT", INT2FIX(CURL_SSLVERSION_DEFAULT));
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1",   INT2FIX(CURL_SSLVERSION_TLSv1));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv2",   INT2FIX(CURL_SSLVERSION_SSLv2));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv3",   INT2FIX(CURL_SSLVERSION_SSLv3));

  rb_define_const(mCurl, "CURL_USESSL_CONTROL", INT2FIX(CURB_FTPSSL_CONTROL));
  rb_define_const(mCurl, "CURL_USESSL_NONE", INT2FIX(CURB_FTPSSL_NONE));
  rb_define_const(mCurl, "CURL_USESSL_TRY", INT2FIX(CURB_FTPSSL_TRY));
  rb_define_const(mCurl, "CURL_USESSL_ALL", INT2FIX(CURB_FTPSSL_ALL));
#else
  rb_define_const(mCurl, "CURL_SSLVERSION_DEFAULT", INT2FIX(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_TLSv1",   INT2FIX(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv2",   INT2FIX(-1));
  rb_define_const(mCurl, "CURL_SSLVERSION_SSLv3",   INT2FIX(-1));

  rb_define_const(mCurl, "CURL_USESSL_CONTROL", INT2FIX(-1));
  rb_define_const(mCurl, "CURL_USESSL_NONE", INT2FIX(-1));
  rb_define_const(mCurl, "CURL_USESSL_TRY", INT2FIX(-1));
  rb_define_const(mCurl, "CURL_USESSL_ALL", INT2FIX(-1));
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
  rb_define_const(mCurl, "CURLAUTH_BASIC", INT2FIX(0));
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

  CURB_DEFINE(CURLOPT_VERBOSE);
  CURB_DEFINE(CURLOPT_HEADER);
  CURB_DEFINE(CURLOPT_NOPROGRESS);
  CURB_DEFINE(CURLOPT_NOSIGNAL);
  CURB_DEFINE(CURLOPT_WRITEFUNCTION);
  CURB_DEFINE(CURLOPT_WRITEDATA);
  CURB_DEFINE(CURLOPT_READFUNCTION);
  CURB_DEFINE(CURLOPT_READDATA);
  CURB_DEFINE(CURLOPT_IOCTLFUNCTION);
  CURB_DEFINE(CURLOPT_IOCTLDATA);
  CURB_DEFINE(CURLOPT_SEEKFUNCTION);
  CURB_DEFINE(CURLOPT_SEEKDATA);
  CURB_DEFINE(CURLOPT_SOCKOPTFUNCTION);
  CURB_DEFINE(CURLOPT_SOCKOPTDATA);
  CURB_DEFINE(CURLOPT_OPENSOCKETFUNCTION);
  CURB_DEFINE(CURLOPT_OPENSOCKETDATA);
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

#ifdef CURLOPT_INTERLEAVEFUNCTION
  CURB_DEFINE(CURLOPT_INTERLEAVEFUNCTION);
#endif
#ifdef CURLOPT_INTERLEAVEDATA
  CURB_DEFINE(CURLOPT_INTERLEAVEDATA);
#endif
#ifdef CURLOPT_CHUNK_BGN_FUNCTION
  CURB_DEFINE(CURLOPT_CHUNK_BGN_FUNCTION);
#endif
#ifdef CURLOPT_CHUNK_END_FUNCTION
  CURB_DEFINE(CURLOPT_CHUNK_END_FUNCTION);
#endif
#ifdef CURLOPT_CHUNK_DATA
  CURB_DEFINE(CURLOPT_CHUNK_DATA);
#endif
#ifdef CURLOPT_FNMATCH_FUNCTION
  CURB_DEFINE(CURLOPT_FNMATCH_FUNCTION);
#endif
#ifdef CURLOPT_FNMATCH_DATA
  CURB_DEFINE(CURLOPT_FNMATCH_DATA);
#endif
#ifdef CURLOPT_ERRORBUFFER
  CURB_DEFINE(CURLOPT_ERRORBUFFER);
#endif
#ifdef CURLOPT_STDERR
  CURB_DEFINE(CURLOPT_STDERR);
#endif
#ifdef CURLOPT_FAILONERROR
  CURB_DEFINE(CURLOPT_FAILONERROR);
#endif
  CURB_DEFINE(CURLOPT_URL);
  CURB_DEFINE(CURLOPT_PROTOCOLS);
  CURB_DEFINE(CURLOPT_REDIR_PROTOCOLS);
  CURB_DEFINE(CURLOPT_PROXY);
  CURB_DEFINE(CURLOPT_PROXYPORT);
#ifdef CURLOPT_PROXYTYPE
  CURB_DEFINE(CURLOPT_PROXYTYPE);
#endif
#ifdef CURLOPT_NOPROXY
  CURB_DEFINE(CURLOPT_NOPROXY);
#endif
  CURB_DEFINE(CURLOPT_HTTPPROXYTUNNEL);
#ifdef CURLOPT_SOCKS5_GSSAPI_SERVICE
  CURB_DEFINE(CURLOPT_SOCKS5_GSSAPI_SERVICE);
#endif
#ifdef CURLOPT_SOCKS5_GSSAPI_NEC
  CURB_DEFINE(CURLOPT_SOCKS5_GSSAPI_NEC);
#endif
  CURB_DEFINE(CURLOPT_INTERFACE);
#ifdef CURLOPT_LOCALPORT
  CURB_DEFINE(CURLOPT_LOCALPORT);
#endif
  CURB_DEFINE(CURLOPT_DNS_CACHE_TIMEOUT);
  CURB_DEFINE(CURLOPT_DNS_USE_GLOBAL_CACHE);
  CURB_DEFINE(CURLOPT_BUFFERSIZE);
  CURB_DEFINE(CURLOPT_PORT);
  CURB_DEFINE(CURLOPT_TCP_NODELAY);
#ifdef CURLOPT_ADDRESS_SCOPE
  CURB_DEFINE(CURLOPT_ADDRESS_SCOPE);
#endif
  CURB_DEFINE(CURLOPT_NETRC);
    CURB_DEFINE(CURL_NETRC_OPTIONAL);
    CURB_DEFINE(CURL_NETRC_IGNORED);
    CURB_DEFINE(CURL_NETRC_REQUIRED);
#ifdef CURLOPT_NETRC_FILE
  CURB_DEFINE(CURLOPT_NETRC_FILE);
#endif
  CURB_DEFINE(CURLOPT_USERPWD);
  CURB_DEFINE(CURLOPT_PROXYUSERPWD);
#ifdef CURLOPT_USERNAME
  CURB_DEFINE(CURLOPT_USERNAME);
#endif
#ifdef CURLOPT_PASSWORD
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif
#ifdef CURLOPT_PROXYUSERNAME
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif
#ifdef CURLOPT_PROXYPASSWORD
  CURB_DEFINE(CURLOPT_PASSWORD);
#endif

#ifdef CURLOPT_HTTPAUTH
  CURB_DEFINE(CURLOPT_HTTPAUTH);
#endif
#ifdef CURLAUTH_DIGEST_IE
    CURB_DEFINE(CURLAUTH_DIGEST_IE);
#endif
#ifdef CURLAUTH_ONLY
    CURB_DEFINE(CURLAUTH_ONLY);
#endif
#ifdef CURLOPT_TLSAUTH_TYPE
  CURB_DEFINE(CURLOPT_TLSAUTH_TYPE);
#endif
#ifdef CURLOPT_TLSAUTH_SRP
  CURB_DEFINE(CURLOPT_TLSAUTH_SRP);
#endif
#ifdef CURLOPT_TLSAUTH_USERNAME
  CURB_DEFINE(CURLOPT_TLSAUTH_USERNAME);
#endif
#ifdef CURLOPT_TLSAUTH_PASSWORD
  CURB_DEFINE(CURLOPT_TLSAUTH_PASSWORD);
#endif
#ifdef CURLOPT_PROXYAUTH
  CURB_DEFINE(CURLOPT_PROXYAUTH);
#endif
#ifdef CURLOPT_AUTOREFERER
  CURB_DEFINE(CURLOPT_AUTOREFERER);
#endif
#ifdef CURLOPT_ENCODING
  CURB_DEFINE(CURLOPT_ENCODING);
#endif
#ifdef CURLOPT_FOLLOWLOCATION
  CURB_DEFINE(CURLOPT_FOLLOWLOCATION);
#endif
#ifdef CURLOPT_UNRESTRICTED_AUTH
  CURB_DEFINE(CURLOPT_UNRESTRICTED_AUTH);
#endif
#ifdef CURLOPT_MAXREDIRS
  CURB_DEFINE(CURLOPT_MAXREDIRS);
#endif
#ifdef CURLOPT_POSTREDIR
  CURB_DEFINE(CURLOPT_POSTREDIR);
#endif
#ifdef CURLOPT_PUT
  CURB_DEFINE(CURLOPT_PUT);
#endif
#ifdef CURLOPT_POST
  CURB_DEFINE(CURLOPT_POST);
#endif
  CURB_DEFINE(CURLOPT_POSTFIELDS);
  CURB_DEFINE(CURLOPT_POSTFIELDSIZE);
#ifdef CURLOPT_POSTFIELDSIZE_LARGE
  CURB_DEFINE(CURLOPT_POSTFIELDSIZE_LARGE);
#endif
#ifdef CURLOPT_COPYPOSTFIELDS
  CURB_DEFINE(CURLOPT_COPYPOSTFIELDS);
#endif
#ifdef CURLOPT_HTTPPOST
  CURB_DEFINE(CURLOPT_HTTPPOST);
#endif
  CURB_DEFINE(CURLOPT_REFERER);
  CURB_DEFINE(CURLOPT_USERAGENT);
  CURB_DEFINE(CURLOPT_HTTPHEADER);
#ifdef CURLOPT_HTTP200ALIASES
  CURB_DEFINE(CURLOPT_HTTP200ALIASES);
#endif

#ifdef CURLOPT_COOKIE
  CURB_DEFINE(CURLOPT_COOKIE);
#endif
#ifdef CURLOPT_COOKIEFILE
  CURB_DEFINE(CURLOPT_COOKIEFILE);
#endif
#ifdef CURLOPT_COOKIEJAR
  CURB_DEFINE(CURLOPT_COOKIEJAR);
#endif
#ifdef CURLOPT_COOKIESESSION
  CURB_DEFINE(CURLOPT_COOKIESESSION);
#endif
#ifdef CURLOPT_COOKIELIST
  CURB_DEFINE(CURLOPT_COOKIELIST);
#endif
#ifdef CURLOPT_HTTPGET
  CURB_DEFINE(CURLOPT_HTTPGET);
#endif
  CURB_DEFINE(CURLOPT_HTTP_VERSION);
    CURB_DEFINE(CURL_HTTP_VERSION_NONE);
    CURB_DEFINE(CURL_HTTP_VERSION_1_0);
    CURB_DEFINE(CURL_HTTP_VERSION_1_1);
#ifdef CURLOPT_IGNORE_CONTENT_LENGTH
  CURB_DEFINE(CURLOPT_IGNORE_CONTENT_LENGTH);
#endif
#ifdef CURLOPT_HTTP_CONTENT_DECODING
  CURB_DEFINE(CURLOPT_HTTP_CONTENT_DECODING);
#endif
#ifdef CURLOPT_HTTP_TRANSFER_DECODING
  CURB_DEFINE(CURLOPT_HTTP_TRANSFER_DECODING);
#endif
#ifdef CURLOPT_MAIL_FROM
  CURB_DEFINE(CURLOPT_MAIL_FROM);
#endif
#ifdef CURLOPT_MAIL_RCPT
  CURB_DEFINE(CURLOPT_MAIL_RCPT);
#endif
#ifdef CURLOPT_TFTP_BLKSIZE
  CURB_DEFINE(CURLOPT_TFTP_BLKSIZE);
#endif
#ifdef CURLOPT_FTPPORT
  CURB_DEFINE(CURLOPT_FTPPORT);
#endif
#ifdef CURLOPT_QUOTE
  CURB_DEFINE(CURLOPT_QUOTE);
#endif
#ifdef CURLOPT_POSTQUOTE
  CURB_DEFINE(CURLOPT_POSTQUOTE);
#endif
#ifdef CURLOPT_PREQUOTE
  CURB_DEFINE(CURLOPT_PREQUOTE);
#endif
#ifdef CURLOPT_DIRLISTONLY
  CURB_DEFINE(CURLOPT_DIRLISTONLY);
#endif
#ifdef CURLOPT_APPEND
  CURB_DEFINE(CURLOPT_APPEND);
#endif
#ifdef CURLOPT_FTP_USE_EPRT
  CURB_DEFINE(CURLOPT_FTP_USE_EPRT);
#endif
#ifdef CURLOPT_FTP_USE_EPSV
  CURB_DEFINE(CURLOPT_FTP_USE_EPSV);
#endif
#ifdef CURLOPT_FTP_USE_PRET
  CURB_DEFINE(CURLOPT_FTP_USE_PRET);
#endif
#ifdef CURLOPT_FTP_CREATE_MISSING_DIRS
  CURB_DEFINE(CURLOPT_FTP_CREATE_MISSING_DIRS);
#endif
#ifdef CURLOPT_FTP_RESPONSE_TIMEOUT
  CURB_DEFINE(CURLOPT_FTP_RESPONSE_TIMEOUT);
#endif
#ifdef CURLOPT_FTP_ALTERNATIVE_TO_USER
  CURB_DEFINE(CURLOPT_FTP_ALTERNATIVE_TO_USER);
#endif
#ifdef CURLOPT_FTP_SKIP_PASV_IP
  CURB_DEFINE(CURLOPT_FTP_SKIP_PASV_IP);
#endif
#ifdef CURLOPT_FTPSSLAUTH
  CURB_DEFINE(CURLOPT_FTPSSLAUTH);
#endif
#ifdef CURLFTPAUTH_DEFAULT
  CURB_DEFINE(CURLFTPAUTH_DEFAULT);
#endif
#ifdef CURLFTPAUTH_SSL
  CURB_DEFINE(CURLFTPAUTH_SSL);
#endif
#ifdef CURLFTPAUTH_TLS
  CURB_DEFINE(CURLFTPAUTH_TLS);
#endif
#ifdef CURLOPT_FTP_SSL_CCC
  CURB_DEFINE(CURLOPT_FTP_SSL_CCC);
#endif
#ifdef CURLFTPSSL_CCC_NONE
  CURB_DEFINE(CURLFTPSSL_CCC_NONE);
#endif
#ifdef CURLFTPSSL_CCC_PASSIVE
  CURB_DEFINE(CURLFTPSSL_CCC_PASSIVE);
#endif
#ifdef CURLFTPSSL_CCC_ACTIVE
  CURB_DEFINE(CURLFTPSSL_CCC_ACTIVE);
#endif
#ifdef CURLOPT_FTP_ACCOUNT
  CURB_DEFINE(CURLOPT_FTP_ACCOUNT);
#endif
#ifdef CURLOPT_FTP_FILEMETHOD
  CURB_DEFINE(CURLOPT_FTP_FILEMETHOD);
#endif
#ifdef CURLFTPMETHOD_MULTICWD
  CURB_DEFINE(CURLFTPMETHOD_MULTICWD);
#endif
#ifdef CURLFTPMETHOD_NOCWD
  CURB_DEFINE(CURLFTPMETHOD_NOCWD);
#endif
#ifdef CURLFTPMETHOD_SINGLECWD
  CURB_DEFINE(CURLFTPMETHOD_SINGLECWD);
#endif
#ifdef CURLOPT_RTSP_REQUEST
  CURB_DEFINE(CURLOPT_RTSP_REQUEST);
#endif
#ifdef CURL_RTSPREQ_OPTIONS
  CURB_DEFINE(CURL_RTSPREQ_OPTIONS);
#endif
#ifdef CURL_RTSPREQ_DESCRIBE
  CURB_DEFINE(CURL_RTSPREQ_DESCRIBE);
#endif
#ifdef CURL_RTSPREQ_ANNOUNCE
  CURB_DEFINE(CURL_RTSPREQ_ANNOUNCE);
#endif
#ifdef CURL_RTSPREQ_SETUP
  CURB_DEFINE(CURL_RTSPREQ_SETUP);
#endif
#ifdef CURL_RTSPREQ_PLAY
  CURB_DEFINE(CURL_RTSPREQ_PLAY);
#endif
#ifdef CURL_RTSPREQ_PAUSE
  CURB_DEFINE(CURL_RTSPREQ_PAUSE);
#endif
#ifdef CURL_RTSPREQ_TEARDOWN
  CURB_DEFINE(CURL_RTSPREQ_TEARDOWN);
#endif
#ifdef CURL_RTSPREQ_GET_PARAMETER
  CURB_DEFINE(CURL_RTSPREQ_GET_PARAMETER);
#endif
#ifdef CURL_RTSPREQ_SET_PARAMETER
  CURB_DEFINE(CURL_RTSPREQ_SET_PARAMETER);
#endif
#ifdef CURL_RTSPREQ_RECORD
  CURB_DEFINE(CURL_RTSPREQ_RECORD);
#endif
#ifdef CURL_RTSPREQ_RECEIVE
  CURB_DEFINE(CURL_RTSPREQ_RECEIVE);
#endif
#ifdef CURLOPT_RTSP_SESSION_ID
  CURB_DEFINE(CURLOPT_RTSP_SESSION_ID);
#endif
#ifdef CURLOPT_RTSP_STREAM_URI
  CURB_DEFINE(CURLOPT_RTSP_STREAM_URI);
#endif
#ifdef CURLOPT_RTSP_TRANSPORT
  CURB_DEFINE(CURLOPT_RTSP_TRANSPORT);
#endif
#ifdef CURLOPT_RTSP_HEADER
  CURB_DEFINE(CURLOPT_RTSP_HEADER);
#endif
#ifdef CURLOPT_RTSP_CLIENT_CSEQ
  CURB_DEFINE(CURLOPT_RTSP_CLIENT_CSEQ);
#endif
#ifdef CURLOPT_RTSP_SERVER_CSEQ
  CURB_DEFINE(CURLOPT_RTSP_SERVER_CSEQ);
#endif

  CURB_DEFINE(CURLOPT_TRANSFERTEXT);
#ifdef CURLOPT_PROXY_TRANSFER_MODE
  CURB_DEFINE(CURLOPT_PROXY_TRANSFER_MODE);
#endif
#ifdef CURLOPT_CRLF
  CURB_DEFINE(CURLOPT_CRLF);
#endif
#ifdef CURLOPT_RANGE
  CURB_DEFINE(CURLOPT_RANGE);
#endif
#ifdef CURLOPT_RESUME_FROM
  CURB_DEFINE(CURLOPT_RESUME_FROM);
#endif
#ifdef CURLOPT_RESUME_FROM_LARGE
  CURB_DEFINE(CURLOPT_RESUME_FROM_LARGE);
#endif
#ifdef CURLOPT_CUSTOMREQUEST
  CURB_DEFINE(CURLOPT_CUSTOMREQUEST);
#endif
#ifdef CURLOPT_FILETIME
  CURB_DEFINE(CURLOPT_FILETIME);
#endif
#ifdef CURLOPT_NOBODY
  CURB_DEFINE(CURLOPT_NOBODY);
#endif
#ifdef CURLOPT_INFILESIZE
  CURB_DEFINE(CURLOPT_INFILESIZE);
#endif
#ifdef CURLOPT_INFILESIZE_LARGE
  CURB_DEFINE(CURLOPT_INFILESIZE_LARGE);
#endif
#ifdef CURLOPT_UPLOAD
  CURB_DEFINE(CURLOPT_UPLOAD);
#endif
#ifdef CURLOPT_MAXFILESIZE
  CURB_DEFINE(CURLOPT_MAXFILESIZE);
#endif
#ifdef CURLOPT_MAXFILESIZE_LARGE
  CURB_DEFINE(CURLOPT_MAXFILESIZE_LARGE);
#endif
#ifdef CURLOPT_TIMECONDITION
  CURB_DEFINE(CURLOPT_TIMECONDITION);
#endif
#ifdef CURLOPT_TIMEVALUE
  CURB_DEFINE(CURLOPT_TIMEVALUE);
#endif

#ifdef CURLOPT_TIMEOUT
  CURB_DEFINE(CURLOPT_TIMEOUT);
#endif
#ifdef CURLOPT_TIMEOUT_MS
  CURB_DEFINE(CURLOPT_TIMEOUT_MS);
#endif
#ifdef CURLOPT_LOW_SPEED_LIMIT
  CURB_DEFINE(CURLOPT_LOW_SPEED_LIMIT);
#endif
#ifdef CURLOPT_LOW_SPEED_TIME
  CURB_DEFINE(CURLOPT_LOW_SPEED_TIME);
#endif
#ifdef CURLOPT_MAX_SEND_SPEED_LARGE
  CURB_DEFINE(CURLOPT_MAX_SEND_SPEED_LARGE);
#endif
#ifdef CURLOPT_MAX_RECV_SPEED_LARGE
  CURB_DEFINE(CURLOPT_MAX_RECV_SPEED_LARGE);
#endif
#ifdef CURLOPT_MAXCONNECTS
  CURB_DEFINE(CURLOPT_MAXCONNECTS);
#endif
#ifdef CURLOPT_CLOSEPOLICY
  CURB_DEFINE(CURLOPT_CLOSEPOLICY);
#endif
#ifdef CURLOPT_FRESH_CONNECT
  CURB_DEFINE(CURLOPT_FRESH_CONNECT);
#endif
#ifdef CURLOPT_FORBID_REUSE
  CURB_DEFINE(CURLOPT_FORBID_REUSE);
#endif
#ifdef CURLOPT_CONNECTTIMEOUT
  CURB_DEFINE(CURLOPT_CONNECTTIMEOUT);
#endif
#ifdef CURLOPT_CONNECTTIMEOUT_MS
  CURB_DEFINE(CURLOPT_CONNECTTIMEOUT_MS);
#endif
#ifdef CURLOPT_IPRESOLVE
  CURB_DEFINE(CURLOPT_IPRESOLVE);
#endif
#ifdef CURL_IPRESOLVE_WHATEVER
  CURB_DEFINE(CURL_IPRESOLVE_WHATEVER);
#endif
#ifdef CURL_IPRESOLVE_V4
  CURB_DEFINE(CURL_IPRESOLVE_V4);
#endif
#ifdef CURL_IPRESOLVE_V6
  CURB_DEFINE(CURL_IPRESOLVE_V6);
#endif
#ifdef CURLOPT_CONNECT_ONLY
  CURB_DEFINE(CURLOPT_CONNECT_ONLY);
#endif
#ifdef CURLOPT_USE_SSL
  CURB_DEFINE(CURLOPT_USE_SSL);
#endif
#ifdef CURLUSESSL_NONE
  CURB_DEFINE(CURLUSESSL_NONE);
#endif
#ifdef CURLUSESSL_TRY
  CURB_DEFINE(CURLUSESSL_TRY);
#endif
#ifdef CURLUSESSL_CONTROL
  CURB_DEFINE(CURLUSESSL_CONTROL);
#endif
#ifdef CURLUSESSL_ALL
  CURB_DEFINE(CURLUSESSL_ALL);
#endif
#ifdef CURLOPT_RESOLVE
  CURB_DEFINE(CURLOPT_RESOLVE);
#endif

#ifdef CURLOPT_SSLCERT
  CURB_DEFINE(CURLOPT_SSLCERT);
#endif
#ifdef CURLOPT_SSLCERTTYPE
  CURB_DEFINE(CURLOPT_SSLCERTTYPE);
#endif
#ifdef CURLOPT_SSLKEY
  CURB_DEFINE(CURLOPT_SSLKEY);
#endif
#ifdef CURLOPT_SSLKEYTYPE
  CURB_DEFINE(CURLOPT_SSLKEYTYPE);
#endif
#ifdef CURLOPT_KEYPASSWD
  CURB_DEFINE(CURLOPT_KEYPASSWD);
#endif
#ifdef CURLOPT_SSLENGINE
  CURB_DEFINE(CURLOPT_SSLENGINE);
#endif
#ifdef CURLOPT_SSLENGINE_DEFAULT
  CURB_DEFINE(CURLOPT_SSLENGINE_DEFAULT);
#endif
#ifdef CURLOPT_SSLVERSION
  CURB_DEFINE(CURLOPT_SSLVERSION);
#endif
#ifdef CURL_SSLVERSION_DEFAULT
  CURB_DEFINE(CURL_SSLVERSION_DEFAULT);
#endif
#ifdef CURL_SSLVERSION_TLSv1
  CURB_DEFINE(CURL_SSLVERSION_TLSv1);
#endif
#ifdef CURL_SSLVERSION_SSLv2
  CURB_DEFINE(CURL_SSLVERSION_SSLv2);
#endif
#ifdef CURL_SSLVERSION_SSLv3
  CURB_DEFINE(CURL_SSLVERSION_SSLv3);
#endif
#ifdef CURLOPT_SSL_VERIFYPEER
  CURB_DEFINE(CURLOPT_SSL_VERIFYPEER);
#endif
#ifdef CURLOPT_CAINFO
  CURB_DEFINE(CURLOPT_CAINFO);
#endif
#ifdef CURLOPT_ISSUERCERT
  CURB_DEFINE(CURLOPT_ISSUERCERT);
#endif
#ifdef CURLOPT_CAPATH
  CURB_DEFINE(CURLOPT_CAPATH);
#endif
#ifdef CURLOPT_CRLFILE
  CURB_DEFINE(CURLOPT_CRLFILE);
#endif
#ifdef CURLOPT_SSL_VERIFYHOST
  CURB_DEFINE(CURLOPT_SSL_VERIFYHOST);
#endif
#ifdef CURLOPT_CERTINFO
  CURB_DEFINE(CURLOPT_CERTINFO);
#endif
#ifdef CURLOPT_RANDOM_FILE
  CURB_DEFINE(CURLOPT_RANDOM_FILE);
#endif
#ifdef CURLOPT_EGDSOCKET
  CURB_DEFINE(CURLOPT_EGDSOCKET);
#endif
#ifdef CURLOPT_SSL_CIPHER_LIST
  CURB_DEFINE(CURLOPT_SSL_CIPHER_LIST);
#endif
#ifdef CURLOPT_SSL_SESSIONID_CACHE
  CURB_DEFINE(CURLOPT_SSL_SESSIONID_CACHE);
#endif
#ifdef CURLOPT_KRBLEVEL
  CURB_DEFINE(CURLOPT_KRBLEVEL);
#endif

#ifdef CURLOPT_SSH_AUTH_TYPES
  CURB_DEFINE(CURLOPT_SSH_AUTH_TYPES);
#endif
#ifdef CURLOPT_SSH_HOST_PUBLIC_KEY_MD5
  CURB_DEFINE(CURLOPT_SSH_HOST_PUBLIC_KEY_MD5);
#endif
#ifdef CURLOPT_SSH_PUBLIC_KEYFILE
  CURB_DEFINE(CURLOPT_SSH_PUBLIC_KEYFILE);
#endif
#ifdef CURLOPT_SSH_PRIVATE_KEYFILE
  CURB_DEFINE(CURLOPT_SSH_PRIVATE_KEYFILE);
#endif
#ifdef CURLOPT_SSH_KNOWNHOSTS
  CURB_DEFINE(CURLOPT_SSH_KNOWNHOSTS);
#endif
#ifdef CURLOPT_SSH_KEYFUNCTION
  CURB_DEFINE(CURLOPT_SSH_KEYFUNCTION);
#endif
#ifdef CURLKHSTAT_FINE_ADD_TO_FILE
  CURB_DEFINE(CURLKHSTAT_FINE_ADD_TO_FILE);
#endif
#ifdef CURLKHSTAT_FINE
  CURB_DEFINE(CURLKHSTAT_FINE);
#endif
#ifdef CURLKHSTAT_REJECT
  CURB_DEFINE(CURLKHSTAT_REJECT);
#endif
#ifdef CURLKHSTAT_DEFER
  CURB_DEFINE(CURLKHSTAT_DEFER);
#endif
#ifdef CURLOPT_SSH_KEYDATA
  CURB_DEFINE(CURLOPT_SSH_KEYDATA);
#endif

#ifdef CURLOPT_PRIVATE
  CURB_DEFINE(CURLOPT_PRIVATE);
#endif
#ifdef CURLOPT_SHARE
  CURB_DEFINE(CURLOPT_SHARE);
#endif
#ifdef CURLOPT_NEW_FILE_PERMS
  CURB_DEFINE(CURLOPT_NEW_FILE_PERMS);
#endif
#ifdef CURLOPT_NEW_DIRECTORY_PERMS
  CURB_DEFINE(CURLOPT_NEW_DIRECTORY_PERMS);
#endif

#ifdef CURLOPT_TELNETOPTIONS
  CURB_DEFINE(CURLOPT_TELNETOPTIONS);
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
