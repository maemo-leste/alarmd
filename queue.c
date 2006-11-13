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
#include "rpc-systemui.h"
#include "xml-common.h"
#include "debug.h"
#include "queue.h"

static void alarmd_queue_init(AlarmdQueue *queue);
static void alarmd_queue_class_init(AlarmdQueueClass *klass);
static void _alarmd_queue_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec);
static void _alarmd_queue_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec);

static void _alarmd_queue_real_time_changed(AlarmdObject *object);
static void _alarmd_queue_finalize(GObject *object);
static glong _alarmd_queue_real_add_event(AlarmdQueue *queue, AlarmdEvent *event);
static gboolean _alarmd_queue_real_remove_event(AlarmdQueue *queue, glong event_id);
static AlarmdEvent *_alarmd_queue_real_get_event(AlarmdQueue *queue, glong event_id);
static xmlNode *_alarmd_queue_to_xml(AlarmdObject *object);
static void _alarmd_queue_event_dequeued(AlarmdQueue *queue, AlarmdEvent *event);
static void _alarmd_queue_event_queued(AlarmdQueue *queue, gpointer plugin, AlarmdEvent *event);
static void _alarmd_queue_event_changed(AlarmdQueue *queue, AlarmdEvent *event);
static void _alarmd_queue_event_fired(AlarmdQueue *queue, gboolean delayed,
	       	AlarmdEvent *event);
static void _alarmd_queue_event_cancelled(AlarmdQueue *queue, AlarmdEvent *event);

static gint _alarmd_event_compare_func(gconstpointer lval, gconstpointer rval);
static AlarmdEvent *_alarmd_queue_find_event(GSList *list, glong event_id);
static AlarmdEvent *_alarmd_queue_find_first_event(GSList *list, gboolean need_power_up);
static glong *_alarmd_queue_real_query_events(AlarmdQueue *queue, gint64 start_time, gint64 end_time, gint32 flag_mask, gint32 flags, guint *n_events);
static GSList *_alarmd_queue_get_saved_properties(void);

enum properties {
	PROP_TIMER = 1,
	PROP_TIMER_POWERUP,
	PROP_SNOOZE,
};

enum saved_props {
	S_SNOOZE,
	S_COUNT
};

static const gchar * const saved_properties[S_COUNT] =
{
	"snooze",
};

typedef struct _AlarmdQueuePrivate AlarmdQueuePrivate;
struct _AlarmdQueuePrivate {
	AlarmdEvent *queued;
	AlarmdEvent *queued_powerup;
	TimerPlugin *timer;
	TimerPlugin *timer_powerup;
	guint32 snooze;
	GSList *events;
	GSList *pending;
};

#define ALARMD_QUEUE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
			      ALARMD_TYPE_QUEUE, AlarmdQueuePrivate));


GType alarmd_queue_get_type(void)
{
	static GType queue_type = 0;

	if (!queue_type)
	{
		static const GTypeInfo queue_info =
		{
			sizeof (AlarmdQueueClass),
			NULL,
			NULL,
			(GClassInitFunc) alarmd_queue_class_init,
			NULL,
			NULL,
			sizeof (AlarmdQueue),
			0,
			(GInstanceInitFunc) alarmd_queue_init,
			NULL
		};

		queue_type = g_type_register_static(ALARMD_TYPE_OBJECT,
				"AlarmdQueue",
				&queue_info, 0);
	}

	return queue_type;
}

AlarmdQueue *alarmd_queue_new(void)
{
	AlarmdQueue *retval;
	ENTER_FUNC;
	retval = g_object_new(ALARMD_TYPE_QUEUE, NULL);
	LEAVE_FUNC;
	return retval;
}

gulong alarmd_queue_add_event(AlarmdQueue *queue, AlarmdEvent *event)
{
	gulong retval;
	ENTER_FUNC;
	retval = ALARMD_QUEUE_GET_CLASS(queue)->add_event(queue, event);
	LEAVE_FUNC;
	return retval;
}

gboolean alarmd_queue_remove_event(AlarmdQueue *queue, gulong event_id)
{
	gboolean retval;
	ENTER_FUNC;
	retval = ALARMD_QUEUE_GET_CLASS(queue)->remove_event(queue, event_id);
	LEAVE_FUNC;
	return retval;
}

