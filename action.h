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

#ifndef _ACTION_H_
#define _ACTION_H_

#include <glib/gtypes.h>
#include <glib-object.h>
#include "object.h"
#include "include/timer-interface.h"

/**
 * SECTION:action
 * @short_description: Base class for any actions done on events.
 * @see_also: #AlarmdActionDialog, #AlarmdActionExec, #AlarmdActionDbus.
 *
 * #AlarmdAction is a base class for objects that define the action run when
 * event is due. It itself does nothing, and should not be used as-is.
 **/

/**
 * AlarmdActionAckType:
 * @ACK_NORMAL: The action was normally acknowledged.
 * @ACK_SNOOZE: The action was snoozed.
 *
 * Defines the ways a action can be acknowledged.
 **/
typedef enum {
       ACK_NORMAL,
       ACK_SNOOZE
} AlarmdActionAckType;

#define ALARMD_TYPE_ACTION (alarmd_action_get_type())
#define ALARMD_ACTION(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ALARMD_TYPE_ACTION, AlarmdAction))
#define ALARMD_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ALARMD_TYPE_ACTION, AlarmdActionClass))
#define ALARMD_IS_ACTION(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), ALARMD_TYPE_ACTION))
#define ALARMD_IS_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ALARMD_TYPE_ACTION))
#define ALARMD_ACTION_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ALARMD_TYPE_ACTION, AlarmdActionClass))

typedef struct _AlarmdAction AlarmdAction;
struct _AlarmdAction
{
       AlarmdObject parent_instance;
};

typedef struct _AlarmdActionClass AlarmdActionClass;
struct _AlarmdActionClass
{
       AlarmdObjectClass parent_class;

       void (*run)(AlarmdAction *action, gboolean delayed);
       void (*acknowledge)(AlarmdAction *action, AlarmdActionAckType ack_type);
       gboolean (*need_power_up)(AlarmdAction *action);
};

/**
 * AlarmdAction::acknowledge:
 * @action: The action that is acknowledged.
 * @ack_type: How was the @action acknowledged, as defined by
 * #AlarmdActionAckType.
 *
 * When an #AlarmdAction is acknowledged, it should emit the acknowledge
 * signal.
 **/

/**
 * AlarmdAction::run:
 * @action: The action that is run.
 * @delayed: Specifies if the execution of @action is delayed.
 *
 * When an #AlarmdAction should be run, this signal should be emitted.
 **/

GType alarmd_action_get_type(void);

/**
 * alarmd_action_new:
 *
 * Creates new alarmd action.
 * Returns: Newly created #AlarmdAction.
 **/
AlarmdAction *alarmd_action_new(void);

/**
 * alarmd_action_run:
 * @action: A #AlarmdAction that should be run.
 * @delayed: Whether the #AlarmdAction is run on time or after it.
 *
 * Emits "run" signal on the given #AlarmdAction. The interpretion of the
 * @delayed flag depends on the action type.
 **/
void alarmd_action_run(AlarmdAction *action, gboolean delayed);

/**
 * alarmd_action_acknowledge:
 * @action: #AlarmdAction that is acknowledged.
 * @ack_type: %ACK_NORMAL if the action was normally acknowledged or
 * %ACK_SNOOZE if it was snoozed.
 *
 * Emits "acknowledge" -signal on the given #AlarmdAction.
 **/
void alarmd_action_acknowledge(AlarmdAction *action,
               AlarmdActionAckType ack_type);

/**
 * alarmd_action_need_power_up:
 * @action: #AlarmdAction that is being queried.
 *
 * Queries the given #AlarmdAction, if it needs power up functionality.
 *
 * Returns: TRUE if the #AlarmdAction needs power up, FALSE otherwise.
 **/
gboolean alarmd_action_need_power_up(AlarmdAction *action);

#endif /* _ACTION_H_ */
