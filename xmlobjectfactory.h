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

#ifndef _XMLOBJECTFACTORY_H_
#define _XMLOBJECTFACTORY_H_

#include <libxml/tree.h>
#include "object.h"

/**
 * SECTION:xmlobjectfactory
 * @short_description: Utilities for creating objects out of xml tree.
 *
 * Contains functions that are needed to get an object from a xml tree.
 **/

/**
 * object_factory:
 * @object_node: The node in the tree, which describes the object to be.
 *
 * Creates a new object from the xnk tree. May recursively call self to create
 * sub-objects.
 * Returns: Newly created object.
 **/
AlarmdObject *object_factory(xmlNode *object_node);

/**
 * elements_to_parameters:
 * @object_node: Node whose children should be handled.
 * @n_params: Pointer to guint that should hold the size of returned array.
 *
 * Scans the children of #object_node for occurrencies of parameter nodes.
 * A GParameter array is build of these.
 * Returns: Newly allocated GParameter array, use alarmd_gparameterv_free to
 * free.
 **/
GParameter *elements_to_parameters(xmlNode *object_node, guint *n_params);

#endif /* _XMLOBJECTFACTORY_H_ */
