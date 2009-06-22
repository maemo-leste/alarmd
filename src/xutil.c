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

#include "xutil.h"
#include "logging.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

/* ------------------------------------------------------------------------- *
 * xscratchbox  --  are we running in scratchbox environment
 * ------------------------------------------------------------------------- */

int
xscratchbox(void)
{
  return access("/targets/links/scratchbox.config", F_OK) == 0;
}

/* ------------------------------------------------------------------------- *
 * xexists  --  file exists
 * ------------------------------------------------------------------------- */

int
xexists(const char *path)
{
  return access(path, F_OK) == 0;
}

/* ------------------------------------------------------------------------- *
 * xloadfile  --  load file in ram, zero padded for c string compatibility
 * ------------------------------------------------------------------------- */

int
xloadfile(const char *path, char **pdata, size_t *psize)
{
  int     err  = -1;
  int     file = -1;
  size_t  size = 0;
  char   *data = 0;

  struct stat st;

  if( (file = open(path, O_RDONLY)) == -1 )
  {
    log_warning_F("%s: open: %s\n", path, strerror(errno));
    goto cleanup;
  }

  if( fstat(file, &st) == -1 )
  {
    log_warning_F("%s: stat: %s\n", path, strerror(errno));
    goto cleanup;
  }

  size = st.st_size;

  // calloc size+1 -> text files are zero terminated

  if( (data = calloc(size+1, 1)) == 0 )
  {
    log_warning_F("%s: calloc %d: %s\n", path, size, strerror(errno));
    goto cleanup;
  }

  errno = 0;
  if( read(file, data, size) != size )
  {
    log_warning_F("%s: read: %s\n", path, strerror(errno));
    goto cleanup;
  }

  err = 0;

  cleanup:

  if( err  !=  0 ) free(data), data = 0, size = 0;
  if( file != -1 ) close(file);

  free(*pdata), *pdata = data, *psize = size;

  return err;
}

/* ------------------------------------------------------------------------- *
 * xsavefile  --  save buffer to file
 * ------------------------------------------------------------------------- */

int
xsavefile(const char *path, int mode, const void *data, size_t size)
{
  int     err  = -1;
  int     file = -1;

  if( (file = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1 )
  {
    log_error_F("%s: open: %s\n", path, strerror(errno));
    goto cleanup;
  }

  errno = 0;
  if( write(file, data, size) != size )
  {
    log_error_F("%s: write: %s\n", path, strerror(errno));
    goto cleanup;
  }

  if( fsync(file) == -1 )
  {
    log_error_F("%s: sync: %s\n", path, strerror(errno));
    goto cleanup;
  }

  err = 0;

  cleanup:

  if( file != -1 && close(file) == -1 )
  {
    log_error_F("%s: close: %s\n", path, strerror(errno));
    err = -1;
  }

  return err;
}

/* ------------------------------------------------------------------------- *
 * xcyclefiles  --  rename: temp -> current -> backup
 * ------------------------------------------------------------------------- */

int
xcyclefiles(const char *temp, const char *path, const char *back)
{
  /* - - - - - - - - - - - - - - - - - - - *
   * not foolproof, but certainly better
   * than directly overwriting datafiles
   * - - - - - - - - - - - - - - - - - - - */

  int err = -1;

  if( !xexists(temp) )
  {
    // temporary file does not exist == wtf
    log_warning_F("%s: missing new file: %s\n", temp, strerror(errno));
  }
  else
  {
    // 1. remove backup

    remove(back);

    // 2. previous -> backup

    if( xexists(path) )
    {
      if( rename(path, back) == -1 )
      {
        log_error("rename %s -> %s: %s\n", path, back, strerror(errno));
        goto done;
      }
    }

    // 3. temporary -> current

    if( rename(temp, path) == -1 )
    {
      log_error("rename %s -> %s: %s\n", temp, path, strerror(errno));

      if( xexists(back) )
      {
        // rollback
        rename(back, path);
      }
      goto done;
    }

    // success
    err = 0;
  }

  done:

  return err;
}

/* ------------------------------------------------------------------------- *
 * xfetchstats  --  get current stats for file
 * ------------------------------------------------------------------------- */

void
xfetchstats(const char *path, struct stat *cur)
{
  if( stat(path, cur) == -1 )
  {
    memset(cur, 0, sizeof *cur);
  }
}

/* ------------------------------------------------------------------------- *
 * xcheckstats  --  check if current stats differ from given
 * ------------------------------------------------------------------------- */

int
xcheckstats(const char *path, const struct stat *old)
{
  int ok = 1;

  struct stat cur;

  xfetchstats(path, &cur);

  if( old->st_dev   != cur.st_dev  ||
      old->st_ino   != cur.st_ino  ||
      old->st_size  != cur.st_size ||
      old->st_mtime != cur.st_mtime )
  {
    ok = 0;
  }

  return ok;
}
