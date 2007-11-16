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

#include "actionexec.h"
#include "rpc-dbus.h"
#include "rpc-systemui.h"
#include "debug.h"

static void alarmd_action_exec_init(AlarmdActionExec *action_exec);
static void alarmd_action_exec_class_init(AlarmdActionExecClass *klass);
static void _alarmd_action_exec_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec);
static void _alarmd_action_exec_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec);
static void _alarmd_action_exec_do_action(AlarmdActionDialog *action);
static void _alarmd_action_exec_finished(GPid pid, gint status, gpointer user_data);
static void _alarmd_action_exec_finalize(GObject *object);
static GSList *_alarmd_action_exec_get_saved_properties(void);

enum properties {
	PROP_PATH = 1,
};

enum saved_props {
	S_PATH,
	S_COUNT
};

static const gchar * const saved_properties[S_COUNT] = {
	"path"
};

typedef struct _AlarmdActionExecPrivate AlarmdActionExecPrivate;
struct _AlarmdActionExecPrivate {
	gchar *path;
};

#define ALARMD_ACTION_EXEC_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
			      ALARMD_TYPE_ACTION_EXEC, AlarmdActionExecPrivate));

GType alarmd_action_exec_get_type(void)
{
	static GType action_exec_type = 0;

	if (!action_exec_type)
	{
		static const GTypeInfo action_exec_info =
		{
			sizeof (AlarmdActionExecClass),
			NULL,
			NULL,
			(GClassInitFunc) alarmd_action_exec_class_init,
			NULL,
			NULL,
			sizeof (AlarmdActionExec),
			0,
			(GInstanceInitFunc) alarmd_action_exec_init,
			NULL
		};

		action_exec_type = g_type_register_static(ALARMD_TYPE_ACTION_DIALOG,
				"AlarmdActionExec",
				&action_exec_info, 0);
	}

	return action_exec_type;
}

static void alarmd_action_exec_class_init(AlarmdActionExecClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	AlarmdObjectClass *aobject_class = ALARMD_OBJECT_CLASS(klass);
	AlarmdActionDialogClass *actiond_class = ALARMD_ACTION_DIALOG_CLASS(klass);

	ENTER_FUNC;
	g_type_class_add_private(klass, sizeof(AlarmdActionExecPrivate));

	gobject_class->set_property = _alarmd_action_exec_set_property;
	gobject_class->get_property = _alarmd_action_exec_get_property;
	gobject_class->finalize = _alarmd_action_exec_finalize;
	aobject_class->get_saved_properties = _alarmd_action_exec_get_saved_properties;
	actiond_class->do_action = _alarmd_action_exec_do_action;

	g_object_class_install_property(gobject_class,
			PROP_PATH,
			g_param_spec_string("path",
				"Path for the dbus call.",
				"Path of the method/signal.",
				NULL,
				G_PARAM_READABLE | G_PARAM_WRITABLE));

	LEAVE_FUNC;
}

static void alarmd_action_exec_init(AlarmdActionExec *action_exec)
{
	AlarmdActionExecPrivate *priv = ALARMD_ACTION_EXEC_GET_PRIVATE(action_exec);
	ENTER_FUNC;

	priv->path = NULL;
	LEAVE_FUNC;
}

static void _alarmd_action_exec_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec)
{
	AlarmdActionExecPrivate *priv = ALARMD_ACTION_EXEC_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_PATH:
		if (priv->path) {
			g_free(priv->path);
		}
		priv->path = g_strdup(g_value_get_string(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		LEAVE_FUNC;
		return;
	}
	alarmd_object_changed(ALARMD_OBJECT(object));
	LEAVE_FUNC;
}

static void _alarmd_action_exec_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec)
{
	AlarmdActionExecPrivate *priv = ALARMD_ACTION_EXEC_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_PATH:
		g_value_set_string(value, priv->path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
	}
	LEAVE_FUNC;
}

static void _alarmd_action_exec_finalize(GObject *object)
{
	AlarmdActionExecPrivate *priv = ALARMD_ACTION_EXEC_GET_PRIVATE(object);
	ENTER_FUNC;

	if (priv->path != NULL) {
		g_free(priv->path);
	}
	G_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_ACTION_EXEC)))->finalize(object);
	LEAVE_FUNC;
}

static void _alarmd_action_exec_do_action(AlarmdActionDialog *action)
{
	AlarmdActionExecPrivate *priv = ALARMD_ACTION_EXEC_GET_PRIVATE(action);
	gchar **argv;
	gint argc;
	GPid pid;

	ENTER_FUNC;
	
	if (priv->path) {
		if (g_shell_parse_argv(priv->path, &argc, &argv, NULL)) {
			g_debug("Running command %s", priv->path);
			g_spawn_async(g_get_home_dir(),
					argv,
					NULL,
					G_SPAWN_SEARCH_PATH |
				       	G_SPAWN_DO_NOT_REAP_CHILD,
					NULL,
					NULL,
					&pid,
					NULL);
			g_object_ref(action);
			g_child_watch_add_full(G_PRIORITY_DEFAULT,
					pid,
					_alarmd_action_exec_finished,
					action,
					g_object_unref);
			g_strfreev(argv);
			argv = NULL;
		} else {
			g_warning("Could not parse command.");
			_alarmd_action_exec_finished(0, 0, action);
		}
	} else {
		g_warning("No command to execute.");
		_alarmd_action_exec_finished(0, 0, action);
	}

	LEAVE_FUNC;
}

static void _alarmd_action_exec_finished(GPid pid, gint status, gpointer user_data)
{
	ENTER_FUNC;
	(void)pid;
	(void)status;

	if (pid) {
		g_spawn_close_pid(pid);
	}
	alarmd_action_acknowledge(ALARMD_ACTION(user_data), ACK_NORMAL);
	LEAVE_FUNC;
}

static GSList *_alarmd_action_exec_get_saved_properties(void)
{
	guint i;
	GSList *retval = NULL;
	ENTER_FUNC;
	retval = ALARMD_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_ACTION_EXEC)))->get_saved_properties();
	for (i = 0; i < S_COUNT; i++) {
		retval = g_slist_append(retval, (gpointer)saved_properties[i]);
	}
	LEAVE_FUNC;
	return retval;
}
