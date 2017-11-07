#include <pebble.h>

#define debug 0
#define DEBUG 0

// Frequency for old applications
#define SAMPLING_TIMER_BACKWARDS 9000
// This has to be the same as SleepService.FRAMERATE
#define SAMPLING_TIMER 10000
// Maximum size of in-memory buffer of accel changes to be send
#define MAX_QUEUED_VALUES 60
// Seconds without a message from phone, till we consider connection te be broken
#define NO_MESSAGE_DEAD_TIME 60

#define SECONDS_IN_WEEK 604800

// CONSTANTS
// Messages coming from phone
enum {
    MSG_KEY_ALARM_START = 1,
    MSG_KEY_ALARM_STOP = 2,
    MSG_KEY_ALARM_HOUR_OF_DAY = 3,
    MSG_KEY_ALARM_MINUTE = 4,
    MSG_KEY_SET_BATCH_SIZE = 5,
    MSG_KEY_SET_SUSPEND_STATUS = 6,
    MSG_KEY_SUSPEND_TILL_TS = 7,
    MSG_KEY_HINT = 8,
    MSG_KEY_ALARM_TS = 9,
    MSG_KEY_ENABLE_HR = 10,
};
// Messages coming from watch
enum {
    MSG_KEY_STARTED = 0,
    MSG_KEY_ACCEL = 1,
    MSG_KEY_SNOOZE = 2,
    MSG_KEY_DISMISS = 3,
    MSG_KEY_PAUSE = 4,
    MSG_KEY_ACCEL_BATCH = 5,
    MSG_KEY_START_APP = 6,
    MSG_KEY_RESUME = 7,
    MSG_KEY_HR = 8,
    MSG_KEY_ACCEL_BATCH_NEW = 9,
};

/*
// Messages coming from js
enum {
    MSG_KEY_TIMELINE_TOKEN = 999,
};
*/

// Clay settings struct
#define SETTINGS_KEY 1
typedef struct ClaySettings {
  bool enableBackground;
  bool enableInfo;
	bool enableHeartrate;
	bool doubleTapExit;
	unsigned char timeFont;
	unsigned char alarmFont;
	
	unsigned char alarmVibe;
} ClaySettings;
static ClaySettings conf;

static void default_settings() {
	APP_LOG(APP_LOG_LEVEL_INFO, "Conf - Loading default settings.");
	conf.enableBackground = true;
	conf.enableInfo = true;
	conf.enableHeartrate = false;
	conf.doubleTapExit = false;
	conf.timeFont = 0;
	conf.alarmFont = 0;
	conf.alarmVibe = 0;
}

// info layer vars
bool bluetoothState = false;
bool bluetoothStateOld = false;
bool quietTimeState = false;
bool quietTimeStateOld = false;
unsigned char batteryLevel = 0;
unsigned char batteryState = 0;
HealthValue bpmValue = 0;
HealthValue bpmValueOld = 1;
static char hr_text_buffer[8];

// VARIABLES
static bool last = false;

static bool hr = false;

static int last_x, last_y, last_z;

static int max_sum;
static int max_sum_new;
static int hint_repeat;
static int requested_buffer_size = 1;
static int pending_values_count = 0;
static int pending_values[MAX_QUEUED_VALUES];
static int pending_values_new[MAX_QUEUED_VALUES];

static bool suspended = false;
static int suspend_till_ts = false;
static int scheduled_alarm_ts = 0; // Time when the next alarm should fire
static int last_received_msg_ts = 0; // Timestamp of last received message from phone.

static bool acked = true;

static bool new_app = false;

static bool hide_ab_with_next_tick = false;
static bool alarm = false;
static int alarm_counter = 0;
static int alarm_delay = 0;

static int postponed_action = -1;

static char* timeline_token;

static int current_hr = 0;

// POINTERS
static AppTimer *timer;
static AppTimer *action_acked_timer;

static GFont font_w800_42;

static GBitmap *image;
static GBitmap *image_alarm;
static GBitmap *image_action_snooze;
static GBitmap *image_action_dismiss;
static GBitmap *image_action_pause;

static GBitmap *icon_sprites, *icon_bt, *icon_qt, *icon_bat, *icon_heart;

static Window *window;
static ActionBarLayer *action_bar_layer;
static TextLayer *text_layer;
static TextLayer *alarm_layer;
static TextLayer *pause_layer;

static TextLayer *alarm_parent_layer;

static BitmapLayer *image_layer;
static BitmapLayer *image_layer_pause;
static BitmapLayer *image_layer_alarm;

static Layer *info_layer;

// Forward declarations
static void alarm_hide();
static void action_bar_hide(Window *window);
static bool is_connection_dead();
static void send_action_using_app_message(int action);
static void send_timeline_token(char* token);


static uint32_t segments1[] = { 800, 400, 800, 400, 800 };
static uint32_t segments2[] = { 800, 400, 800 };
static uint32_t segments3[] = { 800, 400, 800, 400, 800 };
static uint32_t segments4[] = { 800, 400, 800, 400, 800, 400, 800 };
static uint32_t segments5[] = { 800, 400, 800, 400, 800, 400, 800, 400, 800 };
static uint32_t segments10[] = { 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800 };
VibePattern pat = {  .durations = segments1,  .num_segments = ARRAY_LENGTH(segments1),};
VibePattern pat2 = {  .durations = segments2,  .num_segments = ARRAY_LENGTH(segments2),};
VibePattern pat3 = {  .durations = segments3,  .num_segments = ARRAY_LENGTH(segments3),};
VibePattern pat4 = {  .durations = segments4,  .num_segments = ARRAY_LENGTH(segments4),};
VibePattern pat5 = {  .durations = segments5,  .num_segments = ARRAY_LENGTH(segments5),};
VibePattern pat10 = {  .durations = segments10,  .num_segments = ARRAY_LENGTH(segments10),};

// Helper methods
// Returns the hour value
static unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) { return hour; }
  unsigned short display_hour = hour % 12;
  return display_hour ? display_hour : 12;
}

