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

#include <stdarg.h>
#include "object.h"
#include "xml-common.h"
#include "rpc-dbus.h"
#include "debug.h"

static void alarmd_object_class_init(AlarmdObjectClass *klass);
static GParameter *_alarmd_object_real_get_properties(AlarmdObject *object, guint *n_objects);
static void _alarmd_object_real_changed(AlarmdObject *object);
static xmlNode *_alarmd_object_real_to_xml(AlarmdObject *object);
static void _alarmd_object_real_to_dbus(AlarmdObject *object, DBusMessageIter *iter);
static GSList *_alarmd_object_real_get_saved_properties(void);
static void _alarmd_object_real_time_changed(AlarmdObject *object);

struct signal_closure {
	AlarmdObject *object;
	guint signal;
	va_list var_args;
};

enum signals {
	SIGNAL_CHANGED,
	SIGNAL_TIME_CHANGED,
	SIGNAL_COUNT
};

static guint object_signals[SIGNAL_COUNT];

GType alarmd_object_get_type(void)
{
	static GType object_type = 0;

	if (!object_type)
	{
		static const GTypeInfo object_info =
		{
			sizeof (AlarmdObjectClass),
			NULL,
			NULL,
			(GClassInitFunc) alarmd_object_class_init,
			NULL,
			NULL,
			sizeof (AlarmdObject),
			0,
			NULL,
			NULL
		};

		object_type = g_type_register_static(G_TYPE_OBJECT,
				"AlarmdObject",
				&object_info, 0);
	}

	return object_type;
}

GParameter *alarmd_object_get_properties(AlarmdObject *object, guint *n_objects)
{
	GParameter *params = NULL;
	GParamSpec *spec = NULL;
	guint i;
	guint count;
	GSList *list;
	ENTER_FUNC;

	list = alarmd_object_get_saved_properties(object);
	count = g_slist_length(list);
	params = g_new0(GParameter, count);

	for (i = 0; i < count; i++) {
		const gchar *name = (const gchar *)list->data;
		params[i].name = name;
		spec = g_object_class_find_property(G_OBJECT_GET_CLASS(object), name);
		if (g_type_is_a(spec->value_type, G_TYPE_OBJECT)) {
			g_value_init(&params[i].value, G_TYPE_OBJECT);
		} else {
			g_value_init(&params[i].value, spec->value_type);
		}
		g_object_get_property(G_OBJECT(object), name, &params[i].value);
		list = g_slist_delete_link(list, list);
	}

	*n_objects = count;
	LEAVE_FUNC;
	return params;
}

GSList *alarmd_object_get_saved_properties(AlarmdObject *object)
{
	GSList *retval = NULL;
	ENTER_FUNC;
	retval = ALARMD_OBJECT_GET_CLASS(object)->get_saved_properties();
	LEAVE_FUNC;
	return retval;
}

void alarmd_gparameterv_free(GParameter *paramv, guint n_objects)
{
	ENTER_FUNC;
	guint i;
	if (n_objects == 0 || paramv == NULL) {
		LEAVE_FUNC;
		return;
	}

	for (i = 0; i < n_objects; i++) {
		g_value_unset(&paramv[i].value);
	}

	g_free(paramv);
	LEAVE_FUNC;
}

xmlNode *alarmd_object_to_xml(AlarmdObject *object)
{
	xmlNode *retval;
	ENTER_FUNC;
	retval = ALARMD_OBJECT_GET_CLASS(object)->to_xml(object);
	LEAVE_FUNC;
	return retval;
}

void alarmd_object_changed(AlarmdObject *object)
{
	ENTER_FUNC;
	g_signal_emit(object, object_signals[SIGNAL_CHANGED], 0);
	LEAVE_FUNC;
}

void alarmd_object_to_dbus(AlarmdObject *object, DBusMessageIter *iter)
{
	ENTER_FUNC;
	ALARMD_OBJECT_GET_CLASS(object)->to_dbus(object, iter);
	LEAVE_FUNC;
}

void alarmd_object_time_changed(AlarmdObject *queue)
{
	ENTER_FUNC;
	g_signal_emit(ALARMD_OBJECT(queue), object_signals[SIGNAL_TIME_CHANGED], 0);
	LEAVE_FUNC;
}

