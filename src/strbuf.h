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

#ifndef STRBUF_H_
#define STRBUF_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

typedef struct strbuf_t strbuf_t;

/* ------------------------------------------------------------------------- *
 * strbuf_t  --  buffer for encoding/decoding data to/from ascii strings
 * ------------------------------------------------------------------------- */

struct strbuf_t
{
  size_t  sb_head;
  size_t  sb_tail;
  size_t  sb_size;
  char   *sb_data;
};

/* ------------------------------------------------------------------------- *
 * strbuftypetags  --  encoding control characters used with strbuf
 * ------------------------------------------------------------------------- */

typedef enum strbuftypetags
{
  tag_error     = -1,
  tag_done      =  0,

  tag_int8      = 'b',
  tag_int16     = 'w',
  tag_int32     = 'l',
  tag_int64     = 'q',

  tag_uint8     = 'B',
  tag_uint16    = 'W',
  tag_uint32    = 'L',
  tag_uint64    = 'Q',

  tag_double    = 'd',
  tag_string    = 's',

  tag_bool      = 'F',
  tag_objpath   = 'O',
  tag_signature = 'S',
  tag_listbeg   = '[',
  tag_listend   = ']',

  tag_array     = tag_listbeg,

} strbuftypetags;

/* ------------------------------------------------------------------------- *
 * codec_t  --  type specific encoder/decoder functionality
 * ------------------------------------------------------------------------- */

typedef void (*sb_put_fn)(strbuf_t *, const void*);
typedef void (*sb_get_fn)(strbuf_t *, void*);

typedef struct codec_t codec_t;

struct codec_t
{
  int       tag;
  size_t    size;
  sb_put_fn put;
  sb_get_fn get;
};

/* ------------------------------------------------------------------------- *
 * extern strbuf methods
 * ------------------------------------------------------------------------- */

void  strbuf_ctor        (strbuf_t *self);
void  strbuf_ctor_ex     (strbuf_t *self, const char *init);
void  strbuf_dtor        (strbuf_t *self);
char *strbuf_steal       (strbuf_t *self);
void  strbuf_put_array   (strbuf_t *self, int tag, const void *aptr, size_t size);
void  strbuf_get_array   (strbuf_t *self, int tag, const void *aptr, size_t *psize);
int   strbuf_encode      (strbuf_t *self, int tag, ...);
int   strbuf_decode      (strbuf_t *self, int tag, ...);
int   strbuf_peek_type   (strbuf_t *self);
int   strbuf_peek_subtype(strbuf_t *self);

/* ------------------------------------------------------------------------- *
 * strbuf_get_text  --  get strbuf content as c string
 * ------------------------------------------------------------------------- */

static inline const char *strbuf_get_text(const strbuf_t *self)
{
  // always kept zero terminated
  return self->sb_data;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_unparsed  --  get unparsed strbuf content as c string
 * ------------------------------------------------------------------------- */

static inline const char *strbuf_get_unparsed(const strbuf_t *self)
{
  // always kept zero terminated
  return &self->sb_data[self->sb_head];
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_avail  --  number of unparsed chars left in strbuf
 * ------------------------------------------------------------------------- */

static inline size_t strbuf_get_avail(const strbuf_t *self)
{
  return self->sb_size - self->sb_tail;
}

#ifdef __cplusplus
};
#endif

#endif /* STRBUF_H_ */
