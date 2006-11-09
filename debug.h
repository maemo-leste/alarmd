/**
 * This file is part of alarmd
 *
 * Contact Person: David Weinehall <david.weinehall@nokia.com>
 *
 * Copyright (C) 2006 Nokia Corporation
 * alarmd and libalarm are free software; you can redistribute them
 * and/or modify them under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * alarmd and libalarm are distributed in the hope that they will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

/**
 * SECTION:debug
 * @short_description: Debugging macros & functions.
 *
 * Alarmd internal debugging macros & functions.
 **/

#ifdef DEBUG_FUNC
/**
 * ENTER_FUNC:
 *
 * Indicates that we're entering a function.
 * Displays a message and increments the indentation in the
 * debugging output.
 **/
#define ENTER_FUNC enter_func(__FUNCTION__)

/**
 * LEAVE_FUNC:
 *
 * Indicates that we're leaving a function.
 * Displays a message and decrements the indentation in the debugging output.
 **/
#define LEAVE_FUNC leave_func(__FUNCTION__)

/**
 * DEBUG:
 *
 * Writes a debug message to stdout with the current indentation level.
 **/
#define DEBUG(...) dbg(__FUNCTION__, __VA_ARGS__)

#else

#define ENTER_FUNC
#define LEAVE_FUNC
#define DEBUG(...)

#endif

/**
 * enter_func:
 * @name: Name of function being entered.
 *
 * Should be called as #ENTER_FUNC at start of a function.
 **/
void enter_func(const char *name);

/**
 * leave_func:
 * @name: Name of function being entered.
 *
 * Should be called as #LEAVE_FUNC at the end of a function.
 **/
void leave_func(const char *name);

/**
 * dbg:
 * @func: Name of function writing debug info.
 * @format: Format for the message being printed (printf like).
 * @...: Values to fill into the format string.
 *
 * Writes a debug message; should be used through #DEBUG macro.
 **/
void dbg(const char *func, const char *format, ...);

#endif
