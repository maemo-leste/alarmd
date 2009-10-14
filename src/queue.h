/* ========================================================================= *
 *
 * This file is part of Alarmd
 *
 * Copyright (C) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * Alarmd is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Alarmd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Alarmd; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * ========================================================================= */

#ifndef QUEUE_H_
#define QUEUE_H_

#include "libalarm.h"

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

/* ========================================================================= *
 * extern functions
 * ========================================================================= */

void           queue_set_modified_cb  (void (*cb)(void));
unsigned       queue_get_snooze       (void);
void           queue_set_snooze       (unsigned snooze);
void           queue_event_set_trigger(alarm_event_t *event, time_t trigger);
unsigned       queue_event_get_state  (const alarm_event_t *self);
void           queue_event_set_state  (alarm_event_t *self, unsigned state);
cookie_t       queue_add_event        (alarm_event_t *event);
alarm_event_t *queue_get_event        (cookie_t cookie);
int            queue_del_event        (cookie_t cookie);
cookie_t      *queue_query_events     (int *pcnt, time_t lo, time_t hi, unsigned mask, unsigned flag, const char *app);
cookie_t      *queue_query_by_state   (int *pcnt, unsigned state);
int            queue_count_by_state   (unsigned state);
void           queue_cleanup_deleted  (void);
void           queue_save             (void);
void           queue_load             (void);
void           queue_save_forced      (void);
void           queue_set_dirty        (void);
void           queue_clr_dirty        (void);
int            queue_is_dirty         (void);
int            queue_init             (void);
void           queue_quit             (void);

/* ========================================================================= *
 * states for queued events
 * ========================================================================= */

typedef enum alarmeventstates
{
#define ALARM_STATE(name) ALARM_STATE_##name,
#include "states.inc"
} alarmeventstates;

#ifdef __cplusplus
};
#endif

#endif /* QUEUE_H_ */
