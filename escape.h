/* ========================================================================= *
 * File: escape.h
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 17-Jun-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#ifndef ESCAPE_H_
#define ESCAPE_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

int escape_putline(FILE *file, const char *fmt, ...);
int escape_getline(FILE *file, char **pbuff, size_t *psize);

#ifdef __cplusplus
};
#endif

#endif /* ESCAPE_H_ */
