#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <gtk/gtk.h>

#include <dbus/dbus-glib-lowlevel.h>
#include "../ticker.h"
#include "../clockd_dbus.h"

#include <assert.h>

#include "xutil.h"

#include "libalarm.h"
#include "logging.h"
#include "dbusif.h"
#include "systemui_dbus.h"
#include "alarm_dbus.h"

#define numof(a) (sizeof(a)/sizeof*(a))

#define SCRUMDEMO_SERVICE         "com.nokia.scrumdemo"
#define SCRUMDEMO_PATH           "/com/nokia/scrumdemo"
#define SCRUMDEMO_INTERFACE       "com.nokia.scrumdemo"

#define APPID "SCRUM"

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

#define MATCH_CLOCKD_OWNER_CHANGED\
  "type='signal'"\
  ",sender='"DBUS_SERVICE_DBUS"'"\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='NameOwnerChanged'"\
  ",arg0='"CLOCKD_SERVICE"'"

#define MATCH_CLOCKD_TIME_CHANGED\
  "type='signal'"\
  ",sender='"CLOCKD_SERVICE"'"\
  ",interface='"CLOCKD_INTERFACE"'"\
  ",path='"CLOCKD_PATH"'"\
  ",member='"CLOCKD_TIME_CHANGED"'"

// TODO: remove when available in dbus
#ifndef DBUS_ERROR_INIT
# define DBUS_ERROR_INIT { NULL, NULL, TRUE, 0, 0, 0, 0, NULL }
#endif

/* ========================================================================= *
 * CLOCK ALARM STUFF
 * ========================================================================= */

static const char * const scrumdemo_wday_name[7] =
{
  "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

#if 0
static
char *
scrumdemo_myctime(time_t *t)
{
  static char buf[128];
  struct tm tm;
  ticker_get_local_ex(*t, &tm);
  snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d wd=%s tz=%s dst=%s",
           tm.tm_year + 1900,
           tm.tm_mon + 1,
           tm.tm_mday,
           tm.tm_hour,
           tm.tm_min,
           tm.tm_sec,
           scrumdemo_wday_name[tm.tm_wday],
           tm.tm_zone,
           (tm.tm_isdst > 0) ? "Yes" : "No");
  return buf;
}
#endif

static int
scrumdemo_timediff(time_t now, time_t trg, int *pd, int *ph, int *pm, int *ps)
{
  int d,h,m,s,t = (int)(trg - now);

  s = (t<0) ? -t : t;
  m = s/60, s %= 60;
  h = m/60, m %= 60;
  d = h/24, h %= 24;

  if( pd ) *pd = d;
  if( ph ) *ph = h;
  if( pm ) *pm = m;
  if( ps ) *ps = s;

  return t;
}

static
cookie_t
scrumdemo_clock_recurring(int h, int m, int wd, int verbose)
{
  printf("\n----( recurring alarm %02d:%02d every %s )----\n\n",
         h,m, (wd < 0) ? "day" : scrumdemo_wday_name[wd]);

  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;
  alarm_recur_t  *rec = 0;
  cookie_t        aid = 0;

  // event
  eve = alarm_event_create();
  alarm_event_set_alarm_appid(eve, APPID);
  alarm_event_set_title(eve, "-- title --");
  alarm_event_set_message(eve, "-- message --");

  eve->flags |= ALARM_EVENT_BACK_RESCHEDULE;
  eve->flags |= ALARM_EVENT_BOOT;
  eve->flags |= ALARM_EVENT_SHOW_ICON;

  // STOP
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  alarm_action_set_label(act, "Stop");

  // action: SNOOZE
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  alarm_action_set_label(act, "Snooze");

  // action: recurrence
  rec = alarm_event_add_recurrences(eve, 1);
  rec->mask_hour = 1u   << h;
  rec->mask_min  = 1ull << m;
  rec->mask_wday = (wd < 0) ? ALARM_RECUR_WDAY_ALL : (1u << wd);
  eve->recur_count = -1;

  // send -> alarmd
  aid = alarmd_event_add(eve);
  printf("cookie = %d\n", (int)aid);
  alarm_event_delete(eve);

  if( (eve = alarmd_event_get(aid)) )
  {
    int d=0,h=0,m=0,s=0,t=0;
    t = scrumdemo_timediff(ticker_get_time(),eve->trigger, &d,&h,&m,&s);
    printf("Time left: %dd %dh %dm %ds (%d)\n", d,h,m,s, t);
    printf("\n");
    alarm_event_delete(eve);
  }

  return aid;
}

static
cookie_t
scrumdemo_clock_oneshot(int h, int m, int wd, int verbose)
{
  /* - defined in local time
   * -> leave alarm_tz unset
   * -> re-evaluated if tz changes
   *
   * - do not add recurrence masks
   *
   * - set up trigger time via alarm_tm field
   * -> set tm_hour and tm_min fields as required
   * -> if specific weekday is requested set also tm_wday field
   *
   * - if alarm is missed it must be disabled
   * -> set event flag ALARM_EVENT_DISABLE_DELAYED
   *
   * - if user presses "stop", event must be disabled
   * -> set action flag ALARM_ACTION_TYPE_DISABLE
   */

  printf("\n----( oneshot alarm %02d:%02d %s )----\n\n",
         h,m, (wd < 0) ? "anyday" : scrumdemo_wday_name[wd]);

  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;
  cookie_t        aid = 0;

  // event
  eve = alarm_event_create();
  alarm_event_set_alarm_appid(eve, APPID);
  alarm_event_set_title(eve, "-- title --");
  alarm_event_set_message(eve, "-- message --");

  eve->flags |= ALARM_EVENT_DISABLE_DELAYED;
  eve->flags |= ALARM_EVENT_BOOT;
  eve->flags |= ALARM_EVENT_SHOW_ICON;

  // action: STOP
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_DISABLE;
  alarm_action_set_label(act, "Stop");

  // action: SNOOZE
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  alarm_action_set_label(act, "Snooze");

  eve->alarm_tm.tm_hour = h;
  eve->alarm_tm.tm_min  = m;
  eve->alarm_tm.tm_wday = wd;

  // send -> alarmd
  aid = alarmd_event_add(eve);
  printf("cookie = %d\n", (int)aid);
  alarm_event_delete(eve);

  if( (eve = alarmd_event_get(aid)) )
  {
    int d=0,h=0,m=0,s=0,t=0;
    t = scrumdemo_timediff(ticker_get_time(),eve->trigger, &d,&h,&m,&s);
    printf("Time left: %dd %dh %dm %ds (%d)\n", d,h,m,s, t);
    printf("\n");
    alarm_event_delete(eve);
  }

  return aid;
}

