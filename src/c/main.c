#include <pebble.h>

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

// Messages coming from js
enum {
    MSG_KEY_TIMELINE_TOKEN = 999,
};

// VARIABLES
static bool last = false;

static bool debug = false;

static bool hr = false;

static int last_x;
static int last_y;
static int last_z;

static int max_sum;
static int max_sum_new;
static int hint_repeat;
static int requested_buffer_size = 1;
static int pending_values_count = 0;
static int pending_values[MAX_QUEUED_VALUES];
static int pending_values_new[MAX_QUEUED_VALUES];

static bool suspended = false;

static int suspend_till_ts = false;

// Time when the next alarm should fire
static int scheduled_alarm_ts = 0;

// Timestamp of last received message from phone.
static int last_received_msg_ts = 0;

static bool acked = true;

static bool new_app = false;

static bool hide_ab_with_next_tick = false;
static bool alarm = false;
static int alarm_counter = 0;
static int alarm_delay = 0;

static int postponed_action = -1;

static char* timeline_token;

static int current_hr = 0;

// info layer vars
bool bluetoothState = false;
bool bluetoothStateOld = false;
bool quietTimeState = false;
bool quietTimeStateOld = false;
unsigned char batteryLevel = 0;
unsigned char batteryState = 0;
HealthValue bpmValue = 0;
static char info_text_buffer[64];

// POINTERS
static AppTimer *timer;
static AppTimer *action_acked_timer;

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

static Layer *image_layer_alarm_ref;
static Layer *alarm_layer_ref;
static Layer *alarm_parent_layer_ref;

static Layer *info_layer;
static TextLayer *info_text_layer;

// Forward declarations
static void alarm_hide();
static void action_bar_hide(Window *window);
static bool is_connection_dead();

static const uint32_t segments[] = { 800, 400, 800, 400, 800 };
VibePattern pat = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};

static const uint32_t segments2[] = { 800, 400, 800 };
VibePattern pat2 = {
  .durations = segments2,
  .num_segments = ARRAY_LENGTH(segments2),
};

static const uint32_t segments3[] = { 800, 400, 800, 400, 800 };
VibePattern pat3 = {
  .durations = segments3,
  .num_segments = ARRAY_LENGTH(segments3),
};

static const uint32_t segments4[] = { 800, 400, 800, 400, 800, 400, 800 };
VibePattern pat4 = {
  .durations = segments4,
  .num_segments = ARRAY_LENGTH(segments4),
};

static const uint32_t segments5[] = { 800, 400, 800, 400, 800, 400, 800, 400, 800 };
VibePattern pat5 = {
  .durations = segments5,
  .num_segments = ARRAY_LENGTH(segments5),
};

static const uint32_t segments10[] = { 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800, 400, 800 };
VibePattern pat10 = {
  .durations = segments10,
  .num_segments = ARRAY_LENGTH(segments10),
};

// Helper methods
static unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) {
    return hour;
  }
  unsigned short display_hour = hour % 12;
  return display_hour ? display_hour : 12;
}

int absolute(int number) {
    if (number < 0) {
        return -number;
    }
    return number;
}

static int get_timezone_offset() {
    int timezoneoffset = 0;
    time_t now = time(NULL);

    if (clock_is_timezone_set()) {
      struct tm *tick_time = localtime(&now);
      struct tm *gm_time = gmtime(&now);

      timezoneoffset = 60 * (60 * (24 * (tick_time->tm_wday - gm_time->tm_wday) + tick_time->tm_hour - gm_time->tm_hour) + tick_time->tm_min - gm_time->tm_min);

      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "WD LT %d", tick_time->tm_wday);
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "WD GM %d", gm_time->tm_wday);
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "HR LT %d", tick_time->tm_hour);
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "HR GM %d", gm_time->tm_hour);
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "MN LT %d", tick_time->tm_min);
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "MN GM %d", gm_time->tm_min);

      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "OFFSET HR %d", timezoneoffset / 60);
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "OFFSET TS %d", timezoneoffset);

      // Correct for transitions at the end of the week.
      if (timezoneoffset > SECONDS_IN_WEEK/2) timezoneoffset -= SECONDS_IN_WEEK;
      if (timezoneoffset < -SECONDS_IN_WEEK/2) timezoneoffset += SECONDS_IN_WEEK;

      timezoneoffset = timezoneoffset / 60;

    }
    return timezoneoffset;
}

