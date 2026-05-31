#include <pebble.h>

static Window *s_window;
static Window *s_actions_window;
static Window *s_note_window;
static Window *s_delete_confirm_window;
static Layer *s_canvas_layer;
static MenuLayer *s_actions_menu_layer;
static DictationSession *s_dictation_session;
static ScrollLayer *s_note_scroll_layer;
static TextLayer *s_note_title_layer;
static TextLayer *s_note_text_layer;
static TextLayer *s_delete_confirm_prompt_layer;
static TextLayer *s_delete_confirm_yes_layer;
static TextLayer *s_delete_confirm_cancel_layer;
static Layer *s_note_delete_icon_layer;
static GBitmap *s_note_delete_icon_bitmap;
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

// --- Calendar interface animation state ---
#define ANIM_MAX       1000   // progress is tracked on a 0..ANIM_MAX scale
#define ANIM_DAY_MS    160    // duration of a day-selector slide
#define ANIM_MONTH_MS  240    // duration of a month slide-in/out
#define ANIM_FRAME_MS  33     // ~30fps redraw cadence

typedef enum { ANIM_NONE = 0, ANIM_DAY_SLIDE, ANIM_MONTH_SLIDE } AnimKind;
static AnimKind s_anim_kind = ANIM_NONE;
static AppTimer *s_anim_timer = NULL;
static uint32_t s_anim_elapsed_ms = 0;
static uint32_t s_anim_duration_ms = 0;
static int32_t s_anim_progress = 0;   // 0..ANIM_MAX (linear, pre-easing)
static int s_anim_dir = 1;            // +1 = slide up (forward), -1 = slide down (back)

// Snapshot of the previous selection/display, captured when an animation begins.
static int s_prev_selected_day = 1;
static int s_prev_display_month = 0;
static int s_prev_display_year = 2000;

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
#define NOTE_ICON_PANEL_W 30

typedef struct {
  int32_t date_key;
  char text[MAX_NOTE_LENGTH];
} NoteRecord;

static NoteRecord s_notes[MAX_NOTES];
static int s_note_count;
static int s_active_note_index = -1;
static char s_note_title_buffer[24];

static bool date_has_note(int year, int month, int day);
static void start_nav_transition(int prev_day, int prev_disp_month, int prev_disp_year, int dir);

static void apply_colors(uint8_t fg_argb, uint8_t bg_argb) {
  s_fg_color = (GColor) { .argb = fg_argb };
  s_bg_color = (GColor) { .argb = bg_argb };

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

// --- Drawing helpers ---

static void draw_filled_triangle(GContext *ctx, GPoint p1, GPoint p2, GPoint p3) {
  GPoint pts[] = {p1, p2, p3};
  GPathInfo info = { .num_points = 3, .points = pts };
  GPath *path = gpath_create(&info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// --- Animation helpers ---

// Smoothstep easing: maps a linear 0..ANIM_MAX progress to an eased 0..ANIM_MAX
// value (slow start, fast middle, slow finish) using f(t) = 3t^2 - 2t^3.
static int32_t ease_in_out(int32_t p) {
  if (p <= 0) return 0;
  if (p >= ANIM_MAX) return ANIM_MAX;
  int64_t r = ((int64_t)p * p * (3 * ANIM_MAX - 2 * p)) / ((int64_t)ANIM_MAX * ANIM_MAX);
  return (int32_t)r;
}

// Linearly interpolate between two pixel values using an eased 0..ANIM_MAX factor.
static int interp(int from, int to, int32_t eased) {
  return from + (int)(((int64_t)(to - from) * eased) / ANIM_MAX);
}

static void anim_timer_callback(void *context) {
  (void)context;
  s_anim_timer = NULL;
  s_anim_elapsed_ms += ANIM_FRAME_MS;

  if (s_anim_duration_ms == 0 || s_anim_elapsed_ms >= s_anim_duration_ms) {
    // Final frame: snap to the resting (non-animated) drawing path.
    s_anim_progress = ANIM_MAX;
    s_anim_kind = ANIM_NONE;
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
    return;
  }

  s_anim_progress = (int32_t)(((int64_t)s_anim_elapsed_ms * ANIM_MAX) / s_anim_duration_ms);
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
  s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_timer_callback, NULL);
}

static void start_animation(uint32_t duration_ms) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  s_anim_elapsed_ms = 0;
  s_anim_duration_ms = duration_ms;
  s_anim_progress = 0;
  s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_timer_callback, NULL);
}

