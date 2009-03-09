#ifndef SYMTAB_H_
#define SYMTAB_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

typedef void (*symtab_del_fn)(void*);
typedef int  (*symtab_cmp_fn)(const void *,const void *);

typedef struct symtab_t   symtab_t;

/* ------------------------------------------------------------------------- *
 * symtab_t
 * ------------------------------------------------------------------------- */

struct symtab_t
{
  size_t  st_count;
  size_t  st_alloc;
  void  **st_elem;

  symtab_del_fn  st_del;
  symtab_cmp_fn  st_cmp;
};

void     *symtab_lookup   (symtab_t *self, const void *key);
void      symtab_remove   (symtab_t *self, const void *key);
void      symtab_append   (symtab_t *self, void *elem);
void      symtab_clear    (symtab_t *self);
void      symtab_ctor     (symtab_t *self, symtab_del_fn del, symtab_cmp_fn cmp);
void      symtab_dtor     (symtab_t *self);
symtab_t *symtab_create   (symtab_del_fn del, symtab_cmp_fn cmp);
void      symtab_delete   (symtab_t *self);
void      symtab_delete_cb(void *self);

#ifdef __cplusplus
};
#endif

#endif /* SYMTAB_H_ */
