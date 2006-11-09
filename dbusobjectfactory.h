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

#ifndef _DBUS_OBJECT_FACTORY_H_
#define _DBUS_OBJECT_FACTORY_H_
#include <dbus/dbus.h>
#include <glib-object.h>

/**
 * SECTION:dbusobjectfactory
 * @short_description: Utilities for creating objects out of dbus messages.
 *
 * Contains functions that are needed to get an object from a dbus message.
 **/

/**
 * dbus_object_factory:
 * @iter: #DBusMessageIter from which the data about the object to be should
 * be read.
 *
 * Creates a new object from the DBusMessages data fields. The message should
 * be formatted as follows:
 *
 * OBJECT_PATH:ClassName, UINT32:Arguments, STRING:ArgumentName, [TYPE]:Value
 * If TYOE is OBJECT_PATH, the function will recursively call self and the
 * object will be built.
 * Returns: Newly created object.
 **/
GObject *dbus_object_factory(DBusMessageIter *iter);

#endif /* _DBUS_OBJECT_FACTORY_H_ */
