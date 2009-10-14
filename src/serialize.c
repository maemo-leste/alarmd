/* ========================================================================= *
 *
 * This file is part of Alarmd
 *
 * Copyright (C) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * Alarmd is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Alarmd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Alarmd; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * ========================================================================= */

#include "alarmd_config.h"

#include "serialize.h"

#include "dbusif.h"
#include "strbuf.h"
#include "logging.h"
#include "xutil.h"

#include <assert.h>

/* ------------------------------------------------------------------------- *
 * strbuf serialization types <--> dbus data types
 * ------------------------------------------------------------------------- */

static const struct
{
  int stype,dtype;
} typemap [] =
{
  { tag_bool,      DBUS_TYPE_BOOLEAN },

  { tag_uint8,     DBUS_TYPE_BYTE   },

  { tag_uint16,    DBUS_TYPE_UINT16 },
  { tag_uint32,    DBUS_TYPE_UINT32 },
  { tag_uint64,    DBUS_TYPE_UINT64 },

  { tag_int16,     DBUS_TYPE_INT16 },
  { tag_int32,     DBUS_TYPE_INT32 },
  { tag_int64,     DBUS_TYPE_INT64 },

  { tag_double,    DBUS_TYPE_DOUBLE },
  { tag_string,    DBUS_TYPE_STRING },
  { tag_objpath,   DBUS_TYPE_OBJECT_PATH },
  { tag_signature, DBUS_TYPE_SIGNATURE },
};

/* ------------------------------------------------------------------------- *
 * serialize_strbuf_type_to_dbus  --  convert strbuf type to dbus type
 * ------------------------------------------------------------------------- */

static
int
serialize_strbuf_type_to_dbus(int stype)
{
  for( size_t i=0; i < sizeof typemap/sizeof *typemap; ++i )
  {
    if( typemap[i].stype == stype )
    {
      return typemap[i].dtype;
    }
  }
  return -1;
}

/* ------------------------------------------------------------------------- *
 * serialize_dbus_type_to_strbuf  --  convert dbus type to strbuf type
 * ------------------------------------------------------------------------- */

static
int
serialize_dbus_type_to_strbuf(int dtype)
{
  for( size_t i=0; i < sizeof typemap/sizeof *typemap; ++i )
  {
    if( typemap[i].dtype == dtype )
    {
      return typemap[i].stype;
    }
  }
  return -1;
}

/* ------------------------------------------------------------------------- *
 * serialize_pack_strbuf_from_dbus_args  --  dbus_message_append_args compatibility
 * ------------------------------------------------------------------------- */

static
int
serialize_pack_strbuf_from_dbus_args(int mtype, va_list va, strbuf_t *strbuf)
{
  int error = 0;
  int etype = 0;
  int stype = 0;

  for( ; mtype != DBUS_TYPE_INVALID; mtype = va_arg(va, int) )
  {
    // REF: dbus_message_append_args()
    // --> dbus_message_append_args_valist()

    if( mtype == DBUS_TYPE_ARRAY )
    {
      etype = va_arg(va, int);
      stype = serialize_dbus_type_to_strbuf(etype);

      if( stype != -1 && dbus_type_is_basic(etype) )
      {
        void **ptr = va_arg(va, void **);
        int    cnt = va_arg(va, int);
        strbuf_encode(strbuf, tag_array, stype, *ptr, cnt, tag_done);
        continue;
      }

      log_error_F("unhandled array of '%s'\n", dbusif_get_dtatype_name(etype));
      error = -1;
      break;
    }

    if( dbus_type_is_basic(mtype) )
    {
      if( (stype = serialize_dbus_type_to_strbuf(mtype)) != -1 )
      {
        strbuf_encode(strbuf, stype, va_arg(va, void*), tag_done);
        continue;
      }
    }

    log_error_F("unhandled type '%s'\n", dbusif_get_dtatype_name(mtype));
    error = -1;
    goto done;
  }

  done:

  return error;
}

/* ------------------------------------------------------------------------- *
 * serialize_unpack_strbuf_to_dbus_message  --  deserialize data to dbus message iterator
 * ------------------------------------------------------------------------- */

