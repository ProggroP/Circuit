// Circuit -- Watchface im Look einer bestueckten Platine.
// Alle Werte stehen weiss auf schwarzen Chips, die Platine selbst ist nur
// Kulisse. Gezeichnet wird ausschliesslich prozedural in canvas_update,
// einmal pro Minute plus bei Akku-, Verbindungs- und Wetter-Ereignissen.

#include <pebble.h>

// #define DEMO          // feste Beispielwerte fuer Store-Screenshots
#define DEMO_BOARD  0   // Platinenfarbe im DEMO-Build

// ---------------------------------------------------------------------------
// Farben
// ---------------------------------------------------------------------------
typedef struct {
  uint8_t board, trace, window, silk, chip, edge;
} Theme;

// Reihenfolge = Werte "0".."4" der Clay-Auswahl BOARD
static const Theme THEMES[] = {
  // Gruen
  { GColorIslamicGreenARGB8, GColorScreaminGreenARGB8, GColorDarkGreenARGB8,
    GColorMintGreenARGB8, GColorBlackARGB8, GColorDarkGrayARGB8 },
  // Blau
  { GColorCobaltBlueARGB8, GColorPictonBlueARGB8, GColorOxfordBlueARGB8,
    GColorCelesteARGB8, GColorBlackARGB8, GColorDarkGrayARGB8 },
  // Rot
  { GColorDarkCandyAppleRedARGB8, GColorSunsetOrangeARGB8, GColorBulgarianRoseARGB8,
    GColorMelonARGB8, GColorBlackARGB8, GColorDarkGrayARGB8 },
  // Schwarz
  { GColorBlackARGB8, GColorDarkGrayARGB8, GColorDarkGrayARGB8,
    GColorLightGrayARGB8, GColorDarkGrayARGB8, GColorLightGrayARGB8 },
  // Lila
  { GColorImperialPurpleARGB8, GColorPurpureusARGB8, GColorBlackARGB8,
    GColorRichBrilliantLavenderARGB8, GColorBlackARGB8, GColorDarkGrayARGB8 },
};
#define THEME_COUNT     ((int)ARRAY_LENGTH(THEMES))

#define COLOR_PIN        GColorLightGray
#define COLOR_TITLE      GColorLightGray
#define COLOR_VALUE      GColorWhite
#define COLOR_CAP        GColorWindsorTan
#define COLOR_RES        GColorBlack
#define COLOR_SMD_END    GColorLightGray
#define COLOR_LED_ON     GColorGreen
#define COLOR_LED_OFF    GColorRed
// Beide Displays entsaettigen: Rajah/ChromeYellow werden Lachs, Brass bleibt goldnah
#define COLOR_PAD        GColorBrass

// ---------------------------------------------------------------------------
// Schriften
// ---------------------------------------------------------------------------
#define FONT_TIME        FONT_KEY_LECO_60_NUMBERS_AM_PM
#define FONT_TITLE       FONT_KEY_GOTHIC_14
#define FONT_VALUE_L     FONT_KEY_GOTHIC_28_BOLD
#define FONT_VALUE_M     FONT_KEY_GOTHIC_18_BOLD
#define FONT_UNIT        FONT_KEY_GOTHIC_14
#define FONT_SILK        FONT_KEY_GOTHIC_09
#define FONT_AMPM        FONT_KEY_GOTHIC_14_BOLD

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
#define PIN_LEN          3
#define PIN_W            2
#define PIN_PITCH        5
#define PIN_INSET        5
#define CHIP_RADIUS      2
#define CHIP_PAD         5      // Textabstand vom Chiprand
#define TITLE_Y          (-2)   // Gothic 14 hat oben Luft
#define TRACE_W          2
#define VIA_R            3
#define VIA_HOLE_R       1
#define SMD_LONG         11
#define SMD_SHORT        6
#define SMD_END          3      // Metallkappe an beiden Enden
#define SOT_W            9
#define SOT_H            5
#define SOT_LEG          2
#define LED_W            8
#define LED_H            5
#define ROW_H            18
#define WINDOW_RADIUS    4
#define TIME_Y_NUDGE     9      // LECO-Box hat oben mehr Luft als unten (gemessen)
#define ICON_W           22
#define ICON_H           20

