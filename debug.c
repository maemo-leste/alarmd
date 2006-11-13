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

#include <glib/gtypes.h>
#include <stdio.h>
#include <stdarg.h>
#include "debug.h"

static guint depth = 0;

void enter_func(const char *name)
{
	guint i;
	for (i = 0; i < depth; i++) {
		fprintf(stderr, " ");
	}
	fprintf(stderr, "Entering %s\n", name);
	depth++;
}

void leave_func(const char *name)
{
	guint i;
	depth--;
	for (i = 0; i < depth; i++) {
		fprintf(stderr, " ");
	}
	fprintf(stderr, "Leaving %s\n", name);
}

void dbg(const char *func, const char *format, ...)
{
	guint i;
	va_list args;
	for (i = 0; i < depth; i++) {
		fprintf(stderr, " ");
	}
	fprintf(stderr, "*** %s: ", func);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}