static void note_delete_icon_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_stroke_color(ctx, s_fg_color);
  graphics_context_set_fill_color(ctx, s_fg_color);

  // Separator line on left edge of panel
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(0, bounds.size.h));

  if (!s_note_delete_icon_bitmap) {
    return;
  }

  GRect icon_bounds = gbitmap_get_bounds(s_note_delete_icon_bitmap);
  GRect icon_frame = GRect((bounds.size.w - icon_bounds.size.w) / 2,
                           (bounds.size.h - icon_bounds.size.h) / 2,
                           icon_bounds.size.w,
                           icon_bounds.size.h);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_note_delete_icon_bitmap, icon_frame);
}

// Compute the geometry of a day's selection highlight (square position/size and
// vertical centre) within the grid for a given month, ignoring horizontal offset.
static void compute_day_cell(GRect bounds, int month, int year, int day,
                             int *out_hx, int *out_hy, int *out_cy, int *out_sq) {
  int width = bounds.size.w;
  int header_h = 18;
  int dow_h    = 14;
  int cell_w   = width / 7;
  int cell_h   = (bounds.size.h - header_h - dow_h) / 6;

  int start_dow = first_day_of_month(month, year);
  int slot = day - 1 + start_dow;
  int col  = slot % 7;
  int row  = slot / 7;

  int x  = col * cell_w;
  int y  = header_h + dow_h + row * cell_h;
  int cy = y + cell_h / 2;
  int sq = (cell_w < cell_h ? cell_w : cell_h) - 4;

  *out_hx = x + (cell_w - sq) / 2;
  *out_hy = cy - sq / 2;
  *out_cy = cy;
  *out_sq = sq;
}

