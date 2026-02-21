/*
 * ira - iRacing Application
 * Filter Controller — Business logic for race filtering
 *
 * Copyright (c) 2026 Christopher Griffiths
 */

#include "filter_controller.h"

bool filter_controller_get_races(ira_database *db, bool show_all,
                                 race_sort_order order, bool ascending,
                                 filter_results **results_out,
                                 const char **error_out)
{
    if (!db || !results_out) {
        if (error_out) *error_out = "Invalid arguments.";
        return false;
    }

    filter_results *results = filter_results_create();
    if (!results) {
        if (error_out) *error_out = "Could not allocate filter results.";
        return false;
    }

    bool ok = show_all
        ? filter_apply_show_all(db, results)
        : filter_apply(db, results);

    if (!ok) {
        filter_results_destroy(results);
        if (error_out) *error_out = "Filter apply failed.";
        return false;
    }

    filter_results_sort(results, order, ascending);

    *results_out = results;
    return true;
}