// Returns the absolute value of number
int absolute(int number) { if (number < 0) { return -number; } return number; }

// Returns Timezone Offset
static int get_timezone_offset() {
    int timezoneoffset = 0;
    time_t now = time(NULL);

    if (clock_is_timezone_set()) {
      struct tm *tick_time = localtime(&now);
      struct tm *gm_time = gmtime(&now);

      timezoneoffset = 60 * (60 * (24 * (tick_time->tm_wday - gm_time->tm_wday) + tick_time->tm_hour - gm_time->tm_hour) + tick_time->tm_min - gm_time->tm_min);

			if (debug) {
				APP_LOG(APP_LOG_LEVEL_DEBUG, "WD LT %d", tick_time->tm_wday);
				APP_LOG(APP_LOG_LEVEL_DEBUG, "WD GM %d", gm_time->tm_wday);
				APP_LOG(APP_LOG_LEVEL_DEBUG, "HR LT %d", tick_time->tm_hour);
				APP_LOG(APP_LOG_LEVEL_DEBUG, "HR GM %d", gm_time->tm_hour);
				APP_LOG(APP_LOG_LEVEL_DEBUG, "MN LT %d", tick_time->tm_min);
				APP_LOG(APP_LOG_LEVEL_DEBUG, "MN GM %d", gm_time->tm_min);
				APP_LOG(APP_LOG_LEVEL_DEBUG, "OFFSET HR %d", timezoneoffset / 60);
				APP_LOG(APP_LOG_LEVEL_DEBUG, "OFFSET TS %d", timezoneoffset);
			}

      // Correct for transitions at the end of the week.
      if (timezoneoffset > SECONDS_IN_WEEK/2) timezoneoffset -= SECONDS_IN_WEEK;
      if (timezoneoffset < -SECONDS_IN_WEEK/2) timezoneoffset += SECONDS_IN_WEEK;

      timezoneoffset = timezoneoffset / 60;

    }
    return timezoneoffset;
}

// Custom Vibe strength pulses. Currently can only do identical pulses in a row.
// Pebble Vibe limits: maximum 265 (or so) segments and 10 seconds.
void vibes_pwm(int8_t strength, uint16_t duration, int count, int delay) {

	uint16_t totalSegments = 0; 
	uint16_t currentDuration = 0;
	uint16_t totalVibeDuration = duration * count;	// Ignoring delay times
	uint8_t SCALE = (totalVibeDuration/1200)+1; // We use this to always keep under 265 segments.
	
	if (strength == 0 || count == 0) {
		// Why did you ask us to do nothing?
		vibes_cancel();
		return;
	}	
	else if (strength >= 10) {
		// Full strenght, so we utilize the built in functions for speed and reliability
		totalSegments = count*2;
		uint32_t pwm_segments[totalSegments];		
		if (DEBUG) APP_LOG(APP_LOG_LEVEL_INFO, "TotalSegments: %d", totalSegments);
		for (uint16_t i = 0; i < totalSegments; i+=2) {
			if (DEBUG) APP_LOG(APP_LOG_LEVEL_INFO, "%d: %d,%d", i, duration, delay);
			pwm_segments[i] = duration;
			pwm_segments[i+1] = delay;
		}
		vibes_cancel();
		VibePattern pwm_pat = {.durations = pwm_segments, .num_segments = totalSegments};
		vibes_enqueue_custom_pattern(pwm_pat);
	}
	else {
		// And here's where the fun starts
		uint16_t loopSegments = duration/(5*SCALE);
		if (loopSegments % 2 != 0) loopSegments++;
		totalSegments = loopSegments*(count)+(count*2); // We need to make space for the delays too.
		
		// vibes_enqueue_custom_pattern has a hard limit of under 300 segments.
		if (totalSegments >= 265) {
			APP_LOG(APP_LOG_LEVEL_ERROR, "VibePattern segment count over 265 (%d, dur %d, scale %d), we will crash if we vibe!", totalSegments, totalVibeDuration, SCALE);
			vibes_long_pulse(); // We were asked to vibe for a reason so lets do at least something.
			return;
		}
		APP_LOG(APP_LOG_LEVEL_INFO, "(%d, dur %d, scale %d)", totalSegments, totalVibeDuration, SCALE);
		uint32_t pwm_segments[totalSegments];
		uint16_t currentSegment = 0;
		//if (DEBUG) APP_LOG(APP_LOG_LEVEL_INFO, "Segments: loop %d, total %d, size: %d, scale: %d", (int) loopSegments, (int) totalSegments, (int) sizeof(pwm_segments), SCALE);
		
		for (uint8_t j = 0; j < count; j++) {
			for(uint16_t i = 0; i  < loopSegments/2; i++) {
				if (currentDuration+(10*SCALE) <= 9999) {
					//if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "%d (%d): %d, %d",i, loopSegments, currentSegment, currentSegment+1);
					currentDuration = currentDuration+(10*SCALE);
					pwm_segments[currentSegment] = strength*SCALE;
					pwm_segments[currentSegment+1] = 10*SCALE - strength*SCALE;
					currentSegment = currentSegment+2;
				}
				else APP_LOG(APP_LOG_LEVEL_ERROR, "Vibe pattern too long!");
			}
			if (count > 1) {
				if (currentDuration+delay <= 9999) {
					//if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Adding delay to %d and %d", currentSegment, currentSegment+1);
					currentDuration = currentDuration+delay;
					pwm_segments[currentSegment] = 0;
					pwm_segments[currentSegment+1] = delay;
					currentSegment = currentSegment+2;
				}
				else APP_LOG(APP_LOG_LEVEL_ERROR, "Vibe pattern too long!");
			}
		}
		vibes_cancel();
		VibePattern pwm_pat = {.durations = pwm_segments, .num_segments = totalSegments};
		vibes_enqueue_custom_pattern(pwm_pat);
	}
}

