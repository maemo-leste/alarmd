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

#ifndef _ACTION_EXEC_H_
#define _ACTION_EXEC_H_

#include <glib-object.h>
#include "actiondialog.h"

#define ALARMD_TYPE_ACTION_EXEC (alarmd_action_exec_get_type())
#define ALARMD_ACTION_EXEC(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ALARMD_TYPE_ACTION_EXEC, AlarmdActionExec))
#define ALARMD_ACTION_EXEC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ALARMD_TYPE_ACTION_EXEC, AlarmdActionExecClass))
#define ALARMD_IS_ACTION_EXEC(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), ALARMD_TYPE_ACTION_EXEC))
#define ALARMD_IS_ACTION_EXEC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ALARMD_TYPE_ACTION_EXEC))
#define ALARMD_ACTION_EXEC_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ALARMD_TYPE_ACTION_EXEC, AlarmdActionExecClass))

typedef struct _AlarmdActionExec AlarmdActionExec;
struct _AlarmdActionExec
{
       AlarmdActionDialog parent_instance;
};

typedef struct _AlarmdActionExecClass AlarmdActionExecClass;
struct _AlarmdActionExecClass
{
       AlarmdActionDialogClass parent_class;
};

/**
 * SECTION:actionexec
 * @short_description: An action that runs a command.
 * @see_also: #AlarmdAction, #AlarmdActionDbus
 *
 * #AlarmdActionExec is an action type that, upon firing, runs a command. The
 * command is run with #g_spawn_command_line_async, and the command is
 * specified in property #AlarmdActionExec:name. The command is done with the
 * same priviledges as the daemon is running.
 *
 * #AlarmdActionExec also includes the dialog showing properties of
 * #AlarmdActionDialog, see its documentation for details. The rest of the
 * action is only run if the dialog is closed; so on snooze the action will
 * be delayed too.
 **/

GType alarmd_action_exec_get_type(void);

/**
 * alarmd_action_exec_new:
 *
 * Creates new exec alarmd action.
 * Returns: Newly created #AlarmdActionExec.
 **/
AlarmdAction *alarmd_action_exec_new(void);

#endif /* _ACTION_EXEC_H_ */
