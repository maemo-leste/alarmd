#include "inifile.h"

#include "logging.h"
#include "unique.h"
#include "escape.h"

#include <errno.h>

#ifdef DEAD_CODE
# include <unistd.h>
#endif

/* ========================================================================= *
 * inival_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * inival_emit
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inival_emit(const inival_t *self, FILE *file)
{
  fprintf(file, "%s%c %s\n", self->iv_key, SEP, self->iv_val);
}
#endif

/* ------------------------------------------------------------------------- *
 * inival_set
 * ------------------------------------------------------------------------- */

void
inival_set(inival_t *self, const char *val)
{
  xstrset(&self->iv_val, val);
}

/* ------------------------------------------------------------------------- *
 * inival_create
 * ------------------------------------------------------------------------- */

inival_t *
inival_create(const char *key, const char *val)
{
  inival_t *self = calloc(1, sizeof *self);

  self->iv_key = strdup(key);
  self->iv_val = strdup(val);

  return self;
}

/* ------------------------------------------------------------------------- *
 * inival_delete
 * ------------------------------------------------------------------------- */

void
inival_delete(inival_t *self)
{
  if( self != 0 )
  {
    free(self->iv_key);
    free(self->iv_val);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * inival_compare
 * ------------------------------------------------------------------------- */

int
inival_compare(const inival_t *self, const char *key)
{
  return !strcmp(self->iv_key, key);
}

/* ------------------------------------------------------------------------- *
 * inival_compare_cb
 * ------------------------------------------------------------------------- */

int
inival_compare_cb(const void *self, const void *key)
{
  return inival_compare(self, key);
}

/* ------------------------------------------------------------------------- *
 * inival_delete_cb
 * ------------------------------------------------------------------------- */

void
inival_delete_cb(void *self)
{
  inival_delete(self);
}

/* ========================================================================= *
 * inisec_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * inisec_ctor
 * ------------------------------------------------------------------------- */

void
inisec_ctor(inisec_t *self)
{
  self->is_name   = 0;

  symtab_ctor(&self->is_values,
              inival_delete_cb,
              inival_compare_cb);
}

/* ------------------------------------------------------------------------- *
 * inisec_dtor
 * ------------------------------------------------------------------------- */

void
inisec_dtor(inisec_t *self)
{
  symtab_dtor(&self->is_values);

  free(self->is_name);
}

/* ------------------------------------------------------------------------- *
 * inisec_create
 * ------------------------------------------------------------------------- */

inisec_t *
inisec_create(const char *name)
{
  inisec_t *self = calloc(1, sizeof *self);
  inisec_ctor(self);

  inisec_set_name(self, name);

  return self;
}

/* ------------------------------------------------------------------------- *
 * inisec_delete
 * ------------------------------------------------------------------------- */

void
inisec_delete(inisec_t *self)
{
  if( self != 0 )
  {
    inisec_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * inisec_compare
 * ------------------------------------------------------------------------- */

int
inisec_compare(const inisec_t *self, const char *name)
{
  return !strcmp(self->is_name, name);
}

/* ------------------------------------------------------------------------- *
 * inisec_compare_cb
 * ------------------------------------------------------------------------- */

int
inisec_compare_cb(const void *self, const void *name)
{
  return inisec_compare(self, name);
}

/* ------------------------------------------------------------------------- *
 * inisec_delete_cb
 * ------------------------------------------------------------------------- */

void
inisec_delete_cb(void *self)
{
  inisec_delete(self);
}

/* ------------------------------------------------------------------------- *
 * inisec_set
 * ------------------------------------------------------------------------- */

void
inisec_set(inisec_t *self, const char *key, const char *val)
{
  inival_t *res = symtab_lookup(&self->is_values, key);
  if( res == 0 )
  {
    res = inival_create(key, val);
    symtab_append(&self->is_values, res);
  }
  else
  {
    inival_set(res, val);
  }
}

/* ------------------------------------------------------------------------- *
 * inisec_get
 * ------------------------------------------------------------------------- */

const char *
inisec_get(inisec_t *self, const char *key, const char *val)
{
  inival_t *res = symtab_lookup(&self->is_values, key);
  return res ? res->iv_val : val;
}

/* ------------------------------------------------------------------------- *
 * inisec_has
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
inisec_has(inisec_t *self, const char *key)
{
  return symtab_lookup(&self->is_values, key) != 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * inisec_del
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inisec_del(inisec_t *self, const char *key)
{
  symtab_remove(&self->is_values, key);
}
#endif

/* ------------------------------------------------------------------------- *
 * inisec_emit
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inisec_emit(const inisec_t *self, FILE *file)
{
  fprintf(file, "%c%s%c\n", BRA, self->is_name, KET);
  fprintf(file, "\n");

  for( int i = 0; i < self->is_values.st_count; ++i )
  {
    inival_emit(self->is_values.st_elem[i], file);
  }
  fprintf(file, "\n");
}
#endif

/* ========================================================================= *
 * inifile_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * inifile_get_path
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
const char *
inifile_get_path(inifile_t *self)
{
  return self->if_path;
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_set_path
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inifile_set_path(inifile_t *self, const char *path)
{
  xstrset(&self->if_path, path);
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_ctor
 * ------------------------------------------------------------------------- */

void
inifile_ctor(inifile_t *self)
{
  self->if_path     = 0;

  symtab_ctor(&self->if_sections,
              inisec_delete_cb,
              inisec_compare_cb);
}

/* ------------------------------------------------------------------------- *
 * inifile_dtor
 * ------------------------------------------------------------------------- */

void
inifile_dtor(inifile_t *self)
{
  symtab_dtor(&self->if_sections);

  free(self->if_path);
}

/* ------------------------------------------------------------------------- *
 * inifile_create
 * ------------------------------------------------------------------------- */

inifile_t *
inifile_create(void)
{
  inifile_t *self = calloc(1, sizeof *self);
  inifile_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * inifile_delete
 * ------------------------------------------------------------------------- */

void
inifile_delete(inifile_t *self)
{
  if( self != 0 )
  {
    inifile_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * inifile_delete_cb
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inifile_delete_cb(void *self)
{
  inifile_delete(self);
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_has_section
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
inifile_has_section(inifile_t *self, const char *sec)
{
  return symtab_lookup(&self->if_sections, sec) != 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_get_section
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
inisec_t *
inifile_get_section(inifile_t *self, const char *sec)
{
  return symtab_lookup(&self->if_sections, sec);
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_add_section
 * ------------------------------------------------------------------------- */

inisec_t *
inifile_add_section(inifile_t *self, const char *sec)
{
  inisec_t *res = symtab_lookup(&self->if_sections, sec);
  if( res == 0 )
  {
    res = inisec_create(sec);
    symtab_append(&self->if_sections, res);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * inifile_del_section
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inifile_del_section(inifile_t *self, const char *sec)
{
  symtab_remove(&self->if_sections, sec);
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_set
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inifile_set(inifile_t *self, const char *sec, const char *key, const char *val)
{
  inisec_set(inifile_add_section(self, sec), key, val);
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_setfmt
 * ------------------------------------------------------------------------- */

void
inifile_setfmt(inifile_t *self, const char *sec, const char *key, const char *fmt, ...)
{
  va_list va;
  char *val = 0;

  va_start(va, fmt);
  vasprintf(&val, fmt, va);
  va_end(va);

  if( val != 0 )
  {
    inisec_set(inifile_add_section(self, sec), key, val);
    free(val);
  }
}

/* ------------------------------------------------------------------------- *
 * inifile_get
 * ------------------------------------------------------------------------- */

const char *
inifile_get(inifile_t *self, const char *sec, const char *key, const char *val)
{
  inisec_t *s = symtab_lookup(&self->if_sections, sec);
  return s ? inisec_get(s, key, val) : val;
}

/* ------------------------------------------------------------------------- *
 * inifile_getfmt
 * ------------------------------------------------------------------------- */

int
inifile_getfmt(inifile_t *self, const char *sec, const char *key, const char *fmt, ...)
{
  const char *val = inifile_get(self, sec, key, "");
  int         res = 0;
  va_list     va;

  va_start(va, fmt);
  res = vsscanf(val, fmt, va);
  va_end(va);

  return res;
}

/* ------------------------------------------------------------------------- *
 * inifile_del
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inifile_del(inifile_t *self, const char *sec, const char *key)
{
  inisec_t *s = symtab_lookup(&self->if_sections, sec);
  if( s != 0 )
  {
    inisec_del(s, key);
  }
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_has
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
inifile_has(inifile_t *self, const char *sec, const char *key)
{
  inisec_t *s = symtab_lookup(&self->if_sections, sec);
  return s ? inisec_has(s, key) : 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_emit
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
inifile_emit(const inifile_t *self, FILE *file)
{
  for( size_t i = 0; i < self->if_sections.st_count; ++i )
  {
    inisec_emit(self->if_sections.st_elem[i], file);
  }
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_save_to_stream
 * ------------------------------------------------------------------------- */

static
int
inifile_save_to_stream(const inifile_t *self, FILE *file, const char *path)
{
  int   err  = -1;

  for( size_t i = 0; i < self->if_sections.st_count; ++i )
  {
    const inisec_t *sec = self->if_sections.st_elem[i];

    if( escape_putline(file, "%c%s%c", BRA, sec->is_name, KET) == -1 )
    {
      goto cleanup;
    }

    for( size_t j = 0; j < sec->is_values.st_count; ++j )
    {
      const inival_t *val = sec->is_values.st_elem[j];

      if( escape_putline(file, "%s%c %s", val->iv_key, SEP, val->iv_val) == -1 )
      {
        goto cleanup;
      }
    }
    escape_putline(file, "");
  }

  if( ferror(file) != 0 )
  {
    goto cleanup;
  }

  err = 0;

  cleanup:

  return err;
}

/* ------------------------------------------------------------------------- *
 * inifile_save
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
inifile_save(const inifile_t *self, const char *path)
{
  int   err  = -1;
  FILE *file = 0;

  if( (file = fopen(path, "w")) == 0 )
  {
    log_warning("%s: open: %s\n", path, strerror(errno));
    goto cleanup;
  }

  if( inifile_save_to_stream(self, file, path) == -1 )
  {
    log_warning("%s: write: %s\n", path, strerror(errno));
    goto cleanup;
  }

  if( fflush(file) == EOF )
  {
    log_warning("%s: flush: %s\n", path, strerror(errno));
  }

  if( fsync(fileno(file)) == -1 )
  {
    log_warning("%s: sync: %s\n", path, strerror(errno));
  }

  err = 0;

  cleanup:

  if( file != 0 )
  {
    if( fclose(file) == EOF )
    {
      log_warning("%s: fclose: %s\n", path, strerror(errno));
    }
  }
  return err;
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_save_to_memory
 * ------------------------------------------------------------------------- */

int
inifile_save_to_memory(const inifile_t *self, char **pdata, size_t *psize)
{
  int         err  = -1;
  FILE       *file = 0;
  const char *path = "<ram>";

  if( (file = open_memstream (pdata, psize)) == 0 )
  {
    log_warning("%s: open: %s\n", path, strerror(errno));
    goto cleanup;
  }

  if( inifile_save_to_stream(self, file, path) == -1 )
  {
    log_warning("%s: write: %s\n", path, strerror(errno));
    goto cleanup;
  }

  err = 0;

  cleanup:

  if( file != 0 )
  {
    if( fclose(file) == EOF )
    {
      log_warning("%s: close: %s\n", path, strerror(errno));
    }
  }
  return err;
}

/* ------------------------------------------------------------------------- *
 * inifile_load
 * ------------------------------------------------------------------------- */

int
inifile_load(inifile_t *self, const char *path)
{
  int     err  = -1;
  FILE   *file = 0;
  size_t  size = 0;
  char   *data = 0;

  inisec_t *sec = 0;
  char     *key  = 0;
  char     *val  = 0;

  if( (file = fopen(path, "r")) == 0 )
  {
    perror(path); goto cleanup;
  }

  for( ;; )
  {
    ssize_t n = escape_getline(file, &data, &size);

    if( n == -1 ) break;

    if( *data == 0 ) continue;

    if( *data == '#' ) continue;

    if( *data == BRA )
    {
      char *pos = data;
      xsplit(&pos, BRA);
      char *name = xstripall(xsplit(&pos, KET));

      sec = inifile_add_section(self, name);
      continue;
    }

    val = data;
    key = xsplit(&val, SEP);
    xstripall(key);
    xstrip(val);

    if( sec && *key )
    {
      inisec_set(sec, key, val);
    }
  }

  err = 0;

  cleanup:

  free(data);

  if( file != 0 ) fclose(file);

  return err;
}

/* ------------------------------------------------------------------------- *
 * inifile_scan_sections
 * ------------------------------------------------------------------------- */

inisec_t *
inifile_scan_sections(const inifile_t *self,
                      int (*cb)(const inisec_t*, void*),
                      void *aptr)
{
  for( size_t i = 0; i < self->if_sections.st_count; ++i )
  {
    inisec_t *sec = self->if_sections.st_elem[i];
    if( cb(sec, aptr) != 0 ) return sec;
  }
  return 0;
}

/* ------------------------------------------------------------------------- *
 * inifile_scan_values
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
inival_t *
inifile_scan_values(const inifile_t *self,
                    int (*cb)(const inisec_t *, const inival_t*, void*),
                    void *aptr)
{
  for( size_t i = 0; i < self->if_sections.st_count; ++i )
  {
    inisec_t *sec = self->if_sections.st_elem[i];

    for( size_t k = 0; k < sec->is_values.st_count; ++k )
    {
      inival_t *val = sec->is_values.st_elem[k];

      if( cb(sec, val, aptr) != 0 ) return val;
    }
  }
  return 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * inifile_get_section_names
 * ------------------------------------------------------------------------- */

static int
inifile_get_section_names_cb(const inisec_t *sec, void *aptr)
{
  unique_t *unique = aptr;
  unique_add(unique, sec->is_name);
  return 0;
}

char **
inifile_get_section_names(const inifile_t *self, size_t *pcount)
{
  char     **names = 0;
  unique_t   unique;

  unique_ctor(&unique);
  inifile_scan_sections(self, inifile_get_section_names_cb, &unique);
  names = unique_steal(&unique, pcount);
  unique_dtor(&unique);

  return names;
}

/* ------------------------------------------------------------------------- *
 * inifile_get_value_keys
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static int
inifile_get_value_keys_cb(const inisec_t *sec, const inival_t *val, void *aptr)
{
  unique_t *unique = aptr;
  unique_add(unique, val->iv_key);
  return 0;
}

char **
inifile_get_value_keys(const inifile_t *self, size_t *pcount)
{
  char     **names = 0;
  unique_t   unique;

  unique_ctor(&unique);
  inifile_scan_values(self, inifile_get_value_keys_cb, &unique);
  names = unique_steal(&unique, pcount);
  unique_dtor(&unique);
  return names;
}
#endif
