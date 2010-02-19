/**
 * @brief C API to alarm data
 *
 * @file libalarm.h
 *
 * @author Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * This is the API for accessing data on alarm daemon.
 *
 * <p>
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
 */

/**
 * @mainpage
 *
 * @author Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * @section copyright Copyright
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
 * @section introduction Introduction
 *
 * Alarm service is a subsysten that consists of two parts:
 * - alarmd - daemon process that keeps track of current alarms
 * - libalarm - client library for accessing the alarm data
 *
 * @section architecture Architecture
 *
 * Simplified view to system utilizing alarm service:
 * <ul>
 * <li> Alarm daemon supports any number of clients
 * <li> Application code includes libalarm.h
 * <li> Application binary is linked against libalarm.so
 * <li> Libalarm utilizes D-Bus to communicate with the server
 * <li> Alarm daemon accepts request on dbus
 * <li> Alarm data is stored to a file
 * <li> HW RTC can be used to boot up the device
 * <li> Triggered alarms are send to system ui dialog service
 * <li> System ui sends user response back to alarmd
 * <li> Snooze / Execute / D-Bus action is performed by alarm daemon
 * </ul>
 *
 * ASCII art diagram of the above:
 *
 * @code
 *   +----------+  +-------+  +-------+
 *   |          |  |       |  |       |
 *   | calendar |  | clock |  | app X |
 *   |          |  |       |  |       |
 * ======================================  libalarm.h
 *               |          |
 *               | libalarm |
 *               |          |
 *               +----------+
 *                    ^
 *                    |
 *                  ==z=== D-Bus
 *                    |
 *                    v
 *               +---------+
 *               |         |            +---------+
 *               |         |<-----------| snooze  |
 *               |         |            +---------+
 *               | alarmd  |                  ^
 *               |         |                  |
 *  +-------+    |         |            +------------------+
 *  | alarm |<-->|         |----------->|  alarm action    |
 *  | queue |    +---------+            +------------------+
 *  | file  |     ^     | ^                   |          |
 *  +-------+     |     | |            exec ==z==      ==z== D-Bus
 *                |     | |                   |          |
 *                |     | |                   v          v
 *       ioctl ===z== ==z=z=== D-Bus    +---------+ +-------------+
 *                |     | |             |         | | dbus signal |
 *                v     v |             | command | | or method   |
 *      +----------+  +----------+      |         | | call        |
 *      |          |  | systemui |      +---------+ +-------------+
 *      | /dev/rtc |  | dialog   |
 *      |          |  | service  |
 *      +----------+  +----------+
 *                        ^
 *                        |
 *                      ==z=== Touchscreen
 *                        |
 *                        v
 *                    +----------+
 *                    | response |
 *                    | from the |
 *                    | user     |
 *                    +----------+
 * @endcode
 *
 * @section svn SVN
 *
 * All source code is stored in subversion repository
 * @e dms, subdirectory @e alarmd.
 */

#ifndef LIBALARM_H_
#define LIBALARM_H_

#include <stdint.h>
#include <stdarg.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

/* Crude preprocessor tricks to make direct access to
 * some member variables more visible - used for alarmd
 * compilation, hidden from doxygen scanner */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
# ifdef ALARMD_MANGLE_MEMBERS
#  define ALARMD_CAT2(a,b) a##b
#  define ALARMD_PRIVATE(x) ALARMD_CAT2(x,_private)
# else
#  define ALARMD_PRIVATE(x) x
# endif
#endif

#pragma GCC visibility push(default)

/** \brief Unique identifier type for the alarm events.
 **/
typedef long cookie_t;

/* ------------------------------------------------------------------------- *
 * alarm_attr_t
 * ------------------------------------------------------------------------- */

/** @brief Extra alarm attributes
 *
 * Some applications need to transfer more data to system ui
 * alarm service than what basic alarm structure can convey.
 * The alarm attributes can be used for this purpose, the
 * alarm daemon itself does not use the attributes.
 *
 * The attributes have a name and value. Currently the following
 * types of attributes are supported:
 * - text strings : const char *
 * - integer numbers: int
 * - time values : time_t
 *
 * See also:
 * - #alarm_attr_create()
 * - #alarm_attr_delete()
 *
 * - #alarm_attr_set_null()
 * - #alarm_attr_set_string()
 * - #alarm_attr_set_int()
 * - #alarm_attr_set_time()
 *
 * - #alarm_attr_get_string()
 * - #alarm_attr_get_int()
 * - #alarm_attr_get_time()
 *
 * - #alarm_attr_ctor()
 * - #alarm_attr_dtor()
 * - #alarm_attr_delete_cb()
 */
typedef struct alarm_attr_t
{
  /** Attribute name, set by alarm_attr_create(). */
  char *attr_name;

  /** Attribute type, see #alarmattrtype. */
  int   attr_type;

  /** attribute data, access using the get/set methods */
  union
  {
    char  *sval;
    time_t tval;
    int    ival;
  } attr_data;
} alarm_attr_t;

/** @brief supported types for alarm_attr_t objects */
typedef enum alarmattrtype
{
  ALARM_ATTR_NULL,
  ALARM_ATTR_INT,
  ALARM_ATTR_TIME,
  ALARM_ATTR_STRING,
} alarmattrtype;

/** @name alarm_attr_t methods
 */
/*@{*/

/** @brief alarm attr constructor
 *
 *  Initializes alarm_attr_t object to sane values
 *
 *  @since v1.0.4
 *
 *  @param self : alarm_attr_t pointer
 */
void          alarm_attr_ctor      (alarm_attr_t *self);

/** @brief alarm attr destructor
 *
 *  Releases any dynamic resources bound to an alarm_attr_t object
 *
 *  @since v1.0.4
 *
 *  @param self : alarm_attr_t pointer
 */
void          alarm_attr_dtor      (alarm_attr_t *self);

/** @brief sets alarm_attr_t object to state that does not have a value
 *
 *  Any value related dynamic resources are released
 *
 *  @since v1.0.4
 *
 *  @param self : alarm_attr_t pointer
 */
void          alarm_attr_set_null  (alarm_attr_t *self);

/** @brief sets alarm_attr_t object to contain a string value
 *
 *  Type of alarm_attr_t is set to string and copy of the
 *  value is bound to the alarm_attr_t object.
 *
 *  @since v1.0.4
 *
 *  @param self : alarm_attr_t pointer
 *  @param val  : string to set as attr value
 */
void          alarm_attr_set_string(alarm_attr_t *self, const char *val);

/** @brief sets alarm_attr_t object to contain a integer value
 *
 *  Type of alarm_attr_t is set to integer and value
 *  is set to given value.
 *
 *  @since v1.0.4
 *
 *  @param self : alarm_attr_t pointer
 *  @param val  : integer to set as attr value
 */
void          alarm_attr_set_int   (alarm_attr_t *self, int val);

/** @brief sets alarm_attr_t object to contain a time_t value
 *
 *  Type of alarm_attr_t is set to time and value
 *  is set to given value.
 *
 *  @since v1.0.4
 *
 *  @param self : alarm_attr_t pointer
 *  @param val  : time_t to set as attr value
 */
void          alarm_attr_set_time  (alarm_attr_t *self, time_t val);

/** @brief gets string value of an alarm_attr_t
 *
 *  Returns the value held in attr or NULL if the type
 *  of the attr is not string.
 *
 *  @since v1.0.4
 *
 *  @param   self : alarm_attr_t pointer
 *  @returns string value, or NULL if attr type is not string
 */
const char   *alarm_attr_get_string(alarm_attr_t *self);

/** @brief gets integer value of an alarm_attr_t
 *
 *  Returns the value held in attr. If the type of the
 *  attr is not integer, an attempt is made to convert
 *  other types to integer value.
 *
 *  @since v1.0.4
 *
 *  @param   self : alarm_attr_t pointer
 *  @returns integer value, or -1 if conversion is not possible
 */
int           alarm_attr_get_int   (alarm_attr_t *self);

