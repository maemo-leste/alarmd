/* ========================================================================= *
 * File: skeleton.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 02-Oct-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include "xutil.h"
#include "libalarm.h"
#include "dbusif.h"
#include "logging.h"
#include "systemui_dbus.h"
#include "alarm_dbus.h"

#include <gtk/gtk.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <assert.h>

static void skeleton_dialog_rethink(void);
static void skeleton_queue_send_replies(int leave_one);

static int poweup_bit = 0;

/* ------------------------------------------------------------------------- *
 * Quick'n'dirty powerup confirmation dialog
 * ------------------------------------------------------------------------- */

static int acting_dead = 0;
static guint powerup_id = 0;
static GtkWidget *powerup_dlg = 0;

static void powerup_hide(void)
{
  if( powerup_id != 0 )
  {
    printf("%s\n", __FUNCTION__);
    g_source_remove(powerup_id);
    powerup_id = 0;
  }

  if( powerup_dlg != 0 )
  {
    gtk_widget_destroy(powerup_dlg);
    powerup_dlg = 0;
  }
}

static gboolean powerup_timeout_cb(gpointer aptr)
{
  printf("%s\n", __FUNCTION__);

  powerup_id = 0;
  powerup_hide();

  poweup_bit = 0;

  if( aptr != 0 )
  {
    printf("POWERUP TIMEOUT\n");
  }
  else
  {
    printf("POWERUP CLOSED\n");
  }

  skeleton_queue_send_replies(0);
  skeleton_dialog_rethink();

  return FALSE;
}

static void powerup_response_cb(GtkDialog *dialog, gint button, gpointer aptr)
{
  if( button )
  {
    // send DSME_REQ_POWERUP -> dsme
    printf("POWERUP ISSUED\n");
    poweup_bit = 1<<31;
  }
  else
  {
    printf("POWERUP CANCELED\n");
    poweup_bit = 0;
  }
  powerup_hide();
  skeleton_queue_send_replies(0);
  skeleton_dialog_rethink();

}

static void powerup_show(void)
{
  if( powerup_id == 0 )
  {
    printf("%s\n", __FUNCTION__);
    powerup_id = g_timeout_add(15*1000, powerup_timeout_cb, (gpointer)1);
    powerup_dlg = gtk_dialog_new_with_buttons("Power up?",
                                              NULL,
                                              GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, 0,
                                              GTK_STOCK_OK,     1,
                                              NULL);
    g_signal_connect(G_OBJECT(powerup_dlg), "close",
                     G_CALLBACK(powerup_timeout_cb), NULL);
    g_signal_connect(G_OBJECT(powerup_dlg), "response",
                     G_CALLBACK(powerup_response_cb), NULL);
    gtk_widget_show_all(powerup_dlg);
  }
}

/* ========================================================================= *
 * Misc Utils
 * ========================================================================= */

static inline const char *if_empty(const char *str, const char *def)
{
  return (str && *str) ? str : def;
}

static inline int atwhite(const char *s)
{
  int c = *(const unsigned char *)s;
  return (c > 0) && (c <= 32);
}
static inline int atblack(const char *s)
{
  int c = *(const unsigned char *)s;
  return (c > 32);
}

static char *strip(char *str)
{
  char *s = str;
  char *d = str;

  while( atwhite(s) ) ++s;

  for( ;; )
  {
    while( atblack(s) ) *d++ = *s++;
    while( atwhite(s) ) ++s;
    if( *s == 0 ) break;
    *d++ = ' ';
  }
  *d = 0;
  return str;
}

/* ========================================================================= *
 * alarm_state_t
 * ========================================================================= */

typedef struct alarm_state_t alarm_state_t;

struct alarm_state_t
{
  cookie_t       cookie;
  alarm_event_t *event;

  // dialog state data

  int            pending;
  int            button;

   // sound cycle etc state data
  int            cycle;

};

/* ------------------------------------------------------------------------- *
 * alarm_state_create
 * ------------------------------------------------------------------------- */

static
alarm_state_t *
alarm_state_create(cookie_t cookie)
{
  alarm_state_t *self = calloc(1, sizeof *self);

  self->cookie  = cookie;
  self->event   = 0;
  self->pending = 1;
  self->button  = -1;
  self->cycle   = 0;

  return self;
}

/* ------------------------------------------------------------------------- *
 * alarm_state_delete
 * ------------------------------------------------------------------------- */

