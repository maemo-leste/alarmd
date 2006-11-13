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

#include <cal.h>
#include <time.h>
#include <alarmd/alarm_data.h>

typedef enum {
	ALARM_ERR = -1,			/* The alarm was spurious
					 * or something failed
					 */
	ALARM_NORMAL = 0,		/* Alarm; bootup in normal mode */
	ALARM_ACTDEAD = 1,		/* Alarm; bootup in acting dead mode */
	ALARM_FUTURE = 2		/* Alarm reprogrammed; shutdown */
} alarmretval;

#define TIME_T_24H			24 * 60 * 60	/* 24h in seconds */

/*
 * Check the alarm status
 * @return ALARM_ERR If there are no pending alarms (and on error),
 *         ALARM_NORMAL to bootup in normal mode
 *         ALARM_ACTDEAD to bootup in acting dead mode
 *         ALARM_FUTURE the alarm was reprogrammed; shutdown
 */
static int check_alarm_status(void)
{
	struct cal *cal_data = NULL;
	time_t alarm_time = (time_t)-1;
	time_t now;
	int olderrno = errno;
	int status = ALARM_ERR;
	alarmaction action = ALARM_ACTION_INVALID;

	/* If time() fails, ignore the alarm */
	if ((now = time(NULL)) == -1) {
		errno = olderrno;
		goto EXIT;
	}

	/* Retrieve the alarm stored in CAL */
	if (cal_init(&cal_data) == 0) {
		void *ptr = NULL;
		unsigned long len;

		if (cal_read_block(cal_data, "alarm", &ptr, &len,
				   CAL_FLAG_USER) == 0) {
			struct alarm_data *alarmdata = (struct alarm_data *)ptr;

			alarm_time = alarm_data->alarm_time;
			action = alarm_data->action;
		} else {
			goto EXIT;
		}

		cal_finish(cal_data);
	} else {
		goto EXIT;
	}

	/* If alarm_time is == -1, then no alarm has been set */
	if (alarm_time == -1) {
		status = ALARM_ERR;
		goto EXIT;
	} else if ((alarm_time - now) > TIME_T_24H) {
		status = ALARM_FUTURE;

	} else {
		if (action == ALARM_ACTION_NORMAL)
			status = ALARM_NORMAL;
		else if (action == ALARM_ACTION_ACTDEAD)
			status = ALARM_ACTDEAD;
		else
			status = ALARM_ERR;
	}

EXIT:
	return status;
}
