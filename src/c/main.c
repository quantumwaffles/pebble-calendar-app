#include <pebble.h>

static Window *s_window;
static Layer *s_canvas_layer;

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
  graphics_context_set_text_color(ctx, GColorWhite);
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
      // Filled white square, black text
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(hx, hy, sq, sq), 3, GCornersAll);
      graphics_context_set_text_color(ctx, GColorBlack);
    } else if (is_selected) {
      // Outlined white square, white text
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_round_rect(ctx, GRect(hx, hy, sq, sq), 3);
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_text_color(ctx, GColorWhite);
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
  window_set_background_color(window, GColorBlack);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  window_set_click_config_provider(window, click_config_provider);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

static void init(void) {
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