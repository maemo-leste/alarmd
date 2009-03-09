/* ========================================================================= *
 * Wrapper module to allow now using clockd & libtime
 * ========================================================================= */

#include <sys/time.h>
#include <time.h>

#ifndef TICKER_H_
#define TICKER_H_

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

void   ticker_use_libtime    (int enable);

int    ticker_get_synced     (void);
time_t ticker_get_time       (void);
int    ticker_set_time       (time_t tick);
time_t ticker_mktime         (struct tm *tm, const char *tz);
int    ticker_get_timezone   (char *s, size_t max);
int    ticker_get_tzname     (char *s, size_t max);
int    ticker_set_timezone   (const char *tz);
int    ticker_get_utc        (struct tm *tm);
int    ticker_get_utc_ex     (time_t tick, struct tm *tm);
int    ticker_get_local      (struct tm *tm);
int    ticker_get_local_ex   (time_t tick, struct tm *tm);
int    ticker_get_remote     (time_t tick, const char *tz, struct tm *tm);
int    ticker_get_time_format(char *s, size_t max);
int    ticker_set_time_format(const char *fmt);
int    ticker_format_time    (const struct tm *tm, const char *fmt, char *s, size_t max);
int    ticker_get_utc_offset (const char *tz);
int    ticker_get_dst_usage  (time_t tick, const char *tz);
double ticker_diff           (time_t t1, time_t t2);
int    ticker_get_time_diff  (time_t tick, const char *tz1, const char *tz2);
int    ticker_set_autosync   (int enable);
int    ticker_get_autosync   (void);

int    ticker_is_net_time_changed        (time_t *tick, char *s, size_t max);
int    ticker_activate_net_time            (void);
int    ticker_is_operator_time_accessible(void);

int    ticker_tm_is_same(const struct tm *tm1, const struct tm *tm2);

time_t ticker_get_offset(void);
int    ticker_set_offset(time_t offs);

time_t ticker_get_monotonic(void);

int    ticker_get_days_in_month(const struct tm *src);

int    ticker_has_time(const struct tm *self);
int    ticker_has_date(const struct tm *self);

void   ticker_show_tm(const struct tm *tm);
void   ticker_diff_tm(const struct tm *dst, const struct tm *src);

struct tm *ticker_break_tm(time_t t, struct tm *tm, const char *tz);
time_t     ticker_build_tm(struct tm *tm, const char *tz);

int ticker_format_time_ex(const struct tm *tm, const char *tz, const char *fmt, char *s, size_t max);

int ticker_tm_is_uninitialized(const struct tm *tm);

#ifdef __cplusplus
};
#endif

#endif /* TICKER_H_ */