// UI helper methods
// Update the main time layer
static void display_time(struct tm *tick_time) {
    if (text_layer != NULL) {
      static char time_text[] = "00:00";
      if (clock_is_24h_style()) { strftime(time_text, sizeof(time_text), "%R", tick_time); }
			else { strftime(time_text, sizeof(time_text), "%r", tick_time); }
      text_layer_set_text(text_layer, time_text);
    }
}

// Display pause time
static void display_pause() {
  if (pause_layer != NULL) {
    time_t now = time(NULL);

    int timezoneoffset = get_timezone_offset();

    int pause_time = (suspend_till_ts - now) / 60;
    int pause_time_modulo = (suspend_till_ts - now) % 60;

    if (pause_time_modulo > 0) {
      pause_time = pause_time + 1;
    }

    pause_time = pause_time - timezoneoffset;

		if (debug) {
			APP_LOG(APP_LOG_LEVEL_DEBUG, "Timezone offset %d", timezoneoffset);
			APP_LOG(APP_LOG_LEVEL_DEBUG, "Pause time %d", pause_time);
			APP_LOG(APP_LOG_LEVEL_DEBUG, "Suspend %d", suspend_till_ts);
			APP_LOG(APP_LOG_LEVEL_DEBUG, "Now %d", (int) time(NULL));
		}

    if (suspend_till_ts == 0 || pause_time <= 0) {
      text_layer_set_text(pause_layer, "");
    } else if (pause_time > 0) {
      static char time_text[] = "00";
      if (pause_time > 99) {
       pause_time = 99;
      }
      snprintf(time_text, sizeof(time_text), "%02d", pause_time);
      text_layer_set_text(pause_layer, time_text);
    }
  }
}



// APP MESSAGE SENDERS

// send data
static void send_data_using_app_message() {
  if (!acked) { return; }
  if (alarm) { return; }

  DictionaryIterator *iter;
  AppMessageResult app_res = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to initialize AppMessage dictionary for data. Err: %d", app_res);
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Error codes: %d, %d, %d, %d, %d, %d, %d", APP_MSG_SEND_TIMEOUT, APP_MSG_SEND_REJECTED, APP_MSG_NOT_CONNECTED, APP_MSG_APP_NOT_RUNNING, APP_MSG_INVALID_ARGS, APP_MSG_BUSY, APP_MSG_BUFFER_OVERFLOW);
    return;
  }

/*	
	if (pending_values_count == 1) {
		// Use old message for single value batches, so that we are backwards compatible
		Tuplet value = TupletInteger(MSG_KEY_ACCEL, pending_values[0]);
		DictionaryResult res = dict_write_tuplet(iter, &value);
		if (res != DICT_OK) {
			APP_LOG(APP_LOG_LEVEL_ERROR, "Dict write failed with err: %d", res);
			return;
		}
	} else {
*/
    uint8_t out_data[pending_values_count * 2];
    for (int i = 0; i < pending_values_count; i++) {
      out_data[2 * i] = pending_values[i] / 127;
      out_data[2 * i + 1] = pending_values[i] % 127;
    }
    Tuplet value = TupletBytes(MSG_KEY_ACCEL_BATCH, (uint8_t*)out_data, pending_values_count * 2);
    DictionaryResult res = dict_write_tuplet(iter, &value);
    if (res != DICT_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Dict batch write failed with err: %d", res);
      return;
    }

    uint8_t out_data_new[pending_values_count * 2];
    for (int i = 0; i < pending_values_count; i++) {
      out_data_new[2 * i] = pending_values_new[i] / 127;
      out_data_new[2 * i + 1] = pending_values_new[i] % 127;
    }
    Tuplet value2 = TupletBytes(MSG_KEY_ACCEL_BATCH_NEW, (uint8_t*)out_data_new, pending_values_count * 2);
    DictionaryResult res2 = dict_write_tuplet(iter, &value2);
    if (res2 != DICT_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Dict batch write failed with err: %d", res2);
      return;
    }

//  }

  if (hr) {
    #if defined(PBL_HEALTH)
     #if PBL_API_EXISTS(health_service_peek_current_value)
        current_hr = (int) health_service_peek_current_value(HealthMetricHeartRateBPM);
        Tuplet hr_value = TupletInteger(MSG_KEY_HR, current_hr);
        DictionaryResult res2 = dict_write_tuplet(iter, &hr_value);
        if (res2 != DICT_OK) {
          APP_LOG(APP_LOG_LEVEL_ERROR, "Dict batch write failed with err: %d", res2);
          return;
        }
      #endif
    #endif
  }

	if (app_message_outbox_send() == APP_MSG_OK) {
    acked = false;
    pending_values_count = 0;
  }
}

// resend action message later on
static void action_acked_timer_callback(void *data) {
  if (postponed_action != -1) {
    action_acked_timer = NULL;
    send_action_using_app_message(postponed_action);
  }
}

// resend action message later on
static void resend_token_callback(void *data) {
  send_timeline_token(timeline_token);
}

// send action
static void send_action_using_app_message(int action) {
  if (!acked) {
    postponed_action = action;
    APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to send action, postponed %d", action);
    action_acked_timer = app_timer_register(1000, action_acked_timer_callback, NULL);
    return;
  }

  DictionaryIterator *iter;
  AppMessageResult app_res = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to initialize AppMessage dictionary for action. Err: %d", app_res);
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Error codes: %d, %d, %d, %d, %d, %d, %d", APP_MSG_SEND_TIMEOUT, APP_MSG_SEND_REJECTED, APP_MSG_NOT_CONNECTED, APP_MSG_APP_NOT_RUNNING, APP_MSG_INVALID_ARGS, APP_MSG_BUSY, APP_MSG_BUFFER_OVERFLOW);
    return;
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Action message: %d", action);
  Tuplet value = TupletInteger(action, 1);
  DictionaryResult res = dict_write_tuplet(iter, &value);
  if (res != DICT_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Dict write failed with err: %d", res);
    return;
  }


  if (app_message_outbox_send() == APP_MSG_OK) {
    acked = false;
    postponed_action = -1;
  } else {
      action_acked_timer = app_timer_register(1000, action_acked_timer_callback, NULL);
  }

}

