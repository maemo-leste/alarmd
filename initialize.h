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

#ifndef _INITIALIZE_H_
#define _INITIALIZE_H_

#include "queue.h"
#include "glib/gtypes.h"

/**
 * SECTION:initialize
 * @short_description: Miscellaneous initialization functions.
 *
 * Functions related to the alarmd startup.
 **/

/**
 * init_queue:
 * @queue_file: The file the queue should be loaded from.
 * @next_time_file: The file the time of next alarm should be saved to.
 * @next_mode_file: The file the mode of next alarm should be saved to.
 *
 * Initializes alarm queue from the given file.
 * Returns: New AlarmdQueue filled with events.
 **/
AlarmdQueue *init_queue(const gchar *queue_file, const gchar *next_time_file,
               const gchar *next_mode_file);

/**
 * alarmd_type_init:
 *
 * Initializes all types used in alarmd. This is needed due to design. The
 * types need to be registered to Glib type system.
 **/
void alarmd_type_init(void);

#endif /* _INITIALIZE_H_ */
