/* curb_postfield.c - Field class for POST method
 * Copyright (c)2006 Ross Bamford. 
 * Licensed under the Ruby License. See LICENSE for details.
 * 
 * $Id: curb_postfield.c 30 2006-12-09 12:30:24Z roscopeco $
 */
#include "curb_postfield.h"
#include "curb_errors.h"

extern VALUE mCurl;

static VALUE idCall;

#ifdef RDOC_NEVER_DEFINED
  mCurl = rb_define_module("Curl");
#endif

VALUE cCurlPostField;


/* ================= APPEND FORM FUNC ================ */

/* This gets called by the post method on Curl::Easy for each postfield
 * supplied in the arguments. It's job is to add the supplied field to
 * the list that's being built for a perform.
 * 
 * THIS FUNC MODIFIES ITS ARGUMENTS. See curl_formadd(3) for details.
 */
void append_to_form(VALUE self, 
                    struct curl_httppost **first, 
                    struct curl_httppost **last) {                               
  ruby_curl_postfield *rbcpf;
  CURLFORMcode result = -1;
  
  Data_Get_Struct(self, ruby_curl_postfield, rbcpf);
  
  if (rbcpf->name == Qnil) {
    rb_raise(eCurlErrInvalidPostField, "Cannot post unnamed field");
  } else {
    if ((rbcpf->local_file != Qnil) || (rbcpf->remote_file != Qnil)) {
      // is a file upload field
      if (rbcpf->content_proc != Qnil) {
        // with content proc
        rbcpf->buffer_str = rb_funcall(rbcpf->content_proc, idCall, self);
        
        if (rbcpf->remote_file == Qnil) {
          rb_raise(eCurlErrInvalidPostField, "Cannot post file upload field with no filename");
        } else {
          if (rbcpf->content_type == Qnil) {
            result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                               CURLFORM_BUFFER, StringValuePtr(rbcpf->remote_file),
                                               CURLFORM_BUFFERPTR, StringValuePtr(rbcpf->buffer_str),
                                               CURLFORM_BUFFERLENGTH, RSTRING_LEN(rbcpf->buffer_str),
                                               CURLFORM_END);
          } else {
            result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                               CURLFORM_BUFFER, StringValuePtr(rbcpf->remote_file),
                                               CURLFORM_BUFFERPTR, StringValuePtr(rbcpf->buffer_str),
                                               CURLFORM_BUFFERLENGTH, RSTRING_LEN(rbcpf->buffer_str),
                                               CURLFORM_CONTENTTYPE, StringValuePtr(rbcpf->content_type),
                                               CURLFORM_END);
          }
        }
      } else if (rbcpf->content != Qnil) {
        // with content
        if (rbcpf->remote_file == Qnil) {
          rb_raise(eCurlErrInvalidPostField, "Cannot post file upload field with no filename");
        } else {
          if (rbcpf->content_type == Qnil) {
            result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                               CURLFORM_BUFFER, StringValuePtr(rbcpf->remote_file),
                                               CURLFORM_BUFFERPTR, StringValuePtr(rbcpf->content),
                                               CURLFORM_BUFFERLENGTH, RSTRING_LEN(rbcpf->content),
                                               CURLFORM_END);
          } else {
            result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                               CURLFORM_BUFFER, StringValuePtr(rbcpf->remote_file),
                                               CURLFORM_BUFFERPTR, StringValuePtr(rbcpf->content),
                                               CURLFORM_BUFFERLENGTH, RSTRING_LEN(rbcpf->content),
                                               CURLFORM_CONTENTTYPE, StringValuePtr(rbcpf->content_type),
                                               CURLFORM_END);
          }
        }
      } else if (rbcpf->local_file != Qnil) {
        // with local filename
        if (rbcpf->local_file == Qnil) {
          rb_raise(eCurlErrInvalidPostField, "Cannot post file upload field no filename");
        } else {
          if (rbcpf->remote_file == Qnil) {
            rbcpf->remote_file = rbcpf->local_file;
          }
          
          if (rbcpf->content_type == Qnil) {
            result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                               CURLFORM_FILE, StringValuePtr(rbcpf->local_file),
                                               CURLFORM_FILENAME, StringValuePtr(rbcpf->remote_file),
                                               CURLFORM_END);
          } else {
            result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                               CURLFORM_FILE, StringValuePtr(rbcpf->local_file),
                                               CURLFORM_FILENAME, StringValuePtr(rbcpf->remote_file),
                                               CURLFORM_CONTENTTYPE, StringValuePtr(rbcpf->content_type),
                                               CURLFORM_END);
          }            
        }
      } else {
        rb_raise(eCurlErrInvalidPostField, "Cannot post file upload field with no data");
      }
    } else {
      // is a content field
      if (rbcpf->content_proc != Qnil) {
        rbcpf->buffer_str = rb_funcall(rbcpf->content_proc, idCall, self);
        
        if (rbcpf->content_type == Qnil) {
          result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                             CURLFORM_PTRCONTENTS, StringValuePtr(rbcpf->buffer_str),
                                             CURLFORM_CONTENTSLENGTH, RSTRING_LEN(rbcpf->buffer_str),
                                             CURLFORM_END);
        } else {
          result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                             CURLFORM_PTRCONTENTS, StringValuePtr(rbcpf->buffer_str),
                                             CURLFORM_CONTENTSLENGTH, RSTRING_LEN(rbcpf->buffer_str),
                                             CURLFORM_CONTENTTYPE, StringValuePtr(rbcpf->content_type),
                                             CURLFORM_END);
        }
      } else if (rbcpf->content != Qnil) {
        if (rbcpf->content_type == Qnil) {
          result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                             CURLFORM_PTRCONTENTS, StringValuePtr(rbcpf->content),
                                             CURLFORM_CONTENTSLENGTH, RSTRING_LEN(rbcpf->content),
                                             CURLFORM_END);
        } else {
          result = curl_formadd(first, last, CURLFORM_PTRNAME, StringValuePtr(rbcpf->name), 
                                             CURLFORM_PTRCONTENTS, StringValuePtr(rbcpf->content),
                                             CURLFORM_CONTENTSLENGTH, RSTRING_LEN(rbcpf->content),
                                             CURLFORM_CONTENTTYPE, StringValuePtr(rbcpf->content_type),
                                             CURLFORM_END);
        }
      } else {
        rb_raise(eCurlErrInvalidPostField, "Cannot post content field with no data");
      }
    }
  }
  
  if (result != 0) {
    char *reason;
    
    switch (result) {
      case CURL_FORMADD_MEMORY:
        reason = "Memory allocation failed";
        break;
      case CURL_FORMADD_OPTION_TWICE:
        reason = "Duplicate option";
        break;
      case CURL_FORMADD_NULL:
        reason = "Unexpected NULL string";
        break;
      case CURL_FORMADD_UNKNOWN_OPTION:
        reason = "Unknown option";
        break;
      case CURL_FORMADD_INCOMPLETE:
        reason = "Incomplete form data";
        break;
      case CURL_FORMADD_ILLEGAL_ARRAY:
        reason = "Illegal array [BINDING BUG]";
        break;
      case CURL_FORMADD_DISABLED:
        reason = "Installed libcurl cannot support requested feature(s)";
        break;
      default:
        reason = "Unknown error";        
    }
    
    rb_raise(eCurlErrInvalidPostField, "Failed to add field (%s)", reason);
  }
}


