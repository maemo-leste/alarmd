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

#ifndef MISSING_DBUS_H_
# define MISSING_DBUS_H_

/* ========================================================================= *
 * Placeholder for DBUS services not yet available
 * ========================================================================= */

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

/* ========================================================================= *
 * DSME
 * ========================================================================= */

#include <dsme/dsme_dbus_if.h>

#define DSME_SERVICE        dsme_service

#define DSME_REQUEST_IF     dsme_req_interface
#define DSME_SIGNAL_IF      dsme_sig_interface

#define DSME_REQUEST_PATH   dsme_req_path
#define DSME_SIGNAL_PATH    dsme_sig_path

#define DSME_GET_VERSION    dsme_get_version
#define DSME_REQ_POWERUP    dsme_req_powerup
#define DSME_REQ_REBOOT     dsme_req_reboot
#define DSME_REQ_SHUTDOWN   dsme_req_shutdown

#define DSME_DATA_SAVE_SIG  dsme_save_unsaved_data_ind
#define DSME_SHUTDOWN_SIG   dsme_shutdown_ind
#define DSME_THERMAL_SIG    dsme_thermal_shutdown_ind

#ifdef __cplusplus
};
#endif

#endif /* MISSING_DBUS_H_ */
