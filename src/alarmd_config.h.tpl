/* -*- mode: c -*- */

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

#ifndef ALARMD_CONFIG_H_
#define ALARMD_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

// Fixes: NB#141279 - Alarm not shown in UI after "snooze" value set
//        to 0 in alarm_queue.ini
#define FIX_BUG_141279 0 // ignore modified queue file

/* ------------------------------------------------------------------------- *
 * Alarmd monitors contents of the queue file. Nobody should have any need
 * to touch that file - apart from osso-backup restoration process. What
 * happens if the file is modified by somebody else than alarmd is configured
 * here.
 * ------------------------------------------------------------------------- */

/* If non-zero, alarmd will give osso-backup some time to restart
 * alarmd. If that does not happen, alarmd will self terminate and
 * then gets restarted by dsme process lifeguard.
 *
 * This is the documented behavior, but odd things may happen if
 * somebody else than osso-restore modifies the queue file.
 */
#if FIX_BUG_141279
# define ALARMD_QUEUE_MODIFIED_RESTART 0
#else
# define ALARMD_QUEUE_MODIFIED_RESTART 1
#endif

/* If non-zero, alarmd will give osso-backup some time to restart
 * alarmd. If that does not happen, alarmd will replace the modified
 * queue file using internally held state information.
 *
 * Effectively this will make alarmd ignore all modifications made
 * to the queue file unless followed by alarmd restart that is normally
 * part of restore process.
 */
#if FIX_BUG_141279
# define ALARMD_QUEUE_MODIFIED_IGNORE  1
#else
# define ALARMD_QUEUE_MODIFIED_IGNORE  0
#endif

/* ------------------------------------------------------------------------- *
 * D-Bus connection configuration
 * ------------------------------------------------------------------------- */

#define ALARMD_USE_PRIVATE_BUS 0
#define ALARMD_ON_SESSION_BUS  0
#define ALARMD_ON_SYSTEM_BUS   1

#if !ALARMD_ON_SESSION_BUS == !ALARMD_ON_SYSTEM_BUS
# error alrmd dbus configuration error
#endif

/* ------------------------------------------------------------------------- *
 * Enabling special clear-user-data and restore-factory-settings code
 * ------------------------------------------------------------------------- */

/* Enable '-Xcud' command line option */
#define ALARMD_CUD_ENABLE 1

/* Enable '-Xrfs' command line option */
#define ALARMD_RFS_ENABLE 1

/* ------------------------------------------------------------------------- *
 * Various flags originating from Makefile
 * ------------------------------------------------------------------------- */

#define ALARMD_CONFIG_NAME      "@NAME@"
#define ALARMD_CONFIG_VERS      "@VERS@"

#define ALARMD_CONFIG_BINDIR    "@BINDIR@"
#define ALARMD_CONFIG_LIBDIR    "@LIBDIR@"
#define ALARMD_CONFIG_DLLDIR    "@DLLDIR@"
#define ALARMD_CONFIG_INCDIR    "@INCDIR@"
#define ALARMD_CONFIG_DOCDIR    "@DOCDIR@"
#define ALARMD_CONFIG_MANDIR    "@MANDIR@"

#define ALARMD_CONFIG_CACHEDIR  "@CACHEDIR@"
#define ALARMD_CONFIG_DEVDOCDIR "@DEVDOCDIR@"
#define ALARMD_CONFIG_PKGCFGDIR "@PKGCFGDIR@"

/* ------------------------------------------------------------------------- *
 * Boot control in alarm event action flags. TODO: remove the whole thing
 * ------------------------------------------------------------------------- */
#define ALARMD_ACTION_BOOTFLAGS 0

#ifdef __cplusplus
};
#endif

#endif /* ALARMD_CONFIG_H_ */