/* ================== MARK/FREE FUNC ==================*/
void curl_postfield_mark(ruby_curl_postfield *rbcpf) {
  rb_gc_mark(rbcpf->name);
  rb_gc_mark(rbcpf->content);
  rb_gc_mark(rbcpf->content_type);
  rb_gc_mark(rbcpf->local_file);
  rb_gc_mark(rbcpf->remote_file);
  rb_gc_mark(rbcpf->buffer_str);
}

void curl_postfield_free(ruby_curl_postfield *rbcpf) {
  free(rbcpf);
}


/* ================= ALLOC METHODS ====================*/

/*
 * call-seq:
 *   Curl::PostField.content(name, content) => #&lt;Curl::PostField...&gt;
 *   Curl::PostField.content(name, content, content_type = nil) => #&lt;Curl::PostField...&gt;
 *   Curl::PostField.content(name, content_type = nil) { |field| ... } => #&lt;Curl::PostField...&gt;
 * 
 * Create a new Curl::PostField, supplying the field name, content,
 * and, optionally, Content-type (curl will attempt to determine this if
 * not specified).
 * 
 * The block form allows a block to supply the content for this field, called
 * during the perform. The block should return a ruby string with the field
 * data.
 */
static VALUE ruby_curl_postfield_new_content(int argc, VALUE *argv, VALUE klass) {
  ruby_curl_postfield *rbcpf = ALLOC(ruby_curl_postfield);
  
  // wierdness - we actually require two args, unless a block is provided, but
  // we have to work that out below.
  rb_scan_args(argc, argv, "12&", &rbcpf->name, &rbcpf->content, &rbcpf->content_type, &rbcpf->content_proc);
  
  // special handling if theres a block, second arg is actually content_type
  if (rbcpf->content_proc != Qnil) {
    if (rbcpf->content != Qnil) {
      // we were given a content-type
      rbcpf->content_type = rbcpf->content;
      rbcpf->content = Qnil;
    } else {
      // default content type      
      rbcpf->content_type = Qnil;
    }
  } else {
    // no block, so make sure content was provided    
    if (rbcpf->content == Qnil) {
      rb_raise(rb_eArgError, "Incorrect number of arguments (expected 2 or 3)");
    }    
  }
      
  /* assoc objects */
  rbcpf->local_file = Qnil;
  rbcpf->remote_file = Qnil;
  rbcpf->buffer_str = Qnil;
  
  return Data_Wrap_Struct(cCurlPostField, curl_postfield_mark, curl_postfield_free, rbcpf);
}

