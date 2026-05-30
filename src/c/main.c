#include <pebble.h>

static Window *s_window;
static Layer *s_canvas_layer;
static GColor s_fg_color;
static GColor s_bg_color;

enum {
  PERSIST_KEY_FG_COLOR = 1,
  PERSIST_KEY_BG_COLOR = 2
};

// Today's actual date (never changes while app is running)
static int s_today_day;
static int s_today_month;
static int s_today_year;

// Currently displayed month/year (follows the selected date)
static int s_display_month;
static int s_display_year;

// Currently selected date (controlled by Up/Down)
static int s_selected_day;
static int s_selected_month;
static int s_selected_year;

// Navigation mode: 0=day, 1=week
typedef enum { NAV_DAY = 0, NAV_WEEK = 1 } NavMode;
static NavMode s_nav_mode = NAV_DAY;

static const char *DAYS[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};

static const char *MONTHS[] = {
  "January","February","March","April","May","June",
  "July","August","September","October","November","December"
};

static const int DAYS_IN_MONTH[] = {31,28,31,30,31,30,31,31,30,31,30,31};

static void apply_colors(uint8_t fg_argb, uint8_t bg_argb) {
  s_fg_color = GColorFromARGB8(fg_argb);
  s_bg_color = GColorFromARGB8(bg_argb);

  if (s_window) {
    window_set_background_color(s_window, s_bg_color);
  }

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void persist_colors(void) {
  persist_write_int(PERSIST_KEY_FG_COLOR, s_fg_color.argb);
  persist_write_int(PERSIST_KEY_BG_COLOR, s_bg_color.argb);
}

static void load_persisted_colors(void) {
  uint8_t fg_argb = 0xFF;
  uint8_t bg_argb = 0xC0;

  if (persist_exists(PERSIST_KEY_FG_COLOR)) {
    fg_argb = (uint8_t)persist_read_int(PERSIST_KEY_FG_COLOR);
  }

  if (persist_exists(PERSIST_KEY_BG_COLOR)) {
    bg_argb = (uint8_t)persist_read_int(PERSIST_KEY_BG_COLOR);
  }

  apply_colors(fg_argb, bg_argb);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *fg_tuple = dict_find(iter, MESSAGE_KEY_fg_color);
  Tuple *bg_tuple = dict_find(iter, MESSAGE_KEY_bg_color);

  if (!fg_tuple && !bg_tuple) {
    return;
  }

  uint8_t fg_argb = fg_tuple ? (uint8_t)fg_tuple->value->int32 : s_fg_color.argb;
  uint8_t bg_argb = bg_tuple ? (uint8_t)bg_tuple->value->int32 : s_bg_color.argb;

  apply_colors(fg_argb, bg_argb);
  persist_colors();
}

static int days_in_month(int month, int year) {
  if (month == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
    return 29;
  }
  return DAYS_IN_MONTH[month];
}

static int first_day_of_month(int month, int year) {
  static int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (month < 2) year--;
  return (year + year/4 - year/100 + year/400 + t[month] + 1) % 7;
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int width = bounds.size.w;

  int header_h = 18;
  int dow_h    = 14;
  int cell_w   = width / 7;
  int cell_h   = (bounds.size.h - header_h - dow_h) / 6;

  // --- Month/year header ---
  graphics_context_set_text_color(ctx, s_fg_color);
  char title[20];
  snprintf(title, sizeof(title), "%s %d", MONTHS[s_display_month], s_display_year);
  graphics_draw_text(ctx, title,
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 0, width, header_h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // --- Day-of-week row ---
  for (int d = 0; d < 7; d++) {
    graphics_draw_text(ctx, DAYS[d],
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(d * cell_w, header_h, cell_w, dow_h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // --- Day numbers ---
  int start_dow = first_day_of_month(s_display_month, s_display_year);
  int num_days  = days_in_month(s_display_month, s_display_year);
  char buf[4];

  GFont day_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  int text_h      = 16;
  int text_voffset = -2;

  for (int day = 1; day <= num_days; day++) {
    int slot = day - 1 + start_dow;
    int col  = slot % 7;
    int row  = slot / 7;

    int x = col * cell_w;
    int y = header_h + dow_h + row * cell_h;
    int cy = y + cell_h / 2;

    GRect text_rect = GRect(x, cy - text_h / 2 + text_voffset, cell_w, text_h);

    bool is_today = (day == s_today_day &&
                     s_display_month == s_today_month &&
                     s_display_year  == s_today_year);
    bool is_selected = (day == s_selected_day &&
                        s_display_month == s_selected_month &&
                        s_display_year  == s_selected_year);

    int sq = (cell_w < cell_h ? cell_w : cell_h) - 4;
    int hx = x + (cell_w - sq) / 2;
    int hy = cy - sq / 2;

    if (is_today) {
      // Filled foreground square, background text
      graphics_context_set_fill_color(ctx, s_fg_color);
      graphics_fill_rect(ctx, GRect(hx, hy, sq, sq), 3, GCornersAll);
      graphics_context_set_text_color(ctx, s_bg_color);
    } else if (is_selected) {
      // Outlined foreground square, foreground text
      graphics_context_set_stroke_color(ctx, s_fg_color);
      graphics_draw_round_rect(ctx, GRect(hx, hy, sq, sq), 3);
      graphics_context_set_text_color(ctx, s_fg_color);
    } else {
      graphics_context_set_text_color(ctx, s_fg_color);
    }

    snprintf(buf, sizeof(buf), "%d", day);
    graphics_draw_text(ctx, buf, day_font, text_rect,
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// --- Navigation helpers ---

static void sync_display_to_selected(void) {
  s_display_month = s_selected_month;
  s_display_year  = s_selected_year;
}

static void move_selected_by_day(int delta) {
  s_selected_day += delta;
  while (s_selected_day < 1) {
    s_selected_month--;
    if (s_selected_month < 0) { s_selected_month = 11; s_selected_year--; }
    s_selected_day += days_in_month(s_selected_month, s_selected_year);
  }
  while (s_selected_day > days_in_month(s_selected_month, s_selected_year)) {
    s_selected_day -= days_in_month(s_selected_month, s_selected_year);
    s_selected_month++;
    if (s_selected_month > 11) { s_selected_month = 0; s_selected_year++; }
  }
}

// --- Button handlers ---

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  move_selected_by_day(s_nav_mode == NAV_WEEK ? -7 : -1);
  sync_display_to_selected();
  layer_mark_dirty(s_canvas_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  move_selected_by_day(s_nav_mode == NAV_WEEK ? 7 : 1);
  sync_display_to_selected();
  layer_mark_dirty(s_canvas_layer);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_nav_mode = (s_nav_mode == NAV_DAY) ? NAV_WEEK : NAV_DAY;
  layer_mark_dirty(s_canvas_layer);
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Jump to today (will be replaced by actions menu in Phase 3)
  s_selected_day   = s_today_day;
  s_selected_month = s_today_month;
  s_selected_year  = s_today_year;
  s_nav_mode = NAV_DAY;
  sync_display_to_selected();
  layer_mark_dirty(s_canvas_layer);
}

static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP,   100, up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, s_bg_color);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  window_set_click_config_provider(window, click_config_provider);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

static void init(void) {
  load_persisted_colors();

  // Capture today once at startup
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_today_day   = t->tm_mday;
  s_today_month = t->tm_mon;
  s_today_year  = t->tm_year + 1900;

  // Selected date starts on today
  s_selected_day   = s_today_day;
  s_selected_month = s_today_month;
  s_selected_year  = s_today_year;

  // Start display on the current month
  s_display_month = s_today_month;
  s_display_year  = s_today_year;

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(64, 64);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