/** @brief gets time_t value of an alarm_attr_t
 *
 *  Returns the value held in attr. If the type of the
 *  attr is not time_t, an attempt is made to convert
 *  other types to time_t value.
 *
 *  @since v1.0.4
 *
 *  @param   self : alarm_attr_t pointer
 *  @returns integer value, or -1 if conversion is not possible
 */
time_t        alarm_attr_get_time  (alarm_attr_t *self);

/** @brief create a new alarm_attr_t object with name
 *
 *  Allocates memory from heap and initializes the
 *  attribute to NULL value.
 *
 *  @since v1.0.4
 *
 *  @param   name : name of the attr as string
 *  @returns alarm_attr_t pointer
 */
alarm_attr_t *alarm_attr_create    (const char *name);

/** @brief delete an alarm_attr_t object
 *
 *  Releases all dynamic resources bound to the
 *  attr object and the object itself.
 *
 *  Passing NULL object is permitted.
 *
 *  @since v1.0.4
 *
 *  @param   self : alarm_attr_t pointer
 */
void          alarm_attr_delete    (alarm_attr_t *self);

/** @brief normalized delete callback
 *
 *  Alias for alarm_attr_delete for use as callback.
 *
 *  @since v1.0.4
 *
 *  @param   self : alarm_attr_t pointer
 */
void          alarm_attr_delete_cb (void *self);

/*@}*/

/** @brief Options for alarm_action_t items.
 *
 * Bits that define when action is to be taken:
 * <ul>
 * <li> #ALARM_ACTION_WHEN_QUEUED
 * <li> #ALARM_ACTION_WHEN_DELAYED
 * <li> #ALARM_ACTION_WHEN_TRIGGERED
 * <li> #ALARM_ACTION_WHEN_DISABLED
 * <li> #ALARM_ACTION_WHEN_RESPONDED
 * <li> #ALARM_ACTION_WHEN_DELETED
 * </ul>
 *
 * Normally one would use WHEN_RESPONDED for dialog buttons,
 * and WHEN_TRIGGERED for non-interactive alarms. The other
 * WHEN bits can be used for debugging or tracing alarm
 * state changes. For example if application UI sets up
 * recurring alarm and wants to know when the trigger time
 * is changed, a dbus call on WHEN_QUEUED can be requested.
 *
 * Note that #ALARM_ACTION_WHEN_TRIGGERED is effective
 * only if the action label is also non-null, non-empty
 * string (see #alarm_action_is_button).
 *
 * The #ALARM_ACTION_WHEN_DELAYED bit can be used to get
 * notifications if alarm gets handled beyond the orginal
 * trigger time due to power outage for example.
 *
 * Bits that define what is to be done:
 * <ul>
 * <li> #ALARM_ACTION_TYPE_NOP
 * <li> #ALARM_ACTION_TYPE_SNOOZE
 * <li> #ALARM_ACTION_TYPE_DBUS
 * <li> #ALARM_ACTION_TYPE_EXEC
 * </ul>
 *
 * More than one ACTION_TYPE bit can be set, all will be
 * acted on.
 *
 * Bits used for fine tuning D-Bus actions:
 * <ul>
 * <li> #ALARM_ACTION_DBUS_USE_ACTIVATION
 * <li> #ALARM_ACTION_DBUS_USE_SYSTEMBUS
 * <li> #ALARM_ACTION_DBUS_ADD_COOKIE
 * </ul>
 *
 * Bits used for fine tuning Exec actions:
 * <ul>
 * <li> #ALARM_ACTION_EXEC_ADD_COOKIE
 * </ul>
 */
typedef enum alarmactionflags
{
  // 0                   1                   2                   3
  // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // x x x x x x x x x x x x x x x x

  // ---- WHAT ----

  /** Action is: Waiting for the user is enough (default)*/
  ALARM_ACTION_TYPE_NOP       = 0,

  /** Action is: Snooze the alarm
   *
   *  The trigger time is adjusted by custom snooze time
   *  (if nonzero) or the default snooze time, and the
   *  alarm is re-inserted into the alarm queue.
   *
   *  Note: any actions marked as #ALARM_ACTION_WHEN_QUEUED
   *        will be executed again.
   */
  ALARM_ACTION_TYPE_SNOOZE    = 1 << 0,

  /** Action is: Make a dbus method call / send a signal
   *
   *  See also:
   *  <ul>
   *  <li>#ALARM_ACTION_DBUS_USE_ACTIVATION
   *  <li>#ALARM_ACTION_DBUS_USE_SYSTEMBUS
   *  <li>#ALARM_ACTION_DBUS_ADD_COOKIE
   *  </ul>
   */
  ALARM_ACTION_TYPE_DBUS      = 1 << 1,

  /** Action is: Execute a command
   *
   *  Alarm event cookie can be added to the command to be
   *  executed using #ALARM_ACTION_EXEC_ADD_COOKIE.
   */
  ALARM_ACTION_TYPE_EXEC      = 1 << 2,

#if ALARMD_ACTION_BOOTFLAGS
  /** Action is: power up to desktop */
  ALARM_ACTION_TYPE_DESKTOP   = 1 << 3,

  /** Action is: power up to actdead */
  ALARM_ACTION_TYPE_ACTDEAD   = 1 << 8,
#endif

  /** Action is: disable the alarm. Note that disabling an alarm
   *  with this action will not trigger #ALARM_ACTION_WHEN_DISABLED
   *  type actions.
   */
  ALARM_ACTION_TYPE_DISABLE   = 1 << 10,

  // ---- WHEN ----

  /** Act when: the alarm is added in the queue. This will happen when
   * new alarms are added to the queue, or the trigger time of existing
   * alarms is changed due to: snooze, recurrence or timezone changes.
   */
  ALARM_ACTION_WHEN_QUEUED    = 1 << 4,

  /** Act when: the alarm trigger time is missed. Note that this
   *  does not change how the delayed control bits behave.
   *
   * @since v1.0.9
   *
   * See also: #ALARM_EVENT_RUN_DELAYED, #ALARM_EVENT_POSTPONE_DELAYED,
   * #ALARM_EVENT_DISABLE_DELAYED
   */
  ALARM_ACTION_WHEN_DELAYED   = 1 << 11,

  /** Act when: the alarm is triggered. The action will be done before setting
   * up system ui dialog.
   */
  ALARM_ACTION_WHEN_TRIGGERED = 1 << 5,

  /** Act when: the alarm is auto-disbled. This will occur if the
   *  trigger time is missed for example due to power outage and the
   *  alarm event itself has #ALARM_EVENT_DISABLE_DELAYED flag set.
   *
   *  Specifically the auto-disable actions will not be triggered
   *  if client application directly manipulates #ALARM_EVENT_DISABLED
   *  bit (via alarmd_event_update() for example), or if the event
   *  is disable due to user pressing a button with
   *  #ALARM_ACTION_TYPE_DISABLE bit set.
   */
  ALARM_ACTION_WHEN_DISABLED  = 1 << 9,

  /** Act when: user has clicked a button on system ui alarm dialog
   */
  ALARM_ACTION_WHEN_RESPONDED = 1 << 6,

  /** Act when: the alarm is about to be deleted from the event queue
   */
  ALARM_ACTION_WHEN_DELETED   = 1 << 7,

  // ---- DBUS ----

  /** Calls dbus_message_set_auto_start() before sending the message
   */
  ALARM_ACTION_DBUS_USE_ACTIVATION = 1 << 12,

  /** Sends message to systembus instead of session bus
   */
  ALARM_ACTION_DBUS_USE_SYSTEMBUS  = 1 << 13,

  /** Appends alarm cookie as INT32 after user supplied dbus parameters
   */
  ALARM_ACTION_DBUS_ADD_COOKIE     = 1 << 14,

  // ---- EXEC ----

  /** Adds alarm cookie to command line before execution.
   *
   *  @since v1.0.12
   *
   *  If the command contains string "[COOKIE]", that will
   *  be replaced by the cookie number.
   *
   *  "command -c[COOKIE] -Xfoo" -> "command -c1234 -Xfoo"
   *
   *  Otherwise the command string will be appended by one
   *  space and the cookie number.
   *
   *  "command" -> "command 1234"
   */
  ALARM_ACTION_EXEC_ADD_COOKIE     = 1 << 15,

  // ---- MASKS ----

  ALARM_ACTION_TYPE_MASK = (ALARM_ACTION_TYPE_SNOOZE |
                            ALARM_ACTION_TYPE_DISABLE|
                            ALARM_ACTION_TYPE_DBUS   |
                            ALARM_ACTION_TYPE_EXEC   |
#if ALARMD_ACTION_BOOTFLAGS
                            ALARM_ACTION_TYPE_DESKTOP|
                            ALARM_ACTION_TYPE_ACTDEAD|
#endif
                            0),

  ALARM_ACTION_WHEN_MASK = (ALARM_ACTION_WHEN_QUEUED    |
                            ALARM_ACTION_WHEN_DELAYED   |
                            ALARM_ACTION_WHEN_TRIGGERED |
                            ALARM_ACTION_WHEN_DISABLED  |
                            ALARM_ACTION_WHEN_RESPONDED |
                            ALARM_ACTION_WHEN_DELETED),

} alarmactionflags;