static
void
alarm_state_delete(alarm_state_t *self)
{
  if( self != 0 )
  {
    alarm_event_delete(self->event);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * alarm_state_get_event
 * ------------------------------------------------------------------------- */

static
alarm_event_t *
alarm_state_get_event(alarm_state_t *self)
{
  if( self->event == 0 )
  {
    self->event = alarmd_event_get(self->cookie);
  }
  log_debug("EVENT: %d -> %8p\n", self->cookie, self->event);

  return self->event;
}

/* ------------------------------------------------------------------------- *
 * alarm_state_send_reply
 * ------------------------------------------------------------------------- */

static
void
alarm_state_send_reply(alarm_state_t *self)
{
  assert( self->pending == 0 );
  log_debug("REPLY: %d -> %d\n", self->cookie, self->button);
  alarmd_ack_dialog(self->cookie, self->button | poweup_bit);
}

/* ========================================================================= *
 * TRIGGERED ALARMS QUEUE
 * ========================================================================= */

static alarm_state_t **queue_slot  = 0;
static size_t          queue_count = 0;
static size_t          queue_alloc = 0;

/* ------------------------------------------------------------------------- *
 * skeleton_queue_send_replies
 * ------------------------------------------------------------------------- */

static
void
skeleton_queue_send_replies(int leave_one)
{
  size_t k = 0;

  if( leave_one )
  {
    for( size_t i = 0; i < queue_count; ++i )
    {
      alarm_state_t *s = queue_slot[i];
      if( s->pending != 0 )
      {
        leave_one = 0;
        break;
      }
    }
  }

  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_state_t *s = queue_slot[i];
    if( s->pending == 0 && leave_one == 0 )
    {
      alarm_state_send_reply(s);
      alarm_state_delete(s);
    }
    else
    {
      leave_one = 0;
      queue_slot[k++] = s;
    }
  }
  queue_count = k;
}

/* ------------------------------------------------------------------------- *
 * skeleton_queue_current_state
 * ------------------------------------------------------------------------- */

static
alarm_state_t *
skeleton_queue_current_state(void)
{
  alarm_state_t *latest = 0;
  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_state_t *s = queue_slot[i];
    if( s->pending != 0 )
    {
      latest = s;
    }
  }
  return latest;
}

/* ------------------------------------------------------------------------- *
 * skeleton_queue_get_state
 * ------------------------------------------------------------------------- */

static
alarm_state_t *
skeleton_queue_get_state(cookie_t cookie)
{
  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_state_t *s = queue_slot[i];
    if( s->cookie == cookie )
    {
      return s;
    }
  }
  return 0;
}

/* ------------------------------------------------------------------------- *
 * skeleton_reserve_slots
 * ------------------------------------------------------------------------- */

static
void
skeleton_reserve_slots(size_t count)
{
  size_t required = queue_count + count;
  if( queue_alloc < required )
  {
    log_debug("RESIZE: %d -> %d\n", queue_alloc, required);

    queue_alloc = required;
    queue_slot = realloc(queue_slot, queue_alloc * sizeof *queue_slot);
  }
}

/* ------------------------------------------------------------------------- *
 * skeleton_queue_add_event
 * ------------------------------------------------------------------------- */

static
void
skeleton_queue_add_event(cookie_t cookie)
{
  if( !skeleton_queue_get_state(cookie) )
  {
    skeleton_reserve_slots(1);

    log_debug("FILLIN: %d == %d\n", queue_count, cookie);

    queue_slot[queue_count++] = alarm_state_create(cookie);
  }
}

/* ------------------------------------------------------------------------- *
 * skeleton_queue_cancel_event
 * ------------------------------------------------------------------------- */

static
void
skeleton_queue_cancel_event(cookie_t cookie)
{
  size_t di = 0;

  for( size_t si = 0; si < queue_count; ++si )
  {
    alarm_state_t *s = queue_slot[si];

    if( s->cookie == cookie )
    {
      log_debug("CANCEL: %d == %d\n", si, cookie);
      alarm_state_delete(s);
    }
    else
    {
      queue_slot[di++] = s;
    }
  }
  log_debug("INUSE: %d -> %d\n",queue_count, di);
  queue_count = di;
}

/* ------------------------------------------------------------------------- *
 * skeleton_queue_get_events
 * ------------------------------------------------------------------------- */

static
cookie_t *
skeleton_queue_get_events(int *pcnt)
{
  int       cnt = queue_count;
  cookie_t *res = calloc(cnt + 1, sizeof *res);;

  for( int i = 0; i < cnt; ++i )
  {
    res[i] = queue_slot[i]->cookie;
  }

  if( pcnt != 0 ) *pcnt = cnt;
  return res;
}

/* ------------------------------------------------------------------------- *
 * skeleton_queue_cancel_events
 * ------------------------------------------------------------------------- */

static
void
skeleton_queue_cancel_events(void)
{
  for( size_t si = 0; si < queue_count; ++si )
  {
    alarm_state_t *s = queue_slot[si];
    alarm_state_delete(s);
  }
  queue_count = 0;
}

/* ========================================================================= *
 * ACTIVE ALARM DIALOG
 * ========================================================================= */