// Draw the navigation-mode arrows around a selection square at (hx, hy)/cy of size sq.
static void draw_selection_arrows(GContext *ctx, GRect bounds, int hx, int hy, int cy, int sq) {
  int header_h = 18;
  int dow_h    = 14;

  graphics_context_set_fill_color(ctx, s_fg_color);
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

// Draw a full month grid (header, day-of-week row, day numbers) shifted by
// (x_offset, y_offset) pixels. When sel_day > 0 the selection highlight and nav
// arrows are drawn on that day; pass 0 to omit the selection (e.g. day-slide
// overlays it).
static void draw_month_grid(GContext *ctx, GRect bounds, int month, int year,
                            int x_offset, int y_offset, int sel_day) {
  int width = bounds.size.w;

  int header_h = 18;
  int dow_h    = 14;
  int cell_w   = width / 7;
  int cell_h   = (bounds.size.h - header_h - dow_h) / 6;

  // --- Month/year header ---
  graphics_context_set_text_color(ctx, s_fg_color);
  char title[20];
  snprintf(title, sizeof(title), "%s %d", MONTHS[month], year);
  graphics_draw_text(ctx, title,
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(x_offset, y_offset, width, header_h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // --- Day-of-week row ---
  for (int d = 0; d < 7; d++) {
    graphics_draw_text(ctx, DAYS[d],
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(x_offset + d * cell_w, y_offset + header_h, cell_w, dow_h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // --- Day numbers ---
  int start_dow = first_day_of_month(month, year);
  int num_days  = days_in_month(month, year);
  char buf[4];

  GFont day_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  int text_h      = 16;
  int text_voffset = -2;

  for (int day = 1; day <= num_days; day++) {
    int slot = day - 1 + start_dow;
    int col  = slot % 7;
    int row  = slot / 7;

    int x = x_offset + col * cell_w;
    int y = y_offset + header_h + dow_h + row * cell_h;
    int cy = y + cell_h / 2;

    GRect text_rect = GRect(x, cy - text_h / 2 + text_voffset, cell_w, text_h);

    bool is_today = (day == s_today_day &&
                     month == s_today_month &&
                     year  == s_today_year);
    bool is_selected = (sel_day > 0 && day == sel_day);
    bool has_note = date_has_note(year, month, day);

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

    if (has_note) {
      int indicator_y = text_rect.origin.y + 3;
      if (indicator_y >= y) {
        graphics_context_set_fill_color(ctx, is_today ? s_bg_color : s_fg_color);
        graphics_fill_rect(ctx, GRect(x + cell_w / 2 - 4, indicator_y, 8, 2), 0, GCornerNone);
      }
    }

    // --- Navigation mode arrows for selected day ---
    if (is_selected) {
      draw_selection_arrows(ctx, bounds, hx, hy, cy, sq);
    }
  }  // end day loop
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  if (s_anim_kind == ANIM_MONTH_SLIDE) {
    // Outgoing month slides off the top/bottom while the new month slides in.
    int height = bounds.size.h;
    int32_t e = ease_in_out(s_anim_progress);
    int slide = interp(0, height, e);
    int outgoing_off = -s_anim_dir * slide;
    int incoming_off =  s_anim_dir * (height - slide);

    draw_month_grid(ctx, bounds, s_prev_display_month, s_prev_display_year,
                    0, outgoing_off, s_prev_selected_day);
    draw_month_grid(ctx, bounds, s_display_month, s_display_year,
                    0, incoming_off, s_selected_day);
    return;
  }

  if (s_anim_kind == ANIM_DAY_SLIDE) {
    // Draw the grid with no selection, then slide the highlight box across it.
    draw_month_grid(ctx, bounds, s_display_month, s_display_year, 0, 0, 0);

    int32_t e = ease_in_out(s_anim_progress);

    int phx, phy, pcy, psq;
    compute_day_cell(bounds, s_display_month, s_display_year, s_prev_selected_day,
                     &phx, &phy, &pcy, &psq);
    int chx, chy, ccy, csq;
    compute_day_cell(bounds, s_display_month, s_display_year, s_selected_day,
                     &chx, &chy, &ccy, &csq);

    int hx = interp(phx, chx, e);
    int hy = interp(phy, chy, e);
    int cy = interp(pcy, ccy, e);

    graphics_context_set_stroke_color(ctx, s_fg_color);
    graphics_draw_round_rect(ctx, GRect(hx, hy, csq, csq), 3);
    draw_selection_arrows(ctx, bounds, hx, hy, cy, csq);
    return;
  }

  // Resting state: draw the current month with its selection in place.
  draw_month_grid(ctx, bounds, s_display_month, s_display_year, 0, 0, s_selected_day);
}

// --- Navigation helpers ---

static void sync_display_to_selected(void) {
  s_display_month = s_selected_month;
  s_display_year  = s_selected_year;
}

static void jump_selected_to_today(void) {
  int prev_day        = s_selected_day;
  int prev_disp_month = s_display_month;
  int prev_disp_year  = s_display_year;

  // Slide toward today: forward if today is later than the current view, else back.
  int dir;
  if (s_today_year != prev_disp_year) {
    dir = (s_today_year > prev_disp_year) ? 1 : -1;
  } else if (s_today_month != prev_disp_month) {
    dir = (s_today_month > prev_disp_month) ? 1 : -1;
  } else {
    dir = (s_today_day >= prev_day) ? 1 : -1;
  }

  s_selected_day   = s_today_day;
  s_selected_month = s_today_month;
  s_selected_year  = s_today_year;
  s_nav_mode = NAV_DAY;
  sync_display_to_selected();

  start_nav_transition(prev_day, prev_disp_month, prev_disp_year, dir);
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

static void delete_note_at_index(int note_index) {
  if (note_index < 0 || note_index >= s_note_count) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid note index %d", note_index);
    return;
  }

  for (int i = note_index; i < s_note_count - 1; i++) {
    s_notes[i] = s_notes[i + 1];
  }

  s_note_count--;
  if (s_note_count < 0) {
    s_note_count = 0;
  }

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

// --- Note detail and delete confirmation ---

static void refresh_note_view_content(void) {
  if (!s_note_title_layer || !s_note_text_layer || !s_note_scroll_layer) {
    return;
  }

  if (s_active_note_index < 0 || s_active_note_index >= s_note_count) {
    text_layer_set_text(s_note_title_layer, "Note");
    text_layer_set_text(s_note_text_layer, "");
    scroll_layer_set_content_size(s_note_scroll_layer, GSize(0, 0));
    return;
  }

  format_selected_date(s_note_title_buffer, sizeof(s_note_title_buffer));
  text_layer_set_text(s_note_title_layer, s_note_title_buffer);
  text_layer_set_text(s_note_text_layer, s_notes[s_active_note_index].text);

  GRect scroll_bounds = layer_get_bounds(scroll_layer_get_layer(s_note_scroll_layer));
  GSize content_size = text_layer_get_content_size(s_note_text_layer);
  text_layer_set_size(s_note_text_layer, GSize(scroll_bounds.size.w, content_size.h + 16));
  scroll_layer_set_content_size(s_note_scroll_layer,
    GSize(scroll_bounds.size.w, content_size.h + 16));
  scroll_layer_set_content_offset(s_note_scroll_layer, GPointZero, false);
}

static void note_view_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  window_stack_push(s_delete_confirm_window, true);
}

static void note_view_scroll_by(int delta_y) {
  if (!s_note_scroll_layer) {
    return;
  }

  GRect frame = layer_get_bounds(scroll_layer_get_layer(s_note_scroll_layer));
  GSize content_size = scroll_layer_get_content_size(s_note_scroll_layer);
  GPoint offset = scroll_layer_get_content_offset(s_note_scroll_layer);

  int min_y = frame.size.h - content_size.h;
  if (min_y > 0) {
    min_y = 0;
  }

  int next_y = offset.y + delta_y;
  if (next_y > 0) {
    next_y = 0;
  }
  if (next_y < min_y) {
    next_y = min_y;
  }

  scroll_layer_set_content_offset(s_note_scroll_layer, GPoint(0, next_y), true);
}

static void note_view_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  note_view_scroll_by(30);
}

static void note_view_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  note_view_scroll_by(-30);
}

static void note_view_click_config_provider(void *context) {
  (void)context;
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, note_view_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, note_view_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, note_view_select_click_handler);
}

static void delete_confirm_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (s_active_note_index < 0 || s_active_note_index >= s_note_count) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "No active note to delete");
    vibes_double_pulse();
    return;
  }

  delete_note_at_index(s_active_note_index);
  s_active_note_index = -1;

  if (s_actions_menu_layer) {
    menu_layer_reload_data(s_actions_menu_layer);
  }
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

  window_stack_pop(true);
  window_stack_pop(true);
}

