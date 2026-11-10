/*
 * AmoledInkTest - display quality test + velocity-sensitive drawing
 *
 * Target: ESP32-S3 board with PY206-W38-V2 panel
 *   - 2.06" AMOLED 410x502, CO5300AF-51 driver over QSPI
 *   - CST9217 capacitive touch over I2C (0x5A)
 *   - ESP32-S3 with 8 MB OPI PSRAM, 16 MB flash
 *
 * Library: "GFX Library for Arduino" by Moon On Our Nation (Arduino_GFX), v1.5+
 *
 * Arduino IDE board settings:
 *   Board:            ESP32S3 Dev Module
 *   USB CDC On Boot:  Enabled        (Serial over USB)
 *   Flash Size:       16MB (must not exceed the real chip, or it boot-loops in the ROM)
 *   PSRAM:            OPI PSRAM      (REQUIRED - framebuffer lives there)
 *   Partition Scheme: anything with >= 1.5 MB app
 *
 * Usage:
 *   - Draw anywhere between the top status line and the bottom toolbar.
 *     Slow strokes = thick, fast strokes = thin (tap INV to swap).
 *   - Toolbar: 5 ink colours | INV | BRT (brightness) | TST (test patterns) | CLR
 *   - TST: tap the screen to step through full-screen test patterns.
 *     After the last pattern your drawing comes back.
 *   - BOOT button: clear (in draw mode) / leave test patterns.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

enum TouchEvent { TOUCH_NO_DATA, TOUCH_POINT, TOUCH_LIFT };

// ---------------------------------------------------------------------------
// Board pins
// Measured on the board: SDIO0, SCLK, CS, IIC_SDA, IIC_SCL.
// Inferred from the panel's FPC pin order: SDIO1-3, LCD_RESET, TP_INT, TP_RESET.
// ---------------------------------------------------------------------------
#define LCD_SDIO0   14
#define LCD_SDIO1   21
#define LCD_SDIO2   17
#define LCD_SDIO3   16
#define LCD_SCLK    15
#define LCD_CS      18
#define LCD_RESET   13
#define LCD_WIDTH   410
#define LCD_HEIGHT  502
#define LCD_COL_OFS 22

// Panel power IC enable (OLE_EN): both pins have pull-downs, not yet known which it is
#define OLED_EN_A   1
#define OLED_EN_B   4

#define IIC_SDA     10
#define IIC_SCL     9
#define TP_INT      11
#define TP_RESET    12
#define TP_ADDR     0x5A

#define BOOT_BTN    0

// If touch is mirrored/rotated relative to the picture, fix it here.
#define TOUCH_SWAP_XY  0
#define TOUCH_FLIP_X   0
#define TOUCH_FLIP_Y   0

#define TOUCH_TRACE    0   // 1 = log every touch report to Serial

static const uint32_t TOUCH_POLL_US = 25000;    // safety read when INT has been silent this long
static const uint32_t TOUCH_TIMEOUT_MS = 120;   // no reports this long = finger gone

// ---------------------------------------------------------------------------
// Ink tuning
// ---------------------------------------------------------------------------
static const float R_MIN        = 0.6f;   // thinnest stroke radius (px)
static const float R_MAX        = 13.0f;  // thickest stroke radius (px)
static const float V_SLOW       = 0.05f;  // px/ms - at or below: full thickness
static const float V_FAST       = 2.2f;   // px/ms - at or above: min thickness
static const float V_CURVE      = 0.6f;   // <1 thins out earlier, >1 later
static const float V_SMOOTH     = 0.30f;  // speed EMA factor (0..1, higher = snappier)
static const float R_SMOOTH     = 0.25f;  // radius EMA factor
static const float POS_SMOOTH   = 0.65f;  // position EMA factor (1 = raw)

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
static const int W = LCD_WIDTH;
static const int H = LCD_HEIGHT;
static const int HUD_H = 40;                 // status line at top
static const int TB_H = 46;                  // toolbar height
static const int TB_Y = H - TB_H - 16;       // toolbar top (clear of rounded corners)
static const int DRAW_Y0 = HUD_H;            // drawable area [DRAW_Y0, DRAW_Y1)
static const int DRAW_Y1 = TB_Y - 4;

static const int BTN_N = 9;
static const int BTN_W = 38;
static const int BTN_GAP = 2;
static const int BTN_X0 = (W - (BTN_N * BTN_W + (BTN_N - 1) * BTN_GAP)) / 2;

#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_BLACK RGB(0, 0, 0)
#define C_WHITE RGB(255, 255, 255)

static const uint16_t INK[5] = {
  RGB(255, 255, 255), RGB(255, 40, 40), RGB(40, 255, 60), RGB(50, 120, 255), RGB(255, 220, 0)
};
static const uint8_t BRIGHT[] = { 255, 160, 80, 30, 8 };

// ---------------------------------------------------------------------------
// Display objects
// ---------------------------------------------------------------------------
Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(bus, LCD_RESET, 0, W, H, LCD_COL_OFS, 0, 0, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(W, H, panel);

static uint16_t *fb = nullptr;       // canvas framebuffer (PSRAM)
static uint16_t *fbBackup = nullptr; // saved drawing while showing test patterns
static uint8_t *cov = nullptr;       // per-stroke coverage buffer (PSRAM)

// Staging buffer in internal RAM for partial flushes (CO5300 wants contiguous rects)
static const int STAGE_PIX = W * 32;
static uint16_t *stage = nullptr;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static int inkIdx = 0;
static int brightIdx = 0;
static bool invertMap = false;

static bool testMode = false;
static int testIdx = 0;

static bool wasTouched = false;
static bool strokeActive = false;
static float px, py, pr;             // last stroke point / radius
static float speedEma = 0;
static uint32_t lastUs = 0;
static int sbx0, sby0, sbx1, sby1;   // current stroke bbox (to clear cov)

static int dx0 = W, dy0 = H, dx1 = 0, dy1 = 0;  // dirty rect (exclusive max)
static uint32_t lastFlushMs = 0;
static uint32_t flushCount = 0, fpsTimer = 0;
static float fps = 0;
static uint32_t lastHudMs = 0;

// ---------------------------------------------------------------------------
// Dirty-rect handling
// ---------------------------------------------------------------------------
static void markDirty(int x0, int y0, int x1, int y1) {
  if (x0 < dx0) dx0 = x0;
  if (y0 < dy0) dy0 = y0;
  if (x1 > dx1) dx1 = x1;
  if (y1 > dy1) dy1 = y1;
}

static void flushRect(int rx0, int ry0, int rx1, int ry1) {
  // CO5300 needs even start and even size for partial windows
  int x0 = max(0, rx0) & ~1, y0 = max(0, ry0) & ~1;
  int x1 = min(W, (rx1 + 1) & ~1), y1 = min(H, (ry1 + 1) & ~1);
  int w = x1 - x0;
  if (w <= 0 || y1 <= y0) return;
  int rowsPerChunk = (STAGE_PIX / w) & ~1;
  for (int y = y0; y < y1; y += rowsPerChunk) {
    int rows = min(rowsPerChunk, y1 - y);
    for (int r = 0; r < rows; r++)
      memcpy(stage + r * w, fb + (y + r) * W + x0, w * 2);
    panel->draw16bitRGBBitmap(x0, y, stage, w, rows);
  }
}

static void flushDirty() {
  if (dx1 <= dx0 || dy1 <= dy0) return;
  flushRect(dx0, dy0, dx1, dy1);
  dx0 = W; dy0 = H; dx1 = 0; dy1 = 0;
  flushCount++;
}

static void flushAll() {
  gfx->flush();
  dx0 = W; dy0 = H; dx1 = 0; dy1 = 0;
}

// ---------------------------------------------------------------------------
// Anti-aliased variable-width stroke
//
// Each segment is a tapered capsule rendered with per-pixel coverage.
// Coverage is max-combined per stroke (cov buffer), so overlapping segments
// never double-darken their edges: going from coverage a -> b we blend the
// pixel toward the ink by k = (b - a) / (1 - a), which is exact.
// ---------------------------------------------------------------------------
static inline uint16_t blend565(uint16_t d, uint16_t s, uint32_t k /*0..256*/) {
  int dr = d >> 11, dg = (d >> 5) & 63, db = d & 31;
  int sr = s >> 11, sg = (s >> 5) & 63, sb = s & 31;
  dr += ((sr - dr) * (int)k + 128) >> 8;
  dg += ((sg - dg) * (int)k + 128) >> 8;
  db += ((sb - db) * (int)k + 128) >> 8;
  return (dr << 11) | (dg << 5) | db;
}