static GtkWidget *zz_window   = 0;
static GtkWidget *zz_segments = 0;
static GtkWidget *zz_title    = 0;
static GtkWidget *zz_message  = 0;
static GtkWidget *zz_buttons  = 0;
static GtkWidget *zz_pending  = 0;
static cookie_t   zz_cookie   = -1;

/* ------------------------------------------------------------------------- *
 * skeleton_dialog_destroy_callback
 * ------------------------------------------------------------------------- */

static void
skeleton_dialog_destroy_callback(GtkWidget * widget, gpointer data)
{
  printf("@ %s() ...\n", __FUNCTION__);
  zz_window = 0;
}

/* ------------------------------------------------------------------------- *
 * skeleton_dialog_button_callback
 * ------------------------------------------------------------------------- */

static void skeleton_dialog_button_callback(GtkWidget *widget, gpointer data)
{
  printf("@ %s() ...\n", __FUNCTION__);

  log_debug("BUTTON: %d\n", (int)data);
  log_debug("COOKIE: %d\n", (int)zz_cookie);

  alarm_event_t *event = 0;
  alarm_state_t *state = 0;

  if( (state = skeleton_queue_get_state(zz_cookie)) )
  {
    log_debug("STATE: %8p\n", state);
    if( (event = alarm_state_get_event(state)) )
    {
      log_debug("EVENT: %8p\n", event);
      state->pending = 0;
      state->button  = (int)data;
      //alarm_state_send_reply(state);
    }
  }
  //skeleton_queue_cancel_event(zz_cookie);
  zz_cookie = -1;
  //gtk_widget_destroy(zz_window);
  skeleton_dialog_rethink();
}

/* ------------------------------------------------------------------------- *
 * skeleton_dialog_destroy
 * ------------------------------------------------------------------------- */

static void skeleton_dialog_destroy(void)
{
  printf("@ %s() ...\n", __FUNCTION__);
  if( zz_window != 0 )
  {
    gtk_widget_destroy(zz_window);
    zz_window = 0;
    zz_cookie = -1;
  }
}
/* ------------------------------------------------------------------------- *
 * skeleton_dialog_create
 * ------------------------------------------------------------------------- */

static void skeleton_dialog_create(void)
{
  printf("@ %s() ...\n", __FUNCTION__);
  if( zz_window == 0 )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * dialog window
     * - - - - - - - - - - - - - - - - - - - */

    zz_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    //gtk_container_set_border_width(GTK_CONTAINER(zz_window), 2);
    gtk_window_set_title(GTK_WINDOW(zz_window), "Alarm Event");
    gtk_window_set_default_size(GTK_WINDOW(zz_window), 200, 50);
    //gtk_window_set_default_icon_from_file(PIXMAPS_DIR "/icon.png", 0);
    g_signal_connect(G_OBJECT(zz_window), "destroy",
                     G_CALLBACK(skeleton_dialog_destroy_callback), NULL);

    {
      /* - - - - - - - - - - - - - - - - - - - *
       * vertical segments
       * - - - - - - - - - - - - - - - - - - - */

      zz_segments = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(zz_window), zz_segments);

      {
        /* - - - - - - - - - - - - - - - - - - - *
         * title frame
         * - - - - - - - - - - - - - - - - - - - */

        zz_title = gtk_frame_new("[ALARM TITLE]");
        gtk_container_add(GTK_CONTAINER(zz_segments), zz_title);

        /* - - - - - - - - - - - - - - - - - - - *
         * message label
         * - - - - - - - - - - - - - - - - - - - */

        zz_message = gtk_label_new("[ALARM MESSAGE]");
        gtk_container_add(GTK_CONTAINER(zz_segments), zz_message);

        /* - - - - - - - - - - - - - - - - - - - *
         * buttons
         * - - - - - - - - - - - - - - - - - - - */

        zz_buttons = gtk_hbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(zz_segments), zz_buttons);

        {
          // ACTIVE ALARM BUTTONS INSERTED HERE by skeleton_dialog_populate()
        }

        /* - - - - - - - - - - - - - - - - - - - *
         * pending dialogs
         * - - - - - - - - - - - - - - - - - - - */

        zz_pending = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(zz_segments), zz_pending);
        {
          // PENDING ALARM LABELS INSERTED HERE by skeleton_dialog_populate()
        }
      }
    }
    gtk_widget_show_all(zz_window);
  }
}

/* ------------------------------------------------------------------------- *
 * skeleton_dialog_populate
 * ------------------------------------------------------------------------- */

