#include <pebble.h>

static Window *s_window;
static Window *s_actions_window;
static Layer *s_canvas_layer;
static MenuLayer *s_actions_menu_layer;
static DictationSession *s_dictation_session;

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

static const char *MONTHS_SHORT[] = {
  "Jan","Feb","Mar","Apr","May","Jun",
  "Jul","Aug","Sep","Oct","Nov","Dec"
};

static const char *MONTHS[] = {
  "January","February","March","April","May","June",
  "July","August","September","October","November","December"
};

static const int DAYS_IN_MONTH[] = {31,28,31,30,31,30,31,31,30,31,30,31};

#define MAX_NOTES 24
#define MAX_NOTE_LENGTH 128
#define PERSIST_KEY_NOTE_COUNT 100
#define PERSIST_KEY_NOTE_BASE 200

typedef struct {
  int32_t date_key;
  char text[MAX_NOTE_LENGTH];
} NoteRecord;

static NoteRecord s_notes[MAX_NOTES];
static int s_note_count;

static bool date_has_note(int year, int month, int day);

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

// --- Drawing helpers ---

static void draw_filled_triangle(GContext *ctx, GPoint p1, GPoint p2, GPoint p3) {
  GPoint pts[] = {p1, p2, p3};
  GPathInfo info = { .num_points = 3, .points = pts };
  GPath *path = gpath_create(&info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
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
    bool has_note = date_has_note(s_display_year, s_display_month, day);

    int sq = (cell_w < cell_h ? cell_w : cell_h) - 4;
    int hx = x + (cell_w - sq) / 2;
    int hy = cy - sq / 2;

    if (is_today && is_selected) {
      // Today is also the selected date: filled square, black text
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(hx, hy, sq, sq), 3, GCornersAll);
      graphics_context_set_text_color(ctx, GColorBlack);
    } else if (is_today) {
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

    if (has_note) {
      int indicator_y = text_rect.origin.y;
      if (indicator_y >= y) {
        graphics_context_set_fill_color(ctx, is_today ? GColorBlack : GColorWhite);
        graphics_fill_rect(ctx, GRect(x + cell_w / 2 - 1, indicator_y, 3, 1), 0, GCornerNone);
      }
    }

    // --- Navigation mode arrows for selected day ---
    if (is_selected) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      int mx = hx + sq / 2;

      if (s_nav_mode == NAV_DAY) {
        int left_tip_x = hx - 5;
        int left_base_x = hx - 1;
        if (left_tip_x >= 0) {
          draw_filled_triangle(ctx,
            GPoint(left_tip_x, cy),
            GPoint(left_base_x, cy - 4),
            GPoint(left_base_x, cy + 4));
        }

        int right_tip_x = hx + sq + 5;
        int right_base_x = hx + sq + 1;
        if (right_tip_x < bounds.size.w) {
          draw_filled_triangle(ctx,
            GPoint(right_tip_x, cy),
            GPoint(right_base_x, cy - 4),
            GPoint(right_base_x, cy + 4));
        }

      } else { // NAV_WEEK
        int up_tip_y = hy - 4;
        int up_base_y = hy - 1;
        if (up_tip_y >= header_h + dow_h) {
          draw_filled_triangle(ctx,
            GPoint(mx, up_tip_y),
            GPoint(mx - 3, up_base_y),
            GPoint(mx + 3, up_base_y));
        }

        int down_base_y = hy + sq + 1;
        int down_tip_y = hy + sq + 4;
        if (down_tip_y < bounds.size.h) {
          draw_filled_triangle(ctx,
            GPoint(mx, down_tip_y),
            GPoint(mx - 3, down_base_y),
            GPoint(mx + 3, down_base_y));
        }
      }
    }
  }  // end day loop
}

// --- Navigation helpers ---

static void sync_display_to_selected(void) {
  s_display_month = s_selected_month;
  s_display_year  = s_selected_year;
}