/** @brief When and whether to send D-Bus messages, Execute commands or Snooze
 *
 * Describes one action related to alarm_event_t
 *
 * All dynamically allocated members have set/get methods.
 * If these are used, alarm_action_delete and alarm_action_dtor
 * will release all dynamically allocated memory related to
 * alarm_action_t.
 *
 * See also:
 * - #alarm_action_create()
 * - #alarm_action_delete()
 *
 * - #alarm_action_get_label()
 * - #alarm_action_set_label()
 *
 * - #alarm_action_get_exec_command()
 * - #alarm_action_set_exec_command()
 *
 * - #alarm_action_get_dbus_service()
 * - #alarm_action_set_dbus_service()
 *
 * - #alarm_action_get_dbus_interface()
 * - #alarm_action_set_dbus_interface()
 *
 * - #alarm_action_get_dbus_path()
 * - #alarm_action_set_dbus_path()
 *
 * - #alarm_action_get_dbus_name()
 * - #alarm_action_set_dbus_name()
 *
 * - #alarm_action_del_dbus_args()
 * - #alarm_action_set_dbus_args_valist()
 * - #alarm_action_set_dbus_args()
 *
 * - #alarm_action_is_button()
 *
 * - #alarm_action_ctor()
 * - #alarm_action_dtor()
 * - #alarm_action_delete_cb()
 */

typedef struct alarm_action_t
{
  /** Bitwise or of ALARM_ACTION_xxx flags, see #alarmactionflags */
  unsigned  flags;

  /** Button name to be used in system ui dialog, or
   * NULL = not shown on dialog.
   * <br>Note: apart from non-null, non-empty label
   * string also ALARM_ACTION_WHEN_TRIGGERED bit
   * needs to be set in flags for the button to
   * be shown on dialog.
   *
   * See also:
   * - #alarm_action_get_label()
   * - #alarm_action_set_label()
   * - #alarm_action_is_button()
   */
  char     *label;

  /** Exec callback: command line to execute
   *
   * See also:
   * - #alarm_action_get_exec_command()
   * - #alarm_action_set_exec_command()
   */
  char     *exec_command;

  /** DBus callback: interface
   *
   * See also:
   * - #alarm_action_get_dbus_interface()
   * - #alarm_action_set_dbus_interface()
   */
  char     *dbus_interface;

  /** DBus callback: D-Bus name for service (method call), or
   * <br> NULL = send a signal
   *
   * See also:
   * - #alarm_action_get_dbus_service()
   * - #alarm_action_set_dbus_service()
   */
  char     *dbus_service;

  /** DBus callback: D-Bus object path
   *
   * See also:
   * - #alarm_action_get_dbus_path()
   * - #alarm_action_set_dbus_path()
   */
  char     *dbus_path;

  /** DBus callback: D-Bus member name (name of method or signal)
   *
   * See also:
   * - #alarm_action_get_dbus_name()
   * - #alarm_action_set_dbus_name()
   */
  char     *dbus_name;

  /**
   * DBus callback: serialized dbus arguments
   *
   * Note: the serialization protocol is considered
   * private to libalarm, use alarm_action_set_dbus_args()
   * to set up this field.
   *
   * See also:
   * - #alarm_action_del_dbus_args()
   * - #alarm_action_set_dbus_args()
   * - #alarm_action_set_dbus_args_valist()
   */
  char     *dbus_args;

} alarm_action_t;

/* ------------------------------------------------------------------------- *
 * alarm_action_t
 * ------------------------------------------------------------------------- */

/** @name alarm_action_t methods
 */
/*@{*/

/** \brief Constructor */
void            alarm_action_ctor     (alarm_action_t *self);
/** \brief Destructor */
void            alarm_action_dtor     (alarm_action_t *self);
/** \brief New operator */
alarm_action_t *alarm_action_create   (void);
/** \brief Delete operator */
void            alarm_action_delete   (alarm_action_t *self);
/** \brief Normalized delete callback*/
void            alarm_action_delete_cb(void *self);

/** \brief Check if action is button for system ui dialog
 *
 * Action is a button if:
 * - it has non empty label (see #alarm_action_set_label)
 * - #ALARM_ACTION_WHEN_RESPONDED is set in flags
 *
 * @param self : alarm_action_t pointer
 * @returns 0=not a button, 1=is a button
 */
int alarm_action_is_button(const alarm_action_t *self);

/** \brief Add dbus arguments to alarm_action_t
 *
 * Duplicates behavior of dbus_message_append_args(), but
 * encodes to asciz string suitable for storage at alarm
 * daemon end.
 *
 * @param self : alarm_action_t pointer
 * @param type : dbus type of first argument
 * @param ...  : terminated with DBUS_TYPE_INVALID
 *
 * @returns error : 0=success, -1=errors
 */
int             alarm_action_set_dbus_args       (alarm_action_t *self, int type, ...);

/** \brief Removes dbus arguments from alarm_action_t
 *
 * @param self : alarm_action_t pointer
 */
void            alarm_action_del_dbus_args       (alarm_action_t *self);

/** \brief Add dbus arguments to alarm_action_t
 *
 * A va_list version of alarm_action_set_dbus_args()
 *
 * @param self : alarm_action_t pointer
 * @param type : dbus type of first argument
 * @param va   : variable argument list obtained via va_start()
 *
 * @returns error : 0=success, -1=errors
 */
int             alarm_action_set_dbus_args_valist(alarm_action_t *self, int type, va_list va);

/** \brief Gets label string from alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @returns label, or empty string if not set
 */
const char     *alarm_action_get_label           (const alarm_action_t *self);

/** \brief Gets exec_command string from alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @returns exec_command, or empty string if not set
 */
const char     *alarm_action_get_exec_command    (const alarm_action_t *self);

/** \brief Gets dbus_interface string from alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @returns dbus_interface, or empty string if not set
 */
const char     *alarm_action_get_dbus_interface  (const alarm_action_t *self);

/** \brief Gets dbus_service string from alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @returns dbus_service, or empty string if not set
 */
const char     *alarm_action_get_dbus_service    (const alarm_action_t *self);

/** \brief Gets dbus_path string from alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @returns dbus_path, or empty string if not set
 */
const char     *alarm_action_get_dbus_path       (const alarm_action_t *self);

/** \brief Gets dbus_name string from alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @returns dbus_name, or empty string if not set
 */
const char     *alarm_action_get_dbus_name       (const alarm_action_t *self);

/** \brief Sets label string in alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @param label : string, or NULL to unset
 */
void            alarm_action_set_label           (alarm_action_t *self, const char *label);

/** \brief Sets exec_command string in alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @param exec_command : string, or NULL to unset
 */
void            alarm_action_set_exec_command    (alarm_action_t *self, const char *exec_command);