static void drawSegment(float x0, float y0, float r0, float x1, float y1, float r1, uint16_t color) {
  float rm = max(r0, r1) + 1.0f;
  int bx0 = max(0, (int)floorf(min(x0, x1) - rm));
  int bx1 = min(W, (int)ceilf(max(x0, x1) + rm) + 1);
  int by0 = max(DRAW_Y0, (int)floorf(min(y0, y1) - rm));
  int by1 = min(DRAW_Y1, (int)ceilf(max(y0, y1) + rm) + 1);
  if (bx1 <= bx0 || by1 <= by0) return;

  float sx = x1 - x0, sy = y1 - y0;
  float L2 = sx * sx + sy * sy;
  float invL2 = L2 > 1e-6f ? 1.0f / L2 : 0.0f;

  for (int y = by0; y < by1; y++) {
    uint16_t *row = fb + y * W;
    uint8_t *crow = cov + y * W;
    float ry = y - y0;
    for (int x = bx0; x < bx1; x++) {
      float rx = x - x0;
      float t = (rx * sx + ry * sy) * invL2;
      t = t < 0 ? 0 : (t > 1 ? 1 : t);
      float ex = rx - t * sx, ey = ry - t * sy;
      float d2 = ex * ex + ey * ey;
      float rr = r0 + (r1 - r0) * t;
      float lim = rr + 0.5f;
      if (d2 >= lim * lim) continue;
      float c = lim - sqrtf(d2);
      int c8 = c >= 1.0f ? 255 : (int)(c * 255.0f);
      int old = crow[x];
      if (c8 <= old) continue;
      uint32_t k = ((uint32_t)(c8 - old) << 8) / (uint32_t)(255 - old);
      row[x] = blend565(row[x], color, k);
      crow[x] = (uint8_t)c8;
    }
  }

  if (bx0 < sbx0) sbx0 = bx0;
  if (by0 < sby0) sby0 = by0;
  if (bx1 > sbx1) sbx1 = bx1;
  if (by1 > sby1) sby1 = by1;
  markDirty(bx0, by0, bx1, by1);
}