// UI helper methods
// update time
static void display_time(struct tm *tick_time) {
    if (text_layer != NULL) {
      static char time_text[] = "00:00";
//      clock_copy_time_string(time_text, sizeof(time_text));

      if (clock_is_24h_style()) {
        strftime(time_text, sizeof(time_text), "%R", tick_time);
      } else {
        strftime(time_text, sizeof(time_text), "%r", tick_time);
      }
      text_layer_set_text(text_layer, time_text);
    }
}

// UI helper methods
// update time
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

    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Timezone offset %d", timezoneoffset);
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Pause time %d", pause_time);
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Suspend %d", suspend_till_ts);
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Now %d", (int) time(NULL));

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
  if (!acked) {
    return;
  }

  if (alarm) {
    return;
  }

  DictionaryIterator *iter;
  AppMessageResult app_res = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to initialize AppMessage dictionary for data. Err: %d", app_res);
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Error codes: %d, %d, %d, %d, %d, %d, %d", APP_MSG_SEND_TIMEOUT, APP_MSG_SEND_REJECTED, APP_MSG_NOT_CONNECTED, APP_MSG_APP_NOT_RUNNING, APP_MSG_INVALID_ARGS, APP_MSG_BUSY, APP_MSG_BUFFER_OVERFLOW);
    return;
  }

//  if (pending_values_count == 1) {
//    // Use old message for single value batches, so that we are backwards compatible
//    Tuplet value = TupletInteger(MSG_KEY_ACCEL, pending_values[0]);
//    DictionaryResult res = dict_write_tuplet(iter, &value);
//    if (res != DICT_OK) {
//      APP_LOG(APP_LOG_LEVEL_ERROR, "Dict write failed with err: %d", res);
//      return;
//    }
//  } else {
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
//  APP_LOG(APP_LOG_LEVEL_DEBUG, "Tuplet write result: %d", res);

  if (app_message_outbox_send() == APP_MSG_OK) {
    acked = false;
    pending_values_count = 0;
  }


}

static void send_action_using_app_message(int action);

// resend action message later on
static void action_acked_timer_callback(void *data) {
  if (postponed_action != -1) {
    action_acked_timer = NULL;
    send_action_using_app_message(postponed_action);
  }
}

static void send_timeline_token(char* token);

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

// send action
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
  Tuplet value = TupletCString(MSG_KEY_TIMELINE_TOKEN, token);
  DictionaryResult res = dict_write_tuplet(iter, &value);
  if (res != DICT_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Dict write failed with err: %d", res);
    return;
  }

  if (app_message_outbox_send() == APP_MSG_OK) {
    acked = false;
  } else {
      action_acked_timer = app_timer_register(1000, resend_token_callback, NULL);
  }

}

static void stopAlarm() {
    alarm = false;
    alarm_counter = 0;
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Stopping local alarm vibe");
}

// ACTIONBAR

