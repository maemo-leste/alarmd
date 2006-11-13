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

#include "include/alarm_event.h"
#include "rpc-systemui.h"
#include "rpc-dbus.h"
#include "rpc-ic.h"
#include "rpc-mce.h"
#include "actiondialog.h"
#include "event.h"
#include "debug.h"

#define STATUSBAR_SERVICE "com.nokia.statusbar"

static void alarmd_action_dialog_init(AlarmdActionDialog *action_dialog);
static void alarmd_action_dialog_class_init(AlarmdActionDialogClass *klass);
static void _alarmd_action_dialog_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec);
static void _alarmd_action_dialog_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec);
static void _alarmd_action_dialog_run(AlarmdAction *action, gboolean delayed);
static void _alarmd_action_dialog_real_run(gpointer action, gboolean snoozed);
static void _alarmd_action_dialog_delayed_run(gpointer action);
static void _alarmd_action_dialog_finalize(GObject *object);
static GSList *_alarmd_action_dialog_get_saved_properties(void);
static void _alarmd_action_dialog_real_do_action(AlarmdActionDialog *dialog);
static void _alarmd_action_dialog_snooze_powerup(gpointer user_data, gboolean power_up);
static void _alarmd_action_dialog_powerup(gpointer user_data, gboolean power_up);
static void _alarmd_action_dialog_connected(gpointer act);

enum properties {
	PROP_TITLE = 1,
	PROP_MESSAGE,
	PROP_SOUND,
	PROP_ICON,
};

enum saved_props {
	S_TITLE,
	S_MESSAGE,
	S_SOUND,
	S_ICON,
	S_COUNT
};

static const gchar * const saved_properties[S_COUNT] = {
	"title",
	"message",
	"sound",
	"icon",
};

enum Pending {
	PEND_SERVICE = 1 << 0,
	PEND_CONN = 1 << 1,
};

typedef struct _AlarmdActionDialogPrivate AlarmdActionDialogPrivate;
struct _AlarmdActionDialogPrivate {
	gchar *title;
	gchar *message;
	gchar *sound;
	gchar *icon;
	enum Pending pending;
};

#define ALARMD_ACTION_DIALOG_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
			      ALARMD_TYPE_ACTION_DIALOG, AlarmdActionDialogPrivate));

GType alarmd_action_dialog_get_type(void)
{
	static GType action_dialog_type = 0;

	if (!action_dialog_type)
	{
		static const GTypeInfo action_dialog_info =
		{
			sizeof (AlarmdActionDialogClass),
			NULL,
			NULL,
			(GClassInitFunc) alarmd_action_dialog_class_init,
			NULL,
			NULL,
			sizeof (AlarmdActionDialog),
			0,
			(GInstanceInitFunc) alarmd_action_dialog_init,
			NULL
		};

		action_dialog_type = g_type_register_static(ALARMD_TYPE_ACTION,
				"AlarmdActionDialog",
				&action_dialog_info, 0);
	}

	return action_dialog_type;
}

void alarmd_action_dialog_do_action(AlarmdActionDialog *dialog)
{
	ENTER_FUNC;
	ALARMD_ACTION_DIALOG_GET_CLASS(dialog)->do_action(dialog);
	LEAVE_FUNC;
}