// Send timeline token to Phone
static void send_timeline_token(char* token) {
  if (!acked) {
    timeline_token = token;
    APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to send token, postponed %s", token);
    action_acked_timer = app_timer_register(1000, resend_token_callback, NULL);
    return;
  }

  DictionaryIterator *iter;
  AppMessageResult app_res = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to initialize AppMessage dictionary for action. Err: %d", app_res);
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Error codes: %d, %d, %d, %d, %d, %d, %d", APP_MSG_SEND_TIMEOUT, APP_MSG_SEND_REJECTED, APP_MSG_NOT_CONNECTED, APP_MSG_APP_NOT_RUNNING, APP_MSG_INVALID_ARGS, APP_MSG_BUSY, APP_MSG_BUFFER_OVERFLOW);
    return;
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Timeline token: %s", token);
  Tuplet value = TupletCString(MESSAGE_KEY_timeline_token, token);
  DictionaryResult res = dict_write_tuplet(iter, &value);
  if (res != DICT_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Dict write failed with err: %d", res);
    return;
  }

  if (app_message_outbox_send() == APP_MSG_OK) { acked = false; }
	else { action_acked_timer = app_timer_register(1000, resend_token_callback, NULL); }
}

// Stops local alarm
static void stopAlarm() {
    alarm = false;
    alarm_counter = 0;
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Stopping local alarm vibe");
}

// Handles single click events
static void single_click_handler(ClickRecognizerRef recognizer, void *context) {	
	switch (click_recognizer_get_button_id(recognizer)) {
		case BUTTON_ID_UP:
			// Even snooze will stop current alarm. We do not support really snoozing on watch without phone.
			scheduled_alarm_ts = 0;
			if (alarm) {
				if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "SNOOZE");
				send_action_using_app_message(MSG_KEY_SNOOZE);
					stopAlarm();
					hide_ab_with_next_tick = true;
			}
			break;
		case BUTTON_ID_DOWN:
			scheduled_alarm_ts = 0;
			if (alarm) {
				if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "DISMISS");
				send_action_using_app_message(MSG_KEY_DISMISS);
			//    stopAlarm();
			//    hide_ab_with_next_tick = true;
			}
			break;
		case BUTTON_ID_SELECT:
			if (!alarm) {
				send_action_using_app_message(MSG_KEY_PAUSE);
			} else { if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Not doing pause alarm active"); }
			break;
		default: break;
	}
}

// Handles long click events
static void long_click_handler(ClickRecognizerRef recognizer, void *context) {
	switch (click_recognizer_get_button_id(recognizer)) {
		case BUTTON_ID_SELECT:
			if (!alarm) {
				send_action_using_app_message(MSG_KEY_RESUME);
			} else {
				if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Not doing resume alarm active");
			}
			break;
		default: break;
	}
}

// Handles multi click events
static void multi_click_handler(ClickRecognizerRef recognizer, void *context) {
	switch (click_recognizer_get_button_id(recognizer)) {
		case BUTTON_ID_BACK:
			window_stack_pop_all(true);
			break;
		default: break;
	}
}

// Button config for the action bar
static void config_provider_ab(void *ctx) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Click provider");
  window_single_click_subscribe(BUTTON_ID_UP, single_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, single_click_handler);
}

// Button config for other events
static void config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, single_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, long_click_handler, NULL);
	if (conf.doubleTapExit) window_multi_click_subscribe(BUTTON_ID_BACK, 2, 0, 0, true, multi_click_handler);
}

// Create and show the action bar layer
static void action_bar_show(Window *window) {
  action_bar_layer = action_bar_layer_create();
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Click provider setup");
  action_bar_layer_set_click_config_provider(action_bar_layer, config_provider_ab);
  action_bar_layer_set_background_color(action_bar_layer, GColorWhite);

  image_action_snooze = gbitmap_create_with_resource(RESOURCE_ID_ACTION_SNOOZE);
  image_action_dismiss = gbitmap_create_with_resource(RESOURCE_ID_ACTION_DISMISS);

  action_bar_layer_set_icon(action_bar_layer, BUTTON_ID_UP, image_action_snooze);
  action_bar_layer_set_icon(action_bar_layer, BUTTON_ID_DOWN, image_action_dismiss);

  // move may watch out of action bar to keep it readable
  #if defined(PBL_RECT)
    text_layer_set_text_alignment(text_layer, GTextAlignmentLeft);
  #endif

  action_bar_layer_add_to_window(action_bar_layer, window);
}

// Destroy the action bar
static void action_bar_hide(Window *window) {
  if (action_bar_layer) {
    action_bar_layer_remove_from_window(action_bar_layer);
    action_bar_layer_destroy(action_bar_layer);
    gbitmap_destroy(image_action_snooze);
    gbitmap_destroy(image_action_dismiss);
    action_bar_layer = NULL;

    // revert - move may watch out of action bar to keep it readable
    text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);

    window_set_click_config_provider(window, (ClickConfigProvider) config_provider);
  }
}

