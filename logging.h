/* ========================================================================= *
 * File: logging.h
 *
 * Copyright (C) 2007 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 30-Nov-2007 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#ifndef LOGGING_H_
# define LOGGING_H_

#include <stdarg.h>
#include <syslog.h>

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

enum
{
  LOG_TO_STDERR = 0,
  LOG_TO_TMP    = 1,
  LOG_TO_SYSLOG = 2,
  LOG_TO_DUMMY  = 3,
};

#if ENABLE_LOGGING

/* ------------------------------------------------------------------------- *
 * logging enabled
 * ------------------------------------------------------------------------- */

void log_open       (const char *ident, int driver, int daemon);
void log_close      (void);
void log_reopen     (int driver);
void log_set_level  (int level);

void log_critical   (const char *fmt, ...);
void log_error      (const char *fmt, ...);
void log_warning    (const char *fmt, ...);

#if ENABLE_LOGGING >= 2
void log_info       (const char *fmt, ...);
#else
# define log_info(...)        do {} while (0)
#endif
#if ENABLE_LOGGING >= 3
void log_debug      (const char *fmt, ...);
#else
# define log_debug(...)       do {} while (0)
#endif

#  define log_critical_L(FMT, ARG...) log_critical("%s:%d: "FMT, __FILE__, __LINE__, ## ARG)
#  define log_error_L(FMT, ARG...)    log_error("%s:%d: "FMT, __FILE__, __LINE__, ## ARG)
#  define log_warning_L(FMT, ARG...)  log_warning("%s:%d: "FMT, __FILE__, __LINE__, ## ARG)
#  define log_info_L(FMT, ARG...)     log_info("%s:%d: "FMT, __FILE__, __LINE__, ## ARG)
#  define log_debug_L(FMT, ARG...)    log_debug("%s:%d: "FMT, __FILE__, __LINE__, ## ARG)

#  define log_critical_F(FMT, ARG...) log_critical("%s:%d: "FMT, __FUNCTION__, __LINE__, ## ARG)
#  define log_error_F(FMT, ARG...)    log_error("%s:%d: "FMT, __FUNCTION__, __LINE__, ## ARG)
#  define log_warning_F(FMT, ARG...)  log_warning("%s:%d: "FMT, __FUNCTION__, __LINE__, ## ARG)
#  define log_info_F(FMT, ARG...)     log_info("%s:%d: "FMT, __FUNCTION__, __LINE__, ## ARG)
#  define log_debug_F(FMT, ARG...)    log_debug("%s:%d: "FMT, __FUNCTION__, __LINE__, ## ARG)

# else

/* ------------------------------------------------------------------------- *
 * logging disabled
 * ------------------------------------------------------------------------- */
#  define log_open(id,dr,da)   do {} while (0)
#  define log_close()          do {} while (0)
#  define log_set_level(lev)   do {} while (0)
#  define log_reopen(dr)       do {} while (0)

#  define log_critical(...)    do {} while (0)
#  define log_error(...)       do {} while (0)
#  define log_warning(...)     do {} while (0)
#  define log_info(...)        do {} while (0)
#  define log_debug(...)       do {} while (0)

#  define log_critical_L(...)  do {} while (0)
#  define log_error_L(...)     do {} while (0)
#  define log_warning_L(...)   do {} while (0)
#  define log_info_L(...)      do {} while (0)
#  define log_debug_L(...)     do {} while (0)

#  define log_critical_F(...)  do {} while (0)
#  define log_error_F(...)     do {} while (0)
#  define log_warning_F(...)   do {} while (0)
#  define log_info_F(...)      do {} while (0)
#  define log_debug_F(...)     do {} while (0)

# endif

/* ------------------------------------------------------------------------- *
 * utility functions
 * ------------------------------------------------------------------------- */

int    log_parse_driver(const char *name);
int    log_parse_level (const char *name);

char **log_get_level_names (void);
char **log_get_driver_names(void);

# ifdef __cplusplus
};
# endif

#endif /* LOGGING_H_ */