static void jump_selected_to_today(void) {
  s_selected_day   = s_today_day;
  s_selected_month = s_today_month;
  s_selected_year  = s_today_year;
  s_nav_mode = NAV_DAY;
  sync_display_to_selected();
}

static int32_t selected_date_key(void) {
  return s_selected_year * 10000 + (s_selected_month + 1) * 100 + s_selected_day;
}

static void save_notes(void) {
  int old_count = persist_exists(PERSIST_KEY_NOTE_COUNT) ? persist_read_int(PERSIST_KEY_NOTE_COUNT) : 0;
  if (old_count < 0 || old_count > MAX_NOTES) {
    old_count = 0;
  }

  for (int i = 0; i < s_note_count; i++) {
    int written = persist_write_data(PERSIST_KEY_NOTE_BASE + i, &s_notes[i], sizeof(NoteRecord));
    if (written != (int)sizeof(NoteRecord)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to persist note %d", i);
    }
  }

  for (int i = s_note_count; i < old_count; i++) {
    if (persist_exists(PERSIST_KEY_NOTE_BASE + i)) {
      persist_delete(PERSIST_KEY_NOTE_BASE + i);
    }
  }

  if (s_note_count > 0) {
    persist_write_int(PERSIST_KEY_NOTE_COUNT, s_note_count);
  } else if (persist_exists(PERSIST_KEY_NOTE_COUNT)) {
    persist_delete(PERSIST_KEY_NOTE_COUNT);
  }
}

static void load_notes(void) {
  s_note_count = 0;

  if (!persist_exists(PERSIST_KEY_NOTE_COUNT)) {
    return;
  }

  int stored_count = persist_read_int(PERSIST_KEY_NOTE_COUNT);
  if (stored_count < 0) {
    stored_count = 0;
  }
  if (stored_count > MAX_NOTES) {
    stored_count = MAX_NOTES;
  }

  for (int i = 0; i < stored_count; i++) {
    if (!persist_exists(PERSIST_KEY_NOTE_BASE + i)) {
      continue;
    }

    int read = persist_read_data(PERSIST_KEY_NOTE_BASE + i, &s_notes[s_note_count], sizeof(NoteRecord));
    if (read == (int)sizeof(NoteRecord)) {
      s_note_count++;
    } else {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load note %d", i);
    }
  }
}

static int get_note_count_for_selected_date(void) {
  int count = 0;
  int32_t date_key = selected_date_key();

  for (int i = 0; i < s_note_count; i++) {
    if (s_notes[i].date_key == date_key) {
      count++;
    }
  }

  return count;
}

static bool date_has_note(int year, int month, int day) {
  int32_t date_key = year * 10000 + (month + 1) * 100 + day;

  for (int i = 0; i < s_note_count; i++) {
    if (s_notes[i].date_key == date_key) {
      return true;
    }
  }

  return false;
}

static int get_note_index_for_selected_row(int row) {
  int found = 0;
  int32_t date_key = selected_date_key();

  for (int i = 0; i < s_note_count; i++) {
    if (s_notes[i].date_key != date_key) {
      continue;
    }

    if (found == row) {
      return i;
    }

    found++;
  }

  return -1;
}

static void add_note_for_selected_date(const char *text) {
  if (!text || text[0] == '\0') {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Empty note transcription");
    vibes_double_pulse();
    return;
  }

  if (s_note_count >= MAX_NOTES) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Note storage full");
    vibes_double_pulse();
    return;
  }

  s_notes[s_note_count].date_key = selected_date_key();
  snprintf(s_notes[s_note_count].text, sizeof(s_notes[s_note_count].text), "%s", text);
  s_note_count++;
  save_notes();
  vibes_short_pulse();
}

static void format_selected_date(char *buffer, size_t size) {
  snprintf(buffer, size, "%s %d, %d",
    MONTHS_SHORT[s_selected_month], s_selected_day, s_selected_year);
}