// Show the bottom alarm bar layer
static void alarm_show(char* text) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Alarm show: %s", text);
	
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
	
	text_layer_set_text(alarm_layer, text);		
	
	int alarm_layer_height = PBL_IF_RECT_ELSE(40,40);
  int alarm_layer_top = PBL_IF_RECT_ELSE(10,6);
	
	layer_set_frame(text_layer_get_layer(alarm_parent_layer), GRect(0, bounds.size.h - alarm_layer_height, bounds.size.w, bounds.size.h));
	
	
		if 			(conf.alarmFont == 1) layer_set_frame(text_layer_get_layer(alarm_layer), GRect(22, -2, bounds.size.w - 22, alarm_layer_height));
		else if (conf.alarmFont == 3) layer_set_frame(text_layer_get_layer(alarm_layer), GRect(22, 2, bounds.size.w - 22, alarm_layer_height));
		else if (conf.alarmFont == 4) layer_set_frame(text_layer_get_layer(alarm_layer), GRect(22, -1, bounds.size.w - 22, alarm_layer_height));
		else layer_set_frame(text_layer_get_layer(alarm_layer), GRect(22, 0, bounds.size.w - 22, alarm_layer_height));
	
	int text_size = text_layer_get_content_size(alarm_layer).w;
	layer_set_frame(bitmap_layer_get_layer(image_layer_alarm), GRect(((bounds.size.w - 22 - text_size) / 2), alarm_layer_top, 20, 20));
	
	layer_set_hidden(text_layer_get_layer(alarm_parent_layer), false);
}

// Hide the alarm bar
static void alarm_hide() {	
	layer_set_hidden(text_layer_get_layer(alarm_parent_layer), true);
}

float asqrt(const float num) {
    const uint MAX_STEPS = 40;
    const float MAX_ERROR = 0.001;
    float answer = num;
    float ans_sqr = answer * answer;
    uint step = 0;
    while((ans_sqr - num > MAX_ERROR) && (step++ < MAX_STEPS)) {
        answer = (answer + (num / answer)) / 2;
        ans_sqr = answer * answer;
    }
    return answer;
}

// Business logic
void store_max(AccelData data) {
  if (last) {
    int sum = absolute(last_x - data.x) + absolute(last_y - data.y) + absolute(last_z - data.z);
    int sum_new = asqrt(data.x * data.x + data.y * data.y + data.z * data.z);

    if (sum > max_sum) {
			max_sum = sum;
		}
    if (sum_new > max_sum_new) {
        max_sum_new = sum_new;
    }
  }
  last = true;
  last_x = data.x;
  last_y = data.y;
  last_z = data.z;
}

// HANDLERS

// Clay Settings handlers
// Save settings
static void save_settings() {
  int ret = persist_write_data(SETTINGS_KEY, &conf, sizeof(conf));
	if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Conf - Persistent Settings Saved | %d / Vibe: %d", ret, conf.alarmVibe);
	
	// Pop the window to restart with new configs
	window_stack_pop_all(true);
	window_stack_push(window, true);
	send_action_using_app_message(MSG_KEY_START_APP);	
}

// Load settings
static void load_settings() {
	default_settings();
	int ret = persist_read_data(SETTINGS_KEY, &conf, sizeof(conf));
	if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Conf - Persistent Settings Loaded | %d", ret);
}

// minute tick handler
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  display_time(tick_time);
  display_pause();
}

// accel data handler
void accel_data_handler(AccelData *data, uint32_t num_samples) {
  for (uint32_t i = 0; i < num_samples; i++) {
    store_max(data[i]);
  }
}

// message handlers

static bool is_connection_dead() {
    return last_received_msg_ts != 0 && (time(NULL) - last_received_msg_ts > NO_MESSAGE_DEAD_TIME);
}

// Outgoing message to phone delivered.
void out_sent_handler(DictionaryIterator *sent, void *context) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Data acked successfully.");
  acked = true;
  // Positive ack from phone counts as incoming message (for the purpose of liveness detection).
  last_received_msg_ts = time(NULL);
}

// Outgoing message to phone failed.
void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Data acked failed. Err: %d", reason);
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Error codes: %d, %d, %d, %d, %d, %d, %d", APP_MSG_SEND_TIMEOUT, APP_MSG_SEND_REJECTED, APP_MSG_NOT_CONNECTED, APP_MSG_APP_NOT_RUNNING, APP_MSG_INVALID_ARGS, APP_MSG_BUSY, APP_MSG_BUFFER_OVERFLOW);
  acked = true;
}

