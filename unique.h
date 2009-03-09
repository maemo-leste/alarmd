#ifndef UNIQUE_H_
#define UNIQUE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

typedef struct unique_t unique_t;

/* ------------------------------------------------------------------------- *
 * unique_t
 * ------------------------------------------------------------------------- */

struct unique_t
{
  size_t un_count;
  size_t un_alloc;
  int    un_dirty;
  char **un_string;
};

void       unique_ctor     (unique_t *self);
void       unique_dtor     (unique_t *self);
unique_t * unique_create   (void);
void       unique_delete   (unique_t *self);
void       unique_delete_cb(void *self);
char     **unique_final    (unique_t *self, size_t *pcount);
char     **unique_steal    (unique_t *self, size_t *pcount);
void       unique_add      (unique_t *self, const char *str);

#ifdef __cplusplus
};
#endif

#endif /* UNIQUE_H_ */
