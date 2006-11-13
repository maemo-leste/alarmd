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
#include "action.h"
#include "event.h"
#include "rpc-systemui.h"
#include "rpc-statusbar.h"
#include "debug.h"

static void _alarmd_action_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec);
static void _alarmd_action_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec);
static void _alarmd_action_finalize(GObject *object);
static void alarmd_action_init(AlarmdAction *action);
static void alarmd_action_class_init(AlarmdActionClass *klass);
static void _alarmd_action_real_run(AlarmdAction *action, gboolean delayed);
static void _alarmd_action_real_acknowledge(AlarmdAction *action,
		AlarmdActionAckType ack_type);
static gboolean _alarmd_action_real_need_power_up(AlarmdAction *action);
static GSList *_alarmd_action_get_saved_properties(void);


enum properties {
	PROP_EVENT = 1,
	PROP_FLAGS,
};

enum saved_props {
	S_FLAGS,
	S_COUNT
};

static const gchar * const saved_properties[S_COUNT] = {
	"flags",
};

enum signals {
	SIGNAL_RUN,
	SIGNAL_ACKNOWLEDGE,
	SIGNAL_COUNT
};

static guint action_signals[SIGNAL_COUNT];

#define ALARMD_ACTION_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
			      ALARMD_TYPE_ACTION, AlarmdActionPrivate));

typedef struct _AlarmdActionPrivate AlarmdActionPrivate;
struct _AlarmdActionPrivate {
	AlarmdEvent *event;
	gint32 flags;
};


GType alarmd_action_get_type(void)
{
	static GType action_type = 0;

	if (!action_type)
	{
		static const GTypeInfo action_info =
		{
			sizeof (AlarmdActionClass),
			NULL,
			NULL,
			(GClassInitFunc) alarmd_action_class_init,
			NULL,
			NULL,
			sizeof (AlarmdAction),
			0,
			(GInstanceInitFunc) alarmd_action_init,
			NULL
		};

		action_type = g_type_register_static(ALARMD_TYPE_OBJECT,
				"AlarmdAction",
				&action_info, 0);
	}

	return action_type;
}

AlarmdAction *alarmd_action_new(void)
{
	AlarmdAction *retval;
	ENTER_FUNC;
	retval = g_object_new(ALARMD_TYPE_ACTION, NULL);
	LEAVE_FUNC;
	return retval;
}

void alarmd_action_run(AlarmdAction *action, gboolean delayed)
{
	ENTER_FUNC;
	g_signal_emit(action, action_signals[SIGNAL_RUN], 0, delayed);
	LEAVE_FUNC;
}

static void alarmd_action_init(AlarmdAction *action)
{
	ENTER_FUNC;
	(void)action;
	LEAVE_FUNC;
}

static void _alarmd_action_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec)
{
	AlarmdActionPrivate *priv = ALARMD_ACTION_GET_PRIVATE(object);
	ENTER_FUNC;
	switch (param_id) {
	case PROP_EVENT:
		if (priv->event) {
			g_object_remove_weak_pointer(G_OBJECT(priv->event),
					(gpointer *)&priv->event);
		}
		priv->event = g_value_get_object(value);
		if (priv->event) {
			g_object_add_weak_pointer(G_OBJECT(priv->event),
					(gpointer *)&priv->event);
		}
		break;
	case PROP_FLAGS:
		if (priv->flags & ALARM_EVENT_SHOW_ICON) {
			statusbar_hide_icon();
		}
		priv->flags = g_value_get_int(value);
		if (priv->flags & ALARM_EVENT_SHOW_ICON) {
			statusbar_show_icon();
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		LEAVE_FUNC;
		return;
	}

	LEAVE_FUNC;
}

static void _alarmd_action_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec)
{
	AlarmdActionPrivate *priv = ALARMD_ACTION_GET_PRIVATE(object);
	ENTER_FUNC;
	switch (param_id) {
	case PROP_EVENT:
		g_value_set_object(value, priv->event);
		break;
	case PROP_FLAGS:
		g_value_set_int(value, priv->flags);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
	}
	LEAVE_FUNC;
}

