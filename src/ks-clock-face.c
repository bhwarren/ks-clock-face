#include <pebble.h>

#define COLORS       true
#define COLOR_KEY 1
#define WHITE_INT 63
#define PREV_COLOR 1
#define ANTIALIASING true

#define HAND_MARGIN  10
#define FINAL_RADIUS 55

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;

static GColor background_col;
static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0;
static bool s_animating = false;

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void update_proc(Layer *layer, GContext *ctx) {
  // Color background?
  if(COLORS) {
    graphics_context_set_fill_color(ctx, background_col);
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 4);

  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // White clockface
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, s_center, s_radius);

  // Draw outline
  graphics_draw_circle(ctx, s_center, s_radius);

  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;
  if(s_animating) {
    // Hours out of 60 for smoothness
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
  } else {
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  }
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  // Plot hands
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };

  // Draw hands with positive length only
  if(s_radius > 2 * HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, hour_hand);
  } 
  if(s_radius > HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, minute_hand);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

/*********************************** App **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);

  layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

  layer_mark_dirty(s_canvas_layer);
}


static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *t = dict_read_first(iterator);
  while(t != NULL) {
    // Process this pair's key
    if(t->key == COLOR_KEY){
      int int_col = (int)t->value->int32;
      background_col = (GColor8){.argb=((uint8_t)(0xC0|int_col))};
      persist_write_int(PREV_COLOR, int_col);        
      
      // Redraw
      if(s_canvas_layer) {
        layer_mark_dirty(s_canvas_layer);
      }
      
      break;
    }
    // Get next pair, if any so we don't miss an actual request amongst other data
    t = dict_read_next(iterator);
  }

  
}


static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}


static void init() {

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  if (! persist_exists(PREV_COLOR)) {
    persist_write_int(PREV_COLOR, WHITE_INT);
  }
  int int_col = persist_read_int(PREV_COLOR);

  background_col = (GColor8){.argb=((uint8_t)(0xC0|int_col))};
  
  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

  AnimationImplementation hands_impl = {
    .update = hands_update
  };
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
