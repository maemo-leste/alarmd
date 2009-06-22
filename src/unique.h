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

#ifndef UNIQUE_H_
#define UNIQUE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

typedef struct unique_t unique_t;

/* ------------------------------------------------------------------------- *
 * unique_t
 * ------------------------------------------------------------------------- */

struct unique_t
{
  size_t un_count;
  size_t un_alloc;
  int    un_dirty;
  char **un_string;
};

void       unique_ctor     (unique_t *self);
void       unique_dtor     (unique_t *self);
unique_t * unique_create   (void);
void       unique_delete   (unique_t *self);
void       unique_delete_cb(void *self);
char     **unique_final    (unique_t *self, size_t *pcount);
char     **unique_steal    (unique_t *self, size_t *pcount);
void       unique_add      (unique_t *self, const char *str);

#ifdef __cplusplus
};
#endif

#endif /* UNIQUE_H_ */
