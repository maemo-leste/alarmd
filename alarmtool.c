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

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "include/alarm_event.h"

extern char *optarg;
extern int optind;

static int tool_add(int argc, char **argv);
static int tool_remove(const char *arg);
static int tool_list(int argc, char **argv);
static int tool_get(const char *arg);
static void usage(const char *name);

int main(int argc, char **argv)
{
       int optc;
       int opt_index;

       const char optline[] = "ar:qg:";

       struct option const options_top[] = {
               { "add", 0, 0, 'a' },
               { "remove", 1, 0, 'r' },
               { "query", 0, 0, 'q' },
               { "get", 1, 0, 'g' },
               { 0, 0, 0, 0 }
       };

       while ((optc = getopt_long(argc, argv, optline,
                                       options_top, &opt_index)) != -1) {
               switch (optc) {
               case 'a':
                       return tool_add(argc, argv);
                       break;
               case 'r':
                       return tool_remove(optarg);
                       break;
               case 'q':
                       return tool_list(argc, argv);
                       break;
               case 'g':
                       return tool_get(optarg);
                       break;
               default:
                       usage(argv[0]);
                       return 1;
               }
       }

       usage(argv[0]);
       return 1;

       return 0;
}

static int tool_add(int argc, char **argv)
{
       int optc;
       int opt_index;
       alarm_event_t new_event;
       long event_id;

       memset(&new_event, 0, sizeof(new_event));

       struct option const options_add[] = {
               { "time", 1, 0, 't' },
               { "recurr", 1, 0, 'R' },
               { "recurr-count", 1, 0, 'C' },
               { "dbus-interface", 1, 0, 'i' },
               { "dbus-service", 1, 0, 's' },
               { "dbus-path", 1, 0, 'p' },
               { "dbus-name", 1, 0, 'n' },
               { "dbus-system-bus", 0, 0, 'y' },
               { "dialog-message", 1, 0, 'm' },
               { "dialog-icon", 1, 0, 'c' },
               { "dialog-sound", 1, 0, 'u' },
               { "exec", 1, 0, 'e' },
               { "no-dialog", 0, 0, 'd' },
               { "no-snooze", 0, 0, 'z' },
               { "boot", 0, 0, 'o' },
               { "show-icon", 0, 0, 'b' },
               { "run-delayed", 0, 0, 'r' },
               { "connected", 0, 0, 'w' },
               { "activation", 0, 0, 'a' },
               { "postpone-delayed", 0, 0, 'P' },
               { "back-reschedule", 0, 0, 'B' },
               { 0, 0, 0, 0 }
       };

       const char optline[] = "t:R:C:i:s:p:n:ym:c:u:e:dzobrwaPB";

       while ((optc = getopt_long(argc, argv, optline,
                                       options_add, &opt_index)) != -1) {
               switch (optc) {
               case 'm':
                       if (new_event.message) {
                               free(new_event.message);
                       }
                       new_event.message = strdup(optarg);
                       break;
               case 'c':
                       if (new_event.icon) {
                               free(new_event.icon);
                       }
                       new_event.icon = strdup(optarg);
                       break;
               case 'u':
                       if (new_event.sound) {
                               free(new_event.sound);
                       }
                       new_event.sound = strdup(optarg);
                       break;
               case 't':
                       new_event.alarm_time = atol(optarg);
                       break;
               case 'R':
                       new_event.recurrence = atoi(optarg);
                       break;
               case 'C':
                       new_event.recurrence_count = atoi(optarg);
                       break;
               case 'i':
                       if (new_event.dbus_interface) {
                               free(new_event.dbus_interface);
                       }
                       new_event.dbus_interface = strdup(optarg);
                       break;
               case 's':
                       if (new_event.dbus_service) {
                               free(new_event.dbus_service);
                       }
                       new_event.dbus_service = strdup(optarg);
                       break;
               case 'p':
                       if (new_event.dbus_path) {
                               free(new_event.dbus_path);
                       }
                       new_event.dbus_path = strdup(optarg);
                       break;
               case 'n':
                       if (new_event.dbus_name) {
                               free(new_event.dbus_name);
                       }
                       new_event.dbus_name = strdup(optarg);
                       break;
               case 'e':
                       if (new_event.exec_name) {
                               free(new_event.exec_name);
                       }
                       new_event.exec_name = strdup(optarg);
                       break;
               case 'y':
                       new_event.flags |= ALARM_EVENT_SYSTEM;
                       break;
               case 'd':
                       new_event.flags |= ALARM_EVENT_NO_DIALOG;
                       break;
               case 'z':
                       new_event.flags |= ALARM_EVENT_NO_SNOOZE;
                       break;
               case 'o':
                       new_event.flags |= ALARM_EVENT_BOOT;
                       break;
               case 'b':
                       new_event.flags |= ALARM_EVENT_SHOW_ICON;
                       break;
               case 'r':
                       new_event.flags |= ALARM_EVENT_RUN_DELAYED;
                       break;
               case 'w':
                       new_event.flags |= ALARM_EVENT_CONNECTED;
                       break;
               case 'a':
                       new_event.flags |= ALARM_EVENT_ACTIVATION;
                       break;
               case 'P':
                       new_event.flags |= ALARM_EVENT_POSTPONE_DELAYED;
                       break;
               case 'B':
                       new_event.flags |= ALARM_EVENT_BACK_RESCHEDULE;
                       break;
               default:
                       fprintf(stderr, "%s: got %c\n", __FUNCTION__, optc);
                       break;
               }
       }
       event_id = alarm_event_add(&new_event);
       if (event_id) {
               printf("Event added with id %ld\n", event_id);
               return 0;
       }
       else {
               fprintf(stderr, "Adding event failed.\n");
               return 1;
       }
       return 0;
}

