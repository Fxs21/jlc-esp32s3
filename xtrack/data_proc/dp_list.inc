/* Data processing node list
 *
 * ENTRY(id, buf_size, init_fn)
 *   id       — Node name (used as Account id string)
 *   buf_size — Size of double-buffer slot (0 = no buffer)
 *   init_fn  — Node init function (void fn(account_t *account))
 *
 * To add a new node:
 *   1. Write dp_<id>.c with <init_fn>
 *   2. Add one ENTRY line below
 */

ENTRY(Clock, sizeof(hal_clock_info_t), dp_clock_init)
ENTRY(Power, sizeof(hal_power_info_t), dp_power_init)
ENTRY(GPS, sizeof(hal_gps_info_t), dp_gps_init)
