#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <gtk/gtk.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <assert.h>

#include "xutil.h"

#include "libalarm.h"
#include "logging.h"
#include "dbusif.h"
#include "systemui_dbus.h"
#include "alarm_dbus.h"

#define MATCH_ALARMD_OWNERCHANGED\
  "type='signal'"\
  ",sender='"DBUS_SERVICE_DBUS"'"\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='NameOwnerChanged'"\
  ",arg0='"ALARMD_SERVICE"'"

#define MATCH_ALARMD_STATUS_IND\
  "type='signal'"\
  ",sender='"ALARMD_SERVICE"'"\
  ",interface='"ALARMD_INTERFACE"'"\
  ",path='"ALARMD_PATH"'"\
  ",member='"ALARMD_QUEUE_STATUS_IND"'"

// TODO: remove when available in dbus
#ifndef DBUS_ERROR_INIT
# define DBUS_ERROR_INIT { NULL, NULL, TRUE, 0, 0, 0, 0, NULL }
#endif

static int  fuigtk_mainloop_stop(int force);
static void fuigtk_dialog_request_rethink(void);
static void fuigtk_dialog_hide(void);

/* ========================================================================= *
 * Miscellaneous utils
 * ========================================================================= */

static inline const char *
fuigtk_ifempty(const char *s, const char *d)
{
  return (s && *s) ? s : d;
}

/* ========================================================================= *
 * FAKE SYSTEM UI QUEUE MANAGEMENT
 * ========================================================================= */

enum { SLOTS = 128 };

enum dialogstates
{
  DS_QUEUED   = 0,
  DS_ACTIVE   = 1,
  DS_FINISHED = 2,
  DS_DELETED  = 3,
} dialogstates;

typedef struct dialog_t
{
  cookie_t cookie;
  int      button;
  unsigned state;

} dialog_t;

static dialog_t fuigtk_queue_slot[SLOTS];
static size_t   fuigtk_queue_used = 0;
static unsigned fuigtk_queue_dirty_flag = 0;

/* ------------------------------------------------------------------------- *
 * fuigtk_queue_set/clr/is_dirty
 * ------------------------------------------------------------------------- */

static void fuigtk_queue_set_dirty(void)
{
  fuigtk_queue_dirty_flag = 1;
}

static void fuigtk_queue_clr_dirty(void)
{
  fuigtk_queue_dirty_flag = 0;
}

