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

#include "queue.h"
#include "logging.h"
#include "inifile.h"
#include "ticker.h"

#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

/* ========================================================================= *
 * CONSTANTS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue file paths
 * ------------------------------------------------------------------------- */

#define QUEUE_DATABASE    ALARMD_CONFIG_CACHEDIR"/alarm_queue.ini"
#define QUEUE_BACKUP      QUEUE_DATABASE".bak"
#define QUEUE_TEMPSAVE    QUEUE_DATABASE".tmp"

/* ------------------------------------------------------------------------- *
 * default settings
 * ------------------------------------------------------------------------- */

#define QUEUE_SNOOZE_DEFAULT   (10 * 60) // 10 minutes

/* ------------------------------------------------------------------------- *
 * event state names
 * ------------------------------------------------------------------------- */

const char * const queue_event_state_names[] =
{
#define ALARM_STATE(name) #name,
#include "states.inc"
};

/* ========================================================================= *
 * INTERNAL STATE DATA
 * ========================================================================= */

/* current default snooze period, used for events that do
 * not specify custom snooze */
static unsigned        queue_snooze = QUEUE_SNOOZE_DEFAULT;

/* highest cookie currently in use */
static cookie_t        queue_cookie = 0;

/* queue out of sync with persistent storage flag */
static int             queue_dirty  = 0;

/* active events - ordered by event cookie
 *
 * ascending cookie sort: older alarms first */
static alarm_event_t **queue_by_cookie  = 0;

/* active events - ordered by event trigger
 *
 * descending trigger sort: the first to trigger
 * is the last in the queue -> updating trigger
 * values (due to snooze etc) is more likely to
 * affect only the tail of the queue */
static alarm_event_t **queue_by_trigger = 0;

/* number of active events */
static size_t          queue_count      = 0;

/* number of slots available in queue tables */
static size_t          queue_alloc      = 0;

/* stats of the queue file - used for detecting whend somebody
 * else than alarmd has modified the queue file since the last
 * load / save operation (mainly restoring backed up alarms). */
static struct stat     queue_save_stat;

/* callback function: called when queue
 * file modification is detected */
static void (*queue_modified_cb)(void) = 0;

/* ========================================================================= *
 * COMPARE OPERATORS
 * ========================================================================= */

#define CMP(a,b) (((a)>(b))-((a)<(b)))

/* ------------------------------------------------------------------------- *
 * queue_cmp_cookie  --  compare operator for cookie identifiers
 * ------------------------------------------------------------------------- */

static
int
queue_cmp_cookie(cookie_t a, cookie_t b)
{
  return CMP(a,b);
}

/* ------------------------------------------------------------------------- *
 * queue_cmp_trigger  --  compare operator for trigger time values
 * ------------------------------------------------------------------------- */

static
int
queue_cmp_trigger(time_t a, time_t b)
{
  return CMP(a,b);
}

/* ------------------------------------------------------------------------- *
 * queue_cmp_event_cookie  --  compare operator for sorting by cookie
 * ------------------------------------------------------------------------- */

static
int
queue_cmp_event_cookie(const alarm_event_t *a, const alarm_event_t *b)
{
  // cookie table in ascending order: oldest first
  cookie_t ca = alarm_event_get_cookie(a);
  cookie_t cb = alarm_event_get_cookie(b);
  //log_debug("CMP COOKIE:  %d - %d\n", (int)ca, (int)cb);
  return queue_cmp_cookie(ca, cb);
}

/* ------------------------------------------------------------------------- *
 * queue_cmp_event_trigger  --  compare operator for sorting by trigger time
 * ------------------------------------------------------------------------- */

static
int
queue_cmp_event_trigger(const alarm_event_t *a, const alarm_event_t *b)
{
  // trigger table in descending order:
  // - next to trigger last
  // - "oldest" (by cookie) at the same time last

  time_t ta = alarm_event_get_trigger(a);
  time_t tb = alarm_event_get_trigger(b);
  return queue_cmp_trigger(tb, ta) ?: queue_cmp_event_cookie(b,a);
}

#undef CMP

/* ========================================================================= *
 * INTERNAL FUNCTIONALITY
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_get_trigger_slot  --  find event slot in trigger ordered table
 * ------------------------------------------------------------------------- */

static size_t queue_get_trigger_slot(alarm_event_t *eve)
{
  size_t l,h,i;

  for( l = 0, h = queue_count; l < h; )
  {
    if( queue_cmp_event_trigger(queue_by_trigger[(i = (l+h)/2)], eve) < 0 )
    {
      l = i + 1;
    }
    else
    {
      h = i + 0;
    }
  }
  assert( l == h );
  return l;
}

