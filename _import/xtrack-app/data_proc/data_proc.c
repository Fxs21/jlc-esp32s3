/**
 * @file data_proc.c
 * @brief DataProc layer — register and initialise all nodes
 *
 * Uses the X-macro pattern: dp_list.inc is included multiple times
 * with different ENTRY definitions to generate forward declarations
 * and the registration table from a single source of truth.
 */
#include "data_proc.h"
#include "hal/hal_clock.h"
#include "hal/hal_power.h"
#include "hal/hal_gps.h"
#include "log.h"

#define TAG "data_proc"

#define ENTRY(id, buf_size, init_fn) extern void init_fn(account_t *account);
#include "dp_list.inc"
#undef ENTRY

typedef struct {
    const char *name;
    uint32_t buf_size;
    void (*init)(account_t *account);
} dp_node_def_t;

static const dp_node_def_t s_nodes[] = {
#define ENTRY(id, buf_size, init_fn) {#id, buf_size, init_fn},
#include "dp_list.inc"
#undef ENTRY
};

void data_proc_init(data_center_t *dc)
{
    for (size_t i = 0; i < sizeof(s_nodes) / sizeof(s_nodes[0]); i++) {
        account_t *act = account_create(dc, s_nodes[i].name,
                                        s_nodes[i].buf_size, NULL);
        if (!act) {
            LOG_E(TAG, "Failed to create %s account", s_nodes[i].name);
            continue;
        }
        s_nodes[i].init(act);
        LOG_I(TAG, "%s node initialised", s_nodes[i].name);
    }

    LOG_I(TAG, "All DataProc nodes initialised (%zu total)",
          sizeof(s_nodes) / sizeof(s_nodes[0]));
}