static int tool_list(int argc, char **argv)
{
       int optc;
       int opt_index;

       int32_t flag_mask = 0;
       int32_t flags = 0;

       time_t start_time = 0;
       time_t end_time = ~0;

       long *events;

       if (end_time < 0) {
               end_time--;
       }

       struct option const options_list[] = {
               { "first-time", 1, 0, 'f' },
               { "last-time", 1, 0, 'l' },
               { "dbus-system-bus", 0, 0, 'y' },
               { "no-dialog", 0, 0, 'd' },
               { "no-snooze", 0, 0, 'z' },
               { "boot", 0, 0, 'o' },
               { "show-icon", 0, 0, 'b' },
               { "run-delayed", 0, 0, 'r' },
               { "dbus-session-bus", 0, 0, 'Y' },
               { "dialog", 0, 0, 'D' },
               { "snooze", 0, 0, 'Z' },
               { "no-boot", 0, 0, 'O' },
               { "no-show-icon", 0, 0, 'B' },
               { "no-run-delayed", 0, 0, 'R' },
               { "connected", 0, 0, 'c' },
               { "no-connected", 0, 0, 'C' },
               { "activation", 0, 0, 'a' },
               { "no-activation", 0, 0, 'A' },
               { "postpone-delayed", 0, 0, 'p' },
               { "no-postpne-dlayed", 0, 0, 'P' },
               { "back-reschedule", 0, 0, 'e' },
               { "no-back-reschedule", 0, 0, 'E' },
               { 0, 0, 0, 0 }
       };

       const char optline[] = "f:l:ydzobrYDZOBRcCaApPeE";

       while ((optc = getopt_long(argc, argv, optline,
                                       options_list, &opt_index)) != -1) {
               switch (optc) {
               case 'f':
                       start_time = atol(optarg);
                       break;
               case 'l':
                       end_time = atol(optarg);
                       break;
               case 'y':
                       flags |= ALARM_EVENT_SYSTEM;
                       flag_mask |= ALARM_EVENT_SYSTEM;
                       break;
               case 'd':
                       flags |= ALARM_EVENT_NO_DIALOG;
                       flag_mask |= ALARM_EVENT_NO_DIALOG;
                       break;
               case 'z':
                       flags |= ALARM_EVENT_NO_SNOOZE;
                       flag_mask |= ALARM_EVENT_NO_SNOOZE;
                       break;
               case 'o':
                       flags |= ALARM_EVENT_BOOT;
                       flag_mask |= ALARM_EVENT_BOOT;
                       break;
               case 'b':
                       flags |= ALARM_EVENT_SHOW_ICON;
                       flag_mask |= ALARM_EVENT_SHOW_ICON;
                       break;

               case 'r':
                       flags |= ALARM_EVENT_RUN_DELAYED;
                       flag_mask |= ALARM_EVENT_RUN_DELAYED;
                       break;
               case 'Y':
                       flags &= ~ALARM_EVENT_SYSTEM;
                       flag_mask |= ALARM_EVENT_SYSTEM;
                       break;
               case 'D':
                       flags &= ~ALARM_EVENT_NO_DIALOG;
                       flag_mask |= ALARM_EVENT_NO_DIALOG;
                       break;
               case 'Z':
                       flags &= ~ALARM_EVENT_NO_SNOOZE;
                       flag_mask |= ALARM_EVENT_NO_SNOOZE;
                       break;
               case 'O':
                       flags &= ~ALARM_EVENT_BOOT;
                       flag_mask |= ALARM_EVENT_BOOT;
                       break;
               case 'B':
                       flags &= ~ALARM_EVENT_SHOW_ICON;
                       flag_mask |= ALARM_EVENT_SHOW_ICON;
                       break;
               case 'R':
                       flags &= ~ALARM_EVENT_RUN_DELAYED;
                       flag_mask |= ALARM_EVENT_RUN_DELAYED;
                       break;
               case 'c':
                       flags |= ALARM_EVENT_CONNECTED;
                       flag_mask |= ALARM_EVENT_CONNECTED;
                       break;
               case 'C':
                       flags &= ~ALARM_EVENT_CONNECTED;
                       flag_mask |= ALARM_EVENT_CONNECTED;
                       break;
               case 'a':
                       flags |= ALARM_EVENT_ACTIVATION;
                       flag_mask |= ALARM_EVENT_ACTIVATION;
                       break;
               case 'A':
                       flags &= ~ALARM_EVENT_ACTIVATION;
                       flag_mask |= ALARM_EVENT_ACTIVATION;
                       break;
               case 'p':
                       flags |= ALARM_EVENT_POSTPONE_DELAYED;
                       flag_mask |= ALARM_EVENT_POSTPONE_DELAYED;
                       break;
               case 'P':
                       flags &= ~ALARM_EVENT_POSTPONE_DELAYED;
                       flag_mask |= ALARM_EVENT_POSTPONE_DELAYED;
                       break;
               case 'e':
                       flags |= ALARM_EVENT_BACK_RESCHEDULE;
                       flag_mask |= ALARM_EVENT_BACK_RESCHEDULE;
                       break;
               case 'E':
                       flags &= ~ALARM_EVENT_BACK_RESCHEDULE;
                       flag_mask |= ALARM_EVENT_BACK_RESCHEDULE;
                       break;
               default:
                       return 1;
                       break;
               }
       }

       events = alarm_event_query(start_time, end_time, flag_mask, flags);

       if (events == NULL) {
               fprintf(stderr, "Error while querying the events.\n");
               return -1;
       } else {
               if (*events == 0) {
                       printf("No events.\n");
               } else {
                       unsigned int i = 0;

                       printf("Events found:\n");
                       for (i = 0; events[i] != 0; i++) {
                               printf("\t%ld\n", events[i]);
                       }
               }
               free(events);
       }

       return 0;
}