static float radiusForSpeed(float v) {
  float n = (v - V_SLOW) / (V_FAST - V_SLOW);
  n = n < 0 ? 0 : (n > 1 ? 1 : n);
  n = powf(n, V_CURVE);
  return invertMap ? R_MIN + (R_MAX - R_MIN) * n : R_MAX - (R_MAX - R_MIN) * n;
}

static void beginStroke(float x, float y) {
  // wipe coverage left over from the previous stroke
  if (sbx1 > sbx0) {
    for (int yy = sby0; yy < sby1; yy++) memset(cov + yy * W + sbx0, 0, sbx1 - sbx0);
  }
  sbx0 = W; sby0 = H; sbx1 = 0; sby1 = 0;
  px = x; py = y;
  speedEma = 0;
  pr = radiusForSpeed(0) * (invertMap ? 1.0f : 0.7f);
  lastUs = micros();
  strokeActive = true;
  drawSegment(px, py, pr, px, py, pr, INK[inkIdx]);
}

static void continueStroke(float x, float y) {
  // light position smoothing to suppress touch jitter
  x = px + (x - px) * POS_SMOOTH;
  y = py + (y - py) * POS_SMOOTH;
  float ddx = x - px, ddy = y - py;
  float d = sqrtf(ddx * ddx + ddy * ddy);
  if (d < 0.75f) return;  // no movement: keep lastUs so dt spans the pause
  uint32_t now = micros();
  float dtMs = (now - lastUs) / 1000.0f;
  if (dtMs < 0.5f) dtMs = 0.5f;
  lastUs = now;

  float v = d / dtMs;
  speedEma += (v - speedEma) * V_SMOOTH;
  float target = radiusForSpeed(speedEma);
  float r = pr + (target - pr) * R_SMOOTH;

  drawSegment(px, py, pr, x, y, r, INK[inkIdx]);
  px = x; py = y; pr = r;
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
static void btnRect(int i, int &x, int &y) {
  x = BTN_X0 + i * (BTN_W + BTN_GAP);
  y = TB_Y;
}

static void drawToolbar() {
  gfx->fillRect(0, DRAW_Y1, W, H - DRAW_Y1, C_BLACK);
  gfx->drawFastHLine(20, DRAW_Y1 + 1, W - 40, RGB(40, 40, 40));
  gfx->setTextSize(2);
  for (int i = 0; i < BTN_N; i++) {
    int x, y;
    btnRect(i, x, y);
    if (i < 5) {
      gfx->fillRoundRect(x + 3, y + 5, BTN_W - 6, TB_H - 10, 6, INK[i]);
      if (i == inkIdx) {
        gfx->drawRoundRect(x, y + 2, BTN_W, TB_H - 4, 8, C_WHITE);
        gfx->drawRoundRect(x + 1, y + 3, BTN_W - 2, TB_H - 6, 7, C_WHITE);
      }
    } else {
      const char *lbl[] = { "INV", "BRT", "TST", "CLR" };
      bool on = (i == 5 && invertMap);
      gfx->drawRoundRect(x, y + 2, BTN_W, TB_H - 4, 8, RGB(110, 110, 110));
      if (on) gfx->fillRoundRect(x + 1, y + 3, BTN_W - 2, TB_H - 6, 7, RGB(60, 60, 60));
      // labels are 3 chars at size 1 to fit 38px buttons
      gfx->setTextSize(1);
      gfx->setTextColor(C_WHITE);
      gfx->setCursor(x + (BTN_W - 18) / 2, y + TB_H / 2 - 4);
      gfx->print(lbl[i - 5]);
    }
  }
  markDirty(0, DRAW_Y1, W, H);
}

static void drawHud() {
  gfx->fillRect(0, 0, W, HUD_H, C_BLACK);
  gfx->setTextSize(1);
  gfx->setTextColor(RGB(150, 150, 150));
  char buf[64];
  snprintf(buf, sizeof(buf), "v %4.2f px/ms  r %4.1f  %s", speedEma, strokeActive ? pr : radiusForSpeed(0),
           invertMap ? "fast=thick" : "fast=thin");
  int tw = strlen(buf) * 6;
  gfx->setCursor((W - tw) / 2, 14);
  gfx->print(buf);
  snprintf(buf, sizeof(buf), "%.0f updates/s  bright %d", fps, BRIGHT[brightIdx]);
  tw = strlen(buf) * 6;
  gfx->setCursor((W - tw) / 2, 26);
  gfx->print(buf);
  flushRect(0, 0, W, HUD_H);  // pushed separately so it never inflates the stroke dirty rect
}

static void clearDrawing() {
  gfx->fillRect(0, DRAW_Y0, W, DRAW_Y1 - DRAW_Y0, C_BLACK);
  memset(cov, 0, (size_t)W * H);
  sbx0 = W; sby0 = H; sbx1 = 0; sby1 = 0;
  markDirty(0, DRAW_Y0, W, DRAW_Y1);
}

// ---------------------------------------------------------------------------
// Test patterns
// ---------------------------------------------------------------------------
static const char *PATTERN_NAMES[] = {
  "Black (stuck/lit pixels)",
  "White (dead pixels, uniformity, burn-in)",
  "Red", "Green", "Blue",
  "Gray 5% (low-level mura)",
  "Gray 20% (mura / tint)",
  "Gray 50%",
  "Gray + RGB ramps (banding; RGB565 = 32/64 steps)",
  "Color bars",
  "1px checkerboard (sharpness, flicker)",
  "1px horizontal lines",
  "Grid 10px + edge border (alignment, offset, corners)",
  "Radial sweep (gamma/colour shift)",
};
static const int PATTERN_N = sizeof(PATTERN_NAMES) / sizeof(PATTERN_NAMES[0]);

static uint16_t gray565(uint8_t v) { return RGB(v, v, v); }

static void drawPattern(int i) {
  switch (i) {
    case 0: gfx->fillScreen(C_BLACK); break;
    case 1: gfx->fillScreen(C_WHITE); break;
    case 2: gfx->fillScreen(RGB(255, 0, 0)); break;
    case 3: gfx->fillScreen(RGB(0, 255, 0)); break;
    case 4: gfx->fillScreen(RGB(0, 0, 255)); break;
    case 5: gfx->fillScreen(gray565(13)); break;
    case 6: gfx->fillScreen(gray565(51)); break;
    case 7: gfx->fillScreen(gray565(128)); break;
    case 8: {
      int bh = H / 4;
      for (int x = 0; x < W; x++) {
        uint8_t v = (uint32_t)x * 255 / (W - 1);
        gfx->drawFastVLine(x, 0, bh, gray565(v));
        gfx->drawFastVLine(x, bh, bh, RGB(v, 0, 0));
        gfx->drawFastVLine(x, 2 * bh, bh, RGB(0, v, 0));
        gfx->drawFastVLine(x, 3 * bh, H - 3 * bh, RGB(0, 0, v));
      }
      break;
    }
    case 9: {
      const uint16_t bars[8] = { C_WHITE, RGB(255, 255, 0), RGB(0, 255, 255), RGB(0, 255, 0),
                                 RGB(255, 0, 255), RGB(255, 0, 0), RGB(0, 0, 255), C_BLACK };
      for (int b = 0; b < 8; b++) {
        int x0 = b * W / 8, x1 = (b + 1) * W / 8;
        gfx->fillRect(x0, 0, x1 - x0, H, bars[b]);
      }
      break;
    }
    case 10:
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) fb[y * W + x] = ((x ^ y) & 1) ? C_WHITE : C_BLACK;
      break;
    case 11:
      for (int y = 0; y < H; y++) gfx->drawFastHLine(0, y, W, (y & 1) ? C_WHITE : C_BLACK);
      break;
    case 12:
      gfx->fillScreen(C_BLACK);
      for (int x = 0; x < W; x += 10) gfx->drawFastVLine(x, 0, H, (x % 50) ? RGB(60, 60, 60) : RGB(0, 160, 255));
      for (int y = 0; y < H; y += 10) gfx->drawFastHLine(0, y, W, (y % 50) ? RGB(60, 60, 60) : RGB(0, 160, 255));
      gfx->drawRect(0, 0, W, H, RGB(255, 0, 0));
      gfx->drawRect(1, 1, W - 2, H - 2, RGB(255, 255, 0));
      gfx->drawLine(0, 0, W - 1, H - 1, C_WHITE);
      gfx->drawLine(W - 1, 0, 0, H - 1, C_WHITE);
      break;
    case 13: {
      float cx = W / 2.0f, cy = H / 2.0f;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          float a = atan2f(y - cy, x - cx) / (2 * PI) + 0.5f;  // hue 0..1
          float h6 = a * 6;
          int seg = (int)h6;
          float f = h6 - seg;
          uint8_t up = f * 255, dn = 255 - up;
          uint8_t r, g, b;
          switch (seg % 6) {
            case 0: r = 255; g = up; b = 0; break;
            case 1: r = dn; g = 255; b = 0; break;
            case 2: r = 0; g = 255; b = up; break;
            case 3: r = 0; g = dn; b = 255; break;
            case 4: r = up; g = 0; b = 255; break;
            default: r = 255; g = 0; b = dn; break;
          }
          float dist = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) / cy;
          if (dist > 1) dist = 1;
          r = r * dist + 255 * (1 - dist);  // white in the centre
          g = g * dist + 255 * (1 - dist);
          b = b * dist + 255 * (1 - dist);
          fb[y * W + x] = RGB(r, g, b);
        }
      break;
    }
  }
  flushAll();
  Serial.printf("Pattern %d/%d: %s\n", i + 1, PATTERN_N, PATTERN_NAMES[i]);
}

