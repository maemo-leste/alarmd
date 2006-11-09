/**
 * This file is part of alarmd
 *
 * Contact Person: David Weinehall <david.weinehall@nokia.com>
 *
 * Copyright (C) 2006 Nokia Corporation
 * alarmd and libalarm are free software; you can redistribute them
 * and/or modify them under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * alarmd and libalarm are distributed in the hope that they will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <glib.h>
#include <glib-object.h>
#include "include/alarm_event.h"
#include "queue.h"
#include "event.h"
#include "action.h"
#include "debug.h"

static const time_t _day_in_seconds = 24 * 60 * 60;

static void alarmd_event_init(AlarmdEvent *event);
static void alarmd_event_class_init(AlarmdEventClass *klass);
static void _alarmd_event_set_property(GObject *object,
               guint param_id,
               const GValue *value,
               GParamSpec *pspec);
static void _alarmd_event_get_property(GObject *object,
               guint param_id,
               GValue *value,
               GParamSpec *pspec);

static void _alarmd_event_real_fire(AlarmdEvent *event, gboolean delayed);
static void _alarmd_event_fired(AlarmdEvent *event, gboolean delayed);
static void _alarmd_event_real_cancel(AlarmdEvent *event);
static void _alarmd_event_real_acknowledge(AlarmdEvent *event);
static void _alarmd_event_real_snooze(AlarmdEvent *event);
static void _alarmd_event_real_queue(AlarmdEvent *event, TimerPlugin *plugin);
static void _alarmd_event_real_dequeue(AlarmdEvent *event);
static void _alarmd_event_real_dequeued(AlarmdEvent *event);
static time_t _alarmd_event_real_get_time(AlarmdEvent *event);
static gint32 _alarmd_event_real_get_flags(AlarmdEvent *event);
static void _alarmd_event_finalize(GObject *object);
static void _alarmd_event_changed(AlarmdEvent *event, gpointer user_data);
static GSList *_alarmd_event_get_saved_properties(void);
static void _alarmd_event_action_acknowledge(AlarmdEvent *event, guint ack_type, AlarmdAction *action);
static gboolean _alarmd_event_real_need_power_up(AlarmdEvent *event);
static void _alarmd_event_time_changed(AlarmdObject *object);

enum signals {
       SIGNAL_FIRE,
       SIGNAL_CANCEL,
       SIGNAL_ACKNOWLEDGE,
       SIGNAL_SNOOZE,
       SIGNAL_QUEUE,
       SIGNAL_DEQUEUE,
       SIGNAL_COUNT
};

enum properties {
       PROP_ACTION = 1,
       PROP_TIME,
       PROP_SNOOZE_INT,
       PROP_SNOOZE,
       PROP_COOKIE,
       PROP_QUEUE,
};

enum saved_props {
       S_ACTION,
       S_TIME,
       S_SNOOZE_INT,
       S_SNOOZE,
       S_COOKIE,
       S_COUNT
};

static const gchar * const saved_properties[S_COUNT] =
{
       "action",
       "time",
       "snooze_interval",
       "snooze",
       "cookie",
};

static guint event_signals[SIGNAL_COUNT];

typedef struct _AlarmdEventPrivate AlarmdEventPrivate;
struct _AlarmdEventPrivate {
       TimerPlugin *queued;
       time_t alarm_time;
       guint snooze_interval;
       guint snooze;
       AlarmdAction *action;
       AlarmdQueue *queue;
       gulong action_changed_signal;
       gulong action_acknowledged_signal;
       glong cookie;
};


#define ALARMD_EVENT_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                             ALARMD_TYPE_EVENT, AlarmdEventPrivate));


GType alarmd_event_get_type(void)
{
       static GType event_type = 0;

       if (!event_type)
       {
               static const GTypeInfo event_info =
               {
                       sizeof (AlarmdEventClass),
                       NULL,
                       NULL,
                       (GClassInitFunc) alarmd_event_class_init,
                       NULL,
                       NULL,
                       sizeof (AlarmdEvent),
                       0,
                       (GInstanceInitFunc) alarmd_event_init,
                       NULL
               };

               event_type = g_type_register_static(ALARMD_TYPE_OBJECT,
                               "AlarmdEvent",
                               &event_info, 0);
       }

       return event_type;
}

AlarmdEvent *alarmd_event_new(void)
{
       AlarmdEvent *retval;
       ENTER_FUNC;
       retval = g_object_new(ALARMD_TYPE_EVENT, NULL);
       LEAVE_FUNC;
       return retval;
}

void alarmd_event_fire(AlarmdEvent *event, gboolean delayed)
{
       ENTER_FUNC;
       g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_FIRE], 0, delayed);
       LEAVE_FUNC;
}

void alarmd_event_cancel(AlarmdEvent *event)
{
       ENTER_FUNC;
       g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_CANCEL], 0);
       LEAVE_FUNC;
}

void alarmd_event_acknowledge(AlarmdEvent *event)
{
       ENTER_FUNC;
       g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_ACKNOWLEDGE], 0);
       LEAVE_FUNC;
}

void alarmd_event_snooze(AlarmdEvent *event)
{
       ENTER_FUNC;
       g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_SNOOZE], 0);
       LEAVE_FUNC;
}

void alarmd_event_queue(AlarmdEvent *event, TimerPlugin *plugin)
{
       ENTER_FUNC;
       g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_QUEUE], 0, plugin);
       LEAVE_FUNC;
}

void alarmd_event_dequeue(AlarmdEvent *event)
{
       ENTER_FUNC;
       g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_DEQUEUE], 0);
       LEAVE_FUNC;
}

time_t alarmd_event_get_time(AlarmdEvent *event)
{
       AlarmdEventClass *klass = ALARMD_EVENT_GET_CLASS(event);
       time_t retval;
       ENTER_FUNC;
       retval = klass->get_time(event);
       LEAVE_FUNC;
       return retval;
}

static void alarmd_event_init(AlarmdEvent *event)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;

       priv->queued = NULL;
       priv->snooze_interval = 0;
       priv->snooze = 0;
       priv->alarm_time = 0;
       priv->action = NULL;
       priv->cookie = 0;
       priv->queue = NULL;
       LEAVE_FUNC;
}

static void alarmd_event_class_init(AlarmdEventClass *klass)
{
       GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
       AlarmdObjectClass *aobject_class = ALARMD_OBJECT_CLASS(klass);

       ENTER_FUNC;
       g_type_class_add_private(klass, sizeof(AlarmdEventPrivate));

       gobject_class->set_property = _alarmd_event_set_property;
       gobject_class->get_property = _alarmd_event_get_property;
       gobject_class->finalize = _alarmd_event_finalize;

       aobject_class->get_saved_properties = _alarmd_event_get_saved_properties;
       aobject_class->time_changed = _alarmd_event_time_changed;

       klass->fire = _alarmd_event_real_fire;
       klass->cancel = _alarmd_event_real_cancel;
       klass->acknowledge = _alarmd_event_real_acknowledge;
       klass->snooze = _alarmd_event_real_snooze;
       klass->queue = _alarmd_event_real_queue;
       klass->dequeue = _alarmd_event_real_dequeue;

       klass->get_time = _alarmd_event_real_get_time;
       klass->get_flags = _alarmd_event_real_get_flags;
       klass->need_power_up = _alarmd_event_real_need_power_up;

       g_object_class_install_property(gobject_class,
                       PROP_ACTION,
                       g_param_spec_object("action",
                               "Event's action.",
                               "Action done when event is due.",
                               ALARMD_TYPE_ACTION,
                               G_PARAM_READABLE | G_PARAM_WRITABLE));
       g_object_class_install_property(gobject_class,
                       PROP_TIME,
                       g_param_spec_int64("time",
                               "Event's time.",
                               "Time when event is due.",
                               0,
                               G_MAXINT64,
                               0,
                               G_PARAM_READABLE | G_PARAM_WRITABLE));
       g_object_class_install_property(gobject_class,
                       PROP_SNOOZE_INT,
                       g_param_spec_uint("snooze_interval",
                               "Postponing interval.",
                               "Amount of time the event is postponed when snoozed.",
                               0,
                               G_MAXUINT,
                               0,
                               G_PARAM_READABLE | G_PARAM_WRITABLE));
       g_object_class_install_property(gobject_class,
                       PROP_SNOOZE,
                       g_param_spec_uint("snooze",
                               "Time snoozed.",
                               "Amount of time the event has been snoozed.",
                               0,
                               G_MAXUINT,
                               0,
                               G_PARAM_READABLE | G_PARAM_WRITABLE));
       g_object_class_install_property(gobject_class,
                       PROP_COOKIE,
                       g_param_spec_long("cookie",
                               "Event's id.",
                               "Unique ID for the event.",
                               0,
                               G_MAXLONG,
                               0,
                               G_PARAM_READABLE | G_PARAM_WRITABLE));
       g_object_class_install_property(gobject_class,
                       PROP_QUEUE,
                       g_param_spec_object("queue",
                               "Queue the event is on.",
                               "The queue that has the event.",
                               ALARMD_TYPE_QUEUE,
                               G_PARAM_READABLE | G_PARAM_WRITABLE));

       event_signals[SIGNAL_FIRE] = g_signal_new("fire",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                       G_STRUCT_OFFSET(AlarmdEventClass, fire),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__BOOLEAN,
                       G_TYPE_NONE, 1,
                       G_TYPE_BOOLEAN);
       event_signals[SIGNAL_CANCEL] = g_signal_new("cancel",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                       G_STRUCT_OFFSET(AlarmdEventClass, cancel),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
       event_signals[SIGNAL_ACKNOWLEDGE] = g_signal_new("acknowledge",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                       G_STRUCT_OFFSET(AlarmdEventClass, acknowledge),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
       event_signals[SIGNAL_SNOOZE] = g_signal_new("snooze",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                       G_STRUCT_OFFSET(AlarmdEventClass, snooze),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
       event_signals[SIGNAL_QUEUE] = g_signal_new("queue",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                       G_STRUCT_OFFSET(AlarmdEventClass, queue),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1,
                       G_TYPE_POINTER);
       event_signals[SIGNAL_DEQUEUE] = g_signal_new("dequeue",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                       G_STRUCT_OFFSET(AlarmdEventClass, dequeue),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
       LEAVE_FUNC;
}

static void _alarmd_event_action_acknowledge(AlarmdEvent *event, guint ack_type, AlarmdAction *action)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;
       (void)action;

       g_signal_handler_disconnect(priv->action, priv->action_acknowledged_signal);
       if (ack_type == ACK_NORMAL) {
               g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_ACKNOWLEDGE], 0);
       } else {
               g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_SNOOZE], 0);
       }
       LEAVE_FUNC;
}

static void _alarmd_event_real_fire(AlarmdEvent *event, gboolean delayed)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;

       if (priv->action) {
               if (delayed) {
                       gint flags;
                       g_object_get(priv->action, "flags", &flags, NULL);
                       if (flags & ALARM_EVENT_POSTPONE_DELAYED) {
                               time_t now = time(NULL);

                               if (now > priv->alarm_time + _day_in_seconds)
                               {
                                       time_t d = now - priv->alarm_time;
                                       guint count = d / _day_in_seconds + 1;
                                       priv->alarm_time += count *
                                               _day_in_seconds;
                                       g_object_set(event,
                                                       "time",
                                                       priv->alarm_time,
                                                       NULL);
                                       LEAVE_FUNC;
                                       return;
                               } else {
                                       delayed = FALSE;
                               }
                       }
               }
               priv->action_acknowledged_signal =
                       g_signal_connect_swapped(priv->action, "acknowledge", (GCallback)_alarmd_event_action_acknowledge, event);
               alarmd_action_run(priv->action, delayed);
       } else {
               g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_ACKNOWLEDGE], 0);
       }
       LEAVE_FUNC;
}

static void _alarmd_event_fired(AlarmdEvent *event, gboolean delayed)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;

       DEBUG("%p: queued = NULL", event);
       DEBUG("Unreffing %p", event);
       g_object_unref(event);
       priv->queued = NULL;
       alarmd_event_fire(event, delayed);

       LEAVE_FUNC;
}

static void _alarmd_event_real_cancel(AlarmdEvent *event)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;
       if (priv->queued) {
               priv->queued->remove_event(priv->queued);
               DEBUG("%p: queued = NULL", event);
               priv->queued = NULL;
       }
       LEAVE_FUNC;
}

static void _alarmd_event_real_snooze(AlarmdEvent *event)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       guint snooze;
       guint64 now = time(NULL);
       ENTER_FUNC;

       if (priv->snooze_interval) {
               snooze = priv->snooze_interval;
       } else {
               g_object_get(priv->queue, "snooze", &snooze, NULL);
       }

       priv->snooze += snooze;

       if (priv->alarm_time + priv->snooze * 60 < now) {
               priv->snooze = (now - priv->alarm_time) / 60 + snooze;
       }

       alarmd_object_changed(ALARMD_OBJECT(event));
       LEAVE_FUNC;
}

static void _alarmd_event_real_acknowledge(AlarmdEvent *event)
{
       ENTER_FUNC;
       g_signal_emit(ALARMD_OBJECT(event), event_signals[SIGNAL_CANCEL], 0);
       LEAVE_FUNC;
}

static void _alarmd_event_real_queue(AlarmdEvent *event, TimerPlugin *plugin)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;

       if (priv->queued == plugin) {
               DEBUG("%p already on given queue %p.", event, plugin);
               LEAVE_FUNC;
               return;
       }

       if (priv->queued) {
               priv->queued->remove_event(priv->queued);
               DEBUG("%p: queued = NULL", event);
               priv->queued = NULL;
       }
       DEBUG("Reffing %p", event);
       g_object_ref(event);
       if (plugin->set_event(plugin, alarmd_event_get_time(event), (TimerCallback)_alarmd_event_fired, (TimerCancel)_alarmd_event_real_dequeued, event)) {
               DEBUG("%p: queued = %p", event, plugin);
               priv->queued = plugin;

       } else {
               DEBUG("Unreffing %p", event);
               DEBUG("%p: queued = NULL", event);
               priv->queued = NULL;
               g_object_unref(event);
       }
       LEAVE_FUNC;
}

static void _alarmd_event_real_dequeued(AlarmdEvent *event)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;

       if (priv->queued != NULL) {
               DEBUG("%p: queued = NULL", event);
               priv->queued = NULL;
               alarmd_event_dequeue(event);
               DEBUG("Unreffing %p", event);
               g_object_unref(event);
       }

       LEAVE_FUNC;
}

static void _alarmd_event_real_dequeue(AlarmdEvent *event)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;

       if (priv->queued) {
               priv->queued->remove_event(priv->queued);
               DEBUG("%p: queued = NULL", event);
       }
       LEAVE_FUNC;
}

static time_t _alarmd_event_real_get_time(AlarmdEvent *event)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;

       LEAVE_FUNC;
       return priv->alarm_time + priv->snooze * 60;
}

static gint32 _alarmd_event_real_get_flags(AlarmdEvent *event)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       gint32 retval = 0;
       ENTER_FUNC;

       if (priv->action) {
               g_object_get(G_OBJECT(priv->action), "flags", &retval, NULL);
       }

       LEAVE_FUNC;
       return retval;
}

static void _alarmd_event_changed(AlarmdEvent *event, gpointer user_data)
{
       ENTER_FUNC;
       (void)user_data;

       alarmd_object_changed(ALARMD_OBJECT(event));
       LEAVE_FUNC;
}

static void _alarmd_event_set_property(GObject *object,
               guint param_id,
               const GValue *value,
               GParamSpec *pspec)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(object);
       ENTER_FUNC;

       DEBUG("param_id = %u", param_id);

       switch (param_id) {
       case PROP_ACTION:
               if (priv->action) {
                       g_signal_handler_disconnect(priv->action, priv->action_changed_signal);
                       priv->action_changed_signal = 0;
                       g_object_set(priv->action, "event", NULL, NULL);
                       g_object_unref(priv->action);
               }
               priv->action = g_value_get_object(value);
               if (priv->action) {
                       g_object_ref(priv->action);
                       priv->action_changed_signal = g_signal_connect(priv->action, "changed", (GCallback)_alarmd_event_changed, NULL);
                       g_object_set(priv->action, "event", object, NULL);
               }
               break;
       case PROP_TIME:
               priv->alarm_time = g_value_get_int64(value);
               break;
       case PROP_SNOOZE:
               priv->snooze = g_value_get_uint(value);
               break;
       case PROP_SNOOZE_INT:
               priv->snooze_interval = g_value_get_uint(value);
               break;
       case PROP_COOKIE:
               priv->cookie = g_value_get_long(value);
               break;
       case PROP_QUEUE:
               if (priv->queue) {
                       alarmd_queue_remove_event(priv->queue,
                                       priv->cookie);
                       g_object_remove_weak_pointer(G_OBJECT(priv->queue),
                                       (gpointer *)&priv->queue);
               }
               priv->queue = g_value_get_object(value);
               if (priv->queue) {
                       g_object_add_weak_pointer(G_OBJECT(priv->queue),
                                       (gpointer *)&priv->queue);
               }
               LEAVE_FUNC;
               return;
               break;
       default:
               G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
               LEAVE_FUNC;
               return;
               break;
       }
       alarmd_object_changed(ALARMD_OBJECT(object));
       LEAVE_FUNC;
}

static void _alarmd_event_get_property(GObject *object,
               guint param_id,
               GValue *value,
               GParamSpec *pspec)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(object);
       ENTER_FUNC;
       switch (param_id) {
       case PROP_ACTION:
               g_value_set_object(value, priv->action);
               break;
       case PROP_TIME:
               g_value_set_int64(value, priv->alarm_time);
               break;
       case PROP_SNOOZE:
               g_value_set_uint(value, priv->snooze);
               break;
       case PROP_SNOOZE_INT:
               g_value_set_uint(value, priv->snooze_interval);
               break;
       case PROP_COOKIE:
               g_value_set_long(value, priv->cookie);
               break;
       case PROP_QUEUE:
               g_value_set_object(value, priv->queue);
               break;
       default:
               G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
               LEAVE_FUNC;
               return;
               break;
       }
       LEAVE_FUNC;
}

static void _alarmd_event_finalize(GObject *object)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(object);
       ENTER_FUNC;

       DEBUG("time: %lu", priv->alarm_time);
       DEBUG("queued: %p", priv->queued);
       if (priv->queue) {
               g_object_remove_weak_pointer(G_OBJECT(priv->queue), (gpointer *)&priv->queue);
       }
       if (priv->action) {
               g_object_set(priv->action, "event", NULL, NULL);
               g_object_unref(priv->action);
               priv->action = NULL;
       }
       G_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_EVENT)))->finalize(object);
       LEAVE_FUNC;
}

static GSList *_alarmd_event_get_saved_properties()
{
       guint i;
       GSList *retval = NULL;
       ENTER_FUNC;
       retval = ALARMD_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_EVENT)))->get_saved_properties();
       for (i = 0; i < S_COUNT; i++) {
               retval = g_slist_append(retval, (gpointer)saved_properties[i]);
       }
       LEAVE_FUNC;
       return retval;
}

static gboolean _alarmd_event_real_need_power_up(AlarmdEvent *event)
{
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);
       ENTER_FUNC;
       if (!priv->action) {
               LEAVE_FUNC;
               return FALSE;
       } else {
               gboolean retval = alarmd_action_need_power_up(priv->action);
               LEAVE_FUNC;
               return retval;
       }
}

static void _alarmd_event_time_changed(AlarmdObject *object)
{
       AlarmdEvent *event = ALARMD_EVENT(object);
       AlarmdEventPrivate *priv = ALARMD_EVENT_GET_PRIVATE(event);

       ENTER_FUNC;
       /* If we're on timer, the timer plugin will take care of us. */
       if (!priv->queued && alarmd_event_get_time(event) < time(NULL)) {
               alarmd_event_fire(event, TRUE);
       }
       LEAVE_FUNC;
}

gboolean alarmd_event_need_power_up(AlarmdEvent *event)
{
       AlarmdEventClass *klass = ALARMD_EVENT_GET_CLASS(event);
       gboolean retval;
       ENTER_FUNC;
       retval = klass->need_power_up(event);
       LEAVE_FUNC;
       return retval;
}

gint32 alarmd_event_get_flags(AlarmdEvent *event)
{
       AlarmdEventClass *klass = ALARMD_EVENT_GET_CLASS(event);
       gint32 retval;
       ENTER_FUNC;
       retval = klass->get_flags(event);
       LEAVE_FUNC;
       return retval;
}
