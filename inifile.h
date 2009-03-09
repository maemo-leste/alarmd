#ifndef INIFILE_H_
# define INIFILE_H_

#include <stdio.h>

#include "xutil.h"
#include "symtab.h"

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

enum {
  BRA = '[',
  KET = ']',
  SEP = ':',
};

typedef struct inifile_t inifile_t;
typedef struct inisec_t  inisec_t;
typedef struct inival_t  inival_t;

/* ------------------------------------------------------------------------- *
 * inival_t
 * ------------------------------------------------------------------------- */

struct inival_t
{
  char *iv_key;
  char *iv_val;
};

void      inival_emit      (const inival_t *self, FILE *file);
void      inival_set       (inival_t *self, const char *val);
inival_t *inival_create    (const char *key, const char *val);
void      inival_delete    (inival_t *self);
int       inival_compare   (const inival_t *self, const char *key);
int       inival_compare_cb(const void *self, const void *key);
void      inival_delete_cb (void *self);

/* ------------------------------------------------------------------------- *
 * inisec_t
 * ------------------------------------------------------------------------- */

struct inisec_t
{
  char      *is_name;
  symtab_t   is_values;
};

void        inisec_ctor      (inisec_t *self);
void        inisec_dtor      (inisec_t *self);
inisec_t   *inisec_create    (const char *name);
void        inisec_delete    (inisec_t *self);
int         inisec_compare   (const inisec_t *self, const char *name);
int         inisec_compare_cb(const void *self, const void *name);
void        inisec_delete_cb (void *self);
void        inisec_set       (inisec_t *self, const char *key, const char *val);
const char *inisec_get       (inisec_t *self, const char *key, const char *val);
int         inisec_has       (inisec_t *self, const char *key);
void        inisec_del       (inisec_t *self, const char *key);
void        inisec_emit      (const inisec_t *self, FILE *file);

static inline void inisec_set_name(inisec_t *self, const char *name)
{
  xstrset(&self->is_name, name);
}

static inline const char *inisec_get_name(inisec_t *self)
{
  return self->is_name;
}

/* ------------------------------------------------------------------------- *
 * inifile_t
 * ------------------------------------------------------------------------- */

struct inifile_t
{
  char      *if_path;
  symtab_t   if_sections;
};

const char * inifile_get_path         (inifile_t *self);
void         inifile_set_path         (inifile_t *self, const char *path);
void         inifile_ctor             (inifile_t *self);
void         inifile_dtor             (inifile_t *self);
inifile_t  * inifile_create           (void);
void         inifile_delete           (inifile_t *self);
void         inifile_delete_cb        (void *self);
int          inifile_has_section      (inifile_t *self, const char *sec);
inisec_t   * inifile_get_section      (inifile_t *self, const char *sec);
inisec_t   * inifile_add_section      (inifile_t *self, const char *sec);
void         inifile_del_section      (inifile_t *self, const char *sec);
void         inifile_set              (inifile_t *self, const char *sec, const char *key, const char *val);
void         inifile_setfmt           (inifile_t *self, const char *sec, const char *key, const char *fmt, ...);
const char * inifile_get              (inifile_t *self, const char *sec, const char *key, const char *val);
int          inifile_getfmt           (inifile_t *self, const char *sec, const char *key, const char *fmt, ...);
void         inifile_del              (inifile_t *self, const char *sec, const char *key);
int          inifile_has              (inifile_t *self, const char *sec, const char *key);
void         inifile_emit             (const inifile_t *self, FILE *file);
int          inifile_save             (const inifile_t *self, const char *path);
int          inifile_load             (inifile_t *self, const char *path);
inisec_t   * inifile_scan_sections    (const inifile_t *self, int (*cb)(const inisec_t*, void*), void *aptr);
inival_t   * inifile_scan_values      (const inifile_t *self, int (*cb)(const inisec_t *, const inival_t*, void*), void *aptr);
char       **inifile_get_section_names(const inifile_t *self, size_t *pcount);
char       **inifile_get_value_keys   (const inifile_t *self, size_t *pcount);
void         inifile_to_csv           (const inifile_t *self);
int          inifile_save_to_memory   (const inifile_t *self, char **pdata, size_t *psize);

# ifdef __cplusplus
};
# endif

#endif /* INIFILE_H_ */