static void enterTestMode() {
  memcpy(fbBackup, fb, (size_t)W * H * 2);
  testMode = true;
  testIdx = 0;
  drawPattern(testIdx);
}

static void exitTestMode() {
  testMode = false;
  memcpy(fb, fbBackup, (size_t)W * H * 2);
  drawHud();
  drawToolbar();
  flushAll();
}

// ---------------------------------------------------------------------------
// Touch (CST9217)
// ---------------------------------------------------------------------------
static uint32_t statPoint = 0, statNoData = 0, statLift = 0, statErr = 0;
static volatile uint32_t tpIntCount = 0;
// Worst case per second, reported in the touch/s line
static uint32_t maxReadUs = 0, maxSegUs = 0, maxHudUs = 0, maxFlushUs = 0, maxLoopUs = 0, maxGapMs = 0;

static void IRAM_ATTR onTouchInt() { tpIntCount = tpIntCount + 1; }

// The chip reports at its own rate; between reports it answers "no data",
// which says nothing about whether the finger is still down.
static void touchAck() {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0xD0);
  Wire.write(0x00);
  Wire.write(0xAB);
  uint8_t e = Wire.endTransmission();
  if (e != 0) { statErr++; if (TOUCH_TRACE) Serial.printf("E %lu ack %u\n", millis(), e); }
}

