/* ========================================================================= *
 * File: mainloop.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 27-May-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include "mainloop.h"

#include "logging.h"
#include "sighnd.h"
#include "server.h"
#include "queue.h"
#include "hwrtc.h"

#include <glib.h>
#include <stdlib.h>
#include <signal.h>

/* ========================================================================= *
 * Module Data
 * ========================================================================= */

static GMainLoop  *mainloop_loop    = 0;

static guint       mainloop_exit_id = 0; // delayed exit identifier

/* ========================================================================= *
 * Module Functions
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * mainloop_stop
 * ------------------------------------------------------------------------- */

int
mainloop_stop(int force)
{
  if( mainloop_loop != 0 )
  {
    g_main_loop_quit(mainloop_loop);
    return 1;
  }

  log_warning("%s: no main loop\n", __FUNCTION__);

  if( force != 0 )
  {
    exit(EXIT_FAILURE);
  }

  return 0;
}

/* ------------------------------------------------------------------------- *
 * mainloop_exit_cb
 * ------------------------------------------------------------------------- */

static gboolean mainloop_exit_cb(gpointer aptr)
{
  log_critical("The queue file was modified without restarting the device.\n");
  log_critical("Terminating the alarmd process.\n");

  mainloop_stop(1);
  mainloop_exit_id = 0;
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * mainloop_exit_cancel
 * ------------------------------------------------------------------------- */

static void mainloop_exit_cancel(void)
{
  if( mainloop_exit_id != 0 )
  {
    g_source_remove(mainloop_exit_id);
    mainloop_exit_id = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * mainloop_exit_request
 * ------------------------------------------------------------------------- */

static void mainloop_exit_request(void)
{
  if( mainloop_exit_id == 0 )
  {
    /* Usually the only reason we should see the queue file be
     * modified by something else than alarmd itself should be
     * osso-backup doing restore operation.
     *
     * Allow osso-backup some time to finish the restore operation
     * and restart the device.
     *
     * If that is not happening we will terminate the alarmd process
     * so that the queue status will be properly propagated to
     * other components as the alarmd is restarted by dsme.
     */

    mainloop_exit_id = g_timeout_add(30 * 1000, mainloop_exit_cb, 0);
  }
}

/* ------------------------------------------------------------------------- *
 * mainloop_run
 * ------------------------------------------------------------------------- */

int
mainloop_run(void)
{
  int exit_code = EXIT_FAILURE;

  /* - - - - - - - - - - - - - - - - - - - *
   * create mainloop
   * - - - - - - - - - - - - - - - - - - - */

  mainloop_loop = g_main_loop_new(NULL, FALSE);

  /* - - - - - - - - - - - - - - - - - - - *
   * make writing to closed sockets
   * return -1 instead of raising a
   * SIGPIPE signal
   * - - - - - - - - - - - - - - - - - - - */

  signal(SIGPIPE, SIG_IGN);

  /* - - - - - - - - - - - - - - - - - - - *
   * register queuefile overwrite callback
   * - - - - - - - - - - - - - - - - - - - */

  queue_set_restore_cb(mainloop_exit_request);

  /* - - - - - - - - - - - - - - - - - - - *
   * initialize subsystems
   * - - - - - - - - - - - - - - - - - - - */

  if( sighnd_init() == -1 )
  {
    goto cleanup;
  }

  if( hwrtc_init() == -1 )
  {
    goto cleanup;
  }

  if( queue_init() == -1 )
  {
    goto cleanup;
  }

  if( server_init() == -1 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * enter mainloop
   * - - - - - - - - - - - - - - - - - - - */

  log_info("ENTER MAINLOOP\n");
  g_main_loop_run(mainloop_loop);
  log_info("LEAVE MAINLOOP\n");

  exit_code = EXIT_SUCCESS;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & exit
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  mainloop_exit_cancel();

  server_quit();
  queue_quit();
  hwrtc_quit();

  if( mainloop_loop != 0 )
  {
    g_main_loop_unref(mainloop_loop);
    mainloop_loop = 0;
  }

  if( exit_code != EXIT_SUCCESS )
  {
    log_info("%s: exit_code=%d\n", __FUNCTION__, exit_code);
  }

  return exit_code;
}