/** \brief Sets dbus_interface string in alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @param dbus_interface : string, or NULL to unset
 */
void            alarm_action_set_dbus_interface  (alarm_action_t *self, const char *dbus_interface);

/** \brief Sets dbus_service string in alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @param dbus_service : string, or NULL to unset
 */
void            alarm_action_set_dbus_service    (alarm_action_t *self, const char *dbus_service);

/** \brief Sets dbus_path string in alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @param dbus_path : string, or NULL to unset
 */
void            alarm_action_set_dbus_path       (alarm_action_t *self, const char *dbus_path);

/** \brief Sets dbus_name string in alarm_action_t
 *
 * @param self : alarm_action_t pointer
 * @param dbus_name : string, or NULL to unset
 */
void            alarm_action_set_dbus_name       (alarm_action_t *self, const char *dbus_name);

/*@}*/

/** @brief Date and time based recurrence defination
 *
 * Recurrence time can be specified by masks for:
 * - hours
 * - minutes
 *
 * Recurrence date can be specified by masks for:
 * - month
 * - day of the month
 * - day of the week
 *
 * Additionally the following special cases are
 * supported:
 * - biweekly
 * - monthly
 * - yearly.
 *
 * Note: the bit masks are struct tm compatible,
 * that is
 *
 * - mask_xxx |= 1u << tm->tm_xxx;
 *
 * yields expected result.
 *
 * Setting a bit in a mask allows the value
 * to be used as a recurrence point.
 *
 * See also:
 * - #alarmrecurflags
 *
 * - #alarm_recur_create()
 * - #alarm_recur_delete()
 *
 * - #alarm_recur_align()
 * - #alarm_recur_next()
 *
 * - #alarm_recur_init_from_tm()
 *
 * - #alarm_recur_ctor()
 * - #alarm_recur_dtor()
 * - #alarm_recur_delete_cb()
 */
typedef struct alarm_recur_t
{
  // NOTE: bit indexing is struct tm compatible

  /** Bitmask for allowed minute values.
   *
   *  Range: (1ull << 0) ... (1ull << 59).
   *
   *  See #alarmrecurflags, ALARM_RECUR_MIN_xxx
   *
   *  Note that this is a 64 bit value.
   */
  uint64_t mask_min;    // 0 .. 59

  /** Bitmask for allowed hour values.
   *
   *  Range: (1ul << 0) ... (1ul << 23)
   *
   *  See #alarmrecurflags, ALARM_RECUR_HOUR_xxx
   */
  uint32_t mask_hour;   // 0 .. 23

  /** Bitmask for allowed day of month values.
   *
   *  Range: (1ul << 1) ... (1ul << 31)
   *
   *  See #alarmrecurflags, ALARM_RECUR_MDAY_xxx
   */
  uint32_t mask_mday;   // 1 .. 31   1=first

  /** Bitmask for allowed day of week values.
   *
   *  Range: (1ul << 0) ... (1ul << 6) for Sunday ... Monday.
   *
   *  See #alarmrecurflags, ALARM_RECUR_WDAY_xxx
   */
  uint32_t mask_wday;   // 0 .. 6    0=sunday
  /** Bitmask for allowed month values.
   *
   *  Range: (1ul << 0) ... (1ul << 11) for January ... December.
   *
   *  See #alarmrecurflags, ALARM_RECUR_MON_xxx
   */
  uint32_t mask_mon;    // 0 .. 11   0=january

  /** Code for special cases.
   *
   *  See #alarmrecurflags, ALARM_RECUR_SPECIAL_xxx
   *
   *  Note: The special rules are applied first to advance
   *        from the previous trigger time. Then the masks
   *        are used to adjust the result.
   */
  uint32_t special;

} alarm_recur_t;

/** @brief Constants for use with alarm_recur_t items
 *
 * Constants for setting up alarm recurrence masks and
 * special recurres
 */

typedef enum alarmrecurflags
{
  /** Do not adjust minute value */
  ALARM_RECUR_MIN_DONTCARE  = 0,
  /** Allow any minute value */
  ALARM_RECUR_MIN_ALL       = (1ull << 60) - 1,

  /** Do not adjust hour value */
  ALARM_RECUR_HOUR_DONTCARE = 0,
  /** Allow any hour value */
  ALARM_RECUR_HOUR_ALL      = (1u << 24) - 1,

  /** Do not adjust day of month value */
  ALARM_RECUR_MDAY_DONTCARE = 0,
  /** Allow any day of month value */
  ALARM_RECUR_MDAY_ALL      = ~1u,
  /** Allow last day of month value,
   *  regardless of number of days in month */
  ALARM_RECUR_MDAY_EOM      =  1u,

  /** Do not adjust day of week value */
  ALARM_RECUR_WDAY_DONTCARE = 0,
  /** Allow any day of week value */
  ALARM_RECUR_WDAY_ALL      = (1u << 7) - 1,
  /** Allow Sundays */
  ALARM_RECUR_WDAY_SUN      = (1u << 0),
  /** Allow Mondays */
  ALARM_RECUR_WDAY_MON      = (1u << 1),
  /** Allow Tuesdays */
  ALARM_RECUR_WDAY_TUE      = (1u << 2),
  /** Allow Wednesdays */
  ALARM_RECUR_WDAY_WED      = (1u << 3),
  /** Allow Thursdays */
  ALARM_RECUR_WDAY_THU      = (1u << 4),
  /** Allow Fridays */
  ALARM_RECUR_WDAY_FRI      = (1u << 5),
  /** Allow Saturdays */
  ALARM_RECUR_WDAY_SAT      = (1u << 6),
  /** Allow Mon,Tue,Wed,Thu and Fri */
  ALARM_RECUR_WDAY_MONFRI   = ALARM_RECUR_WDAY_SAT - ALARM_RECUR_WDAY_MON,
  /** Allow Saturday and Sunday */
  ALARM_RECUR_WDAY_SATSUN   = ALARM_RECUR_WDAY_SAT | ALARM_RECUR_WDAY_SUN,

  /** Do not adjust month value */
  ALARM_RECUR_MON_DONTCARE = 0,
  /** Allow any month value */
  ALARM_RECUR_MON_ALL       = (1u << 12) - 1,
  /** Allow January */
  ALARM_RECUR_MON_JAN       = (1u <<  0),
  /** Allow Fenruary */
  ALARM_RECUR_MON_FEB       = (1u <<  1),
  /** Allow March */
  ALARM_RECUR_MON_MAR       = (1u <<  2),
  /** Allow April */
  ALARM_RECUR_MON_APR       = (1u <<  3),
  /** Allow May */
  ALARM_RECUR_MON_MAY       = (1u <<  4),
  /** Allow June */
  ALARM_RECUR_MON_JUN       = (1u <<  5),
  /** Allow July */
  ALARM_RECUR_MON_JUL       = (1u <<  6),
  /** Allow August */
  ALARM_RECUR_MON_AUG       = (1u <<  7),
  /** Allow September */
  ALARM_RECUR_MON_SEP       = (1u <<  8),
  /** Allow October */
  ALARM_RECUR_MON_OCT       = (1u <<  9),
  /** Allow November */
  ALARM_RECUR_MON_NOW       = (1u << 10),
  /** Allow December */
  ALARM_RECUR_MON_DEC       = (1u << 11),

  /** No special rule */
  ALARM_RECUR_SPECIAL_NONE      = 0,
  /** Special rule: every 14 days */
  ALARM_RECUR_SPECIAL_BIWEEKLY  = 1,
  /** Special rule: every month */
  ALARM_RECUR_SPECIAL_MONTHLY   = 2,
  /** Special rule: every year */
  ALARM_RECUR_SPECIAL_YEARLY    = 3,
} alarmrecurflags;

/** @name alarm_recur_t methods
 */
/*@{*/

/** \brief Constructor
 *
 *  @param self : alarm_recur_t pointer
 */
void           alarm_recur_ctor           (alarm_recur_t *self);

/** \brief Destructor
 *
 *  @param self : alarm_recur_t pointer
 */
