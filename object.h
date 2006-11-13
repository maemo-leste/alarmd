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

#ifndef _OBJECT_H_
#define _OBJECT_H_

#include <libxml/tree.h>
#include <glib/gtypes.h>
#include <glib-object.h>
#include <dbus/dbus.h>

#define ALARMD_TYPE_OBJECT (alarmd_object_get_type())
#define ALARMD_OBJECT(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ALARMD_TYPE_OBJECT, AlarmdObject))
#define ALARMD_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ALARMD_TYPE_OBJECT, AlarmdObjectClass))
#define ALARMD_IS_OBJECT(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), ALARMD_TYPE_OBJECT))
#define ALARMD_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ALARMD_TYPE_OBJECT))
#define ALARMD_OBJECT_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ALARMD_TYPE_OBJECT, AlarmdObjectClass))

typedef struct _AlarmdObject AlarmdObject;
struct _AlarmdObject
{
	GObject parent_instance;
};

typedef struct _AlarmdObjectClass AlarmdObjectClass;
struct _AlarmdObjectClass
{
	GObjectClass parent_class;

	void (*changed)(AlarmdObject *object);
	GParameter *(*get_properties)(AlarmdObject *object, guint *n_objects);
	GSList *(*get_saved_properties)();
	xmlNode *(*to_xml)(AlarmdObject *object);
	void (*to_dbus)(AlarmdObject *object, DBusMessageIter *iter);
	void (*time_changed)(AlarmdObject *object);
};

/**
 * SECTION:object
 * @short_description: Base class for all alarmd objects.
 * @see_also: AlarmdEvent, AlarmdAction, AlarmdQueue
 *
 * Base class for all objects in alarmd. Idea is to centralize the
 * serialization and to ease spreading the time-changed signal.
 **/

/**
 * AlarmdObject::changed:
 * @object: The object that has changed.
 *
 * Emitted whenever an object has changed so that it should be saved.
 **/

/**
 * AlarmdObject::time-changed:
 * @object: The #AlarmdObject which received the signal.
 *
 * Emitted whenever the system time has changed.
 **/

GType alarmd_object_get_type(void);

/**
 * alarmd_object_get_properties:
 * @object: The object whose properties should be gotten.
 * @n_objects: Pointer to integer that should hold the size of the returned
 * array.
 *
 * Gets all properties of an object that should be saved; mostly a helper
 * function for #alarmd_object_to_xml and #alarmd_object_to_dbus to avoid
 * code duplication.
 * Returns: An array of #GParameter.
 **/
GParameter *alarmd_object_get_properties(AlarmdObject *object, guint *n_objects);
/**
 * alarmd_object_get_saved_properties:
 * @object: The #AlarmdObject whose saved properties should be get.
 *
 * An helper function for #alarmd_object_get_properties to reduce code
 * duplication. Each subclass overriding this should call their parent to
 * get their saved properties too.
 * Returns: A #GSList of property names (as const char *). The names should
 * be owned by the appropriate class and will not be free'd.
 **/
GSList *alarmd_object_get_saved_properties(AlarmdObject *object);

/**
 * alarmd_object_to_xml:
 * @object: The #AlarmdObject that should be serialized into xml.
 *
 * Creates a xml representation of the @object.
 * Returns: A tree of #xmlNode describing the @object.
 **/
xmlNode *alarmd_object_to_xml(AlarmdObject *object);

/**
 * alarmd_object_to_dbus:
 * @object: The #AlarmdObject that should be serialized into dbus message.
 * @iter: The iter that should hould the representation of the object.
 *
 * Creates a dbus message representation of the @object.
 **/
void alarmd_object_to_dbus(AlarmdObject *object, DBusMessageIter *iter);

/**
 * alarmd_object_changed:
 * @object: The object that has changed.
 *
 * Emits #AlarmdObject::changed signal on the @object.
 **/
void alarmd_object_changed(AlarmdObject *object);


/**
 * alarmd_object_time_changed:
 * @object: The object that should receive the signal.
 *
 * Emits #AlarmdObject::time-changed signal on the @object.
 **/
void alarmd_object_time_changed(AlarmdObject *object);

/**
 * alarmd_gparameterv_free:
 * @paramv: The #GParameter array.
 * @n_objects: The size of the @paramv.
 *
 * Frees the @paramv array and all data associated with it.
 **/
void alarmd_gparameterv_free(GParameter *paramv, guint n_objects);

#endif /* _OBJECT_H */
