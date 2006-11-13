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
#ifdef G_MODULE
#include <gmodule.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
#include "timer-loader.h"
#include "include/timer-interface.h"

typedef struct _TimerModule TimerModule;
struct _TimerModule {
	TimerPlugin plugin;
	gchar *module_path;
#ifdef G_MODULE
	GModule *loaded;
#else
	void *loaded;
#endif
};

static gboolean _module_load(TimerModule *mod);
static gint _timer_plugin_compare(gconstpointer lval, gconstpointer rval);
static void _module_free(TimerModule *mod);
static void _module_foreach_free(gpointer mod, gpointer user_data);

static gint _timer_plugin_compare(gconstpointer lval, gconstpointer rval)
{
	const TimerModule *lpl = (const TimerModule *)lval;
	const TimerModule *rpl = (const TimerModule *)rval;

	return rpl->plugin.priority - lpl->plugin.priority;
}

GSList *load_timer_plugins(const gchar *path)
{
	GDir *dir = NULL;
	GSList *retval = NULL;
	gboolean have_power_up = FALSE;
	const gchar *module_name = NULL;

	if (!path) {
		path = PLUGINDIR;
	}
	dir = g_dir_open(path, 0, NULL);

	if (dir == NULL) {
		g_warning("Opening dir %s failed", path);
		return NULL;
	}
	while ((module_name = g_dir_read_name(dir))) {
		TimerModule *module = NULL;

		if (!g_str_has_suffix(module_name, ".so")) {
			continue;
		}
		
		module = g_new0(TimerModule, 1);
		module->module_path = g_build_filename(path, module_name, NULL);
		module->plugin.is_startup = TRUE;
		if (!_module_load(module)) {
			_module_free(module);
			continue;
		}
		retval = g_slist_insert_sorted(retval, (gpointer)module, _timer_plugin_compare);
	}
	g_dir_close(dir);
	
	if (retval) {
		if (((TimerModule *)retval->data)->plugin.can_power_up) {
			have_power_up = TRUE;
		}
		GSList *iter;
		for (iter = retval->next; iter != NULL; iter = iter->next)
		{
			TimerModule *mod = (TimerModule *)iter->data;
			if (!have_power_up && mod->plugin.can_power_up) {
				have_power_up = TRUE;
				continue;
			}
			if (mod->plugin.plugin_deinit != NULL) {
				mod->plugin.plugin_deinit(&mod->plugin);
			}
#ifdef G_MODULE
			g_module_close(mod->loaded);
#else
			dlclose(mod->loaded);
#endif
			mod->loaded = NULL;
			mod->plugin.set_event = NULL;
			mod->plugin.remove_event = NULL;
			mod->plugin.get_time = NULL;
			mod->plugin.plugin_deinit = NULL;
			mod->plugin.plugin_data = NULL;
		}
	}

	return retval;
}

TimerPlugin *timers_get_plugin(GSList *plugins, gboolean need_power_up)
{
	TimerPlugin *retval = NULL;

	if (!plugins) {
		return NULL;
	}

	if (!need_power_up || (need_power_up && ((TimerModule *)plugins->data)->plugin.can_power_up)) {
		_module_load((TimerModule *)plugins->data);
		retval = &((TimerModule *)plugins->data)->plugin;
	} else {
		GSList *iter;
		for (iter = plugins->next; iter != NULL; iter = iter->next) {
			TimerModule *mod = (TimerModule *)iter->data;

			if (mod->plugin.can_power_up) {
				_module_load(mod);
				retval = &mod->plugin;
				break;
			}
		}
	}
	return retval;
}

static void _module_foreach_free(gpointer mod, gpointer user_data)
{
	(void)user_data;
	_module_free((TimerModule *)mod);
}

void close_timer_plugins(GSList *plugins) {
	g_slist_foreach(plugins, _module_foreach_free, NULL);
	g_slist_free(plugins);
}

static void _set_startup(gpointer mod, gpointer user_data)
{
	gboolean is_startup = GPOINTER_TO_INT(user_data);
	TimerModule *plug = mod;
	plug->plugin.is_startup = is_startup;
}

void timer_plugins_set_startup(GSList *plugins, gboolean is_startup)
{
	g_slist_foreach(plugins, _set_startup, GINT_TO_POINTER(is_startup));
}

static void _module_free(TimerModule *mod)
{
	if (mod->plugin.plugin_deinit != NULL) {
		mod->plugin.plugin_deinit(&mod->plugin);
	}
	if (mod->loaded) {
#ifdef G_MODULE
		g_module_close(mod->loaded);
#else
		dlclose(mod->loaded);
#endif
	}
	g_free(mod->module_path);
	g_free(mod);
}

static gboolean _module_load(TimerModule *mod) {
	union {
		gboolean (*init_mod)(TimerPlugin *plugin);
		gpointer pointer;
	} no_warnings;
	if (mod->loaded) {
		return TRUE;
	}
#ifdef G_MODULE
	mod->loaded = g_module_open(mod->module_path, G_MODULE_BIND_LAZY);
#else
	mod->loaded = dlopen(mod->module_path, RTLD_LAZY);
#endif
	if (mod->loaded == NULL) {
#ifdef G_MODULE
		g_warning("Failed to load %s", mod->module_path);
#else
		g_warning("Failed to load %s: %s", mod->module_path, dlerror());
#endif
		return FALSE;
	}
#ifdef G_MODULE
	if (!g_module_symbol(mod->loaded, "plugin_initialize", &no_warnings.pointer)) {
#else
	if (!(no_warnings.pointer = (gboolean (*)(TimerPlugin *plugin))dlsym(mod->loaded, "plugin_initialize"))) {
#endif
		g_warning("Could not find init func on %s", mod->module_path);
		return FALSE;
	}
	if (!no_warnings.init_mod(&mod->plugin))
	{
		g_warning("Module %s initialization failed", mod->module_path);
		return FALSE;
	}

	return TRUE;
}