static int tool_remove(const char *arg)
{
       char* err;
       cookie_t event_id=strtol(arg,&err,10);

       if (*err != '\0') {
               // Invalid cookie
               fprintf(stderr,"Invalid event id \"%s\"\n",arg);
               return 1;
       }

       if (alarm_event_del(event_id)) {
               // event removed
               return 0;
       } else {
               fprintf(stderr,"Error while removing event \"%s\"\n",arg);
               // Error while removing the event
       }
       return -1;
}

static int tool_get(const char *arg)
{
       alarm_event_t* event;
       cookie_t event_id;
       char* err;
       fprintf(stderr, "%s(%s)\n", __FUNCTION__, arg);

       event_id=strtol(arg,&err,10);

       if (*err != '\0') {
               fprintf(stderr,"Invalid event id \"%s\"\n",arg);
               // Invalid cookie
               return 1;
       }

       event = alarm_event_get(event_id);

       if (event==NULL) {
               fprintf(stderr, "Can't get event \"%s\"\n", arg);
               // Error

               return -1;
       } else {
               const char *system, *dialog, *snooze, *boot, *actdead,
                       *icon, *delayed, *connected, *activation,
                       *postpone, *breschedule;

               if (event->flags & ALARM_EVENT_SYSTEM) {
                       system = "System ";
               } else {
                       system = "";
               }

               if (event->flags & ALARM_EVENT_NO_DIALOG) {
                       dialog = "No_dialog ";
               } else {
                       dialog = "";
               }

               if (event->flags & ALARM_EVENT_NO_SNOOZE) {
                       snooze = "No_snooze ";
               } else {
                       snooze = "";
               }

               if (event->flags & ALARM_EVENT_BOOT) {
                       boot = "Boot ";
               } else {
                       boot = "";
               }

               if (event->flags & ALARM_EVENT_ACTDEAD) {
                       actdead = "Boot_Actdead ";
               } else {
                       actdead = "";
               }

               if (event->flags & ALARM_EVENT_SHOW_ICON) {
                       icon = "Icon ";
               } else {
                       icon = "";
               }

               if (event->flags & ALARM_EVENT_RUN_DELAYED) {
                       delayed = "No_dialog ";
               } else {
                       delayed = "";
               }

               if (event->flags & ALARM_EVENT_CONNECTED) {
                       connected = "Connected ";
               } else {
                       connected = "";
               }

               if (event->flags & ALARM_EVENT_ACTIVATION) {
                       activation = "Activation ";
               } else {
                       activation = "";
               }

               if (event->flags & ALARM_EVENT_POSTPONE_DELAYED) {
                       postpone = "Postpone ";
               } else {
                       postpone = "";
               }

               if (event->flags & ALARM_EVENT_BACK_RESCHEDULE) {
                       breschedule = "Back_Reschedule ";
               } else {
                       breschedule = "";
               }

               printf("\ttime\t: \"%u\"\n"
                               "\tmgs\t: \"%s\"\n"
                               "\ticon\t: \"%s\"\n"
                               "\tsound\t: \"%s\"\n"
                               "\tiface\t: \"%s\"\n"
                               "\tservice\t: \"%s\"\n"
                               "\tpath\t: \"%s\"\n"
                               "\tname\t: \"%s\"\n"
                               "\texec-name\t: \"%s\"\n"
                               "\tflags\t: \"%s%s%s%s%s%s%s%s%s%s%s\"\n"
                               ,
                               (unsigned)event->alarm_time,
                               event->message,
                               event->icon,
                               event->sound,
                               event->dbus_interface,
                               event->dbus_service,
                               event->dbus_path,
                               event->dbus_name,
                               event->exec_name,
                               dialog, snooze, system, boot, actdead, icon,
                               delayed, connected, activation, postpone,
                               breschedule
                     );

       }
       return 0;
}

static void usage(const char *name)
{
       printf("Usage:\n"
                       "\t%s -a [-tispnymcuedzobr] [option params]\n"
                       "\t%s -r <cookie>\n"
                       "\t%s -l [-ydzobrYDZOBR]\n"
                       "\t%s -g <cookie>\n\n"
                       "\t-a, --add\tAdd event to queue\n"
                       "\t-t, --time\n", name, name, name, name);
}
