/*
 * ira - Unit Tests for Race Filter
 *
 * Minimal test framework: assert-based, returns 0 on success, 1 on failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "../src/data/models.h"
#include "../src/filter/race_filter.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  %-50s ", #name); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } while(0)

/* ---- filter_next_race_time tests ---- */

static void test_next_race_time_future_start(void)
{
    ira_season season = {0};
    ira_schedule_week week = {0};

    /* Week starts 1 hour from now, repeats every 2 hours */
    week.start_date = time(NULL) + 3600;
    week.repeat_mins = 120;

    time_t next = filter_next_race_time(&season, &week);

    /* Should return the start_date since it's in the future */
    assert(next == week.start_date);
}

static void test_next_race_time_past_start(void)
{
    ira_season season = {0};
    ira_schedule_week week = {0};

    /* Week started 5 hours ago, repeats every 2 hours */
    time_t now = time(NULL);
    week.start_date = now - (5 * 3600);
    week.repeat_mins = 120;

    time_t next = filter_next_race_time(&season, &week);

    /* Next race should be in the future */
    assert(next > now);
    /* And within one repeat interval from now */
    assert(next <= now + (week.repeat_mins * 60));
}

static void test_next_race_time_no_repeat(void)
{
    ira_season season = {0};
    ira_schedule_week week = {0};

    week.start_date = time(NULL) - 3600;
    week.repeat_mins = 0;  /* Non-repeating */

    time_t next = filter_next_race_time(&season, &week);

    /* Should return 0 for non-repeating series */
    assert(next == 0);
}

static void test_next_race_time_no_start(void)
{
    ira_season season = {0};
    ira_schedule_week week = {0};

    week.start_date = 0;
    week.repeat_mins = 120;

    time_t next = filter_next_race_time(&season, &week);
    assert(next == 0);
}

/* ---- category/license conversion tests ---- */

static void test_category_roundtrip(void)
{
    assert(string_to_category("oval") == CATEGORY_OVAL);
    assert(string_to_category("sports_car") == CATEGORY_SPORTS_CAR);
    assert(string_to_category("formula") == CATEGORY_FORMULA);
    assert(string_to_category("dirt_oval") == CATEGORY_DIRT_OVAL);
    assert(string_to_category("dirt_road") == CATEGORY_DIRT_ROAD);
    assert(string_to_category("nonsense") == CATEGORY_UNKNOWN);
}

static void test_license_roundtrip(void)
{
    assert(string_to_license("R") == LICENSE_ROOKIE);
    assert(string_to_license("D") == LICENSE_D);
    assert(string_to_license("C") == LICENSE_C);
    assert(string_to_license("B") == LICENSE_B);
    assert(string_to_license("A") == LICENSE_A);
    assert(string_to_license("Pro") == LICENSE_PRO);
}

static void test_category_is_active(void)
{
    assert(category_is_active(CATEGORY_OVAL) == true);
    assert(category_is_active(CATEGORY_SPORTS_CAR) == true);
    assert(category_is_active(CATEGORY_FORMULA) == true);
    assert(category_is_active(CATEGORY_ROAD) == false);  /* Legacy */
    assert(category_is_active(CATEGORY_UNKNOWN) == false);
}

/* ---- filter_format_duration tests ---- */

static void test_format_duration_time_based(void)
{
    ira_schedule_week week = {0};
    week.race_time_limit_mins = 45;

    char buf[32];
    filter_format_duration(&week, buf, sizeof(buf));

    assert(strstr(buf, "45") != NULL);
}

static void test_format_duration_lap_based(void)
{
    ira_schedule_week week = {0};
    week.race_time_limit_mins = 0;
    week.race_lap_limit = 30;

    char buf[32];
    filter_format_duration(&week, buf, sizeof(buf));

    assert(strstr(buf, "30") != NULL);
}

/* ---- Main ---- */

int main(void)
{
    printf("ira test suite: filter\n");
    printf("========================================\n");

    TEST(test_next_race_time_future_start);
    TEST(test_next_race_time_past_start);
    TEST(test_next_race_time_no_repeat);
    TEST(test_next_race_time_no_start);
    TEST(test_category_roundtrip);
    TEST(test_license_roundtrip);
    TEST(test_category_is_active);
    TEST(test_format_duration_time_based);
    TEST(test_format_duration_lap_based);

    printf("========================================\n");
    printf("%d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
