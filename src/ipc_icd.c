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

#include "ipc_icd.h"
#include "logging.h"
#include "symtab.h"

#include <conic.h>

#include <string.h>
#include <stdlib.h>

/* ========================================================================= *
 * Internet Connectivity Daemon Listener
 * ========================================================================= */

// libconic connection object
static gpointer ipc_icd_conn = NULL;

// callback pointer for forwarding internet connection status changes
static void (*ipc_icd_status_cb)(int enabled) = 0;

// for keeping track of all iap ids that are in connected state
static symtab_t *ipc_icd_connstat_stab = 0;

/* ------------------------------------------------------------------------- *
 * iapid_del, iapid_cmp  --  symtab callbacks
 * ------------------------------------------------------------------------- */

static void iapid_del(void *item)
{
  free(item);
}
static int iapid_cmp(const void *item, const void *key)
{
  return !strcmp(item, key);
}

/* ------------------------------------------------------------------------- *
 * ipc_icd_connstat_rem_iap  --  mark iap disconnected
 * ------------------------------------------------------------------------- */

static void ipc_icd_connstat_rem_iap(const char *iap_id)
{
  if( ipc_icd_connstat_stab != 0 )
  {
    symtab_remove(ipc_icd_connstat_stab, iap_id);
  }
}

/* ------------------------------------------------------------------------- *
 * ipc_icd_connstat_add_iap  --  mark iap connected
 * ------------------------------------------------------------------------- */

static void
ipc_icd_connstat_add_iap(const char *iap_id)
{
  if( ipc_icd_connstat_stab == 0 )
  {
    ipc_icd_connstat_stab = symtab_create(iapid_del, iapid_cmp);
  }

  if( !symtab_lookup(ipc_icd_connstat_stab, iap_id) )
  {
    symtab_append(ipc_icd_connstat_stab, strdup(iap_id));
  }
}

/* ------------------------------------------------------------------------- *
 * ipc_icd_connstat_is_connected  --  active connection on any iap
 * ------------------------------------------------------------------------- */

static int
ipc_icd_connstat_is_connected(void)
{
  return ipc_icd_connstat_stab && ipc_icd_connstat_stab->st_count;
}

/* ------------------------------------------------------------------------- *
 * ipc_icd_event_callback
 * ------------------------------------------------------------------------- */

static
void
ipc_icd_event_callback(ConIcConnection *connection,
                       ConIcConnectionEvent *event,
                       void *user_data)
{
  int connected = 0;

  if( con_ic_connection_event_get_status(event) == CON_IC_STATUS_CONNECTED )
  {
    connected = 1;
  }

  const gchar *iap_id = con_ic_event_get_iap_id(CON_IC_EVENT(event));
  const gchar *bearer = con_ic_event_get_bearer_type(CON_IC_EVENT(event));

  if( iap_id != 0 && bearer != 0 )
  {
    log_debug("conic event: %s - %s: %s\n",
              iap_id, bearer, connected ? "connected" : "disconnected");

    if( connected ) ipc_icd_connstat_add_iap(iap_id);
    else            ipc_icd_connstat_rem_iap(iap_id);

    if( ipc_icd_status_cb != 0 )
    {
      ipc_icd_status_cb(ipc_icd_connstat_is_connected());
    }
  }
}

/* ------------------------------------------------------------------------- *
 * ipc_icd_set_automatic_connection_events
 * ------------------------------------------------------------------------- */

static
void
ipc_icd_set_automatic_connection_events(int enabled)
{
  GValue value;
  memset(&value, 0, sizeof value);
  g_value_init(&value, G_TYPE_BOOLEAN);
  g_value_set_boolean(&value, (enabled != 0));

  g_object_set_property(G_OBJECT(ipc_icd_conn),
                        "automatic-connection-events",
                        &value);

  g_value_unset(&value);
}

/* ------------------------------------------------------------------------- *
 * ipc_icd_quit
 * ------------------------------------------------------------------------- */

void
ipc_icd_quit(void)
{
  if( ipc_icd_conn != 0 )
  {
    ipc_icd_set_automatic_connection_events(FALSE);
    g_object_unref(ipc_icd_conn);
    ipc_icd_conn = 0;
  }

  symtab_delete(ipc_icd_connstat_stab);
  ipc_icd_connstat_stab = 0;
}

/* ------------------------------------------------------------------------- *
 * ipc_icd_init
 * ------------------------------------------------------------------------- */

int
ipc_icd_init(void (*status_cb)(int))
{
  int error = 0;

  ipc_icd_status_cb = status_cb;

  if( ipc_icd_conn == 0 )
  {
    ipc_icd_conn = con_ic_connection_new();

    g_signal_connect(ipc_icd_conn,
                     "connection-event",
                     G_CALLBACK(ipc_icd_event_callback),
                     NULL);

    ipc_icd_set_automatic_connection_events(TRUE);
  }
  return error;
}