#ifdef PBL_PLATFORM_EMERY
#define RECT_WEATHER     GRect(10, 10, 80, 54)
#define RECT_POWER       GRect(142, 12, 48, 34)
#define RECT_WINDOW      GRect(8, 72, 184, 62)
#define RECT_HEART       GRect(10, 146, 70, 44)
#define RECT_DATE        GRect(10, 201, 70, 22)
#define RECT_ACTIVITY    GRect(92, 146, 98, 76)
#define RECT_AMPM        GRect(120, 49, 72, 18)
#define POS_LED          GPoint(110, 29)
#define POS_LED_LABEL    GPoint(121, 24)
#define ACTIVITY_ROWS    3
#define FINGER_X         194
#define FINGER_Y0        74
#define FINGER_Y1        130
#define FINGER_W         6
#define FINGER_H         4
#define FINGER_PITCH     7
#else
#define RECT_WEATHER     GRect(92, 24, 76, 52)
#define RECT_POWER       GRect(28, 72, 54, 34)
#define RECT_DATE        GRect(174, 72, 62, 34)
#define RECT_WINDOW      GRect(26, 114, 208, 62)
#define RECT_ACTIVITY    GRect(74, 188, 112, 52)
#define RECT_AMPM        GRect(88, 85, 84, 18)
#define POS_LED          GPoint(200, 196)
#define POS_LED_LABEL    GPoint(212, 190)
#define ACTIVITY_ROWS    2
#define RIM_CX           130
#define RIM_CY           130
#define RIM_R            123
#define RIM_STEPS        48     // Bohrungen auf dem Rand, sofern frei
#define RIM_CLEARANCE    6
#endif

// ---------------------------------------------------------------------------
// Dekoration: Leiterbahnen, Bauteile, Bestueckungsdruck
// ---------------------------------------------------------------------------
// Punktpaare; TRACE_VIA beendet eine Bahn mit Durchkontaktierung am letzten
// Punkt, TRACE_OPEN ohne.
#define TRACE_VIA        (-1)
#define TRACE_OPEN       (-2)

#define SMD_CAP          0
#define SMD_RES          1
#define SMD_SOT          2      // Transistor im SOT-23-Gehaeuse, Lage egal

typedef struct { int16_t x, y; uint8_t kind; bool vertical; } Smd;
typedef struct { int16_t x, y; const char *text; } Silk;

#ifdef PBL_PLATFORM_EMERY
static const int16_t TRACES[] = {
  93, 22,  102, 22,  108, 16,  134, 16,  TRACE_VIA,
  93, 46,  100, 46,  106, 40,  130, 40,  TRACE_VIA,
  12, 3,  134, 3,  TRACE_VIA,
  7, 24,  3, 28,  3, 62,  TRACE_VIA,
  4, 139,  78, 139,  84, 145,  84, 196,  86, 198,  86, 226,  TRACE_OPEN,
  108, 139,  188, 139,  TRACE_VIA,
  192, 30,  197, 30,  197, 64,  TRACE_VIA,
};
// Moeglichst auf den Leiterbahnen, wie auf einer echten Platine
static const Smd SMDS[] = {
  { 30, 0, SMD_RES, false },
  { 66, 0, SMD_CAP, false },
  { 115, 13, SMD_RES, false },
  { 112, 37, SMD_CAP, false },
  { 96, 30, SMD_SOT, false },
  { 96, 50, SMD_CAP, false },
  { 96, 60, SMD_RES, false },
  { 1, 78, SMD_CAP, true },
  { 1, 96, SMD_RES, true },
  { 1, 114, SMD_CAP, true },
  { 22, 136, SMD_CAP, false },
  { 56, 136, SMD_RES, false },
  { 122, 136, SMD_RES, false },
  { 160, 136, SMD_CAP, false },
  { 194, 38, SMD_RES, true },
  { 194, 150, SMD_CAP, true },
  { 194, 196, SMD_RES, true },
};
static const Silk SILKS[] = {
  { 113, 3, "R21" },
  { 109, 51, "C9" },
};
#else
static const int16_t TRACES[] = {
  89, 32,  72, 32,  60, 44,  48, 44,  TRACE_VIA,
  171, 32,  188, 32,  200, 44,  212, 44,  TRACE_VIA,
  89, 66,  84, 66,  80, 62,  TRACE_VIA,
  171, 66,  176, 66,  180, 62,  TRACE_VIA,
  26, 128,  12, 128,  TRACE_VIA,
  26, 138,  4, 138,  TRACE_OPEN,
  26, 152,  4, 152,  TRACE_OPEN,
  26, 162,  12, 162,  TRACE_VIA,
  234, 128,  248, 128,  TRACE_VIA,
  234, 138,  256, 138,  TRACE_OPEN,
  234, 152,  256, 152,  TRACE_OPEN,
  234, 162,  248, 162,  TRACE_VIA,
  71, 202,  58, 202,  50, 194,  TRACE_VIA,
  71, 226,  60, 226,  50, 216,  TRACE_VIA,
  189, 226,  198, 226,  206, 218,  TRACE_VIA,
  189, 208,  216, 208,  222, 202,  TRACE_VIA,
  100, 243,  100, 256,  TRACE_OPEN,
  130, 243,  130, 256,  TRACE_OPEN,
  160, 243,  160, 256,  TRACE_OPEN,
  130, 176,  130, 185,  TRACE_OPEN,
  55, 109,  55, 114,  TRACE_OPEN,
  205, 109,  205, 114,  TRACE_OPEN,
};
static const Smd SMDS[] = {
  { 60, 52, SMD_RES, false },
  { 188, 52, SMD_CAP, false },
  { 78, 40, SMD_CAP, true },
  { 176, 40, SMD_RES, true },
  { 24, 180, SMD_CAP, false },
  { 38, 180, SMD_RES, false },
  { 216, 180, SMD_CAP, false },
  { 96, 178, SMD_RES, false },
  { 152, 178, SMD_CAP, false },
};
static const Silk SILKS[] = {
  { 60, 59, "R13" },
  { 188, 59, "C22" },
  { 24, 187, "C3" },
};
#endif

