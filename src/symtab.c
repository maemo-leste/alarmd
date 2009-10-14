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

symtab_t *
symtab_create(symtab_del_fn del, symtab_cmp_fn cmp)
{
  symtab_t *self = calloc(1, sizeof *self);
  symtab_ctor(self, del, cmp);
  return self;
}

/* ------------------------------------------------------------------------- *
 * symtab_delete
 * ------------------------------------------------------------------------- */

void
symtab_delete(symtab_t *self)
{
  if( self != 0 )
  {
    symtab_dtor(self);
    free(self);
  }
}

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