static TouchEvent touchRead(int &x, int &y, bool ackAlways) {
  uint8_t b[15];
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0xD0);  // read command 0xD000
  Wire.write(0x00);
  uint8_t e = Wire.endTransmission(false);
  if (e != 0) { statErr++; if (TOUCH_TRACE) Serial.printf("E %lu cmd %u\n", millis(), e); return TOUCH_NO_DATA; }
  int got = Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)sizeof(b));
  if (got != sizeof(b)) { statErr++; if (TOUCH_TRACE) Serial.printf("E %lu read %d\n", millis(), got); return TOUCH_NO_DATA; }
  for (size_t i = 0; i < sizeof(b); i++) b[i] = Wire.read();
  if (b[0] == 0xAB || b[0] == 0x00 || b[6] != 0xAB) {
    statNoData++;
    // An ack after "no data" can swallow a report that arrived in between, so
    // it is only sent by the slow safety read, where it clears a stuck chip.
    if (ackAlways) touchAck();
    return TOUCH_NO_DATA;
  }
  touchAck();

  int n = b[5] & 0x7F;
  if (TOUCH_TRACE)
    Serial.printf("T %lu %02X %02X %02X %02X %02X %02X %02X\n", millis(), b[0], b[1], b[2], b[3], b[4], b[5], b[6]);
  if (n == 0 || n > 2 || (b[0] & 0x0F) != 0x06) { statLift++; return TOUCH_LIFT; }
  statPoint++;
  int tx = (b[1] << 4) | (b[3] >> 4);
  int ty = (b[2] << 4) | (b[3] & 0x0F);
