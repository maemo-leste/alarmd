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

#include <dbus/dbus.h>
#include <glib-object.h>
#include <string.h>
#include "dbusobjectfactory.h"
#include "object.h"
#include "debug.h"

static void dbus_message_iter_get_gvalue(DBusMessageIter *iter, GValue *gvalue)
{
	switch (dbus_message_iter_get_arg_type(iter)) {
	case DBUS_TYPE_BYTE:
		{
			gchar value;
			g_value_init(gvalue, G_TYPE_CHAR);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_char(gvalue, value);
			break;
		}
	case DBUS_TYPE_BOOLEAN:
		{
			dbus_bool_t value;
			g_value_init(gvalue, G_TYPE_BOOLEAN);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_boolean(gvalue, value);
			break;
		}
	case DBUS_TYPE_INT16:
		{
			dbus_int16_t value;
			g_value_init(gvalue, G_TYPE_INT);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_int(gvalue, value);
			break;
		}
	case DBUS_TYPE_UINT16:
		{
			dbus_uint16_t value;
			g_value_init(gvalue, G_TYPE_UINT);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_uint(gvalue, value);
			break;
		}
	case DBUS_TYPE_INT32:
		{
			dbus_int32_t value;
			g_value_init(gvalue, G_TYPE_INT);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_int(gvalue, value);
			break;
		}
	case DBUS_TYPE_UINT32:
		{
			dbus_uint32_t value;
			g_value_init(gvalue, G_TYPE_UINT);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_uint(gvalue, value);
			break;
		}
#ifdef DBUS_HAVE_INT64
	case DBUS_TYPE_INT64:
		{
			dbus_int64_t value;
			g_value_init(gvalue, G_TYPE_INT64);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_int64(gvalue, value);
			break;
		}
	case DBUS_TYPE_UINT64:
		{
			dbus_uint64_t value;
			g_value_init(gvalue, G_TYPE_UINT64);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_uint64(gvalue, value);
			break;
		}
#endif
	case DBUS_TYPE_DOUBLE:
		{
			double value;
			g_value_init(gvalue, G_TYPE_DOUBLE);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_double(gvalue, value);
			break;
		}
	case DBUS_TYPE_STRING:
		{
			gchar *value = NULL;
			g_value_init(gvalue, G_TYPE_STRING);
			dbus_message_iter_get_basic(iter, &value);
			g_value_set_string(gvalue, value);
			break;
		}
	case DBUS_TYPE_OBJECT_PATH:
		{
			GObject *value = dbus_object_factory(iter);
			g_value_init(gvalue, G_TYPE_OBJECT);
			g_value_set_object(gvalue, value);
			if (value) {
				g_object_unref(value);
			}
			break;
		}
	case DBUS_TYPE_VARIANT:
		{
			DBusMessageIter sub;
			dbus_message_iter_recurse(iter, &sub);
			dbus_message_iter_get_gvalue(&sub, gvalue);
			break;
		}
	case DBUS_TYPE_ARRAY:
		{
			DBusMessageIter sub;
			GValueArray *array = g_value_array_new(0);
			g_value_init(gvalue, G_TYPE_VALUE_ARRAY);
			dbus_message_iter_recurse(iter, &sub);
			for (dbus_message_iter_recurse(iter, &sub);
					dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID;
					dbus_message_iter_next(&sub)) {
				GValue gval;
				memset(&gval, 0, sizeof(gval));
				dbus_message_iter_get_gvalue(&sub,
						&gval);
				g_value_array_append(array, &gval);
				g_value_unset(&gval);
			}
			g_value_take_boxed(gvalue, array);
		}
		break;
	default:
		DEBUG("Unsupported type.");
		break;
	}
}

GObject *dbus_object_factory(DBusMessageIter *iter)
{
	const gchar *type_name = NULL;
	guint n_params = 0;
	guint i = 0;
	GParameter *params = NULL;
	GObject *retval = NULL;
	GType object_type = 0;

	ENTER_FUNC;

	if (iter == NULL || dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_OBJECT_PATH) {
		LEAVE_FUNC;
		return NULL;
	}

	dbus_message_iter_get_basic(iter, &type_name);

	if (type_name == NULL || *type_name == '\0') {
		LEAVE_FUNC;
		return NULL;
	}
	object_type = g_type_from_name(type_name + 1);

	if (!dbus_message_iter_next(iter) ||
			dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_UINT32) {
		LEAVE_FUNC;
		return NULL;
	}

	dbus_message_iter_get_basic(iter, &n_params);

	params = g_new0(GParameter, n_params);

	for (i = 0; i < n_params; i++) {
		if (!dbus_message_iter_next(iter)) {
			DEBUG("Ran out of parameters1.");
			break;
		}
		if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(iter, &params[i].name);
			DEBUG("Parameter '%s'", params[i].name);
		} else {
			DEBUG("Wrong parameter type.");
			break;
		}
		if (!dbus_message_iter_next(iter)) {
			DEBUG("Ran out of parameters2.");
			break;
		}
		dbus_message_iter_get_gvalue(iter, &params[i].value);
	}

	if (i == n_params) {
		retval = g_object_newv(object_type, n_params, params);
	}
	alarmd_gparameterv_free(params, n_params);
	LEAVE_FUNC;
	return retval;
}
