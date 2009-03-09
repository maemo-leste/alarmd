#ifndef QUEUE_H_
#define QUEUE_H_

#include "libalarm.h"

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

unsigned       queue_get_snooze       (void);
void           queue_set_snooze       (unsigned snooze);

cookie_t       queue_add_event        (alarm_event_t *event);
alarm_event_t *queue_get_event        (cookie_t cookie);
int            queue_del_event        (cookie_t cookie);
cookie_t      *queue_query_events     (int *pcnt, time_t lo, time_t hi, unsigned mask, unsigned flag, const char *app);

unsigned       queue_get_event_state  (const alarm_event_t *self);
void           queue_set_event_state  (alarm_event_t *self, unsigned state);
cookie_t      *queue_query_by_state   (int *pcnt, unsigned state);
int            queue_count_by_state   (unsigned state);
void           queue_cleanup_deleted  (void);

void           queue_set_event_trigger(alarm_event_t *event, time_t trigger);

void           queue_save             (void);
void           queue_load             (void);
int            queue_init             (void);
void           queue_quit             (void);

void           queue_set_dirty        (void);
void           queue_clr_dirty        (void);
int            queue_is_dirty         (void);

void           queue_set_restore_cb   (void (*cb)(void));

typedef enum alarmeventstates
{
#define ALARM_STATE(name) ALARM_STATE_##name,
#include "states.inc"
} alarmeventstates;

#ifdef __cplusplus
};
#endif

#endif /* QUEUE_H_ */
