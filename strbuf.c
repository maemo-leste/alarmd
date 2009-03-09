#include "strbuf.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <assert.h>

#if 0

static int            strbuf_pop          (strbuf_t *self);
static int            strbuf_popif        (strbuf_t *self, int ch);
static void           strbuf_put_rawdata  (strbuf_t *self, const void *data, size_t size);
static void           strbuf_put_rawbyte  (strbuf_t *self, int c);
static void           strbuf_put_string   (strbuf_t *self, const char **ptxt);
static void           strbuf_put_double   (strbuf_t *self, const double *pnum);
static void           strbuf_put_uint64   (strbuf_t *self, const uint64_t *pnum);
static void           strbuf_put_uint32   (strbuf_t *self, const uint32_t *pnum);
static void           strbuf_put_uint16   (strbuf_t *self, const uint16_t *pnum);
static void           strbuf_put_uint8    (strbuf_t *self, const uint8_t *pnum);
static void           strbuf_put_int64    (strbuf_t *self, const int64_t *pnum);
static void           strbuf_put_int32    (strbuf_t *self, const int32_t *pnum);
static void           strbuf_put_int16    (strbuf_t *self, const int16_t *pnum);
static void           strbuf_put_int8     (strbuf_t *self, const int8_t *pnum);
static void           strbuf_get_string   (strbuf_t *self, char **ptxt);
static void           strbuf_get_double   (strbuf_t *self, double *pnum);
static void           strbuf_get_uint64   (strbuf_t *self, uint64_t *pnum);
static void           strbuf_get_uint32   (strbuf_t *self, uint32_t *pnum);
static void           strbuf_get_uint16   (strbuf_t *self, uint16_t *pnum);
static void           strbuf_get_uint8    (strbuf_t *self, uint8_t *pnum);
static void           strbuf_get_int64    (strbuf_t *self, int64_t *pnum);
static void           strbuf_get_int32    (strbuf_t *self, int32_t *pnum);
static void           strbuf_get_int16    (strbuf_t *self, int16_t *pnum);
static void           strbuf_get_int8     (strbuf_t *self, int8_t *pnum);
static const codec_t *strbuf_codec        (int tag);
static int            strbuf_encode_valist(strbuf_t *self, int tag, va_list va);
static int            strbuf_decode_valist(strbuf_t *self, int tag, va_list va);

void                  strbuf_ctor         (strbuf_t *self);
void                  strbuf_ctor_ex      (strbuf_t *self, const char *init);
void                  strbuf_dtor         (strbuf_t *self);
char                 *strbuf_steal        (strbuf_t *self);
int                   strbuf_encode       (strbuf_t *self, int tag, ...);
int                   strbuf_decode       (strbuf_t *self, int tag, ...);
void                  strbuf_put_array    (strbuf_t *self, int tag, const void *aptr, size_t size);
void                  strbuf_get_array    (strbuf_t *self, int tag, const void *aptr, size_t *psize);
int                   strbuf_peek_type    (strbuf_t *self);
int                   strbuf_peek_subtype (strbuf_t *self);
#endif

/* ========================================================================= *
 * Utilities
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * i2c
 * ------------------------------------------------------------------------- */

static inline int
i2c(unsigned i)
{
  return (i<10) ? ('0'+i) : ('a'+i-10);
}

/* ------------------------------------------------------------------------- *
 * c2i
 * ------------------------------------------------------------------------- */

static inline unsigned
c2i(int c)
{
  switch( c )
  {
  case '0' ... '9': return c - '0';
  case 'A' ... 'Z': return c - 'A' + 10;
  case 'a' ... 'z': return c - 'a' + 10;
  }
  return 255;
}

/* ------------------------------------------------------------------------- *
 * strbuf_escape_p
 * ------------------------------------------------------------------------- */

static inline int
strbuf_escape_p(int c)
{
  // strings on dbus must be "valid utf-8"
  // -> escape everything except for
  // printable ascii chars

  return (c == '\\') || (c < 32u) || (c > 126u);
}

