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

#define ALARMD_USE_PRIVATE_BUS 0
#define ALARMD_ON_SESSION_BUS  0
#define ALARMD_ON_SYSTEM_BUS   1

#if !ALARMD_ON_SESSION_BUS == !ALARMD_ON_SYSTEM_BUS
# error alrmd dbus configuration error
#endif

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

#define ALARMD_CUD_ENABLE 1
#define ALARMD_RFS_ENABLE 1

#define ALARMD_ACTION_BOOTFLAGS 0

#ifdef __cplusplus
};
#endif

#endif /* ALARMD_CONFIG_H_ */