/* ------------------------------------------------------------------------- *
 * queue_get_cookie_slot  --  find event slot in cookie ordered table
 * ------------------------------------------------------------------------- */

static size_t queue_get_cookie_slot(alarm_event_t *eve)
{
  size_t l,h,i;

  for( l = 0, h = queue_count; l < h; )
  {
    if( queue_cmp_event_cookie(queue_by_cookie[(i = (l+h)/2)], eve) < 0 )
    {
      l = i + 1;
    }
    else
    {
      h = i + 0;
    }
  }
  assert( l == h );
  return l;
}

/* ------------------------------------------------------------------------- *
 * queue_insert_event
 * ------------------------------------------------------------------------- */

static
void
queue_insert_event(alarm_event_t *eve)
{
  if( queue_count == queue_alloc )
  {
    queue_alloc += 32;

    queue_by_cookie  = realloc(queue_by_cookie,
                               queue_alloc * sizeof *queue_by_cookie);
    queue_by_trigger = realloc(queue_by_trigger,
                               queue_alloc * sizeof *queue_by_trigger);
  }

  size_t t = queue_get_trigger_slot(eve);
  size_t c = queue_get_cookie_slot(eve);

  assert( (t == queue_count) || (queue_by_trigger[t] != eve) );
  assert( (c == queue_count) || (queue_by_cookie[c]  != eve) );

  for( size_t i = queue_count; i > t; --i )
  {
    queue_by_trigger[i] = queue_by_trigger[i-1];
  }

  for( size_t i = queue_count; i > c; --i )
  {
    queue_by_cookie[i] = queue_by_cookie[i-1];
  }

  queue_by_trigger[t] = eve;
  queue_by_cookie[c]  = eve;
  queue_count += 1;

  queue_set_dirty();
}

/* ========================================================================= *
 * INDICATION INTERFACE
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_set_modified_cb
 * ------------------------------------------------------------------------- */

void queue_set_modified_cb(void (*cb)(void))
{
  queue_modified_cb = cb;
}

/* ------------------------------------------------------------------------- *
 * queue_indicate_modified
 * ------------------------------------------------------------------------- */

static void queue_indicate_modified(void)
{
  if( queue_modified_cb != 0 )
  {
    queue_modified_cb();
  }
}

/* ========================================================================= *
 * SETTINGS INTERFACE
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_get_snooze
 * ------------------------------------------------------------------------- */

unsigned
queue_get_snooze(void)
{
  return queue_snooze;
}

/* ------------------------------------------------------------------------- *
 * queue_set_snooze
 * ------------------------------------------------------------------------- */

void
queue_set_snooze(unsigned snooze)
{
  if( snooze < 1 || snooze > 24*60*60 )
  {
    snooze = QUEUE_SNOOZE_DEFAULT;
  }
  queue_snooze = snooze;
  queue_set_dirty();
}

/* ========================================================================= *
 * EVENT INTERFACE
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_event_set_trigger
 * ------------------------------------------------------------------------- */

void
queue_event_set_trigger(alarm_event_t *event, time_t trigger)
{
  log_info("[%ld] SET TRIGGER: %s (T%s)\n",
           (long)alarm_event_get_cookie(event),
           ticker_date_format_long(0,0,trigger),
           ticker_secs_format(0,0,ticker_get_time()-trigger));

  size_t ti, to;

  /* - - - - - - - - - - - - - - - - - - - *
   * get slot indices for old and updated
   * trigger values
   * - - - - - - - - - - - - - - - - - - - */

  alarm_event_t temp = {
    .ALARMD_PRIVATE(cookie) = event->ALARMD_PRIVATE(cookie),
    .ALARMD_PRIVATE(trigger) = trigger,
  };

  ti = queue_get_trigger_slot(event);
  to = queue_get_trigger_slot(&temp);

  event->ALARMD_PRIVATE(trigger) = trigger;

  /* - - - - - - - - - - - - - - - - - - - *
   * how queue will be shifted depends on
   * whether the updated position is before
   * or after the original
   * - - - - - - - - - - - - - - - - - - - */

  if( to <= ti )
  {
    //  +---+---+---+---+---+---+---+---+
    //  | A | B | C | X | D | E | F | G |
    //  +---+---+---+---+---+---+---+---+
    //      |to     |ti
    //  +---+---+---+---+---+---+---+---+
    //  | A | - | B | C | D | E | F | G |
    //  +---+---+---+---+---+---+---+---+
    //      |to     |ti

    for( ; ti > to; --ti )
    {
      queue_by_trigger[ti] = queue_by_trigger[ti-1];
    }
  }
  else
  {
    //  +---+---+---+---+---+---+---+---+
    //  | A | B | C | X | D | E | F | G |
    //  +---+---+---+---+---+---+---+---+
    //              |ti         |to
    //  +---+---+---+---+---+---+---+---+
    //  | A | B | C | D | E | - | F | G |
    //  +---+---+---+---+---+---+---+---+
    //              |ti     |to

    for( --to; ti < to; ++ti )
    {
      queue_by_trigger[ti] = queue_by_trigger[ti+1];
    }
  }
  queue_by_trigger[to] = event;

  queue_set_dirty();
}