/*
 * call-seq:
 *   Curl::PostField.file(name, local_file_name) => #&lt;Curl::PostField...&gt;
 *   Curl::PostField.file(name, local_file_name, remote_file_name = local_file_name) => #&lt;Curl::PostField...&gt;
 *   Curl::PostField.file(name, remote_file_name) { |field| ... } => #&lt;Curl::PostField...&gt;
 * 
 * Create a new Curl::PostField for a file upload field, supplying the local filename
 * to read from, and optionally the remote filename (defaults to the local name).
 * 
 * The block form allows a block to supply the content for this field, called
 * during the perform. The block should return a ruby string with the field
 * data.
 */
static VALUE ruby_curl_postfield_new_file(int argc, VALUE *argv, VALUE klass) {
  // TODO needs to handle content-type too 
  ruby_curl_postfield *rbcpf = ALLOC(ruby_curl_postfield);
  
  rb_scan_args(argc, argv, "21&", &rbcpf->name, &rbcpf->local_file, &rbcpf->remote_file, &rbcpf->content_proc);
  
  // special handling if theres a block, second arg is actually remote name.
  if (rbcpf->content_proc != Qnil) {
    if (rbcpf->local_file != Qnil) {
      // we were given a local file
      if (rbcpf->remote_file == Qnil) {
        // we weren't given a remote, so local is actually remote
        // (correct block call form)
        rbcpf->remote_file = rbcpf->local_file;
      }
      
      // Shouldn't get a local file, so can ignore it.
      rbcpf->local_file = Qnil;
    }
  } else {
    if (rbcpf->remote_file == Qnil) {
      rbcpf->remote_file = rbcpf->local_file;
    }
  }    
      
  /* assoc objects */
  rbcpf->content = Qnil;
  rbcpf->content_type = Qnil;
  rbcpf->buffer_str = Qnil;
  
  return Data_Wrap_Struct(cCurlPostField, curl_postfield_mark, curl_postfield_free, rbcpf);
}

/* ================= ATTRIBUTES ====================*/

/*
 * call-seq:
 *   field.name = "name"                            => "name"
 * 
 * Set the POST field name for this PostField.
 */
static VALUE ruby_curl_postfield_name_set(VALUE self, VALUE name) {
  CURB_OBJECT_SETTER(ruby_curl_postfield, name);
}