/* ========================================================================= *
 * try to avoid uninteresting dbus
 * messages while debugging in scratchbox
 * ========================================================================= */

static void local_set_time(void)
{
}

static time_t local_get_time(void)
{
  return ticker_get_time();
}

static int  scrumdemo_mainloop_stop(void);
static void scrumdemo_dialog_request_rethink(void);

static const char * const scrumdemo_session_rules[] =
{
  MATCH_ALARMD_OWNERCHANGED,
  MATCH_ALARMD_STATUS_IND,
  0
};
static const char * const scrumdemo_system_rules[] =
{
  MATCH_CLOCKD_OWNER_CHANGED,
  MATCH_CLOCKD_TIME_CHANGED,
  0
};

static const char *timeto(time_t zen, time_t now)
{
  char *res = "";
  static char work[8][64];
  static int curr = 0;

  if( zen < INT_MAX )
  {
    char   *sgn = "T+";
    time_t  t = now - zen;

    if( t < 0 ) sgn = "T-", t = -t;

    int s,m,h,d;

    s = t % 60, t /= 60;
    m = t % 60, t /= 60;
    h = t % 24, t /= 24;
    d = t;

    res = work[curr++ & 7];

    if( d )
    {
      snprintf(res, sizeof *work, "%s%dd%dh%dm%ds", sgn, d, h, m, s);
    }
    else if( h )
    {
      snprintf(res, sizeof *work, "%s%dh%dm%ds", sgn, h, m, s);
    }
    else if( m )
    {
      snprintf(res, sizeof *work, "%s%dm%ds", sgn, m, s);
    }
    else
    {
      snprintf(res, sizeof *work, "%s%d", sgn, s);
    }
  }

  return res;

}

/* ========================================================================= *
 * Miscellaneous utils
 * ========================================================================= */

static inline const char *
scrumdemo_ifempty(const char *s, const char *d)
{
  return (s && *s) ? s : d;
}

/* ========================================================================= *
 * GTK DIALOG
 * ========================================================================= */

#define PIXMAPS_DIR "."
#define PIXMAPS_ICON PIXMAPS_DIR"/icon.png"

static time_t next_alarm = INT_MAX;

static GtkWidget *scrumdemo_window = 0;

static GtkListStore *log_list = 0;
static GtkWidget    *log_view = 0;

static void log_append(const char *fmt, ...)
{
  va_list va;
  char *txt = 0;
  char *pos = 0;
  char *end = 0;

  va_start(va, fmt);
  vasprintf(&txt, fmt, va);
  va_end(va);

  for( pos = txt; pos && *pos; pos = end )
  {
    if( (end = strpbrk(pos, "\r\n")) != 0 )
    {
      *end++ = 0;
    }
    if( *pos != 0 )
    {
      if( log_list != 0 )
      {

        GtkTreeIter  iter;
        gtk_list_store_append(log_list, &iter);
        gtk_list_store_set(log_list, &iter,  0, pos, -1);

        if( log_view != 0 )
        {
          GtkTreePath *path = 0;

          path = gtk_tree_model_get_path(GTK_TREE_MODEL(log_list), &iter);
          gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(log_view),
                                       path,
                                       NULL,
                                       FALSE,
                                       0,0);
          gtk_tree_path_free(path);
        }

      }
      else
      {
        fprintf(stderr, "LOG: %s\n", pos);
      }
    }
  }
  free(txt);
}

static void
scrumdemo_gui_destroy_callback(GtkWidget * widget, gpointer data)
{
  printf("@ %s() ...\n", __FUNCTION__);
  scrumdemo_mainloop_stop();
}

#if 0
static GObject *model = 0;
static void insert_init(void)
{
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(log_view));
  g_object_ref(model);
  gtk_tree_view_set_model(GTK_TREE_VIEW(log_view), NULL);
}

static void insert_done(void)
{
  gtk_tree_view_set_model(GTK_TREE_VIEW(log_view), model);
  g_object_unref(model);
}
#endif

// QUARANTINE static void scrumdemo_dialog_button_callback(GtkWidget *widget, gpointer data)
// QUARANTINE {
// QUARANTINE   printf("@ %s() ...\n", __FUNCTION__);
// QUARANTINE }

static void scrumdemo_log_create(void)
{
  GtkTreeViewColumn   *col = 0;
  GtkCellRenderer *renderer = 0;

  log_list = gtk_list_store_new(1, G_TYPE_STRING);
  log_view = gtk_tree_view_new();

  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "Log");
  gtk_tree_view_append_column(GTK_TREE_VIEW(log_view), col);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  //g_object_set(renderer, "background", "grey90", NULL);
  g_object_set(renderer, "family", "Monospace", NULL);
  g_object_set(renderer, "size-points", 8.0, NULL);
  g_object_set(renderer, "text", "Boooo!", NULL);
  gtk_tree_view_column_add_attribute(col, renderer, "text", 0);

  gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(log_view)),
                              GTK_SELECTION_NONE);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(log_view), TRUE);
  //g_object_unref(log_list);
  gtk_tree_view_set_model(GTK_TREE_VIEW(log_view), GTK_TREE_MODEL(log_list));
}