#if TOUCH_SWAP_XY
  int t = tx; tx = ty; ty = t;
#endif
#if TOUCH_FLIP_X
  tx = W - 1 - tx;
#endif
#if TOUCH_FLIP_Y
  ty = H - 1 - ty;
#endif
  x = constrain(tx, 0, W - 1);
  y = constrain(ty, 0, H - 1);
  return TOUCH_POINT;
}

static void touchInit() {
  pinMode(TP_RESET, OUTPUT);
  digitalWrite(TP_RESET, LOW);
  delay(10);
  digitalWrite(TP_RESET, HIGH);
  delay(100);
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), onTouchInt, FALLING);
  Wire.begin(IIC_SDA, IIC_SCL, 400000);
  Wire.setTimeOut(5);  // a failed read must not hold up drawing
  Wire.beginTransmission(TP_ADDR);
  uint8_t err = Wire.endTransmission();
  Serial.printf("Touch: %s\n", err == 0 ? "OK" : "NOT FOUND");
}

// ---------------------------------------------------------------------------
// Toolbar actions
// ---------------------------------------------------------------------------
static int hitButton(int x, int y) {
  if (y < TB_Y || y >= TB_Y + TB_H) return -1;
  for (int i = 0; i < BTN_N; i++) {
    int bx, by;
    btnRect(i, bx, by);
    if (x >= bx - BTN_GAP / 2 && x < bx + BTN_W + BTN_GAP / 2) return i;
  }
  return -1;
}