/* ------------------------------------------------------------------------- *
 * queue_event_get_state
 * ------------------------------------------------------------------------- */

unsigned
queue_event_get_state(const alarm_event_t *self)
{
  return self->flags >> ALARM_EVENT_CLIENT_BITS;
}

/* ------------------------------------------------------------------------- *
 * queue_event_set_state
 * ------------------------------------------------------------------------- */

void
queue_event_set_state(alarm_event_t *self, unsigned state)
{
  unsigned previous = queue_event_get_state(self);
  unsigned current  = previous;

  /* - - - - - - - - - - - - - - - - - - - *
   * do some filtering for state transitions
   * occuring due to external events
   * - - - - - - - - - - - - - - - - - - - */

  switch( state )
  {
  default:
    log_debug("UNKNOWN TRANSITION: %s -> %u\n",
              queue_event_state_names[previous], state);
    break;

  case ALARM_STATE_NEW:
  case ALARM_STATE_WAITCONN:
  case ALARM_STATE_QUEUED:
  case ALARM_STATE_MISSED:
  case ALARM_STATE_POSTPONED:

  case ALARM_STATE_LIMBO:
  case ALARM_STATE_TRIGGERED:
  case ALARM_STATE_WAITSYSUI:
  case ALARM_STATE_SYSUI_REQ:

  case ALARM_STATE_SNOOZED:
  case ALARM_STATE_SERVED:
  case ALARM_STATE_RECURRING:
  case ALARM_STATE_DELETED:
  case ALARM_STATE_FINALIZED:
    current = state;
    break;

  case ALARM_STATE_SYSUI_ACK:
    switch( previous )
    {
    case ALARM_STATE_SYSUI_REQ:
      current = state;
      break;
    }
    break;

  case ALARM_STATE_SYSUI_RSP:
    switch( previous )
    {
    case ALARM_STATE_SYSUI_REQ:
    case ALARM_STATE_SYSUI_ACK:
      current = state;
      break;
    }
    break;
  }

  if( previous != current )
  {
    log_info("[%d] TRANS: %s -> %s\n",
             self->ALARMD_PRIVATE(cookie),
             queue_event_state_names[previous],
             queue_event_state_names[current]);

    if( (current == ALARM_STATE_NEW) || (current < previous) )
    {
      queue_set_dirty();
    }
  }
  else if( current != state )
  {
    log_warning("[%d] TRANS: %s -> %s - BLOCKED\n",
              self->ALARMD_PRIVATE(cookie),
              queue_event_state_names[previous],
              queue_event_state_names[state]);
  }
  else if( current == ALARM_STATE_NEW )
  {
    log_info("[%d] TRANS: %s -> %s\n",
             self->ALARMD_PRIVATE(cookie),
             queue_event_state_names[previous],
             queue_event_state_names[current]);
  }

  self->flags &= ALARM_EVENT_CLIENT_MASK;
  self->flags |= (current << ALARM_EVENT_CLIENT_BITS);
}

/* ========================================================================= *
 * QUEUE INTERFACE
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_add_event
 * ------------------------------------------------------------------------- */

cookie_t
queue_add_event(alarm_event_t *event)
{
  if( event->ALARMD_PRIVATE(cookie) == 0 )
  {
    event->ALARMD_PRIVATE(cookie) = ++queue_cookie;
  }
  else if( queue_cookie < event->ALARMD_PRIVATE(cookie) )
  {
    queue_cookie = event->ALARMD_PRIVATE(cookie);
  }

  queue_insert_event(event);

  return event->ALARMD_PRIVATE(cookie);
}

/* ------------------------------------------------------------------------- *
 * queue_get_event
 * ------------------------------------------------------------------------- */

alarm_event_t *
queue_get_event(cookie_t cookie)
{
  size_t l,h,i;

  for( l = 0, h = queue_count; l < h; )
  {
    alarm_event_t *event = queue_by_cookie[(i = (l+h)/2)];

    int r = queue_cmp_cookie(event->ALARMD_PRIVATE(cookie), cookie);

    if( r < 0 ) { l = i + 1; continue; }
    if( r > 0 ) { h = i + 0; continue; }

    return event;
  }
  return 0;
}