// Incoming message received.
void in_received_handler(DictionaryIterator *received, void *context) {
  // APP_LOG(APP_LOG_LEVEL_ERROR, "Received data");
	if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Received message.");
  last_received_msg_ts = time(NULL);

  Tuple *alarm_start = dict_find(received, MSG_KEY_ALARM_START);
  Tuple *alarm_stop = dict_find(received, MSG_KEY_ALARM_STOP);
  Tuple *alarm_hour_of_day = dict_find(received, MSG_KEY_ALARM_HOUR_OF_DAY);
  Tuple *alarm_minute = dict_find(received, MSG_KEY_ALARM_MINUTE);
  Tuple *batch_size = dict_find(received, MSG_KEY_SET_BATCH_SIZE);
  Tuple *suspend_mode = dict_find(received, MSG_KEY_SET_SUSPEND_STATUS);
  Tuple *suspend_ts = dict_find(received, MSG_KEY_SUSPEND_TILL_TS);
  Tuple *hint = dict_find(received, MSG_KEY_HINT);
  Tuple *alarm_ts = dict_find(received, MSG_KEY_ALARM_TS);
  Tuple *enable_hr = dict_find(received, MSG_KEY_ENABLE_HR);
	Tuple *timeline_token = dict_find(received, MESSAGE_KEY_timeline_token);

  if (alarm_start) {
    if (alarm_start->length == 4) {
        alarm_delay = alarm_start->value->uint32;
    } else {
        alarm_delay = 0;
    }

    action_bar_show(window);
    alarm = true;
    // Ok, we are firing this alarm -> We do not need to fire it from watch -> reset it.
    scheduled_alarm_ts = 0;
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Alarm start");
    return;
  }

  if (alarm_stop) {
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Alarm stop message");
    action_bar_hide(window);
    stopAlarm();
    return;
  }

  if (alarm_hour_of_day && alarm_minute) {
    static char time_text[] = "00:00";
    snprintf(time_text, sizeof(time_text), "%02d:%02d", get_display_hour(alarm_hour_of_day->value->uint8), alarm_minute->value->uint8);
    alarm_show(time_text);
  } else if (alarm_ts) {
    // We have timestamp, but not time => Alarm is more than 24h away => do not show it.
    alarm_hide();
  }

  if (alarm_ts) {
    scheduled_alarm_ts = alarm_ts->value->uint32;
    scheduled_alarm_ts = scheduled_alarm_ts - (get_timezone_offset() * 60);

    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Next alarm ts %d", scheduled_alarm_ts);
  }

  if (batch_size) {
    new_app = true;  // This is supported only by new app
    requested_buffer_size = batch_size->value->uint8;
  }

  if (suspend_mode) {
    suspended = suspend_mode->value->uint8;
    if (!suspended) {
       suspend_till_ts = 0;
       if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Suspend %d", suspend_till_ts);
       display_pause();
    }
  }

  if (suspend_ts) {
    suspend_till_ts = suspend_ts->value->uint32;
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Suspend %d", suspend_till_ts);
    display_pause();
  }

  if (enable_hr) {
      if (hr == false) {
          hr = true;
          // Set min heart rate sampling period
          if (hr) {
              #if defined(PBL_HEALTH)
                  #if PBL_API_EXISTS(health_service_set_heart_rate_sample_period)
                    health_service_set_heart_rate_sample_period(120);
                  #endif
              #endif
          }
      }
  }

  if (hint) {
		hint_repeat = hint->value->uint8;
		if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Hint vibe event?");
      switch (hint_repeat) {
        case 1: vibes_long_pulse(); break;
        case 2: vibes_enqueue_custom_pattern(pat2); break;
        case 3: vibes_enqueue_custom_pattern(pat3); break;
        case 4: vibes_enqueue_custom_pattern(pat4); break;
        case 5: vibes_enqueue_custom_pattern(pat5); break;
        case 10:vibes_enqueue_custom_pattern(pat10); break;
        default: vibes_long_pulse();
      }
  }

  if (timeline_token) {
    send_timeline_token(timeline_token->value->cstring);
  }
	
	// Configuration handling	
	Tuple *t_uiConf[16];
	for (int i = 0; i < 16; i++){
		t_uiConf[i] = dict_find(received, MESSAGE_KEY_uiConf + i);
	}
	
	if (t_uiConf[0]) conf.doubleTapExit = t_uiConf[0]->value->int32 == 1;
	if (t_uiConf[1]) conf.enableBackground = t_uiConf[1]->value->int32 == 1;
	if (t_uiConf[2]) conf.enableInfo = t_uiConf[2]->value->int32 == 1;
	if (t_uiConf[3]) conf.enableHeartrate = t_uiConf[3]->value->int32 == 1;
	if (t_uiConf[4]) conf.timeFont = atoi(t_uiConf[4]->value->cstring);
	if (t_uiConf[5]) conf.alarmFont = atoi(t_uiConf[5]->value->cstring);
	
	Tuple *t_vibeConf[8];
	for (int i = 0; i < 16; i++){
		t_vibeConf[i] = dict_find(received, MESSAGE_KEY_vibeConf + i);
	}
	
	if (t_vibeConf[0]) conf.alarmVibe = atoi(t_vibeConf[0]->value->cstring);
	
	// Don't bother saving every communication, only if we got config settings.
	if (t_uiConf[0] && t_vibeConf[0]) {
		save_settings();
	} 
}

// Incoming message dropped.
void in_dropped_handler(AppMessageResult reason, void *context) { APP_LOG(APP_LOG_LEVEL_ERROR, "Dropped message");}

// Builds the toggle layer
static void info_update_proc(Layer *layer, GContext *ctx) {
	if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "info_update_proc");
	GRect bounds = layer_get_bounds(layer);
	
	if (conf.enableInfo) {
		// Bluetooth
		gbitmap_set_bounds(icon_bt, GRect(bluetoothState ? 0 : 16,0,16,16));
		graphics_draw_bitmap_in_rect(ctx, icon_bt, PBL_IF_RECT_ELSE(GRect(0,0,16,16), GRect(8,bounds.size.w/2-32,16,16)));

		// QuietTime
		gbitmap_set_bounds(icon_qt, GRect(quietTimeState ? 0 : 16,16,16,16));
		graphics_draw_bitmap_in_rect(ctx, icon_qt, PBL_IF_RECT_ELSE(GRect(16,0,16,16), GRect(8,bounds.size.w/2+16,16,16)));

		// Battery batteryLevel / BatteryState
		graphics_draw_bitmap_in_rect(ctx, icon_bat, PBL_IF_RECT_ELSE(GRect(bounds.size.w-32,0,32,16), GRect(5,(bounds.size.w/2)-16,16,32)));
		graphics_context_set_fill_color(ctx, GColorWhite);
		graphics_fill_rect(ctx, PBL_IF_RECT_ELSE( GRect(118,5,(batteryLevel*2),6), GRect(10,(bounds.size.h/2)-10+(20-batteryLevel*2),6,(batteryLevel*2)) ), 1, GCornersAll);
	}
	#if defined(PBL_HEALTH)
	if (conf.enableHeartrate) {
			graphics_draw_bitmap_in_rect(ctx, icon_heart, GRect(48,3,16,16));
			graphics_context_set_text_color(ctx, GColorWhite);
			snprintf(hr_text_buffer, sizeof(hr_text_buffer), "%3d", (int) bpmValue);
			graphics_draw_text(ctx, hr_text_buffer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(bounds.size.w/2,-7,32,16), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	}
	#endif
}

// Monitor battery status
static void handle_battery(BatteryChargeState charge_state) {	
	if (charge_state.is_charging) batteryState = 1;
	else if (charge_state.is_plugged) batteryState = 2;
	else batteryState = 0;	
	batteryLevel = ((charge_state.charge_percent)/10);
	//layer_mark_dirty(info_layer);
}

// Monitor BT status
static void handle_bluetooth(bool connected) {
	if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_bluetooth: %d", connected);
  bluetoothState = connected;
	//layer_mark_dirty(info_layer);
}