void           alarm_recur_dtor           (alarm_recur_t *self);

/** \brief Create new initialized alarm_recur_t object
 *
 *  @returns pointer to initialized alarm_recur_t object
 */
alarm_recur_t *alarm_recur_create         (void);

/** \brief Cleanp and delete alarm_recur_t object
 *
 *  @param self : alarm_recur_t pointer
 */
void           alarm_recur_delete         (alarm_recur_t *self);

/** \brief Setup alarm_recur_t object from struct tm data
 *
 *  @param self : alarm_recur_t pointer
 *  @param tm : broken down time as struct tm
 */
void           alarm_recur_init_from_tm   (alarm_recur_t *self, const struct tm *tm);

/** \brief Normalized delete callback
 *
 * @param self : alarm recurrence object to free
 **/
void           alarm_recur_delete_cb      (void *self);

/** \brief Align broken down time to recurrence rules, try not to advance time
 *
 * Special rules are ignored.
 *
 * @param self       : alarm recurrence object to free
 * @param trg        : current trigger time as struct tm
 * @param tz         : timezone to use for evaluation
 **/
time_t         alarm_recur_align          (const alarm_recur_t *self, struct tm *trg, const char *tz);

/** \brief Align broken down time to recurrence rules, always advance time
 *
 * Special rules are applied before aligning to time / date masks.
 *
 * @param self       : alarm recurrence object to free
 * @param trg        : current trigger time as struct tm
 * @param tz         : timezone to use for evaluation
 **/
time_t         alarm_recur_next           (const alarm_recur_t *self, struct tm *trg, const char *tz);

/*@}*/

/** @brief Options for alarm_event_t items
 *
 * Describes alarm event. These should be bitwise orred to the flags field in
 * #alarm_event_t
 **/

typedef enum alarmeventflags
{
  // 0                   1                   2                   3
  // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // - - - x x x x x - x x x x - - - R R R R R R R R R R R R R R R R

  /** The event should boot up the system, if powered off.
   **/
  ALARM_EVENT_BOOT             = 1 << 3,

  /** The device should only be powered to acting dead mode.
   **/
  ALARM_EVENT_ACTDEAD          = 1 << 4,

  /** Icon is shown in the statusbar when alarms with this flag set
   *  are in alarmd queue waiting to be triggered.
   **/
  ALARM_EVENT_SHOW_ICON        = 1 << 5,

  /** Run only when internet connection is available.
   *
   * See also: #ALARM_EVENT_RUN_DELAYED, #ALARM_EVENT_POSTPONE_DELAYED.
   **/
  ALARM_EVENT_CONNECTED        = 1 << 7,

  /** If the trigger time is missed (due to the device being off or
   *  system time being adjusted beyond the trigger point) the
   *  alarm should be run anyway.
   *
   *  See also: #ALARM_EVENT_POSTPONE_DELAYED.
   **/
  ALARM_EVENT_RUN_DELAYED      = 1 << 6,

  /** If the trigger time is missed (due to the device being off or
   *  system time being adjusted beyond the trigger point) the
   *  alarm should be postponed for the next day.
   *
   *  See also: #ALARM_EVENT_RUN_DELAYED.
   **/
  ALARM_EVENT_POSTPONE_DELAYED = 1 << 9,

  /** If the trigger time is missed (due to the device being off or
   *  system time being adjusted beyond the trigger point) the
   *  alarm should be disabled, but not deleted.
   *
   *  See also: #ALARM_EVENT_RUN_DELAYED, #ALARM_EVENT_DISABLED,
   *  #ALARM_ACTION_WHEN_DISABLED, #ALARM_ACTION_TYPE_DISABLE.
   **/
  ALARM_EVENT_DISABLE_DELAYED  = 1 << 12,

  /** If the system time is moved backwards, the alarm should be
   *  rescheduled.
   *
   *  Note: this only applies to recurring alarms.
   **/
  ALARM_EVENT_BACK_RESCHEDULE  = 1 << 10,

  /** Alarm is temporarily disabled, ignored by alarmd
   *
   * See also: #ALARM_EVENT_DISABLE_DELAYED
   **/
  ALARM_EVENT_DISABLED         = 1 << 11,

  /** Higher bits are internal to alarmd and ignored when
   *  transferred over the dbus from client side.
   **/
  ALARM_EVENT_CLIENT_BITS      = 16,

  /** Mask: bits accessable both to server and client.
   **/
  ALARM_EVENT_CLIENT_MASK      = (1 << ALARM_EVENT_CLIENT_BITS) - 1,

} alarmeventflags;

/** \brief Calculate value for alarm_event::recur_secs specified in seconds */
#define ALARM_RECURRING_SECONDS(n) (n)
/** \brief Calculate value for alarm_event::recur_secs specified in minutes */
#define ALARM_RECURRING_MINUTES(n) ((n)*60)
/** \brief Calculate value for alarm_event::recur_secs specified in hours */
#define ALARM_RECURRING_HOURS(n)   ((n)*60*60)

/** \brief @brief Alarm time control + actions to take
 *
 * Describes whem alarm should occur, how to handle
 * snoozing and recurrence, and actions to be taken on
 * various trigger points.
 *
 * Also contains information not used directly by alarmd,
 * like dialog title/message strings and paths to custom
 * icons and sounds.
 *
 * To ensure forward compatibility the alarm_event_t
 * objects should be allocated using libalarmd functions
 * like alarm_event_create() or alarmd_event_get().
 *
 * All dynamically allocated members have set/get methods.
 * If these are used, alarm_event_delete and alarm_event_dtor
 * will release all dynamically allocated memory related to
 * alarm_event_t.
 *
 * Related alarmd functions:
 * - #alarmd_event_add()
 * - #alarmd_event_get()
 * - #alarmd_event_del()
 * - #alarmd_event_query()
 * - #alarmd_event_update()
 * - #alarmd_event_add_valist()
 * - #alarmd_event_add_with_dbus_params()
 *
 *
 * See also:
 * - #alarm_event_create()
 * - #alarm_event_delete()
 *
 * - #alarm_event_get_alarm_appid()
 * - #alarm_event_set_alarm_appid()
 *
 * - #alarm_event_get_title()
 * - #alarm_event_set_title()
 *
 * - #alarm_event_get_message()
 * - #alarm_event_set_message()
 *
 * - #alarm_event_get_icon()
 * - #alarm_event_set_icon()
 *
 * - #alarm_event_get_sound()
 * - #alarm_event_set_sound()
 *
 * - #alarm_event_set_time()
 * - #alarm_event_get_time()
 *
 * - #alarm_event_get_alarm_tz()
 * - #alarm_event_set_alarm_tz()
 *
 * - #alarm_event_add_actions()
 * - #alarm_event_del_actions()
 * - #alarm_event_get_action()
 * - #alarm_event_set_action_dbus_args()
 * - #alarm_event_get_action_dbus_args()
 * - #alarm_event_del_action_dbus_args()
 *
 * - #alarm_event_get_recurrence()
 * - #alarm_event_del_recurrences()
 * - #alarm_event_add_recurrences()
 *
 * - #alarm_event_set_attr_int()
 * - #alarm_event_get_attr_int()
 * - #alarm_event_set_attr_string()
 * - #alarm_event_get_attr_string()
 * - #alarm_event_set_attr_time()
 * - #alarm_event_get_attr_time()
 * - #alarm_event_del_attrs()
 * - #alarm_event_rem_attr()
 * - #alarm_event_get_attr()
 * - #alarm_event_has_attr()
 * - #alarm_event_add_attr()
 *
 * - #alarm_event_is_recurring()
 * - #alarm_event_is_sane()
 *
 * - #alarm_event_ctor()
 * - #alarm_event_dtor()
 * - #alarm_event_delete_cb()
 * - #alarm_event_create_ex()
 */

