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

#ifndef RPC_RETUTIME_H
#define RPC_RETUTIME_H

#include <sys/time.h>
#include <glib/gtypes.h>

/**
 * SECTION:rpc-retutime
 * @short_description: Helpers for communicating with retu time chip.
 *
 * these functios are used to set and query the alarm state on the retu rtc
 * chip by using the retutime binary.
 **/

/**
 * retutime_set_alarm_time:
 * @alarm_time: The alarm time to set to the retu chip.
 *
 * Sets the alarm time on the retu chip to the time indicated by @alarm_time.
 * Returns: TRUE on success, FALSE on failure.
 **/
gboolean retutime_set_alarm_time(time_t alarm_time);

/**
 * retutime_disable_alarm:
 *
 * Disables the alarm on the retu rtc chip.
 * Returns: TRUE on success, FALSE on failure.
 **/
gboolean retutime_disable_alarm(void);

/**
 * retutime_ack_alarm:
 *
 * Resets the "alarm launched" flag on the retu chip.
 * Returns: TRUE on success, FALSE on failure.
 **/
gboolean retutime_ack_alarm(void);

/**
 * retutime_query_alarm:
 *
 * Queries the "alarm launched" flag from the retu chip.
 * Returns: TRUE if alarm has launched, FALSE if not.
 **/
gboolean retutime_query_alarm(void);

#endif /* RPC_RETUTIME_H */
