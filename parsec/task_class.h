/*
 * Copyright (c) 2012-2019 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#ifndef PARSEC_TASK_CLASS_H_HAS_BEEN_INCLUDED
#define PARSEC_TASK_CLASS_H_HAS_BEEN_INCLUDED

#include "parsec/parsec_config.h"
#include "parsec/interfaces/interface.h"
#include "parsec_internal.h"

void parsec_shift_all_flows_after(parsec_task_class_t *tc, parsec_flow_t *pivot_flow, int shift);
void parsec_shift_all_deps_after(parsec_flow_t *flow, int dep_in_out, parsec_dep_t *pivot_dep, int shift);
// flow_array should be const, but it creates a warning in the generated code
bool parsec_helper_flow_is_in_flow_array(const parsec_flow_t *flow, parsec_flow_t *flow_array[], int flow_array_size);

void parsec_debug_dump_task_class_at_exec(parsec_task_class_t *tc);
void parsec_check_sanity_of_task_class(parsec_task_class_t *tc);

// Copy a flow (including its deps) and returns a pointer to the new flow
parsec_flow_t *parsec_helper_copy_flow(parsec_flow_t *flow_to, parsec_flow_t *flow_from);
parsec_dep_t *parsec_helper_copy_dep(parsec_dep_t * dep_to, parsec_dep_t * dep_from);

int parsec_helper_dep_is_in_flow(parsec_flow_t *flow, parsec_dep_t *dep);
int parsec_helper_get_dep_index(parsec_task_class_t *tc, parsec_dep_t *dep, int in_out);
int parsec_helper_get_flow_index_that_contains_dep(parsec_task_class_t *tc, parsec_dep_t *dep, int in_out);

#endif  /* PARSEC_TASK_CLASS_H_HAS_BEEN_INCLUDED */
