/**
 * This file is part of alarmd
 *
 * Contact Person: David Weinehall <david.weinehall@nokia.com>
 *
 * Copyright (C) 2006-2007 Nokia Corporation
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

#ifndef _RPC_IC_H_
#define _RPC_IC_H_

#include <glib/gtypes.h>

/**
 * SECTION:rpc-ic
 * @short_description: Helper functions to communicate with ic.
 *
 * Here there be helper functions that can be used to get conncetivity
 * status from ic.
 **/

/**
 * ICDConnectedNotifyCb:
 * @user_data: user data set when the signal handler was connected.
 *
 * Callback to be called when a connection is connected.
 **/
typedef void (*ICConnectedNotifyCb)(gpointer user_data);

/**
 * ic_wait_connection:
 * @cb: Callback to call when a connection is established.
 * @user_data: User data to pass to the @cb.
 *
 * Waits for internet connection and calls cb when one comes.
 **/
void ic_wait_connection(ICConnectedNotifyCb cb, gpointer user_data);

/**
 * ic_unwait_connection:
 * @cb: Callback that should be removed.
 * @user_data: user daata for callback that should be removed.
 *
 * Stopds waiting for internet connection for given callback and data.
 **/
void ic_unwait_connection(ICConnectedNotifyCb cb, gpointer user_data);

#endif
