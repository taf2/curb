/* curb_share.c - Curl share interface
 * Wraps libcurl's CURLSH handle so multiple Curl::Easy handles across Ruby
 * threads can share a connection cache, DNS cache, SSL session cache, and
 * cookie jar without redundant TCP/TLS handshakes or DNS lookups.
 *
 * Thread safety: libcurl requires the caller to supply lock/unlock callbacks
 * when a share object is accessed from multiple threads. We use one
 * pthread_mutex_t per curl_lock_data slot so distinct data types (e.g. DNS
 * vs connections) do not block each other.
 */
#include "curb_share.h"
#include "curb_errors.h"

extern VALUE mCurl;

VALUE cCurlShare;

/* =================== LOCK CALLBACKS ===================
 * libcurl calls these from within curl_easy_perform (with the GVL released),
 * so we must not call any Ruby API here — pure pthreads only.
 */
static void curl_share_lock_cb(CURL *handle, curl_lock_data data,
                               curl_lock_access access, void *userptr) {
    ruby_curl_share *rshare = (ruby_curl_share *)userptr;
    (void)handle;
    (void)access;
    if (data < CURL_LOCK_DATA_LAST) {
        pthread_mutex_lock(&rshare->mutexes[data]);
    }
}

static void curl_share_unlock_cb(CURL *handle, curl_lock_data data,
                                 void *userptr) {
    ruby_curl_share *rshare = (ruby_curl_share *)userptr;
    (void)handle;
    if (data < CURL_LOCK_DATA_LAST) {
        pthread_mutex_unlock(&rshare->mutexes[data]);
    }
}

/* =================== MARK / FREE ===================*/

/*
 * GC free function. Called when the Ruby object is collected.
 * Also called after #close sets closed=1 and share=NULL, in which case
 * we skip the already-done cleanup.
 */
static void rb_curl_share_free(void *ptr) {
    ruby_curl_share *rshare = (ruby_curl_share *)ptr;
    int i;
    if (!rshare) return;

    if (rshare->share) {
        curl_share_cleanup(rshare->share);
        rshare->share = NULL;
    }

    for (i = 0; i < CURL_LOCK_DATA_LAST; i++) {
        pthread_mutex_destroy(&rshare->mutexes[i]);
    }

    free(rshare);
}

/* =================== ALLOC ===================*/

/*
 * call-seq:
 *   Curl::Share.new  => #<Curl::Share>
 *
 * Allocate and initialize a new share handle. Mutexes are initialized for
 * all curl_lock_data slots; lock/unlock callbacks are registered immediately
 * so the share is thread-safe as soon as it is created.
 */
static VALUE rb_curl_share_alloc(VALUE klass) {
    ruby_curl_share *rshare;
    int i;

    rshare = ALLOC(ruby_curl_share);
    if (!rshare) {
        rb_raise(rb_eNoMemError, "Failed to allocate memory for Curl::Share");
    }

    rshare->closed = 0;

    rshare->share = curl_share_init();
    if (!rshare->share) {
        free(rshare);
        rb_raise(rb_eRuntimeError, "curl_share_init() failed");
    }

    for (i = 0; i < CURL_LOCK_DATA_LAST; i++) {
        pthread_mutex_init(&rshare->mutexes[i], NULL);
    }

    curl_share_setopt(rshare->share, CURLSHOPT_LOCKFUNC,   curl_share_lock_cb);
    curl_share_setopt(rshare->share, CURLSHOPT_UNLOCKFUNC, curl_share_unlock_cb);
    curl_share_setopt(rshare->share, CURLSHOPT_USERDATA,   rshare);

    /* NULL mark function: the struct holds no Ruby VALUE references */
    return Data_Wrap_Struct(klass, 0, rb_curl_share_free, rshare);
}

/* =================== RUBY METHODS ===================*/

/*
 * call-seq:
 *   share.enable(:connections)   => share
 *   share.enable(:dns)           => share
 *   share.enable(:ssl_session)   => share
 *   share.enable(:cookies)       => share
 *
 * Enable sharing of the specified data type across all Curl::Easy handles
 * that have been attached to this share object.
 *
 * Raises ArgumentError for unknown symbols.
 * Raises Curl::Err::ShareInUseError if an easy handle is currently using
 * the share (libcurl restriction: options cannot be changed while in use).
 */
static VALUE rb_curl_share_enable(VALUE self, VALUE sym) {
    ruby_curl_share *rshare;
    curl_lock_data data_type;
    CURLSHcode rc;
    ID id;

    Data_Get_Struct(self, ruby_curl_share, rshare);

    if (rshare->closed) {
        rb_raise(rb_eRuntimeError, "cannot enable on a closed Curl::Share");
    }

    Check_Type(sym, T_SYMBOL);
    id = SYM2ID(sym);

    if (id == rb_intern("connections")) {
        data_type = CURL_LOCK_DATA_CONNECT;
    } else if (id == rb_intern("dns")) {
        data_type = CURL_LOCK_DATA_DNS;
    } else if (id == rb_intern("ssl_session")) {
        data_type = CURL_LOCK_DATA_SSL_SESSION;
    } else if (id == rb_intern("cookies")) {
        data_type = CURL_LOCK_DATA_COOKIE;
    } else {
        rb_raise(rb_eArgError,
                 "unknown share type :%s (expected :connections, :dns, :ssl_session, or :cookies)",
                 rb_id2name(id));
    }

    rc = curl_share_setopt(rshare->share, CURLSHOPT_SHARE, data_type);
    if (rc == CURLSHE_IN_USE) {
        rb_raise(eCurlErrShareInUse,
                 "cannot change share options while an easy handle is attached");
    }
    if (rc != CURLSHE_OK) {
        rb_raise(rb_eRuntimeError, "curl_share_setopt failed: %s",
                 curl_share_strerror(rc));
    }

    return self;
}

/*
 * call-seq:
 *   share.close  => nil
 *
 * Release the underlying libcurl share handle and all associated mutexes.
 * Safe to call multiple times (idempotent). After closing, the share must
 * not be used with any new Curl::Easy handle.
 */
static VALUE rb_curl_share_close(VALUE self) {
    ruby_curl_share *rshare;
    int i;

    Data_Get_Struct(self, ruby_curl_share, rshare);

    if (rshare->closed) {
        return Qnil;
    }

    rshare->closed = 1;

    if (rshare->share) {
        curl_share_cleanup(rshare->share);
        rshare->share = NULL;
    }

    for (i = 0; i < CURL_LOCK_DATA_LAST; i++) {
        pthread_mutex_destroy(&rshare->mutexes[i]);
    }

    return Qnil;
}

/* =================== INIT ===================*/

void init_curb_share(void) {
    cCurlShare = rb_define_class_under(mCurl, "Share", rb_cObject);

    rb_define_alloc_func(cCurlShare, rb_curl_share_alloc);
    rb_define_method(cCurlShare, "enable", rb_curl_share_enable, 1);
    rb_define_method(cCurlShare, "close",  rb_curl_share_close,  0);
}