/*
 * call-seq:
 *   field.name                                       => "name"
 * 
 * Obtain the POST field name for this PostField.
 */ 
static VALUE ruby_curl_postfield_name_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_postfield, name);
}

/*
 * call-seq:
 *   field.content = "content"                        => "content"
 * 
 * Set the POST field content for this PostField. Ignored when a 
 * content_proc is supplied via either +Curl::PostField.file+ or 
 * +set_content_proc+.
 */
static VALUE ruby_curl_postfield_content_set(VALUE self, VALUE content) {
  CURB_OBJECT_SETTER(ruby_curl_postfield, content);
}

/*
 * call-seq:
 *   field.content                                    => "content"
 * 
 * Obtain the POST field content for this PostField.
 */ 
static VALUE ruby_curl_postfield_content_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_postfield, content);
}

/*
 * call-seq:
 *   field.content_type = "content_type"              => "content_type"
 * 
 * Set the POST field Content-type for this PostField.
 */
static VALUE ruby_curl_postfield_content_type_set(VALUE self, VALUE content_type) {
  CURB_OBJECT_SETTER(ruby_curl_postfield, content_type);
}

/*
 * call-seq:
 *   field.content_type                               => "content_type"
 * 
 * Get the POST field Content-type for this PostField.
 */ 
static VALUE ruby_curl_postfield_content_type_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_postfield, content_type);
}

/*
 * call-seq:
 *   field.local_file = "filename"                    => "filename"
 * 
 * Set the POST field local filename for this PostField (when performing
 * a file upload). Ignored when a content_proc is supplied via either 
 * +Curl::PostField.file+ or +set_content_proc+.
 */
static VALUE ruby_curl_postfield_local_file_set(VALUE self, VALUE local_file) {
  CURB_OBJECT_SETTER(ruby_curl_postfield, local_file);
}

/*
 * call-seq:
 *   field.local_file                                 => "filename"
 * 
 * Get the POST field local filename for this PostField (when performing
 * a file upload). 
 */ 
static VALUE ruby_curl_postfield_local_file_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_postfield, local_file);
}

/*
 * call-seq:
 *   field.remote_file = "filename"                   => "filename"
 * 
 * Set the POST field remote filename for this PostField (when performing
 * a file upload). If no remote filename is provided, and no content_proc
 * is supplied, the local filename is used. If no remote filename is 
 * specified when a content_proc is used, an exception will be raised 
 * during the perform. 
 */
static VALUE ruby_curl_postfield_remote_file_set(VALUE self, VALUE remote_file) {
  CURB_OBJECT_SETTER(ruby_curl_postfield, remote_file);
}

/*
 * call-seq:
 *   field.local_file                                 => "filename"
 * 
 * Get the POST field remote filename for this PostField (when performing
 * a file upload).
 */ 
static VALUE ruby_curl_postfield_remote_file_get(VALUE self) {
  CURB_OBJECT_GETTER(ruby_curl_postfield, remote_file);
}

/*
 * call-seq:
 *   field.set_content_proc { |field| ... }           => &lt;old proc&gt;
 * 
 * Set a content proc for this field. This proc will be called during the
 * perform to supply the content for this field, overriding any setting
 * of +content+ or +local_file+.
 */
static VALUE ruby_curl_postfield_content_proc_set(int argc, VALUE *argv, VALUE self) {
  CURB_HANDLER_PROC_SETTER(ruby_curl_postfield, content_proc);
}

/*
 * call-seq:
 *   field.to_str                                     => "name=value"
 *   field.to_s                                       => "name=value"
 * 
 * Obtain a String representation of this PostField in url-encoded 
 * format. This is used to construct the post data for non-multipart
 * POSTs.
 * 
 * Only content fields may be converted to strings.
 */