/* ------------------------------------------------------------------------- *
 * queue_del_event
 * ------------------------------------------------------------------------- */

int
queue_del_event(cookie_t cookie)
{
  int            error = -1;
  alarm_event_t *event = queue_get_event(cookie);

  if( event != 0 )
  {
    error = 0;
    queue_event_set_state(event, ALARM_STATE_DELETED);
  }
  return (error != -1);
}

/* ------------------------------------------------------------------------- *
 * queue_query_events
 * ------------------------------------------------------------------------- */

cookie_t *
queue_query_events(int *pcnt, time_t lo, time_t hi, unsigned mask, unsigned flag, const char *app)
{
  cookie_t *res = calloc(queue_count+1, sizeof *res);
  size_t    cnt = 0;

  if( hi <= 0 )
  {
    hi = INT_MAX;
  }
  if( lo <= 0 )
  {
    lo = INT_MIN;
  }

  for( size_t i = queue_count; i--; )
  {
    alarm_event_t *eve = queue_by_trigger[i];

    /* Because alarms are no longer removed from queue
     * immediately after "del_event" method call, we
     * need to filter them out from client query results */

    if( queue_event_get_state(eve) == ALARM_STATE_DELETED )
    {
      continue;
    }

    if( eve->ALARMD_PRIVATE(trigger) < lo ) continue;

    if( eve->ALARMD_PRIVATE(trigger) > hi ) break;

    if( (eve->flags & mask) != flag )
    {
      continue;
    }

    if( !xisempty(app) )
    {
      if( (eve->alarm_appid == 0) || strcmp(eve->alarm_appid, app) )
      {
        continue;
      }
    }

    res[cnt++] = eve->ALARMD_PRIVATE(cookie);
  }
  res[cnt] = 0;

  if( pcnt ) *pcnt = cnt;

  return res;
}

/* ------------------------------------------------------------------------- *
 * queue_query_by_state
 * ------------------------------------------------------------------------- */

cookie_t *
queue_query_by_state(int *pcnt, unsigned state)
{
  cookie_t *res = calloc(queue_count+1, sizeof *res);
  size_t    cnt = 0;

  for( size_t i = queue_count; i--; )
  {
    alarm_event_t *eve = queue_by_trigger[i];

    if( eve->flags & ALARM_EVENT_DISABLED )
    {
      continue;
    }

    if( queue_event_get_state(eve) == state )
    {
      res[cnt++] = eve->ALARMD_PRIVATE(cookie);
    }
  }
  res[cnt] = 0;

  if( pcnt ) *pcnt = cnt;

  return res;
}

/* ------------------------------------------------------------------------- *
 * queue_count_by_state
 * ------------------------------------------------------------------------- */

int
queue_count_by_state(unsigned state)
{
  int cnt = 0;

  for( size_t i = queue_count; i--; )
  {
    alarm_event_t *eve = queue_by_trigger[i];

    if( eve->flags & ALARM_EVENT_DISABLED )
    {
      continue;
    }

    if( queue_event_get_state(eve) == state )
    {
      cnt += 1;
    }
  }

  return cnt;
}

/* ------------------------------------------------------------------------- *
 * queue_count_by_state_and_flag
 * ------------------------------------------------------------------------- */

int
queue_count_by_state_and_flag   (unsigned state, unsigned flag)
{
  int cnt = 0;

  for( size_t i = queue_count; i--; )
  {
    alarm_event_t *eve = queue_by_trigger[i];

    if( eve->flags & ALARM_EVENT_DISABLED )
    {
      continue;
    }

    if( queue_event_get_state(eve) == state && eve->flags & flag )
    {
      cnt += 1;
    }
  }

  return cnt;
}

/* ------------------------------------------------------------------------- *
 * queue_cleanup_deleted
 * ------------------------------------------------------------------------- */

void
queue_cleanup_deleted(void)
{
  size_t t = 0;
  size_t c = 0;

  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_event_t *eve = queue_by_trigger[i];

    switch( queue_event_get_state(eve) )
    {
    case ALARM_STATE_DELETED:
    case ALARM_STATE_FINALIZED:
      //log_debug("T:%03Zd\t%ld\n", i, (long)eve->ALARMD_PRIVATE(cookie));
      queue_event_set_state(eve, ALARM_STATE_FINALIZED);
      break;

    default:
      queue_by_trigger[t++] = eve;
      break;
    }
  }

  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_event_t *eve = queue_by_cookie[i];
    switch( queue_event_get_state(eve) )
    {
    case ALARM_STATE_DELETED:
    case ALARM_STATE_FINALIZED:
      //log_debug("C:%03Zd\t%ld\n", i, (long)eve->ALARMD_PRIVATE(cookie));
      alarm_event_delete(eve);
      break;

    default:
      queue_by_cookie[c++] = eve;
      break;
    }
  }

  assert( c == t );

  queue_count = c;
}

