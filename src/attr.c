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

#include "alarmd_config.h"

#include "libalarm.h"
#include "xutil.h"

#include <stdlib.h>
#include <string.h>

/* ========================================================================= *
 * alarm_attr_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * alarm_attr_ctor
 * ------------------------------------------------------------------------- */

void
alarm_attr_ctor(alarm_attr_t *self)
{
  self->attr_name = 0;
  self->attr_type = ALARM_ATTR_NULL;
  self->attr_data.ival = 0;
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_dtor
 * ------------------------------------------------------------------------- */

void
alarm_attr_dtor(alarm_attr_t *self)
{
  alarm_attr_set_null(self);
  free(self->attr_name);
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_set_null
 * ------------------------------------------------------------------------- */

void
alarm_attr_set_null(alarm_attr_t *self)
{
  switch( self->attr_type )
  {
  case ALARM_ATTR_STRING:
    free(self->attr_data.sval);
    break;
  }
  self->attr_type = ALARM_ATTR_NULL;
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_set_string
 * ------------------------------------------------------------------------- */

void alarm_attr_set_string(alarm_attr_t *self, const char *val)
{
  alarm_attr_set_null(self);
  self->attr_type = ALARM_ATTR_STRING;
  self->attr_data.sval = strdup(val ?: "");
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_set_int
 * ------------------------------------------------------------------------- */

void alarm_attr_set_int(alarm_attr_t *self, int val)
{
  alarm_attr_set_null(self);
  self->attr_type = ALARM_ATTR_INT;
  self->attr_data.ival = val;
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_set_time
 * ------------------------------------------------------------------------- */

void alarm_attr_set_time(alarm_attr_t *self, time_t val)
{
  alarm_attr_set_null(self);
  self->attr_type = ALARM_ATTR_TIME;
  self->attr_data.tval = val;
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_get_string
 * ------------------------------------------------------------------------- */

const char *alarm_attr_get_string(alarm_attr_t *self)
{
  const char *res = NULL;

  switch( self->attr_type )
  {
  case ALARM_ATTR_STRING:
    res = self->attr_data.sval;
    break;
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_get_int
 * ------------------------------------------------------------------------- */

int alarm_attr_get_int(alarm_attr_t *self)
{
  switch( self->attr_type )
  {
  case ALARM_ATTR_INT:
    return self->attr_data.ival;

  case ALARM_ATTR_TIME:
    return self->attr_data.tval;

  case ALARM_ATTR_STRING:
    return strtol(self->attr_data.sval ?: "", 0, 0);
  }

  return -1;
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_get_time
 * ------------------------------------------------------------------------- */

time_t alarm_attr_get_time(alarm_attr_t *self)
{
  switch( self->attr_type )
  {
  case ALARM_ATTR_INT:
    return self->attr_data.ival;

  case ALARM_ATTR_TIME:
    return self->attr_data.tval;

  case ALARM_ATTR_STRING:
    return strtol(self->attr_data.sval ?: "", 0, 0);
  }

  return -1;
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_create
 * ------------------------------------------------------------------------- */

alarm_attr_t *
alarm_attr_create(const char *name)
{
  alarm_attr_t *self = calloc(1, sizeof *self);
  alarm_attr_ctor(self);
  xstrset(&self->attr_name, name);
  return self;
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_delete
 * ------------------------------------------------------------------------- */

void
alarm_attr_delete(alarm_attr_t *self)
{
  if( self != 0 )
  {
    alarm_attr_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * alarm_attr_delete_cb
 * ------------------------------------------------------------------------- */

void
alarm_attr_delete_cb(void *self)
{
  alarm_attr_delete(self);
}
