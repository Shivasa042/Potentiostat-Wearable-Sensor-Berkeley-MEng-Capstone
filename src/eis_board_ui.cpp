/*
 * Board UI for New_EIS_PCB_Akshay (KiCad): SPI display on shared FSPI with AD5941.
 * Default: Adafruit ST7789 driver (240x280). If your FPC is another controller, swap the
 * display class + init in this file.
 */

#include <Arduino.h>
#include <math.h>

#if defined(BOARD_EIS_PCB_AKSHAY)

/* Do not include constants.h before Adafruit headers: our CS macro breaks their `int8_t CS` params. */
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

/* Match lib/HELPStatLib/constants.h (BOARD_EIS_PCB_AKSHAY) / KiCad nets /CS.SCREEN, /DC, /SCREEN.RST */
#ifndef EIS_UI_TFT_CS
#define EIS_UI_TFT_CS 39
#endif
#ifndef EIS_UI_TFT_DC
#define EIS_UI_TFT_DC 3
#endif
#ifndef EIS_UI_TFT_RST
#define EIS_UI_TFT_RST 21
#endif
#ifndef EIS_UI_DISPLAY_WIDTH
#define EIS_UI_DISPLAY_WIDTH 240
#endif
#ifndef EIS_UI_DISPLAY_HEIGHT
#define EIS_UI_DISPLAY_HEIGHT 280
#endif

static Adafruit_ST7789 s_tft((int8_t)EIS_UI_TFT_CS, (int8_t)EIS_UI_TFT_DC, (int8_t)EIS_UI_TFT_RST);
static bool s_ui_ok = false;

extern "C" void eis_board_ui_init(void) {
  if (s_ui_ok) {
    return;
  }

  pinMode(EIS_UI_TFT_CS, OUTPUT);
  digitalWrite(EIS_UI_TFT_CS, HIGH);

  /* Uses global SPI after HELPStat called SPI.begin(SCK, MISO, MOSI, CS_AFE). */
  s_tft.init(EIS_UI_DISPLAY_WIDTH, EIS_UI_DISPLAY_HEIGHT);
  s_tft.setRotation(0);
  s_tft.fillScreen(ST77XX_BLACK);
  s_ui_ok = true;
  Serial.println("eis_board_ui: display ready (ST7789)");
}

extern "C" void eis_board_ui_show_numeric_range(float v_min, float v_max, float v_cur,
                                                const char *title) {
  if (!s_ui_ok) {
    return;
  }

  if (!(v_max > v_min)) {
    v_max = v_min + 1.0f;
  }
  float t = (v_cur - v_min) / (v_max - v_min);
  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }

  s_tft.fillScreen(ST77XX_BLACK);
  s_tft.setTextColor(ST77XX_WHITE);
  s_tft.setTextSize(2);
  s_tft.setCursor(4, 6);
  if (title && title[0]) {
    s_tft.println(title);
  } else {
    s_tft.println("Value");
  }

  s_tft.setTextSize(1);
  s_tft.setCursor(4, 36);
  s_tft.printf("min %.4g  max %.4g", (double)v_min, (double)v_max);

  s_tft.setTextSize(3);
  s_tft.setCursor(4, 70);
  s_tft.printf("%.5g", (double)v_cur);

  int bw = (int)s_tft.width() - 8;
  int x0 = 4;
  int yb = (int)s_tft.height() - 28;
  s_tft.drawRect(x0, yb, bw, 14, ST77XX_WHITE);
  int fillw = (int)((float)(bw - 4) * t);
  if (fillw > 0) {
    s_tft.fillRect(x0 + 2, yb + 2, fillw, 10, ST77XX_GREEN);
  }
}

extern "C" void eis_board_ui_sweep_point(float f_start_hz, float f_end_hz, float f_cur_hz,
                                         uint32_t sweep_index, uint32_t sweep_points_total) {
  if (!s_ui_ok) {
    return;
  }

  float lo = fminf(f_start_hz, f_end_hz);
  float hi = fmaxf(f_start_hz, f_end_hz);
  if (lo < 1.0f) {
    lo = 1.0f;
  }
  if (hi < lo * 1.001f) {
    hi = lo * 1.001f;
  }
  float fclamp = f_cur_hz;
  if (fclamp < lo) {
    fclamp = lo;
  }
  if (fclamp > hi) {
    fclamp = hi;
  }
  float t = (log10f(fclamp) - log10f(lo)) / (log10f(hi) - log10f(lo));
  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }

  s_tft.fillScreen(ST77XX_BLACK);
  s_tft.setTextColor(ST77XX_WHITE);
  s_tft.setTextSize(2);
  s_tft.setCursor(4, 4);
  s_tft.println("EIS sweep");

  s_tft.setTextSize(1);
  s_tft.setCursor(4, 32);
  if (sweep_points_total > 0) {
    s_tft.printf("pt %lu / %lu\n", (unsigned long)(sweep_index + 1),
                 (unsigned long)sweep_points_total);
  }
  s_tft.printf("%.2f .. %.2f Hz\n", (double)f_start_hz, (double)f_end_hz);

  s_tft.setTextSize(2);
  s_tft.setCursor(4, 70);
  s_tft.printf("%.2f Hz", (double)f_cur_hz);

  int bw = (int)s_tft.width() - 8;
  int x0 = 4;
  int yb = (int)s_tft.height() - 28;
  s_tft.drawRect(x0, yb, bw, 14, ST77XX_WHITE);
  int fillw = (int)((float)(bw - 4) * t);
  if (fillw > 0) {
    s_tft.fillRect(x0 + 2, yb + 2, fillw, 10, ST77XX_CYAN);
  }
}

#else /* !BOARD_EIS_PCB_AKSHAY */

extern "C" void eis_board_ui_init(void) {}

extern "C" void eis_board_ui_show_numeric_range(float v_min, float v_max, float v_cur,
                                                const char *title) {
  (void)v_min;
  (void)v_max;
  (void)v_cur;
  (void)title;
}

extern "C" void eis_board_ui_sweep_point(float f_start_hz, float f_end_hz, float f_cur_hz,
                                         uint32_t sweep_index, uint32_t sweep_points_total) {
  (void)f_start_hz;
  (void)f_end_hz;
  (void)f_cur_hz;
  (void)sweep_index;
  (void)sweep_points_total;
}

#endif
