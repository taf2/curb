/* Curb - helper macros for ruby integration
 * Copyright (c)2006 Ross Bamford. 
 * Licensed under the Ruby License. See LICENSE for details.
 * 
 * $Id: curb_macros.h 13 2006-11-23 23:54:25Z roscopeco $
 */

#ifndef __CURB_MACROS_H
#define __CURB_MACROS_H

#define rb_easy_sym(sym) ID2SYM(rb_intern(sym))
#define rb_easy_hkey(key) ID2SYM(rb_intern(key))
#define rb_easy_set(key,val) rb_hash_aset(rbce->opts, rb_easy_hkey(key) , val)
#define rb_easy_get(key) rb_hash_aref(rbce->opts, rb_easy_hkey(key))
#define rb_easy_del(key) rb_hash_delete(rbce->opts, rb_easy_hkey(key))
#define rb_easy_nil(key) (rb_hash_aref(rbce->opts, rb_easy_hkey(key)) == Qnil)
#define rb_easy_type_check(key,type) (rb_type(rb_hash_aref(rbce->opts, rb_easy_hkey(key))) == type)

// TODO: rb_sym_to_s may not be defined?
#define rb_easy_get_str(key) \
  RSTRING_PTR((rb_easy_type_check(key,T_STRING) ? rb_easy_get(key) : rb_str_to_str(rb_easy_get(key))))

/* getter/setter macros for various things */
/* setter for anything that stores a ruby VALUE in the struct */
#define CURB_OBJECT_SETTER(type, attr)                              \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            ptr->attr = attr;                                       \
                                                                    \
            return attr;

/* getter for anything that stores a ruby VALUE */
#define CURB_OBJECT_GETTER(type, attr)                              \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            return ptr->attr; 

/* setter for anything that stores a ruby VALUE in the struct opts hash */
#define CURB_OBJECT_HSETTER(type, attr)                             \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            rb_hash_aset(ptr->opts, rb_easy_hkey(#attr), attr);       \
                                                                    \
            return attr;

/* getter for anything that stores a ruby VALUE in the struct opts hash */
#define CURB_OBJECT_HGETTER(type, attr)                              \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            return rb_hash_aref(ptr->opts, rb_easy_hkey(#attr)); 

/* setter for bool flags */
#define CURB_BOOLEAN_SETTER(type, attr)                             \
            type *ptr;                                              \
            Data_Get_Struct(self, type, ptr);                       \
                                                                    \
            if (attr == Qnil || attr == Qfalse) {                   \
              ptr->attr = 0;                                        \
            } else {                                                \
              ptr->attr = 1;                                        \
            }                                                       \
                                                                    \
            return attr;

/* getter for bool flags */
#define CURB_BOOLEAN_GETTER(type, attr)                             \
            type *ptr;                                              \
            Data_Get_Struct(self, type, ptr);                       \
                                                                    \
            return((ptr->attr) ? Qtrue : Qfalse);

/* special setter for on_event handlers that take a block */
#define CURB_HANDLER_PROC_SETTER(type, handler)                     \
            type *ptr;                                              \
            VALUE oldproc;                                          \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
                                                                    \
            oldproc = ptr->handler;                                 \
            rb_scan_args(argc, argv, "0&", &ptr->handler);          \
                                                                    \
            return oldproc;                                         \

/* special setter for on_event handlers that take a block, same as above but stores int he opts hash */
#define CURB_HANDLER_PROC_HSETTER(type, handler)                        \
            type *ptr;                                                  \
            VALUE oldproc, newproc;                                     \
                                                                        \
            Data_Get_Struct(self, type, ptr);                           \
                                                                        \
            oldproc = rb_hash_aref(ptr->opts, rb_easy_hkey(#handler));   \
            rb_scan_args(argc, argv, "0&", &newproc);                   \
                                                                        \
            rb_hash_aset(ptr->opts, rb_easy_hkey(#handler), newproc);    \
                                                                        \
            return oldproc;

/* setter for numerics that are kept in c longs */
#define CURB_IMMED_SETTER(type, attr, nilval)                       \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            if (attr == Qnil) {                                     \
              ptr->attr = nilval;                                   \
            } else {                                                \
              ptr->attr = NUM2LONG(attr);                           \
            }                                                       \
                                                                    \
            return attr;                                            \

/* setter for numerics that are kept in c longs */
#define CURB_IMMED_GETTER(type, attr, nilval)                       \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            if (ptr->attr == nilval) {                              \
              return Qnil;                                          \
            } else {                                                \
              return LONG2NUM(ptr->attr);                           \
            }

/* special setter for port / port ranges */
#define CURB_IMMED_PORT_SETTER(type, attr, msg)                     \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            if (attr == Qnil) {                                     \
              ptr->attr = 0;                                        \
            } else {                                                \
              int port = NUM2INT(attr);                             \
                                                                    \
              if ((port) && ((port & 0xFFFF) == port)) {            \
                ptr->attr = port;                                   \
              } else {                                              \
                rb_raise(rb_eArgError, "Invalid " msg " %d (expected between 1 and 65535)", port); \
              }                                                     \
            }                                                       \
                                                                    \
            return attr;                                            \

/* special getter for port / port ranges */
#define CURB_IMMED_PORT_GETTER(type, attr)                          \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            if (ptr->attr == 0) {                                   \
              return Qnil;                                          \
            } else {                                                \
              return INT2NUM(ptr->attr);                            \
            }

#define CURB_DEFINE(name) \
  rb_define_const(mCurl, #name, LONG2NUM(name))

#endif
