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
#include "eventrecurring.h"
#include "debug.h"

static void alarmd_event_recurring_init(AlarmdEventRecurring *event_recurring);
static void alarmd_event_recurring_class_init(AlarmdEventRecurringClass *klass);
static void _alarmd_event_recurring_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec);
static void _alarmd_event_recurring_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec);

static void _alarmd_event_recurring_real_acknowledge(AlarmdEvent *event);
static void _alarmd_event_recurring_changed(AlarmdObject *object);
static GSList *_alarmd_event_recurring_get_saved_properties(void);
static void _alarmd_event_recurring_time_changed(AlarmdObject *object);

enum saved_props {
	S_RECURR_INTERVAL,
	S_RECURR_COUNT,
	S_REAL_TIME,
	S_COUNT
};
enum properties {
	PROP_RECURR_INTERVAL = 1,
	PROP_RECURR_COUNT,
	PROP_REAL_TIME
};

static const gchar * const saved_properties[S_COUNT] =
{
	"recurr_interval",
	"recurr_count",
	"real_time"
};

typedef struct _AlarmdEventRecurringPrivate AlarmdEventRecurringPrivate;
struct _AlarmdEventRecurringPrivate {
	gint recurr_interval;
	gint recurr_count;
	guint64 real_time;
};


#define ALARMD_EVENT_RECURRING_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
			      ALARMD_TYPE_EVENT_RECURRING, AlarmdEventRecurringPrivate));


GType alarmd_event_recurring_get_type(void)
{
	static GType event_recurring_type = 0;

	if (!event_recurring_type)
	{
		static const GTypeInfo event_recurring_info =
		{
			sizeof (AlarmdEventRecurringClass),
			NULL,
			NULL,
			(GClassInitFunc) alarmd_event_recurring_class_init,
			NULL,
			NULL,
			sizeof (AlarmdEventRecurring),
			0,
			(GInstanceInitFunc) alarmd_event_recurring_init,
			NULL
		};

		event_recurring_type = g_type_register_static(ALARMD_TYPE_EVENT,
				"AlarmdEventRecurring",
				&event_recurring_info, 0);
	}

	return event_recurring_type;
}

AlarmdEventRecurring *alarmd_event_recurring_new(void)
{
	AlarmdEventRecurring *retval;
	ENTER_FUNC;
	retval = g_object_new(ALARMD_TYPE_EVENT_RECURRING, NULL);
	LEAVE_FUNC;
	return retval;
}

static void alarmd_event_recurring_init(AlarmdEventRecurring *event_recurring)
{
	AlarmdEventRecurringPrivate *priv = ALARMD_EVENT_RECURRING_GET_PRIVATE(event_recurring);
	ENTER_FUNC;

	priv->recurr_interval = 0;
	priv->recurr_count = 0;
	priv->real_time = 0;

	LEAVE_FUNC;
}

