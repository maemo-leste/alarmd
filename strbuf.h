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
 * strbuf_t
 * ------------------------------------------------------------------------- */

struct strbuf_t
{
  size_t  sb_head;
  size_t  sb_tail;
  size_t  sb_size;
  char   *sb_data;
};

static inline const char *strbuf_get_text(const strbuf_t *self)
{
  // always kept zero terminated
  return self->sb_data;
}

static inline const char *strbuf_get_unparsed(const strbuf_t *self)
{
  // always kept zero terminated
  return &self->sb_data[self->sb_head];
}

static inline size_t strbuf_get_avail(const strbuf_t *self)
{
  return self->sb_size - self->sb_tail;
}

enum strbuftypetags
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

void      strbuf_ctor         (strbuf_t *self);
void      strbuf_dtor         (strbuf_t *self);
void      strbuf_ctor_ex      (strbuf_t *self, const char *init);

char     *strbuf_steal        (strbuf_t *self);

int       strbuf_encode       (strbuf_t *self, int tag, ...);
int       strbuf_decode       (strbuf_t *self, int tag, ...);

int       strbuf_peek_type    (strbuf_t *self);
int       strbuf_peek_subtype (strbuf_t *self);

void      strbuf_put_array    (strbuf_t *self, int tag, const void *aptr, size_t size);
void      strbuf_get_array    (strbuf_t *self, int tag, const void *aptr, size_t *psize);

#if 0
#endif

// QUARANTINE strbuf_t *strbuf_create       (void);
// QUARANTINE void      strbuf_delete       (strbuf_t *self);
// QUARANTINE void      strbuf_delete_cb    (void *self);
// QUARANTINE void      strbuf_clear        (strbuf_t *self);
// QUARANTINE int       strbuf_peek         (const strbuf_t *self);
// QUARANTINE int       strbuf_pop          (strbuf_t *self);
// QUARANTINE int       strbuf_popif        (strbuf_t *self, int ch);
// QUARANTINE int       strbuf_pop_type     (strbuf_t *self);
// QUARANTINE void      strbuf_put_rawdata  (strbuf_t *self, const void *data, size_t size);
// QUARANTINE void      strbuf_put_rawbyte  (strbuf_t *self, int c);
// QUARANTINE void      strbuf_put_blob     (strbuf_t *self, const char **ptxt, size_t size);
// QUARANTINE void      strbuf_put_string   (strbuf_t *self, const char **ptxt);
// QUARANTINE void      strbuf_put_double   (strbuf_t *self, const double *pnum);
// QUARANTINE void      strbuf_put_uint64   (strbuf_t *self, const uint64_t *pnum);
// QUARANTINE void      strbuf_put_uint32   (strbuf_t *self, const uint32_t *pnum);
// QUARANTINE void      strbuf_put_uint16   (strbuf_t *self, const uint16_t *pnum);
// QUARANTINE void      strbuf_put_uint8    (strbuf_t *self, const uint8_t *pnum);
// QUARANTINE void      strbuf_put_int64    (strbuf_t *self, const int64_t *pnum);
// QUARANTINE void      strbuf_put_int32    (strbuf_t *self, const int32_t *pnum);
// QUARANTINE void      strbuf_put_int16    (strbuf_t *self, const int16_t *pnum);
// QUARANTINE void      strbuf_put_int8     (strbuf_t *self, const int8_t *pnum);
// QUARANTINE void      strbuf_get_string   (strbuf_t *self, char **ptxt);
// QUARANTINE void      strbuf_get_double   (strbuf_t *self, double *pnum);
// QUARANTINE void      strbuf_get_uint64   (strbuf_t *self, uint64_t *pnum);
// QUARANTINE void      strbuf_get_uint32   (strbuf_t *self, uint32_t *pnum);
// QUARANTINE void      strbuf_get_uint16   (strbuf_t *self, uint16_t *pnum);
// QUARANTINE void      strbuf_get_uint8    (strbuf_t *self, uint8_t *pnum);
// QUARANTINE void      strbuf_get_int64    (strbuf_t *self, int64_t *pnum);
// QUARANTINE void      strbuf_get_int32    (strbuf_t *self, int32_t *pnum);
// QUARANTINE void      strbuf_get_int16    (strbuf_t *self, int16_t *pnum);
// QUARANTINE void      strbuf_get_int8     (strbuf_t *self, int8_t *pnum);
// QUARANTINE int       strbuf_encode_valist(strbuf_t *self, int tag, va_list va);
// QUARANTINE int       strbuf_decode_valist(strbuf_t *self, int tag, va_list va);

#ifdef __cplusplus
};
#endif

#endif /* STRBUF_H_ */