static void alarmd_object_class_init(AlarmdObjectClass *klass)
{
	ENTER_FUNC;
	klass->changed = _alarmd_object_real_changed;
	klass->get_properties = _alarmd_object_real_get_properties;
	klass->get_saved_properties = _alarmd_object_real_get_saved_properties;
	klass->to_xml = _alarmd_object_real_to_xml;
	klass->to_dbus = _alarmd_object_real_to_dbus;
	klass->time_changed = _alarmd_object_real_time_changed;

	object_signals[SIGNAL_CHANGED] = g_signal_new("changed",
			G_TYPE_FROM_CLASS(klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			G_STRUCT_OFFSET(AlarmdObjectClass, changed),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
	object_signals[SIGNAL_TIME_CHANGED] = g_signal_new("time_changed",
			G_TYPE_FROM_CLASS(klass),
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			G_STRUCT_OFFSET(AlarmdObjectClass, time_changed),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
	LEAVE_FUNC;
}

static GParameter *_alarmd_object_real_get_properties(AlarmdObject *object, guint *n_objects)
{
	ENTER_FUNC;
	(void)object;

	*n_objects = 0;
	
	LEAVE_FUNC;
	return NULL;
}

static void _alarmd_object_real_to_dbus(AlarmdObject *object, DBusMessageIter *iter)
{
	GParameter *properties = NULL;
	guint n_props = 0;
	guint i;
	gchar *temp = NULL;
	ENTER_FUNC;

	temp = g_strdup_printf("/%s", G_OBJECT_TYPE_NAME(object));
	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &temp);
	g_free(temp);
	properties = alarmd_object_get_properties(object, &n_props);
	dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &n_props);
	for (i = 0; i < n_props; i++) {
		dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &properties[i].name);
		dbus_message_iter_append_gvalue(iter, &properties[i].value, FALSE);
	}

	alarmd_gparameterv_free(properties, n_props);

	LEAVE_FUNC;
}

static GSList *_value_array_to_xml_nodes(GValueArray *array)
{
	GSList *nodes = NULL;
	guint i;

	if (!array) {
		return NULL;
	}

	for (i = 0; i < array->n_values; i++)
	{
		gchar *temp = NULL;
		GSList *children = NULL;
		xmlNode *added_node = NULL;
		guint j;

		if (G_VALUE_TYPE(&array->values[i]) == G_TYPE_VALUE_ARRAY) {
			temp = NULL;
			children = _value_array_to_xml_nodes(g_value_get_boxed(&array->values[i]));
		} else switch (G_VALUE_TYPE(&array->values[i])) {
		case G_TYPE_BOOLEAN:
			temp = g_strdup_printf("%d", g_value_get_boolean(&array->values[i]));
			break;
		case G_TYPE_CHAR:
			temp = g_strdup_printf("%c", g_value_get_char(&array->values[i]));
			break;
		case G_TYPE_DOUBLE:
			temp = g_strdup_printf("%f", g_value_get_double(&array->values[i]));
			break;
		case G_TYPE_FLOAT:
			temp = g_strdup_printf("%f", g_value_get_float(&array->values[i]));
			break;
		case G_TYPE_INT:
			temp = g_strdup_printf("%d", g_value_get_int(&array->values[i]));
			break;
		case G_TYPE_INT64:
			temp = g_strdup_printf("%lld", g_value_get_int64(&array->values[i]));
			break;
		case G_TYPE_LONG:
			temp = g_strdup_printf("%ld", g_value_get_long(&array->values[i]));
			break;
		case G_TYPE_OBJECT:
			temp = NULL;
			if (g_value_get_object(&array->values[i])) {
				children = g_slist_append(children, alarmd_object_to_xml(ALARMD_OBJECT(g_value_get_object(&array->values[i]))));
			}
			break;
		case G_TYPE_STRING:
			temp = g_strdup(g_value_get_string(&array->values[i]));
			break;
		case G_TYPE_UCHAR:
			temp = g_strdup_printf("%c", g_value_get_uchar(&array->values[i]));
			break;
		case G_TYPE_UINT:
			temp = g_strdup_printf("%u", g_value_get_uint(&array->values[i]));
			break;
		case G_TYPE_UINT64:
			temp = g_strdup_printf("%llu", g_value_get_uint64(&array->values[i]));
			break;
		case G_TYPE_ULONG:
			temp = g_strdup_printf("%lu", g_value_get_long(&array->values[i]));
			break;
		default:
			temp = NULL;
			g_warning("Unsupported type: %s", g_type_name(G_VALUE_TYPE(&array->values[i])));
			break;
		}
		added_node = xmlNewNode(NULL, (xmlChar *) "item");
		xmlNodeAddContent(added_node, (xmlChar *) temp);
		if (temp) {
			g_free(temp);
			temp = NULL;
		}
		while (children) {
			xmlAddChild(added_node, children->data);
			children = g_slist_delete_link(children, children);
		}
		for (j = 0; j < Y_COUNT; j++) {
			if (type_gtypes[j] != G_TYPE_BOXED) {
				if (G_VALUE_TYPE(&array->values[i]) == type_gtypes[j])
					break;
			} else {
				if (G_VALUE_TYPE(&array->values[i]) == G_TYPE_VALUE_ARRAY &&
						j == Y_VALARRAY)
					break;
			}
		}
		xmlNewProp(added_node, (xmlChar *) "type", (xmlChar *) type_names[j]);
		nodes = g_slist_append(nodes, added_node);
	}
	return nodes;
}

