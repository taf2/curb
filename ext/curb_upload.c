/* curb_upload.c - Curl upload handle
 * Copyright (c)2009 Todd A Fisher. 
 * Licensed under the Ruby License. See LICENSE for details.
 */
#include "curb_upload.h"
extern VALUE mCurl;
VALUE cCurlUpload;

#ifdef RDOC_NEVER_DEFINED
  mCurl = rb_define_module("Curl");
#endif

static void curl_upload_mark(void *ptr) {
  ruby_curl_upload *rbcu = (ruby_curl_upload *)ptr;
  if (rbcu && rbcu->stream && !NIL_P(rbcu->stream)) rb_gc_mark(rbcu->stream);
}

static void curl_upload_free(void *ptr) {
  if (ptr) free(ptr);
}

static size_t curl_upload_memsize(const void *ptr) {
  (void)ptr;
  return sizeof(ruby_curl_upload);
}

const rb_data_type_t ruby_curl_upload_data_type = {
  "Curl::Upload",
  {
    curl_upload_mark,
    curl_upload_free,
    curl_upload_memsize,
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    NULL, /* compact */
#endif
  },
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
  NULL, NULL, /* parent, data */
  RUBY_TYPED_FREE_IMMEDIATELY
#endif
};

/*
 * call-seq:
 *   internal class for sending large file uploads
 */
VALUE ruby_curl_upload_new(VALUE klass) {
  VALUE upload;
  ruby_curl_upload *rbcu = ALLOC(ruby_curl_upload);
  if (!rbcu) {
    rb_raise(rb_eNoMemError, "Failed to allocate memory for Curl::Upload");
  }
  rbcu->stream = Qnil;
  rbcu->offset = 0;
  upload = TypedData_Wrap_Struct(klass, &ruby_curl_upload_data_type, rbcu);
  return upload;
}

/*
 * call-seq:
 *   internal class for sending large file uploads
 */
VALUE ruby_curl_upload_stream_set(VALUE self, VALUE stream) {
  ruby_curl_upload *rbcu;
  TypedData_Get_Struct(self, ruby_curl_upload, &ruby_curl_upload_data_type, rbcu);
  rbcu->stream = stream;
  return stream;
}
/*
 * call-seq:
 *   internal class for sending large file uploads
 */
VALUE ruby_curl_upload_stream_get(VALUE self) {
  ruby_curl_upload *rbcu;
  TypedData_Get_Struct(self, ruby_curl_upload, &ruby_curl_upload_data_type, rbcu);
  return rbcu->stream;
}
/*
 * call-seq:
 *   internal class for sending large file uploads
 */
VALUE ruby_curl_upload_offset_set(VALUE self, VALUE offset) {
  ruby_curl_upload *rbcu;
  TypedData_Get_Struct(self, ruby_curl_upload, &ruby_curl_upload_data_type, rbcu);
  rbcu->offset = NUM2LONG(offset);
  return offset;
}
/*
 * call-seq:
 *   internal class for sending large file uploads
 */
VALUE ruby_curl_upload_offset_get(VALUE self) {
  ruby_curl_upload *rbcu;
  TypedData_Get_Struct(self, ruby_curl_upload, &ruby_curl_upload_data_type, rbcu);
  return LONG2NUM(rbcu->offset);
}

/* =================== INIT LIB =====================*/
void init_curb_upload() {
  cCurlUpload = rb_define_class_under(mCurl, "Upload", rb_cObject);
  rb_undef_alloc_func(cCurlUpload);
  rb_define_singleton_method(cCurlUpload, "new", ruby_curl_upload_new, 0);
  rb_define_method(cCurlUpload, "stream=", ruby_curl_upload_stream_set, 1);
  rb_define_method(cCurlUpload, "stream", ruby_curl_upload_stream_get, 0);
  rb_define_method(cCurlUpload, "offset=", ruby_curl_upload_offset_set, 1);
  rb_define_method(cCurlUpload, "offset", ruby_curl_upload_offset_get, 0);
}
