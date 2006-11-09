#include <stdio.h>
#include <stdlib.h>
#include "alarm_event.h"

int main()
{
       cookie_t identifier;
       cookie_t identifier2;
       cookie_t identifier3;
       cookie_t *list;
       int i;
       alarm_event_t event = {
               .alarm_time = 0,
               .recurrence = 0,
               .recurrence_count = 0,
               .snooze = 5,
               .message = (char *)"Rairairai",
               .sound = NULL,
               .icon = (char *)"qgn_indi_voip_mute",
               .dbus_interface = NULL,
               .dbus_service = NULL,
               .dbus_path = NULL,
               .dbus_name = NULL,
               .exec_name = (char *)"/bin/ls",
               .flags = ALARM_EVENT_SHOW_ICON
       };
       event.alarm_time = time(NULL) + 20;
       identifier = alarm_event_add(&event);
       event.alarm_time += 20;
       identifier2 = alarm_event_add(&event);
       event.alarm_time += 20;
       identifier3 = alarm_event_add(&event);
       printf("Got id: %ld\n", identifier);
       printf("Got id2: %ld\n", identifier2);
       printf("Got id3: %ld\n", identifier3);
       list = alarm_event_query(time(NULL), time(NULL) + 40, 0, 0);
       printf("Listing events.\n");
       for (i = 0; list && list[i]; i++) {
               printf("Event[%d]: %ld\n", i, list[i]);
       }
       if (list && list[0]) {
               alarm_event_t *queried = NULL;
               queried = alarm_event_get(list[0]);
               if (queried) {
                       printf("message =  %s, icon = %s, exec_name = %s\n", queried->message, queried->icon, queried->exec_name);
                       alarm_event_free(queried);
               }
       }
       free(list);
       if (alarm_event_del(identifier)) {
               printf("Removing event succeeded.\n");
       } else {
               printf("Removing event failed.\n");
       }
       if (alarm_event_del(identifier2)) {
               printf("Removing event2 succeeded.\n");
       } else {
               printf("Removing event2 failed.\n");
       }
       if (alarm_event_del(identifier3)) {
               printf("Removing event3 succeeded.\n");
       } else {
               printf("Removing event3 failed.\n");
       }
       printf("Listing events.\n");
       list = alarm_event_query(time(NULL), time(NULL) + 40, 0, 0);
       for (i = 0; list && list[i]; i++) {
               printf("Event[%d]: %ld\n", i, list[i]);
       }
       free(list);

       return 0;
}
