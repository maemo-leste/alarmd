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
#include <libxml/tree.h>
#include <string.h>
#include "xmlobjectfactory.h"
#include "xml-common.h"
#include "object.h"
#include "debug.h"

static void iter_get_value(xmlNode *iter, guint type_id, GValue *value);
static GValueArray *iter_get_valarray(xmlNode *node);

enum Type {
	T_ELEMENT,
	T_ATTRIBUTE,
	T_TYPE,
	T_COUNT
};

enum Element {
	E_OBJECT,
	E_PARAMETER,
	E_ITEM,
	E_COUNT
};

enum Attributes {
	A_TYPE,
	A_NAME,
	A_COUNT
};

static const char * const element_names[E_COUNT] = {
	"object",
	"parameter",
	"item"
};

static const char * const attribute_names[A_COUNT] = {
	"type",
	"name"
};

static const char * const * const names[T_COUNT] = {
	element_names,
	attribute_names,
	type_names
};

static const unsigned int counts[T_COUNT] = {
	E_COUNT,
	A_COUNT,
	Y_COUNT
};

static int get_id(enum Type type, const unsigned char * const name) {
	guint i = 0;
	ENTER_FUNC;

	if (name == NULL) {
		LEAVE_FUNC;
		return counts[type];
	}

	for (i = 0; i < counts[type]; i++) {
		if (strcmp((gchar *)name, (gchar *)names[type][i]) == 0) {
			break;
		}
	}

	LEAVE_FUNC;
	return i;
}

static gchar *get_element_text(xmlNode *node) {
	ENTER_FUNC;
	for (; node; node = node->next) {
		if (node->type == XML_TEXT_NODE) {
			LEAVE_FUNC;
			return (gchar *)node->content;
		} else {
			g_warning("Element should only have text children %d.", node->type);
		}
	}
	LEAVE_FUNC;
	return NULL;
}

static GValueArray *iter_get_valarray(xmlNode *node)
{
	GValueArray *array = g_value_array_new(0);
	GValue value;
	xmlNode *cur_node;
	xmlAttr *iter;
	gchar *temp_type = NULL;
	guint type_id = 0;

	memset(&value, 0, sizeof(value));

	for (cur_node = node->children; cur_node != NULL; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			switch (get_id(T_ELEMENT, cur_node->name)) {
			case E_ITEM:
				for (iter = cur_node->properties; iter != NULL; iter = iter->next) {
					switch (get_id(T_ATTRIBUTE, iter->name)) {
					case A_TYPE:
						temp_type = get_element_text(iter->children);
						break;
					default:
						g_warning("Invalid attribute.");
						break;
					}
				}
				type_id = get_id(T_TYPE, (unsigned char *) temp_type);
				if (type_id == Y_COUNT) {
					break;
				}

				iter_get_value(cur_node, type_id, &value);
				g_value_array_append(array, &value);
				g_value_unset(&value);
				break;
			}
		}
	}

	return array;
}

static void iter_get_value(xmlNode *iter, guint type_id, GValue *value)
{
	AlarmdObject *temp_object;
	gchar *temp_name;

	if (type_gtypes[type_id] != G_TYPE_BOXED) {
		g_value_init(value, type_gtypes[type_id]);
	} else if (type_id == Y_VALARRAY) {
		g_value_init(value, G_TYPE_VALUE_ARRAY);
	}

	switch (type_id) {
	case Y_BOOLEAN:
		temp_name = get_element_text(iter->children);
		g_value_set_boolean(value, atoi(temp_name));
		break;
	case Y_CHAR:
		temp_name = get_element_text(iter->children);
		g_value_set_char(value, temp_name ? *temp_name : '\0');
		break;
	case Y_INT:
		temp_name = get_element_text(iter->children);
		g_value_set_int(value, atoi(temp_name));
		break;
	case Y_INT64:
		temp_name = get_element_text(iter->children);
		g_value_set_int64(value, atoll(temp_name));
		break;
	case Y_LONG:
		temp_name = get_element_text(iter->children);
		g_value_set_long(value, atol(temp_name));
		break;
	case Y_OBJECT:
		temp_object = object_factory(iter->children);
		g_value_set_object(value, temp_object);
		g_object_unref(temp_object);
		break;
	case Y_STRING:
		g_value_set_string(value, get_element_text(iter->children));
		break;
	case Y_UCHAR:
		temp_name = get_element_text(iter->children);
		g_value_set_uchar(value, (guchar)(temp_name ? *temp_name : '\0'));
		break;
	case Y_UINT:
		temp_name = get_element_text(iter->children);
		g_value_set_uint(value, strtoul(temp_name, NULL, 10));
		break;
	case Y_UINT64:
		temp_name = get_element_text(iter->children);
		g_value_set_uint64(value, strtoull(temp_name, NULL, 10));
		break;
	case Y_ULONG:
		temp_name = get_element_text(iter->children);
		g_value_set_ulong(value, strtoul(temp_name, NULL, 10));
		break;
	case Y_VALARRAY:
		g_value_take_boxed(value, iter_get_valarray(iter));
		break;
	}
}