static GtkWidget    *queue_view = 0;

static GtkWidget    *clock_label = 0;
static gint          clock_timer = 0;

static gboolean clock_callback(gpointer data)
{
  struct tm tm;
  char date[256];
  char tz[64];
  const char *next;

  time_t t = local_get_time();
  localtime_r(&t, &tm);

  *tz = 0;
  ticker_get_timezone(tz, sizeof tz);

  next = timeto(next_alarm, t);

#if 0
  snprintf(date, sizeof date,
           "%04d-%02d-%02d %02d:%02d:%02d %s %s",
           tm.tm_year + 1900,
           tm.tm_mon + 1,
           tm.tm_mday,
           tm.tm_hour,
           tm.tm_min,
           tm.tm_sec,
           tz, next);
#else
  snprintf(date, sizeof date,
           "%02d-%02d-%02d %02d:%02d:%02d %s %s",
           tm.tm_year % 100,
           tm.tm_mon + 1,
           tm.tm_mday,
           tm.tm_hour,
           tm.tm_min,
           tm.tm_sec,
           tz, next);

#endif
  if( clock_label != 0 )
  {
    gtk_label_set_text(GTK_LABEL(clock_label), date);
  }
  return TRUE;
}

static void clock_stop(void)
{
  if( clock_timer != 0 )
  {
    g_source_remove(clock_timer);
    clock_timer = 0;
  }
}

static void clock_start(void)
{
  if( clock_timer == 0 )
  {
    clock_timer = g_timeout_add(1000, clock_callback, 0);
  }
}

enum
{
  Q_COOKIE,
  Q_TRIGGER,
  Q_RECURR,
  Q_TITLE,
  Q_MESSAGE,

  Q_COLUMNS
};

static void queue_clear(void)
{
  GtkTreeModel *list = 0;

  if( queue_view != 0 )
  {
    if( (list = gtk_tree_view_get_model(GTK_TREE_VIEW(queue_view))) )
    {
      gtk_list_store_clear(GTK_LIST_STORE(list));
    }
  }
}

static void queue_append(cookie_t cookie,
                         //time_t trigger,
                         const char *date,
                         int         rcount,
                         const char *title,
                         const char *message)
{
  GtkTreeModel *list = 0;

  if( queue_view != 0 )
  {
    if( (list = gtk_tree_view_get_model(GTK_TREE_VIEW(queue_view))) )
    {
      GtkTreeIter  iter;
      gtk_list_store_append(GTK_LIST_STORE(list), &iter);

// QUARANTINE       char date[256];
// QUARANTINE       struct tm tm;
// QUARANTINE       localtime_r(&trigger, &tm);
// QUARANTINE       snprintf(date, sizeof date,
// QUARANTINE          "%04d-%02d-%02d %02d:%02d:%02d",
// QUARANTINE          tm.tm_year + 1900,
// QUARANTINE          tm.tm_mon + 1,
// QUARANTINE          tm.tm_mday,
// QUARANTINE          tm.tm_hour,
// QUARANTINE          tm.tm_min,
// QUARANTINE          tm.tm_sec);

      gtk_list_store_set(GTK_LIST_STORE(list), &iter,
                         Q_COOKIE, (gint)cookie,
                         Q_TRIGGER, date,
                         Q_RECURR,  (gint)rcount,
                         Q_TITLE,   title,
                         Q_MESSAGE, message,
                         -1);
    }
  }
}

static gint rescan_id = 0;

static gboolean queue_rescan_callback(void *data)
{
  //log_append("@ %s()\n", __FUNCTION__);

  cookie_t *vec = 0;
  alarm_event_t *eve = 0;

  queue_clear();

  if( (vec = alarmd_event_query(0,0, 0,0, 0)) )
  {
    for( int i = 0; vec[i] > 0; ++i )
    {
      if( (eve = alarmd_event_get(vec[i])) )
      {
        char tdate[256];
        char adate[256];
        char tz[256];
        char trigger[256];

        struct tm tm;
        time_t t = local_get_time();

        *adate = *tz = 0;

        ticker_get_timezone(tz, sizeof tz);
        ticker_get_remote(eve->trigger, tz, &tm);
        snprintf(tdate, sizeof tdate,
                 //"%04d-%02d-%02d %02d:%02d:%02d %s (%s)",
                 //tm.tm_year + 1900,
                 "%02d-%02d-%02d %02d:%02d:%02d %s (%s)",
                 tm.tm_year % 100,
                 tm.tm_mon + 1,
                 tm.tm_mday,
                 tm.tm_hour,
                 tm.tm_min,
                 tm.tm_sec,
                 tz,
                 timeto(eve->trigger, t));

        if( *eve->alarm_tz != 0 )
        {
          ticker_get_remote(eve->trigger, eve->alarm_tz, &tm);
          snprintf(adate, sizeof adate,
                   "\n"
                   //"%04d-%02d-%02d %02d:%02d:%02d %s",
                   //tm.tm_year + 1900,
                   "%02d-%02d-%02d %02d:%02d:%02d %s",
                   tm.tm_year % 100,
                   tm.tm_mon + 1,
                   tm.tm_mday,
                   tm.tm_hour,
                   tm.tm_min,
                   tm.tm_sec,
                   eve->alarm_tz);
        }

        snprintf(trigger, sizeof trigger, "%c %s%s",
                 (eve->flags & ALARM_EVENT_DISABLED) ? 'D' : 'E',
                 tdate, adate);

        queue_append(eve->cookie,
                     trigger,
                     eve->recur_count,
                     eve->title,
                     eve->message);
        alarm_event_delete(eve);
      }

    }
    free(vec);
  }

  rescan_id = 0;
  return FALSE;
}

