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
#include <glib/gtypes.h>
#include <glib/gthread.h>
#include <glib/gmain.h>
#include <string.h>
#include "rpc-dbus.h"
#include "rpc-statusbar.h"
#include "debug.h"

void statusbar_show_icon(void)
{
       ENTER_FUNC;

       /* NOOP in IT2006. */

       LEAVE_FUNC;
}

void statusbar_hide_icon(void)
{
       ENTER_FUNC;

       /* NOOP in IT2006. */

       LEAVE_FUNC;
}
