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

/*
 * @file alarmd.h
 * Alarm/event daemon
 * <p>
 * Copyright (C) 2006 Nokia.  All rights reserved.
 * <p>
 * @author David Weinehall <david.weinehall@nokia.com>
 */
#ifndef _ALARMD_H_
#define _ALARMD_H_

#include <glib.h>
#include <locale.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(__str)               gettext(__str)
#else
#undef bindtextdomain
#define bindtextdomain(__domain, __directory)
#undef textdomain
#define textdomain(__domain)
#define _(__str)               __str
#endif /* ENABLE_NLS */

#define __stringify_1(x)       #x
#define __stringify(x)         __stringify_1(x)

GMainLoop *mainloop;

#endif /* _ALARMD_H_ */