AlarmdEvent *alarmd_queue_get_event(AlarmdQueue *queue, gulong event_id)
{
	AlarmdEvent *retval;
	ENTER_FUNC;
	retval = ALARMD_QUEUE_GET_CLASS(queue)->get_event(queue, event_id);
	LEAVE_FUNC;
	return retval;
}

glong *alarmd_queue_query_events(AlarmdQueue *queue, gint64 start_time, gint64 end_time, gint32 flag_mask, gint32 flags, guint *n_events)
{
	glong *retval;
	ENTER_FUNC;
	retval = ALARMD_QUEUE_GET_CLASS(queue)->query_events(queue, start_time, end_time, flag_mask, flags, n_events);
	LEAVE_FUNC;
	return retval;
}

void alarmd_queue_class_init(AlarmdQueueClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	AlarmdObjectClass *alarmd_object_class = ALARMD_OBJECT_CLASS(klass);
	ENTER_FUNC;

	g_type_class_add_private(klass, sizeof(AlarmdQueuePrivate));

	gobject_class->set_property = _alarmd_queue_set_property;
	gobject_class->get_property = _alarmd_queue_get_property;
	gobject_class->finalize = _alarmd_queue_finalize;

	alarmd_object_class->to_xml = _alarmd_queue_to_xml;
	alarmd_object_class->get_saved_properties = _alarmd_queue_get_saved_properties;
	alarmd_object_class->time_changed = _alarmd_queue_real_time_changed;

	klass->add_event = _alarmd_queue_real_add_event;
	klass->remove_event = _alarmd_queue_real_remove_event;
	klass->get_event = _alarmd_queue_real_get_event;
	klass->query_events = _alarmd_queue_real_query_events;

	g_object_class_install_property(gobject_class,
			PROP_TIMER,
			g_param_spec_pointer("timer",
				"Timer plugin used by the queue.",
				"Timer plugin that will provide timed events.",
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_TIMER_POWERUP,
			g_param_spec_pointer("timer_powerup",
				"Timer plugin used by the queue that can power up.",
				"Timer plugin that will provide timed events with power up capabilities.",
				G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(gobject_class,
			PROP_SNOOZE,
			g_param_spec_uint("snooze",
				"Default snooze time in minutes.",
				"The time events will be snoozed by default.",
				0,
				G_MAXUINT,
				10,
				G_PARAM_READABLE | G_PARAM_WRITABLE));

	LEAVE_FUNC;
}

static void alarmd_queue_init(AlarmdQueue *queue)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	ENTER_FUNC;
	priv->timer = NULL;
	priv->timer_powerup = NULL;
	priv->events = NULL;
	priv->pending = NULL;
	priv->queued = NULL;
	priv->queued_powerup = NULL;
	priv->snooze = 10;
	LEAVE_FUNC;
}

static void _alarmd_queue_set_property(GObject *object,
		guint param_id,
		const GValue *value,
		GParamSpec *pspec)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_TIMER:
		priv->timer = (TimerPlugin *)g_value_get_pointer(value);

		if (priv->queued) {
			alarmd_event_dequeue(priv->queued);
		} else {
			_alarmd_queue_event_dequeued(ALARMD_QUEUE(object), NULL);
		}
		break;
	case PROP_TIMER_POWERUP:
		if (priv->timer_powerup ==
			       	(TimerPlugin *)g_value_get_pointer(value)) {
			break;
		}
		priv->timer_powerup = (TimerPlugin *)g_value_get_pointer(value);

		if (priv->queued && alarmd_event_need_power_up(priv->queued)) {
			alarmd_event_dequeue(priv->queued);
		} else if (priv->queued_powerup) {
			alarmd_event_dequeue(priv->queued_powerup);
		} else {
			_alarmd_queue_event_dequeued(ALARMD_QUEUE(object), NULL);
		}
		break;
	case PROP_SNOOZE:
		priv->snooze = g_value_get_uint(value);
		alarmd_object_changed(ALARMD_OBJECT(object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}
	LEAVE_FUNC;
}

static void _alarmd_queue_get_property(GObject *object,
		guint param_id,
		GValue *value,
		GParamSpec *pspec)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(object);
	ENTER_FUNC;

	switch (param_id) {
	case PROP_SNOOZE:
		g_value_set_uint(value, priv->snooze);
		break;
	case PROP_TIMER_POWERUP:
		g_value_set_pointer(value, (gpointer)priv->timer);
		break;
	case PROP_TIMER:
		g_value_set_pointer(value, (gpointer)priv->timer);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}
	LEAVE_FUNC;
}

static void _alarmd_queue_real_time_changed(AlarmdObject *object)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(object);
	GSList *list = g_slist_concat(g_slist_copy(priv->events),
			g_slist_copy(priv->pending));
	ENTER_FUNC;
	g_slist_foreach(list, (GFunc)alarmd_object_time_changed, NULL);
	g_slist_free(list);
	if (priv->timer) {
		priv->timer->time_changed(priv->timer);
	}
	if (priv->timer_powerup) {
		priv->timer_powerup->time_changed(priv->timer_powerup);
	}
	LEAVE_FUNC;
}