/* ========================================================================= *
 * CONTROL METHODS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * strbuf_pop
 * ------------------------------------------------------------------------- */

static
int
strbuf_pop(strbuf_t *self)
{
  if( self->sb_head < self->sb_tail )
  {
    return self->sb_data[self->sb_head++];
  }
  return EOF;
}

/* ------------------------------------------------------------------------- *
 * strbuf_popif
 * ------------------------------------------------------------------------- */

static
int
strbuf_popif(strbuf_t *self, int ch)
{
  if( self->sb_head < self->sb_tail )
  {
    if( self->sb_data[self->sb_head] == ch )
    {
      self->sb_head++;
      return 1;
    }
  }
  return 0;
}

/* ========================================================================= *
 * PUT DATA METHODS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * strbuf_put_rawdata
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_rawdata(strbuf_t *self, const void *data, size_t size)
{
  size_t need = size + 1;
  size_t have = strbuf_get_avail(self);

  if( have < need )
  {
    self->sb_size += (need - have) + 32;
    self->sb_data = realloc(self->sb_data, self->sb_size);
  }

  memcpy(&self->sb_data[self->sb_tail], data, size);
  self->sb_tail += size;
  self->sb_data[self->sb_tail] = 0;
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_rawbyte
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_rawbyte(strbuf_t *self, int c)
{
  char data = (char)c;
  strbuf_put_rawdata(self, &data, 1);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_blob
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
void
strbuf_put_blob(strbuf_t *self, const char **ptxt, size_t size)
{
  const char *text = *ptxt;

  int n;

  for( ;; )
  {
    for( n = 0; n < size; ++n )
    {
      if( strbuf_escape_p(text[n]) ) break;
    }

    if( n > 0 )
    {
      strbuf_put_rawdata(self, text, n);
      text += n;
      size -= n;
    }

    if( size == 0 )
    {
      break;
    }

    if( *text == 0 )
    {
      break;
    }

    switch( (n = (unsigned char)*text++) )
    {
    case '\\': strbuf_put_rawdata(self, "\\\\", 2); break;
    case '\b': strbuf_put_rawdata(self, "\\b",  2); break;
    case '\n': strbuf_put_rawdata(self, "\\n",  2); break;
    case '\r': strbuf_put_rawdata(self, "\\r",  2); break;
    case '\t': strbuf_put_rawdata(self, "\\t",  2); break;

    default:
      strbuf_put_rawbyte(self, '\\');
      strbuf_put_rawbyte(self, 'x');
      strbuf_put_rawbyte(self, i2c((n>>4)&15));
      strbuf_put_rawbyte(self, i2c((n>>0)&15));
      break;
    }
  }
  strbuf_put_rawdata(self, "\\;", 2);
}
#endif

/* ------------------------------------------------------------------------- *
 * strbuf_put_string
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_string(strbuf_t *self, const char **ptxt)
{
  const char *text = *ptxt;

  int n,c;

  for( ;; )
  {
    for( n = 0; (c = (unsigned char)text[n]); ++n )
    {
      if( strbuf_escape_p(c) ) break;
    }

    //size_t n = strcspn(text, "\n\r\b\t\\");
    if( n > 0 )
    {
      strbuf_put_rawdata(self, text, n);
      text += n;
    }

    if( *text == 0 )
    {
      break;
    }

    switch( (n = (unsigned char)*text++) )
    {
    case '\\': strbuf_put_rawdata(self, "\\\\", 2); break;
    case '\b': strbuf_put_rawdata(self, "\\b",  2); break;
    case '\n': strbuf_put_rawdata(self, "\\n",  2); break;
    case '\r': strbuf_put_rawdata(self, "\\r",  2); break;
    case '\t': strbuf_put_rawdata(self, "\\t",  2); break;

    default:
      strbuf_put_rawbyte(self, '\\');
      strbuf_put_rawbyte(self, 'x');
      strbuf_put_rawbyte(self, i2c((n>>4)&15));
      strbuf_put_rawbyte(self, i2c((n>>0)&15));
      break;
    }
  }
  strbuf_put_rawdata(self, "\\;", 2);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_double
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_double(strbuf_t *self, const double *pnum)
{
  char tmp[64];
  int  len = snprintf(tmp, sizeof tmp, "%.16g;", *pnum);
  strbuf_put_rawdata(self, tmp, len);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_uint64
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_uint64(strbuf_t *self, const uint64_t *pnum)
{
  char tmp[64];
  int  len = snprintf(tmp, sizeof tmp, "%"PRIu64";", *pnum);
  strbuf_put_rawdata(self, tmp, len);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_uint32
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_uint32(strbuf_t *self, const uint32_t *pnum)
{
  char tmp[64];
  int  len = snprintf(tmp, sizeof tmp, "%"PRIu32";", *pnum);
  strbuf_put_rawdata(self, tmp, len);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_uint16
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_uint16(strbuf_t *self, const uint16_t *pnum)
{
  uint32_t num = *pnum;
  strbuf_put_uint32(self, &num);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_uint8
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_uint8(strbuf_t *self, const uint8_t *pnum)
{
  uint32_t num = *pnum;
  strbuf_put_uint32(self, &num);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_int64
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_int64(strbuf_t *self, const int64_t *pnum)
{
  char tmp[64];
  int  len = snprintf(tmp, sizeof tmp, "%"PRId64";", *pnum);
  strbuf_put_rawdata(self, tmp, len);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_int32
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_int32(strbuf_t *self, const int32_t *pnum)
{
  char tmp[64];
  int  len = snprintf(tmp, sizeof tmp, "%"PRId32";", *pnum);
  strbuf_put_rawdata(self, tmp, len);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_int16
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_int16(strbuf_t *self, const int16_t *pnum)
{
  int32_t num = *pnum;
  strbuf_put_int32(self, &num);
}

/* ------------------------------------------------------------------------- *
 * strbuf_put_int8
 * ------------------------------------------------------------------------- */

