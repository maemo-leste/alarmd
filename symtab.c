#include "symtab.h"

#include <stdlib.h>

/* ========================================================================= *
 * symtab_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * symtab_lookup
 * ------------------------------------------------------------------------- */

void *
symtab_lookup(symtab_t *self, const void *key)
{
  for( int i = 0; i < self->st_count; ++i )
  {
    void *elem = self->st_elem[i];
    if( self->st_cmp(elem, key) )
    {
      return elem;
    }
  }
  return 0;
}

/* ------------------------------------------------------------------------- *
 * symtab_remove
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
symtab_remove(symtab_t *self, const void *key)
{
  size_t si = 0;
  size_t di = 0;

  for( ; si < self->st_count; ++si )
  {
    void *elem = self->st_elem[si];
    if( self->st_cmp(elem, key) )
    {
      self->st_del(elem);
    }
    else
    {
      self->st_elem[di++] = elem;
    }
  }

  self->st_count = di;
}
#endif

/* ------------------------------------------------------------------------- *
 * symtab_append
 * ------------------------------------------------------------------------- */

void
symtab_append(symtab_t *self, void *elem)
{
  if( self->st_count == self->st_alloc )
  {
    self->st_alloc += 16;
    self->st_elem = realloc(self->st_elem,
                            self->st_alloc * sizeof *self->st_elem);
  }
  self->st_elem[self->st_count++] = elem;
}

/* ------------------------------------------------------------------------- *
 * symtab_clear
 * ------------------------------------------------------------------------- */

void
symtab_clear(symtab_t *self)
{
  if( self->st_del != 0 )
  {
    for( int i = 0; i < self->st_count; ++i )
    {
      self->st_del(self->st_elem[i]);
    }
  }
  self->st_count = 0;
}

/* ------------------------------------------------------------------------- *
 * symtab_ctor
 * ------------------------------------------------------------------------- */

void
symtab_ctor(symtab_t *self, symtab_del_fn del, symtab_cmp_fn cmp)
{
  self->st_count = 0;
  self->st_alloc = 0;
  self->st_elem  = 0;
  self->st_del   = del;
  self->st_cmp   = cmp;
}

/* ------------------------------------------------------------------------- *
 * symtab_dtor
 * ------------------------------------------------------------------------- */

void
symtab_dtor(symtab_t *self)
{
  symtab_clear(self);
  free(self->st_elem);
}

/* ------------------------------------------------------------------------- *
 * symtab_create
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
symtab_t *
symtab_create(symtab_del_fn del, symtab_cmp_fn cmp)
{
  symtab_t *self = calloc(1, sizeof *self);
  symtab_ctor(self, del, cmp);
  return self;
}
#endif

/* ------------------------------------------------------------------------- *
 * symtab_delete
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
symtab_delete(symtab_t *self)
{
  if( self != 0 )
  {
    symtab_dtor(self);
    free(self);
  }
}
#endif

/* ------------------------------------------------------------------------- *
 * symtab_delete_cb
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
symtab_delete_cb(void *self)
{
  symtab_delete(self);
}
#endif