static void queue_rescan(void)
{
  //log_append("@ %s()\n", __FUNCTION__);

  if( rescan_id == 0 )
  {
    rescan_id = g_idle_add(queue_rescan_callback, 0);
  }
}

static void scrumdemo_queue_create(void)
{
  GtkListStore *queue_list = 0;
  GtkTreeViewColumn   *col = 0;
  GtkCellRenderer *renderer = 0;

  queue_list = gtk_list_store_new(Q_COLUMNS,
                                  G_TYPE_INT,
                                  G_TYPE_STRING,
                                  G_TYPE_INT,
                                  G_TYPE_STRING,
                                  G_TYPE_STRING);

  queue_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(queue_list));

  auto void addcol(int index, const char *label);

  auto void addcol(int index, const char *label)
  {
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, label);
    gtk_tree_view_append_column(GTK_TREE_VIEW(queue_view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    g_object_set(renderer, "family", "Monospace", NULL);
    g_object_set(renderer, "size-points", 8.0, NULL);
    gtk_tree_view_column_add_attribute(col, renderer, "text", index);
  }

  addcol(Q_COOKIE,  "ID");
  addcol(Q_TRIGGER, "Trigger");
  addcol(Q_RECURR,  "#");
  addcol(Q_TITLE,   "Title");
  addcol(Q_MESSAGE, "Message");

  gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(queue_view)),
                              GTK_SELECTION_NONE);

  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(queue_view), TRUE);
  g_object_unref(queue_list);

  gtk_tree_view_set_model(GTK_TREE_VIEW(queue_view), GTK_TREE_MODEL(queue_list));
}

static void scrumdemo_adjust_time(GtkWidget *widget, gpointer data)
{
  time_t now = local_get_time();
  time_t add = (time_t)data;
// QUARANTINE   printf("@ %s(%+d) ...\n", __FUNCTION__, (int)add);
// QUARANTINE   log_append("@ %s(%+d) ...\n", __FUNCTION__, (int)add);
  ticker_set_time(now + add);
  queue_rescan();
}

static void scrumdemo_adjust_tz(GtkWidget *widget, gpointer data)
{
  const char *tz = data;
// QUARANTINE   printf("@ %s(%s) ...\n", __FUNCTION__, tz);
// QUARANTINE   log_append("@ %s(%s) ...\n", __FUNCTION__, tz);
  ticker_set_timezone(tz);
  queue_rescan();
}

static void scrumdemo_alarm_clock(GtkWidget *widget, gpointer data)
{
// QUARANTINE   alarm_action_t new_act[2];
// QUARANTINE
// QUARANTINE   alarm_event_t  new_eve =
// QUARANTINE   {
// QUARANTINE     .alarm_appid = APPID,
// QUARANTINE     .title = "Clock",
// QUARANTINE     .alarm_time = -1,
// QUARANTINE     .snooze_secs = 2,
// QUARANTINE     .message = "Two Buttons",
// QUARANTINE     .flags = ALARM_EVENT_SHOW_ICON | ALARM_EVENT_BOOT,
// QUARANTINE
// QUARANTINE     .action_cnt = 0,
// QUARANTINE     .action_tab = new_act,
// QUARANTINE   };
// QUARANTINE
// QUARANTINE   time_t t = local_get_time() + 30;
// QUARANTINE   localtime_r(&t, &new_eve.alarm_tm);
// QUARANTINE
// QUARANTINE   cookie_t cookie;
// QUARANTINE
// QUARANTINE   alarm_action_t *act;
// QUARANTINE
// QUARANTINE   for( int i = 0; i < numof(new_act); ++i )
// QUARANTINE   {
// QUARANTINE     alarm_action_ctor(&new_act[i]);
// QUARANTINE   }
// QUARANTINE
// QUARANTINE   act = &new_act[new_eve.action_cnt++];
// QUARANTINE   act->flags |= ALARM_ACTION_WHEN_RESPONDED;
// QUARANTINE   act->label = "Stop";
// QUARANTINE
// QUARANTINE   act = &new_act[new_eve.action_cnt++];
// QUARANTINE   act->flags |= ALARM_ACTION_WHEN_RESPONDED;
// QUARANTINE   act->flags |= ALARM_ACTION_TYPE_SNOOZE;
// QUARANTINE   act->label = "Snooze";
// QUARANTINE
// QUARANTINE   cookie = alarmd_event_add(&new_eve);
// QUARANTINE
// QUARANTINE   log_append("ADD -> %d\n", (int)cookie);
// QUARANTINE
// QUARANTINE   queue_rescan();

  cookie_t cookie = scrumdemo_clock_oneshot(12,0,-1,0);
  log_append("ADD -> %d\n", (int)cookie);
  queue_rescan();
}
static void scrumdemo_alarm_clockr(GtkWidget *widget, gpointer data)
{
  cookie_t cookie = scrumdemo_clock_recurring(12,0,-1,0);
  log_append("ADD -> %d\n", (int)cookie);
  queue_rescan();
}
static void scrumdemo_alarm_calendar(GtkWidget *widget, gpointer data)
{
// QUARANTINE   log_append("@ %s() ...\n", __FUNCTION__);
  alarm_action_t new_act[3];

  alarm_event_t  new_eve =
  {
    .alarm_appid = APPID,
    .title = "Calendar",
    .alarm_time = -1,
    .snooze_secs = 2,
    .message = "Three Buttons",
    .flags = ALARM_EVENT_SHOW_ICON | ALARM_EVENT_BOOT,
    .alarm_tz = "CET",
    .action_cnt = 0,
    .action_tab = new_act,
  };

  time_t t = local_get_time() + 30;
  ticker_get_remote(t, new_eve.alarm_tz, &new_eve.alarm_tm);

  cookie_t cookie;

  alarm_action_t *act;

  for( int i = 0; i < numof(new_act); ++i )
  {
    alarm_action_ctor(&new_act[i]);
  }

  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->label = "Stop";

  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  act->label = "Snooze";

  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->label = "View";

  cookie = alarmd_event_add(&new_eve);

  log_append("ADD -> %d\n", (int)cookie);

  queue_rescan();
}
static void scrumdemo_alarm_min(GtkWidget *widget, gpointer data)
{
// QUARANTINE   log_append("@ %s() ...\n", __FUNCTION__);
  alarm_action_t new_act[2];
  alarm_recur_t  new_rec[1];

  alarm_event_t  new_eve =
  {
    .alarm_appid = APPID,
    .title = "Recurring",
    .alarm_time = -1,
    .snooze_secs = 2,
    .message = "Every Minute",
    .flags = ALARM_EVENT_BACK_RESCHEDULE,

    .alarm_tm =
    {
      .tm_sec   = 0,
      .tm_min   = -1,
      .tm_hour  = -1,
      .tm_mday  = -1,
      .tm_mon   = -1,
      .tm_year  = -1,
      .tm_wday  = -1,
      .tm_yday  = -1,
      .tm_isdst = -1
    },

    .action_cnt = 0,
    .action_tab = new_act,

    .recurrence_cnt = 0,
    .recurrence_tab = new_rec,

    .recur_secs  = 0,
    .recur_count = -1,
  };

  cookie_t cookie;

  alarm_recur_t *rec;
  alarm_action_t *act;

  for( int i = 0; i < numof(new_act); ++i )
  {
    alarm_action_ctor(&new_act[i]);
  }

  for( int i = 0; i < numof(new_rec); ++i )
  {
    alarm_recur_ctor(&new_rec[i]);
  }

  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->label = "Stop";

  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  act->label = "Snooze";

  rec = &new_rec[new_eve.recurrence_cnt++];

  rec->mask_min = 0;
  for( int m = 0; m < 60; m += 4 )
  {
    rec->mask_min |= 1ULL << m;
  }

  cookie = alarmd_event_add(&new_eve);

  log_append("ADD -> %d\n", (int)cookie);
  queue_rescan();
}

