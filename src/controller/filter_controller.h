/*
 * ira - iRacing Application
 * Filter Controller — Business logic for race filtering
 *
 * Separates filter orchestration from UI display.
 * UI backends call these functions, then render the results.
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#ifndef IRA_FILTER_CONTROLLER_H
#define IRA_FILTER_CONTROLLER_H

#include <stdbool.h>
#include "../filter/race_filter.h"

/*
 * Get filtered and sorted race results.
 * Creates a filter_results, applies the filter (or shows all if show_all
 * is true), sorts by the given order, and returns via results_out.
 * Caller must call filter_results_destroy() on the returned results.
 * Returns true on success. Sets error_out (if non-NULL) on failure.
 */
bool filter_controller_get_races(ira_database *db, bool show_all,
                                 race_sort_order order, bool ascending,
                                 filter_results **results_out,
                                 const char **error_out);

#endif /* IRA_FILTER_CONTROLLER_H */