// ---------------------------------------------------------------------------
// Wetter
// ---------------------------------------------------------------------------
#define COND_CLEAR          0
#define COND_PARTLY         1
#define COND_CLOUDY         2
#define COND_FOG            3
#define COND_RAIN           4
#define COND_SNOW           5
#define COND_STORM          6
#define COND_CLEAR_NIGHT    7
#define COND_PARTLY_NIGHT   8
#define COND_NONE           (-1)

#define WEATHER_INTERVAL_MIN  30
#define WEATHER_STALE_SEC     (3 * 60 * 60)

// ---------------------------------------------------------------------------
// Persistenz
// ---------------------------------------------------------------------------
#define PKEY_BOARD       1
#define PKEY_TEMP        2
#define PKEY_COND        3
#define PKEY_WX_TIME     4

#define INBOX_SIZE       128
#define OUTBOX_SIZE      64

// ---------------------------------------------------------------------------
// Zustand
// ---------------------------------------------------------------------------
static Window *s_window;
static Layer  *s_canvas;

static int    s_board = 0;
static int    s_temp = 0;
static int    s_cond = COND_NONE;
static time_t s_wx_time = 0;
static bool   s_connected = true;

static GFont s_font_time, s_font_title, s_font_value_l, s_font_value_m,
             s_font_unit, s_font_silk, s_font_ampm;

static inline GColor argb(uint8_t v) { return (GColor){ .argb = v }; }

// ---------------------------------------------------------------------------
// Zeichenhilfen
// ---------------------------------------------------------------------------
static void draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                      GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeFill, align, NULL);
}

static void draw_via(GContext *ctx, GPoint p, GColor hole) {
  graphics_context_set_fill_color(ctx, COLOR_PAD);
  graphics_fill_circle(ctx, p, VIA_R);
  graphics_context_set_fill_color(ctx, hole);
  graphics_fill_circle(ctx, p, VIA_HOLE_R);
}

static void draw_traces(GContext *ctx, const Theme *t) {
  graphics_context_set_stroke_color(ctx, argb(t->trace));
  graphics_context_set_stroke_width(ctx, TRACE_W);
  int n = ARRAY_LENGTH(TRACES);
  int i = 0;
  bool have_prev = false;
  GPoint prev = GPointZero;
  while (i < n) {
    int16_t v = TRACES[i];
    if (v == TRACE_VIA || v == TRACE_OPEN) {
      if (v == TRACE_VIA && have_prev) draw_via(ctx, prev, argb(t->board));
      have_prev = false;
      i++;
      continue;
    }
    GPoint p = GPoint(TRACES[i], TRACES[i + 1]);
    if (have_prev) graphics_draw_line(ctx, prev, p);
    prev = p;
    have_prev = true;
    i += 2;
  }
}

static GRect smd_rect(const Smd *s) {
  if (s->kind == SMD_SOT) return GRect(s->x, s->y - SOT_LEG, SOT_W, SOT_H + 2 * SOT_LEG);
  return s->vertical ? GRect(s->x, s->y, SMD_SHORT, SMD_LONG)
                     : GRect(s->x, s->y, SMD_LONG, SMD_SHORT);
}

static void draw_sot(GContext *ctx, const Smd *s) {
  graphics_context_set_fill_color(ctx, COLOR_SMD_END);
  graphics_fill_rect(ctx, GRect(s->x + 1, s->y + SOT_H, SOT_LEG, SOT_LEG), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(s->x + SOT_W - 1 - SOT_LEG, s->y + SOT_H, SOT_LEG, SOT_LEG),
                     0, GCornerNone);
  graphics_fill_rect(ctx, GRect(s->x + (SOT_W - SOT_LEG) / 2, s->y - SOT_LEG, SOT_LEG, SOT_LEG),
                     0, GCornerNone);
  graphics_context_set_fill_color(ctx, COLOR_RES);
  graphics_fill_rect(ctx, GRect(s->x, s->y, SOT_W, SOT_H), 0, GCornerNone);
}