static void pressButton(int i) {
  if (i < 5) {
    inkIdx = i;
  } else if (i == 5) {
    invertMap = !invertMap;
  } else if (i == 6) {
    brightIdx = (brightIdx + 1) % sizeof(BRIGHT);
    panel->setBrightness(BRIGHT[brightIdx]);
  } else if (i == 7) {
    enterTestMode();
    return;
  } else if (i == 8) {
    clearDrawing();
  }
  drawToolbar();
  drawHud();
}

// ---------------------------------------------------------------------------
// Error screen drawn straight to the panel (no framebuffer needed).
// Text size 4 at even cursor positions keeps every pixel block 4x4 and
// even-aligned, which the CO5300 requires for partial windows.
static void fatal(const char *msg) {
  panel->fillScreen(RGB(160, 0, 0));
  panel->setTextSize(4);
  panel->setTextColor(C_WHITE);
  int y = 180;
  const char *p = msg;
  while (*p) {
    const char *nl = strchr(p, '\n');
    int len = nl ? nl - p : strlen(p);
    panel->setCursor(((W - len * 24) / 2) & ~3, y);
    for (int i = 0; i < len; i++) panel->write((uint8_t)p[i]);
    y += 44;
    p += len + (nl ? 1 : 0);
  }
  while (true) {
    Serial.printf("FATAL: %s\n", msg);
    delay(2000);
  }
}

void setup() {
  Serial.begin(115200);
  // With USB plugged in but no program reading the port, prints would block
  // the loop waiting for the buffer to drain. Drop the output instead.
  Serial.setTxTimeoutMs(0);
  delay(300);
  Serial.println("\nAmoledInkTest");

  // Turn on the panel's power IC before talking to it
  pinMode(OLED_EN_A, OUTPUT);
  digitalWrite(OLED_EN_A, HIGH);
  pinMode(OLED_EN_B, OUTPUT);
  digitalWrite(OLED_EN_B, HIGH);
  delay(50);

  // Bring up the panel on its own first, so we can show errors even if the
  // PSRAM framebuffer can't be allocated.
  if (!panel->begin()) {
    Serial.println("Display init failed");
    fatal("Display init failed");
  }
  panel->setBrightness(255);
  Serial.println("Display init OK - colour self-test");
  const uint16_t selfTest[] = { RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), C_WHITE };
  for (uint16_t c : selfTest) {
    panel->fillScreen(c);
    delay(400);
  }

  Serial.printf("PSRAM: %s, %u bytes free\n", psramFound() ? "found" : "NOT FOUND", ESP.getFreePsram());
  if (!psramFound()) fatal("NO PSRAM\nTools > PSRAM >\nOPI PSRAM");

  if (!gfx->begin(GFX_SKIP_OUTPUT_BEGIN)) fatal("Framebuffer\nalloc failed");
  fb = gfx->getFramebuffer();
  fbBackup = (uint16_t *)ps_malloc((size_t)W * H * 2);
  cov = (uint8_t *)ps_malloc((size_t)W * H);
  stage = (uint16_t *)heap_caps_malloc(STAGE_PIX * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!fb || !fbBackup || !cov || !stage) fatal("Out of memory");
  memset(cov, 0, (size_t)W * H);
  sbx0 = W; sby0 = H; sbx1 = 0; sby1 = 0;

  panel->setBrightness(BRIGHT[brightIdx]);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  touchInit();

  gfx->fillScreen(C_BLACK);
  drawHud();
  drawToolbar();
  flushAll();
  fpsTimer = millis();
}

