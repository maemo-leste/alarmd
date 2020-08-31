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

#include "server.h"
#include "logging.h"
#include "queue.h"
#include "ticker.h"
#include "dbusif.h"
#include "xutil.h"
#include "hwrtc.h"
#include "serialize.h"
#include "mainloop.h"

#include "ipc_statusbar.h"
#include "ipc_icd.h"
#include "ipc_exec.h"
#include "ipc_systemui.h"
#include "ipc_dsme.h"

#include "alarm_dbus.h"
#include "missing_dbus.h"
#include "systemui_dbus.h"
#include "clockd_dbus.h"
#include <clock-plugin-dbus.h>
#include <mce/dbus-names.h>

#include <dbus/dbus-glib-lowlevel.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>

#define SNOOZE_HIJACK_FIX 1  // changes meaning of eve->snooze_total
#define SNOOZE_ADJUST_FIX 1  // snooze triggers follow system time

/* ========================================================================= *
 * Local prototypes
 * ========================================================================= */

static char              **server_get_dsme_signal_matches       (void);
static char               *server_repr_tm                       (const struct tm *tm, const char *tz, char *buff, size_t size);
static time_t              server_bump_time                     (time_t base, time_t skip, time_t target);

static gboolean            server_queue_save_cb                 (gpointer data);
static gboolean            server_queue_idle_cb                 (gpointer data);
static void                server_queue_cancel_save             (void);
static void                server_queue_request_save            (void);
static void                server_queue_forced_save             (void);

static unsigned            server_state_get                     (void);
static void                server_state_clr                     (unsigned clr);
static void                server_state_set                     (unsigned clr, unsigned set);

static void                server_queuestate_reset              (void);
static void                server_queuestate_invalidate         (void);
static void                server_queuestate_init               (void);
static void                server_queuestate_sync               (void);

static void                server_limbo_disable                 (const char *reason);
static gboolean            server_limbo_disable_cb              (gpointer data);
static void                server_limbo_disable_delayed         (const char *reason);

static void                server_timestate_read                (void);
static void                server_timestate_sync                (void);
static int                 server_timestate_changed             (void);
static void                server_timestate_init                (void);

static void                server_state_init                    (void);
static void                server_state_sync                    (void);
static const char         *server_state_get_systemui_service    (void);

static unsigned            server_action_get_type               (alarm_action_t *action);
static unsigned            server_action_get_when               (alarm_action_t *action);
static int                 server_action_do_snooze              (alarm_event_t *event, alarm_action_t *action);
static int                 server_action_do_disable             (alarm_event_t *event, alarm_action_t *action);
static int                 server_action_do_exec                (alarm_event_t *event, alarm_action_t *action);
static int                 server_action_do_dbus                (alarm_event_t *event, alarm_action_t *action);
static void                server_action_do_all                 (alarm_event_t *event, alarm_action_t *action);

static void                server_event_do_state_actions        (alarm_event_t *eve, unsigned when);
static void                server_event_do_response_actions     (alarm_event_t *eve);
static const char         *server_event_get_tz                  (alarm_event_t *self);
static unsigned            server_event_get_boot_mask           (alarm_event_t *eve);
static time_t              server_event_get_snooze              (alarm_event_t *eve);
static int                 server_event_is_snoozed              (alarm_event_t *eve);
static int                 server_event_get_buttons             (alarm_event_t *self, int *vec, int size);
static time_t              server_event_get_next_trigger        (time_t t0, time_t t1, alarm_event_t *self);
static int                 server_event_evaluate_initial_trigger(alarm_event_t *self);

static time_t              server_clock_back_delta              (void);
static time_t              server_clock_forw_delta              (void);
static gboolean            server_clock_source_is_stable        (void);

static gboolean            server_wakeup_start_cb               (gpointer data);
static void                server_cancel_wakeup                 (void);
static void                server_request_wakeup                (time_t tmo);

static int                 server_handle_systemui_ack           (cookie_t *vec, int cnt);
static void                server_systemui_ack_cb               (dbus_int32_t *vec, int cnt);
static int                 server_handle_systemui_rsp           (cookie_t cookie, int button);

static void                server_rethink_new                   (void);
static void                server_rethink_waitconn              (void);
static void                server_rethink_queued                (void);
static void                server_rethink_missed                (void);
static void                server_rethink_postponed             (void);
static void                server_rethink_limbo                 (void);
static void                server_rethink_triggered             (void);
static void                server_rethink_waitsysui             (void);
static void                server_rethink_sysui_req             (void);
static void                server_rethink_sysui_ack             (void);
static void                server_rethink_sysui_rsp             (void);
static void                server_rethink_snoozed               (void);
static void                server_rethink_served                (void);
static void                server_rethink_recurring             (void);
static void                server_rethink_deleted               (void);
static int                 server_rethink_timechange            (void);

static void                server_broadcast_timechange_handled  (void);
static gboolean            server_rethink_start_cb              (gpointer data);
static void                server_rethink_request               (int delayed);

static DBusMessage        *server_handle_set_debug              (DBusMessage *msg);
static DBusMessage        *server_handle_CUD                    (DBusMessage *msg);
static DBusMessage        *server_handle_RFS                    (DBusMessage *msg);
static DBusMessage        *server_handle_snooze_get             (DBusMessage *msg);
static DBusMessage        *server_handle_snooze_set             (DBusMessage *msg);
static DBusMessage        *server_handle_event_add              (DBusMessage *msg);
static DBusMessage        *server_handle_event_update           (DBusMessage *msg);
static DBusMessage        *server_handle_event_del              (DBusMessage *msg);
static DBusMessage        *server_handle_event_query            (DBusMessage *msg);
static DBusMessage        *server_handle_event_get              (DBusMessage *msg);
static DBusMessage        *server_handle_event_ack              (DBusMessage *msg);
static DBusMessage        *server_handle_queue_ack              (DBusMessage *msg);
static DBusMessage        *server_handle_name_acquired          (DBusMessage *msg);
static DBusMessage        *server_handle_save_data              (DBusMessage *msg);
static DBusMessage        *server_handle_shutdown               (DBusMessage *msg);
static DBusMessage        *server_handle_init_done              (DBusMessage *msg);
static DBusMessage        *server_handle_hildon_ready           (DBusMessage *msg);
static DBusMessage        *server_handle_name_owner_chaned      (DBusMessage *msg);
static DBusMessage        *server_handle_time_change            (DBusMessage *msg);

static DBusHandlerResult   server_session_bus_cb                (DBusConnection *conn, DBusMessage *msg, void *user_data);
static DBusHandlerResult   server_system_bus_cb                 (DBusConnection *conn, DBusMessage *msg, void *user_data);

static void                server_icd_status_cb                 (int connected);

static int                 server_init_session_bus              (void);
static int                 server_init_system_bus               (void);
static void                server_quit_session_bus              (void);
static void                server_quit_system_bus               (void);

/* ========================================================================= *
 * Configuration
 * ========================================================================= */

/** Various constants affecting alarmd behaviour */
typedef enum alarmlimits
{
  /** If alarm event is triggered more than SERVER_MISSED_LIMIT
   *  seconds late, it will be handled as delayed */
  //SERVER_MISSED_LIMIT = 10,

  SERVER_MISSED_LIMIT = 59,

  /** The hardware /dev/rtc alarm interrupt is set this many
   *  seconds before the actual alarm time to give the system
   *  time to boot up to acting dead mode before the alarm
   *  time.
   */
  POWERUP_COMPENSATION_SECS = 60,

  /** The hardware /dev/rtc alarm interrupts are set at minimum
   *  this far from the current time.
   */
  ALARM_INTERRUPT_LIMIT_SECS = 60,

  /** Saving of alarm data is delayed until there is at least
   *  this long period of no more changes.
   *
   *  Note: data is saved immediately upon "save data" signal
   *        from dsme or when alarmd exits.
   */
  SERVER_QUEUE_SAVE_DELAY_MSEC = 1 * 1000,

  SERVER_POWERUP_BIT = 1<<31,
} alarmlimits;

/* ========================================================================= *
 * D-Bus Connections
 * ========================================================================= */

static DBusConnection *server_session_bus = NULL;
static DBusConnection *server_system_bus  = NULL;

/* ========================================================================= *
 * D-Bus signal masks
 * ========================================================================= */

// FIXME: these are bound to have standard defs somewhere
#define DBUS_NAME_OWNER_CHANGED "NameOwnerChanged"
#define DBUS_NAME_ACQUIRED      "NameAcquired"

/* ------------------------------------------------------------------------- *
 * clock applet  --  clock alarm status signalled when service is available
 * ------------------------------------------------------------------------- */

#define MATCH_STATUSAREA_CLOCK_OWNERCHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"STATUSAREA_CLOCK_SERVICE"'"

/* ------------------------------------------------------------------------- *
 * systemui  --  alarm dialogs can be shown when system ui is up
 * ------------------------------------------------------------------------- */

#define MATCH_SYSTEMUI_OWNER_CHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"SYSTEMUI_SERVICE"'"

/* ------------------------------------------------------------------------- *
 * hildon home  --  used for enabling alarm dialogs
 * see also: server_limbo_set_control(DESKTOP_WAIT_HOME)
 * ------------------------------------------------------------------------- */

// FIXME: is this available in some include file?
#define HOME_SERVICE "com.nokia.HildonDesktop.Home"

#define MATCH_HOME_OWNER_CHANGED\
  "type='signal'"\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"HOME_SERVICE"'"

/* ------------------------------------------------------------------------- *
 * desktop signal from /etc/X11/Xsession.post/21hildon-desktop-wait
 * see also: server_limbo_set_control(DESKTOP_WAIT_HILDON)
 * ------------------------------------------------------------------------- */

#define HILDON_SIG_IF     "com.nokia.HildonDesktop"
#define HILDON_SIG_PATH   "/com/nokia/HildonDesktop/ready"
#define HILDON_SIG_READY  "ready"

#define MATCH_HILDON_SIGNAL \
  "type='signal'"\
  ",interface='"HILDON_SIG_IF"'"\
  ",path='"HILDON_SIG_PATH"'"\
  ",member='"HILDON_SIG_READY"'"

/* ------------------------------------------------------------------------- *
 * startup signal from /etc/X11/Xsession.post/99initdone
 * see also: server_limbo_set_control(DESKTOP_WAIT_STARTUP)
 * ------------------------------------------------------------------------- */

#define STARTUP_SIG_IF        "com.nokia.startup.signal"
#define STARTUP_SIG_PATH      "/com/nokia/startup/signal"
#define STARTUP_SIG_INIT_DONE "init_done"

#define MATCH_STARTUP_SIGNAL \
  "type='signal'"\
  ",interface='"STARTUP_SIG_IF"'"\
  ",path='"STARTUP_SIG_PATH"'"\
  ",member='"STARTUP_SIG_INIT_DONE"'"

/* ------------------------------------------------------------------------- *
 * mce  --  track ownership and rebroadcast queue status when mce comes up
 * ------------------------------------------------------------------------- */

#define MATCH_MCE_OWNER_CHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"MCE_SERVICE"'"

/* ------------------------------------------------------------------------- *
 * clockd  --  use libtime interface only when clockd is up
 * ------------------------------------------------------------------------- */

#if HAVE_LIBTIME

#define MATCH_CLOCKD_OWNER_CHANGED\
  "type='signal'"\
  /*",sender='"DBUS_SERVICE_DBUS"'"*/\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='"DBUS_NAME_OWNER_CHANGED"'"\
  ",arg0='"CLOCKD_SERVICE"'"

#define MATCH_CLOCKD_TIME_CHANGED\
  "type='signal'"\
  /*",sender='"CLOCKD_SERVICE"'"*/\
  ",interface='"CLOCKD_INTERFACE"'"\
  ",path='"CLOCKD_PATH"'"\
  ",member='"CLOCKD_TIME_CHANGED"'"

#endif /* HAVE_LIBTIME */

/* ------------------------------------------------------------------------- *
 * session bus signals to watch
 * ------------------------------------------------------------------------- */

static const char * const sessionbus_signals[] =
{
  MATCH_HOME_OWNER_CHANGED,
  MATCH_STATUSAREA_CLOCK_OWNERCHANGED,
  0
};

/* ------------------------------------------------------------------------- *
 * system bus signals to watch
 * ------------------------------------------------------------------------- */

static const char * const systembus_signals[] =
{
  MATCH_STARTUP_SIGNAL,
  MATCH_HILDON_SIGNAL,

  MATCH_SYSTEMUI_OWNER_CHANGED,

  MATCH_MCE_OWNER_CHANGED,

#if HAVE_LIBTIME
  MATCH_CLOCKD_OWNER_CHANGED,
  MATCH_CLOCKD_TIME_CHANGED,
#endif
  0
};

/* ------------------------------------------------------------------------- *
 * server_get_dsme_signal_matches  --  get dsme signal matches as str array
 * ------------------------------------------------------------------------- */

static char **server_get_dsme_signal_matches(void)
{
  char **result = 0;

  char  *dsme_owner  = 0;
  char  *dsme_signal = 0;

  asprintf(&dsme_owner,
           "type='signal'"
           ",interface='%s'"
           ",path='%s'"
           ",member='%s'"
           ",arg0='%s'",
           DBUS_INTERFACE_DBUS,
           DBUS_PATH_DBUS,
           DBUS_NAME_OWNER_CHANGED,
           DSME_SERVICE);

  asprintf(&dsme_signal,
           "type='signal'"
           ",interface='%s'"
           ",path='%s'",
           DSME_SIGNAL_IF,
           DSME_SIGNAL_PATH);

  if( dsme_owner && dsme_signal )
  {
    if( (result = calloc(3, sizeof *result)) != 0 )
    {
      result[0] = dsme_owner,  dsme_owner = 0;
      result[1] = dsme_signal, dsme_signal = 0;
      result[2] = 0;
    }
  }

  free(dsme_owner);
  free(dsme_signal);

  return result;
}