static void draw_smds(GContext *ctx) {
  for (int i = 0; i < (int)ARRAY_LENGTH(SMDS); i++) {
    const Smd *s = &SMDS[i];
    if (s->kind == SMD_SOT) { draw_sot(ctx, s); continue; }
    GRect r = smd_rect(s);
    graphics_context_set_fill_color(ctx, COLOR_SMD_END);
    graphics_fill_rect(ctx, r, 0, GCornerNone);
    graphics_context_set_fill_color(ctx, s->kind == SMD_CAP ? COLOR_CAP : COLOR_RES);
    if (s->vertical) {
      graphics_fill_rect(ctx, GRect(r.origin.x, r.origin.y + SMD_END, r.size.w,
                                    r.size.h - 2 * SMD_END), 0, GCornerNone);
    } else {
      graphics_fill_rect(ctx, GRect(r.origin.x + SMD_END, r.origin.y,
                                    r.size.w - 2 * SMD_END, r.size.h), 0, GCornerNone);
    }
  }
}

static void draw_silks(GContext *ctx, const Theme *t) {
  for (int i = 0; i < (int)ARRAY_LENGTH(SILKS); i++) {
    draw_text(ctx, SILKS[i].text, s_font_silk,
              GRect(SILKS[i].x, SILKS[i].y, 30, 12),
              GTextAlignmentLeft, argb(t->silk));
  }
}

// Pins gleichmaessig entlang einer Kante verteilen, mittig zentriert
static void draw_pin_row(GContext *ctx, int start, int length, int fixed,
                         bool horizontal) {
  int count = (length - 2 * PIN_INSET) / PIN_PITCH + 1;
  if (count < 1) return;
  int span = (count - 1) * PIN_PITCH + PIN_W;
  int first = start + (length - span) / 2;
  for (int k = 0; k < count; k++) {
    int pos = first + k * PIN_PITCH;
    GRect r = horizontal ? GRect(pos, fixed, PIN_W, PIN_LEN)
                         : GRect(fixed, pos, PIN_LEN, PIN_W);
    graphics_fill_rect(ctx, r, 0, GCornerNone);
  }
}

static void draw_chip(GContext *ctx, const Theme *t, GRect r, bool side_pins) {
  graphics_context_set_fill_color(ctx, COLOR_PIN);
  draw_pin_row(ctx, r.origin.x, r.size.w, r.origin.y - PIN_LEN, true);
  draw_pin_row(ctx, r.origin.x, r.size.w, r.origin.y + r.size.h, true);
  if (side_pins) {
    draw_pin_row(ctx, r.origin.y, r.size.h, r.origin.x - PIN_LEN, false);
    draw_pin_row(ctx, r.origin.y, r.size.h, r.origin.x + r.size.w, false);
  }
  graphics_context_set_fill_color(ctx, argb(t->chip));
  graphics_fill_rect(ctx, r, CHIP_RADIUS, GCornersAll);
  graphics_context_set_stroke_color(ctx, argb(t->edge));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, r, CHIP_RADIUS);
  // Markierung fuer Pin 1
  graphics_context_set_fill_color(ctx, argb(t->edge));
  graphics_fill_circle(ctx, GPoint(r.origin.x + r.size.w - 6, r.origin.y + 6), 2);
}

static void draw_chip_title(GContext *ctx, GRect r, const char *title) {
  draw_text(ctx, title, s_font_title,
            GRect(r.origin.x + CHIP_PAD, r.origin.y + TITLE_Y, r.size.w - 2 * CHIP_PAD, 16),
            GTextAlignmentLeft, COLOR_TITLE);
}

// ---------------------------------------------------------------------------
// Wettersymbole, weiss in einem ICON_W x ICON_H Feld
// ---------------------------------------------------------------------------
static void draw_sun(GContext *ctx, GPoint c, int r) {
  graphics_context_set_fill_color(ctx, COLOR_VALUE);
  graphics_context_set_stroke_color(ctx, COLOR_VALUE);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_fill_circle(ctx, c, r);
  for (int k = 0; k < 8; k++) {
    int32_t a = TRIG_MAX_ANGLE * k / 8;
    int s = sin_lookup(a), co = cos_lookup(a);
    GPoint p0 = GPoint(c.x + (r + 2) * s / TRIG_MAX_RATIO, c.y - (r + 2) * co / TRIG_MAX_RATIO);
    GPoint p1 = GPoint(c.x + (r + 4) * s / TRIG_MAX_RATIO, c.y - (r + 4) * co / TRIG_MAX_RATIO);
    graphics_draw_line(ctx, p0, p1);
  }
}

static void draw_moon(GContext *ctx, GPoint c, int r, GColor bg) {
  graphics_context_set_fill_color(ctx, COLOR_VALUE);
  graphics_fill_circle(ctx, c, r);
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_circle(ctx, GPoint(c.x + r / 2 + 1, c.y - r / 2), r - 1);
}

