#include "unique.h"

#include <string.h>
#include <stdlib.h>

/* ========================================================================= *
 * unique_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * unique_ctor
 * ------------------------------------------------------------------------- */

void
unique_ctor(unique_t *self)
{
  self->un_count = 0;
  self->un_alloc = 64;
  self->un_string = malloc(self->un_alloc * sizeof *self->un_string);
  *self->un_string = 0;
}

/* ------------------------------------------------------------------------- *
 * unique_dtor
 * ------------------------------------------------------------------------- */

void
unique_dtor(unique_t *self)
{
  for( size_t i = 0; i < self->un_count; ++i )
  {
    free(self->un_string[i]);
  }
  free(self->un_string);
}

/* ------------------------------------------------------------------------- *
 * unique_create
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
unique_t *
unique_create(void)
{
  unique_t *self = calloc(1, sizeof *self);
  unique_ctor(self);
  return self;
}
#endif

/* ------------------------------------------------------------------------- *
 * unique_delete
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
unique_delete(unique_t *self)
{
  if( self != 0 )
  {
    unique_dtor(self);
    free(self);
  }
}
#endif

/* ------------------------------------------------------------------------- *
 * unique_delete_cb
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
unique_delete_cb(void *self)
{
  unique_delete(self);
}
#endif

/* ------------------------------------------------------------------------- *
 * unique_final
 * ------------------------------------------------------------------------- */

char **
unique_final(unique_t *self, size_t *pcount)
{
  auto int cmp(const void *a, const void *b);

  auto int cmp(const void *a, const void *b)
  {
    return strcmp(*(const char **)a, *(const char **)b);
  }

  if( self->un_dirty != 0 )
  {
    qsort(self->un_string, self->un_count, sizeof *self->un_string, cmp);

    size_t si = 0, di = 0;

    char *prev = NULL;

    while( si < self->un_count )
    {
      char *curr = self->un_string[si++];
      if( prev == 0 || strcmp(prev, curr) )
      {
        self->un_string[di++] = prev = curr;
      }
      else
      {
        free(curr);
      }
    }
    self->un_count = di;
    self->un_dirty = 0;
    self->un_string[self->un_count] = 0;
  }

  if( pcount != 0 )
  {
    *pcount = self->un_count;
  }

  return self->un_string;
}

/* ------------------------------------------------------------------------- *
 * unique_steal
 * ------------------------------------------------------------------------- */

char **
unique_steal(unique_t *self, size_t *pcount)
{
  char **res = unique_final(self, pcount);
  self->un_count  = 0;
  self->un_alloc  = 0;
  self->un_dirty  = 0;
  self->un_string = 0;
  return res;
}

/* ------------------------------------------------------------------------- *
 * unique_add
 * ------------------------------------------------------------------------- */

void
unique_add(unique_t *self, const char *str)
{
  if( self->un_count + 2 > self->un_alloc )
  {
    if( self->un_alloc == 0 )
    {
      self->un_alloc = 32;
    }
    else
    {
      self->un_alloc = self->un_alloc * 3 / 2;
    }
    self->un_string = realloc(self->un_string,
                              self->un_alloc * sizeof *self->un_string);
  }

  self->un_string[self->un_count++] = strdup(str);
  self->un_string[self->un_count] = 0;
  self->un_dirty = 1;
}
