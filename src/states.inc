// -*- mode: c -*-

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

#ifdef ALARM_STATE

ALARM_STATE(NEW)
ALARM_STATE(WAITCONN)
ALARM_STATE(QUEUED)
ALARM_STATE(MISSED)
ALARM_STATE(POSTPONED)

ALARM_STATE(LIMBO)

ALARM_STATE(TRIGGERED)
ALARM_STATE(WAITSYSUI)
ALARM_STATE(SYSUI_REQ)
ALARM_STATE(SYSUI_ACK)
ALARM_STATE(SYSUI_RSP)

ALARM_STATE(SNOOZED)
ALARM_STATE(SERVED)
ALARM_STATE(RECURRING)
ALARM_STATE(DELETED)

ALARM_STATE(FINALIZED)

# undef ALARM_STATE
#endif
