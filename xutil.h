#ifndef XUTIL_H_
# define XUTIL_H_

#include <string.h>
#include <stdlib.h>

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

static inline int xisempty(const char *s)
{
  return (s == 0) || (*s == 0);
}

static inline int xiswhite(int c)
{
  return (c > 0u) && (c <= 32u);
}

static inline int xisblack(int c)
{
  return (c > 32u);
}

static inline int xissame(const char *s1, const char *s2)
{
  return !s1 ? !s2 : !s2 ? 0 : !strcmp(s1, s2);
}

static inline char *xstripall(char *str)
{
  // trim white space at start & end of string
  // compress whitespace in middle to one space

  char *s = str;
  char *d = str;

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

static inline void xstrset(char **pdst, const char *src)
{
  char *curr = src ? strdup(src) : 0;
  free(*pdst);
  *pdst = curr;
}

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
