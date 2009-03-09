/* ========================================================================= *
 * File: sighnd.c
 *
 * Copyright (C) 2007 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 06-Dec-2007 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include "sighnd.h"
#include "logging.h"
#include "mainloop.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* ========================================================================= *
 * SIGNAL HANDING FUNCTIONALITY
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * sighnd_terminate  --  terminate process from signal handler
 * ------------------------------------------------------------------------- */

static
void
sighnd_terminate(void)
{
  static int done = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * simple run-once check
   * - - - - - - - - - - - - - - - - - - - */

  if( ++done != 1 )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * we already tried to terminate
     * -> go down quick and dirty
     * - - - - - - - - - - - - - - - - - - - */

    _exit(1);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * try to stop main loop
   * - - - - - - - - - - - - - - - - - - - */

  // TODO: is it safe to call g_main_loop_quit() from signal handler?

  if( !mainloop_stop(0) )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * we caught signal prior to
     * entering glib main loop
     * -> go down immediately
     * - - - - - - - - - - - - - - - - - - - */

    exit(1);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * allow main loop to terminate
   * - - - - - - - - - - - - - - - - - - - */
}

/* ------------------------------------------------------------------------- *
 * sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

/**
   Internal signal helper function which is called when process is signalled.
   This is needed just for breaking away from while loop in main() function.
 */
static
void
sighnd_handler(int sig)
{
  log_error("Got signal [%d] %s\n", sig, strsignal(sig));

  signal(sig, sighnd_handler);

  switch( sig )
  {
  case SIGINT:
  case SIGHUP:
  case SIGTERM:
  case SIGQUIT:
    sighnd_terminate();
    break;

  case SIGUSR1:
    log_reopen(LOG_TO_STDERR);
    log_set_level(LOG_DEBUG);
    break;

  case SIGUSR2:
    log_reopen(LOG_TO_SYSLOG);
    log_set_level(LOG_WARNING);
    break;
  }
}

/* ------------------------------------------------------------------------- *
 * sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

int
sighnd_init(void)
{
  int error = -1;

  static const int sig[] =
  {
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGTERM,
    SIGUSR1,
    SIGUSR2,
    -1
  };

  for( int i = 0; sig[i] != -1; ++i )
  {
    if( signal(sig[i], sighnd_handler) == SIG_ERR )
    {
      goto cleanup;
    }
  }

  error = 0;

  cleanup:

  return error;
}