static void _alarmd_queue_remove_event(gpointer object, gpointer queue)
{
	ENTER_FUNC;
	g_signal_handlers_disconnect_by_func(object, _alarmd_queue_event_queued, queue);
	g_signal_handlers_disconnect_by_func(object, _alarmd_queue_event_dequeued, queue);
	g_signal_handlers_disconnect_by_func(object, _alarmd_queue_event_changed, queue);
	g_signal_handlers_disconnect_by_func(object, _alarmd_queue_event_cancelled, queue);
	g_object_unref(object);
	LEAVE_FUNC;
}

static void _alarmd_queue_finalize(GObject *object)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(object);
	ENTER_FUNC;

	g_slist_foreach(priv->events, _alarmd_queue_remove_event, NULL);
	g_slist_foreach(priv->pending, _alarmd_queue_remove_event, NULL);
	g_slist_free(priv->events);
	g_slist_free(priv->pending);
	priv->events = NULL;
	priv->pending = NULL;
	G_OBJECT_CLASS(g_type_class_peek(g_type_parent(ALARMD_TYPE_QUEUE)))->finalize(object);
	LEAVE_FUNC;
}

static glong _alarmd_queue_real_add_event(AlarmdQueue *queue, AlarmdEvent *event)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	ENTER_FUNC;
	glong event_id;

	DEBUG("Reffing %p", event);
	g_object_ref(event);
	g_object_get(event, "cookie", &event_id, NULL);

	if (event_id == 0) {
		gint64 timed;
		g_object_get(event, "time", &timed, NULL);
		event_id = (glong)timed;
		if (event_id == 0) {
			event_id++;
		}
		while (_alarmd_queue_find_event(priv->events, event_id) ||
				_alarmd_queue_find_event(priv->pending, event_id)) {
			event_id += 1;
		}
		g_object_set(event, "cookie", event_id, NULL);
	}

	priv->events = g_slist_insert_sorted(priv->events, event, _alarmd_event_compare_func);
	g_signal_connect_swapped(event, "queue", (GCallback)_alarmd_queue_event_queued, (gpointer)queue);
	g_signal_connect_swapped(event, "dequeue", (GCallback)_alarmd_queue_event_dequeued, (gpointer)queue);
	g_signal_connect_swapped(event, "changed", (GCallback)_alarmd_queue_event_changed, (gpointer)queue);
	g_signal_connect_swapped(event, "cancel", (GCallback)_alarmd_queue_event_cancelled, (gpointer)queue);
	g_signal_connect_swapped(event, "fire", (GCallback)_alarmd_queue_event_fired, (gpointer)queue);

	if (!priv->timer_powerup) {
		if (priv->queued == NULL) {
			_alarmd_queue_event_dequeued(queue, NULL);
		} else if (ALARMD_EVENT(priv->events->data) != priv->queued) {
			alarmd_event_dequeue(priv->queued);
		}
	} else if (alarmd_event_need_power_up(event)) {
		if (priv->queued_powerup == NULL) {
			_alarmd_queue_event_dequeued(queue, NULL);
		} else {
			AlarmdEvent *first_powerup = _alarmd_queue_find_first_event(priv->events, TRUE);
			DEBUG("Finding event which needs power up.");
			if (first_powerup != priv->queued_powerup) {
				alarmd_event_dequeue(priv->queued_powerup);
			}
		}
	} else {
		if (priv->queued == NULL) {
			_alarmd_queue_event_dequeued(queue, NULL);
		} else {
			DEBUG("Finding event which does not need power up.");
			AlarmdEvent *first_nopowerup = _alarmd_queue_find_first_event(priv->events, FALSE);
			if (first_nopowerup != priv->queued) {
				alarmd_event_dequeue(priv->queued);
			}
		}
	}

	g_object_set(G_OBJECT(event), "queue", queue, NULL);

	LEAVE_FUNC;
	return event_id;
}

