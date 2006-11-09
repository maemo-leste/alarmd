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
 * @file alarm_data.h
 * Event Daemon
 * <p>
 * Copyright (C) 2006 Nokia.  All rights reserved.
 * <p>
 * @author David Weinehall <david.weinehall@nokia.com>
 */
#ifndef _ALARM_DATA_H_
#define _ALARM_DATA_H_

#include <time.h>

extern "C" {

typedef enum {
       ALARM_ACTION_INVALID = -1,
       ALARM_ACTION_NORMAL = 0,
       ALARM_ACTION_ACTDEAD = 1
} alarmaction;

typedef struct {
       time_t alarm_time;              /* Time of alarm; UTC */
       gint recurrence;                /* Number of minutes between
                                        * each recurrence;
                                        * 0 for one-shot alarms
                                        */
       alarmaction action;             /* Action to perform on alarm */
} alarm_data;

}

#endif /* _ALARM_DATA_H_ */
