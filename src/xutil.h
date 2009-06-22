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

#ifndef XUTIL_H_
# define XUTIL_H_

# include <sys/types.h>
# include <sys/stat.h>

# include <string.h>
# include <stdlib.h>

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

/* ========================================================================= *
 * EXTERN FUNCTIONS
 * ========================================================================= */

int  xscratchbox(void);
int  xexists(const char *path);
int  xloadfile(const char *path, char **pdata, size_t *psize);
int  xsavefile(const char *path, int mode, const void *data, size_t size);
int  xcyclefiles(const char *temp, const char *path, const char *back);
void xfetchstats(const char *path, struct stat *cur);
int  xcheckstats(const char *path, const struct stat *old);

/* ========================================================================= *
 * INLINE FUNCTIONS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * xisempty  --  test if string is NULL or empty
 * ------------------------------------------------------------------------- */

static inline int xisempty(const char *s)
{
  return (s == 0) || (*s == 0);
}

/* ------------------------------------------------------------------------- *
 * xiswhite  --  quick and dirty white character check
 * ------------------------------------------------------------------------- */

static inline int xiswhite(int c)
{
  return (c > 0u) && (c <= 32u);
}

/* ------------------------------------------------------------------------- *
 * xiswhite  --  quick and dirty non-white character check
 * ------------------------------------------------------------------------- */

static inline int xisblack(int c)
{
  return (c > 32u);
}

/* ------------------------------------------------------------------------- *
 * xissame  --  test if the given strings are the same, NULLs allowed
 * ------------------------------------------------------------------------- */

static inline int xissame(const char *s1, const char *s2)
{
  return !s1 ? !s2 : !s2 ? 0 : !strcmp(s1, s2);
}

/* ------------------------------------------------------------------------- *
 * xstripall  --  trim whitespace at both ends, compress to ' ' in middle
 * ------------------------------------------------------------------------- */

static inline char *xstripall(char *str)
{
  // trim white space at start & end of string
  // compress whitespace in middle to one space

  char *s = str;
  char *d = str;

  while( xiswhite(*s) ) { ++s; }

  for( ;; )
  {
    while( xisblack(*s) ) { *d++ = *s++; }

    while( xiswhite(*s) ) { ++s; }

    if( *s == 0 )
    {
      *d = 0;
      return str;
    }

    *d++ = ' ';
  }
}

/* ------------------------------------------------------------------------- *
 * xstrip  --  trim whitespace at both ends
 * ------------------------------------------------------------------------- */

static inline char *xstrip(char *str)
{
  // trim white space at start & end of string
  char *s = str;
  char *d = str;

  while( xiswhite(*s) ) { ++s; }

  char *e = str;

  while( *s )
  {
    if( xisblack(*s) ) e = d+1;
    *d++ = *s++;
  }
  *e = 0;
  return str;
}

/* ------------------------------------------------------------------------- *
 * xsplit  --  split string at given separator char
 * ------------------------------------------------------------------------- */

static inline char *xsplit(char **ppos, int sep)
{
  char *res = *ppos;
  char *pos = *ppos;

  for( ; *pos != 0; ++pos )
  {
    if( *pos == sep )
    {
      *pos++ = 0;
      break;
    }
  }

  *ppos = pos;

  return res;
}

/* ------------------------------------------------------------------------- *
 * xstrset  --  replace dynamic string at given address
 * ------------------------------------------------------------------------- */

static inline void xstrset(char **pdst, const char *src)
{
  char *curr = src ? strdup(src) : 0;
  free(*pdst);
  *pdst = curr;
}

/* ------------------------------------------------------------------------- *
 * xfreev  --  free array of strings
 * ------------------------------------------------------------------------- */

static inline void xfreev(char **v)
{
  if( v != 0 )
  {
    for( size_t i = 0; v[i]; ++i )
    {
      free(v[i]);
    }
    free(v);
  }
}

# ifdef __cplusplus
};
# endif

#endif /* XUTIL_H_ */
