#include "alarmd_config.h"

#include "queue.h"
#include "logging.h"
#include "inifile.h"
#include "ticker.h"

#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if 0
#include <assert.h>
#else
#define assert(e) do{\
  if(!(e)){\
    fprintf(stderr, "%s:%d: ASSERT %s\n",\
              __FILE__,__LINE__,#e);\
    *(int *)-1 = 0xdeadbeef;\
  }\
}while(0)

#endif

#define QUEUE_DATABASE    ALARMD_CONFIG_CACHEDIR"/alarm_queue.ini"
#define QUEUE_BACKUP      QUEUE_DATABASE".bak"
#define QUEUE_TEMPSAVE    QUEUE_DATABASE".tmp"

#define QUEUE_SNOOZE_DEFAULT   (10 * 60) // 10 minutes

const char * const queue_event_state_names[] =
{
#define ALARM_STATE(name) #name,
#include "states.inc"
};

/* ========================================================================= *
 * INDICATION CALLBACKS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_restore_cb  --  called if queue file modification detected
 * ------------------------------------------------------------------------- */

static void (*queue_restore_cb)(void) = 0;

/* ------------------------------------------------------------------------- *
 * queue_restore_indication
 * ------------------------------------------------------------------------- */

static void queue_restore_indication(void)
{
  if( queue_restore_cb != 0 )
  {
    queue_restore_cb();
  }
}

/* ------------------------------------------------------------------------- *
 * queue_set_restore_cb
 * ------------------------------------------------------------------------- */

void queue_set_restore_cb(void (*cb)(void))
{
  queue_restore_cb = cb;
}

/* ========================================================================= *
 * UTILITY FUNCTIONS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * file exists
 * ------------------------------------------------------------------------- */

static
int
exists(const char *path)
{
  return access(path, F_OK) == 0;
}

/* ------------------------------------------------------------------------- *
 * load_file
 * ------------------------------------------------------------------------- */

static
int
load_file(const char *path, char **pdata, size_t *psize)
{
  int     err  = -1;
  int     file = -1;
  size_t  size = 0;
  char   *data = 0;

  struct stat st;

  if( (file = open(path, O_RDONLY)) == -1 )
  {
    log_warning_F("%s: open: %s\n", path, strerror(errno));
    goto cleanup;
  }

  if( fstat(file, &st) == -1 )
  {
    log_warning_F("%s: stat: %s\n", path, strerror(errno));
    goto cleanup;
  }

  size = st.st_size;

  // calloc size+1 -> text files are zero terminated

  if( (data = calloc(size+1, 1)) == 0 )
  {
    log_warning_F("%s: calloc %d: %s\n", path, size, strerror(errno));
    goto cleanup;
  }

  errno = 0;
  if( read(file, data, size) != size )
  {
    log_warning_F("%s: read: %s\n", path, strerror(errno));
    goto cleanup;
  }

  err = 0;

  cleanup:

  if( err  !=  0 ) free(data), data = 0, size = 0;
  if( file != -1 ) close(file);

  free(*pdata), *pdata = data, *psize = size;

  return err;
}

/* ------------------------------------------------------------------------- *
 * save_file
 * ------------------------------------------------------------------------- */