GParameter *elements_to_parameters(xmlNode *object_node, guint *n_params)
{
	xmlNode *cur_node;
	GSList *properties = NULL;
	GParameter *param;
	guint i;

	for (cur_node = object_node->children; cur_node != NULL; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			guint type_id;
			xmlAttr *iter;
			gchar *temp_type = NULL;
			gchar *temp_name = NULL;
			AlarmdObject *temp_object;

			switch (get_id(T_ELEMENT, cur_node->name)) {
			case E_PARAMETER:
				for (iter = cur_node->properties; iter != NULL; iter = iter->next) {
					switch (get_id(T_ATTRIBUTE, iter->name)) {
					case A_TYPE:
						temp_type = get_element_text(iter->children);
						break;
					case A_NAME:
						temp_name = get_element_text(iter->children);
						break;
					default:
						g_warning("Invalid attribute.");
						break;
					}
				}
				if (!temp_type || !temp_name) {
					temp_type = NULL;
					temp_name = NULL;
					break;
				}
				type_id = get_id(T_TYPE, (unsigned char *) temp_type);
				if (type_id == T_COUNT) {
					break;
				}
				param = g_new0(GParameter, 1);
				iter_get_value(cur_node, type_id, &param->value);
				param->name = temp_name;
				temp_name = NULL;
				temp_type = NULL;
				temp_object = NULL;
				properties = g_slist_append(properties, param);
				param = NULL;
				break;
			case E_COUNT:
			default:
				break;
			}
		}
	}
	*n_params = g_slist_length(properties);
	param = g_new0(GParameter, *n_params);
	for (i = 0; i < *n_params; i++) {
		GParameter *temp = (GParameter *)properties->data;
		param[i].name = temp->name;
		g_value_init(&param[i].value, G_VALUE_TYPE(&temp->value));
		g_value_copy(&temp->value, &param[i].value);
		g_value_unset(&temp->value);
		g_free(temp);
		properties = g_slist_delete_link(properties, properties);
	}

	return param;
}

AlarmdObject *object_factory(xmlNode *object_node)
{
	xmlAttr *iter = NULL;
	gchar *temp_type = NULL;
	GType object_type = 0;
	GParameter *param = NULL;
	AlarmdObject *temp_object = NULL;
	guint n_params;

	ENTER_FUNC;

	for (; object_node != NULL && object_node->type != XML_ELEMENT_NODE; object_node = object_node->next);

	if (object_node == NULL) {
		DEBUG("No element node.");
		LEAVE_FUNC;
		return NULL;
	}

	if (get_id(T_ELEMENT, object_node->name) != E_OBJECT) {
		DEBUG("Not object element.");
		LEAVE_FUNC;
		return NULL;
	}

	for (iter = object_node->properties; iter != NULL; iter = iter->next) {
		switch (get_id(T_ATTRIBUTE, iter->name)) {
		case A_TYPE:
			temp_type = get_element_text(iter->children);
			break;
		default:
			g_warning("Invalid attribute %s for %s", iter->name, object_node->name);
			break;
		}
	}
	if (temp_type == NULL) {
		DEBUG("No type found.");
		g_warning("No type found.");
		LEAVE_FUNC;
		return NULL;
	}

	object_type = g_type_from_name(temp_type);

	param = elements_to_parameters(object_node, &n_params);

	temp_object = g_object_newv(object_type, n_params, param);

	alarmd_gparameterv_free(param, n_params);

	LEAVE_FUNC;
	return temp_object;
}
