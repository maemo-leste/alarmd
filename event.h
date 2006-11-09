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

#ifndef _EVENT_H_
#define _EVENT_H_

#include <glib/gtypes.h>
#include <glib-object.h>
#include "object.h"
#include "include/timer-interface.h"

#define ALARMD_TYPE_EVENT (alarmd_event_get_type())
#define ALARMD_EVENT(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ALARMD_TYPE_EVENT, AlarmdEvent))
#define ALARMD_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ALARMD_TYPE_EVENT, AlarmdEventClass))
#define ALARMD_IS_EVENT(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), ALARMD_TYPE_EVENT))
#define ALARMD_IS_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ALARMD_TYPE_EVENT))
#define ALARMD_EVENT_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ALARMD_TYPE_EVENT, AlarmdEventClass))

typedef struct _AlarmdEvent AlarmdEvent;
struct _AlarmdEvent
{
       AlarmdObject parent_instance;
};

typedef struct _AlarmdEventClass AlarmdEventClass;
struct _AlarmdEventClass
{
       AlarmdObjectClass parent_class;

       void (*fire)(AlarmdEvent *event, gboolean delayed);
       void (*cancel)(AlarmdEvent *event);
       void (*acknowledge)(AlarmdEvent *event);
       void (*snooze)(AlarmdEvent *event);
       void (*queue)(AlarmdEvent *event, TimerPlugin *applet);
       void (*dequeue)(AlarmdEvent *event);
       gint32 (*get_flags)(AlarmdEvent *event);

       time_t (*get_time)(AlarmdEvent *event);
       gboolean (*need_power_up)(AlarmdEvent *event);
};

/**
 * SECTION:event
 * @short_description: Base class for events.
 * @see_also: #AlarmdEventRecurring, #AlarmdQueue
 *
 * A simple event that waits in the queue for being put on a timer and
 * finally getting fired. Launches the action in #AlarmdEvent:action and
 * after it has finished, removes self from the queue.
 **/

/**
 * AlarmdEvent::acknowledge:
 * @event: The #AlarmdEvent that received the signal.
 *
 * Whenever an event is acknowledged, this signal is emitted.
 **/

/**
 * AlarmdEvent::cancel:
 * @event: The #AlarmdEvent that is being cancelled.
 *
 * Whenever an #AlarmdEvent should be removed from queue, a
 * #AlarmdEvent::cancel signal is emitted.
 **/

/**
 * AlarmdEvent::dequeue:
 * @event: The #AlarmdEvent that is taken off the timer.
 *
 * Whenever an #AlarmdEvent is taken off a timer, a #AlarmdEvent::dequeue
 * signal is emitted.
 **/

/**
 * AlarmdEvent::fire:
 * @event: The #AlarmdEvent that should be fired.
 * @delayed: TRUE if the event is fired delayed.
 *
 * Whenever an #AlarmdEvent is due, a #AlarmdEvent::fire signal is emitted.
 **/

/**
 * AlarmdEvent::queue:
 * @event: The #AlarmdEvent that is put on a timer.
 *
 * Whenever an #AlarmdEvent is put on a timer, a #AlarmdEvent::queue signal is
 * emitted.
 **/

/**
 * AlarmdEvent::snooze:
 * @event: The #AlarmdEvent that is being snoozed.
 *
 * Whenever an #AlarmdEvent is snoozed, a #AlarmdEvent::snooze signal is
 * emitted.
 **/

GType alarmd_event_get_type(void);

/**
 * alarmd_event_new:
 *
 * Creates new alarmd event.
 * Returns: Newly created #AlarmdEvent
 **/
AlarmdEvent *alarmd_event_new(void);

/**
 * alarmd_event_fire:
 * @event: Event that should be fired.
 * @delayed: TRUE if the event is fired delayed.
 *
 * Emits a #AlarmdEvent::fire signal on the given @event.
 **/
void alarmd_event_fire(AlarmdEvent *event, gboolean delayed);

/**
 * alarmd_event_cancel:
 * @event: Event that should be cancelled.
 *
 * Emits a #AlarmdEvent::cancel signal on the given @event.
 **/
void alarmd_event_cancel(AlarmdEvent *event);

/**
 * alarmd_event_snooze:
 * @event: Event that should be snoozed.
 *
 * Emits a #AlarmdEvent::snooze signal on the given @event.
 **/
void alarmd_event_snooze(AlarmdEvent *event);

/**
 * alarmd_event_acknowledge:
 * @event: Event that should be acknowledged.
 *
 * Emits a #AlarmdEvent::acknowledge signal on the given @event.
 **/
void alarmd_event_acknowledge(AlarmdEvent *event);

/**
 * alarmd_event_queue:
 * @event: Event that should be put on a timer.
 * @plugin: The timer the event should be put on.
 *
 * Emits a #AlarmdEvent::queue signal on the given @event.
 **/
void alarmd_event_queue(AlarmdEvent *event, TimerPlugin *plugin);

/**
 * alarmd_event_dequeue:
 * @event: Event that should be taken off a timer.
 *
 * Emits a #AlarmdEvent::dequeue signal on the given @event.
 **/
void alarmd_event_dequeue(AlarmdEvent *event);

/**
 * alarmd_event_get_time:
 * @event: #AlarmdEvent to get the time from.
 *
 * Gets the next time an event should be run.
 * Returns: The time that the event should next be run.
 **/
time_t alarmd_event_get_time(AlarmdEvent *event);

/**
 * alarmd_event_need_power_up:
 * @event: The #AlarmdEvent that is being queried.
 *
 * Queries an event, if it needs the "power up" feature.
 * Returns: TRUE if the power up feature is needed.
 **/
gboolean alarmd_event_need_power_up(AlarmdEvent *event);

/**
 * alarmd_event_get_flags:
 * @event: The #AlarmdEvent that is being queried.
 *
 * Queries an event for its flags.
 * Returns: The bitfield for the flags.
 **/
gint32 alarmd_event_get_flags(AlarmdEvent *event);

#endif /* _EVENT_H_ */
