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

#include "ipc_exec.h"
#include "logging.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

static uid_t ipc_exec_exec_uid = -1;
static gid_t ipc_exec_exec_gid = -1;

/* ------------------------------------------------------------------------- *
 * ipc_exec_drop_privileges  --  loose root privileges for good
 * ------------------------------------------------------------------------- */

static
int
ipc_exec_drop_privileges(void)
{
  uid_t nuid = ipc_exec_exec_uid;
  gid_t ngid = ipc_exec_exec_gid;

  uid_t ruid, euid, suid;
  gid_t rgid, egid, sgid;

  if( setresgid(ngid,ngid,ngid) == -1 )
  {
    return -1;
  }
  if( getresgid(&rgid, &egid, &sgid) == -1 )
  {
    return -1;
  }
  if( rgid != ngid || egid != ngid || sgid != ngid )
  {
    return -1;
  }

  if( setresuid(nuid,nuid,nuid) == -1 )
  {
    return -1;
  }
  if( getresuid(&ruid, &euid, &suid) == -1 )
  {
    return -1;
  }
  if( ruid != nuid || euid != nuid || suid != nuid )
  {
    return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------------- *
 * ipc_exec_get_exec_identity  --  determine user/group to exec as
 * ------------------------------------------------------------------------- */

static
void
ipc_exec_get_exec_identity(void)
{
  int nobody_uid = -1;
  int nobody_gid = -1;
  int user_uid   = -1;
  int user_gid   = -1;

  struct passwd *pw;

  setpwent();

  while( (pw = getpwent()) != 0 )
  {
    if( pw->pw_name == 0 )
    {
    }
    else if( !strcmp(pw->pw_name, "nobody") )
    {
      nobody_uid = pw->pw_uid;
      nobody_gid = pw->pw_gid;

      log_debug("nobody: %d / %d\n", nobody_uid, nobody_gid);

    }
    else if( !strcmp(pw->pw_name, "user") )
    {
      user_uid = pw->pw_uid;
      user_gid = pw->pw_gid;

      log_debug("user: %d / %d\n", user_uid, user_gid);
    }
  }

  endpwent();

  if( user_uid != -1 )
  {
    ipc_exec_exec_uid = user_uid;
    ipc_exec_exec_gid = user_gid;
  }
  else if( nobody_uid != -1 )
  {
    ipc_exec_exec_uid = nobody_uid;
    ipc_exec_exec_gid = nobody_gid;
  }
  else
  {
    log_warning("could not determine exec uid\n");
  }
}

/* ------------------------------------------------------------------------- *
 * ipc_exec_run_command  --  execute command in detached child process
 * ------------------------------------------------------------------------- */

int
ipc_exec_run_command(const char *cmd)
{
  int     err    = -1;
  int     child  = -1;
  int     status = 0;
  gint    argc   = 0;
  gchar **argv   = 0;
  GError *error  = 0;

  if( geteuid() == 0 )
  {
    if( ipc_exec_exec_uid == -1 || ipc_exec_exec_gid == -1 )
    {
      log_error("will not exec as root\n");
      goto cleanup;
    }
  }

  if( !g_shell_parse_argv(cmd, &argc, &argv, &error) )
  {
    if( error != 0 )
    {
      log_error("command line parse: %s\n", error->message);
    }
    goto cleanup;
  }

  fflush(0);

  switch( (child = fork()) )
  {
  case 0: // child
    setsid();

    if( geteuid() == 0 && ipc_exec_drop_privileges() != 0 )
    {
      log_error("can't drop privileges\n");
      _exit(1);
    }
    switch( fork() )
    {
    case 0: // grandchild
      chdir("/");
      umask(0);

      for( int fd = 0, fdmax = sysconf(_SC_OPEN_MAX); fd < fdmax; ++fd )
      {
        close(fd);
      }
      open("/dev/null", O_RDONLY);
      open("/dev/null", O_WRONLY);
      open("/dev/null", O_WRONLY);

      if( geteuid() > 0 && getuid() > 0 )
      {
        execvp(*argv, argv);
      }
      _exit(1);

    case -1: // child @ failure
      _exit(1);
    }
    _exit(0); // child @ success

  case -1: // parent @ error
    log_error("FORK: %s: %s\n", cmd, strerror(errno));
    break;

  default: // parent @ success
    waitpid(child, &status, 0);
    if( WIFEXITED(status) && !WEXITSTATUS(status) )
    {
      err = 0;
    }
    break;
  }

  cleanup:

  if( error != 0 )
  {
    g_error_free(error);
  }

  if( argv != 0 )
  {
    g_strfreev(argv);
  }

  return err;
}

/* ------------------------------------------------------------------------- *
 * ipc_exec_init
 * ------------------------------------------------------------------------- */

int
ipc_exec_init(void)
{
  ipc_exec_get_exec_identity();
  return 0;
}

/* ------------------------------------------------------------------------- *
 * ipc_exec_quit
 * ------------------------------------------------------------------------- */

int
ipc_exec_quit(void)
{
  return 0;
}
