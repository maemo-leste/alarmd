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

#ifndef _ACTION_DIALOG_H_
#define _ACTION_DIALOG_H_

#include <glib-object.h>
#include "action.h"

#define ALARMD_TYPE_ACTION_DIALOG (alarmd_action_dialog_get_type())
#define ALARMD_ACTION_DIALOG(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ALARMD_TYPE_ACTION_DIALOG, AlarmdActionDialog))
#define ALARMD_ACTION_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ALARMD_TYPE_ACTION_DIALOG, AlarmdActionDialogClass))
#define ALARMD_IS_ACTION_DIALOG(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), ALARMD_TYPE_ACTION_DIALOG))
#define ALARMD_IS_ACTION_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ALARMD_TYPE_ACTION_DIALOG))
#define ALARMD_ACTION_DIALOG_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ALARMD_TYPE_ACTION_DIALOG, AlarmdActionDialogClass))

typedef struct _AlarmdActionDialog AlarmdActionDialog;
struct _AlarmdActionDialog
{
       AlarmdAction parent_instance;
};

typedef struct _AlarmdActionDialogClass AlarmdActionDialogClass;
struct _AlarmdActionDialogClass
{
       AlarmdActionClass parent_class;

       void (*do_action)(AlarmdActionDialog *dialog);
};

/**
 * SECTION:actiondialog
 * @short_description: Base class for actions that show a dialog on events.
 * @see_also: #AlarmdActionDBus, #AlarmdActionExec.
 *
 * #AlarmdActionDialog is an action type that, upon firing, may show a dialog
 * if flagged so in #AlarmdActionDialog:flags. After the dialog has been
 * acknowledged, this action type does nothing, use #AlarmdActionExec or
 * #AlarmdActionDbus to do something afterwards. The charasteristics of the
 * dialog are determined by the properties #AlarmdActionDialog:title,
 * #AlarmdActionDialog:icon, #AlarmdActionDialog:message,
 * #AlarmdActionDialog:sound and #AlarmdActionDialog:flags.
 **/

GType alarmd_action_dialog_get_type(void);

/**
 * alarmd_action_dialog_do_action:
 * @dialog: The action that should be done.
 *
 * Does the action of the #AlarmdActionDialog.
 **/
void alarmd_action_dialog_do_action(AlarmdActionDialog *dialog);

/**
 * alarmd_action_dialog_new:
 *
 * Creates new alarmd dialog action.
 * Returns: Newly created #AlarmdActionDialog.
 **/
AlarmdAction *alarmd_action_dialog_new(void);

#endif /* _ACTIONDIALOG_H_ */
