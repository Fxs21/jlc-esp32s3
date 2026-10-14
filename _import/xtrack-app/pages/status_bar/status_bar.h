/**
 * @file  status_bar.h
 * @brief Status bar — persistent top-layer overlay
 *
 * Creates LVGL widgets on lv_layer_top() that stay visible
 * across all page transitions.  Subscribes to Clock and Power
 * DataProc nodes via Account.
 *
 * Visibility is controlled via Account Notify with
 * status_bar_cmd_t / status_bar_info_t.
 *
 * Architecture: standalone module (not a PageManager page).
 * Initialised once in app_init() and never unloaded.
 */
#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include "data_center/data_center.h"
#include <stdbool.h>

typedef enum {
    STATUS_BAR_STYLE_TRANSP,  /**< Transparent (map pages) */
    STATUS_BAR_STYLE_BLACK,   /**< Semi-opaque black (normal pages) */
} status_bar_style_t;

typedef enum {
    STATUS_BAR_CMD_APPEAR,      /**< Show/hide the bar */
    STATUS_BAR_CMD_SET_STYLE,   /**< Switch between TRANSP / BLACK */
} status_bar_cmd_t;

typedef struct {
    status_bar_cmd_t cmd;
    union {
        bool appear;               /**< true = show, false = hide */
        status_bar_style_t style;  /**< Style to apply */
    } param;
} status_bar_info_t;

/**
 * @brief  Create and initialise the status bar overlay
 * @param  dc: Pointer to the global DataCenter
 * @retval None
 * @note   Creates widgets on lv_layer_top() hidden.
 *         Send STATUS_BAR_CMD_APPEAR via Account Notify to show it.
 */
void status_bar_init(data_center_t *dc);

#endif /* STATUS_BAR_H */