static
void
strbuf_put_int8(strbuf_t *self, const int8_t *pnum)
{
  int32_t num = *pnum;
  strbuf_put_int32(self, &num);
}

/* ========================================================================= *
 * GET DATA METHODS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * strbuf_get_string
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_string(strbuf_t *self, char **ptxt)
{
  char *res = NULL;
  strbuf_t temp;
  strbuf_ctor(&temp);
  int ch;

  for( ;; )
  {
    if( (ch = strbuf_pop(self)) == EOF ) break;

    if( ch != '\\' )
    {
      strbuf_put_rawbyte(&temp, ch);
      continue;
    }

    if( (ch = strbuf_pop(self)) == EOF ) break;

    if( ch == ';' ) break;

    switch( ch )
    {
    case '\\': strbuf_put_rawbyte(&temp, '\\'); break;
    case  'b': strbuf_put_rawbyte(&temp, '\b'); break;
    case  'n': strbuf_put_rawbyte(&temp, '\n'); break;
    case  'r': strbuf_put_rawbyte(&temp, '\r'); break;
    case  't': strbuf_put_rawbyte(&temp, '\t'); break;
    case  'x':
      {
        int h = c2i(strbuf_pop(self));
        int l = c2i(strbuf_pop(self));
        strbuf_put_rawbyte(&temp, l + h * 16);
      }
      break;

    default:
      assert( 0 );
      strbuf_put_rawbyte(&temp, '?');
      break;
    }
  }
  res = temp.sb_data;
  temp.sb_data = NULL;

  strbuf_dtor(&temp);

  *ptxt = res;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_double
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_double(strbuf_t *self, double *pnum)
{
  char   *s = &self->sb_data[self->sb_head];
  char   *e = s;
  double  v = strtod(s, &e);

  if( (e > s) && (*e == ';') )
  {
    self->sb_head += e + 1 - s;
  }
  else
  {
    abort();
  }
  *pnum = v;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_uint64
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_uint64(strbuf_t *self, uint64_t *pnum)
{
  char     *s = &self->sb_data[self->sb_head];
  char     *e = s;
  uint64_t  v = strtoull(s, &e, 10);

  if( (e > s) && (*e == ';') )
  {
    self->sb_head += e + 1 - s;
  }
  else
  {
    abort();
  }
  *pnum = v;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_uint32
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_uint32(strbuf_t *self, uint32_t *pnum)
{
  char     *s = &self->sb_data[self->sb_head];
  char     *e = s;
  uint32_t  v = strtoul(s, &e, 10);

  if( (e > s) && (*e == ';') )
  {
    self->sb_head += e + 1 - s;
  }
  else
  {
    abort();
  }
  *pnum = v;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_uint16
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_uint16(strbuf_t *self, uint16_t *pnum)
{
  uint32_t num = 0;
  strbuf_get_uint32(self, &num);
  *pnum = num;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_uint8
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_uint8(strbuf_t *self, uint8_t *pnum)
{
  uint32_t num = 0;
  strbuf_get_uint32(self, &num);
  *pnum = num;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_int64
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_int64(strbuf_t *self, int64_t *pnum)
{
  char     *s = &self->sb_data[self->sb_head];
  char     *e = s;
  int64_t  v  = strtoll(s, &e, 10);

  if( (e > s) && (*e == ';') )
  {
    self->sb_head += e + 1 - s;
  }
  else
  {
    abort();
  }
  *pnum = v;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_int32
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_int32(strbuf_t *self, int32_t *pnum)
{
  char     *s = &self->sb_data[self->sb_head];
  char     *e = s;
  int32_t  v  = strtol(s, &e, 10);

  if( (e > s) && (*e == ';') )
  {
    self->sb_head += e + 1 - s;
  }
  else
  {
    abort();
  }
  *pnum = v;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_int16
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_int16(strbuf_t *self, int16_t *pnum)
{
  int32_t num = 0;
  strbuf_get_int32(self, &num);
  *pnum = num;
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_int8
 * ------------------------------------------------------------------------- */