void loop() {
  uint32_t now = millis();
  uint32_t loopStartUs = micros();

  // BOOT button
  static bool btnPrev = true;
  bool btn = digitalRead(BOOT_BTN);
  if (!btn && btnPrev) {
    if (testMode) exitTestMode();
    else clearDrawing();
  }
  btnPrev = btn;

  // Read when the chip signals a report, plus a slow safety read so a missed
  // interrupt can't stall touch. The finger state is held between reports.
  static int tx = -1, ty = -1;
  static bool touched = false;
  static uint32_t lastPollUs = 0, lastPointMs = 0, intsHandled = 0, statInts = 0, statRecovered = 0;
  bool newPoint = false;
  uint32_t ints = tpIntCount;
  bool intPending = ints != intsHandled;
  if (intPending || micros() - lastPollUs >= TOUCH_POLL_US) {
    statInts += ints - intsHandled;
    intsHandled = ints;
    lastPollUs = micros();
    TouchEvent ev = touchRead(tx, ty, !intPending);
    maxReadUs = max(maxReadUs, micros() - lastPollUs);
    if (ev == TOUCH_POINT && !intPending) statRecovered++;
    if (ev == TOUCH_POINT) {
      if (touched) maxGapMs = max(maxGapMs, now - lastPointMs);
      touched = true;
      newPoint = true;
      lastPointMs = now;
    } else if (ev == TOUCH_LIFT) {
      touched = false;
    }
  }
  if (touched && !newPoint && now - lastPointMs > TOUCH_TIMEOUT_MS) touched = false;

  static uint32_t lastBeat = 0;
  if (now - lastBeat >= 1000) {
    lastBeat = now;
    if (TOUCH_TRACE && (statInts || statPoint || statLift))
      Serial.printf("touch/s: ints %lu  points %lu  recovered %lu  lifts %lu  no-data %lu  errors %lu  gap %lums  max us: read %lu seg %lu hud %lu flush %lu loop %lu  int-pin %d  last %d,%d\n",
                    (unsigned long)statInts, (unsigned long)statPoint, (unsigned long)statRecovered, (unsigned long)statLift,
                    (unsigned long)statNoData, (unsigned long)statErr, (unsigned long)maxGapMs,
                    (unsigned long)maxReadUs, (unsigned long)maxSegUs, (unsigned long)maxHudUs,
                    (unsigned long)maxFlushUs, (unsigned long)maxLoopUs, digitalRead(TP_INT), tx, ty);
    statInts = statRecovered = statPoint = statNoData = statLift = statErr = 0;
    maxReadUs = maxSegUs = maxHudUs = maxFlushUs = maxLoopUs = maxGapMs = 0;
  }

  if (testMode) {
    if (touched && !wasTouched) {
      if (++testIdx >= PATTERN_N) exitTestMode();
      else drawPattern(testIdx);
    }
    wasTouched = touched;
    delay(5);
    return;
  }

  if (touched) {
    if (!wasTouched) {
      int b = hitButton(tx, ty);
      if (b >= 0) pressButton(b);
      else if (ty >= DRAW_Y0 && ty < DRAW_Y1) beginStroke(tx, ty);
    } else if (strokeActive && newPoint) {
      uint32_t t0 = micros();
      continueStroke(tx, ty);
      maxSegUs = max(maxSegUs, micros() - t0);
    }
  } else if (wasTouched) {
    strokeActive = false;
  }
  wasTouched = touched;
  if (testMode) return;  // a button just switched modes

  if (now - lastHudMs >= 100) {
    lastHudMs = now;
    uint32_t t0 = micros();
    drawHud();
    maxHudUs = max(maxHudUs, micros() - t0);
  }

  if (now - lastFlushMs >= 8) {
    lastFlushMs = now;
    uint32_t t0 = micros();
    flushDirty();
    maxFlushUs = max(maxFlushUs, micros() - t0);
  }

  if (now - fpsTimer >= 1000) {
    fps = flushCount * 1000.0f / (now - fpsTimer);
    flushCount = 0;
    fpsTimer = now;
  }
  maxLoopUs = max(maxLoopUs, micros() - loopStartUs);
}