// Handle Health Events
#if defined(PBL_HEALTH)
#if PBL_API_EXISTS(health_service_set_heart_rate_sample_period)
static void handle_health(HealthEventType event, void *context) {
		if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "update_health : %d", event);
		if (event == HealthEventSignificantUpdate || event == HealthEventHeartRateUpdate) {
			bpmValue = health_service_peek_current_value(HealthMetricHeartRateBPM);
			if (bpmValue != bpmValueOld) {
				bpmValueOld = bpmValue;
				//layer_mark_dirty(info_layer);
			}
		}
}
#endif
#endif

static void timer_callback(void *data) {
  if (pending_values_count < MAX_QUEUED_VALUES) {
      if (suspended) {
        max_sum = -10;  // Has to be in sync with Sleep (will lead to -0.01f)
        max_sum_new = -10;  // Has to be in sync with Sleep (will lead to -0.01f)
      }
      pending_values[pending_values_count] = max_sum;
      pending_values_new[pending_values_count] = max_sum_new;
      pending_values_count++;
      max_sum = 0;
      max_sum_new = 0;

      if (pending_values_count >= requested_buffer_size) {
        send_data_using_app_message();
      }
  } else {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Dropping value update due to overfull out buffer");
  }

  time_t now = time(NULL);
//   if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Next alarm ts %d vs now %d", scheduled_alarm_ts, now);
  if (alarm ||
     // Fallback alarm fired just because it was scheduled from watch.
     // + 3 sec so that we give phone some time to fire alarm.
     (scheduled_alarm_ts != 0 && (scheduled_alarm_ts + 3) < now)) {

    if (!alarm) {
        // Alarm == false -> alarm fired in fallback mode due to disconnection => we need to show action-bar.
        alarm = true;
        action_bar_show(window);
    }

    alarm_counter++;

    int alarm_time_elapsed = ((alarm_counter - 1) * SAMPLING_TIMER);
		
    if (alarm_delay > -1 && alarm_time_elapsed >= alarm_delay) {
			if (conf.alarmVibe == 1) {
				if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Vibing for alarm using GentlerPWM");
				// Using GentlerPWM vibes
				if (alarm_time_elapsed - alarm_delay >= 100000) { 
					vibes_enqueue_custom_pattern(pat5);
        } else if (alarm_time_elapsed - alarm_delay >= 80000) {
          vibes_pwm(8,250,6,125);
        } else if (alarm_time_elapsed - alarm_delay >= 60000) {
          vibes_pwm(6,650,3,400);
        } else if (alarm_time_elapsed - alarm_delay >= 40000) {
          vibes_pwm(5,400,2,1500);
				} else if (alarm_time_elapsed - alarm_delay >= 20000) {
					vibes_pwm(4,250,2,1750);
        } else {
          vibes_pwm(3,250,1,0);
        }
			}
			else {
				if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, "Vibing for alarm using default patterns");
				// Using default vibes
        if (alarm_time_elapsed - alarm_delay >= 80000) {
          vibes_enqueue_custom_pattern(pat5);
        } else if (alarm_time_elapsed - alarm_delay >= 40000) {
          vibes_enqueue_custom_pattern(pat);
        } else if (alarm_time_elapsed - alarm_delay >= 20000) {
          vibes_long_pulse();
        } else {
          vibes_short_pulse();
        }
			}
    }
  } else {
    alarm_counter = 0;
  }

  if (hide_ab_with_next_tick) {
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Hiding AB on tick");
    action_bar_hide(window);
    hide_ab_with_next_tick = false;
  }

  timer = app_timer_register(new_app ? SAMPLING_TIMER : SAMPLING_TIMER_BACKWARDS, timer_callback, NULL);
}


// USER INTERFACE INIT

