#include "ipc_icd.h"
#include "logging.h"

#include <conic.h>
#include <string.h>

/* ========================================================================= *
 * Internet Connectivity Daemon Listener
 * ========================================================================= */

static gpointer ipc_icd_conn = NULL;

static void (*ipc_icd_status)(int enabled) = 0;

static
void
ipc_icd_event_callback(ConIcConnection *connection,
                            ConIcConnectionEvent *event,
                            void *user_data)
{
  int connected = 0;

  switch( con_ic_connection_event_get_status(event) )
  {
  case CON_IC_STATUS_CONNECTED:
    log_debug("CON_IC_STATUS == CONNECTED\n");
    connected = 1;
    break;

  default:
    log_debug("CON_IC_STATUS != CONNECTED\n");
    break;
  }

  if( ipc_icd_status != 0 )
  {
    ipc_icd_status(connected);
  }
}

static
void
ipc_icd_tracking(int yes)
{
  GValue value;
  memset(&value, 0, sizeof value);
  g_value_init(&value, G_TYPE_BOOLEAN);
  g_value_set_boolean(&value, (yes != 0));

  g_object_set_property(G_OBJECT(ipc_icd_conn),
                        "automatic-connection-events",
                        &value);

  g_value_unset(&value);
}

void
ipc_icd_quit(void)
{
  if( ipc_icd_conn != 0 )
  {
    ipc_icd_tracking(FALSE);
    g_object_unref(ipc_icd_conn);
    ipc_icd_conn = 0;
  }
}

int
ipc_icd_init(void (*status_cb)(int))
{
  int error = 0;

  ipc_icd_status = status_cb;

  if( ipc_icd_conn == 0 )
  {
    ipc_icd_conn = con_ic_connection_new();

    g_signal_connect(ipc_icd_conn,
                     "connection-event",
                     G_CALLBACK(ipc_icd_event_callback),
                     NULL);

    ipc_icd_tracking(TRUE);
  }
  return error;
}
