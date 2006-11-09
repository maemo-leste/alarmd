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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include "include/alarm_event.h"
#include "initialize.h"
#include "timer-loader.h"
#include "xmlobjectfactory.h"
#include "queue.h"
#include "eventrecurring.h"
#include "action.h"
#include "actiondbus.h"
#include "actionexec.h"
#include "debug.h"

struct files_t {
       gchar *queue_file;
       gchar *next_time_file;
       gchar *next_mode_file;
};

static void _write_data(AlarmdQueue *queue, const struct files_t *files);
static void _free_data(struct files_t *files);
static void _queue_changed(gpointer user_data);

AlarmdQueue *init_queue(const gchar *queue_file, const gchar *next_time_file,
               const gchar *next_mode_file)
{
       AlarmdQueue *retval = NULL;
       xmlDoc *doc = NULL;
       xmlNode *root = NULL;
       struct files_t *files = NULL;
       GSList *timers = NULL;
       gboolean do_save = FALSE;

       if (queue_file == NULL) {
               return NULL;
       }

       retval = alarmd_queue_new();
       timers = load_timer_plugins(NULL);

       g_object_set(retval, "timer", timers_get_plugin(timers, FALSE), NULL);
       g_object_set(retval, "timer_powerup", timers_get_plugin(timers, TRUE), NULL);
       g_object_set_data_full(G_OBJECT(retval), "timers", timers,
                       (GDestroyNotify)close_timer_plugins);

       DEBUG("%s", queue_file);

       doc = xmlReadFile(queue_file, NULL, 0);

       if (doc) {
               root = xmlDocGetRootElement(doc);

               if (strcmp(root->name, "queue") == 0) {
                       guint n_params, i;
                       GParameter *param = elements_to_parameters(root,
                                       &n_params);
                       gulong signal_id;

                       for (i = 0; i < n_params; i++) {
                               g_object_set_property(G_OBJECT(retval), param[i].name, &param[i].value);
                       }
                       alarmd_gparameterv_free(param, n_params);
                       signal_id = g_signal_connect_swapped(retval, "changed",
                                       G_CALLBACK(_queue_changed),
                                       &do_save);

                       for (root = root->children; root != NULL; root = root->next) {
                               AlarmdObject *object = NULL;
                               if (root->type != XML_ELEMENT_NODE) {
                                       continue;
                               }
                               object = object_factory(root);

                               if (!object) continue;

                               if (ALARMD_IS_EVENT(object)) {
                                       alarmd_queue_add_event(retval, ALARMD_EVENT(object));
                               }
                               g_object_unref(object);
                       }

                       g_signal_handler_disconnect(retval, signal_id);
               }

               xmlFreeDoc(doc);
               xmlCleanupParser();
       }

       DEBUG("Connecting...");
       files = g_new(struct files_t, 1);
       files->queue_file = g_strdup(queue_file);
       files->next_time_file = g_strdup(next_time_file);
       files->next_mode_file = g_strdup(next_mode_file);

       if (do_save) {
               _write_data(retval, files);
       }
       g_signal_connect_data(retval, "changed", (GCallback)_write_data, files, (GClosureNotify)_free_data, G_CONNECT_AFTER);

       timer_plugins_set_startup(timers, FALSE);

       return retval;
}

void alarmd_type_init(void)
{
       (void)ALARMD_TYPE_EVENT_RECURRING;
       (void)ALARMD_TYPE_ACTION;
       (void)ALARMD_TYPE_ACTION_DBUS;
       (void)ALARMD_TYPE_ACTION_EXEC;
}

static void _write_data(AlarmdQueue *queue, const struct files_t *files)
{
       ENTER_FUNC;
       xmlDoc *doc = xmlNewDoc("1.0");
       xmlNode *root_node = alarmd_object_to_xml(ALARMD_OBJECT(queue));
       glong *events = NULL;
       guint n_events = 0;
       FILE *write = NULL;
       gint flags = 0;
       time_t event_time = 0;
       const char *mode = "n/a";

       xmlDocSetRootElement(doc, root_node);

       xmlSaveFormatFileEnc(files->queue_file, doc, "UTF-8", 1);

       xmlFreeDoc(doc);
       xmlCleanupParser();

       events = alarmd_queue_query_events(queue, 0, G_MAXINT64,
                       ALARM_EVENT_BOOT, ALARM_EVENT_BOOT,
                       &n_events);

       if (n_events > 0) {
               AlarmdEvent *event = alarmd_queue_get_event(queue, events[0]);
               event_time = alarmd_event_get_time(event);
               AlarmdAction *action = NULL;
               g_object_get(event, "action", &action, NULL);
               if (action) {
                       g_object_get(action, "flags", &flags, NULL);
                       g_object_unref(action);
               }
               mode = (flags & ALARM_EVENT_ACTDEAD) ? "actdead" : "powerup";
               g_free(events);
       }

       write = fopen(files->next_time_file, "w");
       if (write) {
               fprintf(write, "%ld\n", event_time);
               fclose(write);
       }
       write = fopen(files->next_mode_file, "w");
       if (write) {
               fprintf(write, "%s\n", mode);
               fclose(write);
       }

       LEAVE_FUNC;
}

static void _free_data(struct files_t *files)
{
       g_free(files->queue_file);
       g_free(files->next_time_file);
       g_free(files->next_mode_file);
       g_free(files);
}

static void _queue_changed(gpointer user_data)
{
       gboolean *changed = user_data;
       *changed = TRUE;
}