static gboolean _alarmd_queue_real_remove_event(AlarmdQueue *queue, glong event_id)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	AlarmdEvent *event = NULL;
	ENTER_FUNC;

	event = _alarmd_queue_find_event(priv->events, event_id);
	if (event == NULL) {
		event = _alarmd_queue_find_event(priv->pending, event_id);
	}
	
	if (event != NULL) {
		alarmd_event_cancel(event);
		LEAVE_FUNC;
		return TRUE;
	}

	LEAVE_FUNC;
	return FALSE;
}

static AlarmdEvent *_alarmd_queue_real_get_event(AlarmdQueue *queue, glong event_id)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	AlarmdEvent *event = NULL;
	ENTER_FUNC;

	event = _alarmd_queue_find_event(priv->events, event_id);
	if (event == NULL) {
		event = _alarmd_queue_find_event(priv->pending, event_id);
	}

	LEAVE_FUNC;
	return event;
}

static gint _alarmd_event_compare_func(gconstpointer lval, gconstpointer rval)
{
	AlarmdEvent *lev = (AlarmdEvent *)lval;
	AlarmdEvent *rev = (AlarmdEvent *)rval;
	gint retval;
	ENTER_FUNC;

	retval = alarmd_event_get_time(lev) - alarmd_event_get_time(rev);

	LEAVE_FUNC;
	return retval;
}

static void _alarmd_queue_event_to_xml(gpointer child, gpointer node)
{
	AlarmdObject *object = NULL;
	xmlNode *x_node = NULL;
	xmlNode *new_child = NULL;
	ENTER_FUNC;

	object = ALARMD_OBJECT(child);
	x_node = (xmlNode *)node;
	new_child = alarmd_object_to_xml(object);

	if (new_child)
	{
		xmlAddChild(x_node, new_child);
	}
	LEAVE_FUNC;
}

static xmlNode *_alarmd_queue_to_xml(AlarmdObject *object)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(object);
	GParameter *properties = NULL;
	guint n_properties = 0;
	guint i;

	xmlNode *node = xmlNewNode(NULL, "queue");
	ENTER_FUNC;

	properties = alarmd_object_get_properties(object, &n_properties);

	for (i = 0; i < n_properties; i++) {
		gchar *temp = NULL;
		xmlNode *children = NULL;
		xmlNode *added_node = NULL;
		guint j;

		switch (G_VALUE_TYPE(&properties[i].value)) {
		case G_TYPE_BOOLEAN:
			temp = g_strdup_printf("%d", g_value_get_boolean(&properties[i].value));
			break;
		case G_TYPE_CHAR:
			temp = g_strdup_printf("%c", g_value_get_char(&properties[i].value));
			break;
		case G_TYPE_DOUBLE:
			temp = g_strdup_printf("%f", g_value_get_double(&properties[i].value));
			break;
		case G_TYPE_FLOAT:
			temp = g_strdup_printf("%f", g_value_get_float(&properties[i].value));
			break;
		case G_TYPE_INT:
			temp = g_strdup_printf("%d", g_value_get_int(&properties[i].value));
			break;
		case G_TYPE_INT64:
			temp = g_strdup_printf("%lld", g_value_get_int64(&properties[i].value));
			break;
		case G_TYPE_LONG:
			temp = g_strdup_printf("%ld", g_value_get_long(&properties[i].value));
			break;
		case G_TYPE_OBJECT:
			temp = NULL;
			if (g_value_get_object(&properties[i].value)) {
				children = alarmd_object_to_xml(ALARMD_OBJECT(g_value_get_object(&properties[i].value)));
			}
			break;
		case G_TYPE_STRING:
			temp = g_strdup(g_value_get_string(&properties[i].value));
			break;
		case G_TYPE_UCHAR:
			temp = g_strdup_printf("%c", g_value_get_uchar(&properties[i].value));
			break;
		case G_TYPE_UINT:
			temp = g_strdup_printf("%u", g_value_get_uint(&properties[i].value));
			break;
		case G_TYPE_UINT64:
			temp = g_strdup_printf("%llu", g_value_get_uint64(&properties[i].value));
			break;
		case G_TYPE_ULONG:
			temp = g_strdup_printf("%lu", g_value_get_long(&properties[i].value));
			break;
		default:
			temp = NULL;
			g_warning("Unsupported type: %s", g_type_name(G_VALUE_TYPE(&properties[i].value)));
			break;
		}
		added_node = xmlNewChild(node, NULL, "parameter", temp);
		if (temp) {
			g_free(temp);
			temp = NULL;
		}
		if (children) {
			xmlAddChild(added_node, children);
		}
		for (j = 0; j < Y_COUNT; j++) {
			if (G_VALUE_TYPE(&properties[i].value) == type_gtypes[j])
				break;
		}
		xmlNewProp(added_node, "name", properties[i].name);
		xmlNewProp(added_node, "type", type_names[j]);
	}

	alarmd_gparameterv_free(properties, n_properties);

	g_slist_foreach(priv->pending, _alarmd_queue_event_to_xml, (gpointer)node);
	g_slist_foreach(priv->events, _alarmd_queue_event_to_xml, (gpointer)node);

	LEAVE_FUNC;
	return node;
}