static void skeleton_dialog_populate(alarm_state_t *state)
{
  printf("@ %s() ...\n", __FUNCTION__);
  auto void mydestroy(GtkWidget *widget,gpointer data);
  auto void mydestroy(GtkWidget *widget,gpointer data)
  {
    gtk_widget_destroy(widget);
  }

  alarm_event_t *event = 0;

  zz_cookie = -1;

  if( !(event = alarm_state_get_event(state)) )
  {
    goto cleanup;
  }

  skeleton_dialog_create();

  if( zz_window != 0 )
  {
    zz_cookie = state->cookie;

    /* - - - - - - - - - - - - - - - - - - - *
     * alarm title
     * - - - - - - - - - - - - - - - - - - - */

    gtk_frame_set_label(GTK_FRAME(zz_title),
                        if_empty(event->title, "<no title>"));

    /* - - - - - - - - - - - - - - - - - - - *
     * alarm message
     * - - - - - - - - - - - - - - - - - - - */

    gtk_label_set_label(GTK_LABEL(zz_message),
                        if_empty(event->message, "<no message>"));

    /* - - - - - - - - - - - - - - - - - - - *
     * alarm buttons
     * - - - - - - - - - - - - - - - - - - - */

    gtk_widget_hide_all(zz_buttons);
    gtk_container_foreach(GTK_CONTAINER(zz_buttons), mydestroy, 0);

    for( int i = 0; i < event->action_cnt; ++i )
    {
      alarm_action_t *act = &event->action_tab[i];

      if( alarm_action_is_button(act) )
      {
        GtkWidget *button = gtk_button_new_with_label(act->label);
        gtk_container_add(GTK_CONTAINER(zz_buttons), button);

        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(skeleton_dialog_button_callback),
                         (gpointer)i);
      }
    }
    gtk_widget_show_all(zz_buttons);

    /* - - - - - - - - - - - - - - - - - - - *
     * pending alarms
     * - - - - - - - - - - - - - - - - - - - */

    gtk_widget_hide_all(zz_pending);
    gtk_container_foreach(GTK_CONTAINER(zz_pending), mydestroy, 0);

    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_size_new(7 * PANGO_SCALE));

    int pending = 0, N=3;

    for( int n = queue_count; n-- > 0; )
    {
      state = queue_slot[n];

      if( state->cookie != zz_cookie && state->pending )
      {
        event = alarm_state_get_event(state);
        if( event != 0 )
        {
          if( pending < N )
          {
            char txt[128];
            snprintf(txt, sizeof txt, "%s: %s",
                     if_empty(event->title, "<no title>"),
                     if_empty(event->message, "<no message>"));

            GtkWidget *label = gtk_label_new(strip(txt));;
            gtk_label_set_attributes(GTK_LABEL(label), attrs);
            gtk_container_add(GTK_CONTAINER(zz_pending), label);
          }
          pending += 1;
        }
      }
    }

    if( pending )
    {
      if( pending > N )
      {
        char txt[128];
        snprintf(txt, sizeof txt, "... %d more alarms ...", pending - N);
        GtkWidget *label = gtk_label_new(txt);;
        gtk_label_set_attributes(GTK_LABEL(label), attrs);
        gtk_container_add(GTK_CONTAINER(zz_pending), label);
      }

      gtk_widget_show_all(zz_pending);
    }

    pango_attr_list_unref(attrs);

    /* - - - - - - - - - - - - - - - - - - - *
     * show the window
     * - - - - - - - - - - - - - - - - - - - */

// QUARANTINE     gtk_widget_show_all(zz_window);
  }

  cleanup:

  return;
}

/* ------------------------------------------------------------------------- *
 * skeleton_dialog_rethink
 * ------------------------------------------------------------------------- */

static
void
skeleton_dialog_rethink(void)
{
  printf("@ %s() ...\n", __FUNCTION__);

  alarm_state_t *state = 0;

  // in acting dead, do not send reply to final item
  // in the queue before "powerup?" dialog is handled
  skeleton_queue_send_replies(acting_dead != 0);

  for( ;; )
  {
    if( !(state = skeleton_queue_current_state()) )
    {
      // dialog queue is empty -> kill the window
      skeleton_dialog_destroy();

      if( acting_dead && queue_count )
      {
        powerup_show();
      }
      break;
    }

    if( alarm_state_get_event(state) == 0 )
    {
      // looks like the alarm does not exist
      // anymore - deque and ignore it
      skeleton_queue_cancel_event(state->cookie);
      continue;
    }

    powerup_hide();
    skeleton_dialog_populate(state);
    break;
  }
}

/* ========================================================================= *
 * DBUS SERVER
 * ========================================================================= */

// FIXME: these are bound to have standard defs somewhere
#define DBUS_NAME_OWNER_CHANGED "NameOwnerChanged"
#define DBUS_NAME_ACQUIRED      "NameAcquired"

#define MATCH_ALARMD_OWNERCHANGED\
  "type='signal'"\
  ",sender='"DBUS_SERVICE_DBUS"'"\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"ALARMD_SERVICE"'"