/* ------------------------------------------------------------------------- *
 * queue_flush_events  --  delete all events without performing actions
 * ------------------------------------------------------------------------- */

static void
queue_flush_events(void)
{
  // delete events directly, without state
  // transitions and action execution
  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_event_delete(queue_by_cookie[i]);
  }

  // free event tables
  free(queue_by_cookie);
  free(queue_by_trigger);

  // clear related values
  queue_by_cookie  = 0;
  queue_by_trigger = 0;
  queue_count      = 0;
  queue_alloc      = 0;
}

/* ========================================================================= *
 * persistent storage functionality
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_save_to_memory
 * ------------------------------------------------------------------------- */

static
int
queue_save_to_memory(char **pdata, size_t *psize)
{
  int        err = -1;
  inifile_t *ini = inifile_create();

  inifile_setfmt(ini, "config", "snooze", "%u", queue_snooze);

  auto void xu(unsigned           v, const char *s, const char *f, ...);
  auto void xq(unsigned long long v, const char *s, const char *f, ...);
  auto void xi(int                v, const char *s, const char *f, ...);
  auto void xs(const char        *v, const char *s, const char *f, ...);

  auto void xu(unsigned v, const char *s, const char *f, ...)
  {
    if( v != 0 )
    {
      va_list va;
      char k[64];
      va_start(va, f);
      vsnprintf(k, sizeof k, f, va);
      va_end(va);
      inifile_setfmt(ini, s, k, "%u", v);
    }
  }

  auto void xq(unsigned long long v, const char *s, const char *f, ...)
  {
    if( v != 0 )
    {
      va_list va;
      char k[64];
      va_start(va, f);
      vsnprintf(k, sizeof k, f, va);
      va_end(va);
      inifile_setfmt(ini, s, k, "%llu", v);
    }
  }
  auto void xi(int v, const char *s, const char *f, ...)
  {
    if( v != 0 )
    {
      va_list va;
      char k[64];
      va_start(va, f);
      vsnprintf(k, sizeof k, f, va);
      va_end(va);
      inifile_setfmt(ini, s, k, "%d", v);
    }
  }
  auto void xs(const char *v, const char *s, const char *f, ...)
  {
    if( v && *v )
    {
      va_list va;
      char k[64];
      va_start(va, f);
      vsnprintf(k, sizeof k, f, va);
      va_end(va);
      inifile_setfmt(ini, s, k, "%s", v);
    }
  }

  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_event_t *e = queue_by_cookie[i];

    char sec[32];

    snprintf(sec, sizeof sec, "#%08x", (unsigned)e->ALARMD_PRIVATE(cookie));

#define Xu2(n,v) xu(e->v, sec, #n)
#define Xi2(n,v) xi(e->v, sec, #n)
#define Xs2(n,v) xs(e->v, sec, #n)

#define Xu(v) Xu2(v,v)
#define Xi(v) Xi2(v,v)
#define Xs(v) Xs2(v,v)

    Xu2(cookie,  ALARMD_PRIVATE(cookie));
    Xi2(trigger, ALARMD_PRIVATE(trigger));

    Xs(title);
    Xs(message);
    Xs(sound);
    Xs(icon);
    Xu(flags);

    Xs(alarm_appid);

    Xi(alarm_time);

    Xi(alarm_tm.tm_year);
    Xi(alarm_tm.tm_mon);
    Xi(alarm_tm.tm_mday);
    Xi(alarm_tm.tm_hour);
    Xi(alarm_tm.tm_min);
    Xi(alarm_tm.tm_sec);
    Xi(alarm_tm.tm_wday);
    Xi(alarm_tm.tm_yday);
    Xi(alarm_tm.tm_isdst);

    Xs(alarm_tz);

    Xi(recur_secs);
    Xi(recur_count);

    Xi(snooze_secs);
    Xi(snooze_total);

    Xu(action_cnt);
    Xu(recurrence_cnt);
    Xu(attr_cnt);

#undef Xu
#undef Xi
#undef Xs

#undef Xu2
#undef Xi2
#undef Xs2

    for( size_t i = 0; i < e->action_cnt; ++i )
    {
      alarm_action_t  *a= &e->action_tab[i];

#define Xu(v) xu(a->v, sec, "action%d."#v, (int)i)
#define Xs(v) xs(a->v, sec, "action%d."#v, (int)i)

      Xu(flags);
      Xs(label);
      Xs(exec_command);
      Xs(dbus_interface);
      Xs(dbus_service);
      Xs(dbus_path);
      Xs(dbus_name);
      Xs(dbus_args);

#undef Xu
#undef Xs
    }

    for( size_t i = 0; i < e->recurrence_cnt; ++i )
    {
      alarm_recur_t  *r= &e->recurrence_tab[i];

#define Xu(v) xu(r->v, sec, "recurrence_tab%d."#v, (int)i)
#define Xq(v) xq(r->v, sec, "recurrence_tab%d."#v, (int)i)

      Xq(mask_min);
      Xu(mask_hour);
      Xu(mask_mday);
      Xu(mask_wday);
      Xu(mask_mon);
      Xu(special);

#undef Xu
#undef Xq
    }

    for( size_t i = 0; i < e->attr_cnt; ++i )
    {
      alarm_attr_t  *a= e->attr_tab[i];

#define Xi(v) xi(a->v, sec, "attr%d."#v, (int)i)
#define Xs(v) xs(a->v, sec, "attr%d."#v, (int)i)

      Xs(attr_name);
      Xi(attr_type);
      switch( a->attr_type )
      {
      case ALARM_ATTR_NULL:
        break;
      case ALARM_ATTR_INT:
        Xi(attr_data.ival);
        break;
      case ALARM_ATTR_TIME:
        Xi(attr_data.tval);
        break;
      case ALARM_ATTR_STRING:
        Xs(attr_data.sval);
        break;
      }
#undef Xi
#undef Xs
    }

  }

  err = inifile_save_to_memory(ini, pdata, psize);
  inifile_delete(ini);

  return err;
}

