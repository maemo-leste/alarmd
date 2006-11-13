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

#ifndef _ACTION_DBUS_H_
#define _ACTION_DBUS_H_

#include <glib-object.h>
#include "actiondialog.h"

#define ALARMD_TYPE_ACTION_DBUS (alarmd_action_dbus_get_type())
#define ALARMD_ACTION_DBUS(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ALARMD_TYPE_ACTION_DBUS, AlarmdActionDbus))
#define ALARMD_ACTION_DBUS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ALARMD_TYPE_ACTION_DBUS, AlarmdActionDbusClass))
#define ALARMD_IS_ACTION_DBUS(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), ALARMD_TYPE_ACTION_DBUS))
#define ALARMD_IS_ACTION_DBUS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ALARMD_TYPE_ACTION_DBUS))
#define ALARMD_ACTION_DBUS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ALARMD_TYPE_ACTION_DBUS, AlarmdActionDbusClass))

typedef struct _AlarmdActionDbus AlarmdActionDbus;
struct _AlarmdActionDbus
{
	AlarmdActionDialog parent_instance;
};

typedef struct _AlarmdActionDbusClass AlarmdActionDbusClass;
struct _AlarmdActionDbusClass
{
	AlarmdActionDialogClass parent_class;
};

/**
 * SECTION:actiondbus
 * @short_description: An action type that sends a DBus message when launched.
 * @see_also: #AlarmdActionDialog, #AlarmActionExec
 *
 * #AlarmdActionDbus is a action type that, upon firing, sends a message over
 * dbus. The message may be sent on session or system bus. The semantics of
 * this message are determined by properties #AlarmdActionDbus:interface,
 * #AlarmdActionDbus:name, #AlarmdActionDbus:service, #AlarmdActionDbus:path
 * and inherited property #AlarmdActionDialog:flags.
 *
 * #AlarmdActionDbus also includes the dialog showing properties of
 * #AlarmdActionDialog, see its documentation for details. The rest of the
 * action is only run if the dialog is closed; so on snooze the action will
 * be delayed too.
 **/

GType alarmd_action_dbus_get_type(void);

/**
 * alarmd_action_dbus_new:
 *
 * Creates new alarmd dbus action object.
 * Returns: Newly created #AlarmdActionDbus.
 **/
AlarmdAction *alarmd_action_dbus_new(void);

#endif /* _ACTION_DBUS_H_ */