#define MATCH_ALARMD_STATUS_IND\
  "type='signal'"\
  ",sender='"ALARMD_SERVICE"'"\
  ",interface='"ALARMD_INTERFACE"'"\
  ",path='"ALARMD_PATH"'"\
  ",member='"ALARMD_QUEUE_STATUS_IND"'"

static DBusConnection *skeleton_server_system_bus    = NULL;
static DBusConnection *skeleton_server_session_bus   = NULL;

static const char * const skeleton_server_system_sigs[] =
{
#ifdef MATCH_ALARMD_STATUS_IND
  MATCH_ALARMD_STATUS_IND,
#endif
  0
};

static const char * const skeleton_server_session_sigs[] =
{
  MATCH_ALARMD_OWNERCHANGED,
  0
};

/* ------------------------------------------------------------------------- *
 * skeleton_server_handle_query  --  handle SYSTEMUI_ALARM_QUERY method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
skeleton_server_handle_query(DBusMessage *msg)
{
  int          cnt = 0;
  cookie_t    *cur = skeleton_queue_get_events(&cnt);
  DBusMessage *rsp = dbusif_reply_create(msg,
                                         DBUS_TYPE_ARRAY,
                                         DBUS_TYPE_INT32, &cur, cnt,
                                         DBUS_TYPE_INVALID);
  free(cur);
  return rsp;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_handle_add  --  handle SYSTEMUI_ALARM_ADD method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
skeleton_server_handle_add(DBusMessage *msg)
{
  DBusMessage   *rsp = 0;
  dbus_int32_t  *vec = 0;
  int            cnt = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_ARRAY,
                                       DBUS_TYPE_INT32, &vec, &cnt,
                                       DBUS_TYPE_INVALID)) )
  {
    skeleton_reserve_slots(cnt);

    for( int i = 0; i < cnt; ++i )
    {
      skeleton_queue_add_event(vec[i]);
    }

    rsp = skeleton_server_handle_query(msg);
  }

  skeleton_dialog_rethink();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_handle_del  -- handle SYSTEMUI_ALARM_DEL method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
skeleton_server_handle_del(DBusMessage *msg)
{
  DBusMessage   *rsp = 0;
  dbus_int32_t  *vec = 0;
  int            cnt = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_ARRAY,
                                       DBUS_TYPE_INT32, &vec, &cnt,
                                       DBUS_TYPE_INVALID)) )
  {
    for( int i = 0; i < cnt; ++i )
    {
      skeleton_queue_cancel_event(vec[i]);
    }
    rsp = skeleton_server_handle_query(msg);
  }

  skeleton_dialog_rethink();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_handle_name_acquired
 * ------------------------------------------------------------------------- */

static DBusMessage *
skeleton_server_handle_name_acquired(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;
  char          *service   = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_STRING, &service,
                                       DBUS_TYPE_INVALID)) )
  {
    log_debug("NAME ACQUIRED: '%s'\n", service);
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_handle_owner  --  handle dbus service ownership signal
 * ------------------------------------------------------------------------- */

static
DBusMessage *
skeleton_server_handle_owner(DBusMessage *msg)
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
      skeleton_queue_cancel_events();
      skeleton_dialog_rethink();
    }
    else
    {
      log_debug("ALARMD CAME UP\n");
      skeleton_queue_cancel_events();
      skeleton_dialog_rethink();
    }
  }
  dbus_error_free(&err);

  return 0;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_handle_status  --  handle alarmd queue status signal
 * ------------------------------------------------------------------------- */

static
DBusMessage *
skeleton_server_handle_status(DBusMessage *msg)
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

  return 0;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_send_queue_status
 * ------------------------------------------------------------------------- */

static guint skeleton_server_send_queue_status_id = 0;

static gboolean
skeleton_server_send_queue_status_cb(void *aptr)
{
  int          cnt = 0;
  cookie_t    *cur = skeleton_queue_get_events(&cnt);
  alarmd_ack_queue(cur, cnt);
  free(cur);

  skeleton_server_send_queue_status_id = 0;
  return FALSE;
}

static void
skeleton_server_send_queue_status(void)
{
  if( skeleton_server_send_queue_status_id == 0)
  {
    skeleton_server_send_queue_status_id = g_idle_add(skeleton_server_send_queue_status_cb, 0);
  }
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_handle_open  --  handle SYSTEMUI_ALARM_OPEN_REQ method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
skeleton_server_handle_open(DBusMessage *msg)
{
  DBusMessage   *rsp = 0;
  dbus_int32_t   val = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32, &val,
                                       DBUS_TYPE_INVALID)) )
  {
    skeleton_reserve_slots(1);
    skeleton_queue_add_event(val);
    val = 1;
    rsp = dbusif_reply_create(msg,
                              DBUS_TYPE_INT32, &val,
                              DBUS_TYPE_INVALID);
  }

  skeleton_dialog_rethink();

  skeleton_server_send_queue_status();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_handle_close  -- handle SYSTEMUI_ALARM_CLOSE_REQ method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
