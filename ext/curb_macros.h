/* Curb - helper macros for ruby integration
 * Copyright (c)2006 Ross Bamford. 
 * Licensed under the Ruby License. See LICENSE for details.
 * 
 * $Id: curb_macros.h 13 2006-11-23 23:54:25Z roscopeco $
 */

#ifndef __CURB_MACROS_H
#define __CURB_MACROS_H

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

/* setter for numerics that are kept in c ints */
#define CURB_IMMED_SETTER(type, attr, nilval)                       \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            if (attr == Qnil) {                                     \
              ptr->attr = nilval;                                   \
            } else {                                                \
              ptr->attr = NUM2INT(attr);                            \
            }                                                       \
                                                                    \
            return attr;                                            \

/* setter for numerics that are kept in c ints */
#define CURB_IMMED_GETTER(type, attr, nilval)                       \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            if (ptr->attr == nilval) {                              \
              return Qnil;                                          \
            } else {                                                \
              return INT2NUM(ptr->attr);                            \
            }

/* special setter for port / port ranges */
#define CURB_IMMED_PORT_SETTER(type, attr, msg)                     \
            type *ptr;                                              \
                                                                    \
            Data_Get_Struct(self, type, ptr);                       \
            if (attr == Qnil) {                                     \
              ptr->attr = 0;                                        \
            } else {                                                \
              int port = FIX2INT(attr);                             \
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
              return INT2FIX(ptr->attr);                            \
            }

#endif