static void alarmd_event_recurring_class_init(AlarmdEventRecurringClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	AlarmdObjectClass *aobject_class = ALARMD_OBJECT_CLASS(klass);
	AlarmdEventClass *event_class = ALARMD_EVENT_CLASS(klass);

	ENTER_FUNC;
	g_type_class_add_private(klass, sizeof(AlarmdEventRecurringPrivate));

	gobject_class->set_property = _alarmd_event_recurring_set_property;
	gobject_class->get_property = _alarmd_event_recurring_get_property;

	aobject_class->get_saved_properties = _alarmd_event_recurring_get_saved_properties;
	aobject_class->changed = _alarmd_event_recurring_changed;
	aobject_class->time_changed = _alarmd_event_recurring_time_changed;

	event_class->acknowledge = _alarmd_event_recurring_real_acknowledge;

	g_object_class_install_property(gobject_class,
			PROP_REAL_TIME,
			g_param_spec_uint64("real_time",
				"The time without snooze time.",
				"The time without snooze.",
				0,
				G_MAXUINT64,
				0,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_RECURR_INTERVAL,
			g_param_spec_uint("recurr_interval",
				"Postponing interval.",
				"Amount of time the event_recurring is postponed when acknowledged.",
				0,
				G_MAXUINT,
				0,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_RECURR_COUNT,
			g_param_spec_int("recurr_count",
				"EventRecurring's id.",
				"Unique ID for the event_recurring.",
				-1,
				G_MAXINT,
				0,
				G_PARAM_READABLE | G_PARAM_WRITABLE));

	LEAVE_FUNC;
}

static void _alarmd_event_recurring_real_acknowledge(AlarmdEvent *event)
{
	AlarmdEventRecurringPrivate *priv = ALARMD_EVENT_RECURRING_GET_PRIVATE(event);
	guint64 diff = time(NULL) - priv->real_time;
	glong count = diff / (priv->recurr_interval * 60) + 1;
	ENTER_FUNC;
	DEBUG("old time: %llu, count: %u", priv->real_time, priv->recurr_count);

	if (priv->recurr_count != -1) {
		if (priv->recurr_count < count) {
			alarmd_event_cancel(event);
			LEAVE_FUNC;
			return;
		}
		priv->recurr_count -= count;
	}

	priv->real_time += count * priv->recurr_interval * 60;
	g_object_set(event, "time", priv->real_time, "snooze", 0, NULL);

	LEAVE_FUNC;
}

static void _alarmd_event_recurring_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec)
{
	AlarmdEventRecurringPrivate *priv = ALARMD_EVENT_RECURRING_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_RECURR_INTERVAL:
		priv->recurr_interval = g_value_get_uint(value);
		break;
	case PROP_RECURR_COUNT:
		priv->recurr_count = g_value_get_int(value);
		break;
	case PROP_REAL_TIME:
		priv->real_time = g_value_get_uint64(value);
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

static void _alarmd_event_recurring_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec)
{
	AlarmdEventRecurringPrivate *priv = ALARMD_EVENT_RECURRING_GET_PRIVATE(object);
	ENTER_FUNC;
	switch (param_id) {
	case PROP_RECURR_INTERVAL:
		g_value_set_uint(value, priv->recurr_interval);
		break;
	case PROP_RECURR_COUNT:
		g_value_set_int(value, priv->recurr_count);
		break;
	case PROP_REAL_TIME:
		g_value_set_uint64(value, priv->real_time);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		LEAVE_FUNC;
		return;
		break;
	}
	LEAVE_FUNC;
}

static GSList *_alarmd_event_recurring_get_saved_properties()
{
	guint i;
	GSList *retval = NULL;
	ENTER_FUNC;
	retval = ALARMD_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_EVENT_RECURRING)))->get_saved_properties();
	for (i = 0; i < S_COUNT; i++) {
		retval = g_slist_append(retval, (gpointer)saved_properties[i]);
	}
	LEAVE_FUNC;
	return retval;
}

static void _alarmd_event_recurring_changed(AlarmdObject *object)
{
	AlarmdEventRecurringPrivate *priv = ALARMD_EVENT_RECURRING_GET_PRIVATE(object);
	ENTER_FUNC;
	if (priv->real_time == 0) {
		g_object_get(object, "time", &priv->real_time, NULL);
	}
	LEAVE_FUNC;
}

static void _alarmd_event_recurring_time_changed(AlarmdObject *object)
{
	AlarmdEventRecurring *event = ALARMD_EVENT_RECURRING(object);
	AlarmdEventRecurringPrivate *priv = ALARMD_EVENT_RECURRING_GET_PRIVATE(event);
	time_t alarm_time;
	time_t now_time = time(NULL);
	GObject *action;
	gint flags;

	ENTER_FUNC;
	g_object_get(event, "action", &action, NULL);
	g_object_get(action, "flags", &flags, NULL);

	alarm_time = alarmd_event_get_time(ALARMD_EVENT(event));

	if ((flags & ALARM_EVENT_BACK_RESCHEDULE)) {

		if (alarm_time > now_time + priv->recurr_interval * 60) {
			glong count = (alarm_time - now_time) / (priv->recurr_interval * 60);
			priv->real_time = alarm_time - count * priv->recurr_interval * 60;
			DEBUG("Rescheduling to %d", priv->real_time);
			g_object_set(event, "time", priv->real_time, "snooze", 0, NULL);
			LEAVE_FUNC;
			return;
		}
	}
	/* If we're on timer, the timer plugin will take care of us. */
	ALARMD_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_EVENT_RECURRING)))->time_changed(object);
	
	LEAVE_FUNC;
}