static
void
strbuf_get_int8(strbuf_t *self, int8_t *pnum)
{
  int32_t num = 0;
  strbuf_get_int32(self, &num);
  *pnum = num;
}

/* ========================================================================= *
 * CODEC
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * strbuf_codecs  --  lookup table
 * ------------------------------------------------------------------------- */

#define cat2(a,b) a##b

static const codec_t strbuf_codecs[] =
{
  { tag_bool, sizeof(uint32_t),
    (sb_put_fn)strbuf_put_uint32,
    (sb_get_fn)strbuf_get_uint32
  },

  { tag_double, sizeof(double),
    (sb_put_fn)strbuf_put_double,
    (sb_get_fn)strbuf_get_double
  },

  { tag_string, sizeof(char*),
    (sb_put_fn)strbuf_put_string,
    (sb_get_fn)strbuf_get_string
  },

  { tag_objpath, sizeof(char*),
    (sb_put_fn)strbuf_put_string,
    (sb_get_fn)strbuf_get_string
  },

  { tag_signature, sizeof(char*),
    (sb_put_fn)strbuf_put_string,
    (sb_get_fn)strbuf_get_string
  },

#define X(type) {\
  cat2(tag_,type), sizeof(cat2(type,_t)), \
  (sb_put_fn)strbuf_put_##type, \
  (sb_get_fn)strbuf_get_##type \
},

  X(int8)
  X(int16)
  X(int32)
  X(int64)

  X(uint8)
  X(uint16)
  X(uint32)
  X(uint64)

#undef X

  {tag_done, }
};

/* ------------------------------------------------------------------------- *
 * strbuf_codec
 * ------------------------------------------------------------------------- */