/*
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "SNOOZE");

  // Even snooze will stop current alarm. We do not support really snoozing on watch without phone.
  scheduled_alarm_ts = 0;
  if (alarm) {
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "SNOOZE");
    send_action_using_app_message(MSG_KEY_SNOOZE);
      stopAlarm();
      hide_ab_with_next_tick = true;
    }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "DISMISS");

  scheduled_alarm_ts = 0;
  if (alarm) {
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "DISMISS");
    send_action_using_app_message(MSG_KEY_DISMISS);

//    stopAlarm();
//    hide_ab_with_next_tick = true;
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!alarm) {
    send_action_using_app_message(MSG_KEY_PAUSE);
  } else {
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Not doing pause alarm active");
  }
}

// Capture the back button to stop quitting the app accidentally, but do so on double tap
static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Back click provider");
  window_stack_pop_all(true);
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!alarm) {
    send_action_using_app_message(MSG_KEY_RESUME);
  } else {
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Not doing resume alarm active");
  }
}
*/


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

static void multi_click_handler(ClickRecognizerRef recognizer, void *context) {
	switch (click_recognizer_get_button_id(recognizer)) {
		case BUTTON_ID_BACK:
			window_stack_pop_all(true);
			break;
		default: break;
	}
}

static void config_provider_ab(void *ctx) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Click provider");
  window_single_click_subscribe(BUTTON_ID_UP, single_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, single_click_handler);
}

static void config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, single_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, long_click_handler, NULL);
	window_multi_click_subscribe(BUTTON_ID_BACK, 2, 0, 0, true, multi_click_handler);
//  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
//  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

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

// action bar
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

static void alarm_show(char* text) {
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Alarm show: %s", text);

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "Bounds: %d x %d", bounds.size.h, bounds.size.w);

  int alarm_layer_height = 40;
  int alarm_layer_top = 10;

  #if defined(PBL_ROUND)
    alarm_layer_height = 40;
    alarm_layer_top = 6;
  #endif

  alarm_parent_layer = text_layer_create(GRect(0, bounds.size.h - alarm_layer_height, bounds.size.w, bounds.size.h));
  alarm_parent_layer_ref = text_layer_get_layer(alarm_parent_layer);

  alarm_layer = text_layer_create(GRect(22, 0, bounds.size.w - 22, alarm_layer_height));
  text_layer_set_text(alarm_layer, text);
  text_layer_set_text_alignment(alarm_layer, GTextAlignmentCenter);

  #if defined(PBL_RECT)
    text_layer_set_font(alarm_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_background_color(alarm_parent_layer, GColorWhite);
    text_layer_set_overflow_mode(alarm_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_color(alarm_layer, GColorBlack);
  #elif defined(PBL_ROUND)
    text_layer_set_font(alarm_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_background_color(alarm_parent_layer, GColorClear);
    text_layer_set_background_color(alarm_layer, GColorClear);
    text_layer_set_text_color(alarm_layer, GColorWhite);
  #endif

  alarm_layer_ref = text_layer_get_layer(alarm_layer);
  layer_add_child(alarm_parent_layer_ref, alarm_layer_ref);

  int text_size = text_layer_get_content_size(alarm_layer).w;

  image_alarm = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ALARM);
  image_layer_alarm = bitmap_layer_create(GRect(((bounds.size.w - 22 - text_size) / 2), alarm_layer_top, 20, 20));
  bitmap_layer_set_bitmap(image_layer_alarm, image_alarm);
  bitmap_layer_set_alignment(image_layer_alarm, GAlignLeft);
  image_layer_alarm_ref = bitmap_layer_get_layer(image_layer_alarm);
  layer_add_child(alarm_parent_layer_ref, image_layer_alarm_ref);

  layer_add_child(window_layer, alarm_parent_layer_ref);
}

static void alarm_hide() {
  if (image_layer_alarm) {
    layer_remove_from_parent(alarm_layer_ref);
    layer_remove_from_parent(image_layer_alarm_ref);
    layer_remove_from_parent(alarm_parent_layer_ref);

    gbitmap_destroy(image_alarm);
    bitmap_layer_destroy(image_layer_alarm);
    text_layer_destroy(alarm_layer);
    text_layer_destroy(alarm_parent_layer);
  }
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

//    APP_LOG(APP_LOG_LEVEL_ERROR, "Accel %d %d %d", data.x, data.y, data.z);


    int sum = absolute(last_x - data.x) + absolute(last_y - data.y) + absolute(last_z - data.z);
    int sum_new = asqrt(data.x * data.x + data.y * data.y + data.z * data.z);

    if (sum > max_sum) {
        max_sum = sum;
    }

    if (sum_new > max_sum_new) {
        max_sum_new = sum_new;
    }
//      max_sum = max_sum + sum;

  }
  last = true;
  last_x = data.x;
  last_y = data.y;
  last_z = data.z;
}

