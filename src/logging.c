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

/* Make sure we compile this module to support
 * all functionality that can be available */
#undef  ENABLE_LOGGING
#define ENABLE_LOGGING 3
#include "logging.h"
#include "xutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define numof(a) (sizeof(a)/sizeof*(a))

#if ENABLE_LOGGING

typedef struct log_driver_t
{
  const char *name;
  void      (*open)(void);
  void      (*close)(void);
  void      (*write)(int, const char *, va_list);
} log_driver_t;

/* ========================================================================= *
 * SYSLOG
 * ========================================================================= */

static int         log_facility = LOG_USER;
static const char *log_identity = "<unknown>";
static FILE       *log_output   = 0;
static int         log_pid      = 0;

static int         log_level    = LOG_WARNING;

static void
dummy_open(void)
{
}

static void
dummy_close(void)
{
}

static void
dummy_write(int priority, const char *fmt, va_list va)
{
}

static void
syslog_open(void)
{
  openlog(log_identity, LOG_PID | LOG_NDELAY, log_facility);
}

static void
syslog_close(void)
{
  closelog();
}

static void
syslog_write(int priority, const char *fmt, va_list va)
{
  vsyslog(priority | log_facility, fmt, va);
}

static void
stream_open(void)
{
  char *path = 0;
  FILE *file = 0;

  asprintf(&path, "/tmp/%s.log", log_identity);

  if( path && (file = fopen(path, "w")) )
  {
    log_output = file;
  }
  free(path);
}

static void
stream_close(void)
{
  if( log_output != 0 )
  {
    fclose(log_output);
    log_output = 0;
  }
}

static void
stream_write(int priority, const char *fmt, va_list va)
{
  const char *desc = "Unknown";
  FILE       *file = log_output ?: stderr;
  char       *mesg = 0;

  switch( priority )
  {
  case LOG_EMERG:   desc = "Emergency";break;
  case LOG_ALERT:   desc = "Alert";    break;
  case LOG_CRIT:    desc = "Critical"; break;
  case LOG_ERR:     desc = "Error";    break;
  case LOG_WARNING: desc = "Warning";  break;
  case LOG_NOTICE:  desc = "Notice";   break;
  case LOG_INFO:    desc = "Info";     break;
  case LOG_DEBUG:   desc = "Debug";    break;
  }

  vasprintf(&mesg, fmt, va);
  if( mesg != 0 )
  {
    xstripall(mesg);
    fprintf(file, "%s[%d]: %s: %s\n", log_identity, log_pid, desc, mesg);
  }

  free(mesg);

// QUARANTINE   fprintf(file, "%s[%d]: %s: ", log_identity, log_pid, desc);
// QUARANTINE   vfprintf(file, fmt, va);
// QUARANTINE   fflush(file);
}

static void
stderr_open(void)
{
  // NOP
}

static void
stderr_close(void)
{
  // NOP
}

static log_driver_t log_drivers[] =
{
  {
    .name  = "stderr",
    .open  = stderr_open,
    .write = stream_write,
    .close = stderr_close
  },
  {
    .name  = "tmpfile",
    .open  = stream_open,
    .write = stream_write,
    .close = stream_close
  },
  {
    .name  = "syslog",
    .open  = syslog_open,
    .write = syslog_write,
    .close = syslog_close
  },
  {
    .name  = "dummy",
    .open  = dummy_open,
    .write = dummy_write,
    .close = dummy_close
  }
};

#if 01
// default to stderr
static log_driver_t *log_driver = log_drivers;
#else
// default to syslog
static log_driver_t *log_driver = &log_drivers[2];
#endif

static void
log_write(int pri, const char *fmt, va_list va)
{
  if( log_pid == 0 )
  {
    char path[256], *base;

    memset(path, 0, sizeof path);
    if( readlink("/proc/self/exe", path, sizeof path - 1) != -1 )
    {
      if( (base = strrchr(path, '/')) )
      {
        log_identity = strdup(base + 1);
      }
    }
    log_pid = getpid();
  }
  if( pri <= log_level )
  {
    log_driver->write(pri, fmt, va);
  }
}

