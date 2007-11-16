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

#include <glib-object.h>
#include <glib.h>
#include <dbus/dbus.h>

#include "include/alarm_event.h"

#include "actiondbus.h"
#include "rpc-dbus.h"
#include "rpc-systemui.h"
#include "debug.h"

static void alarmd_action_dbus_init(AlarmdActionDbus *action_dbus);
static void alarmd_action_dbus_class_init(AlarmdActionDbusClass *klass);
static void _alarmd_action_dbus_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec);
static void _alarmd_action_dbus_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec);
static void _alarmd_action_dbus_do_action(AlarmdActionDialog *action);
static void _alarmd_action_dbus_finalize(GObject *object);
static GSList *_alarmd_action_dbus_get_saved_properties(void);

enum properties {
	PROP_INTERFACE = 1,
	PROP_SERVICE,
	PROP_PATH,
	PROP_NAME,
	PROP_ARGS,
};

enum saved_props {
	S_INTERFACE,
	S_SERVICE,
	S_PATH,
	S_NAME,
	S_ARGS,
	S_COUNT
};

static const gchar * const saved_properties[S_COUNT] = {
	"interface",
	"service",
	"path",
	"name",
	"arguments",
};

typedef struct _AlarmdActionDbusPrivate AlarmdActionDbusPrivate;
struct _AlarmdActionDbusPrivate {
	gchar *interface;
	gchar *service;
	gchar *path;
	gchar *name;
	GValueArray *args;
};

#define ALARMD_ACTION_DBUS_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
			      ALARMD_TYPE_ACTION_DBUS, AlarmdActionDbusPrivate));

GType alarmd_action_dbus_get_type(void)
{
	static GType action_dbus_type = 0;

	if (!action_dbus_type)
	{
		static const GTypeInfo action_dbus_info =
		{
			sizeof (AlarmdActionDbusClass),
			NULL,
			NULL,
			(GClassInitFunc) alarmd_action_dbus_class_init,
			NULL,
			NULL,
			sizeof (AlarmdActionDbus),
			0,
			(GInstanceInitFunc) alarmd_action_dbus_init,
			NULL
		};

		action_dbus_type = g_type_register_static(ALARMD_TYPE_ACTION_DIALOG,
				"AlarmdActionDbus",
				&action_dbus_info, 0);
	}

	return action_dbus_type;
}