static
int
save_file(const char *path, int mode, const void *data, size_t size)
{
  int     err  = -1;
  int     file = -1;

  if( (file = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1 )
  {
    log_error_F("%s: open: %s\n", path, strerror(errno));
    goto cleanup;
  }

  errno = 0;
  if( write(file, data, size) != size )
  {
    log_error_F("%s: write: %s\n", path, strerror(errno));
    goto cleanup;
  }

  if( fsync(file) == -1 )
  {
    log_error_F("%s: sync: %s\n", path, strerror(errno));
    goto cleanup;
  }

  err = 0;

  cleanup:

  if( file != -1 && close(file) == -1 )
  {
    log_error_F("%s: close: %s\n", path, strerror(errno));
    err = -1;
  }

  return err;
}

/* ------------------------------------------------------------------------- *
 * cycle_files
 * ------------------------------------------------------------------------- */

static int
cycle_files(const char *temp, const char *path, const char *back)
{
  /* - - - - - - - - - - - - - - - - - - - *
   * not foolproof, but certainly better
   * than directly overwriting datafiles
   * - - - - - - - - - - - - - - - - - - - */

  int err = -1;

  if( !exists(temp) )
  {
    // temporary file does not exist == wtf
    perror(temp);
  }
  else
  {
    // 1. remove backup

    remove(back);

    // 2. previous -> backup

    if( exists(path) )
    {
      if( rename(path, back) == -1 )
      {
        log_error("rename %s -> %s: %s\n", path, back, strerror(errno));
        goto done;
      }
    }

    // 3. temporary -> current

    if( rename(temp, path) == -1 )
    {
      log_error("rename %s -> %s: %s\n", temp, path, strerror(errno));

      if( exists(back) )
      {
        // rollback
        rename(back, path);
      }
      goto done;
    }

    // success
    err = 0;
  }

  done:

  return err;
}

/* ------------------------------------------------------------------------- *
 * compare operators
 * ------------------------------------------------------------------------- */

#define CMP(a,b) (((a)>(b))-((a)<(b)))

static int
cmp_cookie(cookie_t a, cookie_t b)
{
  return CMP(a,b);
}
static int
cmp_trigger(time_t a, time_t b)
{
  return CMP(a,b);
}
static int
cmp_event_cookie(const alarm_event_t *a, const alarm_event_t *b)
{
  // cookie table in ascending order: oldest first

  //log_debug("CMP COOKIE:  %d - %d\n", (int)a->cookie, (int)b->cookie);
  return cmp_cookie(a->cookie, b->cookie);
}
static int
cmp_event_trigger(const alarm_event_t *a, const alarm_event_t *b)
{
  // trigger table in descending order:
  // - next to trigger last
  // - "oldest" (by cookie) at the same time last

  //log_debug("CMP TRIGGER: %d - %d\n", (int)a->trigger, (int)b->trigger);
  return cmp_trigger(b->trigger, a->trigger) ?: cmp_event_cookie(b,a);
}

#undef CMP

/* ========================================================================= *
 * INTERNAL STATE DATA
 * ========================================================================= */

static unsigned        queue_snooze = QUEUE_SNOOZE_DEFAULT;
static cookie_t        queue_cookie = 0;
static int             queue_dirty  = 0;

/* ascending cookie sort: older alarms first */
static alarm_event_t **queue_by_cookie  = 0;

/* descending trigger sort: the first to trigger is the last in the queue
 * -> updating trigger values (due to snooze etc) is more likely to affect
 *    only the tail of the queue */
static alarm_event_t **queue_by_trigger = 0;

static size_t          queue_count      = 0;
static size_t          queue_alloc      = 0;

/* stats of the queue file - used for detecting whend somebody
 * else than alarmd has modified the queue file since the last
 * load / save operation (mainly restoring backed up alarms). */
static struct stat     queue_save_stat;

/* ========================================================================= *
 * INTERNAL FUNCTIONALITY
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * queue_save_to_memory
 * ------------------------------------------------------------------------- */

// queue_save_to_path

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

    snprintf(sec, sizeof sec, "#%08x", (unsigned)e->cookie);

#define Xu(v) xu(e->v, sec, #v)
#define Xi(v) xi(e->v, sec, #v)
#define Xs(v) xs(e->v, sec, #v)

    Xu(cookie);
    Xi(trigger);

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
    log_warning("%s: load failed", path);
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

#define Xu(v) e->v = strtoul(inifile_get(ini, sec, #v, ""),0,0)
#define Xi(v) e->v = strtol(inifile_get(ini, sec, #v, ""),0,0)
#define Xs(v) xstrset(&(e->v), inifile_get(ini, sec, #v, ""))

      Xu(cookie);
      Xi(trigger);

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
  snprintf(key, sizeof key, "action%d.%s", (int)k, #v);\
  a->v = strtol(inifile_get(ini, sec, key, ""),0,0)

#define Xs(v)\
  snprintf(key, sizeof key, "action%d.%s", (int)k, #v);\
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

      switch( queue_get_event_state(e) )
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
        queue_set_event_state(e, ALARM_STATE_LIMBO);
        break;

      default:
        queue_set_event_state(e, ALARM_STATE_NEW);
        break;
      }
      queue_add_event(e);
    }
  }

  cleanup:

  xfreev(secs);
  inifile_delete(ini);
}

