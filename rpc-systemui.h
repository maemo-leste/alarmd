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

#ifndef _RPC_SYSTEMUI_H_
#define _RPC_SYSTEMUI_H_

#include <time.h>
#include <glib/gtypes.h>

/**
 * SECTION:rpc-systemui
 * @short_description: Helpers for communicating with systemui alarm plugin.
 *
 * These functions maintain a queue of alarm dialog requests and passes them
 * to systemui as previous ones are closed.
 **/

/**
 * SystemuiDialogReason:
 * @DIALOG_ACK: An alarm dialog was acked normally.
 * @DIALOG_SNOOZE: An alarm dialog was snoozed.
 * @DIALOG_POWEROFF: An alarm dialog was acked due to shutdown.
 * @DIALOG_POWERUP: An powerup dialog was acked for power up.
 * @DIALOG_POWERDOWN: An powerup dialog was acked for power down.
 *
 * An enumeration describing the possible acknowledge reasons for dialogs.
 **/
typedef enum {
	DIALOG_ACK,
	DIALOG_SNOOZE,
	DIALOG_POWEROFF,
	DIALOG_POWERUP,
	DIALOG_POWERDOWN
} SystemuiDialogReason;

/**
 * SystemuiAlarmdDialogCallback:
 * @user_data: user data set when the callback was connected.
 * @reason: The reason for calling the callback, will be one of #SystemuiDialogReason.
 *
 * Defines the callbeck used for replies from systemui-alarm.
 **/
typedef void (*SystemuiAlarmdDialogCallback)(gpointer user_data, SystemuiDialogReason reason);

/**
 * systemui_alarm_dialog_queue_append:
 * @alarm_time: Time of the alarm that should be shown in the dialog.
 * @title: Title of the dialog to be appended to the queue.
 * @message: Message of the dialog to be appended to the queue.
 * @sound: Sound for the dialog to be appended to the queue.
 * @icon: Icon for the dialog to be appended to the queue.
 * @can_snooze: TRUE if the dialog should have snooze button enabled.
 * @cb: Callback called when the dialog is acknowledged.
 * @user_data: user date to pass to the callback.
 *
 * Adds a request for a dialog to the queue. After the dialog has been
 * acknowledged, the request is removed from the queue, no need to remove
 * it manually.
 **/
void systemui_alarm_dialog_queue_append(time_t alarm_time, const gchar *title, const gchar *message, const gchar *sound, const gchar *icon, gboolean can_snooze, SystemuiAlarmdDialogCallback cb, gpointer user_data);

/**
 * systemui_alarm_dialog_queue_remove:
 * @alarm_time: As passed to #systemui_alarm_dialog_queue_append.
 * @title: As passed to #systemui_alarm_dialog_queue_append.
 * @message: As passed to #systemui_alarm_dialog_queue_append.
 * @sound: As passed to #systemui_alarm_dialog_queue_append.
 * @icon: As passed to #systemui_alarm_dialog_queue_append.
 * @cb: As passed to #systemui_alarm_dialog_queue_append.
 * @user_data: As passed to #systemui_alarm_dialog_queue_append.
 *
 * Removes an alarm dialog request from the dialog queue.
 **/
void systemui_alarm_dialog_queue_remove(time_t alarm_time, const gchar *title, const gchar *message, const gchar *sound, const gchar *icon, SystemuiAlarmdDialogCallback cb, gpointer user_data);

/**
 * systemui_powerup_dialog_queue_append:
 * @cb: Callback called when the dialog is acknowledged.
 * @user_data: user date to pass to the callback.
 *
 * Appends a powerup dialog request to the dialog queue. After the dialog has
 * been acknowledged, the request is removed from the queue, no need to remove
 * it manually.
 **/
void systemui_powerup_dialog_queue_append(SystemuiAlarmdDialogCallback cb, gpointer user_data);

/**
 * systemui_powerup_dialog_queue_remove:
 * @cb: As passed to #systemui_powerup_dialog_queue_append.
 * @user_data: As passed to #systemui_powerup_dialog_queue_append.
 *
 * Removes a powerup dialog request from the dialog queue.
 **/
void systemui_powerup_dialog_queue_remove(SystemuiAlarmdDialogCallback cb, gpointer user_data);

/**
 * update_mce_alarm_visibility:
 *
 * Sends message to mce telling the alarm dialog visibility status.
 **/
void update_mce_alarm_visibility(void);

/**
 * systemui_ack_all_dialogs:
 *
 * Clears the queue acknowledging all alarms and tells systemui to close the
 * currently showing one.
 **/
void systemui_ack_all_dialogs(void);

/**
 * systemui_is_acting_dead:
 *
 * Queries system UI whether the acting dead mode is enabled or not.
 * Returns: TRUE if device is in acting dead, false otherwise.
 **/
gboolean systemui_is_acting_dead(void);

#endif /* _RPC_SYSTEMUI_H_ */