static void draw_cloud(GContext *ctx, GPoint o, GColor fill, GColor bg) {
  // o = linke obere Ecke eines 18 x 11 Felds
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_circle(ctx, GPoint(o.x + 6, o.y + 6), 6);
  graphics_fill_circle(ctx, GPoint(o.x + 11, o.y + 4), 6);
  graphics_fill_rect(ctx, GRect(o.x - 1, o.y + 5, 20, 8), 3, GCornersAll);
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_circle(ctx, GPoint(o.x + 6, o.y + 6), 4);
  graphics_fill_circle(ctx, GPoint(o.x + 11, o.y + 4), 5);
  graphics_fill_rect(ctx, GRect(o.x + 1, o.y + 6, 16, 6), 3, GCornersAll);
}

static void draw_weather_icon(GContext *ctx, GRect box, int cond, GColor bg) {
  GPoint o = box.origin;
  graphics_context_set_stroke_color(ctx, COLOR_VALUE);
  graphics_context_set_stroke_width(ctx, 1);
  switch (cond) {
    case COND_CLEAR:
      draw_sun(ctx, GPoint(o.x + 11, o.y + 10), 4);
      break;
    case COND_CLEAR_NIGHT:
      draw_moon(ctx, GPoint(o.x + 10, o.y + 10), 7, bg);
      break;
    case COND_PARTLY:
      draw_sun(ctx, GPoint(o.x + 14, o.y + 6), 3);
      draw_cloud(ctx, GPoint(o.x + 1, o.y + 7), COLOR_VALUE, bg);
      break;
    case COND_PARTLY_NIGHT:
      draw_moon(ctx, GPoint(o.x + 14, o.y + 6), 5, bg);
      draw_cloud(ctx, GPoint(o.x + 1, o.y + 7), COLOR_VALUE, bg);
      break;
    case COND_CLOUDY:
      draw_cloud(ctx, GPoint(o.x + 2, o.y + 4), COLOR_VALUE, bg);
      break;
    case COND_FOG:
      for (int k = 0; k < 4; k++) {
        int y = o.y + 4 + k * 4;
        graphics_draw_line(ctx, GPoint(o.x + 2 + (k & 1) * 3, y), GPoint(o.x + 19 - (k & 1) * 2, y));
      }
      break;
    case COND_RAIN:
    case COND_SNOW:
    case COND_STORM:
      draw_cloud(ctx, GPoint(o.x + 2, o.y + 1), COLOR_VALUE, bg);
      if (cond == COND_RAIN) {
        for (int k = 0; k < 3; k++) {
          int x = o.x + 6 + k * 5;
          graphics_draw_line(ctx, GPoint(x, o.y + 15), GPoint(x - 2, o.y + 19));
        }
      } else if (cond == COND_SNOW) {
        graphics_context_set_fill_color(ctx, COLOR_VALUE);
        for (int k = 0; k < 3; k++) {
          graphics_fill_rect(ctx, GRect(o.x + 5 + k * 5, o.y + 16 + (k & 1) * 2, 2, 2), 0, GCornerNone);
        }
      } else {
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_line(ctx, GPoint(o.x + 12, o.y + 13), GPoint(o.x + 9, o.y + 17));
        graphics_draw_line(ctx, GPoint(o.x + 9, o.y + 17), GPoint(o.x + 13, o.y + 17));
        graphics_draw_line(ctx, GPoint(o.x + 13, o.y + 17), GPoint(o.x + 10, o.y + 21));
      }
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Daten
// ---------------------------------------------------------------------------
static void upcase(char *s) {
  for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 'a' - 'A';
}

static int steps_today(void) {
#ifdef DEMO
  return 6842;
#else
  return (int)health_service_sum_today(HealthMetricStepCount);
#endif
}

static int meters_today(void) {
#ifdef DEMO
  return 4870;
#else
  return (int)health_service_sum_today(HealthMetricWalkedDistanceMeters);
#endif
}

static int kcal_today(void) {
#ifdef DEMO
  return 214;
#else
  return (int)health_service_sum_today(HealthMetricActiveKCalories);
#endif
}

#ifdef PBL_PLATFORM_EMERY
static int heart_rate(void) {
#ifdef DEMO
  return 72;
#else
  return (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
#endif
}
#endif

static int battery_pct(bool *charging) {
  BatteryChargeState b = battery_state_service_peek();
  *charging = b.is_charging;
#ifdef DEMO
  return 80;
#else
  return b.charge_percent;
#endif
}

static void format_distance(char *buf, size_t len, const char **unit) {
  int m = meters_today();
  if (m < 0) m = 0;
  bool imperial = health_service_get_measurement_system_for_display(
      HealthMetricWalkedDistanceMeters) == MeasurementSystemImperial;
  // Hundertstel der Anzeigeeinheit, geklemmt fuer -Wformat-truncation
  int hundredths = imperial ? (int)((int64_t)m * 100 / 1609) : m / 10;
  if (hundredths > 99999) hundredths = 99999;
  *unit = imperial ? "mi" : "km";
  if (hundredths < 1000) {
    snprintf(buf, len, "%d.%02d", hundredths / 100, hundredths % 100);
  } else {
    snprintf(buf, len, "%d.%d", hundredths / 100, (hundredths % 100) / 10);
  }
}

// ---------------------------------------------------------------------------
// Bereiche
// ---------------------------------------------------------------------------
static void draw_weather(GContext *ctx, const Theme *t) {
  GRect r = RECT_WEATHER;
  draw_chip(ctx, t, r, true);
  draw_chip_title(ctx, r, "WEATHER");

  char buf[8];
  bool fresh = s_cond != COND_NONE && time(NULL) - s_wx_time < WEATHER_STALE_SEC;
#ifdef DEMO
  fresh = true;
  s_temp = 17;
  s_cond = COND_PARTLY;
#endif
  if (fresh) {
    int temp = s_temp;
    if (temp < -99) temp = -99;
    if (temp > 999) temp = 999;
    snprintf(buf, sizeof(buf), "%d°", temp);
    draw_weather_icon(ctx, GRect(r.origin.x + CHIP_PAD, r.origin.y + 22, ICON_W, ICON_H),
                      s_cond, argb(t->chip));
  } else {
    snprintf(buf, sizeof(buf), "--°");
  }
  draw_text(ctx, buf, s_font_value_l,
            GRect(r.origin.x + CHIP_PAD + ICON_W, r.origin.y + 14,
                  r.size.w - 2 * CHIP_PAD - ICON_W, 32),
            GTextAlignmentRight, COLOR_VALUE);
}

static void draw_power(GContext *ctx, const Theme *t) {
  GRect r = RECT_POWER;
  draw_chip(ctx, t, r, false);
  bool charging;
  int pct = battery_pct(&charging);
  draw_chip_title(ctx, r, charging ? "CHG" : "PWR");
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  draw_text(ctx, buf, s_font_value_m,
            GRect(r.origin.x + CHIP_PAD, r.origin.y + 10, r.size.w - 2 * CHIP_PAD, 22),
            GTextAlignmentLeft, COLOR_VALUE);
}

static void draw_date(GContext *ctx, const Theme *t, const struct tm *now) {
  GRect r = RECT_DATE;
  char day[12];
  strftime(day, sizeof(day), "%a %d", now);
  upcase(day);
#ifdef PBL_PLATFORM_EMERY
  draw_chip(ctx, t, r, false);
  draw_text(ctx, day, s_font_value_m,
            GRect(r.origin.x, r.origin.y - 3, r.size.w, 22),
            GTextAlignmentCenter, COLOR_VALUE);
#else
  draw_chip(ctx, t, r, false);
  char month[8];
  strftime(month, sizeof(month), "%b", now);
  upcase(month);
  draw_chip_title(ctx, r, month);
  draw_text(ctx, day, s_font_value_m,
            GRect(r.origin.x + CHIP_PAD, r.origin.y + 10, r.size.w - 2 * CHIP_PAD, 22),
            GTextAlignmentLeft, COLOR_VALUE);
#endif
}

#ifdef PBL_PLATFORM_EMERY
static void draw_heart(GContext *ctx, const Theme *t) {
  GRect r = RECT_HEART;
  draw_chip(ctx, t, r, true);
  draw_chip_title(ctx, r, "HEART");
  char buf[8];
  int bpm = heart_rate();
  if (bpm > 0 && bpm < 255) snprintf(buf, sizeof(buf), "%d", bpm);
  else snprintf(buf, sizeof(buf), "--");
  draw_text(ctx, buf, s_font_value_l,
            GRect(r.origin.x + CHIP_PAD, r.origin.y + 8, r.size.w - 2 * CHIP_PAD, 32),
            GTextAlignmentLeft, COLOR_VALUE);
  draw_text(ctx, "bpm", s_font_unit,
            GRect(r.origin.x + CHIP_PAD, r.origin.y + 24, r.size.w - 2 * CHIP_PAD, 16),
            GTextAlignmentRight, COLOR_TITLE);
}
#endif

static void draw_activity_row(GContext *ctx, GRect r, int row, const char *value,
                              const char *unit) {
  int y = r.origin.y + 13 + row * ROW_H;
  GRect box = GRect(r.origin.x + CHIP_PAD, y, r.size.w - 2 * CHIP_PAD, 20);
  draw_text(ctx, value, s_font_value_m, GRect(box.origin.x, y - 4, box.size.w, 22),
            GTextAlignmentLeft, COLOR_VALUE);
  draw_text(ctx, unit, s_font_unit, GRect(box.origin.x, y, box.size.w, 16),
            GTextAlignmentRight, COLOR_TITLE);
}

static void draw_activity(GContext *ctx, const Theme *t) {
  GRect r = RECT_ACTIVITY;
  draw_chip(ctx, t, r, true);
  draw_chip_title(ctx, r, "ACTIVITY");

  char steps[8], dist[8], kcal[8];
  const char *dist_unit;
  int s = steps_today();
  if (s < 0) s = 0;
  if (s > 999999) s = 999999;
  snprintf(steps, sizeof(steps), "%d", s);
  format_distance(dist, sizeof(dist), &dist_unit);
  draw_activity_row(ctx, r, 0, steps, "steps");
  draw_activity_row(ctx, r, 1, dist, dist_unit);
  if (ACTIVITY_ROWS > 2) {
    int k = kcal_today();
    if (k < 0) k = 0;
    if (k > 99999) k = 99999;
    snprintf(kcal, sizeof(kcal), "%d", k);
    draw_activity_row(ctx, r, 2, kcal, "kcal");
  }
}

static void draw_led(GContext *ctx, const Theme *t) {
  GPoint p = POS_LED;
  graphics_context_set_fill_color(ctx, COLOR_SMD_END);
  graphics_fill_rect(ctx, GRect(p.x, p.y, LED_W, LED_H), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, s_connected ? COLOR_LED_ON : COLOR_LED_OFF);
  graphics_fill_rect(ctx, GRect(p.x + 2, p.y, LED_W - 4, LED_H), 0, GCornerNone);
  GPoint l = POS_LED_LABEL;
  draw_text(ctx, "BT", s_font_silk, GRect(l.x, l.y, 20, 12), GTextAlignmentLeft,
            argb(t->silk));
}

static void draw_time(GContext *ctx, const Theme *t, const struct tm *now) {
  GRect w = RECT_WINDOW;
  graphics_context_set_fill_color(ctx, argb(t->window));
  graphics_fill_rect(ctx, w, WINDOW_RADIUS, GCornersAll);
  graphics_context_set_stroke_color(ctx, argb(t->trace));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, w, WINDOW_RADIUS);

  char buf[8];
  strftime(buf, sizeof(buf), clock_is_24h_style() ? "%H:%M" : "%I:%M", now);
  const char *text = (!clock_is_24h_style() && buf[0] == '0') ? buf + 1 : buf;
  GSize size = graphics_text_layout_get_content_size(text, s_font_time, w,
                                                     GTextOverflowModeFill,
                                                     GTextAlignmentCenter);
  int y = w.origin.y + (w.size.h - size.h) / 2 - TIME_Y_NUDGE;
  draw_text(ctx, text, s_font_time, GRect(w.origin.x, y, w.size.w, size.h + 8),
            GTextAlignmentCenter, COLOR_VALUE);
}

// AM / PM / 24H als Bestueckungsdruck, das zutreffende leuchtet weiss
static void draw_ampm(GContext *ctx, const Theme *t, const struct tm *now) {
  GRect r = RECT_AMPM;
  static const char *const LABELS[] = { "AM", "PM", "24H" };
  int active = clock_is_24h_style() ? 2 : (now->tm_hour < 12 ? 0 : 1);
  int col_w = r.size.w / 3;
  for (int k = 0; k < 3; k++) {
    draw_text(ctx, LABELS[k], s_font_ampm,
              GRect(r.origin.x + k * col_w, r.origin.y - 3, col_w, r.size.h),
              GTextAlignmentCenter, k == active ? COLOR_VALUE : argb(t->trace));
  }
}

#ifdef PBL_PLATFORM_EMERY
static void draw_edge(GContext *ctx) {
  graphics_context_set_fill_color(ctx, COLOR_PAD);
  for (int y = FINGER_Y0; y + FINGER_H <= FINGER_Y1; y += FINGER_PITCH) {
    graphics_fill_rect(ctx, GRect(FINGER_X, y, FINGER_W, FINGER_H), 0, GCornerNone);
  }
}
#else
static bool near_rect(GPoint p, GRect r, int d) {
  return p.x >= r.origin.x - d && p.x <= r.origin.x + r.size.w + d &&
         p.y >= r.origin.y - d && p.y <= r.origin.y + r.size.h + d;
}

// Bohrungen rund um den Rand, ausser wo ein Chip oder eine Bahn im Weg ist
static void draw_edge(GContext *ctx, const Theme *t) {
  const GRect blocks[] = {
    RECT_WEATHER, RECT_POWER, RECT_DATE, RECT_WINDOW, RECT_ACTIVITY,
  };
  for (int k = 0; k < RIM_STEPS; k++) {
    int32_t a = TRIG_MAX_ANGLE * k / RIM_STEPS;
    GPoint p = GPoint(RIM_CX + RIM_R * sin_lookup(a) / TRIG_MAX_RATIO,
                      RIM_CY - RIM_R * cos_lookup(a) / TRIG_MAX_RATIO);
    bool blocked = false;
    for (int b = 0; b < (int)ARRAY_LENGTH(blocks); b++) {
      if (near_rect(p, blocks[b], RIM_CLEARANCE)) { blocked = true; break; }
    }
    for (int i = 0; !blocked && i < (int)ARRAY_LENGTH(SMDS); i++) {
      if (near_rect(p, smd_rect(&SMDS[i]), RIM_CLEARANCE)) blocked = true;
    }
    // auch nicht auf Endpunkte von Leiterbahnen
    for (int i = 0; !blocked && i + 1 < (int)ARRAY_LENGTH(TRACES); ) {
      if (TRACES[i] < 0) { i++; continue; }
      int dx = TRACES[i] - p.x, dy = TRACES[i + 1] - p.y;
      if (dx * dx + dy * dy < RIM_CLEARANCE * RIM_CLEARANCE * 2) blocked = true;
      i += 2;
    }
    if (!blocked) draw_via(ctx, p, argb(t->board));
  }
}
#endif

// ---------------------------------------------------------------------------
// Einziger Zeichen-Einstiegspunkt
// ---------------------------------------------------------------------------
static void canvas_update(Layer *layer, GContext *ctx) {
  const Theme *t = &THEMES[s_board];
  GRect bounds = layer_get_bounds(layer);
  time_t tt = time(NULL);
  struct tm *now = localtime(&tt);

  graphics_context_set_fill_color(ctx, argb(t->board));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  draw_traces(ctx, t);
  draw_smds(ctx);
  draw_silks(ctx, t);
#ifdef PBL_PLATFORM_EMERY
  draw_edge(ctx);
#else
  draw_edge(ctx, t);
#endif
  draw_led(ctx, t);

  draw_weather(ctx, t);
  draw_power(ctx, t);
  draw_date(ctx, t, now);
#ifdef PBL_PLATFORM_EMERY
  draw_heart(ctx, t);
#endif
  draw_activity(ctx, t);
  draw_ampm(ctx, t, now);
  draw_time(ctx, t, now);
}

// ---------------------------------------------------------------------------
// Ereignisse
// ---------------------------------------------------------------------------
static void request_weather(void) {
  if (!s_connected) return;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
    app_message_outbox_send();
  }
}

static void tick_handler(struct tm *now, TimeUnits changed) {
  if (now->tm_min % WEATHER_INTERVAL_MIN == 0) request_weather();
  layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
  layer_mark_dirty(s_canvas);
}

static void connection_handler(bool connected) {
  s_connected = connected;
  if (connected) request_weather();
  layer_mark_dirty(s_canvas);
}

static int tuple_int(Tuple *t) {
  return (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring)
                                    : (int)t->value->int32;
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_BOARD)) != NULL) {
    int v = tuple_int(t);
    if (v >= 0 && v < THEME_COUNT) {
      s_board = v;
      persist_write_int(PKEY_BOARD, v);
    }
  }
  Tuple *temp = dict_find(iter, MESSAGE_KEY_TEMP);
  Tuple *cond = dict_find(iter, MESSAGE_KEY_CONDITION);
  if (temp && cond) {
    s_temp = tuple_int(temp);
    s_cond = tuple_int(cond);
    s_wx_time = time(NULL);
    persist_write_int(PKEY_TEMP, s_temp);
    persist_write_int(PKEY_COND, s_cond);
    persist_write_int(PKEY_WX_TIME, (int)s_wx_time);
  }
  layer_mark_dirty(s_canvas);
}