static void delete_confirm_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  window_stack_pop(true);
}

static void delete_confirm_click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_UP, delete_confirm_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, delete_confirm_down_click_handler);
}

static void note_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, s_bg_color);

  s_note_title_layer = text_layer_create(GRect(4, 0, bounds.size.w - 8, 24));
  text_layer_set_background_color(s_note_title_layer, s_bg_color);
  text_layer_set_text_color(s_note_title_layer, s_fg_color);
  text_layer_set_font(s_note_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_note_title_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_note_title_layer));

  s_note_delete_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TRASH_ICON);

  // Icon panel: full-height right column below the title
  s_note_delete_icon_layer = layer_create(GRect(bounds.size.w - NOTE_ICON_PANEL_W, 24, NOTE_ICON_PANEL_W, bounds.size.h - 24));
  layer_set_update_proc(s_note_delete_icon_layer, note_delete_icon_update_proc);
  layer_add_child(window_layer, s_note_delete_icon_layer);

  // Scroll panel: left column below the title
  GRect scroll_frame = GRect(0, 24, bounds.size.w - NOTE_ICON_PANEL_W, bounds.size.h - 24);
  s_note_scroll_layer = scroll_layer_create(scroll_frame);

  s_note_text_layer = text_layer_create(GRect(0, 0, scroll_frame.size.w, 2000));
  text_layer_set_background_color(s_note_text_layer, s_bg_color);
  text_layer_set_text_color(s_note_text_layer, s_fg_color);
  text_layer_set_font(s_note_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_overflow_mode(s_note_text_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(s_note_text_layer, GTextAlignmentLeft);
  scroll_layer_add_child(s_note_scroll_layer, text_layer_get_layer(s_note_text_layer));
  layer_add_child(window_layer, scroll_layer_get_layer(s_note_scroll_layer));

  window_set_click_config_provider(window, note_view_click_config_provider);
  refresh_note_view_content();
}

static void note_window_unload(Window *window) {
  (void)window;
  layer_destroy(s_note_delete_icon_layer);
  gbitmap_destroy(s_note_delete_icon_bitmap);
  text_layer_destroy(s_note_text_layer);
  text_layer_destroy(s_note_title_layer);
  scroll_layer_destroy(s_note_scroll_layer);
  s_note_delete_icon_layer = NULL;
  s_note_delete_icon_bitmap = NULL;
  s_note_text_layer = NULL;
  s_note_title_layer = NULL;
  s_note_scroll_layer = NULL;
}

static void delete_confirm_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, s_bg_color);

  s_delete_confirm_yes_layer = text_layer_create(GRect(bounds.size.w - 48, 10, 44, 22));
  text_layer_set_background_color(s_delete_confirm_yes_layer, s_bg_color);
  text_layer_set_text_color(s_delete_confirm_yes_layer, s_fg_color);
  text_layer_set_font(s_delete_confirm_yes_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_delete_confirm_yes_layer, GTextAlignmentRight);
  text_layer_set_text(s_delete_confirm_yes_layer, "Yes");
  layer_add_child(window_layer, text_layer_get_layer(s_delete_confirm_yes_layer));

  s_delete_confirm_prompt_layer = text_layer_create(GRect(0, 42, bounds.size.w, 50));
  text_layer_set_background_color(s_delete_confirm_prompt_layer, s_bg_color);
  text_layer_set_text_color(s_delete_confirm_prompt_layer, s_fg_color);
  text_layer_set_font(s_delete_confirm_prompt_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_delete_confirm_prompt_layer, GTextAlignmentCenter);
  text_layer_set_text(s_delete_confirm_prompt_layer, "Delete note?");
  layer_add_child(window_layer, text_layer_get_layer(s_delete_confirm_prompt_layer));

  s_delete_confirm_cancel_layer = text_layer_create(GRect(4, bounds.size.h - 32, bounds.size.w - 8, 22));
  text_layer_set_background_color(s_delete_confirm_cancel_layer, s_bg_color);
  text_layer_set_text_color(s_delete_confirm_cancel_layer, s_fg_color);
  text_layer_set_font(s_delete_confirm_cancel_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_delete_confirm_cancel_layer, GTextAlignmentRight);
  text_layer_set_text(s_delete_confirm_cancel_layer, "Nevermind");
  layer_add_child(window_layer, text_layer_get_layer(s_delete_confirm_cancel_layer));

  window_set_click_config_provider(window, delete_confirm_click_config_provider);
}