static VALUE ruby_curl_postfield_to_str(VALUE self) {
  // FIXME This is using the deprecated curl_escape func
  ruby_curl_postfield *rbcpf;
  VALUE result = Qnil;
  
  Data_Get_Struct(self, ruby_curl_postfield, rbcpf);

  if ((rbcpf->local_file == Qnil) && (rbcpf->remote_file == Qnil)) {
    if (rbcpf->name != Qnil) {

      char *tmpchrs = curl_escape(RSTRING_PTR(rbcpf->name), RSTRING_LEN(rbcpf->name));
      
      if (!tmpchrs) {
        rb_raise(eCurlErrInvalidPostField, "Failed to url-encode name `%s'", tmpchrs);
      } else {
        VALUE tmpcontent = Qnil;
        VALUE escd_name = rb_str_new2(tmpchrs);
        curl_free(tmpchrs);
        
        if (rbcpf->content_proc != Qnil) {
          tmpcontent = rb_funcall(rbcpf->content_proc, idCall, 1, self);
        } else if (rbcpf->content != Qnil) {
          tmpcontent = rbcpf->content;
        } else {
          tmpcontent = rb_str_new2("");
        }
        if (TYPE(tmpcontent) != T_STRING) {
          if (rb_respond_to(tmpcontent, rb_intern("to_s"))) {
            tmpcontent = rb_funcall(tmpcontent, rb_intern("to_s"), 0);
          }
          else {
            rb_raise(rb_eRuntimeError, "postfield(%s) is not a string and does not respond_to to_s", RSTRING_PTR(escd_name) );
          }
        }
        //fprintf(stderr, "encoding content: %ld - %s\n", RSTRING_LEN(tmpcontent), RSTRING_PTR(tmpcontent) );
        tmpchrs = curl_escape(RSTRING_PTR(tmpcontent), RSTRING_LEN(tmpcontent));
        if (!tmpchrs) {
          rb_raise(eCurlErrInvalidPostField, "Failed to url-encode content `%s'", tmpchrs);
        } else {
          VALUE escd_content = rb_str_new2(tmpchrs);
          curl_free(tmpchrs);
          
          result = escd_name;
          rb_str_cat(result, "=", 1);
          rb_str_concat(result, escd_content); 
        }            
      }
    } else {
      rb_raise(eCurlErrInvalidPostField, "Cannot convert unnamed field to string");
    }      
  } else {
    rb_raise(eCurlErrInvalidPostField, "Cannot convert non-content field to string");
  }  
  
  return result;
}


/* =================== INIT LIB =====================*/
void init_curb_postfield() {
  idCall = rb_intern("call");
  
  cCurlPostField = rb_define_class_under(mCurl, "PostField", rb_cObject);

  /* Class methods */
  rb_define_singleton_method(cCurlPostField, "content", ruby_curl_postfield_new_content, -1);
  rb_define_singleton_method(cCurlPostField, "file", ruby_curl_postfield_new_file, -1);
  
  VALUE sc = rb_singleton_class(cCurlPostField);
  rb_undef(sc, rb_intern("new"));
  
  rb_define_method(cCurlPostField, "name=", ruby_curl_postfield_name_set, 1);
  rb_define_method(cCurlPostField, "name", ruby_curl_postfield_name_get, 0);  
  rb_define_method(cCurlPostField, "content=", ruby_curl_postfield_content_set, 1);
  rb_define_method(cCurlPostField, "content", ruby_curl_postfield_content_get, 0);  
  rb_define_method(cCurlPostField, "content_type=", ruby_curl_postfield_content_type_set, 1);
  rb_define_method(cCurlPostField, "content_type", ruby_curl_postfield_content_type_get, 0);  
  rb_define_method(cCurlPostField, "local_file=", ruby_curl_postfield_local_file_set, 1);
  rb_define_method(cCurlPostField, "local_file", ruby_curl_postfield_local_file_get, 0);  
  rb_define_method(cCurlPostField, "remote_file=", ruby_curl_postfield_remote_file_set, 1);
  rb_define_method(cCurlPostField, "remote_file", ruby_curl_postfield_remote_file_get, 0);  
  
  rb_define_method(cCurlPostField, "set_content_proc", ruby_curl_postfield_content_proc_set, -1);  

  rb_define_method(cCurlPostField, "to_str", ruby_curl_postfield_to_str, 0);  
  rb_define_alias(cCurlPostField, "to_s", "to_str");
}