static void _alarmd_action_finalize(GObject *object)
{
	AlarmdActionPrivate *priv = ALARMD_ACTION_GET_PRIVATE(object);
	ENTER_FUNC;
	if (priv->flags & ALARM_EVENT_SHOW_ICON) {
		statusbar_hide_icon();
	}
	if (priv->event) {
		g_object_remove_weak_pointer(G_OBJECT(priv->event),
			       	(gpointer *)&priv->event);
	}
	G_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_ACTION)))->finalize(object);
	LEAVE_FUNC;

}

static void alarmd_action_class_init(AlarmdActionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	AlarmdObjectClass *aobject_class = ALARMD_OBJECT_CLASS(klass);
	ENTER_FUNC;

	g_type_class_add_private(klass, sizeof(AlarmdActionPrivate));

	gobject_class->set_property = _alarmd_action_set_property;
	gobject_class->get_property = _alarmd_action_get_property;
	gobject_class->finalize = _alarmd_action_finalize;

	aobject_class->get_saved_properties = _alarmd_action_get_saved_properties;

	klass->run = _alarmd_action_real_run;
	klass->need_power_up = _alarmd_action_real_need_power_up;
	klass->acknowledge = _alarmd_action_real_acknowledge;

	/**
	 * AlarmdAction::run
	 *
	 * This signal is emitted when the action should be run.
	 */
	action_signals[SIGNAL_RUN] = g_signal_new("run",
			G_TYPE_FROM_CLASS(gobject_class),
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			G_STRUCT_OFFSET(AlarmdActionClass, run),
			NULL, NULL,
			g_cclosure_marshal_VOID__BOOLEAN,
			G_TYPE_NONE, 1,
			G_TYPE_BOOLEAN);
	action_signals[SIGNAL_ACKNOWLEDGE] = g_signal_new("acknowledge",
			G_TYPE_FROM_CLASS(gobject_class),
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			G_STRUCT_OFFSET(AlarmdActionClass, acknowledge),
			NULL, NULL,
			g_cclosure_marshal_VOID__UINT,
			G_TYPE_NONE, 1,
			G_TYPE_UINT);

	g_object_class_install_property(gobject_class,
			PROP_EVENT,
			g_param_spec_object("event",
				"Event of the action.",
				"Event of the action.",
				ALARMD_TYPE_EVENT,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_FLAGS,
			g_param_spec_int("flags",
				"Flags of the action.",
				"Flags of the action.",
				G_MININT32,
				G_MAXINT32,
				0,
				G_PARAM_READABLE | G_PARAM_WRITABLE));

	LEAVE_FUNC;
}

static void _alarmd_action_real_run(AlarmdAction *action, gboolean delayed)
{
	ENTER_FUNC;
	(void)delayed;
	alarmd_action_acknowledge(action, ACK_NORMAL);
	LEAVE_FUNC;
}

void alarmd_action_acknowledge(AlarmdAction *action, guint ack_type)
{
	ENTER_FUNC;
	g_signal_emit(ALARMD_OBJECT(action), action_signals[SIGNAL_ACKNOWLEDGE], 0, ack_type);
	LEAVE_FUNC;
}

gboolean alarmd_action_need_power_up(AlarmdAction *action)
{
	AlarmdActionClass *klass = ALARMD_ACTION_GET_CLASS(action);
	gboolean retval;
	ENTER_FUNC;
	retval = klass->need_power_up(action);
	LEAVE_FUNC;
	return retval;
}

static gboolean _alarmd_action_real_need_power_up(AlarmdAction *action)
{
	AlarmdActionPrivate *priv = ALARMD_ACTION_GET_PRIVATE(action);
	ENTER_FUNC;
	DEBUG("Need: %s", priv->flags & ALARM_EVENT_BOOT ? "true" : "false");
	LEAVE_FUNC;
	return priv->flags & ALARM_EVENT_BOOT ? TRUE : FALSE;
}

static void _alarmd_action_real_acknowledge(AlarmdAction *action,
		AlarmdActionAckType ack_type)
{
	ENTER_FUNC;
	(void)action;
	(void)ack_type;

	update_mce_alarm_visibility();
	LEAVE_FUNC;
}

static GSList *_alarmd_action_get_saved_properties(void)
{
	guint i;
	GSList *retval = NULL;
	ENTER_FUNC;
	retval = ALARMD_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_ACTION)))->get_saved_properties();
	for (i = 0; i < S_COUNT; i++) {
		retval = g_slist_append(retval, (gpointer)saved_properties[i]);
	}
	LEAVE_FUNC;
	return retval;
}

