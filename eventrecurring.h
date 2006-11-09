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

#ifndef _EVENTRECURRING_H_
#define _EVENTRECURRING_H_

#include <glib/gtypes.h>
#include <glib-object.h>
#include "object.h"
#include "event.h"

#define ALARMD_TYPE_EVENT_RECURRING (alarmd_event_recurring_get_type())
#define ALARMD_EVENT_RECURRING(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ALARMD_TYPE_EVENT_RECURRING, AlarmdEventRecurring))
#define ALARMD_EVENT_RECURRING_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ALARMD_TYPE_EVENT_RECURRING, AlarmdEventRecurringClass))
#define ALARMD_IS_EVENT_RECURRING(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), ALARMD_TYPE_EVENT_RECURRING))
#define ALARMD_IS_EVENT_RECURRING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ALARMD_TYPE_EVENT_RECURRING))
#define ALARMD_EVENT_RECURRING_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ALARMD_TYPE_EVENT_RECURRING, AlarmdEventRecurringClass))

typedef struct _AlarmdEventRecurring AlarmdEventRecurring;
struct _AlarmdEventRecurring
{
       AlarmdEvent parent_instance;
};

typedef struct _AlarmdEventRecurringClass AlarmdEventRecurringClass;
struct _AlarmdEventRecurringClass
{
       AlarmdEventClass parent_class;
};

/**
 * SECTION:eventrecurring
 * @short_description: An event that recurrs many time.
 * @see_also: #AlarmdEvent, #AlarmdQueue
 *
 * Almost like #AlarmdEvent, but unlike it, the #AlarmdEventRecurring may,
 * after the action has finished, move it's time by certain interval (as
 * specified in #AlarmdEventRecurring:recurr-interval) forward. The event
 * may recurr as many times as specified in #AlarmdEventRecurring:recurr-count.
 **/

GType alarmd_event_recurring_get_type(void);

/**
 * alarmd_event_recurring_new:
 *
 * Creates new recurring alarmd event.
 * Returns: Newly created #AlarmEventRecurring.
 **/
AlarmdEventRecurring *alarmd_event_recurring_new(void);

#endif /* _EVENTRECURRING_H_ */