typedef struct alarm_event_t
{
  /** Unique identifier, issued by alarmd.
   *
   * Ignored by #alarmd_event_add().
   *
   * Required by #alarmd_event_update().
   *
   * Available after #alarmd_event_get().
   *
   * Access via: #alarm_event_get_cookie(), #alarm_event_set_cookie()
   */
  cookie_t       ALARMD_PRIVATE(cookie);

  /** Trigger time, managed by alarmd.
   *
   * Available after #alarmd_event_get().
   *
   * Access via: #alarm_event_get_trigger(), #alarm_event_set_trigger()
   */
  time_t         ALARMD_PRIVATE(trigger);

  /** Title string for the alarm dialog.
   *
   * See also:
   * - #alarm_event_get_title()
   * - #alarm_event_set_title()
   */
  char          *title;

  /** Message string for the alarm dialog.
   *
   * See also:
   * - #alarm_event_get_message()
   * - #alarm_event_set_message()
   */
  char          *message;

  /** Custom sound to play when alarm dialog is shown.
   *
   * See also:
   * - #alarm_event_get_sound()
   * - #alarm_event_set_sound()
   */
  char          *sound;

  /** Custom icon to use when alarm dialog is shown.
   *
   * See also:
   * - #alarm_event_get_icon()
   * - #alarm_event_set_icon()
   */
  char          *icon;

  /** Event specific behaviour, bitwise or of #alarmeventflags */
  unsigned       flags;

  /** Name of the application this alarm belongs to.
   *
   * See also:
   * - #alarm_event_get_alarm_appid()
   * - #alarm_event_set_alarm_appid()
   */
  char          *alarm_appid;

  /** Seconds since epoch, or <br>
   *  -1 = use broken down time spec below.
   *
   * This specifies the alarm time in absolute terms
   * (seconds since epoch) and thus is not affected
   * by timezone changes.
   *
   * Note: This means that the interpretation of the alarm time
   * as time-of-day in localtime can change if the timezone is
   * changed. So use this only if you absolutely must have more
   * than one minute accuracy for alarms, or if you do not care
   * about the time-of-day when the alarm should be triggered. 
   * Otherwise use #alarm_tm instead.
   *
   * See also:
   * - #alarm_event_get_time()
   * - #alarm_tm
   * - #alarm_tx
   */
  time_t         alarm_time;

  /** Alarm time broken down to year month day and hour min min sec.
   *
   *
   * If #alarm_tz is left unset, the actual triggering time will
   * be re-evaluated when currently active timezone changes.
   *
   * Note: Alarm daemon will round up the actual alarm time to the
   * next full minute. If you absolutely need more than one minute
   * accuracy for alarms, use #alarm_time instead.
   *
   * See also:
   * - #alarm_event_get_time()
   * - #alarm_event_set_time()
   * - #alarm_tz
   * - #alarm_time
   */
  struct tm      alarm_tm;

  /** Timezone, or NULL for floating local time
   *
   * Note that alarm timezone is used only
   * when evaluating trigger time from
   * broken down time (#alarm_tm) or recurrence time
   * from recurrence masks.
   *
   * If timezone is left unset the trigger
   * times are re-evaluated on timezone change.
   *
   * See also:
   * - #alarm_event_get_alarm_tz()
   * - #alarm_event_set_alarm_tz()
   * - #alarm_tm
   */
  char          *alarm_tz;

  /** Number of seconds an alarm is postponed on snooze. or
   * <br> 0 = use server default snooze value.
   *
   * See also:
   * - #alarmd_set_default_snooze()
   * - #alarmd_get_default_snooze()
   */
  time_t         snooze_secs;

  /** How many seconds the event has been snoozed in total.
   *  Managed by alarmd. Used to align recurrencies with
   *  the original trigger time.
   */
  time_t         snooze_total;

  /** Number of items in the action_tab.
   *
   * See also:
   * - #alarm_event_add_actions()
   * - #alarm_event_del_actions()
   */
  size_t         action_cnt;

  /** Action descriptors.
   *
   * See also:
   * - #alarm_event_add_actions()
   * - #alarm_event_del_actions()
   */
  alarm_action_t *action_tab;

  /** Dialog response from user, managed by alarmd */
  int            response;

  /** Number of recurrences, or
   * <br> -1 = infinite.
   *
   * See also:
   */
  int             recur_count;

  /** Number of seconds between each recurrence,
   *  or 0 to use recurrence specifiers below */
  time_t          recur_secs;

  /** Number of items in the recurrence_tab.
   *
   * See also: #alarm_event_t::recurrence_cnt
   */
  size_t          recurrence_cnt;
  /** Recurrence specifiers.
   *
   * See also:
   * - #alarm_event_add_recurrences()
   * - #alarm_event_del_recurrences()
   * - #alarm_event_get_recurrence()
   */
  alarm_recur_t  *recurrence_tab;

  /** Number of items in the attr_tab.
   *
   * See also: #alarm_event_t::attr_cnt
   */
  size_t          attr_cnt;
  /** Array of event attributes.
   *
   * See also:
   * - #alarm_event_add_attr()
   * - #alarm_event_get_attr()
   * - #alarm_event_has_attr()
   * - #alarm_event_rem_attr()
   * - #alarm_event_get_attr_int()
   * - #alarm_event_set_attr_int()
   * - #alarm_event_get_attr_string()
   * - #alarm_event_set_attr_string()
   * - #alarm_event_get_attr_time()
   * - #alarm_event_set_attr_time()
   * - #alarm_event_del_attrs()
   */
  alarm_attr_t  **attr_tab;

  /* - - - - - - - - - - - - - - - - - - - *
   * changes to this structure -> check also:
   *
   * event.c:
   *   alarm_event_ctor()
   *   alarm_event_dtor()
   *
   * codec.c:
   *   encode_event()
   *   decode_event()
   *
   * queue.c:
   *   queue_save_to_memory()
   *   queue_load_from_path()
   *
   * - - - - - - - - - - - - - - - - - - - */

} alarm_event_t;

/* ------------------------------------------------------------------------- *
 * alarm_event_t
 * ------------------------------------------------------------------------- */

/** @name alarm_event_t methods
 */
/*@{*/

/** \brief Will free all memory associated with the alarm event
 *
 * The alarm_event_t should be obtained via:
 * - #alarm_event_create()
 * - #alarmd_event_get()
 *
 * @param self : alarm event to free
 **/
void alarm_event_delete(alarm_event_t *self);

/** \brief Create a new alarm event
 *
 * Allocates dynamic memory for alarm_event_t and initializes
 * all values to sane defaults using alarm_event_ctor().
 *
 * Use alarm_event_delete() to release the event structure.
 *
 * @returns event : alarm_event_t pointer
 */
alarm_event_t *alarm_event_create(void);

/** \brief Create a new alarm event with actions
 *
 * Use alarm_event_delete() to release the event structure.
 *
 * @returns event : alarm_event_t pointer
 */
alarm_event_t *alarm_event_create_ex(size_t actions);

/** \brief Normalized delete callback
 *
 * @param self : alarm event to free
 **/
void alarm_event_delete_cb(void *self);

/** \brief Constructor
 *
 * @param self : alarm_event_t pointer
 **/
void alarm_event_ctor(alarm_event_t *self);

/** \brief Destructor
 *
 * @param self : alarm_event_t pointer
 **/
void alarm_event_dtor(alarm_event_t *self);

/** \brief Get unique cookie
 *
 * @since v1.1.4
 *
 * @param self : alarm_event_t pointer
 *
 * @returns cookie : unique identifier for alarm event
 **/
cookie_t alarm_event_get_cookie(const alarm_event_t *self);

/** \brief Set unique cookie
 *
 * @since v1.1.4
 *
 * @param self : alarm_event_t pointer
 *
 * @param cookie : unique identifier for alarm event
 **/
void alarm_event_set_cookie(alarm_event_t *self, cookie_t cookie);

/** \brief Get alarm trigger time
 *
 * @since v1.1.4
 *
 * @param self : alarm_event_t pointer
 *
 * @returns trigger : triggering time of alarm event
 **/
time_t alarm_event_get_trigger(const alarm_event_t *self);

