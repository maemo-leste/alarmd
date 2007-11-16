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

#include <glib.h>
#include <glib-object.h>
#include <errno.h>
#include <fcntl.h>		/* open */
#include <stdio.h>
#include <getopt.h>
#include <signal.h>		/* signal */
#include <stdlib.h>		/* exit */
#include <string.h>		/* strerror */
#include <unistd.h>		/* close */
#include <sys/stat.h>		/* open */
#include <sys/types.h>		/* open */

#include "alarmd.h"
#include "initialize.h"
#include "queue.h"
#include "rpc-osso.h"
#include "rpc-dbus.h"

#define ALARMD_LOCKFILE		"/var/run/alarmd.pid"
#define PRG_NAME		"alarmd"

extern int optind;
extern char *optarg;

static const gchar *progname;

/*
 * Display usage information
 */
static void usage(void)
{
	fprintf(stdout,
		_("Usage: %s [OPTION]...\n"
		  "Alarm daemon\n"
		  "\n"
		  "  -d, --daemonflag    run alarmd as a daemon\n"
		  "      --help          display this help and exit\n"
		  "      --version       output version information and exit\n"
		  "\n"
		  "Report bugs to <david.weinehall@nokia.com>\n"),
		progname);
}

/*
 * Display version information
 */
static void version(void)
{
	fprintf(stdout, _("%s v%s\n%s"),
		progname,
		VERSION,
		_("Written by Santtu Lakkala.\n"
		  "Contact person: David Weinehall "
		  "<david.weinehall@nokia.com>\n"
		  "\n"
		  "Copyright (C) 2006 Nokia Corporation.\n"
		  "This is free software; see the source for copying "
		  "conditions. There is NO\n"
		  "warranty; not even for MERCHANTABILITY or FITNESS FOR A "
		  "PARTICULAR PURPOSE.\n"));
}

/*
 * Initialise locale support
 *
 * @param name The program name to output in usage/version information
 * @return 0 on success, non-zero on failure
 */
static gint init_locales(const gchar *const name)
{
	gint status = 0;

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");

	if ((bindtextdomain(name, LOCALEDIR) == 0) && (errno == ENOMEM)) {
		status = errno;
		goto EXIT;
	}

	if ((textdomain(name) == 0) && (errno == ENOMEM)) {
		status = errno;
		return 0;
	}

EXIT:
	/* In this error-message we don't use _(), since we don't
	 * know where the locales failed, and we probably won't
	 * get a reasonable result if we try to use them.
	 */
	if (status != 0) {
		fprintf(stderr,
			"%s: `%s' failed; %s. Aborting.\n",
			name, "init_locales", strerror(errno));
	} else {
		progname = _(name);
		errno = 0;
	}
#else
	progname = name;
#endif /* ENABLE_NLS */

	return status;
}

/*
 * Signal handler
 *
 * @param signr Signal type
 */
static void signal_handler(const gint signr)
{
	switch (signr) {
	case SIGUSR1:
		/* we'll probably want some way to communicate with alarmd */
		break;

	case SIGHUP:
		/* possibly for re-reading configuration? */
		break;

	case SIGTERM:
		g_debug("Stopping alarmd...");
		g_main_loop_quit(mainloop);
		break;

	default:
		/* should never happen */
		break;
	}
}

/*
 * Daemonize the program
 */