static
const codec_t *
strbuf_codec(int tag)
{
  const codec_t *res = 0;

  for( int i = 0; ; ++i )
  {
    if( strbuf_codecs[i].tag == tag_done )
    {
      log_warning("no codec for type tag '%c'\n", tag);
      break;
    }

    if( strbuf_codecs[i].tag == tag )
    {
      res = &strbuf_codecs[i];
      break;
    }
  }

  return res;
}

/* ========================================================================= *
 * strbuf_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * strbuf_ctor
 * ------------------------------------------------------------------------- */

void
strbuf_ctor(strbuf_t *self)
{
  self->sb_head = 0;
  self->sb_tail = 0;
  self->sb_size = 32;
  self->sb_data = malloc(self->sb_size);
  *self->sb_data = 0;
}

/* ------------------------------------------------------------------------- *
 * strbuf_ctor_ex
 * ------------------------------------------------------------------------- */

void
strbuf_ctor_ex(strbuf_t *self, const char *init)
{
  strbuf_ctor(self);
  strbuf_put_rawdata(self, init, strlen(init));
}

/* ------------------------------------------------------------------------- *
 * strbuf_dtor
 * ------------------------------------------------------------------------- */

void
strbuf_dtor(strbuf_t *self)
{
  free(self->sb_data);
}

/* ------------------------------------------------------------------------- *
 * strbuf_create
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
strbuf_t *
strbuf_create(void)
{
  strbuf_t *self = calloc(1, sizeof *self);
  strbuf_ctor(self);
  return self;
}
#endif

/* ------------------------------------------------------------------------- *
 * strbuf_delete
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
void
strbuf_delete(strbuf_t *self)
{
  if( self != 0 )
  {
    strbuf_dtor(self);
    free(self);
  }
}
#endif

/* ------------------------------------------------------------------------- *
 * strbuf_delete_cb
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
void
strbuf_delete_cb(void *self)
{
  strbuf_delete(self);
}
#endif

/* ------------------------------------------------------------------------- *
 * strbuf_steal
 * ------------------------------------------------------------------------- */

char *
strbuf_steal(strbuf_t *self)
{
  char *res = self->sb_data;
  self->sb_data = NULL;
  strbuf_dtor(self);
  strbuf_ctor(self);
  return res;
}

/* ------------------------------------------------------------------------- *
 * strbuf_clear
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
void
strbuf_clear(strbuf_t *self)
{
  self->sb_head = 0;
  self->sb_tail = 0;
  *self->sb_data = 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * strbuf_peek
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
int
strbuf_peek(const strbuf_t *self)
{
  if( self->sb_head < self->sb_tail )
  {
    return self->sb_data[self->sb_head];
  }
  return EOF;
}
#endif

/* ------------------------------------------------------------------------- *
 * strbuf_pop_type
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
int
strbuf_pop_type(strbuf_t *self)
{
  int tag = strbuf_pop(self);
  return (tag != EOF) ? tag : tag_done;
}
#endif

/* ------------------------------------------------------------------------- *
 * strbuf_put_array
 * ------------------------------------------------------------------------- */

void
strbuf_put_array(strbuf_t *self, int tag, const void *aptr, size_t size)
{
  const codec_t *cod = 0;
  const char    *pos = aptr;
  uint32_t       cnt = size;

  if( (cod = strbuf_codec(tag)) == 0 )
  {
    abort();
  }

  strbuf_put_rawbyte(self, tag_listbeg);
  strbuf_put_rawbyte(self, tag);
  strbuf_put_uint32(self, &cnt);
  for( size_t i = 0; i < size; ++i )
  {
    cod->put(self, pos);
    pos += cod->size;
  }
  strbuf_put_rawbyte(self, tag_listend);
}

/* ------------------------------------------------------------------------- *
 * strbuf_get_array
 * ------------------------------------------------------------------------- */