static void _alarmd_queue_event_dequeued(AlarmdQueue *queue, AlarmdEvent *event)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	ENTER_FUNC;

	if (priv->queued == event) {
		priv->queued = NULL;
	} else if (priv->queued_powerup == event) {
		priv->queued_powerup = NULL;
	}

	if (priv->events && 
			((!priv->queued &&
			  priv->timer &&
			  !priv->timer_powerup) ||
		     	(!priv->queued_powerup &&
			 priv->timer_powerup &&
			 !priv->timer))) {
		gpointer event = priv->events->data;
		DEBUG("event: %p", event);
		g_object_ref(event);

		g_signal_handlers_block_by_func(event,
			       	_alarmd_queue_event_dequeued,
			       	queue);
		alarmd_event_queue((AlarmdEvent *)priv->events->data, priv->timer ? priv->timer : priv->timer_powerup);
		g_signal_handlers_unblock_by_func(event,
			       	_alarmd_queue_event_dequeued,
			       	queue);

		g_object_unref(event);
	} else if (priv->events) {
		if (priv->queued == NULL && priv->timer) {
			AlarmdEvent *first_nopowerup = _alarmd_queue_find_first_event(priv->events, FALSE);
			if (first_nopowerup) {
				g_signal_handlers_block_by_func(first_nopowerup, _alarmd_queue_event_dequeued, queue);
				alarmd_event_queue(first_nopowerup, priv->timer);
				g_signal_handlers_unblock_by_func(first_nopowerup, _alarmd_queue_event_dequeued, queue);
			}
		}
		if (priv->queued_powerup == NULL && priv->timer_powerup) {
			AlarmdEvent *first_powerup = _alarmd_queue_find_first_event(priv->events, TRUE);
			if (first_powerup) {
				g_signal_handlers_block_by_func(first_powerup, _alarmd_queue_event_dequeued, queue);
				alarmd_event_queue(first_powerup, priv->timer_powerup);
				g_signal_handlers_unblock_by_func(first_powerup, _alarmd_queue_event_dequeued, queue);
			}
		}
	}

	LEAVE_FUNC;
}

static void _alarmd_queue_event_queued(AlarmdQueue *queue, gpointer plugin, AlarmdEvent *event)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);

	(void)plugin;
	ENTER_FUNC;

	if (plugin == priv->timer_powerup) {
		priv->queued_powerup = event;
	} else {
		priv->queued = event;
	}
	LEAVE_FUNC;
}

static void _alarmd_queue_event_changed(AlarmdQueue *queue, AlarmdEvent *event)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	ENTER_FUNC;

	priv->events = g_slist_remove(priv->events, event);
	priv->pending = g_slist_remove(priv->pending, event);

	alarmd_event_dequeue(event);

	priv->events = g_slist_insert_sorted(priv->events, event, _alarmd_event_compare_func);

	if (priv->queued == NULL) {
		_alarmd_queue_event_dequeued(queue, NULL);
	} else if (priv->queued != ALARMD_EVENT(priv->events->data)) {
		alarmd_event_dequeue(priv->queued);
	}

	alarmd_object_changed(ALARMD_OBJECT(queue));

	LEAVE_FUNC;
}

