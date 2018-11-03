/* curb_errors.c - Ruby exception types for curl errors
 * Copyright (c)2006 Ross Bamford.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id: curb_errors.c 10 2006-11-20 00:17:30Z roscopeco $
 */
#include "curb_errors.h"

extern VALUE mCurl;

#ifdef RDOC_NEVER_DEFINED
  mCurl = rb_define_module("Curl");
#endif

/* base errors */
VALUE mCurlErr;
VALUE eCurlErrError;
VALUE eCurlErrFTPError;
VALUE eCurlErrHTTPError;
VALUE eCurlErrFileError;
VALUE eCurlErrLDAPError;
VALUE eCurlErrTelnetError;
VALUE eCurlErrTFTPError;
VALUE eCurlErrRTSPError;

/* Specific libcurl errors */
VALUE eCurlErrOK; /* not really an error but a return code */
VALUE eCurlErrUnsupportedProtocol;
VALUE eCurlErrFailedInit;
VALUE eCurlErrMalformedURL;
VALUE eCurlErrMalformedURLUser;
VALUE eCurlErrNotBuiltIn;
VALUE eCurlErrProxyResolution;
VALUE eCurlErrHostResolution;
VALUE eCurlErrConnectFailed;
VALUE eCurlErrFTPWeirdReply;
VALUE eCurlErrFTPAccessDenied;
VALUE eCurlErrFTPBadPassword;
VALUE eCurlErrFTPWeirdPassReply;
VALUE eCurlErrFTPWeirdUserReply;
VALUE eCurlErrFTPWeirdPasvReply;
VALUE eCurlErrFTPWeird227Format;
VALUE eCurlErrFTPCantGetHost;
VALUE eCurlErrFTPCantReconnect;
VALUE eCurlErrFTPCouldntSetBinary;
VALUE eCurlErrPartialFile;
VALUE eCurlErrFTPCouldntRetrFile;
VALUE eCurlErrFTPWrite;
VALUE eCurlErrFTPQuote;
VALUE eCurlErrHTTPFailed;
VALUE eCurlErrWriteError;
VALUE eCurlErrMalformedUser;
VALUE eCurlErrFTPCouldntStorFile;
VALUE eCurlErrReadError;
VALUE eCurlErrOutOfMemory;
VALUE eCurlErrTimeout;
VALUE eCurlErrFTPCouldntSetASCII;
VALUE eCurlErrFTPPortFailed;
VALUE eCurlErrFTPCouldntUseRest;
VALUE eCurlErrFTPCouldntGetSize;
VALUE eCurlErrHTTPRange;
VALUE eCurlErrHTTPPost;
VALUE eCurlErrSSLConnectError;
VALUE eCurlErrBadResume;
VALUE eCurlErrFileCouldntRead;
VALUE eCurlErrLDAPCouldntBind;
VALUE eCurlErrLDAPSearchFailed;
VALUE eCurlErrLibraryNotFound;
VALUE eCurlErrFunctionNotFound;
VALUE eCurlErrAbortedByCallback;
VALUE eCurlErrBadFunctionArgument;
VALUE eCurlErrBadCallingOrder;
VALUE eCurlErrInterfaceFailed;
VALUE eCurlErrBadPasswordEntered;
VALUE eCurlErrTooManyRedirects;
VALUE eCurlErrTelnetUnknownOption;
VALUE eCurlErrTelnetBadOptionSyntax;
VALUE eCurlErrObsolete;
VALUE eCurlErrSSLPeerCertificate;
VALUE eCurlErrGotNothing;
VALUE eCurlErrSSLEngineNotFound;
VALUE eCurlErrSSLEngineSetFailed;
VALUE eCurlErrSendError;
VALUE eCurlErrRecvError;
VALUE eCurlErrShareInUse;
VALUE eCurlErrSSLCertificate;
VALUE eCurlErrSSLCipher;
VALUE eCurlErrSSLCACertificate;
VALUE eCurlErrBadContentEncoding;
VALUE eCurlErrLDAPInvalidURL;
VALUE eCurlErrFileSizeExceeded;
VALUE eCurlErrFTPSSLFailed;
VALUE eCurlErrSendFailedRewind;
VALUE eCurlErrSSLEngineInitFailed;
VALUE eCurlErrLoginDenied;
VALUE eCurlErrTFTPNotFound;
VALUE eCurlErrTFTPPermission;
VALUE eCurlErrTFTPDiskFull;
VALUE eCurlErrTFTPIllegalOperation;
VALUE eCurlErrTFTPUnknownID;
VALUE eCurlErrTFTPFileExists;
VALUE eCurlErrTFTPNoSuchUser;

VALUE eCurlErrConvFailed;
VALUE eCurlErrConvReqd;
VALUE eCurlErrSSLCacertBadfile;
VALUE eCurlErrRemoteFileNotFound;
VALUE eCurlErrSSH;
VALUE eCurlErrSSLShutdownFailed;
VALUE eCurlErrAgain;
VALUE eCurlErrSSLCRLBadfile;
VALUE eCurlErrSSLIssuerError;