// HANDLERS

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
  last_received_msg_ts = time(NULL);

  Tuple *alarm_start = dict_find(received, MSG_KEY_ALARM_START);
  Tuple *alarm_stop = dict_find(received, MSG_KEY_ALARM_STOP);
  Tuple *alarm_hour_of_day = dict_find(received, MSG_KEY_ALARM_HOUR_OF_DAY);
  Tuple *alarm_minute = dict_find(received, MSG_KEY_ALARM_MINUTE);
  Tuple *batch_size = dict_find(received, MSG_KEY_SET_BATCH_SIZE);
  Tuple *timeline_token = dict_find(received, MSG_KEY_TIMELINE_TOKEN);
  Tuple *suspend_mode = dict_find(received, MSG_KEY_SET_SUSPEND_STATUS);
  Tuple *suspend_ts = dict_find(received, MSG_KEY_SUSPEND_TILL_TS);
  Tuple *hint = dict_find(received, MSG_KEY_HINT);
  Tuple *alarm_ts = dict_find(received, MSG_KEY_ALARM_TS);
  Tuple *enable_hr = dict_find(received, MSG_KEY_ENABLE_HR);

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
      switch (hint_repeat) {
        case 1:
            vibes_long_pulse();
            break;
        case 2:
            vibes_enqueue_custom_pattern(pat2);
            break;
        case 3:
            vibes_enqueue_custom_pattern(pat3);
            break;
        case 4:
            vibes_enqueue_custom_pattern(pat4);
            break;
        case 5:
            vibes_enqueue_custom_pattern(pat5);
            break;
        case 10:
            vibes_enqueue_custom_pattern(pat10);
            break;
        default:
            vibes_long_pulse();
      }
  }

  if (timeline_token) {
    send_timeline_token(timeline_token->value->cstring);
  }

}

// Incoming message dropped.
void in_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Dropped message");
}




// Builds the toggle layer
static void info_update_proc(Layer *layer, GContext *ctx) {
	if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "info_update_proc");

	// Battery batteryLevel / BatteryState
	graphics_draw_bitmap_in_rect(ctx, icon_bat, GRect(144-32,0,32,16));
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, GRect(118,5,(batteryLevel*2),6), 0, 0);
	
	// batteryLevel * 26 -> 26 to 260

	// Bluetooth
	icon_bt = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(bluetoothState ? 0 : 16,0,16,16));
	graphics_draw_bitmap_in_rect(ctx, icon_bt, GRect(0,0,16,16));
 
	// QuietTime
	icon_qt = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(quietTimeState ? 0 : 16,16,16,16));
	graphics_draw_bitmap_in_rect(ctx, icon_qt, GRect(16,0,16,16));
	
	#if defined(PBL_HEALTH)
		icon_heart = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(0,48,16,16));
		graphics_draw_bitmap_in_rect(ctx, icon_heart, GRect(48,0,16,16));
		snprintf(info_text_buffer, sizeof(info_text_buffer), "%3d", (int) bpmValue);
		text_layer_set_text(info_text_layer, info_text_buffer);
	#endif
		
}

// Monitor battery status
static void handle_battery(BatteryChargeState charge_state) {	
	if (charge_state.is_charging) batteryState = 1;
	else if (charge_state.is_plugged) batteryState = 2;
	else batteryState = 0;	
	batteryLevel = ((charge_state.charge_percent)/10);
	if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_battery: state %d, level %d", batteryState, batteryLevel);
	layer_mark_dirty(info_layer);
}