static
int
serialize_unpack_strbuf_to_dbus_message(strbuf_t *strbuf, DBusMessageIter *msgiter)
{
  int  error = -1;
  int  mtype = 0;
  int  etype = 0;
  int  dtype = 0;
  char dsign[2];

  for( ;; )
  {
    int ok = 0; // assume snafu

    // End of message reached?

    if( (mtype = strbuf_peek_type(strbuf)) == tag_done )
    {
      break;
    }

    // My eyes are hurting from ugliness of all things dbus
    // (hopefully Coverity is happy though)

    if( mtype == tag_array )
    {
      // handle array of elements

      etype = strbuf_peek_subtype(strbuf);

      if( (dtype = serialize_strbuf_type_to_dbus(etype)) == -1 )
      {
        goto cleanup;
      }

      dsign[0] = dtype;
      dsign[1] = 0;

      if( dbus_type_is_fixed(dtype) )
      {
        DBusMessageIter sub;
        size_t cnt = 0;
        void  *vec = 0;
        strbuf_get_array(strbuf, etype, &vec, &cnt);

        if( (ok = dbus_message_iter_open_container(msgiter, DBUS_TYPE_ARRAY,
                                                   dsign, &sub)) )
        {
          if( !dbus_message_iter_append_fixed_array(&sub, dtype, &vec, cnt) )
          {
            ok = 0;
          }
          if( !dbus_message_iter_close_container(msgiter, &sub) )
          {
            ok = 0;
          }
        }

        free(vec);
      }
      else if( dbus_type_is_basic(dtype) )
      {
        DBusMessageIter sub;
        void   *vec = 0;
        size_t  cnt = 0;
        strbuf_get_array(strbuf, etype, &vec, &cnt);

        if( (ok = dbus_message_iter_open_container(msgiter, DBUS_TYPE_ARRAY,
                                                   dsign, &sub)) )
        {
          for( size_t i = 0; i < cnt; ++i )
          {
            char *str = ((char **)vec)[i];
            if( !dbus_message_iter_append_basic(&sub, dtype, &str) )
            {
              ok = 0; break;
            }
          }
          if( !dbus_message_iter_close_container(msgiter, &sub) )
          {
            ok = 0;
          }
        }
        xfreev(vec);
      }
    }
    else
    {
      // handle individual element

      if( (dtype = serialize_strbuf_type_to_dbus(mtype)) == -1 )
      {
        goto cleanup;
      }

      if( dbus_type_is_fixed(dtype) )
      {
        uint64_t data = 0;
        strbuf_decode(strbuf, mtype, &data, tag_done);
        ok = dbus_message_iter_append_basic(msgiter, dtype, &data);
      }
      else if( dbus_type_is_basic(dtype) )
      {
        char *data = 0;
        strbuf_decode(strbuf, mtype, &data, tag_done);
        ok = dbus_message_iter_append_basic(msgiter, dtype, &data);
        free(data);
      }
    }

    // bailout on errors
    if( !ok ) goto cleanup;
  }

  error = 0;

  cleanup:

  return error;
}

/* ------------------------------------------------------------------------- *
 * serialize_unpack_to_mesg
 * ------------------------------------------------------------------------- */

int
serialize_unpack_to_mesg(const char *args, DBusMessage *msg)
{
  int             err = -1;
  DBusMessageIter iter;
  strbuf_t        sbuf;

  strbuf_ctor_ex(&sbuf, args);

  dbus_message_iter_init_append(msg, &iter);
  err = serialize_unpack_strbuf_to_dbus_message(&sbuf, &iter);

  strbuf_dtor(&sbuf);

  return err;
}

/* ------------------------------------------------------------------------- *
 * serialize_pack_dbus_args
 * ------------------------------------------------------------------------- */

char *
serialize_pack_dbus_args(int type, va_list va)
{
  char     *args = 0;
  strbuf_t  sbuf;

  strbuf_ctor(&sbuf);

  if( serialize_pack_strbuf_from_dbus_args(type, va, &sbuf) != -1 )
  {
    args = strbuf_steal(&sbuf);
  }

  strbuf_dtor(&sbuf);

  return args;
}