static void alarmd_action_dialog_class_init(AlarmdActionDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	AlarmdObjectClass *aobject_class = ALARMD_OBJECT_CLASS(klass);
	AlarmdActionClass *action_class = ALARMD_ACTION_CLASS(klass);

	ENTER_FUNC;
	g_type_class_add_private(klass, sizeof(AlarmdActionDialogPrivate));

	gobject_class->set_property = _alarmd_action_dialog_set_property;
	gobject_class->get_property = _alarmd_action_dialog_get_property;
	gobject_class->finalize = _alarmd_action_dialog_finalize;
	aobject_class->get_saved_properties = _alarmd_action_dialog_get_saved_properties;
	action_class->run = _alarmd_action_dialog_run;
	klass->do_action = _alarmd_action_dialog_real_do_action;

	g_object_class_install_property(gobject_class,
			PROP_TITLE,
			g_param_spec_string("title",
				"Title of the dialog.",
				"The title for the alarm dialog.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_MESSAGE,
			g_param_spec_string("message",
				"Message shown by dialog.",
				"Message that will be shown in the alarm dialog.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_SOUND,
			g_param_spec_string("sound",
				"Sound played when alarm is due.",
				"Sound that should be played when showing the dialog.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_ICON,
			g_param_spec_string("icon",
				"Icon for dialog.",
				"Icon that should be shown in the alarm dialog.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
				
	LEAVE_FUNC;
}

static void alarmd_action_dialog_init(AlarmdActionDialog *action_dialog)
{
	AlarmdActionDialogPrivate *priv = ALARMD_ACTION_DIALOG_GET_PRIVATE(action_dialog);
	ENTER_FUNC;

	priv->title = NULL;
	priv->message = NULL;
	priv->sound = NULL;
	priv->icon = NULL;
	priv->pending = 0;
	LEAVE_FUNC;
}

static void _alarmd_action_dialog_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec)
{
	AlarmdActionDialogPrivate *priv = ALARMD_ACTION_DIALOG_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_TITLE:
		if (priv->title) {
			g_free(priv->title);
		}
		priv->title = g_strdup(g_value_get_string(value));
		break;
	case PROP_MESSAGE:
		if (priv->message) {
			g_free(priv->message);
		}
		priv->message = g_strdup(g_value_get_string(value));
		break;
	case PROP_SOUND:
		if (priv->sound) {
			g_free(priv->sound);
		}
		priv->sound = g_strdup(g_value_get_string(value));
		break;
	case PROP_ICON:
		if (priv->icon) {
			g_free(priv->icon);
		}
		priv->icon = g_strdup(g_value_get_string(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		LEAVE_FUNC;
		return;
	}
	alarmd_object_changed(ALARMD_OBJECT(object));
	LEAVE_FUNC;
}

static void _alarmd_action_dialog_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec)
{
	AlarmdActionDialogPrivate *priv = ALARMD_ACTION_DIALOG_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_TITLE:
		g_value_set_string(value, priv->title);
		break;
	case PROP_MESSAGE:
		g_value_set_string(value, priv->message);
		break;
	case PROP_SOUND:
		g_value_set_string(value, priv->sound);
		break;
	case PROP_ICON:
		g_value_set_string(value, priv->icon);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
	}
	LEAVE_FUNC;
}

static void _alarmd_action_dialog_finalize(GObject *object)
{
	AlarmdActionDialogPrivate *priv = ALARMD_ACTION_DIALOG_GET_PRIVATE(object);
	ENTER_FUNC;

	if (priv->title != NULL) {
		g_free(priv->title);
	}
	if (priv->message != NULL) {
		g_free(priv->message);
	}
	if (priv->sound != NULL) {
		g_free(priv->sound);
	}
	if (priv->icon != NULL) {
		g_free(priv->icon);
	}
	if (priv->pending & PEND_SERVICE) {
		dbus_unwatch_name(STATUSBAR_SERVICE, _alarmd_action_dialog_delayed_run, object);
	}
	if (priv->pending & PEND_CONN) {
		ic_unwait_connection(_alarmd_action_dialog_connected, object);
	}
	G_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_ACTION_DIALOG)))->finalize(object);
	LEAVE_FUNC;
}

static void _alarmd_action_dialog_delayed_run(gpointer action)
{
	AlarmdActionDialogPrivate *priv = ALARMD_ACTION_DIALOG_GET_PRIVATE(action);
	ENTER_FUNC;
	priv->pending &= ~PEND_SERVICE;
	alarmd_action_dialog_do_action(ALARMD_ACTION_DIALOG(action));
	LEAVE_FUNC;
}

static void _alarmd_action_dialog_real_run(gpointer action, gboolean snoozed)
{
	AlarmdActionDialogPrivate *priv = ALARMD_ACTION_DIALOG_GET_PRIVATE(action);
	gint32 flags;
	ENTER_FUNC;

	g_object_get(action, "flags", &flags, NULL);

	if (snoozed) {
		if (!(flags & ALARM_EVENT_NO_DIALOG)) {
			if (systemui_is_acting_dead()) {
				systemui_powerup_dialog_queue_append(_alarmd_action_dialog_snooze_powerup, action);
			}
		}
		alarmd_action_acknowledge(ALARMD_ACTION(action), ACK_SNOOZE);
	} else if (flags & ALARM_EVENT_ACTDEAD) {
		if (!(flags & ALARM_EVENT_NO_DIALOG)) {
			if (systemui_is_acting_dead()) {
				systemui_powerup_dialog_queue_append(_alarmd_action_dialog_powerup, action);
			} else {
				alarmd_action_dialog_do_action(ALARMD_ACTION_DIALOG(action));
			}
		} else {
			alarmd_action_dialog_do_action(ALARMD_ACTION_DIALOG(action));
		}
	} else {
		DBusConnection *system_bus = get_dbus_connection(DBUS_BUS_SYSTEM);
		mce_request_powerup(system_bus);
		dbus_connection_unref(system_bus);
		priv->pending |= PEND_SERVICE;
		dbus_watch_name(STATUSBAR_SERVICE, _alarmd_action_dialog_delayed_run, action);
	}

	LEAVE_FUNC;
}

