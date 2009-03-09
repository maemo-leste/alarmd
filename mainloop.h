/* ========================================================================= *
 * File: mainloop.h
 *
 * Copyright (C) 2007 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 06-Dec-2007 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#ifndef MAINLOOP_H_
#define MAINLOOP_H_

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

/* -- mainloop -- */

int mainloop_stop(int force);
int mainloop_run (void);

#ifdef __cplusplus
};
#endif

#endif /* MAINLOOP_H_ */