static void alarmd_action_dbus_class_init(AlarmdActionDbusClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	AlarmdObjectClass *aobject_class = ALARMD_OBJECT_CLASS(klass);
	AlarmdActionDialogClass *actiond_class = ALARMD_ACTION_DIALOG_CLASS(klass);

	ENTER_FUNC;
	g_type_class_add_private(klass, sizeof(AlarmdActionDbusPrivate));

	gobject_class->set_property = _alarmd_action_dbus_set_property;
	gobject_class->get_property = _alarmd_action_dbus_get_property;
	gobject_class->finalize = _alarmd_action_dbus_finalize;
	aobject_class->get_saved_properties = _alarmd_action_dbus_get_saved_properties;
	
	actiond_class->do_action = _alarmd_action_dbus_do_action;

	g_object_class_install_property(gobject_class,
			PROP_INTERFACE,
			g_param_spec_string("interface",
				"Interface for the dbus call.",
				"Interface the method/signal should be sent to/from.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_SERVICE,
			g_param_spec_string("service",
				"Service for the dbus call.",
				"Service the method should be sent to.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_PATH,
			g_param_spec_string("path",
				"Path for the dbus call.",
				"Path of the method/signal.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_NAME,
			g_param_spec_string("name",
				"Name for the dbus call.",
				"Name of the method/signal.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_ARGS,
			g_param_spec_boxed("arguments",
				"Arguments for the dbus call.",
				"Array of arguments the dbus call is done with",
				G_TYPE_VALUE_ARRAY,
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	LEAVE_FUNC;
}

static void alarmd_action_dbus_init(AlarmdActionDbus *action_dbus)
{
	AlarmdActionDbusPrivate *priv = ALARMD_ACTION_DBUS_GET_PRIVATE(action_dbus);
	ENTER_FUNC;

	priv->interface = NULL;
	priv->service = NULL;
	priv->path = NULL;
	priv->name = NULL;
	priv->args = NULL;
	LEAVE_FUNC;
}

static void _alarmd_action_dbus_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec)
{
	AlarmdActionDbusPrivate *priv = ALARMD_ACTION_DBUS_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_INTERFACE:
		if (priv->interface) {
			g_free(priv->interface);
		}
		priv->interface = g_strdup(g_value_get_string(value));
		break;
	case PROP_SERVICE:
		if (priv->service) {
			g_free(priv->service);
		}
		priv->service = g_strdup(g_value_get_string(value));
		break;
	case PROP_PATH:
		if (priv->path) {
			g_free(priv->path);
		}
		priv->path = g_strdup(g_value_get_string(value));
		break;
	case PROP_NAME:
		if (priv->name) {
			g_free(priv->name);
		}
		priv->name = g_strdup(g_value_get_string(value));
		break;
	case PROP_ARGS:
		if (priv->args) {
			g_value_array_free(priv->args);
		}
		priv->args = g_value_dup_boxed(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		LEAVE_FUNC;
		return;
	}
	alarmd_object_changed(ALARMD_OBJECT(object));
	LEAVE_FUNC;
}

static void _alarmd_action_dbus_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec)
{
	AlarmdActionDbusPrivate *priv = ALARMD_ACTION_DBUS_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_INTERFACE:
		g_value_set_string(value, priv->interface);
		break;
	case PROP_SERVICE:
		g_value_set_string(value, priv->service);
		break;
	case PROP_PATH:
		g_value_set_string(value, priv->path);
		break;
	case PROP_NAME:
		g_value_set_string(value, priv->name);
		break;
	case PROP_ARGS:
		g_value_set_boxed(value, priv->args);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
	}
	LEAVE_FUNC;
}

static void _alarmd_action_dbus_finalize(GObject *object)
{
	AlarmdActionDbusPrivate *priv = ALARMD_ACTION_DBUS_GET_PRIVATE(object);
	ENTER_FUNC;

	if (priv->interface != NULL) {
		g_free(priv->interface);
	}
	if (priv->service != NULL) {
		g_free(priv->service);
	}
	if (priv->path != NULL) {
		g_free(priv->path);
	}
	if (priv->name != NULL) {
		g_free(priv->name);
	}
	if (priv->args != NULL) {
		g_value_array_free(priv->args);
	}

	G_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_ACTION_DBUS)))->finalize(object);
	LEAVE_FUNC;
}

static void _alarmd_action_dbus_do_action(AlarmdActionDialog *action)
{
	AlarmdActionDbusPrivate *priv = ALARMD_ACTION_DBUS_GET_PRIVATE(action);
	DBusConnection *conn;
	guint flags;

	ENTER_FUNC;

	g_object_get(action, "flags", &flags, NULL);
	conn = get_dbus_connection(((flags & ALARM_EVENT_SYSTEM) ? DBUS_BUS_SYSTEM : DBUS_BUS_SESSION));
	if (conn != NULL) {
		g_debug("Sending DBus message to %s\n", priv->path);
		dbus_do_call_gvalue(conn, NULL, (flags & ALARM_EVENT_ACTIVATION), priv->service, priv->path, priv->interface, priv->name, priv->args);
		dbus_connection_unref(conn);
	} else {
		g_warning("Could not get DBus bus for sending message.");
	}
	alarmd_action_acknowledge(ALARMD_ACTION(action), ACK_NORMAL);

	LEAVE_FUNC;
}

static GSList *_alarmd_action_dbus_get_saved_properties(void)
{
	guint i;
	GSList *retval = NULL;
	ENTER_FUNC;
	retval = ALARMD_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_ACTION_DBUS)))->get_saved_properties();
	for (i = 0; i < S_COUNT; i++) {
		retval = g_slist_append(retval, (gpointer)saved_properties[i]);
	}
	LEAVE_FUNC;
	return retval;
}