// Monitor BT status
static void handle_bluetooth(bool connected) {
	if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_bluetooth: %d", connected);
  bluetoothState = connected;
	//bluetoothStateOld = bluetoothState;
	layer_mark_dirty(info_layer);
}

// Handle Health Events
#if defined(PBL_HEALTH)
	#if PBL_API_EXISTS(health_service_set_heart_rate_sample_period)
static void handle_health(HealthEventType event, void *context) {
		if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "update_health : %d", event);
		if (event == HealthEventSignificantUpdate || event == HealthEventHeartRateUpdate) {
			bpmValue = health_service_peek_current_value(HealthMetricHeartRateBPM);
		}
		layer_mark_dirty(info_layer);
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

  image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG);
  image_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(image_layer, image);
  bitmap_layer_set_alignment(image_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(image_layer));

  image_action_pause = gbitmap_create_with_resource(RESOURCE_ID_ACTION_PAUSE);
  image_layer_pause = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(image_layer_pause, image_action_pause);
  bitmap_layer_set_alignment(image_layer_pause, GAlignRight);
  layer_add_child(window_layer, bitmap_layer_get_layer(image_layer_pause));

  text_layer = text_layer_create((GRect) { .origin = { 0, 1 }, .size = { bounds.size.w, 80 } });
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);

  #if defined(PBL_RECT)
    text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  #elif defined(PBL_ROUND)
    text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  #endif

  text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

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
	
	
	// Create Info Layer
	info_layer = layer_create(GRect(0,0,144,75));
	layer_set_update_proc(info_layer, info_update_proc);
	layer_add_child(window_layer, info_layer);
	
	// Create Info images
	icon_sprites = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPRITES);
	icon_bt = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(0,0,16,16));
	icon_qt = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(0,16,16,16));
	icon_bat = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(0,32,32,16));
	icon_heart = gbitmap_create_as_sub_bitmap(icon_sprites, GRect(0,48,16,16));
	
	// Create Info Text Layer (Temporary)
	info_text_layer = text_layer_create(GRect(72,-2,32,16));
	text_layer_set_text_color(info_text_layer, GColorWhite);
	text_layer_set_background_color(info_text_layer, GColorClear);
	text_layer_set_overflow_mode(info_text_layer, GTextOverflowModeWordWrap);
	text_layer_set_text_alignment(info_text_layer, GTextAlignmentLeft);
	
	layer_add_child(info_layer, text_layer_get_layer(info_text_layer));
	
	// subscribe to event handlers
	#if defined(PBL_HEALTH)
		#if PBL_API_EXISTS(health_service_set_heart_rate_sample_period)
			health_service_events_subscribe(handle_health, NULL);
		#endif	
	#endif
	
	battery_state_service_subscribe(handle_battery);
	handle_battery(battery_state_service_peek());
	
	connection_service_subscribe((ConnectionHandlers) {.pebble_app_connection_handler = handle_bluetooth});
	handle_bluetooth(bluetooth_connection_service_peek());
	
	quietTimeState = quiet_time_is_active();

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
	
	// destroy info layer stuff	
	gbitmap_destroy(icon_bt);
	gbitmap_destroy(icon_qt);
	gbitmap_destroy(icon_bat);
	gbitmap_destroy(icon_heart);
	gbitmap_destroy(icon_sprites);
	
	text_layer_destroy(info_text_layer);
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
  // Initialize the communication channels with phone.
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);

  app_focus_service_subscribe(focus_handler);

  const uint32_t inbound_size = 64;
  const uint32_t outbound_size = 128;
  app_message_open(inbound_size, outbound_size);

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
	
	battery_state_service_unsubscribe();
	connection_service_unsubscribe();
	health_service_events_unsubscribe();

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
