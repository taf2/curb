/* curb_errors.h - Ruby exception types for curl errors
 * Copyright (c)2006 Ross Bamford.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 * $Id: curb_errors.h 4 2006-11-17 18:35:31Z roscopeco $
 */
#ifndef __CURB_ERRORS_H
#define __CURB_ERRORS_H

#include "curb.h"

/* base errors */
extern VALUE cCurlErr;

/* easy errors */
extern VALUE mCurlErr;
extern VALUE eCurlErrError;
extern VALUE eCurlErrFTPError;
extern VALUE eCurlErrHTTPError;
extern VALUE eCurlErrFileError;
extern VALUE eCurlErrLDAPError;
extern VALUE eCurlErrTelnetError;
extern VALUE eCurlErrTFTPError;

/* libcurl errors */
extern VALUE eCurlErrUnsupportedProtocol;
extern VALUE eCurlErrFailedInit;
extern VALUE eCurlErrMalformedURL;
extern VALUE eCurlErrMalformedURLUser;
extern VALUE eCurlErrProxyResolution;
extern VALUE eCurlErrHostResolution;
extern VALUE eCurlErrConnectFailed;
extern VALUE eCurlErrFTPWeirdReply;
extern VALUE eCurlErrFTPAccessDenied;
extern VALUE eCurlErrFTPBadPassword;
extern VALUE eCurlErrFTPWeirdPassReply;
extern VALUE eCurlErrFTPWeirdUserReply;
extern VALUE eCurlErrFTPWeirdPasvReply;
extern VALUE eCurlErrFTPWeird227Format;
extern VALUE eCurlErrFTPCantGetHost;
extern VALUE eCurlErrFTPCantReconnect;
extern VALUE eCurlErrFTPCouldntSetBinary;
extern VALUE eCurlErrPartialFile;
extern VALUE eCurlErrFTPCouldntRetrFile;
extern VALUE eCurlErrFTPWrite;
extern VALUE eCurlErrFTPQuote;
extern VALUE eCurlErrHTTPFailed;
extern VALUE eCurlErrWriteError;
extern VALUE eCurlErrMalformedUser;
extern VALUE eCurlErrFTPCouldntStorFile;
extern VALUE eCurlErrReadError;
extern VALUE eCurlErrOutOfMemory;
extern VALUE eCurlErrTimeout;
extern VALUE eCurlErrFTPCouldntSetASCII;
extern VALUE eCurlErrFTPPortFailed;
extern VALUE eCurlErrFTPCouldntUseRest;
extern VALUE eCurlErrFTPCouldntGetSize;
extern VALUE eCurlErrHTTPRange;
extern VALUE eCurlErrHTTPPost;
extern VALUE eCurlErrSSLConnectError;
extern VALUE eCurlErrBadResume;
extern VALUE eCurlErrFileCouldntRead;
extern VALUE eCurlErrLDAPCouldntBind;
extern VALUE eCurlErrLDAPSearchFailed;
extern VALUE eCurlErrLibraryNotFound;
extern VALUE eCurlErrFunctionNotFound;
extern VALUE eCurlErrAbortedByCallback;
extern VALUE eCurlErrBadFunctionArgument;
extern VALUE eCurlErrBadCallingOrder;
extern VALUE eCurlErrInterfaceFailed;
extern VALUE eCurlErrBadPasswordEntered;
extern VALUE eCurlErrTooManyRedirects;
extern VALUE eCurlErrTelnetUnknownOption;
extern VALUE eCurlErrTelnetBadOptionSyntax;
extern VALUE eCurlErrObsolete;
extern VALUE eCurlErrSSLPeerCertificate;
extern VALUE eCurlErrGotNothing;
extern VALUE eCurlErrSSLEngineNotFound;
extern VALUE eCurlErrSSLEngineSetFailed;
extern VALUE eCurlErrSendError;
extern VALUE eCurlErrRecvError;
extern VALUE eCurlErrShareInUse;
extern VALUE eCurlErrSSLCertificate;
extern VALUE eCurlErrSSLCipher;
extern VALUE eCurlErrSSLCACertificate;
extern VALUE eCurlErrBadContentEncoding;
extern VALUE eCurlErrLDAPInvalidURL;
extern VALUE eCurlErrFileSizeExceeded;
extern VALUE eCurlErrFTPSSLFailed;
extern VALUE eCurlErrSendFailedRewind;
extern VALUE eCurlErrSSLEngineInitFailed;
extern VALUE eCurlErrLoginDenied;
extern VALUE eCurlErrTFTPNotFound;
extern VALUE eCurlErrTFTPPermission;
extern VALUE eCurlErrTFTPDiskFull;
extern VALUE eCurlErrTFTPIllegalOperation;
extern VALUE eCurlErrTFTPUnknownID;
extern VALUE eCurlErrTFTPFileExists;
extern VALUE eCurlErrTFTPNoSuchUser;
extern VALUE eCurlErrConvFailed;
extern VALUE eCurlErrConvReqd;
extern VALUE eCurlErrSSLCacertBadfile;
extern VALUE eCurlErrRemoteFileNotFound;
extern VALUE eCurlErrSSH;
extern VALUE eCurlErrSSLShutdownFailed;
extern VALUE eCurlErrAgain;
extern VALUE eCurlErrSSLCRLBadfile;
extern VALUE eCurlErrSSLIssuerError;

/* multi errors */
extern VALUE mCurlErrFailedInit;
extern VALUE mCurlErrCallMultiPerform;
extern VALUE mCurlErrBadHandle;
extern VALUE mCurlErrBadEasyHandle;
extern VALUE mCurlErrOutOfMemory;
extern VALUE mCurlErrInternalError;
extern VALUE mCurlErrBadSocket;
extern VALUE mCurlErrUnknownOption;
#if HAVE_CURLM_ADDED_ALREADY
extern VALUE mCurlErrAddedAlready;
#endif

/* binding errors */
extern VALUE eCurlErrInvalidPostField;

void init_curb_errors();
void raise_curl_easy_error_exception(CURLcode code);
void raise_curl_multi_error_exception(CURLMcode code);
VALUE rb_curl_easy_error(CURLcode code);
VALUE rb_curl_multi_error(CURLMcode code);

#endif