/** \brief Set alarm trigger time
 *
 * @since v1.1.4
 *
 * @param self : alarm_event_t pointer
 *
 * @param trigger : triggering time of alarm event
 **/
void alarm_event_set_trigger(alarm_event_t *self, time_t trigger);

/** \brief Gets title string from alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @returns title, or empty string if not set
 */
const char     *alarm_event_get_title           (const alarm_event_t *self);

/** \brief Sets title string in alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @param title : string, or NULL to unset
 */
void            alarm_event_set_title           (alarm_event_t *self, const char *title);

/** \brief Gets message string from alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @returns message, or empty string if not set
 */
const char     *alarm_event_get_message           (const alarm_event_t *self);

/** \brief Sets message string in alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @param message : string, or NULL to unset
 */
void            alarm_event_set_message           (alarm_event_t *self, const char *message);

/** \brief Gets sound string from alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @returns sound, or empty string if not set
 */
const char     *alarm_event_get_sound           (const alarm_event_t *self);

/** \brief Sets sound string in alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @param sound : string, or NULL to unset
 */
void            alarm_event_set_sound           (alarm_event_t *self, const char *sound);

/** \brief Gets icon string from alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @returns icon, or empty string if not set
 */
const char     *alarm_event_get_icon           (const alarm_event_t *self);

/** \brief Sets icon string in alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @param icon : string, or NULL to unset
 */
void            alarm_event_set_icon           (alarm_event_t *self, const char *icon);

/** \brief Gets alarm_appid string from alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @returns alarm_appid, or empty string if not set
 */
const char     *alarm_event_get_alarm_appid           (const alarm_event_t *self);

/** \brief Sets alarm_appid string in alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @param alarm_appid : string, or NULL to unset
 */
void            alarm_event_set_alarm_appid           (alarm_event_t *self, const char *alarm_appid);

/** \brief Gets alarm_tz string from alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @returns alarm_tz, or empty string if not set
 */
const char     *alarm_event_get_alarm_tz           (const alarm_event_t *self);

/** \brief Sets alarm_tz string in alarm_event_t
 *
 * @param self : alarm_event_t pointer
 * @param alarm_tz : string, or NULL to unset
 */
void            alarm_event_set_alarm_tz           (alarm_event_t *self, const char *alarm_tz);

/** \brief Check if alarm event is recurring
 *
 * Alarm event is recurring if:
 * - #alarm_event_t::recur_count != 0, and:
 *   - #alarm_event_t::recur_secs > 0, or
 *   - #alarm_event_t::recurrence_cnt > 0
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @returns non zero value if alarm event is recurring
 */
int             alarm_event_is_recurring(const alarm_event_t *self);

/** \brief Add & initialize actions to event
 *
 * Note: the pointer returned is valid until
 * next call to alarm_event_add_actions() or
 * alarm_event_del_actions().
 *
 * @param self : alarm_event_t pointer
 * @param count : number of actions to add
 * @returns pointer to initialized new actions
 */
alarm_action_t *alarm_event_add_actions(alarm_event_t *self, size_t count);

/** \brief Get pointer to alarm event action by index
 * @param self  : alarm_event_t pointer
 * @param index : index of alarm action
 * @returns pointer to alarm action or NULL
 */
alarm_action_t *alarm_event_get_action(const alarm_event_t *self, int index);

/** \brief Delete all actions from event and release all
 *  dynamic memory related to them.
 *
 * @param self : alarm_event_t pointer
 */
void alarm_event_del_actions(alarm_event_t *self);

/** \brief Helper method for manipulating alarm action dbus arguments
 *
 * Set dbus args for alarm event action.
 * See alarm_action_set_dbus_args() for reference.
 *
 * @param self  : alarm_event_t pointer
 * @param index : action index
 * @param type  : dbus type of first argument
 * @param ...   : dbus arguments
 *
 * @returns error : 0=success, -1=errors
 */
int alarm_event_set_action_dbus_args(const alarm_event_t *self, int index, int type, ...);

/** \brief Helper method for manipulating alarm action dbus arguments
 *
 * Get serialized dbus args for alarm event action.
 *
 * @param self  : alarm_event_t pointer
 * @param index : action index
 *
 * @returns serialized dbus args or NULL on error
 */
const char *alarm_event_get_action_dbus_args(const alarm_event_t *self, int index);

/** \brief Helper method for manipulating alarm action dbus arguments
 *
 * Delete serialized dbus args from alarm event action.
 *
 * @param self  : alarm_event_t pointer
 * @param index : action index
 */
void alarm_event_del_action_dbus_args(const alarm_event_t *self, int index);

/** \brief Add & initialize recurrence templates to event
 *
 * Note: the pointer returned is valid until
 * next call to alarm_event_add_recurrences() or
 * alarm_event_del_arecurrences().
 *
 * @param self : alarm_event_t pointer
 * @param count : number of recurrencies to add
 * @returns pointer to initialized new recurrencies
 */
alarm_recur_t *alarm_event_add_recurrences(alarm_event_t *self, size_t count);

/** \brief Get pointer to recurrence template in event
 *
 * @since v1.0.3
 *
 * Note: the pointer returned is valid until
 * next call to alarm_event_add_recurrences() or
 * alarm_event_del_arecurrences().
 *
 * @param self : alarm_event_t pointer
 * @param index : index of recurrence to get
 * @returns pointer to initialized new recurrencies
 */
alarm_recur_t *alarm_event_get_recurrence(const alarm_event_t *self, int index);

/** \brief Delete all recurrence templates from event
 *  and release all dynamic memory related to them.
 *
 * @param self : alarm_event_t pointer
 */
void alarm_event_del_recurrences(alarm_event_t *self);

/** \brief Set alarm time from broken down time
 *
 * Broken down time supplied will be used
 * to calculate trigger time.
 *
 * See also alarm_event_set_alarm_tz().
 *
 *
 * @param self : alarm_event_t pointer
 * @param tm : pointer to struct tm
 */
void alarm_event_set_time(alarm_event_t *self, const struct tm *tm);

/** \brief Get alarm time as broken down time
 *
 * If alarm event is configured to use absolute time
 * instead of broken down time, the absolute time
 * will be evaluated in currently active local timezone.
 *
 * @param self : alarm_event_t pointer
 * @param tm : pointer to struct tm
 */
void alarm_event_get_time(const alarm_event_t *self, struct tm *tm);

/** \brief Check that alarm event content makes sense.
 *
 * Goes through all the flags and fields in an alarm event
 * strucure, logs out things that do not make sense.
 *
 * Returns 1 if everything is ok, 0 if there were minor
 * issues and -1 if event configuration is broken enough
 * not to be usable.
 *
 * Note: adding and updating events that return -1 on this
 *       test will fail already on the client side, the
 *       events will not be sent over to alarmd.
 *
 * @since v1.0.3
 *
 * @param self : alarm_event_t pointer
 * @returns 1=ok, 0=warnings, -1=failure
 */
int alarm_event_is_sane(const alarm_event_t *self);

/** \brief Remove all attributes from alarm event object
 *
 * Removes all attributes from alarm event object and
 * releases all dynamic resources bound to them.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 */
void            alarm_event_del_attrs           (alarm_event_t *self);

/** \brief Remove named attribute from alarm event object
 *
 * Removes named attribute from alarm event object.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 */
void            alarm_event_rem_attr            (alarm_event_t *self, const char *name);

/** \brief Locate named attribute from alarm event object
 *
 * Locates named attribute from alarm event object.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 *
 * @returns alarm_attr_t pointer, or NULL if no attribute was found
 */
alarm_attr_t   *alarm_event_get_attr            (alarm_event_t *self, const char *name);

/** \brief Check if alarm event object has named attribute
 *
 * Check if alarm event object has named attribute.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 *
 * @returns 1 if attribute is present, or 0 if not
 */
int             alarm_event_has_attr            (alarm_event_t *self, const char *name);

/** \brief Adds named attribute to alarm event object
 *
 * Adds named attribute to alarm event object. If
 * attribute by given name already exists that will
 * be returned.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 *
 * @returns alarm_attr_t pointer
 */