/* ------------------------------------------------------------------------- *
 * queue_insert_event
 * ------------------------------------------------------------------------- */

static size_t queue_slot_by_trigger(alarm_event_t *eve)
{
  size_t l,h,i;

  for( l = 0, h = queue_count; l < h; )
  {
    if( cmp_event_trigger(queue_by_trigger[(i = (l+h)/2)], eve) < 0 )
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

static size_t queue_slot_by_cookie(alarm_event_t *eve)
{
  size_t l,h,i;

  for( l = 0, h = queue_count; l < h; )
  {
    if( cmp_event_cookie(queue_by_cookie[(i = (l+h)/2)], eve) < 0 )
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

  size_t t = queue_slot_by_trigger(eve);
  size_t c = queue_slot_by_cookie(eve);

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

/* ------------------------------------------------------------------------- *
 * queue_remove_event
 * ------------------------------------------------------------------------- */

// QUARANTINE static
// QUARANTINE void
// QUARANTINE queue_remove_event(alarm_event_t *eve)
// QUARANTINE {
// QUARANTINE   size_t t = queue_slot_by_trigger(eve);
// QUARANTINE   size_t c = queue_slot_by_cookie(eve);
// QUARANTINE
// QUARANTINE   assert( (t < queue_count) && (queue_by_trigger[t] == eve) );
// QUARANTINE   assert( (c < queue_count) && (queue_by_cookie[c]  == eve) );
// QUARANTINE
// QUARANTINE   queue_count -= 1;
// QUARANTINE
// QUARANTINE   for( size_t i = t; i < queue_count; ++i )
// QUARANTINE   {
// QUARANTINE     queue_by_trigger[i] = queue_by_trigger[i+1];
// QUARANTINE   }
// QUARANTINE
// QUARANTINE   for( size_t i = c; i < queue_count; ++i )
// QUARANTINE   {
// QUARANTINE     queue_by_cookie[i] = queue_by_cookie[i+1];
// QUARANTINE   }
// QUARANTINE   queue_by_trigger[queue_count] = 0;
// QUARANTINE   queue_by_cookie[queue_count] = 0;
// QUARANTINE }

/* ========================================================================= *
 * ALARM_QUEUE API
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

/* ------------------------------------------------------------------------- *
 * queue_add_event
 * ------------------------------------------------------------------------- */

cookie_t
queue_add_event(alarm_event_t *event)
{
  if( event->cookie == 0 )
  {
    event->cookie = ++queue_cookie;
  }
  else if( queue_cookie < event->cookie )
  {
    queue_cookie = event->cookie;
  }

  queue_insert_event(event);

  return event->cookie;
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

    int r = cmp_cookie(event->cookie, cookie);

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
    queue_set_event_state(event, ALARM_STATE_DELETED);
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

  //for( size_t i = 0; i < queue_count; ++i )
  for( size_t i = queue_count; i--; )
  {
    alarm_event_t *eve = queue_by_trigger[i];

    if( eve->trigger < lo ) continue;

    if( eve->trigger > hi ) break;

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

    res[cnt++] = eve->cookie;
  }
  res[cnt] = 0;

  if( pcnt ) *pcnt = cnt;

  return res;
}

/* ------------------------------------------------------------------------- *
 * queue_get_event_state
 * ------------------------------------------------------------------------- */

unsigned
queue_get_event_state(const alarm_event_t *self)
{
  return self->flags >> ALARM_EVENT_CLIENT_BITS;
}

/* ------------------------------------------------------------------------- *
 * queue_set_event_state
 * ------------------------------------------------------------------------- */

void
queue_set_event_state(alarm_event_t *self, unsigned state)
{
  unsigned previous = queue_get_event_state(self);
  unsigned current  = previous;

  /* - - - - - - - - - - - - - - - - - - - *
   * do some filtering for state transitions
   * occuring due to external events
   * - - - - - - - - - - - - - - - - - - - */

  switch( state )
  {
  default:
    log_debug("UNKNOWN TRANSITION: %s -> %s\n",
              queue_event_state_names[previous],
              queue_event_state_names[state]);
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
    log_debug("TRANS [%ld] %s -> %s\n",
              self->cookie,
            queue_event_state_names[previous],
            queue_event_state_names[current]);

    if( (current == ALARM_STATE_NEW) || (current < previous) )
    {
      queue_set_dirty();
    }
  }
  else if( current != state )
  {
    log_debug("TRANS [%ld] %s -> %s - BLOCKED\n",
              self->cookie,
              queue_event_state_names[previous],
              queue_event_state_names[state]);
  }

  self->flags &= ALARM_EVENT_CLIENT_MASK;
  self->flags |= (current << ALARM_EVENT_CLIENT_BITS);
}

/* ------------------------------------------------------------------------- *
 * queue_query_by_state
 * ------------------------------------------------------------------------- */

cookie_t *
queue_query_by_state(int *pcnt, unsigned state)
{
  cookie_t *res = calloc(queue_count+1, sizeof *res);
  size_t    cnt = 0;

  //for( size_t i = 0; i < queue_count; ++i )
  for( size_t i = queue_count; i--; )
  {
    alarm_event_t *eve = queue_by_trigger[i];

    if( eve->flags & ALARM_EVENT_DISABLED )
    {
      continue;
    }

    if( queue_get_event_state(eve) == state )
    {
      res[cnt++] = eve->cookie;
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

    if( queue_get_event_state(eve) == state )
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

    switch( queue_get_event_state(eve) )
    {
    case ALARM_STATE_DELETED:
    case ALARM_STATE_FINALIZED:
      //log_debug("T:%03Zd\t%ld\n", i, (long)eve->cookie);
      queue_set_event_state(eve, ALARM_STATE_FINALIZED);
      break;

    default:
      queue_by_trigger[t++] = eve;
      break;
    }
  }

  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_event_t *eve = queue_by_cookie[i];
    switch( queue_get_event_state(eve) )
    {
    case ALARM_STATE_DELETED:
    case ALARM_STATE_FINALIZED:
      //log_debug("C:%03Zd\t%ld\n", i, (long)eve->cookie);
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
 * queue_set_event_trigger
 * ------------------------------------------------------------------------- */

void
queue_set_event_trigger(alarm_event_t *event, time_t trigger)
{
#if 01

  log_debug("!!! RESET TRIGGER -> %+d secs\n", (int)(trigger - ticker_get_time()));

  size_t ti, to;

  /* - - - - - - - - - - - - - - - - - - - *
   * get slot indices for old and updated
   * trigger values
   * - - - - - - - - - - - - - - - - - - - */

  alarm_event_t temp = {
    .cookie = event->cookie,
    .trigger = trigger,
  };

  ti = queue_slot_by_trigger(event);
  to = queue_slot_by_trigger(&temp);

  event->trigger = trigger;

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
#else
  size_t si = 0;
  size_t di = 0;

  for( si = di = 0; si < queue_count; ++si )
  {
    alarm_event_t *e = queue_by_trigger[si];

    if( e == event )
    {
      continue;
    }

    if( event && cmp_event_trigger(event, e) < 0 )
    {
      queue_by_trigger[di++] = event, event = 0;
    }
    queue_by_trigger[di++] = e;
  }
  if( event )
  {
    queue_by_trigger[di++] = event, event = 0;
  }
  assert( di == queue_count );
#endif

#ifndef NDEBUG
// QUARANTINE   log_debug("-- beg sanity --\n");
// QUARANTINE   for( size_t i = 0; i < queue_count; ++i )
// QUARANTINE   {
// QUARANTINE     alarm_event_t *e = queue_by_trigger[i];
// QUARANTINE     log_debug("[%3d] T:%08x C:%08x\n", i, (int)e->trigger, (int)e->cookie);
// QUARANTINE   }
// QUARANTINE   // do sanity checks if asserts are enabled
// QUARANTINE   for( size_t i = 1; i < queue_count; ++i )
// QUARANTINE   {
// QUARANTINE     alarm_event_t *a = queue_by_trigger[i-1];
// QUARANTINE     alarm_event_t *b = queue_by_trigger[i-0];
// QUARANTINE     assert( a->cookie != b->cookie );
// QUARANTINE     assert( a != b );
// QUARANTINE     assert( cmp_event_trigger(a,b) < 0 );
// QUARANTINE   }
// QUARANTINE   log_debug("-- end sanity --\n");
#endif
  queue_set_dirty();
}

/* ------------------------------------------------------------------------- *
 * helpers for detecting file modifications by osso-backup -restore
 * ------------------------------------------------------------------------- */

static void fetch_stats(const char *path, struct stat *cur)
{
  if( stat(path, cur) == -1 )
  {
    memset(cur, 0, sizeof *cur);
  }
}

static int check_stats(const char *path, const struct stat *old)
{
  int ok = 1;

  struct stat cur;

  fetch_stats(path, &cur);

  if( old->st_dev   != cur.st_dev  ||
      old->st_ino   != cur.st_ino  ||
      old->st_size  != cur.st_size ||
      old->st_mtime != cur.st_mtime )
  {
    ok = 0;
  }

  return ok;
}

/* ------------------------------------------------------------------------- *
 * queue_flush
 * ------------------------------------------------------------------------- */

static void
queue_flush(void)
{
  for( size_t i = 0; i < queue_count; ++i )
  {
    alarm_event_delete(queue_by_cookie[i]);
  }
  free(queue_by_cookie);
  free(queue_by_trigger);

  queue_by_cookie = 0;
  queue_by_trigger = 0;
  queue_count = 0;
  queue_alloc = 0;
}

/* ------------------------------------------------------------------------- *
 * queue_save
 * ------------------------------------------------------------------------- */

void
queue_save(void)
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

  if( restored != 0 )
  {
    log_warning("queue save blocked while waiting for device restart\n");
    goto cleanup;
  }

  if( !check_stats(QUEUE_DATABASE, &queue_save_stat) )
  {
    log_critical("ALARM DB CHANGED SINCE LAST SAVE"
                 " - assuming restore from backup\n");

    restored = 1;
    queue_restore_indication();

    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * try really hard to avoid writing to
   * flash file system
   *
   * 1) load previous contents to RAM
   * 2) generate current contents in RAM
   * 3) write only if the two differ
   * - - - - - - - - - - - - - - - - - - - */

  load_file(QUEUE_DATABASE, &old_data, &old_size);
  queue_save_to_memory(&new_data, &new_size);

  if( old_size != new_size || memcmp(old_data, new_data, new_size) )
  {
    if( save_file(QUEUE_TEMPSAVE, 0666, new_data, new_size) == 0 &&
        cycle_files(QUEUE_TEMPSAVE, QUEUE_DATABASE, QUEUE_BACKUP) == 0 )
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

  fetch_stats(QUEUE_DATABASE, &queue_save_stat);

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

  log_debug("SAVE ALARM QUEUE: %d -> save=%d, skip=%d, fail=%d\n",
            result, save_cnt, skip_cnt, fail_cnt);

  /* - - - - - - - - - - - - - - - - - - - *
   * release buffers
   * - - - - - - - - - - - - - - - - - - - */

  free(old_data);
  free(new_data);

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

  fetch_stats(*order, &queue_save_stat);

  /* - - - - - - - - - - - - - - - - - - - *
   * save if loaded from something else
   * than actual queue file
   * - - - - - - - - - - - - - - - - - - - */

  if( !first )
  {
    queue_save();
  }
}

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
  queue_flush();
}

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
