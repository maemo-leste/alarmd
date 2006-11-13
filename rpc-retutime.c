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

#include <unistd.h>
#include <glib/gstrfuncs.h>
#include <glib/gspawn.h>
#include <glib/gmem.h>
#include <time.h>
#include "rpc-retutime.h"


#include <stdio.h>

#define RETUTIME_TIME_FORMAT "%Y-%m-%d/%H:%M:%S"
#define RETUTIME_DISABLE "0000-00-00/00:00:00"
#define RETUTIME_CMD "/usr/sbin/chroot /mnt/initfs /usr/bin/retutime "
#define USER_RETUTIME_CMD "sudo " RETUTIME_CMD

#define RETUTIME_SET_ALARM "-A "
#define RETUTIME_ACK_ALARM "-S "
#define RETUTIME_QRY_ALARM "-s "

static gint _retutime_do(const gchar *operation,
	       	const gchar *argument,
		gchar **output) {
	const gchar *command;
	gchar *cmd;
	gint retval = 1;

	if (getuid() == 0) {
		command = RETUTIME_CMD;
	} else {
		command = USER_RETUTIME_CMD;
	}

	cmd = g_strconcat(command, operation, argument, NULL);
	
	fprintf(stderr, "Running command %s\n", cmd);
	g_spawn_command_line_sync(cmd, output, NULL, &retval, NULL);
	g_free(cmd);

	return retval;
}

gboolean retutime_set_alarm_time(time_t alarm_time) {
	gchar buffer[64];
	struct tm ftm;
	gint status;

	gmtime_r(&alarm_time, &ftm);
	strftime(buffer, 63, RETUTIME_TIME_FORMAT, &ftm);
	buffer[63] = '\0';

	status = _retutime_do(RETUTIME_SET_ALARM, buffer, NULL);

	if (status) {
		return FALSE;
	}
	return TRUE;
}

gboolean retutime_disable_alarm(void) {
	gint status = _retutime_do(RETUTIME_SET_ALARM,
		       	RETUTIME_DISABLE,
		       	NULL);

	if (status) {
		return FALSE;
	}
	return TRUE;
}

gboolean retutime_ack_alarm(void) {
	gint status = _retutime_do(RETUTIME_ACK_ALARM,
			NULL,
			NULL);

	if (status) {
		return FALSE;
	}
	return TRUE;
}

gboolean retutime_query_alarm(void) {
	gchar *state = NULL;
	gboolean retval = FALSE;
	gint status = _retutime_do(RETUTIME_QRY_ALARM,
			NULL,
			&state);

	if (state) {
		if (*state == '1') {
			retval = TRUE;
		}
		g_free(state);
	}

	if (status) {
		return FALSE;
	}
	return retval;
}