#if 0
static void scrumdemo_alarm_1230(GtkWidget *widget, gpointer data)
{
// QUARANTINE   log_append("@ %s() ...\n", __FUNCTION__);
  alarm_action_t new_act[16];
  alarm_recur_t  new_rec[1];

  alarm_event_t  new_eve =
  {
    .alarm_appid = APPID,
    .title = "Recurring",
    .alarm_time = -1,
    .snooze_secs = 21,
    .message = "12:30",
    .flags = ALARM_EVENT_BACK_RESCHEDULE,

    .alarm_time = local_get_time(),

    .action_cnt = 0,
    .action_tab = new_act,

    .recurrence_cnt = 0,
    .recurrence_tab = new_rec,

    .recur_secs  = 0,
    .recur_count = 3,
  };

  cookie_t cookie;

  alarm_recur_t *rec;
  alarm_action_t *act;

  for( int i = 0; i < numof(new_act); ++i )
  {
    alarm_action_ctor(&new_act[i]);
  }

  for( int i = 0; i < numof(new_rec); ++i )
  {
    alarm_recur_ctor(&new_rec[i]);
  }

  // STOP
  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->label = "Stop";

  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  act->dbus_service   = SCRUMDEMO_SERVICE;
  act->dbus_interface = SCRUMDEMO_INTERFACE;
  act->dbus_path      = SCRUMDEMO_PATH;
  act->dbus_name      = "stopped";

  // SNOOZE
  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  act->label = "Snooze";

  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  act->dbus_service   = SCRUMDEMO_SERVICE;
  act->dbus_interface = SCRUMDEMO_INTERFACE;
  act->dbus_path      = SCRUMDEMO_PATH;
  act->dbus_name      = "snoozed";

  // ON QUEUE
  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_QUEUED;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  act->dbus_service   = SCRUMDEMO_SERVICE;
  act->dbus_interface = SCRUMDEMO_INTERFACE;
  act->dbus_path      = SCRUMDEMO_PATH;
  act->dbus_name      = "queued";

  // ON TRIGGER
  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_TRIGGERED;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  act->dbus_service   = SCRUMDEMO_SERVICE;
  act->dbus_interface = SCRUMDEMO_INTERFACE;
  act->dbus_path      = SCRUMDEMO_PATH;
  act->dbus_name      = "triggered";

  // ON DELETE
  act = &new_act[new_eve.action_cnt++];
  act->flags |= ALARM_ACTION_WHEN_DELETED;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  act->dbus_service   = SCRUMDEMO_SERVICE;
  act->dbus_interface = SCRUMDEMO_INTERFACE;
  act->dbus_path      = SCRUMDEMO_PATH;
  act->dbus_name      = "deleted";

  rec = &new_rec[new_eve.recurrence_cnt++];

  rec->mask_min = 0;

  for( int m = 0; m < 60; m += 1 )
  {
    rec->mask_hour |= 1UL  << 12;
    rec->mask_min  |= 1ULL << 30;
  }

  cookie = alarmd_event_add(&new_eve);

  log_append("ADD -> %d\n", (int)cookie);
  queue_rescan();
}
#endif
static void scrumdemo_alarm_clear(GtkWidget *widget, gpointer data)
{
// QUARANTINE   log_append("@ %s() ...\n", __FUNCTION__);

  cookie_t *vec = 0;

  if( (vec = alarmd_event_query(0,0, 0,0, 0)) )
  {
    for( int i = 0; vec[i] > 0; ++i )
    {
      alarmd_event_del(vec[i]);
    }
    free(vec);
  }
  queue_rescan();
}

