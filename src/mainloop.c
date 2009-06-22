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

void
mainloop_stop(void)
{
  if( mainloop_loop == 0 )
  {
    log_warning("no mainloop to stop - exiting now\n");
    exit(EXIT_FAILURE);
  }

  if( mainloop_loop != 0 )
  {
    log_warning("stopping mainloop\n");
    g_main_loop_quit(mainloop_loop);
  }
}

/* ------------------------------------------------------------------------- *
 * mainloop_exit_cb
 * ------------------------------------------------------------------------- */

static gboolean mainloop_exit_cb(gpointer aptr)
{
  log_critical("The queue file was modified without restarting the device.\n");
  log_critical("Terminating the alarmd process.\n");

  mainloop_stop();
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

  queue_set_modified_cb(mainloop_exit_request);

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

  log_debug("-- enter mainloop --\n");
  g_main_loop_run(mainloop_loop);
  log_debug("-- leave mainloop --\n");

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
    log_info("exit_code=%d\n", exit_code);
  }

  return exit_code;
}