/* ========================================================================= *
 * Misc utilities
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_repr_tm  --  struct tm to string
 * ------------------------------------------------------------------------- */

static
char *
server_repr_tm(const struct tm *tm, const char *tz, char *buff, size_t size)
{
  static const char * const wday[7] =
  {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };

  static const char * const mon[12] =
  {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };

  snprintf(buff, size,
           "%s %04d-%s-%02d %02d:%02d:%02d (%d/%d) TZ=%s",
           (tm->tm_wday < 0) ? "???" : wday[tm->tm_wday],
           (tm->tm_year < 0) ? -1 : (tm->tm_year + 1900),
           (tm->tm_mon  < 0) ? "???" : mon[tm->tm_mon],
           tm->tm_mday,
           tm->tm_hour,
           tm->tm_min,
           tm->tm_sec,
           tm->tm_yday,
           tm->tm_isdst,
           tz);
  return buff;
}

/* ------------------------------------------------------------------------- *
 * server_bump_time
 * ------------------------------------------------------------------------- */

static
time_t
server_bump_time(time_t base, time_t skip, time_t target)
{
  /* return base + skip * N that is larger than target */

  if( target < base )
  {
    time_t add = (base - target) % skip;
    return target + (add ? add : skip);
  }
  return target + skip - (target - base) % skip;
}

/* ========================================================================= *
 * Delayed Database Save
 * ========================================================================= */

static guint  server_queue_save_id = 0;

/* ------------------------------------------------------------------------- *
 * server_queue_save_cb  --  save for real after timeout
 * ------------------------------------------------------------------------- */