void
log_open(const char *ident, int driver, int daemon)
{
  log_identity = strdup(ident);
  log_facility = daemon ? LOG_DAEMON : LOG_USER;
  log_pid      = getpid();

  if( driver < numof(log_drivers) )
  {
    log_driver = &log_drivers[driver];
  }
  log_driver->open();

#ifndef NDEBUG
  /* - - - - - - - - - - - - - - - - - - - *
   * If we are:
   * 1) running in scratchbox
   * 2) running in xterm
   * 3) stdin is not redirected
   *
   * -> set the xterm window label to
   *    show logging identity
   * - - - - - - - - - - - - - - - - - - - */

  if( access("/targets/links/scratchbox.config", F_OK) == 0 )
  {
    if( isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) )
    {
      if( strstr(getenv("TERM") ?: "", "xterm") )
      {
        fflush(0);
        fprintf(stdout, "\x1b]0;--( %s )--\x07", ident);
        fflush(stdout);
      }
    }
  }
#endif
}

void
log_reopen(int driver)
{
  log_driver_t *new_driver = 0;

  if( driver < numof(log_drivers) )
  {
    new_driver = &log_drivers[driver];

    if( new_driver != log_driver )
    {
      log_driver->close();
      log_driver = new_driver;
      log_driver->open();
    }
  }
}

void
log_close(void)
{
  log_driver->close();
}

void
log_critical(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_write(LOG_CRIT, fmt, va);
  va_end(va);
}

void
log_error(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_write(LOG_ERR, fmt, va);
  va_end(va);
}

void
log_warning(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_write(LOG_WARNING, fmt, va);
  va_end(va);
}

void
log_notice(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_write(LOG_NOTICE, fmt, va);
  va_end(va);
}

void
log_info(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_write(LOG_INFO, fmt, va);
  va_end(va);
}

void
log_debug(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  log_write(LOG_DEBUG, fmt, va);
  va_end(va);
}

void
log_set_level(int level)
{
  log_level = level;
}

#endif

typedef struct log_lut_t log_lut_t;

struct log_lut_t
{
  const char *key;
  int         val;
};

static
int
log_lut_lookup(const log_lut_t *lut, const char *key)
{
  int n = strlen(key);
  for( int i = 0; ; ++i )
  {
    if( lut[i].key == 0 || !strncasecmp(lut[i].key, key, n) )
    {
      return lut[i].val;
    }
  }
}

#ifdef DEAD_CODE
static
const char *
log_lut_rlookup(const log_lut_t *lut, int val)
{
  for( int i = 0; ; ++i )
  {
    if( (lut[i].val == val) || (lut[i].key == 0) )
    {
      return lut[i].key;
    }
  }
}
#endif

static
char **
log_lut_keys(const log_lut_t *lut)
{
  char **v = 0;
  int    N = 0;

  while( lut[N].key ) ++N;

  v = calloc(N+1, sizeof *v);

  for( int i = 0; i < N; ++i )
  {
    v[i] = strdup(lut[i].key);
  }

  return v;
}

const log_lut_t log_level_lut[] =
{
  { "EMERG",   LOG_EMERG   },
  { "ALERT",   LOG_ALERT   },
  { "CRIT",    LOG_CRIT    },
  { "ERR",     LOG_ERR     },
  { "WARNING", LOG_WARNING },
  { "NOTICE",  LOG_NOTICE  },
  { "INFO",    LOG_INFO    },
  { "DEBUG",   LOG_DEBUG   },

  { NULL,      LOG_WARNING },
};

/* ------------------------------------------------------------------------- *
 * log_parse_level
 * ------------------------------------------------------------------------- */

int
log_parse_level(const char *name)
{
  return log_lut_lookup(log_level_lut, name);
}

/* ------------------------------------------------------------------------- *
 * log_get_level_names
 * ------------------------------------------------------------------------- */

char **
log_get_level_names(void)
{
  // use xfreev() to release

  return log_lut_keys(log_level_lut);
}

/* ------------------------------------------------------------------------- *
 * log_parse_driver
 * ------------------------------------------------------------------------- */

int
log_parse_driver(const char *name)
{
  int N = numof(log_drivers);
  int n = strlen(name);

  for( int i = 0; i < N; ++i )
  {
    if( !strncasecmp(log_drivers[i].name, name, n) )
    {
      return i;
    }
  }

  return LOG_TO_DUMMY;
}

/* ------------------------------------------------------------------------- *
 * log_get_driver_names
 * ------------------------------------------------------------------------- */

char **
log_get_driver_names(void)
{
  // use xfreev() to release

  int    N = numof(log_drivers);
  char **v = calloc(N+1, sizeof *v);

  for( int i = 0; i < N; ++i )
  {
    v[i] = strdup(log_drivers[i].name);
  }
  return v;
}
