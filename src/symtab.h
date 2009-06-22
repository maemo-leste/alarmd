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
