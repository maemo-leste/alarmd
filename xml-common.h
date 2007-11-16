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

#ifndef _XML_COMMON_H_
#define _XML_COMMON_H_

/**
 * SECTION:xml-common
 * @short_description: Definitions for xml<->glib typing.
 *
 * These arrays help to get a GType for a type string from xml file and vice
 * versa.
 **/

enum Types {
	Y_BOOLEAN,
	Y_CHAR,
	Y_DOUBLE,
	Y_FLOAT,
	Y_INT,
	Y_INT64,
	Y_LONG,
	Y_OBJECT,
	Y_STRING,
	Y_UCHAR,
	Y_UINT,
	Y_UINT64,
	Y_ULONG,
	Y_VALARRAY,
	Y_COUNT
};

static const char * const type_names[Y_COUNT] = {
	"boolean",
	"char",
	"double",
	"float",
	"int",
	"int64",
	"long",
	"object",
	"string",
	"uchar",
	"uint",
	"uint64",
	"ulong",
	"value_array",
};

static const GType type_gtypes[Y_COUNT] = {
	G_TYPE_BOOLEAN,       
	G_TYPE_CHAR,
	G_TYPE_DOUBLE,
	G_TYPE_FLOAT,
	G_TYPE_INT,
	G_TYPE_INT64,
	G_TYPE_LONG,
	G_TYPE_OBJECT,
	G_TYPE_STRING,
	G_TYPE_UCHAR,
	G_TYPE_UINT,
	G_TYPE_UINT64,
	G_TYPE_ULONG,
	G_TYPE_BOXED,
};

#endif /* _XML_COMMON_H_ */