alarm_attr_t   *alarm_event_add_attr            (alarm_event_t *self, const char *name);

/** \brief Adds named integer attribute to alarm event object
 *
 * If attribute with the same name already exists, it
 * will be converted to integer type with the given value.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 * @param val  : attribute value as integer
 */
void            alarm_event_set_attr_int        (alarm_event_t *self, const char *name, int val);

/** \brief Adds named time_t attribute to alarm event object
 *
 * If attribute with the same name already exists, it
 * will be converted to time_t type with the given value.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 * @param val  : attribute value as time_t
 */
void            alarm_event_set_attr_time       (alarm_event_t *self, const char *name, time_t val);

/** \brief Adds named string attribute to alarm event object
 *
 * If attribute with the same name already exists, it
 * will be converted to string type with the given value.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 * @param val  : attribute value as string
 */
void            alarm_event_set_attr_string     (alarm_event_t *self, const char *name, const char *val);

/** \brief Gets integer value of named attribute in alarm event object
 *
 * Gets integer value of named attribute in alarm event object.
 * If the attribute does not exist, the given default value is returned.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 * @param def  : value to return if attribute is not found
 *
 * @returns integer value or given default value
 */
int             alarm_event_get_attr_int        (alarm_event_t *self, const char *name, int def);

/** \brief Gets time_t value of named attribute in alarm event object
 *
 * Gets time_t value of named attribute in alarm event object.
 * If the attribute does not exist, the given default value is returned.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 * @param def  : value to return if attribute is not found
 *
 * @returns time_t value or given default value
 */
time_t          alarm_event_get_attr_time       (alarm_event_t *self, const char *name, time_t def);

/** \brief Gets string value of named attribute in alarm event object
 *
 * Gets string value of named attribute in alarm event object.
 * If the attribute does not exist, the given default value is returned.
 *
 * @since v1.0.4
 *
 * @param self : alarm_event_t pointer
 * @param name : attribute name
 * @param def  : value to return if attribute is not found
 *
 * @returns string value or given default value
 */
const char     *alarm_event_get_attr_string     (alarm_event_t *self, const char *name, const char *def);

/*@}*/

/* ------------------------------------------------------------------------- *
 * alarmd functions
 * ------------------------------------------------------------------------- */

/** @name alarmd functions for all clients
 */
/*@{*/

/** \brief Adds an event to the alarm queue.
 *
 * No dialog will be shown unless the event has at least one
 * action with non-empty label and  ALARM_ACTION_WHEN_RESPONDED in flags.
 *
 * Returns: Unique identifier for the alarm, or 0 on failure.
 *
 * @param event : alarm_event_t pointer
 *
 * @returns cookie : unique alarm event identifier
 **/
cookie_t alarmd_event_add(const alarm_event_t *event);

/** \brief Adds an event with dbus parameters to the alarm queue.
 *
 * Just like alarmd_event_add(), but with arguments for the D-Bus
 * method call / signal.
 *
 * The DBus arguments should be passed as pointers, like for
 * dbus_message_append_args(). List should be terminated with
 * DBUS_TYPE_INVALID.
 *
 * The DBus arguments will be used for the first action that
 * has null dbus_args and ALARM_ACTION_TYPE_DBUS set in flags.
 * If such action is not found, error is returned and event is
 * not sent to alarmd.
 *
 * Note, you need dbus/dbus-protocol.h for the type
 * macros.
 *
 * @param event : alarm_event_t pointer
 * @param type  : dbus type identifier of the the first element to add
 *
 * @returns cookie : unique alarm event identifier, or 0 = failure
**/
cookie_t alarmd_event_add_with_dbus_params(const alarm_event_t *event,
                                           int type, ...);

/** \brief Deletes alarm from the alarm queue.
 *
 * The alarm with the event_cookie identifier will be removed from the
 * alarm queue, if it exists.
 *
 * @param cookie : unique alarm event identifier
 *
 * @returns error : 0 = no error, -1 = error
 **/
int alarmd_event_del(cookie_t cookie);

/** \brief Updates an event already in the alarm queue.
 *
 * Uses only one dbus transaction, acts bit like doing:
 * <ol>
 * <li> alarmd_event_del(event->cookie);
 * <li> alarmd_event_add(event);
 * </ol>
 *
 * Notes:
 * <ul>
 * <li> Will cancel existing systemui dialog if the event was
 *      already triggered.
 * <li> Will add new event if the old cookie was already deleted.
 * <li> Different cookie will be returned regardless of the
 *      status of the possibly existing old event.
 * </ul>
 *
 * @param event : alarm_event_t pointer with nonzero event->cookie
 *
 * @returns cookie : unique alarm event identifier, or 0 on error
 **/
cookie_t alarmd_event_update(const alarm_event_t *event);

/** \brief Queries alarms in given time span.
 *
 * Finds every alarm whose _next_ occurence time is between first and last.
 *
 * Returns zero-terminated array of alarm cookies.
 *
 * Use free() to release the returned array.
 *
 * Special cases:
 * - first=0, last=0 -> match any time
 * - flag_mask=0, flags=0 -> match any flags
 *
 * So alarmd_event_query(0,0, 0,0, appid) will return cookies of
 * alarms for the given appid, and alarmd_event_query(0,0, 0,0, NULL)
 * will return all cookies for all alarms for all apps.
 *
 * @param first      : start of time span (inclusive)
 * @param last       : end of time span (inclusive)
 * @param flag_mask  : Mask describing which flags you're interested in.
 *                     Pass 0 to get all events.
 * @param flags      : Values for the flags you're querying.
 * @param appid      : Name of application, or NULL for all
 *
 * @returns cookies  : zero terminated array of cookies
 **/
cookie_t *alarmd_event_query(const time_t first, const time_t last,
                             int32_t flag_mask, int32_t flags,
                             const char *appid);

/** \brief Fetches alarm defails.
 *
 * Finds an alarm with given identifier and returns alarm_event_t struct
 * describing it.
 *
 * Use alarm_event_delete() to release the event structure.
 *
 * @param cookie : unique alarm event identifier
 *
 * @returns event : alarm_event_t pointer
 **/
alarm_event_t *alarmd_event_get(cookie_t cookie);

/** \brief Sets the amount events will be snoozed by default.
 *
 * Sets the amount events will be snoozed by default.
 *
 * Using value of zero will restore the builtin default
 * value (10 minutes).
 *
 * @param   snooze : preferred snooze time in seconds
 *
 * @returns error  : 0=success, -1=errors
 **/
int alarmd_set_default_snooze(unsigned int snooze);

/** \brief Gets the amount events will be snoozed by default.
 *
 * Gets the amount events will be snoozed by default.
 *
 * @returns snooze : preferred snooze time in seconds
 **/
unsigned int alarmd_get_default_snooze(void);

/** \brief va_list version of alarmd_event_add_with_dbus_params() */
cookie_t alarmd_event_add_valist(const alarm_event_t *event, int type, va_list va);

/*@}*/

/* ------------------------------------------------------------------------- *
 * systemui functions
 * ------------------------------------------------------------------------- */

/** @name Alarmd functions for System UI usage only
 */

/*@{*/

/** \brief SystemUI: acknowledge user action on dialog */
int alarmd_ack_dialog(cookie_t cookie, int button);

/** \brief SystemUI: acknowledge having queued alarm events for dialogs */
int alarmd_ack_queue(cookie_t *cookies, int count);

/*@}*/

/* ------------------------------------------------------------------------- *
 * debug functions
 * ------------------------------------------------------------------------- */

/** @name Alarmd functions for development usage only
 */

/*@{*/

/** \brief Debug state manipulation, undocumented */
int alarmd_set_debug(unsigned mask_set, unsigned mask_clr,
                     unsigned flag_set, unsigned flag_clr);

/*@}*/

#pragma GCC visibility pop

#ifdef __cplusplus
};
#endif

#endif /* LIBALARM_H_ */
