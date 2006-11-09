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

#ifndef _RPC_OSSO_H_
#define _RPC_OSSO_H_

#include <libosso.h>
#include "queue.h"

/**
 * SECTION:rpc-osso
 * @short_description: Handles all libosso related things.
 *
 * These functions are the ones that do all the osso de/initialization.
 **/

/**
 * init_osso:
 * @queue: A #AlarmdQueue that will be notified on time changes, and where
 * new events should be added to.
 *
 * Initializes osso connectivity.
 * Returns: The osso_context_t returned by osso_initialize or NULL on failure.
 **/
osso_context_t *init_osso(void);

/**
 * set_osso_callbacks:
 * @osso: A pointer to the osso_context_t struct, as returned by init_osso.
 * @queue: The queue that the callbacks should affect.
 *
 * Starts listening on dbus system and session buses for incoming requests.
 **/
void set_osso_callbacks(osso_context_t *osso, AlarmdQueue *queue);

/**
 * deinit_osso:
 * @osso: The osso_context_t structure as returned by init_osso.
 * @queue: The #AlarmdQueue that was passed to init_osso.
 *
 * Deinitializes osso connectivity. Stops listening on dbus buses.
 **/
void deinit_osso(osso_context_t *osso, AlarmdQueue *queue);

#endif /* _RPC_OSSO_H_ */