static void post_dictation_update(void *context) {
  (void)context;

  if (s_actions_menu_layer) {
    menu_layer_reload_data(s_actions_menu_layer);
  }

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void dictation_session_callback(DictationSession *session,
                                       DictationSessionStatus status,
                                       char *transcription,
                                       void *context) {
  (void)session;
  (void)context;

  if (status != DictationSessionStatusSuccess) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Dictation failed: %d", status);
    vibes_double_pulse();
    return;
  }

  add_note_for_selected_date(transcription);

  // Defer UI updates until the dictation overlay has fully torn down.
  app_timer_register(50, post_dictation_update, NULL);
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

// --- Actions menu ---

static uint16_t actions_menu_get_num_sections(MenuLayer *menu_layer, void *context) {
  (void)menu_layer;
  (void)context;
  return 3;
}

static uint16_t actions_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  (void)menu_layer;
  (void)context;

  if (section_index == 2) {
    int note_count = get_note_count_for_selected_date();
    return note_count > 0 ? (uint16_t)note_count : 1;
  }

  return 1;
}

static int16_t actions_menu_get_header_height(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  (void)menu_layer;
  (void)section_index;
  (void)context;
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void actions_menu_draw_header(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *context) {
  (void)context;

  if (section_index == 0) {
    menu_cell_basic_header_draw(ctx, cell_layer, "Jump");
  } else if (section_index == 1) {
    menu_cell_basic_header_draw(ctx, cell_layer, "Create");
  } else {
    menu_cell_basic_header_draw(ctx, cell_layer, "Notes");
  }
}

static void actions_menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  (void)context;

  if (cell_index->section == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "Go To Today", NULL, NULL);
    return;
  }

  if (cell_index->section == 1) {
    menu_cell_basic_draw(ctx, cell_layer, "Add Note", "Voice dictation", NULL);
    return;
  }

  if (get_note_count_for_selected_date() == 0) {
    char selected_date[24];
    format_selected_date(selected_date, sizeof(selected_date));
    menu_cell_basic_draw(ctx, cell_layer, "No notes yet", selected_date, NULL);
    return;
  }

  int note_index = get_note_index_for_selected_row(cell_index->row);
  if (note_index >= 0) {
    menu_cell_basic_draw(ctx, cell_layer, s_notes[note_index].text, NULL, NULL);
  }
}

static void actions_menu_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  (void)menu_layer;
  (void)context;

  if (cell_index->section == 0) {
    jump_selected_to_today();
    layer_mark_dirty(s_canvas_layer);
    window_stack_pop(true);
    return;
  }

  if (cell_index->section == 1) {
    if (s_dictation_session) {
      dictation_session_start(s_dictation_session);
    } else {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Dictation session unavailable");
      vibes_double_pulse();
    }
  }
}

static void actions_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, GColorBlack);

  s_actions_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_actions_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = actions_menu_get_num_sections,
    .get_num_rows = actions_menu_get_num_rows,
    .get_header_height = actions_menu_get_header_height,
    .draw_header = actions_menu_draw_header,
    .draw_row = actions_menu_draw_row,
    .select_click = actions_menu_select,
  });
  menu_layer_set_click_config_onto_window(s_actions_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_actions_menu_layer));
}

static void actions_window_unload(Window *window) {
  (void)window;
  menu_layer_destroy(s_actions_menu_layer);
  s_actions_menu_layer = NULL;
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
  (void)recognizer;
  (void)context;

  if (s_actions_menu_layer) {
    menu_layer_reload_data(s_actions_menu_layer);
  }
  window_stack_push(s_actions_window, true);
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

  load_notes();
  s_dictation_session = dictation_session_create(MAX_NOTE_LENGTH - 1, dictation_session_callback, NULL);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });

  s_actions_window = window_create();
  window_set_window_handlers(s_actions_window, (WindowHandlers) {
    .load   = actions_window_load,
    .unload = actions_window_unload,
  });

  window_stack_push(s_window, true);
}

static void deinit(void) {
  dictation_session_destroy(s_dictation_session);
  window_destroy(s_actions_window);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
