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

#ifndef _RPC_STATUSBAR_H_
#define _RPC_STATUSBAR_H_

/**
 * SECTION:rpc-statusbar
 * @short_description: Communication with the statusbar applet.
 *
 * These functions can be used to show/hide the statusbar icon for alarm.
 **/

/**
 * statusbar_show_icon:
 *
 * Increments the statusbar applet show count, if the counter was zero,
 * the icon will be shown.
 **/
void statusbar_show_icon(void);

/**
 * statusbar_hide_icon:
 *
 * Decrements the statusbar applet show count, if the counter becomes zero,
 * the icon will be hidden.
 **/
void statusbar_hide_icon(void);

#endif /* _RPC_STATUSBAR_H_ */
