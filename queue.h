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

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "event.h"
#include "object.h"

#define ALARMD_TYPE_QUEUE (alarmd_queue_get_type())
#define ALARMD_QUEUE(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ALARMD_TYPE_QUEUE, AlarmdQueue))
#define ALARMD_QUEUE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ALARMD_TYPE_QUEUE, AlarmdQueueClass))
#define ALARMD_IS_QUEUE(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), ALARMD_TYPE_QUEUE))
#define ALARMD_IS_QUEUE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ALARMD_TYPE_QUEUE))
#define ALARMD_QUEUE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ALARMD_TYPE_QUEUE, AlarmdQueueClass))

typedef struct _AlarmdQueue AlarmdQueue;
struct _AlarmdQueue
{
	AlarmdObject parent_instance;
};

typedef struct _AlarmdQueueClass AlarmdQueueClass;
struct _AlarmdQueueClass
{
	AlarmdObjectClass parent_class;

	glong (*add_event)(AlarmdQueue *queue, AlarmdEvent *event);
	gboolean (*remove_event)(AlarmdQueue *queue, glong event_id);
	AlarmdEvent *(*get_event)(AlarmdQueue *queue, glong event_id);
	glong *(*query_events)(AlarmdQueue *queue, gint64 start_time, gint64 end_time, gint32 flag_mask, gint32 flags, guint *n_events);
};

/**
 * SECTION:queue
 * @short_description: Object type to handle the event queue.
 * @see_also: #AlarmdEvent
 *
 * The #AlarmdQueue handles the #AlarmdEvent objects. It has timer plugins
 * that handle the events; one that can power up the device and one that
 * cannot. If only one timer is available, it is used for all events.
 **/

GType alarmd_queue_get_type(void);

/**
 * alarmd_queue_new:
 *
 * Creates new alarmd queue.
 * Returns: Newly created #AlarmdQueue
 **/
AlarmdQueue *alarmd_queue_new(void);

/**
 * alarmd_queue_add_event:
 * @queue: A #AlarmdQueue the @event should be added into.
 * @event: A #AlarmdEvent that should be added to the queue.
 *
 * Adds the @event to the @queue to be launched at apecified time.
 * Returns: Unique identifier for the event.
 **/
gulong alarmd_queue_add_event(AlarmdQueue *queue, AlarmdEvent *event);

/**
 * alarmd_queue_remove_event:
 * @queue: The #AlamrmdQueue from where the event should be removed from.
 * @event_id: The identifier for the #AlarmdEvent.
 *
 * Removes the event specified by the identifier @event_id from the @queue.
 * Returns: TRUE if the event was found and removed, FALSE otherwise.
 **/
gboolean alarmd_queue_remove_event(AlarmdQueue *queue, gulong event_id);

/**
 * alarmd_queue_query_events:
 * @queue: The #AlarmdQueue that should be queried.
 * @start_time: Events that launch avter this time should be returned.
 * @end_time: Events that launch before this time should be returned.
 * @flag_mask: Bitfield that specifies whchi fields should be checked.
 * @flags: The values wanted for the flags specified in @flag_mask.
 * @n_events: Pointer to an integer that should contain the number of events
 * found.
 *
 * Queries events that occurr between @start_time adn @end_time with flags
 * chosen with @flag_mask that have values specified in @flags.
 * Returns: Array of identifiers for the found events.
 **/
glong *alarmd_queue_query_events(AlarmdQueue *queue, gint64 start_time, gint64 end_time, gint32 flag_mask, gint32 flags, guint *n_events);

/**
 * alarmd_queue_get_event:
 * @queue: The #AlarmdQueue that contains the event.
 * @event_id: The identifier for the #AlarmdEvent.
 *
 * Finds and returns the wanted event from the queue. The event is still on
 * the queue.
 * Returns: The #AlarmdEvent with the wanted identifier or NULL if not found.
 **/
AlarmdEvent *alarmd_queue_get_event(AlarmdQueue *queue, gulong event_id);

#endif /* _QUEUE_H_ */