static void scrumdemo_log_clear(GtkWidget *widget, gpointer data)
{
  gtk_list_store_clear(GTK_LIST_STORE(log_list));
}

static void scrumdemo_gui_create(void)
{
// QUARANTINE   printf("@ %s() ...\n", __FUNCTION__);

  scrumdemo_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width(GTK_CONTAINER(scrumdemo_window), 2);

  gtk_window_set_title(GTK_WINDOW(scrumdemo_window), "Alarmd Scrum Demo");
  gtk_window_set_default_size(GTK_WINDOW(scrumdemo_window), 200, 50);
  if( access(PIXMAPS_ICON, R_OK) == 0 )
  {
    gtk_window_set_default_icon_from_file(PIXMAPS_DIR "/icon.png", 0);
  }

  g_signal_connect(G_OBJECT(scrumdemo_window), "destroy",
                   G_CALLBACK(scrumdemo_gui_destroy_callback), NULL);

  GtkWidget   *horz = 0;
  GtkWidget   *vert = 0;
  GtkWidget   *four = 0;

  horz = gtk_hbox_new(TRUE, 3);
  gtk_container_add(GTK_CONTAINER(scrumdemo_window), horz);
  {
    vert = gtk_vbox_new(TRUE, 1);
    gtk_container_add(GTK_CONTAINER(horz), vert);

    {
      clock_label = gtk_label_new("date + time");
      gtk_container_add(GTK_CONTAINER(vert), clock_label);

      four = gtk_hbox_new(FALSE, 3);
      gtk_container_add(GTK_CONTAINER(vert), four);
      {
        GtkWidget *button;

        button = gtk_button_new_with_label("+24h");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(24*60*60));

        button = gtk_button_new_with_label("+1h");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(60*60));

        button = gtk_button_new_with_label("+10m");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(10*60));

        button = gtk_button_new_with_label("+1m");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(1*60));

        button = gtk_button_new_with_label("+15s");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(15));
      }

      four = gtk_hbox_new(FALSE, 3);
      gtk_container_add(GTK_CONTAINER(vert), four);
      {
        GtkWidget *button;

        button = gtk_button_new_with_label("-24h");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(-24*60*60));

        button = gtk_button_new_with_label("-1h");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(-60*60));

        button = gtk_button_new_with_label("-10m");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(-10*60));

        button = gtk_button_new_with_label("-1m");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(-1*60));

        button = gtk_button_new_with_label("-15s");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_time),
                         (gpointer)(-15));
      }

      four = gtk_hbox_new(FALSE, 3);
      gtk_container_add(GTK_CONTAINER(vert), four);
      {
        GtkWidget *button;

        button = gtk_button_new_with_label("EET");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_tz),
                         (gpointer)"EET");

        button = gtk_button_new_with_label("CET");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_tz),
                         (gpointer)"CET");

        button = gtk_button_new_with_label("WET");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_tz),
                         (gpointer)"WET");

        button = gtk_button_new_with_label("GMT");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_adjust_tz),
                         (gpointer)"GMT");

      }

      four = gtk_hbox_new(FALSE, 3);
      gtk_container_add(GTK_CONTAINER(vert), four);
      {
        GtkWidget *button;

        button = gtk_button_new_with_label("CLK");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_alarm_clock), 0);

        button = gtk_button_new_with_label("REP");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_alarm_clockr), 0);

        button = gtk_button_new_with_label("CAL");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_alarm_calendar), 0);

        button = gtk_button_new_with_label("MIN");
        gtk_container_add(GTK_CONTAINER(four), button);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(scrumdemo_alarm_min), 0);

// QUARANTINE         button = gtk_button_new_with_label("12:30");
// QUARANTINE         gtk_container_add(GTK_CONTAINER(four), button);
// QUARANTINE         g_signal_connect(G_OBJECT(button), "clicked",
// QUARANTINE                          G_CALLBACK(scrumdemo_alarm_1230), 0);

      }

      four = gtk_hbox_new(FALSE, 3);
      gtk_container_add(GTK_CONTAINER(vert), four);
      {

      GtkWidget *button;
      button = gtk_button_new_with_label("Remove Alarms");
      gtk_container_add(GTK_CONTAINER(four), button);
      g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(scrumdemo_alarm_clear), 0);

      button = gtk_button_new_with_label("Clear Log");
      gtk_container_add(GTK_CONTAINER(four), button);
      g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(scrumdemo_log_clear), 0);
      }
    }

    vert = gtk_vbox_new(TRUE, 1);
    gtk_container_add(GTK_CONTAINER(horz), vert);

    {
      scrumdemo_queue_create();
      gtk_container_add(GTK_CONTAINER(vert), queue_view);
      scrumdemo_log_create();
      gtk_container_add(GTK_CONTAINER(vert), log_view);
    }
  }

