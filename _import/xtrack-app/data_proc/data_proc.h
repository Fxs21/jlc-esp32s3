/**
 * @file  data_proc.h
 * @brief DataProc layer — initialise all middleware nodes
 *
 * Each DataProc node is an Account registered on the global DataCenter.
 * data_proc_init() creates and initialises all nodes from the
 * registration table (dp_list.inc).
 */
#ifndef DATA_PROC_H
#define DATA_PROC_H

#include "data_center/data_center.h"

/**
 * @brief  Initialise all DataProc nodes
 * @param  dc: Pointer to the global DataCenter
 * @note   Creates one Account per node entry in dp_list.inc,
 *         then calls each node's init function.
 */
void data_proc_init(data_center_t *dc);

#endif /* DATA_PROC_H */