skeleton_server_handle_close(DBusMessage *msg)
{
  DBusMessage   *rsp = 0;
  dbus_int32_t   val = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32, &val,
                                       DBUS_TYPE_INVALID)) )
  {
    skeleton_queue_cancel_event(val);

    val = 1;
    rsp = dbusif_reply_create(msg,
                              DBUS_TYPE_INT32, &val,
                              DBUS_TYPE_INVALID);
  }

  skeleton_dialog_rethink();
  skeleton_server_send_queue_status();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_filter  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static const struct
{
  int          type;
  const char  *iface;
  const char  *object;
  const char  *member;
  DBusMessage *(*func)(DBusMessage *);
  int          result;

} lut[] =
{
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    SYSTEMUI_REQUEST_IF,
    SYSTEMUI_REQUEST_PATH,
    SYSTEMUI_ALARM_ADD,
    skeleton_server_handle_add,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    SYSTEMUI_REQUEST_IF,
    SYSTEMUI_REQUEST_PATH,
    SYSTEMUI_ALARM_DEL,
    skeleton_server_handle_del,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    SYSTEMUI_REQUEST_IF,
    SYSTEMUI_REQUEST_PATH,
    SYSTEMUI_ALARM_QUERY,
    skeleton_server_handle_query,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    SYSTEMUI_REQUEST_IF,
    SYSTEMUI_REQUEST_PATH,
    SYSTEMUI_ALARM_OPEN_REQ,
    skeleton_server_handle_open,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    SYSTEMUI_REQUEST_IF,
    SYSTEMUI_REQUEST_PATH,
    SYSTEMUI_ALARM_CLOSE_REQ,
    skeleton_server_handle_close,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_SIGNAL,
    DBUS_INTERFACE_DBUS,
    DBUS_PATH_DBUS,
    DBUS_NAME_OWNER_CHANGED,
    skeleton_server_handle_owner,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_SIGNAL,
    DBUS_INTERFACE_DBUS,
    DBUS_PATH_DBUS,
    DBUS_NAME_ACQUIRED,
    skeleton_server_handle_name_acquired,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED
  },
#ifdef MATCH_ALARMD_STATUS_IND
  {
    DBUS_MESSAGE_TYPE_SIGNAL,
    ALARMD_INTERFACE,
    ALARMD_PATH,
    ALARMD_QUEUE_STATUS_IND,
    skeleton_server_handle_status,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED
  },
#endif
  { .type = -1, }
};