/* ------------------------------------------------------------------------- *
 * queue_load_from_path
 * ------------------------------------------------------------------------- */

static
void
queue_load_from_path(const char *path)
{
  char      **secs = 0;
  inifile_t  *ini  = inifile_create();
  unsigned    snooze = 0;

  if( inifile_load(ini, path) == -1 )
  {
    log_warning("%s: load failed\n", path);
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * parse data from save file
   * - - - - - - - - - - - - - - - - - - - */

  inifile_getfmt(ini, "config", "snooze", "%u", &snooze);
  queue_set_snooze(snooze);

  if( (secs = inifile_get_section_names(ini, 0)) != 0 )
  {
    for( size_t i = 0; secs[i]; ++i )
    {
      const char *sec = secs[i];

      if( *sec != '#' ) continue;

      alarm_event_t *e = alarm_event_create();

      size_t cnt;

      if( inifile_getfmt(ini, sec, "action_cnt", "%zi", &cnt) == 1 )
      {
        alarm_event_add_actions(e, cnt);
      }
      if( inifile_getfmt(ini, sec, "recurrence_cnt", "%zi", &cnt) == 1 )
      {
        alarm_event_add_recurrences(e, cnt);
      }

#define Xu2(n,v) e->v = strtoul(inifile_get(ini, sec, #n, ""),0,0)
#define Xi2(n,v) e->v = strtol(inifile_get(ini, sec, #n, ""),0,0)
#define Xs2(n,v) xstrset(&(e->v), inifile_get(ini, sec, #n, ""))

#define Xu(v) Xu2(v,v)
#define Xi(v) Xi2(v,v)
#define Xs(v) Xs2(v,v)

      Xu2(cookie,  ALARMD_PRIVATE(cookie));
      Xi2(trigger, ALARMD_PRIVATE(trigger));

      Xs(title);
      Xs(message);
      Xs(sound);
      Xs(icon);
      Xu(flags);

      Xs(alarm_appid);

      Xi(alarm_time);

      Xi(alarm_tm.tm_year);
      Xi(alarm_tm.tm_mon);
      Xi(alarm_tm.tm_mday);
      Xi(alarm_tm.tm_hour);
      Xi(alarm_tm.tm_min);
      Xi(alarm_tm.tm_sec);
      Xi(alarm_tm.tm_wday);
      Xi(alarm_tm.tm_yday);
      Xi(alarm_tm.tm_isdst);

      Xs(alarm_tz);

      Xi(recur_secs);
      Xi(recur_count);

      Xi(snooze_secs);
      Xi(snooze_total);

#undef Xu
#undef Xi
#undef Xs

#undef Xu2
#undef Xi2
#undef Xs2

      /* - - - - - - - - - - - - - - - - - - - *
       * action table
       * - - - - - - - - - - - - - - - - - - - */

      for( size_t k = 0; k < e->action_cnt; ++k )
      {
        alarm_action_t  *a = &e->action_tab[k];
        char key[64];

#define Xu(v) \
  snprintf(key, sizeof key, "action%d.%s", (int)k, #v);\
  a->v = strtoul(inifile_get(ini, sec, key, ""),0,0)

#define Xs(v)\
  snprintf(key, sizeof key, "action%d.%s", (int)k, #v);\
  xstrset(&a->v, inifile_get(ini, sec, key, ""))

        Xu(flags);
        Xs(label);
        Xs(exec_command);
        Xs(dbus_interface);
        Xs(dbus_service);
        Xs(dbus_path);
        Xs(dbus_name);
        Xs(dbus_args);

#undef Xu
#undef Xs
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * recurrence table
       * - - - - - - - - - - - - - - - - - - - */

      for( size_t k = 0; k < e->recurrence_cnt; ++k )
      {
        alarm_recur_t  *r = &e->recurrence_tab[k];
        char key[64];

#define Xu(v) \
  snprintf(key, sizeof key, "recurrence_tab%d.%s", (int)k, #v);\
  r->v = strtoul(inifile_get(ini, sec, key,  ""),0,0)

#define Xq(v)\
  snprintf(key, sizeof key, "recurrence_tab%d.%s", (int)k, #v);\
  r->v = strtoull(inifile_get(ini, sec, key,  ""),0,0)

        Xq(mask_min);
        Xu(mask_hour);
        Xu(mask_mday);
        Xu(mask_wday);
        Xu(mask_mon);
        Xu(special);

#undef Xu
#undef Xq
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * attribute table
       * - - - - - - - - - - - - - - - - - - - */

      if( inifile_getfmt(ini, sec, "attr_cnt", "%zi", &cnt) != 1 )
      {
        cnt = 0;
      }

      for( size_t k = 0; k < cnt; ++k )
      {
        alarm_attr_t  *a = alarm_event_add_attr(e, "\x7f");
        char key[64];

#define Xi(v) \
  snprintf(key, sizeof key, "attr%d.%s", (int)k, #v);\
  a->v = strtol(inifile_get(ini, sec, key, ""),0,0)

#define Xs(v)\
  snprintf(key, sizeof key, "attr%d.%s", (int)k, #v);\
  xstrset(&a->v, inifile_get(ini, sec, key, ""))

        Xs(attr_name);
        Xi(attr_type);

        switch( a->attr_type )
        {
        case ALARM_ATTR_NULL:
          break;
        case ALARM_ATTR_INT:
          Xi(attr_data.ival);
          break;
        case ALARM_ATTR_TIME:
          Xi(attr_data.tval);
          break;
        case ALARM_ATTR_STRING:
          Xs(attr_data.sval);
          break;
        }
#undef Xi
#undef Xs
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * add to queue
       * - - - - - - - - - - - - - - - - - - - */

      switch( queue_event_get_state(e) )
      {
      case ALARM_STATE_LIMBO:
      case ALARM_STATE_TRIGGERED:
      case ALARM_STATE_WAITSYSUI:
      case ALARM_STATE_SYSUI_REQ:
      case ALARM_STATE_SYSUI_ACK:
      case ALARM_STATE_SYSUI_RSP:
        // put alarms that were in triggered state
        // back to limbo so that we have a chance
        // to evaluate conditions and perform actions
        // again
        queue_event_set_state(e, ALARM_STATE_LIMBO);
        break;

      default:
        queue_event_set_state(e, ALARM_STATE_NEW);
        break;
      }

#if 0 // disabled: rejecting events on load might cause regression
      if( alarm_event_is_sane(e) != -1 )
      {
        queue_add_event(e), e = 0;
      }
      alarm_event_delete(e);
#else
      //alarm_event_is_sane(e);
      queue_add_event(e);
#endif
    }
  }

  cleanup:

  xfreev(secs);
  inifile_delete(ini);
}

/* ------------------------------------------------------------------------- *
 * queue_save_internal
 * ------------------------------------------------------------------------- */

static
void
queue_save_internal(int forced)
{
  static int restored = 0;

  static int skip_cnt = 0;
  static int save_cnt = 0;
  static int fail_cnt = 0;

  int        result   = -1;
  char      *old_data = 0;
  char      *new_data = 0;
  size_t     old_size = 0;
  size_t     new_size = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * try to deal with osso-backup restoring
   * the database without stopping or
   * signaling alarmd first ...
   * - - - - - - - - - - - - - - - - - - - */

  if( forced != 0 )
  {
    restored = 0;
    xfetchstats(QUEUE_DATABASE, &queue_save_stat);
  }
  else
  {
    if( restored != 0 )
    {
      log_warning("queue save blocked while waiting for device restart\n");
      goto cleanup;
    }

    if( !xcheckstats(QUEUE_DATABASE, &queue_save_stat) )
    {
      log_critical("ALARM DB CHANGED SINCE LAST SAVE"
                   " - assuming restore from backup\n");

      restored = 1;
      queue_indicate_modified();

      goto cleanup;
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * try really hard to avoid writing to
   * flash file system
   *
   * 1) load previous contents to RAM
   * 2) generate current contents in RAM
   * 3) write only if the two differ
   * - - - - - - - - - - - - - - - - - - - */

  xloadfile(QUEUE_DATABASE, &old_data, &old_size);
  queue_save_to_memory(&new_data, &new_size);

  if( old_size != new_size || memcmp(old_data, new_data, new_size) )
  {
    if( xsavefile(QUEUE_TEMPSAVE, 0666, new_data, new_size) == 0 &&
        xcyclefiles(QUEUE_TEMPSAVE, QUEUE_DATABASE, QUEUE_BACKUP) == 0 )
    {
      result = 1;
    }
  }
  else
  {
    result = 0;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * update "current content" stats
   * - - - - - - - - - - - - - - - - - - - */

  xfetchstats(QUEUE_DATABASE, &queue_save_stat);

  cleanup:

  /* - - - - - - - - - - - - - - - - - - - *
   * update & log statistics
   * - - - - - - - - - - - - - - - - - - - */

  switch( result )
  {
  case 1:  save_cnt += 1; break;
  case 0:  skip_cnt += 1; break;
  default: fail_cnt += 1; break;
  }

  log_info("queue save: %s -> saved=%d, skipped=%d, failed=%d\n",
           (result==0) ? "SKIP" : (result==1) ? "SAVE" : "FAIL",
           save_cnt, skip_cnt, fail_cnt);

  /* - - - - - - - - - - - - - - - - - - - *
   * release buffers
   * - - - - - - - - - - - - - - - - - - - */

  free(old_data);
  free(new_data);

}

/* ------------------------------------------------------------------------- *
 * queue_save
 * ------------------------------------------------------------------------- */

void
queue_save(void)
{
  queue_save_internal(0);
}

/* ------------------------------------------------------------------------- *
 * queue_save_forced
 * ------------------------------------------------------------------------- */

void
queue_save_forced(void)
{
  queue_save_internal(1);
}

/* ------------------------------------------------------------------------- *
 * queue_load
 * ------------------------------------------------------------------------- */

void
queue_load(void)
{
  // load candidates
  static const char * const order[] =
  {
    QUEUE_DATABASE,
    QUEUE_BACKUP,
    QUEUE_TEMPSAVE,
    NULL
  };

  // the actual queue file was loaded
  int first = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * try loading the queue file from the
   * list of candidates
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; order[i] != 0; ++i )
  {
    if( access(order[i], F_OK) == 0 )
    {
      queue_load_from_path(order[i]);
      first = (i == 0);
      break;
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * update "current content" stats
   * - - - - - - - - - - - - - - - - - - - */

  xfetchstats(*order, &queue_save_stat);

  /* - - - - - - - - - - - - - - - - - - - *
   * save if loaded from something else
   * than actual queue file
   * - - - - - - - - - - - - - - - - - - - */

  if( !first )
  {
    queue_save();
  }
}

/* ========================================================================= *
 * DIRTY STATE CONTROL
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_set_dirty
 * ------------------------------------------------------------------------- */

void
queue_set_dirty(void)
{
  queue_dirty = 1;
}

/* ------------------------------------------------------------------------- *
 * queue_clr_dirty
 * ------------------------------------------------------------------------- */

void
queue_clr_dirty(void)
{
  queue_dirty = 0;
}

/* ------------------------------------------------------------------------- *
 * queue_is_dirty
 * ------------------------------------------------------------------------- */

int
queue_is_dirty(void)
{
  return queue_dirty;
}

/* ========================================================================= *
 * START / STOP COMPONENT
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_init
 * ------------------------------------------------------------------------- */

int
queue_init(void)
{
  queue_load();
  return 0;
}

/* ------------------------------------------------------------------------- *
 * queue_quit
 * ------------------------------------------------------------------------- */

void
queue_quit(void)
{
  queue_save();
  queue_flush_events();
}