/* multi errors */
VALUE mCurlErrFailedInit;
VALUE mCurlErrCallMultiPerform;
VALUE mCurlErrBadHandle;
VALUE mCurlErrBadEasyHandle;
VALUE mCurlErrOutOfMemory;
VALUE mCurlErrInternalError;
VALUE mCurlErrBadSocket;
#if HAVE_CURLM_ADDED_ALREADY
VALUE mCurlErrAddedAlready;
#endif
VALUE mCurlErrUnknownOption;

/* binding errors */
VALUE eCurlErrInvalidPostField;


/* new errors */
VALUE eCurlErrFTPPRETFailed;
VALUE eCurlErrRTSPCseqError;
VALUE eCurlErrRTSPSessionError;
VALUE eCurlErrFTPBadFileList;
VALUE eCurlErrChunkFailed;
VALUE eCurlErrNoConnectionAvailable;
VALUE eCurlErrSSLPinnedPubKeyNotMatch;
VALUE eCurlErrSSLInvalidCertStatus;
VALUE eCurlErrHTTP2Stream;


VALUE rb_curl_easy_error(CURLcode code) {
  VALUE exclz;
  const char *exmsg = NULL;
  VALUE results;

  switch (code) {
    case CURLE_OK:                      /* 0 */
      exclz = eCurlErrOK;
      break;
    case CURLE_UNSUPPORTED_PROTOCOL:    /* 1 */
      exclz = eCurlErrUnsupportedProtocol;
      break;
    case CURLE_FAILED_INIT:             /* 2 */
      exclz = eCurlErrFailedInit;
      break;
    case CURLE_URL_MALFORMAT:           /* 3 */
      exclz = eCurlErrMalformedURL;
      break;
#ifdef HAVE_CURLE_NOT_BUILT_IN
    case CURLE_NOT_BUILT_IN:            /* 4 - [was obsoleted in August 2007 for 7.17.0, reused in April 2011 for 7.21.5] */
      exclz = eCurlErrNotBuiltIn;
      break;
#else
    case CURLE_URL_MALFORMAT_USER:      /* 4 (NOT USED) */
      exclz = eCurlErrMalformedURLUser;
      break;
#endif
    case CURLE_COULDNT_RESOLVE_PROXY:   /* 5 */
      exclz = eCurlErrProxyResolution;
      break;
    case CURLE_COULDNT_RESOLVE_HOST:    /* 6 */
      exclz = eCurlErrHostResolution;
      break;
    case CURLE_COULDNT_CONNECT:         /* 7 */
      exclz = eCurlErrConnectFailed;
      break;
    case CURLE_FTP_WEIRD_SERVER_REPLY:  /* 8 */
      exclz = eCurlErrFTPWeirdReply;
      break;
    case CURLE_FTP_ACCESS_DENIED:       /* 9 denied due to lack of access. */
      exclz = eCurlErrFTPAccessDenied;
      break;
    case CURLE_FTP_USER_PASSWORD_INCORRECT: /* 10 */
      exclz = eCurlErrFTPBadPassword;
      break;
    case CURLE_FTP_WEIRD_PASS_REPLY:    /* 11 */
      exclz = eCurlErrFTPWeirdPassReply;
      break;
    case CURLE_FTP_WEIRD_USER_REPLY:    /* 12 */
      exclz = eCurlErrFTPWeirdUserReply;
      break;
    case CURLE_FTP_WEIRD_PASV_REPLY:    /* 13 */
      exclz = eCurlErrFTPWeirdPasvReply;
      break;
    case CURLE_FTP_WEIRD_227_FORMAT:    /* 14 */
      exclz = eCurlErrFTPWeird227Format;
      break;
    case CURLE_FTP_CANT_GET_HOST:       /* 15 */
      exclz = eCurlErrFTPCantGetHost;
      break;
    case CURLE_FTP_CANT_RECONNECT:      /* 16 */
      exclz = eCurlErrFTPCantReconnect;
      break;
    case CURLE_FTP_COULDNT_SET_BINARY:  /* 17 */
      exclz = eCurlErrFTPCouldntSetBinary;
      break;
    case CURLE_PARTIAL_FILE:            /* 18 */
      exclz = eCurlErrPartialFile;
      break;
    case CURLE_FTP_COULDNT_RETR_FILE:   /* 19 */
      exclz = eCurlErrFTPCouldntRetrFile;
      break;
    case CURLE_FTP_WRITE_ERROR:         /* 20 */
      exclz = eCurlErrFTPWrite;
      break;
    case CURLE_FTP_QUOTE_ERROR:         /* 21 */
      exclz = eCurlErrFTPQuote;
      break;
    case CURLE_HTTP_RETURNED_ERROR:     /* 22 */
      exclz = eCurlErrHTTPFailed;
      break;
    case CURLE_WRITE_ERROR:             /* 23 */
      exclz = eCurlErrWriteError;
      break;
    case CURLE_MALFORMAT_USER:          /* 24 - NOT USED */
      exclz = eCurlErrMalformedUser;
      break;
    case CURLE_FTP_COULDNT_STOR_FILE:   /* 25 - failed FTP upload */
      exclz = eCurlErrFTPCouldntStorFile;
      break;
    case CURLE_READ_ERROR:              /* 26 - could open/read from file */
      exclz = eCurlErrReadError;
      break;
    case CURLE_OUT_OF_MEMORY:           /* 27 */
      exclz = eCurlErrOutOfMemory;
      break;
    case CURLE_OPERATION_TIMEOUTED:     /* 28 - the timeout time was reached */
      exclz = eCurlErrTimeout;
      break;
    case CURLE_FTP_COULDNT_SET_ASCII:   /* 29 - TYPE A failed */
      exclz = eCurlErrFTPCouldntSetASCII;
      break;
    case CURLE_FTP_PORT_FAILED:         /* 30 - FTP PORT operation failed */
      exclz = eCurlErrFTPPortFailed;
      break;
    case CURLE_FTP_COULDNT_USE_REST:    /* 31 - the REST command failed */
      exclz = eCurlErrFTPCouldntUseRest;
      break;
    case CURLE_FTP_COULDNT_GET_SIZE:    /* 32 - the SIZE command failed */
      exclz = eCurlErrFTPCouldntGetSize;
      break;
    case CURLE_HTTP_RANGE_ERROR:        /* 33 - RANGE "command" didn't work */
      exclz = eCurlErrHTTPRange;
      break;
    case CURLE_HTTP_POST_ERROR:         /* 34 */
      exclz = eCurlErrHTTPPost;
      break;
    case CURLE_SSL_CONNECT_ERROR:       /* 35 - wrong when connecting with SSL */
      exclz = eCurlErrSSLConnectError;
      break;
    case CURLE_BAD_DOWNLOAD_RESUME:     /* 36 - couldn't resume download */
      exclz = eCurlErrBadResume;
      break;
    case CURLE_FILE_COULDNT_READ_FILE:  /* 37 */
      exclz = eCurlErrFileCouldntRead;
      break;
    case CURLE_LDAP_CANNOT_BIND:        /* 38 */
      exclz = eCurlErrLDAPCouldntBind;
      break;
    case CURLE_LDAP_SEARCH_FAILED:      /* 39 */
      exclz = eCurlErrLDAPSearchFailed;
      break;
    case CURLE_LIBRARY_NOT_FOUND:       /* 40 */
      exclz = eCurlErrLibraryNotFound;
      break;
    case CURLE_FUNCTION_NOT_FOUND:      /* 41 */
      exclz = eCurlErrFunctionNotFound;
      break;
    case CURLE_ABORTED_BY_CALLBACK:     /* 42 */
      exclz = eCurlErrAbortedByCallback;
      break;
    case CURLE_BAD_FUNCTION_ARGUMENT:   /* 43 */
      exclz = eCurlErrBadFunctionArgument;
      break;
    case CURLE_BAD_CALLING_ORDER:       /* 44 - NOT USED */
      exclz = eCurlErrBadCallingOrder;
      break;
    case CURLE_INTERFACE_FAILED:        /* 45 - CURLOPT_INTERFACE failed */
      exclz = eCurlErrInterfaceFailed;
      break;
    case CURLE_BAD_PASSWORD_ENTERED:    /* 46 - NOT USED */
      exclz = eCurlErrBadPasswordEntered;
      break;
    case CURLE_TOO_MANY_REDIRECTS:      /* 47 - catch endless re-direct loops */
      exclz = eCurlErrTooManyRedirects;
      break;
    case CURLE_UNKNOWN_TELNET_OPTION:   /* 48 - User specified an unknown option */
      exclz = eCurlErrTelnetUnknownOption;
      break;
    case CURLE_TELNET_OPTION_SYNTAX:    /* 49 - Malformed telnet option */
      exclz = eCurlErrTelnetBadOptionSyntax;
      break;
#ifdef HAVE_CURLE_OBSOLETE
    case CURLE_OBSOLETE:                /* 50 - NOT USED */
      exclz = eCurlErrObsolete;
      break;
#endif
#if LIBCURL_VERSION_NUM < 0x073e00
    case CURLE_SSL_PEER_CERTIFICATE:    /* 51 - peer's certificate wasn't ok */
      exclz = eCurlErrSSLPeerCertificate;
      break;
#endif
    case CURLE_GOT_NOTHING:             /* 52 - when this is a specific error */
      exclz = eCurlErrGotNothing;
      break;
    case CURLE_SSL_ENGINE_NOTFOUND:     /* 53 - SSL crypto engine not found */
      exclz = eCurlErrSSLEngineNotFound;
      break;
    case CURLE_SSL_ENGINE_SETFAILED:    /* 54 - can not set SSL crypto engine as default */
      exclz = eCurlErrSSLEngineSetFailed;
      break;
    case CURLE_SEND_ERROR:              /* 55 - failed sending network data */
      exclz = eCurlErrSendError;
      break;
    case CURLE_RECV_ERROR:              /* 56 - failure in receiving network data */
      exclz = eCurlErrRecvError;
      break;
    case CURLE_SHARE_IN_USE:            /* 57 - share is in use */
      exclz = eCurlErrShareInUse;
      break;
    case CURLE_SSL_CERTPROBLEM:         /* 58 - problem with the local certificate */
      exclz = eCurlErrSSLCertificate;
      break;
    case CURLE_SSL_CIPHER:              /* 59 - couldn't use specified cipher */
      exclz = eCurlErrSSLCipher;
      break;
#if LIBCURL_VERSION_NUM >= 0x073e00
    case CURLE_PEER_FAILED_VERIFICATION:              /* 60 - problem with the CA cert (path?) */
      exclz = eCurlErrSSLPeerCertificate;
#else
    case CURLE_SSL_CACERT:              /* 60 - problem with the CA cert (path?) */
      exclz = eCurlErrSSLCACertificate;
#endif
      break;
    case CURLE_BAD_CONTENT_ENCODING:    /* 61 - Unrecognized transfer encoding */
      exclz = eCurlErrBadContentEncoding;
      break;
    case CURLE_LDAP_INVALID_URL:        /* 62 - Invalid LDAP URL */
      exclz = eCurlErrLDAPInvalidURL;
      break;
    case CURLE_FILESIZE_EXCEEDED:       /* 63 - Maximum file size exceeded */
      exclz = eCurlErrFileSizeExceeded;
      break;
    case CURLE_FTP_SSL_FAILED:          /* 64 - Requested FTP SSL level failed */
      exclz = eCurlErrFTPSSLFailed;
      break;
#ifdef HAVE_CURLE_SEND_FAIL_REWIND
    case CURLE_SEND_FAIL_REWIND:        /* 65 - Sending the data requires a rewind that failed */
      exclz = eCurlErrSendFailedRewind;
      break;
#endif
#ifdef HAVE_CURLE_SSL_ENGINE_INITFAILED
    case CURLE_SSL_ENGINE_INITFAILED:   /* 66 - failed to initialise ENGINE */
      exclz = eCurlErrSSLEngineInitFailed;
      break;
#endif
#ifdef HAVE_CURLE_LOGIN_DENIED
    case CURLE_LOGIN_DENIED:            /* 67 - user, password or similar was not accepted and we failed to login */
      exclz = eCurlErrLoginDenied;
      break;
#endif

      // recent additions, may not be present in all supported versions
#ifdef HAVE_CURLE_TFTP_NOTFOUND
    case CURLE_TFTP_NOTFOUND:           /* 68 - file not found on server */
      exclz = eCurlErrTFTPNotFound;
      break;
#endif
#ifdef HAVE_CURLE_TFTP_PERM
    case CURLE_TFTP_PERM:               /* 69 - permission problem on server */
      exclz = eCurlErrTFTPPermission;
      break;
#endif
#ifdef HAVE_CURLE_TFTP_DISKFULL
    case CURLE_TFTP_DISKFULL:           /* 70 - out of disk space on server */
      exclz = eCurlErrTFTPDiskFull;
      break;
#endif
#ifdef HAVE_CURLE_TFTP_ILLEGAL
    case CURLE_TFTP_ILLEGAL:            /* 71 - Illegal TFTP operation */
      exclz = eCurlErrTFTPIllegalOperation;
      break;
#endif
#ifdef HAVE_CURLE_TFTP_UNKNOWNID
    case CURLE_TFTP_UNKNOWNID:          /* 72 - Unknown transfer ID */
      exclz = eCurlErrTFTPUnknownID;
      break;
#endif
#ifdef HAVE_CURLE_TFTP_EXISTS
    case CURLE_TFTP_EXISTS:             /* 73 - File already exists */
      exclz = eCurlErrTFTPFileExists;
      break;
#endif
#ifdef HAVE_CURLE_TFTP_NOSUCHUSER
    case CURLE_TFTP_NOSUCHUSER:         /* 74 - No such user */
      exclz = eCurlErrTFTPNotFound;
      break;
#endif
#ifdef HAVE_CURLE_CONV_FAILED
    case CURLE_CONV_FAILED:             /* 75 - conversion failed */
      exclz = eCurlErrConvFailed;
      break;
#endif
#ifdef HAVE_CURLE_CONV_REQD
    case CURLE_CONV_REQD:               /* 76 - caller must register conversion
                                                callbacks using curl_easy_setopt options
                                                CURLOPT_CONV_FROM_NETWORK_FUNCTION,
                                                CURLOPT_CONV_TO_NETWORK_FUNCTION, and
                                                CURLOPT_CONV_FROM_UTF8_FUNCTION */
      exclz = eCurlErrConvReqd;
      break;
#endif
#ifdef HAVE_CURLE_SSL_CACERT_BADFILE
    case CURLE_SSL_CACERT_BADFILE:      /* 77 - could not load CACERT file, missing
                                                or wrong format */
      exclz = eCurlErrSSLCacertBadfile;
      break;
#endif
#ifdef  HAVE_CURLE_REMOTE_FILE_NOT_FOUND
    case CURLE_REMOTE_FILE_NOT_FOUND:   /* 78 - remote file not found */
      exclz = eCurlErrRemoteFileNotFound;
      break;
#endif
#ifdef  HAVE_CURLE_SSH
    case CURLE_SSH:                     /* 79 - error from the SSH layer, somewhat
                                    generic so the error message will be of
                                    interest when this has happened */
      exclz = eCurlErrSSH;
      break;
#endif
#ifdef  HAVE_CURLE_SSL_SHUTDOWN_FAILED
    case CURLE_SSL_SHUTDOWN_FAILED:     /* 80 - Failed to shut down the SSL
                                    connection */
      exclz = eCurlErrSSLShutdownFailed;
      break;
#endif
#ifdef  HAVE_CURLE_AGAIN
    case CURLE_AGAIN:                   /* 81 - socket is not ready for send/recv,
                                    wait till it's ready and try again (Added
                                    in 7.18.2) */
      exclz = eCurlErrAgain;
      break;
#endif
#ifdef  HAVE_CURLE_SSL_CRL_BADFILE
    case CURLE_SSL_CRL_BADFILE:         /* 82 - could not load CRL file, missing or
                                    wrong format (Added in 7.19.0) */
      exclz = eCurlErrSSLCRLBadfile;
      break;
#endif
#ifdef  HAVE_CURLE_SSL_ISSUER_ERROR
    case CURLE_SSL_ISSUER_ERROR:        /* 83 - Issuer check failed.  (Added in
                                    7.19.0) */
      exclz = eCurlErrSSLIssuerError;
      break;
#endif

#ifdef  HAVE_CURLE_FTP_PRET_FAILED
    case CURLE_FTP_PRET_FAILED:  /* 84 */
      exclz = eCurlErrFTPPRETFailed;
      break;
#endif

#ifdef  HAVE_CURLE_RTSP_CSEQ_ERROR
    case CURLE_RTSP_CSEQ_ERROR:  /* 85 */
      exclz = eCurlErrRTSPCseqError;
      break;
#endif

#ifdef  HAVE_CURLE_RTSP_SESSION_ERROR
    case CURLE_RTSP_SESSION_ERROR:  /* 86 */
      exclz = eCurlErrRTSPSessionError;
      break;
#endif

#ifdef  HAVE_CURLE_FTP_BAD_FILE_LIST
    case CURLE_FTP_BAD_FILE_LIST:  /* 87 */
      exclz = eCurlErrFTPBadFileList;
      break;
#endif

#ifdef  HAVE_CURLE_CHUNK_FAILED
    case CURLE_CHUNK_FAILED:  /* 88 */
      exclz = eCurlErrChunkFailed;
      break;
#endif

#ifdef  HAVE_CURLE_NO_CONNECTION_AVAILABLE
    case CURLE_NO_CONNECTION_AVAILABLE:  /* 89 */
      exclz = eCurlErrNoConnectionAvailable;
      break;
#endif

#ifdef  HAVE_CURLE_SSL_PINNEDPUBKEYNOTMATCH
    case CURLE_SSL_PINNEDPUBKEYNOTMATCH:  /* 90 */
      exclz = eCurlErrSSLPinnedPubKeyNotMatch;
      break;
#endif

#ifdef  HAVE_CURLE_SSL_INVALIDCERTSTATUS
    case CURLE_SSL_INVALIDCERTSTATUS:  /* 91 */
      exclz = eCurlErrSSLInvalidCertStatus;
      break;
#endif

#ifdef  HAVE_CURLE_HTTP2_STREAM
    case CURLE_HTTP2_STREAM:  /* 92 */
      exclz = eCurlErrHTTP2Stream;
      break;
#endif

    default:
      exclz = eCurlErrError;
      exmsg = "Unknown error result from libcurl";
  }

  if (!exmsg) {
    exmsg = curl_easy_strerror(code);
  }

  results = rb_ary_new2(2);
  rb_ary_push(results, exclz);
  rb_ary_push(results, rb_str_new2(exmsg));
  return results;
}
/* rb_raise an approriate exception for the supplied CURLcode */
void raise_curl_easy_error_exception(CURLcode code) {
  VALUE obj = rb_curl_easy_error(code);
  VALUE exmsg = rb_ary_entry(obj,1);
  rb_raise(rb_ary_entry(obj,0), "CURLError: %s", StringValueCStr(exmsg));
}
VALUE rb_curl_multi_error(CURLMcode code) {
  VALUE exclz;
  const char *exmsg = NULL;
  VALUE results;

  switch(code) {
    case CURLM_CALL_MULTI_PERFORM: /* -1 */
      exclz = mCurlErrCallMultiPerform;
      break;
    case CURLM_BAD_HANDLE: /* 1 */
      exclz = mCurlErrBadHandle;
      break;
    case CURLM_BAD_EASY_HANDLE: /* 2 */
      exclz = mCurlErrBadEasyHandle;
      break;
    case CURLM_OUT_OF_MEMORY: /* 3 */
      exclz = mCurlErrOutOfMemory;
      break;
    case CURLM_INTERNAL_ERROR: /* 4 */
      exclz = mCurlErrInternalError;
      break;
#if HAVE_CURLM_BAD_SOCKET
    case CURLM_BAD_SOCKET: /* 5 */
      exclz = mCurlErrBadSocket;
      break;
#endif
#if HAVE_CURLM_UNKNOWN_OPTION
    case CURLM_UNKNOWN_OPTION: /* 6 */
      exclz = mCurlErrUnknownOption;
      break;
#endif
#if HAVE_CURLM_ADDED_ALREADY
    case CURLM_ADDED_ALREADY: /* 7 */
      exclz = mCurlErrAddedAlready;
      break;
#endif
    default:
      exclz = eCurlErrError;
      exmsg = "Unknown error result from libcurl";
  }

  if (!exmsg) {
    exmsg = curl_multi_strerror(code);
  }

  results = rb_ary_new2(2);
  rb_ary_push(results, exclz);
  rb_ary_push(results, rb_str_new2(exmsg));

  return results;
}
void raise_curl_multi_error_exception(CURLMcode code) {
  VALUE obj = rb_curl_multi_error(code);
  VALUE exmsg = rb_ary_entry(obj,1);
  rb_raise(rb_ary_entry(obj,0), "CURLError: %s", StringValueCStr(exmsg));
}