static xmlNode *_alarmd_object_real_to_xml(AlarmdObject *object)
{
	xmlNode *node;
	guint n_properties = 0;
	guint i;
	GParameter *properties = NULL;

	ENTER_FUNC;
	node = xmlNewNode(NULL, (xmlChar *) "object");
	xmlNewProp(node, (xmlChar *) "type", (xmlChar *) g_type_name(G_OBJECT_TYPE(object)));

	properties = alarmd_object_get_properties(object, &n_properties);
	for (i = 0; i < n_properties; i++) {
		gchar *temp = NULL;
		GSList *children = NULL;
		xmlNode *added_node = NULL;
		guint j;

		if (G_VALUE_TYPE(&properties[i].value) == G_TYPE_VALUE_ARRAY) {
			temp = NULL;
			children = _value_array_to_xml_nodes(g_value_get_boxed(&properties[i].value));
		} else switch (G_VALUE_TYPE(&properties[i].value)) {
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
				children = g_slist_append(children, alarmd_object_to_xml(ALARMD_OBJECT(g_value_get_object(&properties[i].value))));
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
		added_node = xmlNewChild(node, NULL, (xmlChar *) "parameter", (xmlChar *) temp);
		if (temp) {
			g_free(temp);
			temp = NULL;
		}
		while (children) {
			xmlAddChild(added_node, children->data);
			children = g_slist_delete_link(children, children);
		}
		for (j = 0; j < Y_COUNT; j++) {
			if (type_gtypes[j] != G_TYPE_BOXED) {
				if (G_VALUE_TYPE(&properties[i].value) == type_gtypes[j])
					break;
			} else {
				if (G_VALUE_TYPE(&properties[i].value) == G_TYPE_VALUE_ARRAY &&
						j == Y_VALARRAY)
					break;
			}
		}
		xmlNewProp(added_node, (xmlChar *) "name", (xmlChar *) properties[i].name);
		xmlNewProp(added_node, (xmlChar *) "type", (xmlChar *) type_names[j]);
	}

	alarmd_gparameterv_free(properties, n_properties);

	LEAVE_FUNC;
	return node;
}

static void _alarmd_object_real_changed(AlarmdObject *object)
{
	ENTER_FUNC;
	(void)object;
	LEAVE_FUNC;
}

static GSList *_alarmd_object_real_get_saved_properties(void)
{
	ENTER_FUNC;
	LEAVE_FUNC;
	return NULL;
}

static void _alarmd_object_real_time_changed(AlarmdObject *object)
{
	ENTER_FUNC;
	(void)object;
	LEAVE_FUNC;
}
