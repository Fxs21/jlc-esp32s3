/**
 * @file  app.h
 * @brief Application initialization and global references
 *
 * Provides the global DataCenter and PageManager instances,
 * and the single app_init() entry point.
 *
 * Usage (in main.c):
 * @code
 *   app_init();
 * @endcode
 */

#ifndef APP_H
#define APP_H

#include "data_center/data_center.h"
#include "page_manager/page_manager.h"

/** Global DataCenter instance (set by app_init) */
extern data_center_t *g_data_center;

/** Global PageManager instance (set by app_init) */
extern page_manager_t g_pm;

/**
 * @brief  Initialize the application
 * @retval None
 * @note   Creates the DataCenter, registers built-in accounts,
 *         initializes the PageManager, and pushes the first page.
 *         Must be called AFTER lv_init() and hal_init().
 */
void app_init(void);

#endif /* APP_H */
