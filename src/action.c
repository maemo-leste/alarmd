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

#include "libalarm.h"
#include "xutil.h"
#include "serialize.h"

/* ------------------------------------------------------------------------- *
 * alarm_action_ctor
 * ------------------------------------------------------------------------- */

void
alarm_action_ctor(alarm_action_t *self)
{
  self->flags          = 0;
  self->label          = 0;

  self->exec_command   = 0;

  self->dbus_interface = 0;
  self->dbus_service   = 0;
  self->dbus_path      = 0;
  self->dbus_name      = 0;
  self->dbus_args      = 0;
}

/* ------------------------------------------------------------------------- *
 * alarm_action_dtor
 * ------------------------------------------------------------------------- */

void
alarm_action_dtor(alarm_action_t *self)
{
  free(self->label);

  free(self->exec_command);

  free(self->dbus_interface);
  free(self->dbus_service);
  free(self->dbus_path);
  free(self->dbus_name);
  free(self->dbus_args);
}

/* ------------------------------------------------------------------------- *
 * alarm_action_create
 * ------------------------------------------------------------------------- */

alarm_action_t *
alarm_action_create(void)
{
  alarm_action_t *self = calloc(1, sizeof *self);
  alarm_action_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * alarm_action_delete
 * ------------------------------------------------------------------------- */

void
alarm_action_delete(alarm_action_t *self)
{
  if( self != 0 )
  {
    alarm_action_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * alarm_action_delete_cb
 * ------------------------------------------------------------------------- */

void
alarm_action_delete_cb(void *self)
{
  alarm_action_delete(self);
}

/* ------------------------------------------------------------------------- *
 * alarm_action_del_dbus_args
 * ------------------------------------------------------------------------- */

void
alarm_action_del_dbus_args(alarm_action_t *self)
{
  free(self->dbus_args);
  self->dbus_args = 0;
}

/* ------------------------------------------------------------------------- *
 * alarm_action_set_dbus_args_valist
 * ------------------------------------------------------------------------- */

int
alarm_action_set_dbus_args_valist(alarm_action_t *self, int type, va_list va)
{
  alarm_action_del_dbus_args(self);
  self->dbus_args = serialize_pack_dbus_args(type, va);
  return (self->dbus_args == 0) ? -1 : 0;
}

/* ------------------------------------------------------------------------- *
 * alarm_action_set_dbus_args
 * ------------------------------------------------------------------------- */

int
alarm_action_set_dbus_args(alarm_action_t *self, int type, ...)
{
  int err = -1;
  va_list va;

  va_start(va, type);
  err = alarm_action_set_dbus_args_valist(self, type, va);
  va_end(va);

  return err;
}

/* ------------------------------------------------------------------------- *
 * alarm_action_get_label
 * ------------------------------------------------------------------------- */

const char *
alarm_action_get_label(const alarm_action_t *self)
{
  return self->label ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_action_get_exec_command
 * ------------------------------------------------------------------------- */

const char *
alarm_action_get_exec_command(const alarm_action_t *self)
{
  return self->exec_command ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_action_get_dbus_interface
 * ------------------------------------------------------------------------- */

const char *
alarm_action_get_dbus_interface(const alarm_action_t *self)
{
  return self->dbus_interface ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_action_get_dbus_service
 * ------------------------------------------------------------------------- */

const char *
alarm_action_get_dbus_service(const alarm_action_t *self)
{
  return self->dbus_service ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_action_get_dbus_path
 * ------------------------------------------------------------------------- */

const char *
alarm_action_get_dbus_path(const alarm_action_t *self)
{
  return self->dbus_path ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_action_get_dbus_name
 * ------------------------------------------------------------------------- */

const char *
alarm_action_get_dbus_name(const alarm_action_t *self)
{
  return self->dbus_name ?: "";
}

/* ------------------------------------------------------------------------- *
 * alarm_action_set_label
 * ------------------------------------------------------------------------- */

void
alarm_action_set_label(alarm_action_t *self, const char *label)
{
  xstrset(&self->label, label);
}

/* ------------------------------------------------------------------------- *
 * alarm_action_set_exec_command
 * ------------------------------------------------------------------------- */

void
alarm_action_set_exec_command(alarm_action_t *self, const char *exec_command)
{
  xstrset(&self->exec_command, exec_command);
}

/* ------------------------------------------------------------------------- *
 * alarm_action_set_dbus_interface
 * ------------------------------------------------------------------------- */

void
alarm_action_set_dbus_interface(alarm_action_t *self, const char *dbus_interface)
{
  xstrset(&self->dbus_interface, dbus_interface);
}

/* ------------------------------------------------------------------------- *
 * alarm_action_set_dbus_service
 * ------------------------------------------------------------------------- */

void
alarm_action_set_dbus_service(alarm_action_t *self, const char *dbus_service)
{
  xstrset(&self->dbus_service, dbus_service);
}

/* ------------------------------------------------------------------------- *
 * alarm_action_set_dbus_path
 * ------------------------------------------------------------------------- */

void
alarm_action_set_dbus_path(alarm_action_t *self, const char *dbus_path)
{
  xstrset(&self->dbus_path, dbus_path);
}

/* ------------------------------------------------------------------------- *
 * alarm_action_set_dbus_name
 * ------------------------------------------------------------------------- */

void
alarm_action_set_dbus_name(alarm_action_t *self, const char *dbus_name)
{
  xstrset(&self->dbus_name, dbus_name);
}

/* ------------------------------------------------------------------------- *
 * alarm_action_is_button
 * ------------------------------------------------------------------------- */

int
alarm_action_is_button(const alarm_action_t *self)
{
  return !xisempty(self->label) && (self->flags & ALARM_ACTION_WHEN_RESPONDED);
}