void init_curb_errors() {
  mCurlErr = rb_define_module_under(mCurl, "Err");
  eCurlErrError = rb_define_class_under(mCurlErr, "CurlError", rb_eRuntimeError);

  eCurlErrFTPError = rb_define_class_under(mCurlErr, "FTPError", eCurlErrError);
  eCurlErrHTTPError = rb_define_class_under(mCurlErr, "HTTPError", eCurlErrError);
  eCurlErrFileError = rb_define_class_under(mCurlErr, "FileError", eCurlErrError);
  eCurlErrLDAPError = rb_define_class_under(mCurlErr, "LDAPError", eCurlErrError);
  eCurlErrTelnetError = rb_define_class_under(mCurlErr, "TelnetError", eCurlErrError);
  eCurlErrTFTPError = rb_define_class_under(mCurlErr, "TFTPError", eCurlErrError);
  eCurlErrRTSPError = rb_define_class_under(mCurlErr, "RTSPError", eCurlErrError);

  eCurlErrOK = rb_define_class_under(mCurlErr, "CurlOK", eCurlErrError);
  eCurlErrUnsupportedProtocol = rb_define_class_under(mCurlErr, "UnsupportedProtocolError", eCurlErrError);
  eCurlErrFailedInit = rb_define_class_under(mCurlErr, "FailedInitError", eCurlErrError);
  eCurlErrMalformedURL = rb_define_class_under(mCurlErr, "MalformedURLError", eCurlErrError);
  eCurlErrNotBuiltIn = rb_define_class_under(mCurlErr, "NotBuiltInError", eCurlErrError);
  eCurlErrMalformedURLUser = rb_define_class_under(mCurlErr, "MalformedURLUserError", eCurlErrError);
  eCurlErrProxyResolution = rb_define_class_under(mCurlErr, "ProxyResolutionError", eCurlErrError);
  eCurlErrHostResolution = rb_define_class_under(mCurlErr, "HostResolutionError", eCurlErrError);
  eCurlErrConnectFailed = rb_define_class_under(mCurlErr, "ConnectionFailedError", eCurlErrError);

  eCurlErrFTPWeirdReply = rb_define_class_under(mCurlErr, "WeirdReplyError", eCurlErrFTPError);
  eCurlErrFTPAccessDenied = rb_define_class_under(mCurlErr, "AccessDeniedError", eCurlErrFTPError);
  eCurlErrFTPBadPassword = rb_define_class_under(mCurlErr, "BadPasswordError", eCurlErrFTPError);
  eCurlErrFTPWeirdPassReply = rb_define_class_under(mCurlErr, "WeirdPassReplyError", eCurlErrFTPError);
  eCurlErrFTPWeirdUserReply = rb_define_class_under(mCurlErr, "WeirdUserReplyError", eCurlErrFTPError);
  eCurlErrFTPWeirdPasvReply = rb_define_class_under(mCurlErr, "WeirdPasvReplyError", eCurlErrFTPError);
  eCurlErrFTPWeird227Format = rb_define_class_under(mCurlErr, "Weird227FormatError", eCurlErrFTPError);
  eCurlErrFTPCantGetHost = rb_define_class_under(mCurlErr, "CantGetHostError", eCurlErrFTPError);
  eCurlErrFTPCantReconnect = rb_define_class_under(mCurlErr, "CantReconnectError", eCurlErrFTPError);
  eCurlErrFTPCouldntSetBinary = rb_define_class_under(mCurlErr, "CouldntSetBinaryError", eCurlErrFTPError);

  eCurlErrPartialFile = rb_define_class_under(mCurlErr, "PartialFileError", eCurlErrError);

  eCurlErrFTPCouldntRetrFile = rb_define_class_under(mCurlErr, "CouldntRetrFileError", eCurlErrFTPError);
  eCurlErrFTPWrite = rb_define_class_under(mCurlErr, "FTPWriteError", eCurlErrFTPError);
  eCurlErrFTPQuote = rb_define_class_under(mCurlErr, "FTPQuoteError", eCurlErrFTPError);

  eCurlErrHTTPFailed = rb_define_class_under(mCurlErr, "HTTPFailedError", eCurlErrHTTPError);

  eCurlErrWriteError = rb_define_class_under(mCurlErr, "WriteError", eCurlErrError);
  eCurlErrMalformedUser = rb_define_class_under(mCurlErr, "MalformedUserError", eCurlErrError);

  eCurlErrFTPCouldntStorFile = rb_define_class_under(mCurlErr, "CouldntStorFileError", eCurlErrFTPError);

  eCurlErrReadError = rb_define_class_under(mCurlErr, "ReadError", eCurlErrError);
  eCurlErrOutOfMemory = rb_define_class_under(mCurlErr, "OutOfMemoryError", eCurlErrError);
  eCurlErrTimeout = rb_define_class_under(mCurlErr, "TimeoutError", eCurlErrError);

  eCurlErrFTPCouldntSetASCII = rb_define_class_under(mCurlErr, "CouldntSetASCIIError", eCurlErrFTPError);
  eCurlErrFTPPortFailed = rb_define_class_under(mCurlErr, "PortFailedError", eCurlErrFTPError);
  eCurlErrFTPCouldntUseRest = rb_define_class_under(mCurlErr, "CouldntUseRestError", eCurlErrFTPError);
  eCurlErrFTPCouldntGetSize = rb_define_class_under(mCurlErr, "CouldntGetSizeError", eCurlErrFTPError);

  eCurlErrHTTPRange = rb_define_class_under(mCurlErr, "HTTPRangeError", eCurlErrHTTPError);
  eCurlErrHTTPPost = rb_define_class_under(mCurlErr, "HTTPPostError", eCurlErrHTTPError);

  eCurlErrSSLConnectError = rb_define_class_under(mCurlErr, "SSLConnectError", eCurlErrError);
  eCurlErrBadResume = rb_define_class_under(mCurlErr, "BadResumeError", eCurlErrError);

  eCurlErrFileCouldntRead = rb_define_class_under(mCurlErr, "CouldntReadError", eCurlErrFileError);

  eCurlErrLDAPCouldntBind = rb_define_class_under(mCurlErr, "CouldntBindError", eCurlErrLDAPError);
  eCurlErrLDAPSearchFailed = rb_define_class_under(mCurlErr, "SearchFailedError", eCurlErrLDAPError);

  eCurlErrLibraryNotFound = rb_define_class_under(mCurlErr, "LibraryNotFoundError", eCurlErrError);
  eCurlErrFunctionNotFound = rb_define_class_under(mCurlErr, "FunctionNotFoundError", eCurlErrError);
  eCurlErrAbortedByCallback = rb_define_class_under(mCurlErr, "AbortedByCallbackError", eCurlErrError);
  eCurlErrBadFunctionArgument = rb_define_class_under(mCurlErr, "BadFunctionArgumentError", eCurlErrError);
  eCurlErrBadCallingOrder = rb_define_class_under(mCurlErr, "BadCallingOrderError", eCurlErrError);
  eCurlErrInterfaceFailed = rb_define_class_under(mCurlErr, "InterfaceFailedError", eCurlErrError);

  eCurlErrBadPasswordEntered = rb_define_class_under(mCurlErr, "BadPasswordEnteredError", eCurlErrError);
  eCurlErrTooManyRedirects = rb_define_class_under(mCurlErr, "TooManyRedirectsError", eCurlErrError);

  eCurlErrTelnetUnknownOption = rb_define_class_under(mCurlErr, "UnknownOptionError", eCurlErrTelnetError);
  eCurlErrTelnetBadOptionSyntax = rb_define_class_under(mCurlErr, "BadOptionSyntaxError", eCurlErrTelnetError);

  eCurlErrObsolete = rb_define_class_under(mCurlErr, "ObsoleteError", eCurlErrError);
  eCurlErrSSLPeerCertificate = rb_define_class_under(mCurlErr, "SSLPeerCertificateError", eCurlErrError);
  eCurlErrGotNothing = rb_define_class_under(mCurlErr, "GotNothingError", eCurlErrError);

  eCurlErrSSLEngineNotFound = rb_define_class_under(mCurlErr, "SSLEngineNotFoundError", eCurlErrError);
  eCurlErrSSLEngineSetFailed = rb_define_class_under(mCurlErr, "SSLEngineSetFailedError", eCurlErrError);

  eCurlErrSendError = rb_define_class_under(mCurlErr, "SendError", eCurlErrError);
  eCurlErrRecvError = rb_define_class_under(mCurlErr, "RecvError", eCurlErrError);
  eCurlErrShareInUse = rb_define_class_under(mCurlErr, "ShareInUseError", eCurlErrError);

  eCurlErrConvFailed  = rb_define_class_under(mCurlErr, "ConvFailed", eCurlErrError);
  eCurlErrConvReqd    = rb_define_class_under(mCurlErr, "ConvReqd", eCurlErrError);
  eCurlErrRemoteFileNotFound  = rb_define_class_under(mCurlErr, "RemoteFileNotFound", eCurlErrError);
  eCurlErrAgain  = rb_define_class_under(mCurlErr, "Again", eCurlErrError);

  eCurlErrSSLCertificate = rb_define_class_under(mCurlErr, "SSLCertificateError", eCurlErrError);
  eCurlErrSSLCipher = rb_define_class_under(mCurlErr, "SSLCypherError", eCurlErrError);
  eCurlErrSSLCACertificate = rb_define_class_under(mCurlErr, "SSLCACertificateError", eCurlErrError);
  eCurlErrBadContentEncoding = rb_define_class_under(mCurlErr, "BadContentEncodingError", eCurlErrError);
  eCurlErrSSLCacertBadfile   = rb_define_class_under(mCurlErr, "SSLCaertBadFile", eCurlErrError);
  eCurlErrSSLCRLBadfile      = rb_define_class_under(mCurlErr, "SSLCRLBadfile", eCurlErrError);
  eCurlErrSSLIssuerError     = rb_define_class_under(mCurlErr, "SSLIssuerError", eCurlErrError);
  eCurlErrSSLShutdownFailed  = rb_define_class_under(mCurlErr, "SSLShutdownFailed", eCurlErrError);
  eCurlErrSSH                = rb_define_class_under(mCurlErr, "SSH", eCurlErrError);

  mCurlErrFailedInit         = rb_define_class_under(mCurlErr, "MultiInitError", eCurlErrError);
  mCurlErrCallMultiPerform   = rb_define_class_under(mCurlErr, "MultiPerform", eCurlErrError);
  mCurlErrBadHandle          = rb_define_class_under(mCurlErr, "MultiBadHandle", eCurlErrError);
  mCurlErrBadEasyHandle      = rb_define_class_under(mCurlErr, "MultiBadEasyHandle", eCurlErrError);
  mCurlErrOutOfMemory        = rb_define_class_under(mCurlErr, "MultiOutOfMemory", eCurlErrError);
  mCurlErrInternalError      = rb_define_class_under(mCurlErr, "MultiInternalError", eCurlErrError);
  mCurlErrBadSocket          = rb_define_class_under(mCurlErr, "MultiBadSocket", eCurlErrError);
#if HAVE_CURLM_ADDED_ALREADY
  mCurlErrAddedAlready       = rb_define_class_under(mCurlErr, "MultiAddedAlready", eCurlErrError);
#endif
  mCurlErrUnknownOption      = rb_define_class_under(mCurlErr, "MultiUnknownOption", eCurlErrError);

  eCurlErrLDAPInvalidURL = rb_define_class_under(mCurlErr, "InvalidLDAPURLError", eCurlErrLDAPError);

  eCurlErrFileSizeExceeded = rb_define_class_under(mCurlErr, "FileSizeExceededError", eCurlErrError);

  eCurlErrFTPSSLFailed = rb_define_class_under(mCurlErr, "FTPSSLFailed", eCurlErrFTPError);

  eCurlErrSendFailedRewind = rb_define_class_under(mCurlErr, "SendFailedRewind", eCurlErrError);
  eCurlErrSSLEngineInitFailed = rb_define_class_under(mCurlErr, "SSLEngineInitFailedError", eCurlErrError);
  eCurlErrLoginDenied = rb_define_class_under(mCurlErr, "LoginDeniedError", eCurlErrError);

  eCurlErrTFTPNotFound = rb_define_class_under(mCurlErr, "NotFoundError", eCurlErrTFTPError);
  eCurlErrTFTPPermission = rb_define_class_under(mCurlErr, "PermissionError", eCurlErrTFTPError);
  eCurlErrTFTPDiskFull = rb_define_class_under(mCurlErr, "DiskFullError", eCurlErrTFTPError);
  eCurlErrTFTPIllegalOperation = rb_define_class_under(mCurlErr, "IllegalOperationError", eCurlErrTFTPError);
  eCurlErrTFTPUnknownID = rb_define_class_under(mCurlErr, "UnknownIDError", eCurlErrTFTPError);
  eCurlErrTFTPFileExists = rb_define_class_under(mCurlErr, "FileExistsError", eCurlErrTFTPError);
  eCurlErrTFTPNoSuchUser = rb_define_class_under(mCurlErr, "NoSuchUserError", eCurlErrTFTPError);

  eCurlErrInvalidPostField = rb_define_class_under(mCurlErr, "InvalidPostFieldError", eCurlErrError);

  eCurlErrFTPPRETFailed = rb_define_class_under(mCurlErr, "PPRETFailedError", eCurlErrFTPError);
  eCurlErrRTSPCseqError = rb_define_class_under(mCurlErr, "CseqError", eCurlErrRTSPError);
  eCurlErrRTSPSessionError = rb_define_class_under(mCurlErr, "SessionError", eCurlErrRTSPError);
  eCurlErrFTPBadFileList = rb_define_class_under(mCurlErr, "BadFileListError", eCurlErrFTPError);
  eCurlErrChunkFailed = rb_define_class_under(mCurlErr, "ChunkFailedError", eCurlErrError);
  eCurlErrNoConnectionAvailable = rb_define_class_under(mCurlErr, "NoConnectionAvailableError", eCurlErrError);
  eCurlErrSSLPinnedPubKeyNotMatch = rb_define_class_under(mCurlErr, "SSLPinnedPubKeyNotMatchError", eCurlErrError);
  eCurlErrSSLInvalidCertStatus = rb_define_class_under(mCurlErr, "SSLInvalidCertStatusError", eCurlErrError);
  eCurlErrHTTP2Stream = rb_define_class_under(mCurlErr, "HTTP2StreamError", eCurlErrHTTPError);
}