static void load_settings(void) {
  if (persist_exists(PKEY_BOARD)) {
    int v = persist_read_int(PKEY_BOARD);
    if (v >= 0 && v < THEME_COUNT) s_board = v;
  }
  if (persist_exists(PKEY_COND)) {
    s_temp    = persist_read_int(PKEY_TEMP);
    s_cond    = persist_read_int(PKEY_COND);
    s_wx_time = (time_t)persist_read_int(PKEY_WX_TIME);
  }
}

// ---------------------------------------------------------------------------
// Fenster und Lebenszyklus
// ---------------------------------------------------------------------------
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  load_settings();
#ifdef DEMO
  s_board = DEMO_BOARD;
#endif

  s_font_time    = fonts_get_system_font(FONT_TIME);
  s_font_title   = fonts_get_system_font(FONT_TITLE);
  s_font_value_l = fonts_get_system_font(FONT_VALUE_L);
  s_font_value_m = fonts_get_system_font(FONT_VALUE_M);
  s_font_unit    = fonts_get_system_font(FONT_UNIT);
  s_font_silk    = fonts_get_system_font(FONT_SILK);
  s_font_ampm    = fonts_get_system_font(FONT_AMPM);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  s_connected = connection_service_peek_pebble_app_connection();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = connection_handler,
  });

  app_message_register_inbox_received(inbox_received);
  AppMessageResult res = app_message_open(INBOX_SIZE, OUTBOX_SIZE);
  if (res != APP_MSG_OK) APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_open: %d", (int)res);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