static
DBusHandlerResult
skeleton_server_filter(DBusConnection *conn,
                       DBusMessage *msg,
                       void *user_data)
{
  DBusHandlerResult   result    = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char         *iface = dbus_message_get_interface(msg);
  const char         *member    = dbus_message_get_member(msg);
  const char         *object    = dbus_message_get_path(msg);
  int                 type      = dbus_message_get_type(msg);
  DBusMessage        *rsp       = 0;

#if ENABLE_LOGGING
  const char         *typestr   = dbusif_get_msgtype_name(type);
// QUARANTINE   log_debug("\n");
  log_debug("----------------------------------------------------------------\n");
  log_debug("BUS = %s\n",
            (conn == skeleton_server_system_bus) ? "SYSTEM" :
            (conn == skeleton_server_session_bus) ? "SESSION" :
            "UNKNOWN");

  log_debug("@ %s: %s(%s, %s, %s)\n", __FUNCTION__, typestr, object, iface, member);
#endif

  if( !iface || !member || !object )
  {
    goto cleanup;
// QUARANTINE     skeleton_server_handle_add();
// QUARANTINE     skeleton_server_handle_del();
// QUARANTINE     skeleton_server_handle_query();
// QUARANTINE     skeleton_server_handle_open();
// QUARANTINE     skeleton_server_handle_close();
// QUARANTINE     skeleton_server_handle_owner();
// QUARANTINE     skeleton_server_handle_status();
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * lookup callback to handle the message
   * and possibly generate a reply
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; lut[i].type != -1; ++i )
  {
    if( lut[i].type != type ) continue;
    if( strcmp(lut[i].iface, iface) ) continue;
    if( strcmp(lut[i].object, object) ) continue;
    if( strcmp(lut[i].member, member) ) continue;

    result = lut[i].result;
    rsp  = lut[i].func(msg);
    break;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * if no responce message was created
   * above and the peer expects reply,
   * create a generic error message
   * - - - - - - - - - - - - - - - - - - - */

  if( type == DBUS_MESSAGE_TYPE_METHOD_CALL )
  {
    if( rsp == 0 && !dbus_message_get_no_reply(msg) )
    {
      rsp = dbus_message_new_error(msg, DBUS_ERROR_FAILED, member);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * send responce if we have something
   * to send
   * - - - - - - - - - - - - - - - - - - - */

  if( rsp != 0 )
  {
    dbus_connection_send(conn, rsp, 0);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup and return to caller
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  if( rsp != 0 )
  {
    dbus_message_unref(rsp);
  }

#if ENABLE_LOGGING
// QUARANTINE   log_debug("----------------------------------------------------------------\n");
// QUARANTINE   log_debug("\n");
#endif

  return result;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_init_system_bus
 * ------------------------------------------------------------------------- */

static
int
skeleton_server_init_system_bus(void)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;

  /* - - - - - - - - - - - - - - - - - - - *
   * connect to system bus
   * - - - - - - - - - - - - - - - - - - - */

  if( (skeleton_server_system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    log_critical("can't connect to system bus: %s: %s\n",
                 err.name, err.message);
    goto cleanup;
  }

  dbus_connection_setup_with_g_main(skeleton_server_system_bus, NULL);
  dbus_connection_set_exit_on_disconnect(skeleton_server_system_bus, 0);

  /* - - - - - - - - - - - - - - - - - - - *
   * add message filter
   * - - - - - - - - - - - - - - - - - - - */

  if( !dbus_connection_add_filter(skeleton_server_system_bus,
                                  skeleton_server_filter, 0, 0) )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * request signals to be sent to us
   * - - - - - - - - - - - - - - - - - - - */

  if( dbusif_add_matches(skeleton_server_system_bus,
                         skeleton_server_system_sigs) == -1 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * claim service name
   * - - - - - - - - - - - - - - - - - - - */

  int ret = dbus_bus_request_name(skeleton_server_system_bus,
                                  SYSTEMUI_SERVICE,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                  &err);

  if( ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
  {
    if( dbus_error_is_set(&err) )
    {
      log_critical("can't claim service name: %s: %s\n",
                   err.name, err.message);
    }
    else
    {
      log_critical("can't claim service name\n");
    }
    goto cleanup;
  }

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
 * skeleton_server_quit_system_bus
 * ------------------------------------------------------------------------- */

static
void
skeleton_server_quit_system_bus(void)
{
  if( skeleton_server_system_bus != 0 )
  {
    dbusif_remove_matches(skeleton_server_system_bus,
                          skeleton_server_system_sigs);

    dbus_connection_remove_filter(skeleton_server_system_bus,
                                  skeleton_server_filter, 0);

    dbus_connection_unref(skeleton_server_system_bus);
    skeleton_server_system_bus = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_init_session_bus
 * ------------------------------------------------------------------------- */

static
int
skeleton_server_init_session_bus(void)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;

  /* - - - - - - - - - - - - - - - - - - - *
   * connect to session bus
   * - - - - - - - - - - - - - - - - - - - */

  if( (skeleton_server_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    log_critical("can't connect to session bus: %s: %s\n",
                 err.name, err.message);
    goto cleanup;
  }

  dbus_connection_setup_with_g_main(skeleton_server_session_bus, NULL);
  dbus_connection_set_exit_on_disconnect(skeleton_server_session_bus, 0);

  /* - - - - - - - - - - - - - - - - - - - *
   * add message filter
   * - - - - - - - - - - - - - - - - - - - */

  if( !dbus_connection_add_filter(skeleton_server_session_bus,
                                  skeleton_server_filter, 0, 0) )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * request signals to be sent to us
   * - - - - - - - - - - - - - - - - - - - */

  if( dbusif_add_matches(skeleton_server_session_bus,
                         skeleton_server_session_sigs) == -1 )
  {
    goto cleanup;
  }

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
 * skeleton_server_quit_session_bus
 * ------------------------------------------------------------------------- */

static
void
skeleton_server_quit_session_bus(void)
{
  if( skeleton_server_session_bus != 0 )
  {
    dbusif_remove_matches(skeleton_server_session_bus,
                          skeleton_server_session_sigs);

    dbus_connection_remove_filter(skeleton_server_session_bus,
                                  skeleton_server_filter, 0);

    dbus_connection_unref(skeleton_server_session_bus);
    skeleton_server_session_bus = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_init
 * ------------------------------------------------------------------------- */

static
int
skeleton_server_init(void)
{
  int res = -1;

  if( skeleton_server_init_system_bus() == 0 )
  {
    if(skeleton_server_init_session_bus() == 0 )
    {
      res = 0;
    }
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * skeleton_server_quit
 * ------------------------------------------------------------------------- */

static
void
skeleton_server_quit(void)
{
  skeleton_server_quit_session_bus();
  skeleton_server_quit_system_bus();
}

/* ========================================================================= *
 * FAKE SYSTEM UI MAINLOOP
 * ========================================================================= */

static int skeleton_mainloop_running = 0;

/* ------------------------------------------------------------------------- *
 * skeleton_mainloop_stop
 * ------------------------------------------------------------------------- */

static
int
skeleton_mainloop_stop(void)
{
  if( ! skeleton_mainloop_running )
  {
    exit(EXIT_FAILURE);
  }

  gtk_main_quit();
  return 0;
}

/* ------------------------------------------------------------------------- *
 * skeleton_mainloop_run
 * ------------------------------------------------------------------------- */

static
int
skeleton_mainloop_run(void)
{
  int error = -1;

  /* - - - - - - - - - - - - - - - - - - - *
   * enter mainloop
   * - - - - - - - - - - - - - - - - - - - */

  log_info("ENTER MAINLOOP\n");
  skeleton_mainloop_running = 1;

  gtk_main();

  skeleton_mainloop_running = 0;
  log_info("LEAVE MAINLOOP\n");

  error = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & exit
   * - - - - - - - - - - - - - - - - - - - */

// QUARANTINE   cleanup:

  return error;
}

/* ========================================================================= *
 * FAKE SUSTEM UI SIGNAL HANDLER
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * skeleton_sighnd_terminate  --  terminate process from signal handler
 * ------------------------------------------------------------------------- */

static
void
skeleton_sighnd_terminate(void)
{
  static int done = 0;

  switch( ++done )
  {
  case 1:
    if( skeleton_mainloop_stop() )
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
 * skeleton_sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

static
void
skeleton_sighnd_handler(int sig)
{
  log_error("Got signal [%d] %s\n", sig, strsignal(sig));

  switch( sig )
  {
  case SIGINT:
  case SIGHUP:
  case SIGTERM:
  case SIGQUIT:
    signal(sig, skeleton_sighnd_handler);
    skeleton_sighnd_terminate();
    break;
  }
}

/* ------------------------------------------------------------------------- *
 * skeleton_sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

static
void
skeleton_sighnd_init(void)
{
  static const int sig[] =
  {
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGTERM,
    -1
  };

  /* - - - - - - - - - - - - - - - - - - - *
   * trap some signals for clean exit
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; sig[i] != -1; ++i )
  {
    signal(sig[i], skeleton_sighnd_handler);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * make writing to closed sockets
   * return -1 instead of raising a
   * SIGPIPE signal
   * - - - - - - - - - - - - - - - - - - - */

  signal(SIGPIPE, SIG_IGN);

  /* - - - - - - - - - - - - - - - - - - - *
   * allow core dumps on segfault
   * - - - - - - - - - - - - - - - - - - - */

#ifndef NDEBUG
  signal(SIGSEGV, SIG_DFL);
#endif

}

/* ========================================================================= *
 * MAIN ENTRY POINT
 * ========================================================================= */

int
main(int ac, char **av)
{
  int exit_code = EXIT_FAILURE;

  acting_dead = (access("/tmp/ACT_DEAD",F_OK) == 0);

  /* - - - - - - - - - - - - - - - - - - - *
   * start logging
   * - - - - - - - - - - - - - - - - - - - */

  log_set_level(LOG_DEBUG);
  log_open("skeleton", LOG_TO_STDERR, 0);

  /* - - - - - - - - - - - - - - - - - - - *
   * sanity checks
   * - - - - - - - - - - - - - - - - - - - */

  if( sizeof(dbus_int32_t) != sizeof(cookie_t) )
  {
    log_critical("sizeof(dbus_int32_t) != sizeof(cookie_t)\n");
    goto cleanup;
  }

  if( getenv("DISPLAY") == 0 )
  {
    log_critical("DISPLAY is not set\n");
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * initialize gtk
   * - - - - - - - - - - - - - - - - - - - */

  if( !gtk_init_check(&ac, &av) )
  {
    log_critical("gtk init failed\n");
    goto cleanup;
  }

// QUARANTINE   gtk_init(&ac, &av);

  /* - - - - - - - - - - - - - - - - - - - *
   * install signal handlers
   * - - - - - - - - - - - - - - - - - - - */

  skeleton_sighnd_init();

  /* - - - - - - - - - - - - - - - - - - - *
   * start dbus service
   * - - - - - - - - - - - - - - - - - - - */

  if( skeleton_server_init() == -1 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * enter mainloop
   * - - - - - - - - - - - - - - - - - - - */

  if( skeleton_mainloop_run() == 0 )
  {
    exit_code = EXIT_SUCCESS;
  }

  cleanup:

  /* - - - - - - - - - - - - - - - - - - - *
   * stop dbbus service
   * - - - - - - - - - - - - - - - - - - - */

  skeleton_server_quit();

  return exit_code;
}