// QUARANTINE   vert = gtk_vbox_new(FALSE, 1);
// QUARANTINE   gtk_container_add(GTK_CONTAINER(scrumdemo_window), vert);
// QUARANTINE
// QUARANTINE   frame = gtk_frame_new(TITLE);
// QUARANTINE   gtk_container_add(GTK_CONTAINER(vert), frame);
// QUARANTINE
// QUARANTINE   button = gtk_label_new(MESSAGE);
// QUARANTINE   gtk_container_add(GTK_CONTAINER(frame), button);
// QUARANTINE
// QUARANTINE   horz = gtk_hbox_new(FALSE, 3);
// QUARANTINE   gtk_container_add(GTK_CONTAINER(vert), horz);
// QUARANTINE
// QUARANTINE   for( int i = 0; i < event->action_cnt; ++i )
// QUARANTINE   {
// QUARANTINE     alarm_action_t *act = &event->action_tab[i];
// QUARANTINE
// QUARANTINE     if( !xisempty(act->label) && (act->flags && ALARM_ACTION_WHEN_RESPONDED) )
// QUARANTINE     {
// QUARANTINE       button = gtk_button_new_with_label(act->label);
// QUARANTINE       gtk_container_add(GTK_CONTAINER(horz), button);
// QUARANTINE       g_signal_connect(G_OBJECT(button), "clicked",
// QUARANTINE                        G_CALLBACK(scrumdemo_dialog_button_callback),
// QUARANTINE                        (gpointer)i);
// QUARANTINE     }
// QUARANTINE   }
// QUARANTINE
// QUARANTINE   scrumdemo_dialog_pending = gtk_vbox_new(FALSE, 0);
// QUARANTINE   gtk_container_add(GTK_CONTAINER(vert), scrumdemo_dialog_pending);

  gtk_widget_show_all(scrumdemo_window);
}

/* ========================================================================= *
 * RETHINK CALLBACK
 * ========================================================================= */

static guint scrumdemo_rethink_id = 0;

/* ------------------------------------------------------------------------- *
 * scrumdemo_dialog_rethink_callback  --  handle changes in queue state
 * ------------------------------------------------------------------------- */