void
strbuf_get_array(strbuf_t *self, int tag, const void *aptr, size_t *psize)
{
  const codec_t *cod  = 0;
  uint32_t       cnt  = 0;
  char          *data = 0;
  char          *pos  = 0;

  if( (cod = strbuf_codec(tag)) == 0 )
  {
    abort();
  }

  if( !strbuf_popif(self, tag_listbeg) )
  {
    abort();
  }

  if( !strbuf_popif(self, tag) )
  {
    abort();
  }

  strbuf_get_uint32(self, &cnt);

  data = pos = calloc(cnt, cod->size);

  for( size_t i = 0; i < cnt; ++i )
  {
    cod->get(self, pos);
    pos += cod->size;
  }
  if( !strbuf_popif(self, tag_listend) )
  {
    abort();
  }

  *(void **)aptr = data;
  *psize = (size_t)cnt;
}

/* ------------------------------------------------------------------------- *
 * strbuf_encode_valist
 * ------------------------------------------------------------------------- */

static
int
strbuf_encode_valist(strbuf_t *self, int tag, va_list va)
{
  const codec_t *cod = 0;

  for( ; tag != tag_done; tag = va_arg(va, int) )
  {
    switch( tag )
    {
    case tag_listbeg:
      {
        int   elem  = va_arg(va, int);
        void *data  = va_arg(va, void *);
        size_t size = va_arg(va, size_t);

        strbuf_put_array(self, elem, data, size);
      }
      continue;
    }

    if( (cod = strbuf_codec(tag)) == 0 )
    {
      goto cleanup;
    }

    strbuf_put_rawbyte(self, tag);
    cod->put(self, va_arg(va, const void *));
  }

  cleanup:

  return tag;
}

/* ------------------------------------------------------------------------- *
 * strbuf_decode_valist
 * ------------------------------------------------------------------------- */

static
int
strbuf_decode_valist(strbuf_t *self, int tag, va_list va)
{
  const codec_t *cod = 0;

  for( ; tag != tag_done; tag = va_arg(va, int) )
  {
    switch( tag )
    {
    case tag_listbeg:
      {
        int     elem = va_arg(va, int);
        void   *data = va_arg(va, void *);
        size_t *size = va_arg(va, size_t*);

        strbuf_get_array(self, elem, data, size);
      }
      continue;
    }

    if( (cod = strbuf_codec(tag)) == 0 )
    {
      goto cleanup;
    }

    if( !strbuf_popif(self, tag) )
    {
      goto cleanup;
    }
    cod->get(self, va_arg(va, void *));
  }

  cleanup:

  return tag;
}

/* ------------------------------------------------------------------------- *
 * strbuf_encode
 * ------------------------------------------------------------------------- */

int
strbuf_encode(strbuf_t *self, int tag, ...)
{
  va_list va;

  va_start(va, tag);
  tag = strbuf_encode_valist(self, tag, va);
  va_end(va);
  return tag;
}

/* ------------------------------------------------------------------------- *
 * strbuf_decode
 * ------------------------------------------------------------------------- */

int
strbuf_decode(strbuf_t *self, int tag, ...)
{
  va_list va;

  va_start(va, tag);
  tag = strbuf_decode_valist(self, tag, va);
  va_end(va);
  return tag;
}

/* ------------------------------------------------------------------------- *
 * strbuf_peek_type
 * ------------------------------------------------------------------------- */

int
strbuf_peek_type(strbuf_t *self)
{
  if( self->sb_head < self->sb_tail )
  {
    return self->sb_data[self->sb_head];
  }
  return tag_done;
}

/* ------------------------------------------------------------------------- *
 * strbuf_peek_subtype
 * ------------------------------------------------------------------------- */

int
strbuf_peek_subtype(strbuf_t *self)
{
  if( self->sb_head+1 < self->sb_tail )
  {
    return self->sb_data[self->sb_head+1];
  }
  return tag_done;
}