static
gboolean
server_queue_save_cb(gpointer data)
{
  queue_save();
  server_queue_save_id = 0;
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_queue_idle_cb  --  wait for idle before starting timeout
 * ------------------------------------------------------------------------- */

static
gboolean
server_queue_idle_cb(gpointer data)
{
  server_queue_save_id = g_timeout_add(SERVER_QUEUE_SAVE_DELAY_MSEC, server_queue_save_cb, 0);
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_queue_cancel_save  --  cancel save request
 * ------------------------------------------------------------------------- */

static
void
server_queue_cancel_save(void)
{
  if( server_queue_save_id != 0 )
  {
    g_source_remove(server_queue_save_id);
    server_queue_save_id = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * server_queue_request_save  --  request db save after idle + timeout
 * ------------------------------------------------------------------------- */

static
void
server_queue_request_save(void)
{
  if( server_queue_save_id == 0 )
  {
    server_queue_save_id = g_idle_add(server_queue_idle_cb, 0);
  }
}

/* ------------------------------------------------------------------------- *
 * server_queue_forced_save  --  save db immediately
 * ------------------------------------------------------------------------- */

static
void
server_queue_forced_save(void)
{
  server_queue_cancel_save();
  server_queue_save_cb(0);
}

/* ========================================================================= *
 * Server State
 * ========================================================================= */

enum
{
  SF_CONNECTED    = 1 << 0, // we have internet connection
  SF_STARTUP      = 1 << 1, // alarmd is starting up

  SF_SYSTEMUI_UP  = 1 << 2, // system ui is currently up
  SF_SYSTEMUI_DN  = 1 << 3, // system ui has been down

  SF_CLOCKD_UP    = 1 << 4, // clockd is currently up
  SF_CLOCKD_DN    = 1 << 5, // clockd has been down

  SF_DSME_UP      = 1 << 6, // dsme is currently up
  SF_DSME_DN      = 1 << 7, // dsme has been down

  SF_STATUSBAR_UP = 1 << 8, // statusbar is currently up
  SF_STATUSBAR_DN = 1 << 9, // statusbar has been down

  SF_TZ_CHANGED   = 1 << 10, // timezone change from clockd
  SF_CLK_CHANGED  = 1 << 11, // clock change from clocd
  SF_CLK_MV_FORW  = 1 << 12, // clock moved forwards
  SF_CLK_MV_BACK  = 1 << 13, // clock moved backwards

  // UNUSED       = 1 << 14,
  // UNUSED       = 1 << 15,

  SF_ACT_DEAD     = 1 << 16, // act dead mode active
  SF_DESKTOP_UP   = 1 << 17, // desktop boot finished

  SF_MCE_UP       = 1 << 18, // mce is currently up
  SF_MCE_DN       = 1 << 19, // mce has been down

  SF_CLK_BCAST    = 1 << 20, // clock change related actions done

  SF_SEND_POWERUP = 1 << 21, // act dead -> user from system ui

};

static unsigned server_state_prev = 0;
static unsigned server_state_real = 0;
static unsigned server_state_mask = 0;
static unsigned server_state_fake = 0;

static int server_icons_curr = 0; // alarms with statusbar icon flag
static int server_icons_prev = 0;

static int   server_limbo_wait_state  = DESKTOP_WAIT_STARTUP;
static int   server_limbo_timeout_len = 60;
static guint server_limbo_timeout_id  = 0;

/* ------------------------------------------------------------------------- *
 * server_state_get
 * ------------------------------------------------------------------------- */

static
unsigned
server_state_get(void)
{
  return ((server_state_fake &  server_state_mask) |
          (server_state_real & ~server_state_mask));
}

/* ------------------------------------------------------------------------- *
 * server_state_clr
 * ------------------------------------------------------------------------- */

static
void
server_state_clr(unsigned clr)
{
  unsigned prev = server_state_get();

  server_state_real &= ~clr;
  server_state_prev = server_state_get();

  unsigned curr = server_state_get();

  if( prev != curr )
  {
    log_debug("clr state: %08x -> %08x\n", prev, curr);
  }
}

/* ------------------------------------------------------------------------- *
 * server_state_set
 * ------------------------------------------------------------------------- */

static
void
server_state_set(unsigned clr, unsigned set)
{
  server_state_real &= ~clr;
  server_state_real |=  set;

  unsigned temp = server_state_get();
  if( server_state_prev != temp )
  {
    log_debug("set state: %08x -> %08x\n", server_state_prev, temp);
    server_state_prev = temp;
    server_rethink_request(0);
  }
}

/* ========================================================================= *
 * Server Queue State
 * ========================================================================= */

typedef struct
{
  int    qs_alarms;     // triggered alarm dialogs
  time_t qs_desktop;    // nearest boot to desktop alarm
  time_t qs_actdead;    // nearest boot to acting dead alarm
  time_t qs_no_boot;    // nearest non booting alarm

} server_queuestate_t;

static server_queuestate_t server_queuestate_curr;
static server_queuestate_t server_queuestate_prev;

/* ------------------------------------------------------------------------- *
 * server_queuestate_reset
 * ------------------------------------------------------------------------- */

static
void
server_queuestate_reset(void)
{
  // make sure all gaps are filled -> allows binary comparison
  memset(&server_queuestate_curr, 0, sizeof server_queuestate_curr);

  server_queuestate_curr.qs_alarms  = 0;
  server_queuestate_curr.qs_desktop = INT_MAX;
  server_queuestate_curr.qs_actdead = INT_MAX;
  server_queuestate_curr.qs_no_boot = INT_MAX;

  server_icons_curr = 0;
}

/* ------------------------------------------------------------------------- *
 * server_queuestate_invalidate
 * ------------------------------------------------------------------------- */

static
void
server_queuestate_invalidate(void)
{
  // set previous number of alarms to impossible
  // value -> force emitting queuestatus change signal
  server_queuestate_prev.qs_alarms  = -1;
}

/* ------------------------------------------------------------------------- *
 * server_queuestate_init
 * ------------------------------------------------------------------------- */

static
void
server_queuestate_init(void)
{
  server_queuestate_reset();

  /* - - - - - - - - - - - - - - - - - - - *
   * reset queue state, make sure first
   * check yields difference
   * - - - - - - - - - - - - - - - - - - - */

  server_queuestate_prev = server_queuestate_curr;
  server_queuestate_invalidate();
}

/* ------------------------------------------------------------------------- *
 * server_queuestate_sync
 * ------------------------------------------------------------------------- */

static
void
server_queuestate_sync(void)
{
  if( server_state_get() & SF_STATUSBAR_UP )
  {
    if( server_icons_prev != server_icons_curr )
    {
      if( server_icons_curr > 0 )
      {
        if( server_icons_prev <= 0 )
        {
          ipc_statusbar_alarm_show(server_session_bus);
        }
      }
      else
      {
        ipc_statusbar_alarm_hide(server_session_bus);
      }
      server_icons_prev = server_icons_curr;
    }
  }

  if( memcmp(&server_queuestate_prev, &server_queuestate_curr,
             sizeof server_queuestate_curr) )
  {
    time_t t = ticker_get_time();

    auto const char *timeto(char *s, size_t n, time_t d);
    auto const char *timeto(char *s, size_t n, time_t d)
    {
      if( d < INT_MAX )
      {
        return ticker_secs_format(s,n,t-d);
      }
      return "-Inf";
    }

    char d[32];
    char a[32];
    char n[32];

    log_info("QSTATE - active: %d, desktop: T%s, act dead: T%s, no boot: T%s\n",
             server_queuestate_curr.qs_alarms,
             timeto(d, sizeof d, server_queuestate_curr.qs_desktop),
             timeto(a, sizeof a, server_queuestate_curr.qs_actdead),
             timeto(n, sizeof n, server_queuestate_curr.qs_no_boot));

// QUARANTINE     auto int timeto(time_t d);
// QUARANTINE     auto int timeto(time_t d)
// QUARANTINE     {
// QUARANTINE       return (d < t) ? 0 : (d < INT_MAX) ? (int)(d-t) : 9999;
// QUARANTINE     }
// QUARANTINE
// QUARANTINE     log_debug("QSTATE: active=%d, desktop=%d, act dead=%d, no boot=%d\n",
// QUARANTINE               server_queuestate_curr.qs_alarms,
// QUARANTINE               timeto(server_queuestate_curr.qs_desktop),
// QUARANTINE               timeto(server_queuestate_curr.qs_actdead),
// QUARANTINE               timeto(server_queuestate_curr.qs_no_boot));

    // FIXME: find proper place for this
    {
      dbus_int32_t c = server_queuestate_curr.qs_alarms;
      dbus_int32_t d = server_queuestate_curr.qs_desktop;
      dbus_int32_t a = server_queuestate_curr.qs_actdead;
      dbus_int32_t n = server_queuestate_curr.qs_no_boot;

      /* - - - - - - - - - - - - - - - - - - - *
       * send signal to session bus
       * - - - - - - - - - - - - - - - - - - - */

      dbusif_signal_send(server_session_bus,
                         ALARMD_PATH,
                         ALARMD_INTERFACE,
                         ALARMD_QUEUE_STATUS_IND,
                         DBUS_TYPE_INT32, &c,
                         DBUS_TYPE_INT32, &d,
                         DBUS_TYPE_INT32, &a,
                         DBUS_TYPE_INT32, &n,
                         DBUS_TYPE_INVALID);

      /* - - - - - - - - - - - - - - - - - - - *
       * send also to system bus so that dsme
       * can keep off the session bus
       * - - - - - - - - - - - - - - - - - - - */

      dbusif_signal_send(server_system_bus,
                         ALARMD_PATH,
                         ALARMD_INTERFACE,
                         ALARMD_QUEUE_STATUS_IND,
                         DBUS_TYPE_INT32, &c,
                         DBUS_TYPE_INT32, &d,
                         DBUS_TYPE_INT32, &a,
                         DBUS_TYPE_INT32, &n,
                         DBUS_TYPE_INVALID);

    }
    server_queuestate_prev = server_queuestate_curr;
  }
}

/* ========================================================================= *
 * Limbo State Control
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_limbo_set_timeout
 * ------------------------------------------------------------------------- */

void server_limbo_set_timeout(int secs)
{
  server_limbo_timeout_len = secs;
}
/* ------------------------------------------------------------------------- *
 * server_limbo_set_control
 * ------------------------------------------------------------------------- */

void server_limbo_set_control(int mode)
{
  if( mode < 0 || mode >= DESKTOP_WAIT_NUMOF )
  {
    mode = DESKTOP_WAIT_DISABLED;
  }
  server_limbo_wait_state = mode;
}

/* ------------------------------------------------------------------------- *
 * server_limbo_disable
 * ------------------------------------------------------------------------- */

static void server_limbo_disable(const char *reason)
{
  // cancel delayed disabling
  if( server_limbo_timeout_id != 0 )
  {
    g_source_remove(server_limbo_timeout_id);
    server_limbo_timeout_id = 0;
    log_debug("cancelling delayed LIMBO disable\n");
  }

  // disable limbo
  if( !(server_state_get() & SF_DESKTOP_UP) )
  {
    log_info("%s -> LIMBO disabled\n", reason);
    server_state_set(0, SF_DESKTOP_UP);
  }
}

/* ------------------------------------------------------------------------- *
 * server_limbo_disable_cb
 * ------------------------------------------------------------------------- */

static
gboolean
server_limbo_disable_cb(gpointer data)
{
  server_limbo_timeout_id = 0;

  log_info("================ DESKTOP READY TIMEOUT ================\n");
  server_limbo_disable("desktop ready timeout");

  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_limbo_disable_delayed
 * ------------------------------------------------------------------------- */

static void server_limbo_disable_delayed(const char *reason)
{
  if( !(server_state_get() & SF_DESKTOP_UP) )
  {
    if( server_limbo_timeout_len > 0 && server_limbo_timeout_id == 0 )
    {
      log_info("%s -> LIMBO timeout = %d seconds\n", reason,
                server_limbo_timeout_len);
      server_limbo_timeout_id = g_timeout_add_seconds(server_limbo_timeout_len,
                                                      server_limbo_disable_cb,
                                                      0);
    }
  }
}

/* ========================================================================= *
 * Server Time State
 * ========================================================================= */

static char server_tz_prev[128];  // current and previous timezone
static char server_tz_curr[128];

static int  server_dst_prev = -1;
static int  server_dst_curr = -1;

/* ------------------------------------------------------------------------- *
 * server_timestate_read
 * ------------------------------------------------------------------------- */

static
void
server_timestate_read(void)
{
  struct tm tm;
  ticker_get_timezone(server_tz_curr, sizeof server_tz_curr);
  ticker_get_local(&tm);
  server_dst_curr = tm.tm_isdst;
}

/* ------------------------------------------------------------------------- *
 * server_timestate_sync
 * ------------------------------------------------------------------------- */

static
void
server_timestate_sync(void)
{
  strcpy(server_tz_prev, server_tz_curr);
  server_dst_prev = server_dst_curr;
}

/* ------------------------------------------------------------------------- *
 * server_timestate_changed
 * ------------------------------------------------------------------------- */

static
int
server_timestate_changed(void)
{
  return (strcmp(server_tz_prev, server_tz_curr) ||
          server_dst_prev != server_dst_curr);
}

/* ------------------------------------------------------------------------- *
 * server_timestate_init
 * ------------------------------------------------------------------------- */

static
void
server_timestate_init(void)
{
  ticker_get_synced();
  server_timestate_read();
  server_timestate_sync();

  log_info("timezone: '%s'\n", server_tz_curr);
}

/* ========================================================================= *
 * Server State
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_state_init
 * ------------------------------------------------------------------------- */

static
void
server_state_init(void)
{
  /* - - - - - - - - - - - - - - - - - - - *
   * starting up ...
   * - - - - - - - - - - - - - - - - - - - */

  server_state_real |= SF_STARTUP;

  /* - - - - - - - - - - - - - - - - - - - *
   * make synchronization happen
   * - - - - - - - - - - - - - - - - - - - */

  server_state_real |= SF_SYSTEMUI_DN;
  server_state_real |= SF_CLOCKD_DN;
  server_state_real |= SF_DSME_DN;
  server_state_real |= SF_MCE_DN;
  server_state_real |= SF_STATUSBAR_DN;

  server_icons_prev  = -1;

  /* - - - - - - - - - - - - - - - - - - - *
   * check existence of peer services
   * - - - - - - - - - - - - - - - - - - - */

  if( dbusif_check_name_owner(server_system_bus, SYSTEMUI_SERVICE) == 0 )
  {
    server_state_real |= SF_SYSTEMUI_UP;
  }

  if( dbusif_check_name_owner(server_system_bus, CLOCKD_SERVICE) == 0 )
  {
    server_state_real |= SF_CLOCKD_UP;
    ticker_use_libtime(TRUE);
  }
  else
  {
    ticker_use_libtime(FALSE);
  }
  server_timestate_init();

  if( dbusif_check_name_owner(server_system_bus, DSME_SERVICE) == 0 )
  {
    server_state_real |= SF_DSME_UP;
  }
  if( dbusif_check_name_owner(server_system_bus, MCE_SERVICE) == 0 )
  {
    server_state_real |= SF_MCE_UP;
  }

  if( dbusif_check_name_owner(server_session_bus, STATUSAREA_CLOCK_SERVICE) == 0 )
  {
    server_state_real |= SF_STATUSBAR_UP;
  }

  if( server_limbo_wait_state == DESKTOP_WAIT_DISABLED )
  {
    log_info("desktop wait disabled -> LIMBO disabled\n");
    server_state_real |= SF_DESKTOP_UP;
  }

  if( dbusif_check_name_owner(server_session_bus, HOME_SERVICE) == 0 )
  {
    log_info("home ready detected -> LIMBO disabled\n");
    server_state_real |= SF_DESKTOP_UP;
  }

  if( access("/tmp/ACT_DEAD", F_OK) == 0 )
  {
    log_info("not in USER state -> LIMBO enabled\n");
    server_state_real |= SF_ACT_DEAD;
  }

  server_state_clr(0);
}

/* ------------------------------------------------------------------------- *
 * server_state_sync
 * ------------------------------------------------------------------------- */

static
void
server_state_sync(void)
{
  unsigned flgs = server_state_get();

  unsigned mask;

  /* - - - - - - - - - - - - - - - - - - - *
   * clockd
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_CLOCKD_DN | SF_CLOCKD_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_CLOCKD_DN);

    // sync with emergent clockd
    ticker_get_synced();

    server_timestate_read();
    if( server_timestate_changed() )
    {
      server_state_set(0, SF_TZ_CHANGED);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * dsme
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_DSME_DN | SF_DSME_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_DSME_DN);

    // make sure status broadcast is made
    server_queuestate_invalidate();
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * mce
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_MCE_DN | SF_MCE_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_MCE_DN);

    // make sure status broadcast is made
    server_queuestate_invalidate();
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * statusbar
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_STATUSBAR_DN | SF_STATUSBAR_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_STATUSBAR_DN);

    // make sure status broadcast is made
    server_icons_prev = -1;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * systemui
   * - - - - - - - - - - - - - - - - - - - */

  mask = SF_SYSTEMUI_DN | SF_SYSTEMUI_UP;
  if( (flgs & mask) == mask )
  {
    server_state_clr(SF_SYSTEMUI_DN);

    // systemui related actions handled directly
    // in alarm event state transition logic
  }

  // done
  server_state_prev = server_state_get();
}

/* ------------------------------------------------------------------------- *
 * server_state_get_systemui_service
 * ------------------------------------------------------------------------- */

static
const char *
server_state_get_systemui_service(void)
{
  unsigned flags = server_state_get();

  if( flags & SF_SYSTEMUI_UP )
  {
    return SYSTEMUI_SERVICE;
  }
  return NULL;
}

/* ========================================================================= *
 * Alarm Action Functionality
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_action_get_type  --  get action type mask
 * ------------------------------------------------------------------------- */

static
unsigned
server_action_get_type(alarm_action_t *action)
{
  return action ? (action->flags & ALARM_ACTION_TYPE_MASK) : 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_get_when  -- get action when mask
 * ------------------------------------------------------------------------- */

static
unsigned
server_action_get_when(alarm_action_t *action)
{
  return action ? (action->flags & ALARM_ACTION_WHEN_MASK) : 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_snooze  --  execute snooze action
 * ------------------------------------------------------------------------- */

static
int
server_action_do_snooze(alarm_event_t *event, alarm_action_t *action)
{
  queue_event_set_state(event, ALARM_STATE_SNOOZED);
  return 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_disable  --  execute disable action
 * ------------------------------------------------------------------------- */

static
int
server_action_do_disable(alarm_event_t *event, alarm_action_t *action)
{
  /* The event comes invisible to alarmd, no state transition
   * required. The client application needs to:
   * - delete the event via alarmd_event_del()
   * - re-enable the event via alarmd_event_update()
   *
   * In the latter case the event will re-enter the
   * state machine via ALARM_STATE_NEW state.
   */
  log_info("DISABLING DUE TO ACTION: cookie=%d\n", (int)event->ALARMD_PRIVATE(cookie));
  event->flags |= ALARM_EVENT_DISABLED;
  return 0;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_desktop  --  execute boot to desktop action
 * ------------------------------------------------------------------------- */

#if ALARMD_ACTION_BOOTFLAGS
static
int
server_action_do_desktop(alarm_event_t *event, alarm_action_t *action)
{
#ifdef DEAD_CODE
  ipc_dsme_req_powerup(server_system_bus);
#endif
  return 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * server_action_do_actdead  --  execute boot to acting dead action
 * ------------------------------------------------------------------------- */

#if ALARMD_ACTION_BOOTFLAGS
static
int
server_action_do_actdead(alarm_event_t *event, alarm_action_t *action)
{
  /* nothing to do */
  return 0;
}
#endif

/* ------------------------------------------------------------------------- *
 * server_action_do_exec  --  execute run command line action
 * ------------------------------------------------------------------------- */

static
int
server_action_do_exec(alarm_event_t *event, alarm_action_t *action)
{
  static const char tag[] = "[COOKIE]";

  int         err = -1;
  const char *cmd = action->exec_command;
  const char *hit = 0;
  char       *tmp = 0;

  if( xisempty(cmd) )
  {
    goto cleanup;
  }

  if( action->flags & ALARM_ACTION_EXEC_ADD_COOKIE )
  {
    if( (hit = strstr(cmd, tag)) != 0 )
    {
      asprintf(&tmp, "%.*s%d%s",
               (int)(hit-cmd), cmd,
               (int)event->ALARMD_PRIVATE(cookie),
               hit + sizeof tag - 1);
    }
    else
    {
      asprintf(&tmp, "%s %d", cmd, (int)event->ALARMD_PRIVATE(cookie));
    }
    cmd = tmp;
  }

  if( xisempty(cmd) )
  {
    goto cleanup;
  }

  log_info("EXEC: %s\n", cmd);
  ipc_exec_run_command(cmd);
  err = 0;

  cleanup:

  free(tmp);
  return err;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_dbus  --  execute send dbus message action
 * ------------------------------------------------------------------------- */

static
int
server_action_do_dbus(alarm_event_t *event, alarm_action_t *action)
{
  int          err = -1;
  DBusMessage *msg = 0;

  log_info("DBUS: service   = '%s'\n", action->dbus_service);
  log_info("DBUS: path      = '%s'\n", action->dbus_path);
  log_info("DBUS: interface = '%s'\n", action->dbus_interface);
  log_info("DBUS: name      = '%s'\n", action->dbus_name);

  if( !xisempty(action->dbus_service) )
  {
    log_info("DBUS: is method\n");
    msg = dbus_message_new_method_call(action->dbus_service,
                                       action->dbus_path,
                                       action->dbus_interface,
                                       action->dbus_name);

    if( action->flags & ALARM_ACTION_DBUS_USE_ACTIVATION )
    {
      log_info("DBUS: is auto start\n");
      dbus_message_set_auto_start(msg, TRUE);
    }
    dbus_message_set_no_reply(msg, TRUE);
  }
  else
  {
    log_info("DBUS: is signal\n");
    msg = dbus_message_new_signal(action->dbus_path,
                                  action->dbus_interface,
                                  action->dbus_name);
  }

  if( msg != 0 )
  {
    DBusConnection *conn = server_session_bus;

    if( !xisempty(action->dbus_args) )
    {
      log_info("DBUS: has user args\n");
      serialize_unpack_to_mesg(action->dbus_args, msg);
    }

    if( action->flags & ALARM_ACTION_DBUS_ADD_COOKIE )
    {
      log_info("DBUS: appending cookie %d\n", (int)event->ALARMD_PRIVATE(cookie));

      dbus_int32_t cookie = event->ALARMD_PRIVATE(cookie);
      dbus_message_append_args(msg,
                           DBUS_TYPE_INT32, &cookie,
                           DBUS_TYPE_INVALID);
    }

    if( action->flags & ALARM_ACTION_DBUS_USE_SYSTEMBUS )
    {
      log_info("DBUS: using system bus\n");
      conn = server_system_bus;
    }

    if( conn != 0 && dbus_connection_send(conn, msg, 0) )
    {
      log_info("DBUS: send ok\n");
      err = 0;
    }
  }

  log_info("DBUS: error=%d\n", err);

  if( msg != 0 )
  {
    dbus_message_unref(msg);
  }
  return err;
}

/* ------------------------------------------------------------------------- *
 * server_action_do_all  --  execute all configured actions
 * ------------------------------------------------------------------------- */

static
void
server_action_do_all(alarm_event_t *event, alarm_action_t *action)
{
  if( event && action )
  {
    unsigned flags = server_action_get_type(action);

    if( flags & ALARM_ACTION_TYPE_SNOOZE )
    {
      log_info("ACTION: SNOOZE\n");
      server_action_do_snooze(event, action);
    }
    if( flags & ALARM_ACTION_TYPE_DISABLE )
    {
      log_info("ACTION: DISABLE\n");
      server_action_do_disable(event, action);
    }
    if( flags & ALARM_ACTION_TYPE_DBUS )
    {
      log_info("ACTION: DBUS\n");
      server_action_do_dbus(event, action);
    }
    if( flags & ALARM_ACTION_TYPE_EXEC )
    {
      log_info("ACTION: EXEC: %s\n", action->exec_command);
      server_action_do_exec(event, action);
    }
#if ALARMD_ACTION_BOOTFLAGS
    if( flags & ALARM_ACTION_TYPE_DESKTOP )
    {
      log_info("ACTION: DESKTOP\n");
      server_action_do_desktop(event, action);
    }
    if( flags & ALARM_ACTION_TYPE_ACTDEAD )
    {
      log_info("ACTION: ACTDEAD\n");
      server_action_do_actdead(event, action);
    }
#endif
  }
}

/* ========================================================================= *
 * Server Event Handling Functions
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_event_do_state_actions  --  execute actions that match given when mask
 * ------------------------------------------------------------------------- */

static
void
server_event_do_state_actions(alarm_event_t *eve, unsigned when)
{
  if( eve && when )
  {
    for( int i = 0; i < eve->action_cnt; ++i )
    {
      alarm_action_t *act = &eve->action_tab[i];
      if( server_action_get_when(act) & when )
      {
        server_action_do_all(eve, act);
      }
    }
  }
}

/* ------------------------------------------------------------------------- *
 * server_event_do_response_actions  --  execute actions for user response
 * ------------------------------------------------------------------------- */

static
void
server_event_do_response_actions(alarm_event_t *eve)
{
  if( 0 <= eve->response && eve->response < eve->action_cnt )
  {
    alarm_action_t *act = &eve->action_tab[eve->response];

    if( server_action_get_when(act) & ALARM_ACTION_WHEN_RESPONDED )
    {
      server_action_do_all(eve, act);
    }
  }
}

/* ------------------------------------------------------------------------- *
 * server_event_get_tz  --  return timezone from event or current default
 * ------------------------------------------------------------------------- */

static
const char *
server_event_get_tz(alarm_event_t *self)
{
  if( !xisempty(self->alarm_tz) )
  {
    return self->alarm_tz;
  }
  return server_tz_curr;
}

/* ------------------------------------------------------------------------- *
 * server_event_get_boot_mask
 * ------------------------------------------------------------------------- */

static
unsigned
server_event_get_boot_mask(alarm_event_t *eve)
{
  unsigned acc = eve->flags & (ALARM_EVENT_ACTDEAD | ALARM_EVENT_BOOT);

#if ALARMD_ACTION_BOOTFLAGS
  for( int i = 0; i < eve->action_cnt; ++i )
  {
    alarm_action_t *act = &eve->action_tab[i];

    if( act->flags & ALARM_ACTION_TYPE_DESKTOP )
    {
      acc |= ALARM_EVENT_BOOT;
    }
    if( act->flags & ALARM_ACTION_TYPE_ACTDEAD )
    {
      acc |= ALARM_EVENT_ACTDEAD;
    }
  }
#endif

  return acc;
}

/* ------------------------------------------------------------------------- *
 * server_event_get_snooze
 * ------------------------------------------------------------------------- */

static
time_t
server_event_get_snooze(alarm_event_t *eve)
{
  time_t snooze = eve->snooze_secs;

  if( snooze <= 0 )
  {
    snooze = queue_get_snooze();
  }
  if( snooze < 10 )
  {
    snooze = 10;
  }
  return snooze;
}

/* ------------------------------------------------------------------------- *
 * server_event_is_snoozed
 * ------------------------------------------------------------------------- */

static
int
server_event_is_snoozed(alarm_event_t *eve)
{
  return (eve->snooze_total != 0);
}

/* ------------------------------------------------------------------------- *
 * server_event_get_buttons
 * ------------------------------------------------------------------------- */

static
int
server_event_get_buttons(alarm_event_t *self, int *vec, int size)
{
  int cnt = 0;

  for( int i = 0; i < self->action_cnt; ++i )
  {
    alarm_action_t *act = &self->action_tab[i];

    if( !xisempty(act->label) && (act->flags & ALARM_ACTION_WHEN_RESPONDED) )
    {
      if( cnt < size )
      {
        vec[cnt] = i;
      }
      ++cnt;
    }
  }
  return cnt;
}

/* ------------------------------------------------------------------------- *
 * server_event_get_next_trigger
 * ------------------------------------------------------------------------- */

static
time_t
server_event_get_next_trigger(time_t t0, time_t t1, alarm_event_t *self)
{
  const char *tz = server_event_get_tz(self);

  /* If absolute trigger time is not given, we need
   * to evaluate it from broken down time */
  if( t1 <= 0 )
  {
    if( ticker_tm_is_uninitialized(&self->alarm_tm) )
    {
      /* Broken down time was not specified
       * either -> user current time */
      t1 = t0;
    }
    else
    {
      // assume fully qualified time structure
      int           fqt = 1;
      struct tm     tpl = self->alarm_tm;
      alarm_recur_t rec;

      // build recurrency mask from broken down tim
      alarm_recur_ctor(&rec);
      if( tpl.tm_min  < 0 ) fqt=0; else rec.mask_min  = 1ull << tpl.tm_min;
      if( tpl.tm_hour < 0 ) fqt=0; else rec.mask_hour = 1u   << tpl.tm_hour;

      if( tpl.tm_wday < 0 ) {}     else rec.mask_wday = 1u   << tpl.tm_wday;
      if( tpl.tm_mday < 0 ) fqt=0; else rec.mask_mday = 1u   << tpl.tm_mday;
      if( tpl.tm_mon  < 0 ) fqt=0; else rec.mask_mon  = 1u   << tpl.tm_mon;
      if( tpl.tm_year < 0 ) fqt=0;

      if( fqt )
      {
        // all necessary fields were filled - adjust to
        // full minutes and evaluate directly
        tpl.tm_min += (tpl.tm_sec > 0);
        tpl.tm_sec = 0;
        t1 = ticker_mktime(&tpl, tz);
      }
      else
      {
        // align to next occurence of given values
        ticker_break_tm(t0, &tpl, tz);
        t1 = alarm_recur_align(&rec, &tpl, tz);
      }

      alarm_recur_dtor(&rec);
    }
  }

  /* for recurring events with recurrence masks, the
   * above calculated trigger time is low bound and
   * we need to get to the next unmasked point in time */
  if( alarm_event_is_recurring(self) )
  {
    if( self->recur_secs > 0 )
    {
      // simple recurrence
      if( t1 <= t0 )
      {
        t1 = server_bump_time(t1, self->recur_secs, t0);
      }
    }
    else
    {
      // recurrence masks
      time_t next = INT_MAX;
      struct tm now;

      if( t1 < t0 )
      {
        t1 = t0;
      }

      ticker_break_tm(t1, &now, tz);

      for( size_t i = 0; i < self->recurrence_cnt; ++i )
      {
        alarm_recur_t *rec = &self->recurrence_tab[i];
        struct tm tmp = now;
        time_t    trg = alarm_recur_align(rec, &tmp, tz);

        if( t1 < trg && trg < next )
        {
          next = trg;
        }
      }

      if( t1 < next && next < INT_MAX )
      {
        t1 = next;
      }
    }
  }
  if( t1 < t0 )
  {
    log_debug("next trigger is in past: T%+ld\n", (long)(t0-t1));
  }

  return t1;
}

/* ------------------------------------------------------------------------- *
 * server_event_evaluate_initial_trigger
 * ------------------------------------------------------------------------- */

static
int
server_event_evaluate_initial_trigger(alarm_event_t *self)
{
  time_t t0 = ticker_get_time();
  time_t t1 = server_event_get_next_trigger(t0, self->alarm_time, self);

  log_debug("[%ld] INITIAL TRIGGER: T%s, %s\n",
            (long)alarm_event_get_cookie(self),
            ticker_secs_format(0,0,t0-t1),
            ticker_date_format_long(0,0,t1));

  return t1;
}

/* ------------------------------------------------------------------------- *
 * configuration constants for server_clock_source_is_stable()
 * ------------------------------------------------------------------------- */

enum
{
  CLK_JITTER  = 2, /* If alarmd notices that system time vs. monotonic
                    * time changes more than CLK_JITTER seconds ... */

  CLK_STABLE  = 2, /* ... it will delay alarm event state transition
                    * evaluation for CLK_STABLE seconds. */

  CLK_RESCHED = 5, /* If detected change is larger than CLK_RESCHED secs,
                    * the trigger times for certain alarms will be
                    * re-evaluated */
};

static time_t delta_back = 0;
static time_t delta_forw = 0;

/* ------------------------------------------------------------------------- *
 * server_clock_back_delta  -- get and clear accumulated clock change back
 * ------------------------------------------------------------------------- */

static time_t server_clock_back_delta(void)
{
  time_t t = delta_back; delta_back = 0; return t;
}

/* ------------------------------------------------------------------------- *
 * server_clock_forw_delta  --  get and clear accumulated clock change forw
 * ------------------------------------------------------------------------- */

static time_t server_clock_forw_delta(void)
{
  time_t t = delta_forw; delta_forw = 0; return t;
}

/* ------------------------------------------------------------------------- *
 * server_clock_source_is_stable
 * ------------------------------------------------------------------------- */

static
gboolean
server_clock_source_is_stable(void)
{
  static int    initialized = 0;
  static time_t delta_sched = 0;
  static time_t delta_clock = 0;
  static time_t delay_until = 0;

  time_t real_time  = ticker_get_time();
  time_t mono_time  = ticker_get_monotonic();
  time_t delta      = real_time - mono_time;
  time_t delta_diff = 0;
  int    detected   = 0;

  if( !initialized )
  {
    log_info("clock source initialized\n");
    initialized = 1;
    delta_clock = delta;
    delta_sched = delta;
    delay_until = mono_time;
  }

  if( server_state_get() & SF_CLK_CHANGED )
  {
    log_info("clock source changed\n");
    server_state_clr(SF_CLK_CHANGED);
    detected = 1;
  }

  delta_diff = delta - delta_clock;

  if( delta_diff < -CLK_JITTER || +CLK_JITTER < delta_diff )
  {
    log_info("clock change: %+ld seconds\n", (long)delta_diff);
    detected = 1;
  }

  if( detected != 0 )
  {
    delta_clock = delta;
    delay_until = mono_time + CLK_STABLE;
  }

  if( mono_time < delay_until )
  {
    log_info("waiting clock source to stabilize ...\n");
    return FALSE;
  }

  delta_diff = delta_clock - delta_sched;

  if( delta_diff > +CLK_RESCHED )
  {
    log_info("overall change: %+ld seconds\n", (long)delta_diff);
    delta_sched = delta_clock;
    delta_forw += delta_diff;
    server_state_set(0, SF_CLK_MV_FORW);
  }
  else if( delta_diff < -CLK_RESCHED )
  {
    log_info("overall change: %+ld seconds\n", (long)delta_diff);
    delta_sched = delta_clock;
    delta_back += delta_diff;
    server_state_set(0, SF_CLK_MV_BACK);
  }

  return TRUE;
}

/* ========================================================================= *
 * Server Wakeup for alarm
 * ========================================================================= */

static time_t server_wakeup_time = INT_MAX;
static guint  server_wakeup_id   = 0;

/* ------------------------------------------------------------------------- *
 * server_wakeup_start_cb
 * ------------------------------------------------------------------------- */

static
gboolean
server_wakeup_start_cb(gpointer data)
{
  server_wakeup_time = INT_MAX;
  server_wakeup_id   = 0;
  server_rethink_request(0);
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_cancel_wakeup
 * ------------------------------------------------------------------------- */

static
void
server_cancel_wakeup(void)
{
  if( server_wakeup_id != 0 )
  {
    g_source_remove(server_wakeup_id);
    server_wakeup_id = 0;
  }
  server_wakeup_time = INT_MAX;
}

/* ------------------------------------------------------------------------- *
 * server_request_wakeup
 * ------------------------------------------------------------------------- */

static
void
server_request_wakeup(time_t tmo)
{
  time_t now = ticker_get_time();
  time_t top = now + 14 * 24 * 60 * 60; // two weeks ahead

  {
    struct tm tm; char tmp[128];
    const char *tz = server_tz_curr;
    ticker_break_tm(tmo, &tm, tz);
    server_repr_tm(&tm, tz, tmp, sizeof tmp);
    log_debug("REQ WAKEUP: %s\n", tmp);
  }

  if( tmo > top )
  {
    tmo = top;
  }

  if( server_wakeup_time > tmo )
  {
    if( server_wakeup_id != 0 )
    {
      g_source_remove(server_wakeup_id);
      server_wakeup_id = 0;
    }

    server_wakeup_time = tmo;

    {
      const char *tz = server_tz_curr;
      struct tm tm; char tmp[128];
      ticker_break_tm(tmo, &tm, tz);
      server_repr_tm(&tm, tz, tmp, sizeof tmp);
      log_debug("SET WAKEUP: %s (%+ld)\n", tmp, (long)(tmo-now));
    }

    if( tmo <= now )
    {
      server_wakeup_id = g_idle_add(server_wakeup_start_cb, 0);
    }
    else
    {
      tmo -= now;

      //TODO: fix when g_timeout_add_seconds() is available
      server_wakeup_id = g_timeout_add_seconds(tmo, server_wakeup_start_cb, 0);
      //server_wakeup_id = g_timeout_add(tmo*1000, server_wakeup_start_cb, 0);
    }
  }
}

/* ========================================================================= *
 * Rething states of queued events
 * ========================================================================= */

static int    server_rethink_req = 0;
static guint  server_rethink_id  = 0;

static time_t server_rethink_time = 0;

/* ------------------------------------------------------------------------- *
 * time_filt  --  utility for scanning lowest time_t value
 * ------------------------------------------------------------------------- */

static inline void time_filt(time_t *low, time_t add)
{
  if( *low > add ) *low = add;
}

/* ------------------------------------------------------------------------- *
 * server_handle_systemui_ack
 * ------------------------------------------------------------------------- */

static
int
server_handle_systemui_ack(cookie_t *vec, int cnt)
{
  int err = 0;
  log_debug_F("vec=%p, cnt=%d\n", vec, cnt);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);
    log_debug("[%d] ACK: event=%p\n", vec[i], eve);
    if( eve != 0 )
    {
      queue_event_set_state(eve, ALARM_STATE_SYSUI_ACK);
    }
  }
  server_rethink_request(1);

  return err;
}

/* ------------------------------------------------------------------------- *
 * server_systemui_ack_cb
 * ------------------------------------------------------------------------- */

static
void
server_systemui_ack_cb(dbus_int32_t *vec, int cnt)
{
  server_handle_systemui_ack((cookie_t*)vec, cnt);
}

/* ------------------------------------------------------------------------- *
 * server_handle_systemui_rsp
 * ------------------------------------------------------------------------- */

static
int
server_handle_systemui_rsp(cookie_t cookie, int button)
{
  int err = 0;

  alarm_event_t *eve = queue_get_event(cookie);

  log_info("rsp %d -> %p (button=%d)\n", cookie, eve, button);

  if( eve != 0 )
  {
    queue_event_set_state(eve, ALARM_STATE_SYSUI_RSP);
    eve->response = button;
  }

  server_rethink_request(1);

  return err;
}

/* ------------------------------------------------------------------------- *
 * server_rethink_new
 * ------------------------------------------------------------------------- */

static
void
server_rethink_new(void)
{
  int       cnt = 0;
  cookie_t *vec = 0;

  vec = queue_query_by_state(&cnt, ALARM_STATE_NEW);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

#if ENABLE_LOGGING >= 3
    {
      time_t now = server_rethink_time;
      time_t trg = alarm_event_get_trigger(eve);

      log_info("[%ld] NEW: %s (T%s)\n",
               (long)vec[i],
               ticker_date_format_long(0,0,trg),
               ticker_secs_format(0,0,now-trg));

// QUARANTINE       time_t    now = server_rethink_time;
// QUARANTINE       time_t    tot = alarm_event_get_trigger(eve) - eve->alarm_time;
// QUARANTINE       time_t    rec = eve->recur_secs ?: 1;
// QUARANTINE       const char *tz = server_event_get_tz(eve);
// QUARANTINE       struct tm tm;
// QUARANTINE       char trg[128];
// QUARANTINE       ticker_break_tm(alarm_event_get_trigger(eve), &tm, tz);
// QUARANTINE       server_repr_tm(&tm, tz, trg, sizeof trg);
// QUARANTINE
// QUARANTINE       log_debug("NEW: %s at: %+d, %+d %% %d = %+d\n",
// QUARANTINE                 trg,
// QUARANTINE                 (int)alarm_event_get_trigger(eve) - now,
// QUARANTINE                 (int)tot,(int)rec,(int)(tot%rec));
    }
#endif
    // required internet connection not available?
    if( eve->flags & ALARM_EVENT_CONNECTED )
    {
      if( !(server_state_get() & SF_CONNECTED) )
      {
        queue_event_set_state(eve, ALARM_STATE_WAITCONN);
        continue;
      }
    }

    queue_event_set_state(eve, ALARM_STATE_QUEUED);
    server_event_do_state_actions(eve, ALARM_ACTION_WHEN_QUEUED);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_waitconn
 * ------------------------------------------------------------------------- */

static
void
server_rethink_waitconn(void)
{
  if( server_state_get() & SF_CONNECTED )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_WAITCONN);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_event_set_state(eve, ALARM_STATE_NEW);
    }
    free(vec);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_queued
 * ------------------------------------------------------------------------- */

static
void
server_rethink_queued(void)
{
  time_t    now = server_rethink_time;
  time_t    tsw = INT_MAX;
  time_t    thw = INT_MAX;
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_QUEUED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    // required internet connection not available?
    if( eve->flags & ALARM_EVENT_CONNECTED )
    {
      if( !(server_state_get() & SF_CONNECTED) )
      {
        queue_event_set_state(eve, ALARM_STATE_WAITCONN);
        continue;
      }
    }

// QUARANTINE     {
// QUARANTINE       const char *tz = server_event_get_tz(eve);
// QUARANTINE       struct tm tm;
// QUARANTINE       char trg[128];
// QUARANTINE       ticker_break_tm(alarm_event_get_trigger(eve), &tm, tz);
// QUARANTINE       server_repr_tm(&tm, tz, trg, sizeof trg);
// QUARANTINE       log_debug("Q: %s, at %+d\n", trg, (int)(alarm_event_get_trigger(eve) - now));
// QUARANTINE     }

    // in future -> just update wakeup time values
    if( alarm_event_get_trigger(eve) > now )
    {
      unsigned boot = server_event_get_boot_mask(eve);

      if( boot & ALARM_EVENT_BOOT )
      {
        time_filt(&server_queuestate_curr.qs_desktop, alarm_event_get_trigger(eve));
      }
      else if( boot & ALARM_EVENT_ACTDEAD )
      {
        time_filt(&server_queuestate_curr.qs_actdead, alarm_event_get_trigger(eve));
      }
      else
      {
        time_filt(&server_queuestate_curr.qs_no_boot, alarm_event_get_trigger(eve));
      }

      if( eve->flags & ALARM_EVENT_SHOW_ICON )
      {
        server_icons_curr += 1;
      }
      continue;
    }

    // missed for some reason, power off for example
    if( (now - alarm_event_get_trigger(eve)) > SERVER_MISSED_LIMIT )
    {
      queue_event_set_state(eve, ALARM_STATE_MISSED);
      continue;
    }

    // trigger it
    queue_event_set_state(eve, ALARM_STATE_LIMBO);
  }

  // determinetimeout values
  time_filt(&thw, server_queuestate_curr.qs_desktop);
  time_filt(&thw, server_queuestate_curr.qs_actdead);

  time_filt(&tsw, thw);
  time_filt(&tsw, server_queuestate_curr.qs_no_boot);

  // set software timeout
  if( tsw < INT_MAX )
  {
    server_request_wakeup(tsw);
  }

  // set hardware timeout
  if( thw < INT_MAX )
  {
    struct tm tm;

    time_t lim = now + ALARM_INTERRUPT_LIMIT_SECS;
    time_t trg = thw - POWERUP_COMPENSATION_SECS;

    if( trg < lim ) trg = lim;
    gmtime_r(&trg, &tm);
    hwrtc_set_alarm(&tm, 1);
  }

  // cleanup
  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_missed
 * ------------------------------------------------------------------------- */

static
void
server_rethink_missed(void)
{
  int       cnt = 0;
  cookie_t *vec = 0;

  vec = queue_query_by_state(&cnt, ALARM_STATE_MISSED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    log_debug("[%ld] MISSED\n", (long)vec[i]);

    /* - - - - - - - - - - - - - - - - - - - *
     * handle actions bound to missed alarms
     * - - - - - - - - - - - - - - - - - - - */

    server_event_do_state_actions(eve, ALARM_ACTION_WHEN_DELAYED);

    /* - - - - - - - - - - - - - - - - - - - *
     * next state for missed alarms depends
     * on event configuration bits
     * - - - - - - - - - - - - - - - - - - - */

    if( eve->flags & ALARM_EVENT_RUN_DELAYED )
    {
      queue_event_set_state(eve, ALARM_STATE_LIMBO);
      continue;
    }
    if( eve->flags & ALARM_EVENT_POSTPONE_DELAYED )
    {
      queue_event_set_state(eve, ALARM_STATE_POSTPONED);
      continue;
    }
    if( eve->flags & ALARM_EVENT_DISABLE_DELAYED )
    {
      log_info("DISABLING MISSED ALARM: cookie=%d\n", (int)eve->ALARMD_PRIVATE(cookie));
      // the event will come invisible to alarmd state machine
      // once the ALARM_EVENT_DISABLED flag is set
      // -> no state transfer required
      eve->flags |= ALARM_EVENT_DISABLED;
      server_event_do_state_actions(eve, ALARM_ACTION_WHEN_DISABLED);
      continue;
    }
    //queue_event_set_state(eve, ALARM_STATE_DELETED);
    queue_event_set_state(eve, ALARM_STATE_SERVED);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_postponed
 * ------------------------------------------------------------------------- */

static
void
server_rethink_postponed(void)
{
  time_t    day = 24 * 60 * 60;
  time_t    now = server_rethink_time;
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_POSTPONED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    log_debug("[%ld] POSTPONED\n", (long)vec[i]);

    time_t snooze = server_event_get_snooze(eve);

    time_t trg = alarm_event_get_trigger(eve);

    /* trigger anyway if less than one day late */
    if( now - trg - snooze < day )
    {
      queue_event_set_state(eve, ALARM_STATE_LIMBO);
      continue;
    }

    /* otherwise autosnooze by one day */
    time_t add = now + day - alarm_event_get_trigger(eve);
    time_t pad = add % snooze;

    if( pad != 0 )
    {
      add = add - pad + snooze;
    }

    trg += add;

    queue_event_set_trigger(eve, trg);
    queue_event_set_state(eve, ALARM_STATE_NEW);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_limbo
 * ------------------------------------------------------------------------- */

static
void
server_rethink_limbo(void)
{
  int       cnt = 0;
  cookie_t *vec = 0;
  unsigned current_server_state = 0;
  unsigned is_user_mode = 0;

  current_server_state = server_state_get();

  if( current_server_state & SF_DESKTOP_UP )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * We are in user mode and desktop has
     * been started up, all events will pass
     * through from the limbo
     * OR we are in acting dead mode and init was done
     * - - - - - - - - - - - - - - - - - - - */

    is_user_mode = !(current_server_state & SF_ACT_DEAD);
    vec = queue_query_by_state(&cnt, ALARM_STATE_LIMBO);

    for( int i = 0; i < cnt; ++i )
    {
    /* - - - - - - - - - - - - - - - - - - - *
     * if we are in acting dead, only alarms
     * with ALARM_EVENT_ACTDEAD will pass
     * through the limbo
     * Otherwise all alarms will be triggered
     * - - - - - - - - - - - - - - - - - - - */

      alarm_event_t *eve = queue_get_event(vec[i]);
      if(is_user_mode  || eve->flags & ALARM_EVENT_ACTDEAD )
      {
        queue_event_set_state(eve, ALARM_STATE_TRIGGERED);
      }
    }
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_triggered
 * ------------------------------------------------------------------------- */

static
void
server_rethink_triggered(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_TRIGGERED);
  int       req = 0;

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    server_event_do_state_actions(eve, ALARM_ACTION_WHEN_TRIGGERED);

    {
      struct tm tm;
      char now[128];
      char trg[128];
      const char *tz = server_event_get_tz(eve);

      ticker_break_tm(server_rethink_time, &tm, tz);
      server_repr_tm(&tm, tz, now, sizeof now);

      ticker_break_tm(alarm_event_get_trigger(eve), &tm, tz);
      server_repr_tm(&tm, tz, trg, sizeof trg);

      log_info("[%ld] TRIGGERED: %s -- %s\n", (long)vec[i], now, trg);
    }

    if( server_event_get_buttons(eve, 0,0) )
    {
      // transfer control to system ui
      vec[req++] = eve->ALARMD_PRIVATE(cookie);
      queue_event_set_state(eve, ALARM_STATE_WAITSYSUI);
    }
    else if( queue_event_get_state(eve) == ALARM_STATE_SNOOZED )
    {
      // if snoozed, stay snoozed
    }
    else
    {
      // otherwise mark served
      queue_event_set_state(eve, ALARM_STATE_SERVED);
    }
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_waitsysui
 * ------------------------------------------------------------------------- */

static
void
server_rethink_waitsysui(void)
{
  if( server_state_get_systemui_service() )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_WAITSYSUI);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_event_set_state(eve, ALARM_STATE_SYSUI_REQ);
    }

    if( cnt != 0 )
    {
      ipc_systemui_add_dialog(server_system_bus, vec, cnt);
    }
    free(vec);
  }

}

/* ------------------------------------------------------------------------- *
 * server_rethink_sysui_req
 * ------------------------------------------------------------------------- */

static
void
server_rethink_sysui_req(void)
{
  /* Note: transition ALARM_STATE_SYSUI_REQ -> ALARM_STATE_SYSUI_ACK
   *       is done via callback server_handle_systemui_ack() */

  if( !server_state_get_systemui_service() )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_SYSUI_REQ);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_event_set_state(eve, ALARM_STATE_WAITSYSUI);
    }
    free(vec);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_sysui_ack
 * ------------------------------------------------------------------------- */

static
void
server_rethink_sysui_ack(void)
{
  /* Note: transition ALARM_STATE_SYSUI_ACK -> ALARM_STATE_SYSUI_RSP
   *       is done via callback server_handle_systemui_rsp() */

  if( !server_state_get_systemui_service() )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_SYSUI_ACK);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_event_set_state(eve, ALARM_STATE_WAITSYSUI);
    }
    free(vec);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_sysui_rsp
 * ------------------------------------------------------------------------- */

static
void
server_rethink_sysui_rsp(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_SYSUI_RSP);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);
    log_info("[%ld] RSP: button=%d\n", (long)vec[i], eve->response);

    if ( -1 == eve->response && 
         (eve->flags & ALARM_EVENT_DISABLE_DELAYED) &&
         !alarm_event_is_recurring(eve) )
    {
      eve->flags |= ALARM_EVENT_DISABLED;
      continue;
    }

    server_event_do_response_actions(eve);

    if( queue_event_get_state(eve) != ALARM_STATE_SNOOZED )
    {
      queue_event_set_state(eve, ALARM_STATE_SERVED);
    }
  }
  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_snoozed
 * ------------------------------------------------------------------------- */

static
void
server_rethink_snoozed(void)
{
  int       cnt = 0;
  cookie_t *vec = 0;
  time_t    now = server_rethink_time;

  vec = queue_query_by_state(&cnt, ALARM_STATE_SNOOZED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    time_t snooze = server_event_get_snooze(eve);
    time_t curr   = now + snooze;

    log_debug("[%ld] SNOOZED: secs=%ld\n", (long)vec[i], (long)snooze);

#if SNOOZE_HIJACK_FIX
    if( eve->snooze_total == 0 )
    {
      eve->snooze_total = alarm_event_get_trigger(eve);
    }
#else
    time_t prev   = alarm_event_get_trigger(eve);
    time_t add    = curr - prev;
    eve->snooze_total += add;
    log_debug("SNOOZE: %+d -> %+d\n", add, eve->snooze_total);
#endif

    queue_event_set_trigger(eve, curr);
    queue_event_set_state(eve, ALARM_STATE_NEW);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_served
 * ------------------------------------------------------------------------- */

static
void
server_rethink_served(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_SERVED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);

    log_debug("[%ld] SERVED\n", (long)vec[i]);

    if( alarm_event_is_recurring(eve) )
    {
      queue_event_set_state(eve, ALARM_STATE_RECURRING);
      continue;
    }
    queue_event_set_state(eve, ALARM_STATE_DELETED);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_recurring
 * ------------------------------------------------------------------------- */

static
void
server_rethink_recurring(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_RECURRING);
  time_t    now = server_rethink_time;

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);
    const char *tz = server_event_get_tz(eve);

    time_t prev = alarm_event_get_trigger(eve);
    time_t curr = INT_MAX;

    log_debug("[%ld] RECURRING: count=%d\n", (long)vec[i], eve->recur_count);

    if( eve->recur_count > 0 )
    {
      eve->recur_count -= 1;
    }
#if SNOOZE_HIJACK_FIX
    prev = eve->snooze_total ?: alarm_event_get_trigger(eve);
    eve->snooze_total = 0;
#else
    prev -= eve->snooze_total;
    eve->snooze_total = 0;
#endif

    if( eve->recur_count != 0 )
    {
      if( eve->recur_secs > 0 )
      {
        curr = server_bump_time(prev, eve->recur_secs, now);
      }
      else
      {
        struct tm tm_now;

        ticker_break_tm(now, &tm_now, tz);

        for( size_t i = 0; i < eve->recurrence_cnt; ++i )
        {
          struct tm tm_tmp = tm_now;
          alarm_recur_t *rec = &eve->recurrence_tab[i];
          time_t t = alarm_recur_next(rec, &tm_tmp, tz);
          if( t > 0 && t < curr ) curr = t;
        }
      }
    }

    if( 0 < curr && curr < INT_MAX )
    {
      queue_event_set_trigger(eve, curr);
      queue_event_set_state(eve, ALARM_STATE_NEW);
    }
    else
    {
      queue_event_set_state(eve, ALARM_STATE_DELETED);
    }
  }
  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_deleted
 * ------------------------------------------------------------------------- */

static
void
server_rethink_deleted(void)
{
  int       cnt = 0;
  cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_DELETED);

  for( int i = 0; i < cnt; ++i )
  {
    alarm_event_t *eve = queue_get_event(vec[i]);
    log_debug("[%ld] DELETED\n", (long)vec[i]);
    server_event_do_state_actions(eve, ALARM_ACTION_WHEN_DELETED);
  }

  if( cnt != 0 )
  {
    ipc_systemui_cancel_dialog(server_system_bus, vec, cnt);
  }

  free(vec);
}

/* ------------------------------------------------------------------------- *
 * server_rethink_timechange
 * ------------------------------------------------------------------------- */

static
int
server_rethink_timechange(void)
{
  int    again = 0;
  int    zone  = 0;
  time_t back  = 0;
  time_t forw  = 0;
  time_t adj   = 0;
  int    dirty = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * save queue state so that we can track
   * changes originating from this function
   * - - - - - - - - - - - - - - - - - - - */

  dirty = queue_is_dirty(), queue_clr_dirty();

  /* - - - - - - - - - - - - - - - - - - - *
   * process state flag chages
   * - - - - - - - - - - - - - - - - - - - */

  if( server_state_get() & SF_TZ_CHANGED )
  {
    server_state_clr(SF_TZ_CHANGED);
    zone = 1;
    log_info("timezone: '%s' -> '%s'\n", server_tz_prev, server_tz_curr);
  }

  if( server_state_get() & SF_CLK_MV_FORW )
  {
    server_state_clr(SF_CLK_MV_FORW);
    forw = server_clock_forw_delta();
    //log_info("system time: %+ld (FORWARDS)\n", (long)forw);
  }

  if( server_state_get() & SF_CLK_MV_BACK )
  {
    server_state_clr(SF_CLK_MV_BACK);
    back = server_clock_back_delta();
    //log_info("system time: %+ld (BACKWARDS)\n", (long)back);
  }

  if( (adj = forw + back) != 0 )
  {
    log_info("system time: %+ld seconds\n", (long)adj);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * process events only if timezone or
   * system time has actually changed
   * - - - - - - - - - - - - - - - - - - - */

  if( zone || adj != 0 )
  {
    int       cnt = 0;
    cookie_t *vec = queue_query_by_state(&cnt, ALARM_STATE_QUEUED);

    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);

      /* - - - - - - - - - - - - - - - - - - - *
       * Snoozed alarms: Just adjust the trigger
       * time so that it still happens relative
       * to when snoozing took place
       * - - - - - - - - - - - - - - - - - - - */

      if( server_event_is_snoozed(eve) )
      {
#if SNOOZE_ADJUST_FIX
        queue_event_set_trigger(eve, alarm_event_get_trigger(eve) + adj);
        queue_event_set_state(eve, ALARM_STATE_NEW);
#endif
        continue;
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * Alarms that use absolute triggering
       * are not affected by system time or
       * time zone changes
       * - - - - - - - - - - - - - - - - - - - */

      if( eve->alarm_time > 0 )
      {
        continue;
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * evaluate updated trigger time
       * - - - - - - - - - - - - - - - - - - - */

      time_t now = server_rethink_time;
      time_t old = alarm_event_get_trigger(eve);
      time_t use = server_event_get_next_trigger(now, -1, eve);

      if( use == old )
      {
        // no change in triggering time
        continue;
      }

      if( !(eve->flags & ALARM_EVENT_BACK_RESCHEDULE) )
      {
        // The trigger time of the alarm is not to be moved
        // backwards due to system time change. Minor time
        // adjustments (network time sync etc) are allowed,
        // so that the alarm time stays at the expected time
        // of day.

        if( (use < old) && (adj < -5*60) )
        {
          // the trigger time would go backwards
          // system went back more than 5 mins
          // -> do not reschedule
          continue;
        }
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * adjust alarm triggering time
       * - - - - - - - - - - - - - - - - - - - */

      {
        const char *tz = NULL;
        struct tm tm;
        char trg[128];

        if( !xisempty(eve->alarm_tz) )
        {
          tz = eve->alarm_tz;
        }

        ticker_break_tm(old, &tm, tz ?: server_tz_prev);
        server_repr_tm(&tm, tz ?: server_tz_prev, trg, sizeof trg);
        log_debug("[%d] OLD: %s, at %+d\n", vec[i], trg, (int)(old - now));

        ticker_break_tm(use, &tm, tz ?: server_tz_curr);
        server_repr_tm(&tm, tz ?: server_tz_curr, trg, sizeof trg);
        log_debug("[%d] NEW: %s, at %+d\n", vec[i], trg, (int)(use - now));

        queue_event_set_trigger(eve, use);
        queue_event_set_state(eve, ALARM_STATE_NEW);
      }
    }

    server_timestate_sync();

    free(vec);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * obtain & return queue changes due to
   * this function, make sure previous
   * dirty state is not lost
   * - - - - - - - - - - - - - - - - - - - */

  again = queue_is_dirty();
  if( dirty ) queue_set_dirty();

  return again;
}

/* ------------------------------------------------------------------------- *
 * server_broadcast_timechange_handled  --  tz/clock change signal handled
 * ------------------------------------------------------------------------- */

static void server_broadcast_timechange_handled(void)
{
  if( server_state_get() & SF_CLK_BCAST)
  {
    server_state_clr(SF_CLK_BCAST);

    dbusif_signal_send(server_session_bus,
                       ALARMD_PATH,
                       ALARMD_INTERFACE,
                       ALARMD_TIME_CHANGE_IND,
                       DBUS_TYPE_INVALID);

    dbusif_signal_send(server_system_bus,
                       ALARMD_PATH,
                       ALARMD_INTERFACE,
                       ALARMD_TIME_CHANGE_IND,
                       DBUS_TYPE_INVALID);
  }
}

/* ------------------------------------------------------------------------- *
 * server_rethink_start_cb
 * ------------------------------------------------------------------------- */

static
gboolean
server_rethink_start_cb(gpointer data)
{
  //log_debug("----------------------------------------------------------------\n");

  if( !server_clock_source_is_stable() )
  {
    server_state_set(0, SF_CLK_BCAST);
    server_cancel_wakeup();

    // broadcast queue status after time is stable
    server_queuestate_invalidate();

    server_rethink_id = g_timeout_add(1*1000, server_rethink_start_cb, 0);
    return FALSE;
  }

  server_rethink_id   = 0;
  server_rethink_time = ticker_get_time();

  server_queue_cancel_save();

  log_info("-- rethink --\n");
  for( ;; )
  {
    queue_clr_dirty();

    server_queuestate_reset();

    //log_debug("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");

    server_rethink_new();
    server_rethink_waitconn();

    if( server_rethink_timechange() )
    {
      // ALARM_STATE_QUEUED -> ALARM_STATE_NEW
      // alarm state transitions were made and
      // those alarms need to be re-evaluated
      continue;
    }

    server_rethink_queued();
    server_rethink_missed();
    server_rethink_postponed();

    server_rethink_limbo();
    server_rethink_triggered();
    server_rethink_waitsysui();
    server_rethink_sysui_req();
    server_rethink_sysui_ack();
    server_rethink_sysui_rsp();

    server_rethink_snoozed();
    server_rethink_served();
    server_rethink_recurring();
    server_rethink_deleted();

    queue_cleanup_deleted();

    //log_debug(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

    if( !queue_is_dirty() ) break;
  }

// QUARANTINE   server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_LIMBO);
// QUARANTINE   server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_TRIGGERED);
// QUARANTINE   server_queuestate_curr.qs_alarms += queue_count_by_state(ALARM_STATE_SYSUI_RSP);

  if( server_state_get() & SF_ACT_DEAD )
  {
    server_queuestate_curr.qs_alarms += queue_count_by_state_and_flag(ALARM_STATE_LIMBO, ALARM_EVENT_ACTDEAD);
  }
  server_queuestate_curr.qs_alarms  += queue_count_by_state(ALARM_STATE_WAITSYSUI);
  server_queuestate_curr.qs_alarms  += queue_count_by_state(ALARM_STATE_SYSUI_REQ);
  server_queuestate_curr.qs_alarms  += queue_count_by_state(ALARM_STATE_SYSUI_ACK);

  server_state_sync();

  if( server_state_get() & SF_SEND_POWERUP )
  {
    if( server_state_get() & SF_DSME_UP )
    {
      log_info("sending power up request to dsme\n");
      server_state_clr(SF_SEND_POWERUP);
      server_queue_forced_save();
      ipc_dsme_req_powerup(server_system_bus);
    }
    else
    {
      log_warning("can't send powerup request - dsme is not up\n");
    }
  }

  server_queuestate_sync();
  server_broadcast_timechange_handled();
  server_queue_request_save();

  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_rethink_request
 * ------------------------------------------------------------------------- */

static
void
server_rethink_request(int delayed)
{
  if( server_rethink_id == 0 )
  {
    server_rethink_req = 0;
    if( delayed )
    {
      server_rethink_id = g_timeout_add(100, server_rethink_start_cb, 0);
    }
    else
    {
      server_rethink_id = g_idle_add(server_rethink_start_cb, 0);
    }
  }
  else
  {
    server_rethink_req = 1;
  }
}

/* ========================================================================= *
 * DBUS MESSAGE HANDLING
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_handle_set_debug  --  set debug state
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_set_debug(DBusMessage *msg)
{
  uint32_t ms = 0;
  uint32_t mc = 0;
  uint32_t fs = 0;
  uint32_t fc = 0;

  DBusMessage  *rsp = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_UINT32, &ms,
                                       DBUS_TYPE_UINT32, &mc,
                                       DBUS_TYPE_UINT32, &fs,
                                       DBUS_TYPE_UINT32, &fc,
                                       DBUS_TYPE_INVALID)) )
  {
    server_state_mask |=  ms;
    server_state_mask &= ~mc;
    server_state_fake |=  fs;
    server_state_fake &= ~fc;

    log_debug("set debug state: mask=%08x, bits=%08x\n",
              server_state_mask,
              server_state_fake & server_state_mask);

    ms = server_state_get();

    rsp = dbusif_reply_create(msg, DBUS_TYPE_UINT32, &ms, DBUS_TYPE_INVALID);

    server_state_set(0,0);
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_CUD  --  "clear user data" handler
 * ------------------------------------------------------------------------- */

#if ALARMD_CUD_ENABLE
static
DBusMessage *
server_handle_CUD(DBusMessage *msg)
{
  DBusMessage  *rsp = 0;
  dbus_int32_t  nak = 0;

  int       cnt = 0;
  cookie_t *vec = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * reset default snooze value
   * - - - - - - - - - - - - - - - - - - - */

  queue_set_snooze(0);

  /* - - - - - - - - - - - - - - - - - - - *
   * set all events deleted
   * - - - - - - - - - - - - - - - - - - - */

  if( (vec = queue_query_events(&cnt, 0,0, 0,0, 0)) )
  {
    for( int i = 0; i < cnt; ++i )
    {
      alarm_event_t *eve = queue_get_event(vec[i]);
      queue_event_set_state(eve, ALARM_STATE_DELETED);
    }
    free(vec);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * remove deleted events - no state
   * transition messages are generated
   * - - - - - - - - - - - - - - - - - - - */

  queue_cleanup_deleted();

  /* - - - - - - - - - - - - - - - - - - - *
   * save twice so that the backup gets
   * cleared too
   * - - - - - - - - - - - - - - - - - - - */

  server_queue_forced_save();
  server_queue_forced_save();

  rsp = dbusif_reply_create(msg, DBUS_TYPE_INT32, &nak, DBUS_TYPE_INVALID);
  return rsp;
}
#endif

/* ------------------------------------------------------------------------- *
 * server_handle_RFS  --  "restore factory settings" handler
 * ------------------------------------------------------------------------- */

#if ALARMD_RFS_ENABLE
static
DBusMessage *
server_handle_RFS(DBusMessage *msg)
{
  DBusMessage  *rsp = 0;
  dbus_int32_t  nak = 0;

  queue_set_snooze(0);
  server_queue_forced_save();

  rsp = dbusif_reply_create(msg, DBUS_TYPE_INT32, &nak, DBUS_TYPE_INVALID);
  return rsp;
}
#endif

/* ------------------------------------------------------------------------- *
 * server_handle_snooze_get  --  handle ALARMD_SNOOZE_GET method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_snooze_get(DBusMessage *msg)
{
  dbus_uint32_t val = queue_get_snooze();
  return dbusif_reply_create(msg, DBUS_TYPE_UINT32, &val, DBUS_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- *
 * server_handle_snooze_set  --  handle ALARMD_SNOOZE_SET method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_snooze_set(DBusMessage *msg)
{
  DBusMessage  *rsp = 0;
  dbus_uint32_t val = 0;
  dbus_bool_t   res = 1;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_UINT32, &val,
                                       DBUS_TYPE_INVALID)) )
  {
    queue_set_snooze(val);
    rsp = dbusif_reply_create(msg, DBUS_TYPE_BOOLEAN, &res, DBUS_TYPE_INVALID);
  }

  server_queue_request_save();
  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_add  --  handle ALARMD_EVENT_ADD method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_add(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  cookie_t       cookie = 0;
  alarm_event_t *event  = 0;

  if( (event = dbusif_decode_event(msg)) != 0 )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * client -> server: reset fields that
     * are manged by the server
     * - - - - - - - - - - - - - - - - - - - */

    event->response = -1;
    event->snooze_total = 0;

    alarm_event_set_cookie(event, 0);
    alarm_event_set_trigger(event, 0);
    queue_event_set_state(event, ALARM_STATE_NEW);

    time_t trigger = server_event_evaluate_initial_trigger(event);

    time_t now = ticker_get_time();
    if( trigger >= now )
    {
      alarm_event_set_trigger(event, trigger);
      if( (cookie = queue_add_event(event)) != 0 )
      {
        event = 0;
      }
    }
  }

  alarm_event_delete(event);

  rsp = dbusif_reply_create(msg, DBUS_TYPE_INT32, &cookie, DBUS_TYPE_INVALID);

  server_rethink_request(1);

  log_info("%s() -> %ld\n", __FUNCTION__, (long)cookie);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_update  --  handle ALARMD_EVENT_UPDATE method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_update(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  cookie_t       cookie = 0;
  alarm_event_t *event  = 0;

  if( (event = dbusif_decode_event(msg)) != 0 )
  {
    /* - - - - - - - - - - - - - - - - - - - *
     * remove if exists
     * - - - - - - - - - - - - - - - - - - - */

    queue_del_event(event->ALARMD_PRIVATE(cookie));

    /* - - - - - - - - - - - - - - - - - - - *
     * continue as with add
     * - - - - - - - - - - - - - - - - - - - */

    event->response = -1;
    event->snooze_total = 0;

    alarm_event_set_cookie(event, 0);
    alarm_event_set_trigger(event, 0);
    queue_event_set_state(event, ALARM_STATE_NEW);

    time_t trigger = server_event_evaluate_initial_trigger(event);

    time_t now = ticker_get_time();
    if( trigger >= now )
    {
      alarm_event_set_trigger(event, trigger);
      if( (cookie = queue_add_event(event)) != 0 )
      {
        event = 0;
      }
    }
  }

  alarm_event_delete(event);

  rsp = dbusif_reply_create(msg, DBUS_TYPE_INT32, &cookie, DBUS_TYPE_INVALID);

  server_rethink_request(1);

  log_info("%s() -> %ld\n", __FUNCTION__, (long)cookie);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_del  -- handle ALARMD_EVENT_DEL method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_del(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  dbus_int32_t   cookie = 0;
  dbus_bool_t    res    = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32, &cookie,
                                       DBUS_TYPE_INVALID)) )
  {
    res = queue_del_event(cookie);
    rsp = dbusif_reply_create(msg, DBUS_TYPE_BOOLEAN, &res, DBUS_TYPE_INVALID);
    server_rethink_request(1);
  }

  log_info("%s() -> %s\n", __FUNCTION__, res ? "OK" : "ERR");

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_query  --  handle ALARMD_EVENT_QUERY method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_query(DBusMessage *msg)
{
  DBusMessage   *rsp  = 0;
  dbus_int32_t   lo   = 0;
  dbus_int32_t   hi   = 0;
  dbus_int32_t   mask = 0;
  dbus_int32_t   flag = 0;
  cookie_t      *vec  = 0;
  int            cnt  = 0;
  char          *app  = 0;

  assert( sizeof(dbus_int32_t) == sizeof(cookie_t) );

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32,  &lo,
                                       DBUS_TYPE_INT32,  &hi,
                                       DBUS_TYPE_INT32,  &mask,
                                       DBUS_TYPE_INT32,  &flag,
                                       DBUS_TYPE_STRING, &app,
                                       DBUS_TYPE_INVALID)) )
  {
    if( (vec = queue_query_events(&cnt, lo, hi, mask, flag, app)) )
    {
      rsp = dbusif_reply_create(msg,
                                DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &vec, cnt,
                                DBUS_TYPE_INVALID);
    }
  }

  free(vec);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_get  --  handle ALARMD_EVENT_GET method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_get(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  dbus_int32_t   cookie = 0;
  alarm_event_t *event  = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32,  &cookie,
                                       DBUS_TYPE_INVALID)) )
  {
    if( (event = queue_get_event(cookie)) != 0 )
    {
      /* Do not return events that are in deleted state */
      if( queue_event_get_state(event) != ALARM_STATE_DELETED )
      {
        rsp = dbusif_reply_create(msg, DBUS_TYPE_INVALID);
        dbusif_encode_event(rsp, event, 0);
      }
    }
  }

// QUARANTINE   log_debug_F("cookie=%ld, event=%p, rsp=%p\n", (long)cookie, event, rsp);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_event_ack  -- handle ALARMD_DIALOG_RSP method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_event_ack(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  dbus_int32_t   cookie = 0;
  dbus_int32_t   button = 0;
  dbus_bool_t    res    = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32,  &cookie,
                                       DBUS_TYPE_INT32,  &button,
                                       DBUS_TYPE_INVALID)) )
  {
    if( button != -1 && (button & SERVER_POWERUP_BIT) )
    {
      button &= ~SERVER_POWERUP_BIT;
      server_state_set(0, SF_SEND_POWERUP);
    }
    res = (server_handle_systemui_rsp(cookie, button) != -1);
    rsp = dbusif_reply_create(msg, DBUS_TYPE_BOOLEAN, &res, DBUS_TYPE_INVALID);

    server_rethink_request(1);
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_queue_ack  -- handle ALARMD_DIALOG_ACK method call
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_queue_ack(DBusMessage *msg)
{
  DBusMessage   *rsp    = 0;
  dbus_bool_t    res    = 0;
  dbus_int32_t  *vec    = 0;
  int            cnt    = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &vec, &cnt,
                                       DBUS_TYPE_INVALID)) )
  {
    res = (server_handle_systemui_ack((cookie_t *)vec, cnt) != -1);
    rsp = dbusif_reply_create(msg, DBUS_TYPE_BOOLEAN, &res, DBUS_TYPE_INVALID);

  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_name_acquired
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_name_acquired(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;
  char          *service   = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_STRING, &service,
                                       DBUS_TYPE_INVALID)) )
  {
    log_debug("dbus name acquired: '%s'\n", service);
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_save_data
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_save_data(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;

  log_info("================ SAVE DATA SIGNAL ================\n");

  server_queue_forced_save();

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_shutdown
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_shutdown(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;

  log_info("================ SHUT DOWN SIGNAL ================\n");

  // FIXME: what are we supposed to do when dsme sends us the shutdown signal?

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_init_done
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_init_done(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;

  log_info("================ INIT DONE SIGNAL ================\n");

  // do it anyway ...
  //if( server_limbo_wait_state == DESKTOP_WAIT_STARTUP )
  {
    server_limbo_disable("init done detected");
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_hildon_ready
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_hildon_ready(DBusMessage *msg)
{
  DBusMessage   *rsp       = 0;

  log_info("================ HILDON READY ================\n");

  if( server_limbo_wait_state == DESKTOP_WAIT_HILDON )
  {
    server_limbo_disable("hildon ready detected");
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_name_owner_chaned  --  handle dbus name owner changes
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_name_owner_chaned(DBusMessage *msg)
{

  DBusMessage   *rsp       = 0;
  char          *service   = 0;
  char          *old_owner = 0;
  char          *new_owner = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_STRING, &service,
                                       DBUS_TYPE_STRING, &old_owner,
                                       DBUS_TYPE_STRING, &new_owner,
                                       DBUS_TYPE_INVALID)) )
  {
    log_debug("dbus name owner changed: '%s': '%s' -> '%s'\n",
              service, old_owner, new_owner);

    unsigned clr = 0, set = 0;

    if( !strcmp(service, SYSTEMUI_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_SYSTEMUI_UP;
        set |= SF_SYSTEMUI_DN;
      }
      else
      {
        set |= SF_SYSTEMUI_UP;
      }
    }
    else if( !strcmp(service, CLOCKD_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_CLOCKD_UP;
        set |= SF_CLOCKD_DN;

        ticker_use_libtime(FALSE);
      }
      else
      {
        set |= SF_CLOCKD_UP;

        ticker_use_libtime(TRUE);
      }
    }
    else if( !strcmp(service, DSME_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_DSME_UP;
        set |= SF_DSME_DN;
      }
      else
      {
        set |= SF_DSME_UP;
      }
    }
    else if( !strcmp(service, MCE_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_MCE_UP;
        set |= SF_MCE_DN;
      }
      else
      {
        set |= SF_MCE_UP;
      }
    }
    else if( !strcmp(service, STATUSAREA_CLOCK_SERVICE) )
    {
      if( xisempty(new_owner) )
      {
        clr |= SF_STATUSBAR_UP;
        set |= SF_STATUSBAR_DN;
      }
      else
      {
        set |= SF_STATUSBAR_UP;
      }
    }
    else if( !strcmp(service, HOME_SERVICE) )
    {
      if( !xisempty(new_owner) )
      {
        log_info("================ HOME READY ================\n");

        if( server_limbo_wait_state == DESKTOP_WAIT_HOME )
        {
          server_limbo_disable("home ready detected");
        }
        else
        {
          server_limbo_disable_delayed("home ready detected");
        }
      }
    }

    server_state_set(clr, set);
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_handle_time_change
 * ------------------------------------------------------------------------- */

static
DBusMessage *
server_handle_time_change(DBusMessage *msg)
{
  DBusMessage *rsp = 0;
  dbus_int32_t clk = 0;
  DBusError    err = DBUS_ERROR_INIT;

  log_info("clockd - time change notification received\n");

  server_state_set(0, SF_CLK_BCAST);

  if( !dbus_message_get_args(msg, &err,
                             DBUS_TYPE_INT32, &clk,
                             DBUS_TYPE_INVALID) )
  {
    log_error_F("%s: %s\n", err.name, err.message);
  }

  ticker_get_synced();

  server_timestate_read();

  if( strcmp(server_tz_curr, server_tz_prev) )
  {
    log_info("clockd - timezone changed\n");
    server_state_set(0, SF_TZ_CHANGED);
  }

  if( server_dst_curr != server_dst_prev )
  {
    log_info("clockd - isdst changed\n");
    server_state_set(0, SF_TZ_CHANGED);
  }

  if( clk != 0 )
  {
    log_info("clockd - system time changed\n");
    server_state_set(0, SF_CLK_CHANGED);
  }

  dbus_error_free(&err);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * server_session_bus_cb  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
server_session_bus_cb(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
#if ALARMD_ON_SESSION_BUS
  static const dbusif_method_lut alarm_methods[] =
  {
    {ALARMD_EVENT_ADD,   server_handle_event_add},
    {ALARMD_EVENT_DEL,   server_handle_event_del},
    {ALARMD_EVENT_GET,   server_handle_event_get},
    {ALARMD_EVENT_QUERY, server_handle_event_query},
    {ALARMD_EVENT_UPDATE,server_handle_event_update},

    {ALARMD_SNOOZE_SET,  server_handle_snooze_set},
    {ALARMD_SNOOZE_GET,  server_handle_snooze_get},

    {ALARMD_DIALOG_RSP,  server_handle_event_ack},
    {ALARMD_DIALOG_ACK,  server_handle_queue_ack},

    {"alarmd_set_debug", server_handle_set_debug},

#if ALARMD_CUD_ENABLE
    {"clear_user_data",          server_handle_CUD},
#endif
#if ALARMD_RFS_ENABLE
    {"restore_factory_settings", server_handle_RFS},
#endif

    {0,}
  };
#endif

  static const dbusif_method_lut dbus_signals[] =
  {
    {DBUS_NAME_OWNER_CHANGED, server_handle_name_owner_chaned},
    {DBUS_NAME_ACQUIRED,      server_handle_name_acquired},
    {0,}
  };

  static const dbusif_interface_lut filter[] =
  {
#if ALARMD_ON_SESSION_BUS
    {
      ALARMD_INTERFACE,
      ALARMD_PATH,
      DBUS_MESSAGE_TYPE_METHOD_CALL,
      alarm_methods,
      DBUS_HANDLER_RESULT_HANDLED
    },
#endif

    {
      DBUS_INTERFACE_DBUS,
      DBUS_PATH_DBUS,
      DBUS_MESSAGE_TYPE_SIGNAL,
      dbus_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    { 0, }
  };

  if( dbus_message_is_signal(msg, DBUS_INTERFACE_LOCAL, "Disconnected") )
  {
    log_warning("disconnected from session bus - expecting restart soon\n");
    //mainloop_stop(0);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  return dbusif_handle_message_by_interface(filter, conn, msg);
}

/* ------------------------------------------------------------------------- *
 * server_system_bus_cb
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
server_system_bus_cb(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
#if ALARMD_ON_SYSTEM_BUS
  static const dbusif_method_lut alarm_methods[] =
  {
    {ALARMD_EVENT_ADD,   server_handle_event_add},
    {ALARMD_EVENT_DEL,   server_handle_event_del},
    {ALARMD_EVENT_GET,   server_handle_event_get},
    {ALARMD_EVENT_QUERY, server_handle_event_query},
    {ALARMD_EVENT_UPDATE,server_handle_event_update},

    {ALARMD_SNOOZE_SET,  server_handle_snooze_set},
    {ALARMD_SNOOZE_GET,  server_handle_snooze_get},

    {ALARMD_DIALOG_RSP,  server_handle_event_ack},
    {ALARMD_DIALOG_ACK,  server_handle_queue_ack},

    {"alarmd_set_debug", server_handle_set_debug},

#if ALARMD_CUD_ENABLE
    {"clear_user_data",          server_handle_CUD},
#endif
#if ALARMD_RFS_ENABLE
    {"restore_factory_settings", server_handle_RFS},
#endif

    {0,}
  };
#endif

  static const dbusif_method_lut clockd_signals[] =
  {
    {CLOCKD_TIME_CHANGED,   server_handle_time_change},
    {0,}
  };

  static const dbusif_method_lut dbus_signals[] =
  {
    {DBUS_NAME_OWNER_CHANGED, server_handle_name_owner_chaned},
    {DBUS_NAME_ACQUIRED,      server_handle_name_acquired},
    {0,}
  };

  static const dbusif_method_lut dsme_signals[] =
  {
    {DSME_DATA_SAVE_SIG,   server_handle_save_data},
    {DSME_SHUTDOWN_SIG,    server_handle_shutdown},
    {0,}
  };

  static const dbusif_method_lut startup_signals[] =
  {
    {STARTUP_SIG_INIT_DONE, server_handle_init_done},
    {0,}
  };
  static const dbusif_method_lut hildon_signals[] =
  {
    {HILDON_SIG_READY, server_handle_hildon_ready},
    {0,}
  };

  static const dbusif_interface_lut filter[] =
  {
#if ALARMD_ON_SYSTEM_BUS
    {
      ALARMD_INTERFACE,
      ALARMD_PATH,
      DBUS_MESSAGE_TYPE_METHOD_CALL,
      alarm_methods,
      DBUS_HANDLER_RESULT_HANDLED
    },
#endif
    {
      CLOCKD_INTERFACE,
      CLOCKD_PATH,
      DBUS_MESSAGE_TYPE_SIGNAL,
      clockd_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    {
      DBUS_INTERFACE_DBUS,
      DBUS_PATH_DBUS,
      DBUS_MESSAGE_TYPE_SIGNAL,
      dbus_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    {
      DSME_SIGNAL_IF,
      DSME_SIGNAL_PATH,
      DBUS_MESSAGE_TYPE_SIGNAL,
      dsme_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    {
      STARTUP_SIG_IF,
      STARTUP_SIG_PATH,
      DBUS_MESSAGE_TYPE_SIGNAL,
      startup_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },
    {
      HILDON_SIG_IF,
      HILDON_SIG_PATH,
      DBUS_MESSAGE_TYPE_SIGNAL,
      hildon_signals,
      DBUS_HANDLER_RESULT_NOT_YET_HANDLED
    },

    { 0, }
  };

  if( dbus_message_is_signal(msg, DBUS_INTERFACE_LOCAL, "Disconnected") )
  {
    log_warning("disconnected from system bus - terminating\n");
    mainloop_stop();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  return dbusif_handle_message_by_interface(filter, conn, msg);
}

/* ------------------------------------------------------------------------- *
 * server_icd_status_cb
 * ------------------------------------------------------------------------- */

static
void
server_icd_status_cb(int connected)
{
  log_info("internet connection: %s\n",
            connected ? "available" : "not available");
  if( connected )
  {
    server_state_set(0, SF_CONNECTED);
  }
  else
  {
    server_state_set(SF_CONNECTED, 0);
  }
}

/* ========================================================================= *
 * IGNORE MODIFIED QUEUE FILE UNLESS FOLLOWED BY ALARMD RESTART
 * ========================================================================= */

#if ALARMD_QUEUE_MODIFIED_IGNORE
// timeout for ignoring modified queue file
static guint server_queue_touched_restart_id = 0;

/* ------------------------------------------------------------------------- *
 * server_queue_touched_ignore_cb
 * ------------------------------------------------------------------------- */

static gboolean server_queue_touched_ignore_cb(gpointer aptr)
{
  log_critical("The queue file was modified without restarting the device.\n");
  log_critical("Restoring queue file from internal state information.\n");

  // do forced overwrite
  queue_save_forced();

  server_queue_touched_restart_id = 0;
  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * server_queue_touched_ignore_cancel
 * ------------------------------------------------------------------------- */

static void server_queue_touched_ignore_cancel(void)
{
  if( server_queue_touched_restart_id != 0 )
  {
    g_source_remove(server_queue_touched_restart_id);
    server_queue_touched_restart_id = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * server_queue_touched_ignore_request
 * ------------------------------------------------------------------------- */

static void server_queue_touched_ignore_request(void)
{
  if( server_queue_touched_restart_id == 0 )
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

    server_queue_touched_restart_id =
      g_timeout_add_seconds(60, server_queue_touched_ignore_cb, 0);
  }
}

/* ------------------------------------------------------------------------- *
 * server_queue_touched_ignore_setup
 * ------------------------------------------------------------------------- */

static void server_queue_touched_ignore_setup(void)
{
  queue_set_modified_cb(server_queue_touched_ignore_request);
}
#endif

/* ========================================================================= *
 * SERVER INIT/QUIT
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * server_init_session_bus
 * ------------------------------------------------------------------------- */

static
int
server_init_session_bus(void)
{
  int       res = -1;
  DBusError err = DBUS_ERROR_INIT;

  // connect
  if( (server_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    log_error_F("%s: %s\n", err.name, err.message);
    goto cleanup;
  }

  // register filter callback
  if( !dbus_connection_add_filter(server_session_bus, server_session_bus_cb, 0, 0) )
  {
    log_error_F("%s: %s\n", "add filter", "FAILED");
    goto cleanup;
  }

  // listen to signals
  if( dbusif_add_matches(server_session_bus, sessionbus_signals) == -1 )
  {
    goto cleanup;
  }

  // bind to gmainloop
  dbus_connection_setup_with_g_main(server_session_bus, NULL);
  dbus_connection_set_exit_on_disconnect(server_session_bus, FALSE);

  // claim service name
  int ret = dbus_bus_request_name(server_session_bus, ALARMD_SERVICE,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);

  if( ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
  {
    if( dbus_error_is_set(&err) )
    {
      log_error_F("%s: %s\n", err.name, err.message);
    }
    else
    {
      log_error_F("%s: %s\n", "request name",
                  "not primary owner of connection\n");
    }
    goto cleanup;
  }

  // success

  res = 0;

  // cleanup & return

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * server_init_system_bus
 * ------------------------------------------------------------------------- */

static
int
server_init_system_bus(void)
{
  int       res = -1;
  DBusError err = DBUS_ERROR_INIT;

  char    **dsme_signals = 0;

  // connect
  if( (server_system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    log_error_F("%s: %s\n", err.name, err.message);
    if( xscratchbox() )
    {
      log_warning("scratchbox detected: system bus actions not available\n");
      res = 0;
    }
    goto cleanup;
  }

  // register filter callback
  if( !dbus_connection_add_filter(server_system_bus, server_system_bus_cb, 0, 0) )
  {
    log_error_F("%s: %s\n", "add filter", "FAILED");
    goto cleanup;
  }

  // listen to signals
  if( dbusif_add_matches(server_system_bus, systembus_signals) == -1 )
  {
    goto cleanup;
  }

  if( (dsme_signals = server_get_dsme_signal_matches()) == 0 )
  {
    goto cleanup;
  }
  if( dbusif_add_matches(server_system_bus, (const char * const *)dsme_signals) == -1 )
  {
    goto cleanup;
  }

  // bind to gmainloop
  dbus_connection_setup_with_g_main(server_system_bus, NULL);
  dbus_connection_set_exit_on_disconnect(server_system_bus, FALSE);

  // claim service name
  int ret = dbus_bus_request_name(server_system_bus, ALARMD_SERVICE,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);

  if( ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
  {
    if( dbus_error_is_set(&err) )
    {
      log_error_F("%s: %s\n", err.name, err.message);
    }
    else
    {
      log_error_F("%s: %s\n", "request name",
                  "not primary owner of connection\n");
    }
    goto cleanup;
  }

  // success

  res = 0;

  // cleanup & return

  cleanup:

  xfreev(dsme_signals);
  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * server_quit_session_bus
 * ------------------------------------------------------------------------- */

static
void
server_quit_session_bus(void)
{
  if( server_session_bus != 0 )
  {
    dbusif_remove_matches(server_session_bus, sessionbus_signals);
    dbus_connection_remove_filter(server_session_bus, server_session_bus_cb, 0);
    dbus_connection_unref(server_session_bus);
    server_session_bus = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * server_quit_system_bus
 * ------------------------------------------------------------------------- */

static
void
server_quit_system_bus(void)
{
  char **dsme_signals = 0;

  if( server_system_bus != 0 )
  {
    if( (dsme_signals = server_get_dsme_signal_matches()) != 0 )
    {
      dbusif_remove_matches(server_system_bus, (const char * const *)dsme_signals);
    }

    dbusif_remove_matches(server_system_bus, systembus_signals);

    dbus_connection_remove_filter(server_system_bus, server_system_bus_cb, 0);
    dbus_connection_unref(server_system_bus);
    server_system_bus = 0;
  }

  xfreev(dsme_signals);
}

/* ------------------------------------------------------------------------- *
 * server_init
 * ------------------------------------------------------------------------- */

int
server_init(void)
{
  int       res = -1;
  DBusError err = DBUS_ERROR_INIT;

  /* - - - - - - - - - - - - - - - - - - - *
   * even if alarmd is run as root, the
   * execute actions are performed as
   * 'user' or 'nobody'
   * - - - - - - - - - - - - - - - - - - - */

  ipc_exec_init();

  /* - - - - - - - - - - - - - - - - - - - *
   * setup system ui response callback
   * - - - - - - - - - - - - - - - - - - - */

  ipc_systemui_set_ack_callback(server_systemui_ack_cb);
  ipc_systemui_set_service_callback(server_state_get_systemui_service);

  /* - - - - - - - - - - - - - - - - - - - *
   * session & system bus connections
   * - - - - - - - - - - - - - - - - - - - */

  if( server_init_system_bus() == -1 )
  {
    goto cleanup;
  }

  if( server_init_session_bus() == -1 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * start icd state tracking
   * - - - - - - - - - - - - - - - - - - - */

  if( server_system_bus != 0 )
  {
    // conic signalling uses system bus
    ipc_icd_init(server_icd_status_cb);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * register queuefile overwrite callback
   * - - - - - - - - - - - - - - - - - - - */

#if ALARMD_QUEUE_MODIFIED_IGNORE
  server_queue_touched_ignore_setup();
#endif

  /* - - - - - - - - - - - - - - - - - - - *
   * set the ball rolling
   * - - - - - - - - - - - - - - - - - - - */

  server_state_init();
  server_queuestate_init();

  server_rethink_request(0);

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
 * server_quit
 * ------------------------------------------------------------------------- */

void
server_quit(void)
{
#if ALARMD_QUEUE_MODIFIED_IGNORE
  server_queue_touched_ignore_cancel();
#endif

  server_queue_cancel_save();

  ipc_icd_quit();

  server_quit_session_bus();
  server_quit_system_bus();

  ipc_exec_quit();
}
