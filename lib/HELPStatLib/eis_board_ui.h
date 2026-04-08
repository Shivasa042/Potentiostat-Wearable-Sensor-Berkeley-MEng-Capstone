#ifndef EIS_BOARD_UI_H
#define EIS_BOARD_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Called once after AD5940_MCUResourceInit() / SPI.begin (shared SPI bus). */
void eis_board_ui_init(void);

/* Show a numeric value against [v_min, v_max] with a title (EIS magnitude, temp, etc.). */
void eis_board_ui_show_numeric_range(float v_min, float v_max, float v_cur, const char *title);

/* Live sweep: current frequency between sweep endpoints; index is 0..totalPoints-1 for this cycle. */
void eis_board_ui_sweep_point(float f_start_hz, float f_end_hz, float f_cur_hz,
                              uint32_t sweep_index, uint32_t sweep_points_total);

#ifdef __cplusplus
}
#endif

#endif
