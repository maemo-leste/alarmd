#ifndef MISSING_DBUS_H_
# define MISSING_DBUS_H_

/* ========================================================================= *
 * Placeholder for DBUS services not yet available
 * ========================================================================= */

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

/* ========================================================================= *
 * STATUSAREA CLOCK PLUGIN
 * ========================================================================= */

// QUARANTINE # define STATUSAREA_CLOCK_SERVICE   "com.nokia.clock.status_plugin"
// QUARANTINE # define STATUSAREA_CLOCK_PATH      "/com/nokia/clock/status_plugin"
// QUARANTINE # define STATUSAREA_CLOCK_INTERFACE "com.nokia.clock.status_plugin"
// QUARANTINE # define STATUSAREA_CLOCK_ALARM_SET "clock_sa_alarm_set"
// QUARANTINE # define STATUSAREA_CLOCK_NO_ALARMS "clock_sa_no_alarms"

/* ========================================================================= *
 * DSME
 * ========================================================================= */

# if 0

extern const char dsme_service[];
extern const char dsme_req_interface[];
extern const char dsme_sig_interface[];
extern const char dsme_sig_path[];

extern const char dsme_get_version[];
extern const char dsme_req_powerup[];
extern const char dsme_req_reboot[];
extern const char dsme_req_shutdown[];

extern const char dsme_shutdown_ind[];
extern const char dsme_thermal_shutdown_ind[];
extern const char dsme_save_unsaved_data_ind[];

#  define DSME_SERVICE      dsme_service

#  define DSME_REQUEST_IF   dsme_req_interface
#  define DSME_SIGNAL_IF    dsme_sig_interface
#  define DSME_REQUEST_PATH "/com/nokia/dsme/request"
#  define DSME_SIGNAL_PATH  dsme_sig_path

/* dsme methods */
#  define DSME_GET_VERSION           dsme_get_version
#  define DSME_REQ_POWERUP           dsme_req_powerup
#  define DSME_REQ_REBOOT            dsme_req_reboot
#  define DSME_REQ_SHUTDOWN          dsme_req_shutdown

/* dsme signals */
#  define DSME_DATA_SAVE_SIG  dsme_save_unsaved_data_ind
#  define DSME_SHUTDOWN_SIG   dsme_shutdown_ind
#  define DSME_THERMAL_SIG    dsme_thermal_shutdown_ind

/* ???? */
#  define DSME_PREVENT_SUSP_REQ           "prevent_suspend"
#  define DSME_PREVENT_BLANK_REQ          "prevent_display_blanking"
#  define DSME_SET_BLANK_TIMEOUT_REQ      "set_display_blank_timeout"
#  define DSME_SET_DIM_TIMEOUT_REQ        "set_display_dim_timeout"
#  define DSME_SET_DISPLAY_BRIGHTNESS_REQ "set_display_brightness"
#  define DSME_SET_SUSPEND_TIMEOUT_REQ    "set_suspend_timeout"
#  define DSME_CHANGE_STATE_REQ           "change_state_request"
#  define DSME_STATE_QUERY_REQ            "state_query_req"

#  define DSME_DISPLAY_STATUS_SIG         "display_status_change"
#  define DSME_BATTERY_STATUS_SIG         "battery_status_change"
#  define DSME_DEVICE_STATE_CHANGE_SIG    "device_state_change"
#  define DSME_CHARGER_STATUS_SIG         "charger_status_change"
#  define DSME_DATA_SAVE_SIG              "save_unsaved_data"
#  define DSME_INACTIVITY_SIG             "system_inactivity"

#  define DSME_SUSPEND_TIMEOUT        20
#  define DSME_DIM_TIMEOUT            10
#  define DSME_BLANK_TIMEOUT          30
#  define DSME_PREVENT_BLANK_TIMEOUT  60

/* display state defines */
#  ifndef OSSO_DISPLAY_OFF
#   define OSSO_DISPLAY_OFF       1
#   define OSSO_DISPLAY_DIMMED    2
#   define OSSO_DISPLAY_ON        3
#  endif

# else

#  define DSME_SERVICE      "com.nokia.dsme"

#  define DSME_REQUEST_IF   "com.nokia.dsme.request"
#  define DSME_SIGNAL_IF    "com.nokia.dsme.signal"
#  define DSME_REQUEST_PATH "/com/nokia/dsme/request"
#  define DSME_SIGNAL_PATH  "/com/nokia/dsme/signal"

#  define DSME_PREVENT_SUSP_REQ           "prevent_suspend"
#  define DSME_PREVENT_BLANK_REQ          "prevent_display_blanking"
#  define DSME_SET_BLANK_TIMEOUT_REQ      "set_display_blank_timeout"
#  define DSME_SET_DIM_TIMEOUT_REQ        "set_display_dim_timeout"
#  define DSME_SET_DISPLAY_BRIGHTNESS_REQ "set_display_brightness"
#  define DSME_SET_SUSPEND_TIMEOUT_REQ    "set_suspend_timeout"
#  define DSME_CHANGE_STATE_REQ           "change_state_request"
#  define DSME_STATE_QUERY_REQ            "state_query_req"

#  define DSME_DISPLAY_STATUS_SIG         "display_status_change"
#  define DSME_BATTERY_STATUS_SIG         "battery_status_change"
#  define DSME_DEVICE_STATE_CHANGE_SIG    "device_state_change"
#  define DSME_CHARGER_STATUS_SIG         "charger_status_change"
#  define DSME_DATA_SAVE_SIG              "save_unsaved_data"
#  define DSME_INACTIVITY_SIG             "system_inactivity"

#  define DSME_SUSPEND_TIMEOUT        20
#  define DSME_DIM_TIMEOUT            10
#  define DSME_BLANK_TIMEOUT          30
#  define DSME_PREVENT_BLANK_TIMEOUT  60

/* display state defines */
#  ifndef OSSO_DISPLAY_OFF
#   define OSSO_DISPLAY_OFF       1
#   define OSSO_DISPLAY_DIMMED    2
#   define OSSO_DISPLAY_ON        3
#  endif

/* are these in use anymore ? */
#  define DSME_GET_VERSION           "get_version"
#  define DSME_REQ_POWERUP           "req_powerup"
#  define DSME_REQ_REBOOT            "req_reboot"
#  define DSME_REQ_SHUTDOWN          "req_shutdown"
#  define DSME_REQ_ALARM_MODE_CHANGE "req_alarm_mode_change"

/* these seem to actually be sent by dsme??? */

#  undef  DSME_DATA_SAVE_SIG
#  undef  DSME_SHUTDOWN_SIG

#  define DSME_DATA_SAVE_SIG  "save_unsaved_data_ind"
#  define DSME_SHUTDOWN_SIG   "shutdown_ind"

# endif

# ifdef __cplusplus
};
#endif

#endif /* MISSING_DBUS_H_ */