static void delete_confirm_window_unload(Window *window) {
  (void)window;
  text_layer_destroy(s_delete_confirm_prompt_layer);
  text_layer_destroy(s_delete_confirm_yes_layer);
  text_layer_destroy(s_delete_confirm_cancel_layer);
  s_delete_confirm_prompt_layer = NULL;
  s_delete_confirm_yes_layer = NULL;
  s_delete_confirm_cancel_layer = NULL;
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

// Kick off the right animation for a navigation that has already updated the
// selected/display state. dir > 0 slides content upward (forward in time).
static void start_nav_transition(int prev_day, int prev_disp_month, int prev_disp_year, int dir) {
  bool month_changed = (s_display_month != prev_disp_month ||
                        s_display_year  != prev_disp_year);

  s_prev_selected_day = prev_day;

  if (month_changed) {
    s_anim_kind = ANIM_MONTH_SLIDE;
    s_prev_display_month = prev_disp_month;
    s_prev_display_year  = prev_disp_year;
    s_anim_dir = (dir >= 0) ? 1 : -1;
    start_animation(ANIM_MONTH_MS);
  } else {
    s_anim_kind = ANIM_DAY_SLIDE;
    start_animation(ANIM_DAY_MS);
  }

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void navigate_selected_by_day(int delta) {
  int prev_day        = s_selected_day;
  int prev_disp_month = s_display_month;
  int prev_disp_year  = s_display_year;

  move_selected_by_day(delta);
  sync_display_to_selected();

  start_nav_transition(prev_day, prev_disp_month, prev_disp_year, delta);
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
    return;
  }

  int note_index = get_note_index_for_selected_row(cell_index->row);
  if (note_index >= 0) {
    s_active_note_index = note_index;
    window_stack_push(s_note_window, true);
  }
}

static void actions_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, s_bg_color);

  s_actions_menu_layer = menu_layer_create(bounds);
  menu_layer_set_normal_colors(s_actions_menu_layer, s_bg_color, s_fg_color);
  menu_layer_set_highlight_colors(s_actions_menu_layer, s_fg_color, s_bg_color);
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
  navigate_selected_by_day(s_nav_mode == NAV_WEEK ? -7 : -1);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  navigate_selected_by_day(s_nav_mode == NAV_WEEK ? 7 : 1);
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
  window_set_background_color(window, s_bg_color);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  window_set_click_config_provider(window, click_config_provider);
}

static void window_unload(Window *window) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  s_anim_kind = ANIM_NONE;
  layer_destroy(s_canvas_layer);
  s_canvas_layer = NULL;
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

  load_notes();
  s_dictation_session = dictation_session_create(MAX_NOTE_LENGTH - 1, dictation_session_callback, NULL);
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(64, 64);

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

  s_note_window = window_create();
  window_set_window_handlers(s_note_window, (WindowHandlers) {
    .load   = note_window_load,
    .unload = note_window_unload,
  });

  s_delete_confirm_window = window_create();
  window_set_window_handlers(s_delete_confirm_window, (WindowHandlers) {
    .load   = delete_confirm_window_load,
    .unload = delete_confirm_window_unload,
  });

  window_stack_push(s_window, true);
}

static void deinit(void) {
  dictation_session_destroy(s_dictation_session);
  window_destroy(s_delete_confirm_window);
  window_destroy(s_note_window);
  window_destroy(s_actions_window);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
