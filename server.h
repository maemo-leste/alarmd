/* ========================================================================= *
 * File: proxy.h
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

#ifndef SERVER_H_
# define SERVER_H_

# ifdef __cplusplus
extern "C" {
# endif

  int  server_init(void);
  void server_quit(void);

# ifdef __cplusplus
};
# endif

#endif /* SERVER_H_ */