static void daemonize(void)
{
	gint i = 0;
	gchar str[10];

	if (getppid() == 1)
		return;		/* Already daemonized */

	/* detach from process group */
	switch (fork()) {
	case -1:
		/* Failure */
		g_critical("daemonize: fork failed: %s", strerror(errno));
		exit(EXIT_FAILURE);

	case 0:
		/* Child */
		break;

	default:
		/* Parent -- exit */
		exit(EXIT_SUCCESS);
	}

	/* Detach TTY */
	setsid();

	/* Close all file descriptors and redirect stdio to /dev/null */
	if ((i = getdtablesize()) == -1)
		i = 256;

	while (--i >= 0)
		close(i);

	i = open("/dev/null", O_RDWR);
	dup(i);
	dup(i);

	/* Set umask */
	umask(022);

	/* Set working directory */
	chdir("/tmp");

	/* If the file exists, we have crashed / restarted */
	if (access(ALARMD_LOCKFILE, F_OK) == 0) {
		/* OK */
	} else if (errno != ENOENT) {
		g_critical("access() failed: %s. Exiting.", g_strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Single instance */
	if ((i = open(ALARMD_LOCKFILE, O_RDWR | O_CREAT, 0640)) < 0) {
		g_critical("Cannot open lockfile. Exiting.");
		exit(EXIT_FAILURE);
	}

	if (lockf(i, F_TLOCK, 0) < 0) {
		g_critical("Already running. Exiting.");
		exit(EXIT_FAILURE);
	}

	sprintf(str, "%d\n", getpid());
	write(i, str, strlen(str));
	close(i);

	/* Ignore TTY signals */
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);

	/* Ignore child terminate signal */
	signal(SIGCHLD, SIG_IGN);
}

/*
 * Main
 *
 * @param argc Number of command line arguments
 * @param argv Array with command line arguments
 * @return 0 on success, non-zero on failure
 */
int main(int argc, char **argv)
{
	int optc;
	int opt_index;

	gint status = 0;
	gboolean daemonflag = FALSE;

	AlarmdQueue *queue = NULL;
	osso_context_t *osso = NULL;
	gchar *queue_file = NULL;
	gchar *next_time_file = NULL;
	gchar *next_mode_file = NULL;

	const char optline[] = "d";

	struct option const options[] = {
		{ "daemonflag", no_argument, 0, 'd' },
		{ "help", no_argument, 0, 'h' },
		{ "version", no_argument, 0, 'V' },
		{ 0, 0, 0, 0 }
	};

	/* NULL the mainloop */
	mainloop = NULL;

	/* Initialise support for locales, and set the program-name */
	if (init_locales(PRG_NAME) != 0)
		goto EXIT;

	/* Parse the command-line options */
	while ((optc = getopt_long(argc, argv, optline,
				   options, &opt_index)) != -1) {
		switch (optc) {
		case 'd':
			daemonflag = TRUE;
			break;

		case 'h':
			usage();
			goto EXIT;

		case 'V':
			version();
			goto EXIT;

		default:
			usage();
			status = EINVAL;
			goto EXIT;
		}
	}

	/* We don't take any non-flag arguments */
	if ((argc - optind) > 0) {
		fprintf(stderr,
			_("%s: Too many arguments\n"
			  "Try: `%s --help' for more information.\n"),
			progname, progname);
		status = EINVAL;
		goto EXIT;
	}

	/* Daemonize if requested */
	if (daemonflag == TRUE)
		daemonize();

	signal(SIGUSR1, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Initialize GLib type system. */
	g_type_init();

	alarmd_type_init();
	osso = init_osso();
	dbus_set_osso(osso);

	queue_file = g_build_filename(DATADIR, "alarm_queue.xml", NULL);
	next_time_file = g_build_filename(DATADIR, "next_alarm_time", NULL);
	next_mode_file = g_build_filename(DATADIR, "next_alarm_mode", NULL);

	queue = init_queue(queue_file, next_time_file, next_mode_file);
	set_osso_callbacks(osso, queue);
	g_free(queue_file);
	g_free(next_time_file);
	g_free(next_mode_file);

	/* Register a mainloop */
	mainloop = g_main_loop_new(NULL, FALSE);

	/* Run the main loop */
	g_main_loop_run(mainloop);


	deinit_osso(osso, queue);
	g_object_unref(queue);
	/* If we get here, the main loop has terminated;
	 * either because we requested or because of an error
	 */
EXIT:
	/* If the mainloop is initialised, unreference it */
	if (mainloop != NULL)
		g_main_loop_unref(mainloop);

	/* Log a farewell message and close the log */
	g_message("Exiting...");

	return status;
}