static
gboolean
scrumdemo_dialog_rethink_callback(gpointer data)
{
  // we are done for now
  scrumdemo_rethink_id = 0;
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * scrumdemo_dialog_request_rethink
 * ------------------------------------------------------------------------- */

static
void
scrumdemo_dialog_request_rethink(void)
{
  if( scrumdemo_rethink_id == 0 )
  {
    scrumdemo_rethink_id = g_idle_add(scrumdemo_dialog_rethink_callback, 0);
  }
}

/* ========================================================================= *
 * FAKE SYSTEM UI SERVER
 * ========================================================================= */

static DBusConnection *scrumdemo_session_bus    = NULL;
static DBusConnection *scrumdemo_system_bus    = NULL;

/* ------------------------------------------------------------------------- *
 * scrumdemo_dbus_filter  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
scrumdemo_dbus_filter(DBusConnection *conn,
                     DBusMessage *msg,
                     void *user_data)
{
  DBusHandlerResult   result    = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char         *interface = dbus_message_get_interface(msg);
  const char         *member    = dbus_message_get_member(msg);
  const char         *object    = dbus_message_get_path(msg);
  int                 type      = dbus_message_get_type(msg);
  DBusMessage        *rsp       = 0;
  const char         *typestr   = dbusif_get_msgtype_name(type);

  log_debug("----------------------------------------------------------------\n");
  log_debug("%s(%s, %s, %s)\n", typestr, object, interface, member);

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

    if( serv != 0 && !strcmp(serv, CLOCKD_SERVICE) )
    {
      if( !curr || !*curr )
      {
        log_debug("CLOCKD WENT DOWN\n");
        log_append("CLOCKD WENT DOWN\n");
      }
      else
      {
        log_debug("CLOCKD CAME UP\n");
        log_append("CLOCKD CAME UP\n");
        ticker_get_synced();
      }
    }

    if( serv != 0 && !strcmp(serv, ALARMD_SERVICE) )
    {
      if( !curr || !*curr )
      {
        log_debug("ALARMD WENT DOWN\n");
        log_append("ALARMD WENT DOWN\n");
      }
      else
      {
        log_debug("ALARMD CAME UP\n");
        log_append("ALARMD CAME UP\n");
        queue_rescan();
      }
    }

    dbus_error_free(&err);
  }

  if( type == DBUS_MESSAGE_TYPE_SIGNAL &&
      !strcmp(interface, CLOCKD_INTERFACE) &&
      !strcmp(object,CLOCKD_PATH) &&
      !strcmp(member, CLOCKD_TIME_CHANGED) )
  {
    log_debug("CLOCKD_TIME_CHANGED\n");
    log_append("CLOCKD_TIME_CHANGED\n");
    ticker_get_synced();
    local_set_time();
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

    next_alarm = INT_MAX;
    if( next_alarm > dt ) next_alarm = dt;
    if( next_alarm > ad ) next_alarm = ad;
    if( next_alarm > nb ) next_alarm = nb;

    time_t t = local_get_time();
    auto int timeto(time_t d);
    auto int timeto(time_t d)
    {
      return (d < INT_MAX) ? (d-t) : 9999;
    }
    log_append("Q: act=%d, dtop=%+d, actd=%+d, norm=%+d\n",
               ac, timeto(dt), timeto(ad), timeto(nb));
    queue_rescan();
  }

  if( !strcmp(interface, SCRUMDEMO_INTERFACE) && !strcmp(object, SCRUMDEMO_PATH) )
  {
    result = DBUS_HANDLER_RESULT_HANDLED;

    dbus_int32_t cookie = -1;

    dbusif_method_parse_args(msg,
                             DBUS_TYPE_INT32, &cookie,
                             DBUS_TYPE_INVALID);

    log_append("DBUS: %s %d\n", member, (int)cookie);

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

  return result;
}

/* ------------------------------------------------------------------------- *
 * scrumdemo_dbus_init
 * ------------------------------------------------------------------------- */

static
int
scrumdemo_dbus_init(void)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;

  /* - - - - - - - - - - - - - - - - - - - *
   * session bus
   * - - - - - - - - - - - - - - - - - - - */

  if( (scrumdemo_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    goto cleanup;
  }

  int ret = dbus_bus_request_name(scrumdemo_session_bus, SCRUMDEMO_SERVICE,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);

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

  dbus_connection_setup_with_g_main(scrumdemo_session_bus, NULL);
  dbus_connection_set_exit_on_disconnect(scrumdemo_session_bus, 0);

  if( !dbus_connection_add_filter(scrumdemo_session_bus,
                                  scrumdemo_dbus_filter, 0, 0) )
  {
    goto cleanup;
  }

  dbusif_add_matches(scrumdemo_session_bus, scrumdemo_session_rules);

  /* - - - - - - - - - - - - - - - - - - - *
   * system bus
   * - - - - - - - - - - - - - - - - - - - */

  if( (scrumdemo_system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    goto cleanup;
  }

  dbus_connection_setup_with_g_main(scrumdemo_system_bus, NULL);
  dbus_connection_set_exit_on_disconnect(scrumdemo_system_bus, 0);

  if( !dbus_connection_add_filter(scrumdemo_system_bus,
                                  scrumdemo_dbus_filter, 0, 0) )
  {
    goto cleanup;
  }

  dbusif_add_matches(scrumdemo_system_bus, scrumdemo_system_rules);

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
 * scrumdemo_dbus_quit
 * ------------------------------------------------------------------------- */

static
void
scrumdemo_dbus_quit(void)
{

  if( scrumdemo_session_bus != 0 )
  {
    dbusif_remove_matches(scrumdemo_session_bus, scrumdemo_session_rules);
    dbus_connection_remove_filter(scrumdemo_session_bus,
                                  scrumdemo_dbus_filter, 0);
    dbus_connection_unref(scrumdemo_session_bus);
    scrumdemo_session_bus = 0;
  }

  if( scrumdemo_system_bus != 0 )
  {
    dbusif_remove_matches(scrumdemo_system_bus, scrumdemo_system_rules);
    dbus_connection_remove_filter(scrumdemo_system_bus,
                                  scrumdemo_dbus_filter, 0);
    dbus_connection_unref(scrumdemo_system_bus);
    scrumdemo_system_bus = 0;
  }
}
/* ========================================================================= *
 * FAKE SUSTEM UI SIGNAL HANDLER
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * scrumdemo_sighnd_terminate  --  terminate process from signal handler
 * ------------------------------------------------------------------------- */

static
void
scrumdemo_sighnd_terminate(void)
{
  static int done = 0;

  switch( ++done )
  {
  case 1:
    if( scrumdemo_mainloop_stop() )
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
 * scrumdemo_sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

static
void
scrumdemo_sighnd_handler(int sig)
{
  log_error("Got signal [%d] %s\n", sig, strsignal(sig));

  switch( sig )
  {
  case SIGINT:
  case SIGHUP:
  case SIGTERM:
  case SIGQUIT:
    signal(sig, scrumdemo_sighnd_handler);
    scrumdemo_sighnd_terminate();
    break;
  }
}

/* ------------------------------------------------------------------------- *
 * scrumdemo_sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

static
void
scrumdemo_sighnd_init(void)
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
    signal(sig[i], scrumdemo_sighnd_handler);
  }
}

/* ========================================================================= *
 * FAKE SYSTEM UI MAINLOOP
 * ========================================================================= */

static int scrumdemo_mainloop_running = 0;

/* ------------------------------------------------------------------------- *
 * scrumdemo_mainloop_stop
 * ------------------------------------------------------------------------- */

static
int
scrumdemo_mainloop_stop(void)
{
  if( !scrumdemo_mainloop_running )
  {
    exit(EXIT_FAILURE);
  }
  scrumdemo_mainloop_running = 0;
  gtk_main_quit();
  return 0;
}

/* ------------------------------------------------------------------------- *
 * scrumdemo_mainloop_run
 * ------------------------------------------------------------------------- */

static
int
scrumdemo_mainloop_run(int argc, char* argv[])
{
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

  scrumdemo_sighnd_init();

  /* - - - - - - - - - - - - - - - - - - - *
   * start dbus service
   * - - - - - - - - - - - - - - - - - - - */

  if( scrumdemo_dbus_init() == -1 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * enter mainloop
   * - - - - - - - - - - - - - - - - - - - */

  scrumdemo_gui_create();

  scrumdemo_dialog_request_rethink();

  queue_clear();
// QUARANTINE   queue_append(123, local_get_time(), "Clock", "stuffendahl");
// QUARANTINE   queue_append(456, local_get_time(), "Clock", "GGG");
// QUARANTINE   queue_append(789, local_get_time(), "Clock", "DASPOOK");

  clock_start();
  queue_rescan();

  log_info("ENTER MAINLOOP\n");
  scrumdemo_mainloop_running = 1;
  gtk_main();
  scrumdemo_mainloop_running = 0;
  log_info("LEAVE MAINLOOP\n");

  clock_stop();

  exit_code = EXIT_SUCCESS;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & exit
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  scrumdemo_dbus_quit();

  return exit_code;
}

/* ========================================================================= *
 * MAIN ENTRY POINT
 * ========================================================================= */

int
main(int ac, char **av)
{
  log_set_level(LOG_DEBUG);
  log_open("scrumdemo", LOG_TO_STDERR, 0);

  ticker_use_libtime(1);

  ticker_get_synced();
  local_set_time();

  gtk_init(&ac, &av);

  log_debug("ENTER MAIN\n");
  return scrumdemo_mainloop_run(ac, av);
}