static void _alarmd_action_dialog_connected(gpointer act)
{
	AlarmdAction *action = act;
	AlarmdActionDialogPrivate *priv = ALARMD_ACTION_DIALOG_GET_PRIVATE(action);
	gint32 flags;
	ENTER_FUNC;
	priv->pending &= ~PEND_CONN;

	g_object_get(action, "flags", &flags, NULL);

	if (!(flags & ALARM_EVENT_NO_DIALOG)) {
		AlarmdEvent *event;
		time_t alarm_time;

		g_object_get(action, "event", &event, NULL);
		alarm_time = alarmd_event_get_time(event);
		g_object_unref(event);

		systemui_alarm_dialog_queue_append(alarm_time, priv->title, priv->message, priv->sound, priv->icon, !(flags & ALARM_EVENT_NO_SNOOZE), _alarmd_action_dialog_real_run, action);
		update_mce_alarm_visibility();
	} else {
		_alarmd_action_dialog_real_run(action, FALSE);
	}

	LEAVE_FUNC;
}

static void _alarmd_action_dialog_run(AlarmdAction *action, gboolean delayed)
{
	AlarmdActionDialogPrivate *priv = ALARMD_ACTION_DIALOG_GET_PRIVATE(action);
	guint32 flags;
	ENTER_FUNC;

	g_object_get(action, "flags", &flags, NULL);
	if (delayed && !(flags & ALARM_EVENT_RUN_DELAYED)) {
		alarmd_action_acknowledge(action, ACK_NORMAL);
	} else if ((flags & ALARM_EVENT_CONNECTED) &&
			!ic_get_connected()) {
		ic_wait_connection(_alarmd_action_dialog_connected, action);
		priv->pending |= PEND_CONN;
	} else if (!(flags & ALARM_EVENT_NO_DIALOG)) {
		AlarmdEvent *event;
		time_t alarm_time;

		g_object_get(action, "event", &event, NULL);
		alarm_time = alarmd_event_get_time(event);
		g_object_unref(event);

		systemui_alarm_dialog_queue_append(alarm_time, priv->title, priv->message, priv->sound, priv->icon, !(flags & ALARM_EVENT_NO_SNOOZE), _alarmd_action_dialog_real_run, action);
		update_mce_alarm_visibility();
	} else {
		_alarmd_action_dialog_real_run(action, FALSE);
	}

	LEAVE_FUNC;
}

static GSList *_alarmd_action_dialog_get_saved_properties(void)
{
	guint i;
	GSList *retval = NULL;
	ENTER_FUNC;
	retval = ALARMD_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_ACTION_DIALOG)))->get_saved_properties();
	for (i = 0; i < S_COUNT; i++) {
		retval = g_slist_append(retval, (gpointer)saved_properties[i]);
	}
	LEAVE_FUNC;
	return retval;
}

static void _alarmd_action_dialog_real_do_action(AlarmdActionDialog *dialog)
{
	ENTER_FUNC;
	alarmd_action_acknowledge(ALARMD_ACTION(dialog), ACK_NORMAL);
	LEAVE_FUNC;
}

static void _alarmd_action_dialog_powerup(gpointer user_data, gboolean power_up)
{
	ENTER_FUNC;

	if (power_up) {
		DBusConnection *system_bus =
		       	get_dbus_connection(DBUS_BUS_SYSTEM);
		if (system_bus) {
			mce_request_powerup(system_bus);
			dbus_connection_unref(system_bus);
		}
	}
	alarmd_action_acknowledge(ALARMD_ACTION(user_data), ACK_NORMAL);
	update_mce_alarm_visibility();
	LEAVE_FUNC;
}

static void _alarmd_action_dialog_snooze_powerup(gpointer user_data, gboolean power_up)
{
	ENTER_FUNC;

	if (power_up) {
		DBusConnection *system_bus =
		       	get_dbus_connection(DBUS_BUS_SYSTEM);
		if (system_bus) {
			mce_request_powerup(system_bus);
			dbus_connection_unref(system_bus);
		}
	}
	alarmd_action_acknowledge(ALARMD_ACTION(user_data), ACK_SNOOZE);
	update_mce_alarm_visibility();
	LEAVE_FUNC;
}