// window load
static void window_load(Window *window) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Window load");

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorBlack);

	// Background layer
  image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG);
  image_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(image_layer, image);
  bitmap_layer_set_alignment(image_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(image_layer));

	// Pause bitmap layer
  image_action_pause = gbitmap_create_with_resource(RESOURCE_ID_ACTION_PAUSE);
  image_layer_pause = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(image_layer_pause, image_action_pause);
  bitmap_layer_set_alignment(image_layer_pause, GAlignRight);
  layer_add_child(window_layer, bitmap_layer_get_layer(image_layer_pause));

	// Time text layer
  text_layer = text_layer_create(GRect(0,PBL_IF_RECT_ELSE(15,3), bounds.size.w, 80));
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
	
	font_w800_42 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_HOUR_35));

  #if defined(PBL_RECT)
		if 			(conf.timeFont == 1) text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
		else if (conf.timeFont == 2) text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
		else if (conf.timeFont == 4) text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
		else if (conf.timeFont == 5) {
			layer_set_frame(text_layer_get_layer(text_layer), GRect(0, 25, bounds.size.w, 80 ));
			text_layer_set_font(text_layer, font_w800_42);
		}
		else 
		{
			text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
			layer_set_frame(text_layer_get_layer(text_layer), GRect(0, 10, bounds.size.w, 80 ));
		}

  #elif defined(PBL_ROUND)
    text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  #endif

  text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

	// Pause layer
  pause_layer = text_layer_create(GRect(0, bounds.size.h / 2 - 10, bounds.size.w - 20, bounds.size.h / 2 + 10));
  text_layer_set_text_alignment(pause_layer, GTextAlignmentRight);
  #if defined(PBL_RECT)
		text_layer_set_font(pause_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  #elif defined(PBL_ROUND)
    text_layer_set_font(pause_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  #endif
  text_layer_set_background_color(pause_layer, GColorClear);
  text_layer_set_text_color(pause_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(pause_layer));

  // Avoids a blank screen on watch start.
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  display_time(tick_time);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	
	// Alarm layer
	alarm_parent_layer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));

	alarm_layer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
	text_layer_set_text_alignment(alarm_layer, GTextAlignmentCenter);		

	#if defined(PBL_RECT)
		if 			(conf.alarmFont == 1) text_layer_set_font(alarm_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
		else if (conf.alarmFont == 3) text_layer_set_font(alarm_layer, fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS));
		else if (conf.alarmFont == 4) text_layer_set_font(alarm_layer, fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS));
		else text_layer_set_font(alarm_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
			
		text_layer_set_background_color(alarm_parent_layer, GColorWhite);
		text_layer_set_overflow_mode(alarm_layer, GTextOverflowModeWordWrap);
		text_layer_set_text_color(alarm_layer, GColorBlack);
	#elif defined(PBL_ROUND)
		text_layer_set_font(alarm_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
		text_layer_set_background_color(alarm_parent_layer, GColorClear);
		text_layer_set_background_color(alarm_layer, GColorClear);
		text_layer_set_text_color(alarm_layer, GColorWhite);
	#endif

	layer_add_child(text_layer_get_layer(alarm_parent_layer), text_layer_get_layer(alarm_layer));

	image_alarm = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ALARM);
	image_layer_alarm = bitmap_layer_create(GRect(0, 0, 20, 20));
	bitmap_layer_set_bitmap(image_layer_alarm, image_alarm);
	bitmap_layer_set_alignment(image_layer_alarm, GAlignLeft);
	layer_add_child(text_layer_get_layer(alarm_parent_layer), bitmap_layer_get_layer(image_layer_alarm));

	layer_add_child(window_layer, text_layer_get_layer(alarm_parent_layer));
	layer_set_hidden(text_layer_get_layer(alarm_parent_layer), true);

	// Create Info Layer
	info_layer = layer_create(GRect(0,0,bounds.size.w,bounds.size.h));
	layer_set_update_proc(info_layer, info_update_proc);
	layer_add_child(window_layer, info_layer);
	
	// Create Info images
	icon_sprites = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPRITES);
	icon_bt = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(0,0,32,16));
	icon_qt = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(0,16,32,16));
	icon_bat = gbitmap_create_as_sub_bitmap(icon_sprites, PBL_IF_RECT_ELSE(GRect(0,32,32,16),GRect(16,48,16,32)));
	icon_heart = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(0,48,16,16));
		
	// If infobar is enabled, we register the handlers	
	if (conf.enableInfo) {
		battery_state_service_subscribe(handle_battery);
		handle_battery(battery_state_service_peek());	
		connection_service_subscribe((ConnectionHandlers) {.pebble_app_connection_handler = handle_bluetooth});
		handle_bluetooth(bluetooth_connection_service_peek());	
		quietTimeState = quiet_time_is_active();
	}
	
	if (conf.enableHeartrate) {
			// subscribe to event handlers
			#if defined(PBL_HEALTH)
				#if PBL_API_EXISTS(health_service_set_heart_rate_sample_period)
					health_service_events_subscribe(handle_health, NULL);
				#endif	
			#endif
		}
	
	// Show or hide layers based on settings
	layer_set_hidden(bitmap_layer_get_layer(image_layer), !conf.enableBackground);
	layer_set_hidden(info_layer, (!conf.enableInfo && !conf.enableHeartrate));
}

// window unload
static void window_unload(Window *window) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Window unload");

  text_layer_destroy(text_layer);
  text_layer_destroy(pause_layer);
  bitmap_layer_destroy(image_layer_pause);
  bitmap_layer_destroy(image_layer);
  gbitmap_destroy(image);
  gbitmap_destroy(image_action_pause);
  action_bar_hide(window);
  alarm_hide();
  tick_timer_service_unsubscribe();
	
	// Alarm bar
	gbitmap_destroy(image_alarm);
	bitmap_layer_destroy(image_layer_alarm);
	text_layer_destroy(alarm_layer);
	text_layer_destroy(alarm_parent_layer);
	
	// destroy info layer stuff	
	gbitmap_destroy(icon_bt);
	gbitmap_destroy(icon_qt);
	gbitmap_destroy(icon_bat);
	gbitmap_destroy(icon_heart);
	gbitmap_destroy(icon_sprites);	
	fonts_unload_custom_font(font_w800_42);
	
	battery_state_service_unsubscribe();
	connection_service_unsubscribe();
	health_service_events_unsubscribe();

	layer_destroy(info_layer);	
}

static void focus_handler(bool in_focus) {
  if (debug && in_focus) APP_LOG(APP_LOG_LEVEL_DEBUG, "In focus");
  if (debug && !in_focus) APP_LOG(APP_LOG_LEVEL_DEBUG, "NOT In focus");
  if (in_focus && alarm) {
      action_bar_hide(window);
      action_bar_show(window);
  }
}

// INITIALIZATION AND MAIN

static void init(void) {	
	// Load clay settings
	load_settings();
	
  // Initialize the communication channels with phone.
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);

  app_focus_service_subscribe(focus_handler);
  app_message_open(512, 128);

  // Initialize window
  window = window_create();
  // window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  // Initialize accelerometer
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  accel_data_service_subscribe(25, &accel_data_handler);

  // starting main timer for outbound messages
  timer = app_timer_register(SAMPLING_TIMER_BACKWARDS, timer_callback, NULL);

  window_set_click_config_provider(window, (ClickConfigProvider) config_provider);

  send_action_using_app_message(MSG_KEY_START_APP);
}

static void deinit(void) {
  app_message_deregister_callbacks();

  app_focus_service_unsubscribe();

  if (action_acked_timer) {
    app_timer_cancel(action_acked_timer);
  }
  app_timer_cancel(timer);

  accel_data_service_unsubscribe();
	
  // Reset heart rate sampling period to watch-controlled default
  if (hr) {
      #if defined(PBL_HEALTH)
          #if PBL_API_EXISTS(health_service_set_heart_rate_sample_period)
            health_service_set_heart_rate_sample_period(0);
            hr = false;
          #endif
      #endif
  }
	
  window_destroy(window);
}

int main(void) {
  init();
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
  app_event_loop();
  deinit();
}
