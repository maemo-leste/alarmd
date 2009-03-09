#include "libalarm.h"

#include "logging.h"
#include "xutil.h"
#include "ticker.h"

#include <stdio.h>

/* ========================================================================= *
 * alarm_event_t  --  methods
 * ========================================================================= */

static const struct tm tm_initializer =
{
  .tm_sec   =  0,
  .tm_min   = -1,
  .tm_hour  = -1,

  .tm_mday  = -1,
  .tm_mon   = -1,
  .tm_year  = -1,

  .tm_wday  = -1,
  .tm_yday  = -1,
  .tm_isdst = -1,
};

/* ------------------------------------------------------------------------- *
 * alarm_event_ctor
 * ------------------------------------------------------------------------- */

void
alarm_event_ctor(alarm_event_t *self)
{
  self->cookie         = 0;
  self->trigger        = 0;

  self->title          = 0;
  self->message        = 0;
  self->sound          = 0;
  self->icon           = 0;
  self->flags          = 0;

  self->alarm_appid    = 0;

  self->alarm_time     = -1;
  self->alarm_tm       = tm_initializer;
  self->alarm_tz       = 0;

  self->recur_secs     = 0;
  self->recur_count    = 0;

  self->snooze_secs    = 0;
  self->snooze_total   = 0;

  self->response       = -1;
  self->action_cnt     = 0;
  self->action_tab     = 0;

  self->recurrence_cnt = 0;
  self->recurrence_tab = 0;

  self->attr_cnt       = 0;
  self->attr_tab       = 0;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_dtor
 * ------------------------------------------------------------------------- */

void
alarm_event_dtor(alarm_event_t *self)
{
  free(self->title);
  free(self->message);
  free(self->sound);
  free(self->icon);
  free(self->alarm_appid);
  free(self->alarm_tz);

  alarm_event_del_actions(self);
  alarm_event_del_recurrences(self);

  alarm_event_del_attrs(self);
}

/* ------------------------------------------------------------------------- *
 * alarm_event_create
 * ------------------------------------------------------------------------- */

alarm_event_t *
alarm_event_create(void)
{
  alarm_event_t *self = calloc(1, sizeof *self);
  alarm_event_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_create_ex
 * ------------------------------------------------------------------------- */

alarm_event_t *
alarm_event_create_ex(size_t actions)
{
  alarm_event_t *self = calloc(1, sizeof *self);

  alarm_event_ctor(self);

  alarm_event_add_actions(self, actions);

  return self;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_delete
 * ------------------------------------------------------------------------- */

void
alarm_event_delete(alarm_event_t *self)
{
  if( self != 0 )
  {
    alarm_event_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * alarm_event_delete_cb
 * ------------------------------------------------------------------------- */

void
alarm_event_delete_cb(void *self)
{
  alarm_event_delete(self);
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_action
 * ------------------------------------------------------------------------- */

alarm_action_t *
alarm_event_get_action(const alarm_event_t *self, int index)
{
  alarm_action_t *action = 0;
  if( 0 <= index && index < self->action_cnt )
  {
    action = &self->action_tab[index];
  }
  return action;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_set_action_dbus_args
 * ------------------------------------------------------------------------- */

int
alarm_event_set_action_dbus_args(const alarm_event_t *self, int index, int type, ...)
{
  int             error = -1;
  alarm_action_t *action = 0;

  if( (action = alarm_event_get_action(self, index)) != 0 )
  {
    va_list va;
    va_start(va, type);
    error = alarm_action_set_dbus_args_valist(action, type, va);
    va_end(va);
  }

  return error;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_action_dbus_args
 * ------------------------------------------------------------------------- */

const char *
alarm_event_get_action_dbus_args(const alarm_event_t *self, int index)
{
  const char     *args   = 0;
  alarm_action_t *action = 0;

  if( (action = alarm_event_get_action(self, index)) != 0 )
  {
    args = action->dbus_args;
  }

  return args;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_del_action_dbus_args
 * ------------------------------------------------------------------------- */

void
alarm_event_del_action_dbus_args(const alarm_event_t *self, int index)
{
  alarm_action_t *action = 0;

  if( (action = alarm_event_get_action(self, index)) != 0 )
  {
    alarm_action_del_dbus_args(action);
  }
}

/* ------------------------------------------------------------------------- *
 * alarm_event_set_time
 * ------------------------------------------------------------------------- */

void
alarm_event_set_time(alarm_event_t *self, const struct tm *tm)
{
  self->alarm_tm = *tm;
  self->alarm_time = -1;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_time
 * ------------------------------------------------------------------------- */

void
alarm_event_get_time(const alarm_event_t *self, struct tm *tm)
{
  if( self->alarm_time > 0 )
  {
    ticker_get_local_ex(self->alarm_time, tm);

// QUARANTINE     // FIXME: libtime code does not compile, huh?
// QUARANTINE     localtime_r(&self->alarm_time, tm);

// QUARANTINE     char tz[128];
// QUARANTINE     time_get_timezone(tz, sizeof tz);
// QUARANTINE     time_get_remote(self->alarm_time, tz, tm);
  }
  else
  {
    *tm = self->alarm_tm;
  }
}

/* ------------------------------------------------------------------------- *
 * alarm_event_del_actions
 * ------------------------------------------------------------------------- */

void
alarm_event_del_actions(alarm_event_t *self)
{
  for( size_t i = 0; i < self->action_cnt; ++i )
  {
    alarm_action_dtor(&self->action_tab[i]);
  }
  free(self->action_tab);
  self->action_tab = 0;
  self->action_cnt = 0;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_add_actions
 * ------------------------------------------------------------------------- */

alarm_action_t *
alarm_event_add_actions(alarm_event_t *self, size_t count)
{
  size_t previously = self->action_cnt;

  self->action_cnt += count;

  self->action_tab = realloc(self->action_tab,
                          self->action_cnt * sizeof *self->action_tab);

  for( size_t i = previously; i < self->action_cnt; ++i )
  {
    alarm_action_ctor(&self->action_tab[i]);
  }

  return &self->action_tab[previously];
}

/* ------------------------------------------------------------------------- *
 * alarm_event_del_recurrences
 * ------------------------------------------------------------------------- */

void
alarm_event_del_recurrences(alarm_event_t *self)
{
  for( size_t i = 0; i < self->recurrence_cnt; ++i )
  {
    alarm_recur_dtor(&self->recurrence_tab[i]);
  }
  free(self->recurrence_tab);
  self->recurrence_tab  = 0;
  self->recurrence_cnt = 0;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_add_recurrences
 * ------------------------------------------------------------------------- */

alarm_recur_t *
alarm_event_add_recurrences(alarm_event_t *self, size_t count)
{
  size_t previously = self->recurrence_cnt;

  self->recurrence_cnt += count;

  self->recurrence_tab = realloc(self->recurrence_tab,
                            self->recurrence_cnt * sizeof *self->recurrence_tab);

  for( size_t i = previously; i < self->recurrence_cnt; ++i )
  {
    alarm_recur_ctor(&self->recurrence_tab[i]);
  }
  return &self->recurrence_tab[previously];
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_recurrence
 * ------------------------------------------------------------------------- */

alarm_recur_t *
alarm_event_get_recurrence(const alarm_event_t *self, int index)
{
  alarm_recur_t *rec = 0;
  if( 0 <= index && index < self->recurrence_cnt )
  {
    rec = &self->recurrence_tab[index];
  }
  return rec;
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_title
 * ------------------------------------------------------------------------- */

const char *
alarm_event_get_title(const alarm_event_t *self)
{
  return self->title ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_event_set_title
 * ------------------------------------------------------------------------- */

void
alarm_event_set_title(alarm_event_t *self, const char *title)
{
  xstrset(&self->title, title);
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_message
 * ------------------------------------------------------------------------- */

const char *
alarm_event_get_message(const alarm_event_t *self)
{
  return self->message ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_event_set_message
 * ------------------------------------------------------------------------- */

void
alarm_event_set_message(alarm_event_t *self, const char *message)
{
  xstrset(&self->message, message);
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_sound
 * ------------------------------------------------------------------------- */

const char *
alarm_event_get_sound(const alarm_event_t *self)
{
  return self->sound ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_event_set_sound
 * ------------------------------------------------------------------------- */

void
alarm_event_set_sound(alarm_event_t *self, const char *sound)
{
  xstrset(&self->sound, sound);
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_icon
 * ------------------------------------------------------------------------- */

const char *
alarm_event_get_icon(const alarm_event_t *self)
{
  return self->icon ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_event_set_icon
 * ------------------------------------------------------------------------- */

void
alarm_event_set_icon(alarm_event_t *self, const char *icon)
{
  xstrset(&self->icon, icon);
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_alarm_appid
 * ------------------------------------------------------------------------- */

const char *
alarm_event_get_alarm_appid(const alarm_event_t *self)
{
  return self->alarm_appid ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_event_set_alarm_appid
 * ------------------------------------------------------------------------- */

void
alarm_event_set_alarm_appid(alarm_event_t *self, const char *alarm_appid)
{
  xstrset(&self->alarm_appid, alarm_appid);
}

/* ------------------------------------------------------------------------- *
 * alarm_event_get_alarm_tz
 * ------------------------------------------------------------------------- */

const char *
alarm_event_get_alarm_tz(const alarm_event_t *self)
{
  return self->alarm_tz ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_event_set_alarm_tz
 * ------------------------------------------------------------------------- */

void
alarm_event_set_alarm_tz(alarm_event_t *self, const char *alarm_tz)
{
  xstrset(&self->alarm_tz, alarm_tz);
}

/* ------------------------------------------------------------------------- *
 * alarm_event_is_sane
 * ------------------------------------------------------------------------- */

int
alarm_event_is_sane(const alarm_event_t *event)
{
  int err = 0;
#if ENABLE_LOGGING
  const char *function = __FUNCTION__;
#endif

  auto void W(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
  auto void E(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

  auto void E(const char *fmt, ...)
  {
    char *msg = 0;
    va_list va;
    va_start(va, fmt);
    vasprintf(&msg, fmt, va);
    va_end(va);
    log_error("%s: %s", function, msg);
    free(msg);
    err |= 2;
  }
  auto void W(const char *fmt, ...)
  {
    char *msg = 0;
    va_list va;
    va_start(va, fmt);
    vasprintf(&msg, fmt, va);
    va_end(va);
    vasprintf(&msg, fmt, va);
    log_warning("%s: %s", function, msg);
    free(msg);
    err |= 1;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * appid must be defined
   * - - - - - - - - - - - - - - - - - - - */

  if( xisempty(event->alarm_appid) )
  {
    E("alarm_appid is empty\n");
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * scan action table first
   * - - - - - - - - - - - - - - - - - - - */

  int actions = 0; // valid actions found
  int buttons = 0; // valid button actions found

  if( (int)event->action_cnt < 0 )
  {
    E("action_cnt set to negative value?\n");
  }

  if( event->action_cnt == 0 && event->action_tab != 0 )
  {
    W("action_cnt == 0 && action_tab != NULL?\n");
  }

  if( event->action_cnt != 0 && event->action_tab == 0 )
  {
    E("action_cnt != 0 && action_tab == NULL?\n");
  }

  for( int i = 0; i < (int)event->action_cnt; ++i )
  {
    alarm_action_t *act = alarm_event_get_action(event, i);

    if( act == 0 )
    {
      E("action_tab[%d] could not be accessed\n", i);
      break;
    }

    // valid action ?
    if( alarm_action_is_button(act) )
    {
      buttons += 1;
      actions += 1;
    }
    else if( (act->flags & ALARM_ACTION_WHEN_MASK) &&
             (act->flags & ALARM_ACTION_TYPE_MASK) )
    {
      actions += 1;
    }

    // action must have at least one WHEN bit set
    if( (act->flags & ALARM_ACTION_WHEN_MASK) == 0 )
    {
      W("action_tab[%d] no ALARM_ACTION_WHEN_xxx bits set?\n", i);
    }

    // only button actions can have ALARM_ACTION_TYPE_NOP
    if( (act->flags & ALARM_ACTION_TYPE_MASK) == 0 )
    {
      if( !alarm_action_is_button(act) ||
          (act->flags & ALARM_ACTION_WHEN_MASK) != ALARM_ACTION_WHEN_RESPONDED )
      {
        W("action_tab[%d] no ALARM_ACTION_TYPE_xxx bits set?\n", i);
      }
    }

    // action label is required for button, and makes no sense for
    // non-button actions
    if( (act->flags & ALARM_ACTION_WHEN_RESPONDED) && xisempty(act->label) )
    {
      E("action_tab[%d] ALARM_ACTION_WHEN_RESPONDED but no button label\n", i);
    }
    if( !(act->flags & ALARM_ACTION_WHEN_RESPONDED) && !xisempty(act->label) )
    {
      W("action_tab[%d] act->label is set for non button action\n", i);
    }

    if( act->flags & ALARM_ACTION_TYPE_DBUS )
    {
      // if it is dbus action, some fields are required

      if( xisempty(act->dbus_interface) )
      {
        E("action_tab[%d] dbus_interface not set for dbus action\n", i);
      }
      if( xisempty(act->dbus_path) )
      {
        E("action_tab[%d] dbus_path not set for dbus action\n", i);
      }
      if( xisempty(act->dbus_name) )
      {
        E("action_tab[%d] dbus_name not set for dbus action\n", i);
      }
    }
    else
    {
      // otherwise they should not be set

      if( act->flags & ALARM_ACTION_DBUS_USE_SYSTEMBUS )
      {
        W("action_tab[%d] ALARM_ACTION_DBUS_USE_SYSTEMBUS set for non dbus action\n", i);
      }
      if( act->flags & ALARM_ACTION_DBUS_USE_ACTIVATION )
      {
        W("action_tab[%d] ALARM_ACTION_DBUS_USE_ACTIVATION set for non dbus action\n", i);
      }
      if( act->flags & ALARM_ACTION_DBUS_ADD_COOKIE )
      {
        W("action_tab[%d] ALARM_ACTION_DBUS_ADD_COOKIE set for non dbus action\n", i);
      }
      if( !xisempty(act->dbus_interface) )
      {
        W("action_tab[%d] dbus_interface set for non dbus action\n", i);
      }
      if( !xisempty(act->dbus_service) )
      {
        W("action_tab[%d] dbus_service set for non dbus action\n", i);
      }
      if( !xisempty(act->dbus_path) )
      {
        W("action_tab[%d] dbus_path set for non dbus action\n", i);
      }
      if( !xisempty(act->dbus_name) )
      {
        W("action_tab[%d] dbus_name set for non dbus action\n", i);
      }
      if( !xisempty(act->dbus_args) )
      {
        W("action_tab[%d] dbus_args set for non dbus action\n", i);
      }
    }

    if( act->flags & ALARM_ACTION_TYPE_EXEC )
    {
      // if it is execute action, it should have command
      if( xisempty(act->exec_command) )
      {
        E("action_tab[%d] exec_command not set for exec action\n", i);
      }
    }
    else
    {
      // otherwise command should not be set
      if( !xisempty(act->exec_command) )
      {
        W("action_tab[%d] exec_command set for non exec action\n", i);
      }
      // or adding cookie requested
      if( act->flags & ALARM_ACTION_EXEC_ADD_COOKIE )
      {
        W("action_tab[%d] adding exec cookie set for non exec action\n", i);
      }
    }
  }

  if( event->action_cnt == 0 )
  {
    E("event has no actions!\n");
  }
  else if( actions == 0 )
  {
    E("event has no actions that are configured to do something\n");
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * only one of the boot bits should be set
   * - - - - - - - - - - - - - - - - - - - */

  if( (event->flags & ALARM_EVENT_BOOT) &&
      (event->flags & ALARM_EVENT_ACTDEAD) )
  {
    W("flags has both BOOT and ACTDEAD?\n");
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * alarm time checks
   * - - - - - - - - - - - - - - - - - - - */

  // timezone makes no sence for absolute alarm time
  if( (event->alarm_time > 0) && !xisempty(event->alarm_tz) )
  {
    W("alarm_tz set for absolute time alarm\n");
  }

  // should not specify both broken down & absolute alarm time
  if( event->alarm_time > 0 )
  {
    if( !ticker_tm_is_uninitialized(&event->alarm_tm) )
    {
      alarm_event_t def;
      alarm_event_ctor(&def);
      if( !ticker_tm_is_same(&event->alarm_tm, &def.alarm_tm) )
      {
        W("both alarm_time and alarm_tm are set!\n");
      }
      alarm_event_dtor(&def);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * snooze checks
   * - - - - - - - - - - - - - - - - - - - */

  // negative snooze makes no sense
  if( event->snooze_secs < 0 )
  {
    W("snooze_secs is negative?\n");
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * dialog settings make sense only if
   * there are buttons too
   * - - - - - - - - - - - - - - - - - - - */

  if( xisempty(event->message) && buttons != 0 )
  {
    W("message is empty, but there are button actions?\n");
  }

  if( !xisempty(event->message) && buttons == 0 )
  {
    W("message is non-empty, but there are no button actions?\n");
  }

  if( !xisempty(event->sound) && buttons == 0 )
  {
    W("sound is non-empty, but there are no button actions?\n");
  }

  if( !xisempty(event->icon) && buttons == 0 )
  {
    W("icon is non-empty, but there are no button actions?\n");
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * recurrence
   * - - - - - - - - - - - - - - - - - - - */

  if( event->recur_secs < 0 )
  {
    E("recur_secs is negative\n");
  }

  if( (int)event->recurrence_cnt < 0 )
  {
    E("recurrence_cnt set to negative value?\n");
  }

  if( event->recurrence_cnt == 0 && event->recurrence_tab != 0 )
  {
    W("recurrence_cnt == 0 && recurrence_tab != NULL?\n");
  }

  if( event->recurrence_cnt != 0 && event->recurrence_tab == 0 )
  {
    E("recurrence_cnt != 0 && recurrence_tab == NULL?\n");
  }

  int recmasks = 0;

  for( int i = 0; i < (int)event->recurrence_cnt; ++i )
  {
    alarm_recur_t *rec = alarm_event_get_recurrence(event, i);
    if( rec == 0 )
    {
      E("recurrence_tab[%d] could not be accessed\n", i);
      break;
    }

    if( rec->mask_min  == ALARM_RECUR_MIN_DONTCARE &&
        rec->mask_hour == ALARM_RECUR_HOUR_DONTCARE &&
        rec->mask_mday == ALARM_RECUR_MDAY_DONTCARE &&
        rec->mask_wday == ALARM_RECUR_WDAY_DONTCARE &&
        rec->mask_mon  == ALARM_RECUR_MON_DONTCARE &&
        rec->special   == ALARM_RECUR_SPECIAL_NONE )
    {
      W("recurrence_tab[%d] is blank and makes no sense\n", i);
    }
    else
    {
      recmasks += 1;
    }
  }

  if( event->recur_count != 0 )
  {
    if( event->recur_secs <= 0 )
    {
      if( event->recurrence_cnt == 0 )
      {
        E("recur_count != 0, recurrence interval not set\n");
      }
      else if( recmasks == 0 )
      {
        E("recur_count != 0, no valid recurrence items\n");
      }
    }
  }

  if( event->recur_secs > 0 && event->recur_count == 0 )
  {
    E("recur_secs > 0 and recur_count == 0\n");
  }

  if( event->recurrence_cnt > 0 && event->recur_count == 0 )
  {
    E("recurrence_cnt > 0 and recur_count == 0\n");
  }

  if( event->recur_secs > 0 && event->recurrence_cnt > 0 )
  {
    E("recur_secs > 0 and recurrence_cnt > 0\n");
  }

  return (err&2) ? -1 : err ? 0 : 1;
}

void
alarm_event_del_attrs(alarm_event_t *self)
{
  for( size_t i = 0; i < self->attr_cnt; ++i )
  {
    alarm_attr_delete(self->attr_tab[i]);
  }
  free(self->attr_tab);

  self->attr_cnt       = 0;
  self->attr_tab       = 0;
}

void
alarm_event_rem_attr(alarm_event_t *self, const char *name)
{
  size_t k = 0;
  for( size_t i = 0; i < self->attr_cnt; ++i )
  {
    alarm_attr_t *att = self->attr_tab[i];
    if( !strcmp(att->attr_name, name) )
    {
      alarm_attr_delete(att);
    }
    else
    {
      self->attr_tab[k++] = att;
    }
  }
  self->attr_cnt = k;
}

alarm_attr_t *
alarm_event_get_attr(alarm_event_t *self, const char *name)
{

  for( size_t i = 0; i < self->attr_cnt; ++i )
  {
    alarm_attr_t *res = self->attr_tab[i];
    if( !strcmp(res->attr_name, name) )
    {
      return res;
    }
  }
  return 0;
}

int
alarm_event_has_attr(alarm_event_t *self, const char *name)
{
  return alarm_event_get_attr(self, name) != 0;
}

alarm_attr_t *
alarm_event_add_attr(alarm_event_t *self, const char *name)
{
  alarm_attr_t *res = alarm_event_get_attr(self, name);

  if( res == 0 )
  {
    res = alarm_attr_create(name);

    self->attr_cnt += 1;
    self->attr_tab = realloc(self->attr_tab,
                             self->attr_cnt * sizeof *self->attr_tab);
    self->attr_tab[self->attr_cnt-1] = res;
  }

  return res;
}

void
alarm_event_set_attr_int(alarm_event_t *self, const char *name, int val)
{
  alarm_attr_t *att = alarm_event_add_attr(self, name);
  alarm_attr_set_int(att, val);
}

void
alarm_event_set_attr_time(alarm_event_t *self, const char *name, time_t val)
{
  alarm_attr_t *att = alarm_event_add_attr(self, name);
  alarm_attr_set_time(att, val);
}

void
alarm_event_set_attr_string(alarm_event_t *self, const char *name, const char *val)
{
  alarm_attr_t *att = alarm_event_add_attr(self, name);
  alarm_attr_set_string(att, val);
}

int
alarm_event_get_attr_int(alarm_event_t *self, const char *name, int def)
{
  alarm_attr_t *att = alarm_event_add_attr(self, name);

  return att ? alarm_attr_get_int(att) : def;
}

time_t
alarm_event_get_attr_time(alarm_event_t *self, const char *name, time_t def)
{
  alarm_attr_t *att = alarm_event_add_attr(self, name);
  return att ? alarm_attr_get_time(att) : def;
}

const char *
alarm_event_get_attr_string(alarm_event_t *self, const char *name, const char *def)
{
  alarm_attr_t *att = alarm_event_add_attr(self, name);
  return att ? alarm_attr_get_string(att) : def;
}

int
alarm_event_is_recurring(const alarm_event_t *self)
{
  return (self->recur_count != 0) && ((self->recur_secs > 0) ||
                                      (self->recurrence_cnt > 0));
}