static int fuigtk_queue_is_dirty(void)
{
  return fuigtk_queue_dirty_flag;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_queue_add_event
 * ------------------------------------------------------------------------- */

static
int
fuigtk_queue_add_event(cookie_t cookie)
{
  int result = -1;

  if( cookie <= 0 ) goto cleanup;

  for( size_t i = 0; i < SLOTS; ++i )
  {
    if( i == fuigtk_queue_used )
    {
      dialog_t *dialog = &fuigtk_queue_slot[fuigtk_queue_used++];

      dialog->cookie = cookie;
      dialog->button = -1;
      dialog->state  = DS_QUEUED;

      result = 1;
      fuigtk_queue_set_dirty();
      log_debug("\tADD [%03Zd] %ld\n", i, cookie);
      break;
    }

    if( fuigtk_queue_slot[i].cookie == cookie )
    {
      result = 0;
      break;
    }
  }
  cleanup:

  return result;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_queue_cancel_event
 * ------------------------------------------------------------------------- */

static
int
fuigtk_queue_cancel_event(cookie_t cookie)
{
  int result = 0;

  for( size_t i = 0; i < fuigtk_queue_used; ++i )
  {
    dialog_t *dialog = &fuigtk_queue_slot[i];
    if( dialog->cookie == cookie )
    {
      if( dialog->state == DS_ACTIVE )
      {
        fuigtk_dialog_hide();
      }

      dialog->state = DS_DELETED;
      result = 1;
      fuigtk_queue_set_dirty();
      log_debug("\tCAN [%03Zd] %ld\n", i, cookie);
    }
  }

  return result;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_queue_get_events
 * ------------------------------------------------------------------------- */

static
cookie_t *
fuigtk_queue_get_events(int *pcnt)
{
  int       cnt = 0;
  cookie_t *vec = calloc(fuigtk_queue_used+1, sizeof *vec);

  for( int i = 0; i < fuigtk_queue_used; ++i )
  {
    dialog_t *dialog = &fuigtk_queue_slot[i];
    if( dialog->state != DS_DELETED )
    {
      vec[cnt++] = dialog->cookie;
    }
  }

  if( pcnt ) *pcnt = cnt;
  return vec;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_get_buttons
 * ------------------------------------------------------------------------- */

// QUARANTINE static int
// QUARANTINE fuigtk_get_buttons(alarm_event_t *event, int **plut)
// QUARANTINE {
// QUARANTINE   int cnt = 0;
// QUARANTINE
// QUARANTINE   for( int i = 0; i < event->action_cnt; ++i )
// QUARANTINE   {
// QUARANTINE     alarm_action_t *act = &event->action_tab[i];
// QUARANTINE
// QUARANTINE     if( !xisempty(act->label) && (act->flags && ALARM_ACTION_WHEN_RESPONDED) )
// QUARANTINE     {
// QUARANTINE       cnt++;
// QUARANTINE     }
// QUARANTINE   }
// QUARANTINE   *plut = calloc(cnt, sizeof **plut);
// QUARANTINE
// QUARANTINE   cnt = 0;
// QUARANTINE
// QUARANTINE   for( int i = 0; i < event->action_cnt; ++i )
// QUARANTINE   {
// QUARANTINE     alarm_action_t *act = &event->action_tab[i];
// QUARANTINE
// QUARANTINE     if( !xisempty(act->label) && (act->flags && ALARM_ACTION_WHEN_RESPONDED) )
// QUARANTINE     {
// QUARANTINE       (*plut)[cnt++] = i;
// QUARANTINE     }
// QUARANTINE   }
// QUARANTINE
// QUARANTINE   return cnt;
// QUARANTINE }

/* ========================================================================= *
 * GTK DIALOG
 * ========================================================================= */

#define PIXMAPS_DIR "."

static GtkWidget *fuigtk_dialog_window = 0;
static int        fuigtk_dialog_result = -1;
static cookie_t   fuigtk_dialog_cookie = -1;
static GtkWidget *fuigtk_dialog_pending = 0;

static void
fuigtk_dialog_destroy_callback(GtkWidget * widget, gpointer data)
{
  printf("@ %s() ...\n", __FUNCTION__);

  fuigtk_dialog_window = 0;
  fuigtk_dialog_pending = 0;
  printf("DESTROY\n");

  for( int i = 0; i < fuigtk_queue_used; ++i )
  {
    dialog_t *dialog = &fuigtk_queue_slot[i];

    if( dialog->cookie == fuigtk_dialog_cookie )
    {
      if( dialog->state == DS_ACTIVE )
      {
        dialog->state = DS_FINISHED;
      }
      if( fuigtk_dialog_result != -1 )
      {
        dialog->button = fuigtk_dialog_result;
        log_debug("EXIT %d\n", dialog->button);
      }
      else
      {
        log_error("ABNORMAL EXIT\n");
      }

      fuigtk_queue_set_dirty();
      fuigtk_dialog_request_rethink();
    }
  }
}

static void fuigtk_dialog_button_callback(GtkWidget *widget, gpointer data)
{
  printf("@ %s() ...\n", __FUNCTION__);

  g_print("BUTTON: %d\n", (int)data);
  fuigtk_dialog_result = (int)data;
  fuigtk_dialog_hide();
}

static void fuigtk_update_pending_list(void)
{
  auto void flush(GtkWidget *widget, gpointer data);

  auto void flush(GtkWidget *widget, gpointer data)
  {
    log_debug("FLUSH %p\n", widget);
    gtk_widget_destroy(widget);
    //gtk_container_remove(GTK_CONTAINER(fuigtk_dialog_pending), widget);
  }

  if( fuigtk_dialog_window && fuigtk_dialog_pending )
  {
    log_debug("fuigtk_update_pending_list ...\n");
    gtk_widget_hide_all(fuigtk_dialog_pending);

    gtk_container_foreach(GTK_CONTAINER(fuigtk_dialog_pending), flush, 0);

    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute*at = 0;

    at = pango_attr_size_new(7 * PANGO_SCALE);
    pango_attr_list_insert(attrs, at);

    int added = 0;

    for( int i = 0; i < fuigtk_queue_used; ++i )
    {
      dialog_t *d = &fuigtk_queue_slot[i];
      alarm_event_t *e = 0;
      GtkWidget *button;

      if( d->cookie != fuigtk_dialog_cookie )
      {
        if( (e = alarmd_event_get(d->cookie)) != 0 )
        {
          char txt[128];
          snprintf(txt, sizeof txt, "%s: %s",
                   e->title,
                   e->message);
          button = gtk_label_new(txt);
          log_debug("APPEND %p\n", button);
          gtk_label_set_attributes(GTK_LABEL(button), attrs);
          gtk_container_add(GTK_CONTAINER(fuigtk_dialog_pending), button);
          added += 1;
        }
      }
    }
    pango_attr_list_unref(attrs);
    if( added != 0 )
    {
      gtk_widget_show_all(fuigtk_dialog_pending);
    }
  }
}

static void fuigtk_dialog_create(dialog_t *dialog)
{
  alarm_event_t *event = 0;

  printf("@ %s() ...\n", __FUNCTION__);

  fuigtk_dialog_cookie = dialog->cookie;

  if( !(event = alarmd_event_get(fuigtk_dialog_cookie)) )
  {
    dialog->state = DS_DELETED;
    printf("NON EXISTING COOKIE: %d\n", (int)fuigtk_dialog_cookie);
    goto cleanup;
  }

  const char *TITLE   = fuigtk_ifempty(event->title, "ALARM");
  const char *MESSAGE = fuigtk_ifempty(event->message, "MESSAGE");

  GtkWidget *button;
  GtkWidget *frame;
  GtkWidget *horz;
  GtkWidget *vert;

  fuigtk_dialog_pending = 0;
  fuigtk_dialog_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width(GTK_CONTAINER(fuigtk_dialog_window), 2);

  gtk_window_set_title(GTK_WINDOW(fuigtk_dialog_window), "Alarm Event");
  gtk_window_set_default_size(GTK_WINDOW(fuigtk_dialog_window), 200, 50);
  gtk_window_set_default_icon_from_file(PIXMAPS_DIR "/icon.png", 0);

  g_signal_connect(G_OBJECT(fuigtk_dialog_window), "destroy",
                   G_CALLBACK(fuigtk_dialog_destroy_callback), NULL);

  vert = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(fuigtk_dialog_window), vert);

  frame = gtk_frame_new(TITLE);
  gtk_container_add(GTK_CONTAINER(vert), frame);

  button = gtk_label_new(MESSAGE);
  gtk_container_add(GTK_CONTAINER(frame), button);

  horz = gtk_hbox_new(FALSE, 3);
  gtk_container_add(GTK_CONTAINER(vert), horz);

  for( int i = 0; i < event->action_cnt; ++i )
  {
    alarm_action_t *act = &event->action_tab[i];

    if( !xisempty(act->label) && (act->flags && ALARM_ACTION_WHEN_RESPONDED) )
    {
      button = gtk_button_new_with_label(act->label);
      gtk_container_add(GTK_CONTAINER(horz), button);
      g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(fuigtk_dialog_button_callback),
                       (gpointer)i);
    }
  }

  fuigtk_dialog_pending = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(vert), fuigtk_dialog_pending);

  gtk_widget_show_all(fuigtk_dialog_window);

  //fuigtk_update_pending_list();

  dialog->state = DS_ACTIVE;

  cleanup:

  alarm_event_delete(event);
}

static int fuigtk_dialog_show(dialog_t *dialog)
{
  printf("@ %s() ...\n", __FUNCTION__);

  if( fuigtk_dialog_window == 0 )
  {
    fuigtk_dialog_result = -1;
    fuigtk_dialog_create(dialog);

    if( fuigtk_dialog_window != 0 )
    {
      return 0;
    }
  }
  return -1;
}

static void fuigtk_dialog_hide(void)
{
  printf("@ %s() ...\n", __FUNCTION__);

  if( fuigtk_dialog_window != 0 )
  {
    gtk_widget_destroy(fuigtk_dialog_window);
    fuigtk_dialog_window = 0;
    fuigtk_dialog_pending = 0;
  }
}

/* ========================================================================= *
 * RETHINK CALLBACK
 * ========================================================================= */

static guint fuigtk_rethink_id = 0;

/* ------------------------------------------------------------------------- *
 * fuigtk_dialog_rethink_finished  --  handle finished dialogs
 * ------------------------------------------------------------------------- */

static
void
fuigtk_dialog_rethink_finished(void)
{
  for( int i = 0; i < fuigtk_queue_used; ++i )
  {
    dialog_t *dialog = &fuigtk_queue_slot[i];

    if( dialog->state == DS_FINISHED )
    {
      log_debug("\tFIN [%03Zd] %ld %d\n", i, dialog->cookie, dialog->button);
      alarmd_ack_dialog(dialog->cookie, dialog->button);
      dialog->state = DS_DELETED;
      fuigtk_queue_set_dirty();
    }
  }
}

/* ------------------------------------------------------------------------- *
 * fuigtk_dialog_rethink_queued  --  handle dialogs waiting in queue
 * ------------------------------------------------------------------------- */

static
void
fuigtk_dialog_rethink_queued(void)
{
  // check if we already have an active dialog up

  for( int i = 0; i < fuigtk_queue_used; ++i )
  {
    dialog_t *dialog = &fuigtk_queue_slot[i];
    if( dialog->state == DS_ACTIVE )
    {
      goto cleanup;
    }
  }

  // otherwise start up oldest dialog we have in queue

  for( int i = 0; i < fuigtk_queue_used; ++i )
  {
    dialog_t *dialog = &fuigtk_queue_slot[i];
    if( dialog->state == DS_QUEUED )
    {
      //if( fuigtk_dialog_fork(d) != -1 )
      if( fuigtk_dialog_show(dialog) != -1 )
      {
        log_debug("\tACT [%03Zd] %ld\n", i, dialog->cookie);
        fuigtk_queue_set_dirty();
        break;
      }
    }
  }

  cleanup:

  return;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_dialog_rethink_deleted  --  remove deleted items from queue
 * ------------------------------------------------------------------------- */

static
void
fuigtk_dialog_rethink_deleted(void)
{
  int cnt = 0;
  for( int i = 0; i < fuigtk_queue_used; ++i )
  {
    dialog_t *dialog = &fuigtk_queue_slot[i];

    switch( dialog->state )
    {
    case DS_DELETED:
      log_debug("\tDEL [%03Zd] %ld\n", i, dialog->cookie);
      fuigtk_queue_set_dirty();
      break;

    default:
      fuigtk_queue_slot[cnt++] = *dialog;
      break;
    }
  }
  fuigtk_queue_used = cnt;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_dialog_rethink_callback  --  handle changes in queue state
 * ------------------------------------------------------------------------- */

static
gboolean
fuigtk_dialog_rethink_callback(gpointer data)
{
// QUARANTINE   log_debug("\n");
// QUARANTINE   log_debug("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
// QUARANTINE
// QUARANTINE   log_debug("@ %s\n", __FUNCTION__);

  fuigtk_queue_clr_dirty();

  fuigtk_dialog_rethink_finished();
  fuigtk_dialog_rethink_queued();
  fuigtk_dialog_rethink_deleted();

// QUARANTINE   log_debug(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
// QUARANTINE   log_debug("\n");

  if( fuigtk_queue_is_dirty() )
  {
    // repeat until queue stabilizes
    return TRUE;
  }

  fuigtk_update_pending_list();

  // we are done for now
  fuigtk_rethink_id = 0;
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_dialog_request_rethink
 * ------------------------------------------------------------------------- */

static
void
fuigtk_dialog_request_rethink(void)
{
  if( !fuigtk_rethink_id  && fuigtk_queue_is_dirty() )
  {
    fuigtk_rethink_id = g_idle_add(fuigtk_dialog_rethink_callback, 0);
  }
}

/* ========================================================================= *
 * FAKE SYSTEM UI SERVER
 * ========================================================================= */

static DBusConnection *fuigtk_server_conn    = NULL;

/* ------------------------------------------------------------------------- *
 * fuigtk_server_handle_add  --  handle SYSTEMUI_ALARM_ADD method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
fuigtk_server_handle_add(DBusMessage *msg)
{
  DBusMessage   *rsp = 0;
  dbus_int32_t  *vec = 0;
  int            cnt = 0;
  cookie_t      *cur = 0;

  assert( sizeof(dbus_int32_t) == sizeof(cookie_t) );

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_ARRAY,
                                       DBUS_TYPE_INT32, &vec, &cnt,
                                       DBUS_TYPE_INVALID)) )
  {

    for( int i = 0; i < cnt; ++i )
    {
      fuigtk_queue_add_event(vec[i]);
    }

    cur = fuigtk_queue_get_events(&cnt);
    rsp = dbusif_reply_create(msg,
                              DBUS_TYPE_ARRAY,
                              DBUS_TYPE_INT32, &cur, cnt,
                              DBUS_TYPE_INVALID);
  }

  free(cur);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_server_handle_del  -- handle SYSTEMUI_ALARM_DEL method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
fuigtk_server_handle_del(DBusMessage *msg)
{
  DBusMessage   *rsp = 0;
  dbus_int32_t  *vec = 0;
  int            cnt = 0;
  cookie_t      *cur = 0;

  assert( sizeof(dbus_int32_t) == sizeof(cookie_t) );

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_ARRAY,
                                       DBUS_TYPE_INT32, &vec, &cnt,
                                       DBUS_TYPE_INVALID)) )
  {
    for( int i = 0; i < cnt; ++i )
    {
      fuigtk_queue_cancel_event(vec[i]);
    }

    cur = fuigtk_queue_get_events(&cnt);
    rsp = dbusif_reply_create(msg,
                              DBUS_TYPE_ARRAY,
                              DBUS_TYPE_INT32, &cur, cnt,
                              DBUS_TYPE_INVALID);
  }

  free(cur);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_server_handle_query  --  handle SYSTEMUI_ALARM_QUERY method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
fuigtk_server_handle_query(DBusMessage *msg)
{
  DBusMessage *rsp = 0;
  int          cnt = 0;
  cookie_t    *cur = 0;

  assert( sizeof(dbus_int32_t) == sizeof(cookie_t) );

  cur = fuigtk_queue_get_events(&cnt);
  rsp = dbusif_reply_create(msg,
                            DBUS_TYPE_ARRAY,
                            DBUS_TYPE_INT32, &cur, cnt,
                            DBUS_TYPE_INVALID);
  free(cur);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_server_filter  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
fuigtk_server_filter(DBusConnection *conn,
                     DBusMessage *msg,
                     void *user_data)
{
  DBusHandlerResult   result    = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char         *interface = dbus_message_get_interface(msg);
  const char         *member    = dbus_message_get_member(msg);
  const char         *object    = dbus_message_get_path(msg);
  int                 type      = dbus_message_get_type(msg);
  DBusMessage        *rsp       = 0;

#if ENABLE_LOGGING
  const char         *typestr   = dbusif_get_msgtype_name(type);
  log_debug("\n");
  log_debug("----------------------------------------------------------------\n");
  log_debug("@ %s: %s(%s, %s, %s)\n", __FUNCTION__, typestr, object, interface, member);
#endif

  if( !interface || !member || !object )
  {
    goto cleanup;
  }

  if( type == DBUS_MESSAGE_TYPE_SIGNAL &&
      !strcmp(interface, DBUS_INTERFACE_DBUS) &&
      !strcmp(object,DBUS_PATH_DBUS) &&
      !strcmp(member, "NameOwnerChanged") )
  {
    DBusError   err  = DBUS_ERROR_INIT;
    const char *prev = 0;
    const char *curr = 0;
    const char *serv = 0;

    dbus_message_get_args(msg, &err,
                          DBUS_TYPE_STRING, &serv,
                          DBUS_TYPE_STRING, &prev,
                          DBUS_TYPE_STRING, &curr,
                          DBUS_TYPE_INVALID);

    if( serv != 0 && !strcmp(serv, ALARMD_SERVICE) )
    {
      if( !curr || !*curr )
      {
        log_debug("ALARMD WENT DOWN\n");
        fuigtk_dialog_hide();
        fuigtk_queue_used = 0;
        fuigtk_queue_set_dirty();
      }
      else
      {
        log_debug("ALARMD CAME UP\n");
      }
    }
    dbus_error_free(&err);
  }

  if( type == DBUS_MESSAGE_TYPE_SIGNAL &&
      !strcmp(interface, ALARMD_INTERFACE) &&
      !strcmp(object,ALARMD_PATH) &&
      !strcmp(member, ALARMD_QUEUE_STATUS_IND) )
  {
    DBusError    err = DBUS_ERROR_INIT;
    dbus_int32_t ac = 0;
    dbus_int32_t dt = 0;
    dbus_int32_t ad = 0;
    dbus_int32_t nb = 0;

    dbus_message_get_args(msg, &err,
                          DBUS_TYPE_INT32, &ac,
                          DBUS_TYPE_INT32, &dt,
                          DBUS_TYPE_INT32, &ad,
                          DBUS_TYPE_INT32, &nb,
                          DBUS_TYPE_INVALID);

    log_debug("ALARM QUEUE: active=%d, desktop=%d, actdead=%d, no-boot=%d\n",
              ac,dt,ad,nb);
    dbus_error_free(&err);
  }

  if( !strcmp(interface, SYSTEMUI_REQUEST_IF) && !strcmp(object, SYSTEMUI_REQUEST_PATH) )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * as far as dbus message filtering is
     * considered handling of SYSTEMUI_REQUEST_IF
     * should happen right here
     * - - - - - - - - - - - - - - - - - - - */

    result = DBUS_HANDLER_RESULT_HANDLED;

    /* - - - - - - - - - - - - - - - - - - - *
     * handle method_call messages
     * - - - - - - - - - - - - - - - - - - - */

    if( type == DBUS_MESSAGE_TYPE_METHOD_CALL )
    {
      static const struct
      {
        const char *member;
        DBusMessage *(*func)(DBusMessage *);
      } lut[] =
      {
        {SYSTEMUI_ALARM_ADD,   fuigtk_server_handle_add},
        {SYSTEMUI_ALARM_DEL,   fuigtk_server_handle_del},
        {SYSTEMUI_ALARM_QUERY, fuigtk_server_handle_query},

        {0,0}
      };

      for( int i = 0; ; ++i )
      {
        if( lut[i].member == 0 )
        {
          log_error("UNKNOWN %s\n", member);
          rsp = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_METHOD, member);
          break;
        }

        if( !strcmp(lut[i].member, member) )
        {
          rsp = lut[i].func(msg);
          break;
        }
      }
    }

    /* - - - - - - - - - - - - - - - - - - - *
     * if no responce message was created
     * above and the peer expects reply,
     * create a generic error message
     * - - - - - - - - - - - - - - - - - - - */

    if( rsp == 0 && !dbus_message_get_no_reply(msg) )
    {
      rsp = dbus_message_new_error(msg, DBUS_ERROR_FAILED, member);
    }

    /* - - - - - - - - - - - - - - - - - - - *
     * send responce if we have something
     * to send
     * - - - - - - - - - - - - - - - - - - - */

    if( rsp != 0 )
    {
      dbus_connection_send(conn, rsp, 0);
    }
  }

  cleanup:

  if( rsp != 0 )
  {
    dbus_message_unref(rsp);
  }

  fuigtk_dialog_request_rethink();

#if ENABLE_LOGGING
  log_debug("----------------------------------------------------------------\n");
  log_debug("\n");
#endif

  return result;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_server_init
 * ------------------------------------------------------------------------- */

static
int
fuigtk_server_init(int use_fake_name)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;

  //dbus_error_init(&err);

  /* - - - - - - - - - - - - - - - - - - - *
   * connect to session bus
   * - - - - - - - - - - - - - - - - - - - */

  if( (fuigtk_server_conn = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    //CLEANUP fatalf("%s: %s\n", "dbus_bus_get", err.message);
    goto cleanup;
  }

  const char *service = SYSTEMUI_SERVICE;

  if( use_fake_name )
  {
    service = FAKESYSTEMUI_SERVICE;
  }

  int ret = dbus_bus_request_name(fuigtk_server_conn, service,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                  //DBUS_NAME_FLAG_REPLACE_EXISTING,
                                  &err);

  if( ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
  {
    if( dbus_error_is_set(&err) )
    {
      log_error("Name Error (%s)\n", err.message);
    }
    else
    {
      log_error("not primary owner of connection\n");
    }
    goto cleanup;
  }

  dbus_connection_setup_with_g_main(fuigtk_server_conn, NULL);
  dbus_connection_set_exit_on_disconnect(fuigtk_server_conn, 0);

  /* - - - - - - - - - - - - - - - - - - - *
   * register message filter
   * - - - - - - - - - - - - - - - - - - - */

  if( !dbus_connection_add_filter(fuigtk_server_conn, fuigtk_server_filter, 0, 0) )
  {
    //CLEANUP fatalf("%s: %s\n", "dbus_connection_add_filter", "FAILED");
    goto cleanup;
  }

  dbus_bus_add_match(fuigtk_server_conn, MATCH_ALARMD_OWNERCHANGED, &err);
  dbus_bus_add_match(fuigtk_server_conn, MATCH_ALARMD_STATUS_IND, &err);

  fuigtk_dialog_request_rethink();

  /* - - - - - - - - - - - - - - - - - - - *
   * success
   * - - - - - - - - - - - - - - - - - - - */

  res = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & return
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_server_quit
 * ------------------------------------------------------------------------- */

static
void
fuigtk_server_quit(void)
{

  if( fuigtk_server_conn != 0 )
  {
    dbus_connection_unref(fuigtk_server_conn);
    fuigtk_server_conn = 0;
  }
}
/* ========================================================================= *
 * FAKE SUSTEM UI SIGNAL HANDLER
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * fuigtk_sighnd_terminate  --  terminate process from signal handler
 * ------------------------------------------------------------------------- */

static
void
fuigtk_sighnd_terminate(void)
{
  static int done = 0;

  switch( ++done )
  {
  case 1:
    if( fuigtk_mainloop_stop(0) )
    {
      break;
    }

  case 2:
    exit(1);

  default:
    _exit(1);
  }
}

/* ------------------------------------------------------------------------- *
 * fuigtk_sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

static
void
fuigtk_sighnd_handler(int sig)
{
  log_error("Got signal [%d] %s\n", sig, strsignal(sig));

  switch( sig )
  {
  case SIGINT:
  case SIGHUP:
  case SIGTERM:
  case SIGQUIT:
    signal(sig, fuigtk_sighnd_handler);
    fuigtk_sighnd_terminate();
    break;
  }
}

/* ------------------------------------------------------------------------- *
 * fuigtk_sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

static
void
fuigtk_sighnd_init(void)
{
  static const int sig[] =
  {
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGTERM,
    -1
  };

  for( int i = 0; sig[i] != -1; ++i )
  {
    signal(sig[i], fuigtk_sighnd_handler);
  }
}

/* ========================================================================= *
 * FAKE SYSTEM UI MAINLOOP
 * ========================================================================= */

// QUARANTINE static GMainLoop  *fuigtk_mainloop_handle     = 0;

/* ------------------------------------------------------------------------- *
 * fuigtk_mainloop_stop
 * ------------------------------------------------------------------------- */

static
int
fuigtk_mainloop_stop(int force)
{
  if( force != 0 )
  {
    exit(EXIT_FAILURE);
  }
  gtk_main_quit();
  return 0;
}

/* ------------------------------------------------------------------------- *
 * fuigtk_mainloop_run
 * ------------------------------------------------------------------------- */

static
int
fuigtk_mainloop_run(int argc, char* argv[])
{
  int fake_name = (argc < 2);
  int exit_code = EXIT_FAILURE;

  /* - - - - - - - - - - - - - - - - - - - *
   * make writing to closed sockets
   * return -1 instead of raising a
   * SIGPIPE signal
   * - - - - - - - - - - - - - - - - - - - */

  signal(SIGPIPE, SIG_IGN);
  signal(SIGSEGV, SIG_DFL);

  /* - - - - - - - - - - - - - - - - - - - *
   * install signal handlers
   * - - - - - - - - - - - - - - - - - - - */

  fuigtk_sighnd_init();

  /* - - - - - - - - - - - - - - - - - - - *
   * start dbus service
   * - - - - - - - - - - - - - - - - - - - */

  if( fuigtk_server_init(fake_name) == -1 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * enter mainloop
   * - - - - - - - - - - - - - - - - - - - */

  log_info("ENTER MAINLOOP\n");
  gtk_main();
  log_info("LEAVE MAINLOOP\n");

  exit_code = EXIT_SUCCESS;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & exit
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  fuigtk_server_quit();

  return exit_code;
}

/* ========================================================================= *
 * MAIN ENTRY POINT
 * ========================================================================= */

int
main(int ac, char **av)
{
  log_set_level(LOG_DEBUG);
  log_open("fakeui", LOG_TO_STDERR, 0);

  gtk_init(&ac, &av);

  return fuigtk_mainloop_run(ac, av);
}