static void _alarmd_queue_event_fired(AlarmdQueue *queue, gboolean delayed,
		AlarmdEvent *event)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	ENTER_FUNC;

	(void)delayed;

	priv->events = g_slist_remove(priv->events, event);
	priv->pending = g_slist_append(priv->pending, event);

	_alarmd_queue_event_dequeued(queue, event);
	LEAVE_FUNC;
}

static void _alarmd_queue_event_cancelled(AlarmdQueue *queue, AlarmdEvent *event)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	ENTER_FUNC;

	g_signal_handlers_disconnect_by_func(event, _alarmd_queue_event_queued, queue);
	g_signal_handlers_disconnect_by_func(event, _alarmd_queue_event_dequeued, queue);
	g_signal_handlers_disconnect_by_func(event, _alarmd_queue_event_changed, queue);
	g_signal_handlers_disconnect_by_func(event, _alarmd_queue_event_cancelled, queue);
	g_signal_handlers_disconnect_by_func(event, _alarmd_queue_event_fired, queue);
	priv->events = g_slist_remove(priv->events, event);
	priv->pending = g_slist_remove(priv->pending, event);

	DEBUG("Unreffing %p", event);
	g_object_unref(event);
	alarmd_object_changed(ALARMD_OBJECT(queue));
	_alarmd_queue_event_dequeued(queue, event);
	LEAVE_FUNC;
}

static AlarmdEvent *_alarmd_queue_find_event(GSList *list, glong event_id)
{
	GSList *iter;
	ENTER_FUNC;

	for (iter = list; iter != NULL; iter = iter->next) {
		AlarmdEvent *event = ALARMD_EVENT(iter->data);
		glong events_id;
		g_object_get(event, "cookie", &events_id, NULL);
		if (events_id == event_id) {
			LEAVE_FUNC;
			return iter->data;
		}
	}
	
	LEAVE_FUNC;
	return NULL;
}

static AlarmdEvent *_alarmd_queue_find_first_event(GSList *list, gboolean need_power_up)
{
	GSList *iter;
	ENTER_FUNC;

	for (iter = list; iter != NULL; iter = iter->next) {
		AlarmdEvent *event = ALARMD_EVENT(iter->data);
		if (alarmd_event_need_power_up(event) == need_power_up) {
			LEAVE_FUNC;
			return iter->data;
		}
	}
	
	LEAVE_FUNC;
	return NULL;
}

static glong *_alarmd_queue_real_query_events(AlarmdQueue *queue, gint64 start_time, gint64 end_time, gint32 flag_mask, gint32 flags, guint *n_events)
{
	AlarmdQueuePrivate *priv = ALARMD_QUEUE_GET_PRIVATE(queue);
	GSList *events = g_slist_concat(g_slist_copy(priv->events), g_slist_copy(priv->pending));
	GSList *iter;
	GSList *result = NULL;
	glong *retval;
	guint i;
	ENTER_FUNC;
	if (n_events == NULL) {
		LEAVE_FUNC;
		return NULL;
	}
	*n_events = 0;
	for (iter = events; iter != NULL; iter = iter->next) {
		gint64 event_time = alarmd_event_get_time(ALARMD_EVENT(iter->data));
		glong event_id;
		if (event_time < start_time) {
			continue;
		} else if (event_time > end_time) {
			break;
		}
		if ((alarmd_event_get_flags(ALARMD_EVENT(iter->data)) & flag_mask) ==
				(flags & flag_mask)) {
			g_object_get(iter->data, "cookie", &event_id, NULL);
			result = g_slist_append(result, GINT_TO_POINTER(event_id));
			(*n_events)++;
			DEBUG("Count now %u", *n_events);
		}
	}
	g_slist_free(events);
	DEBUG("Allocated %u event id's", *n_events);
	retval = g_new0(glong, *n_events);
	for (i = 0; i < *n_events; i++) {
		retval[i] = GPOINTER_TO_INT(result->data);
		result = g_slist_delete_link(result, result);
	}
	g_slist_free(result);
	LEAVE_FUNC;
	return retval;
}

static GSList *_alarmd_queue_get_saved_properties()
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
