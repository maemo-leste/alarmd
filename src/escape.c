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

#include "escape.h"

#include <stdarg.h>
#include <stdlib.h>

static inline int
itoc(int i)
{
  return (i < 10) ? ('0' + i) : ('a' - 10 + i);
}

static inline int
ctoi(int c)
{
  switch( c )
  {
  case '0' ... '9': return c - '0';
  case 'a' ... 'z': return c - 'a' + 10;
  case 'A' ... 'Z': return c - 'A' + 10;
  }
  return -1;
}

static inline int
esc_p(int c)
{
  return (c < 32u) || (c == '\\') || (c > 126u);
}

static inline int
output_esc(FILE *f, int c)
{
  char hex[5];

  switch( c )
  {
  case '\\': return fputs("\\\\", f);
  case '\b': return fputs("\\b", f);
  case '\n': return fputs("\\n", f);
  case '\r': return fputs("\\r", f);
  case '\t': return fputs("\\t", f);
  }

  hex[0] = '\\';
  hex[1] = 'x';
  hex[2] = itoc((c >> 4)&15);
  hex[3] = itoc((c >> 0)&15);
  hex[4] = 0;
  return fputs(hex, f);
}

int
escape_putline(FILE *file, const char *fmt, ...)
{
  int   err = -1;
  char *txt = 0;
  va_list va;

  va_start(va, fmt);

  if( vasprintf(&txt, fmt, va) == -1 )
  {
    goto cleanup;
  }

  for( char *pos = txt; *pos; ++pos )
  {
    if( esc_p(*pos) )
    {
      if( output_esc(file, *pos) == EOF )
      {
        goto cleanup;
      }
    }
    else
    {
      if( putc(*pos, file) == EOF )
      {
        goto cleanup;
      }
    }
  }

  if( putc('\n', file) != EOF )
  {
    err = 0;
  }

  cleanup:

  va_end(va);

  free(txt);

  return err;
}

int
escape_getline(FILE *file, char **pbuff, size_t *psize)
{
  int    err  = -1;
  char  *buff = *pbuff;
  size_t size = *psize;

  int n,l,h;

  if( (n = getline(&buff, &size, file)) == -1 )
  {
    goto cleanup;
  }

  while( (n > 0) && (buff[n-1] <= 32u) )
  {
    buff[--n] = 0;
  }

  char *src = buff;
  char *dst = buff;

  while( (n = *src++) != 0 )
  {
    if( n != '\\' )
    {
      *dst++ = n;
      continue;
    }

    switch( (n = *src++) )
    {
    case '\\': *dst++ = '\\'; break;
    case 'b':  *dst++ = '\b'; break;
    case 'n':  *dst++ = '\n'; break;
    case 'r':  *dst++ = '\r'; break;
    case 't':  *dst++ = '\t'; break;

    case 'x':
      if( (h = ctoi(*src++)) == -1 ) goto cleanup;
      if( (l = ctoi(*src++)) == -1 ) goto cleanup;
      *dst++ = (h << 4) | (l << 0);
      break;

    default:
      goto cleanup;
    }

  }

  *dst = 0;
  err = 0;

  cleanup:

  *pbuff = buff;
  *psize = size;

  return err;
}
