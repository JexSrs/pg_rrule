#ifndef PG_RRULE_H
#define PG_RRULE_H

#include <libical/ical.h>

#include <postgres.h>
#include <fmgr.h>
#include <lib/stringinfo.h>
#include <libpq/pqformat.h>

PG_MODULE_MAGIC;

/* ========================================================================
 * Type I/O Functions
 * ======================================================================== */

/**
 * pg_rrule_in - Text input function for rrule type
 *
 * Parses a string representation of an RRULE (RFC 5545) and converts it
 * to the internal icalrecurrencetype structure. The input should be the
 * RRULE content without the "RRULE:" prefix.
 *
 * Example input: "FREQ=DAILY;INTERVAL=1;BYHOUR=9;BYMINUTE=0;BYSECOND=0"
 *
 * @param fcinfo Function call info containing cstring argument
 * @return Datum containing pointer to icalrecurrencetype structure
 * @throws ERROR if RRULE string is invalid or has unsupported frequency
 */
PG_FUNCTION_INFO_V1(pg_rrule_in);
Datum pg_rrule_in(PG_FUNCTION_ARGS);

/**
 * pg_rrule_out - Text output function for rrule type
 *
 * Converts an internal icalrecurrencetype structure back to its string
 * representation for display and text-based operations.
 *
 * @param fcinfo Function call info containing rrule pointer argument
 * @return Datum containing cstring representation of the RRULE
 * @throws ERROR if conversion to string fails
 */
PG_FUNCTION_INFO_V1(pg_rrule_out);
Datum pg_rrule_out(PG_FUNCTION_ARGS);

/* occurrences */
/* ========================================================================
 * Occurrence Generation Functions
 * ======================================================================== */

/**
 * pg_rrule_get_occurrences_dtstart_tz - Generate occurrences with timezone
 *
 * Generates an array of timestamp occurrences based on the RRULE and a
 * starting datetime, with timezone information preserved.
 *
 * @param fcinfo Function call info containing rrule and timestamptz arguments
 * @return Datum containing array of timestamptz values
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_occurrences_dtstart_tz);
Datum pg_rrule_get_occurrences_dtstart_tz(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_occurrences_dtstart_until_tz - Generate bounded occurrences with timezone
 *
 * Generates timestamp occurrences between dtstart and until dates,
 * with timezone information preserved.
 *
 * @param fcinfo Function call info containing rrule, start timestamptz, and end timestamptz
 * @return Datum containing array of timestamptz values within the specified range
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_occurrences_dtstart_until_tz);
Datum pg_rrule_get_occurrences_dtstart_until_tz(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_occurrences_dtstart - Generate occurrences without timezone
 *
 * Generates an array of timestamp occurrences based on the RRULE and a
 * starting datetime, treating times as local/naive timestamps.
 *
 * @param fcinfo Function call info containing rrule and timestamp arguments
 * @return Datum containing array of timestamp values
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_occurrences_dtstart);
Datum pg_rrule_get_occurrences_dtstart(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_occurrences_dtstart_until - Generate bounded occurrences without timezone
 *
 * Generates timestamp occurrences between dtstart and until dates,
 * treating times as local/naive timestamps.
 *
 * @param fcinfo Function call info containing rrule, start timestamp, and end timestamp
 * @return Datum containing array of timestamp values within the specified range
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_occurrences_dtstart_until);
Datum pg_rrule_get_occurrences_dtstart_until(PG_FUNCTION_ARGS);

/* ========================================================================
 * Comparison Operators
 * ======================================================================== */

/**
 * pg_rrule_eq - Equality operator for rrule type
 *
 * Compares two RRULE values for equality by comparing their string
 * representations. Two RRULEs are considered equal if they generate
 * the same recurrence pattern.
 *
 * @param fcinfo Function call info containing two rrule arguments
 * @return Datum containing boolean result of equality comparison
 */
PG_FUNCTION_INFO_V1(pg_rrule_eq);
Datum pg_rrule_eq(PG_FUNCTION_ARGS);

/**
 * pg_rrule_ne - Inequality operator for rrule type
 *
 * Compares two RRULE values for inequality. Returns the logical
 * negation of the equality operator.
 *
 * @param fcinfo Function call info containing two rrule arguments
 * @return Datum containing boolean result of inequality comparison
 */
PG_FUNCTION_INFO_V1(pg_rrule_ne);
Datum pg_rrule_ne(PG_FUNCTION_ARGS);

/* ========================================================================
 * Property Accessor Functions
 * ======================================================================== */

/**
 * pg_rrule_get_freq_rrule - Extract FREQ property
 *
 * Returns the frequency component of the RRULE as a text value.
 * Possible values: SECONDLY, MINUTELY, HOURLY, DAILY, WEEKLY, MONTHLY, YEARLY
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing text representation of frequency
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_freq_rrule);
Datum pg_rrule_get_freq_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_until_rrule - Extract UNTIL property as timestamp
 *
 * Returns the UNTIL date/time as a timestamp without timezone.
 * Returns NULL if no UNTIL is specified in the RRULE.
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing timestamp or NULL
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_until_rrule);
Datum pg_rrule_get_until_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_untiltz_rrule - Extract UNTIL property as timestamptz
 *
 * Returns the UNTIL date/time as a timestamp with timezone information.
 * Returns NULL if no UNTIL is specified in the RRULE.
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing timestamptz or NULL
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_untiltz_rrule);
Datum pg_rrule_get_untiltz_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_count_rrule - Extract COUNT property
 *
 * Returns the COUNT value specifying the maximum number of occurrences.
 * Returns NULL if no COUNT is specified (infinite recurrence or UNTIL-bounded).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer count or NULL
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_count_rrule);
Datum pg_rrule_get_count_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_interval_rrule - Extract INTERVAL property
 *
 * Returns the INTERVAL value specifying the step size for recurrence.
 * Default is 1 if not specified in the RRULE.
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer interval value
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_interval_rrule);
Datum pg_rrule_get_interval_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_bysecond_rrule - Extract BYSECOND property
 *
 * Returns an array of integers representing the seconds within a minute
 * when the recurrence should occur (0-60, where 60 represents leap seconds).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_bysecond_rrule);
Datum pg_rrule_get_bysecond_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_byminute_rrule - Extract BYMINUTE property
 *
 * Returns an array of integers representing the minutes within an hour
 * when the recurrence should occur (0-59).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_byminute_rrule);
Datum pg_rrule_get_byminute_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_byhour_rrule - Extract BYHOUR property
 *
 * Returns an array of integers representing the hours within a day
 * when the recurrence should occur (0-23).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_byhour_rrule);
Datum pg_rrule_get_byhour_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_byday_rrule - Extract BYDAY property
 *
 * Returns an array of integers representing the days of the week
 * when the recurrence should occur. Values can include optional
 * occurrence numbers (e.g., -1MO for last Monday).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_byday_rrule);
Datum pg_rrule_get_byday_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_bymonthday_rrule - Extract BYMONTHDAY property
 *
 * Returns an array of integers representing the days of the month
 * when the recurrence should occur (1-31, or negative for counting
 * from the end of the month).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_bymonthday_rrule);
Datum pg_rrule_get_bymonthday_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_byyearday_rrule - Extract BYYEARDAY property
 *
 * Returns an array of integers representing the days of the year
 * when the recurrence should occur (1-366, or negative for counting
 * from the end of the year).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_byyearday_rrule);
Datum pg_rrule_get_byyearday_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_byweekno_rrule - Extract BYWEEKNO property
 *
 * Returns an array of integers representing the weeks of the year
 * when the recurrence should occur (1-53, or negative for counting
 * from the end of the year).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_byweekno_rrule);
Datum pg_rrule_get_byweekno_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_bymonth_rrule - Extract BYMONTH property
 *
 * Returns an array of integers representing the months of the year
 * when the recurrence should occur (1-12).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_bymonth_rrule);
Datum pg_rrule_get_bymonth_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_bysetpos_rrule - Extract BYSETPOS property
 *
 * Returns an array of integers representing which occurrences within
 * the recurrence set should be included (1-366, or negative for
 * counting from the end).
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing integer array or NULL if not specified
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_bysetpos_rrule);
Datum pg_rrule_get_bysetpos_rrule(PG_FUNCTION_ARGS);

/**
 * pg_rrule_get_wkst_rrule - Extract WKST (week start) property
 *
 * Returns the day of the week that marks the beginning of the work week
 * as a text value (MO, TU, WE, TH, FR, SA, SU). Default is MO if not specified.
 *
 * @param fcinfo Function call info containing rrule argument
 * @return Datum containing text representation of week start day
 */
PG_FUNCTION_INFO_V1(pg_rrule_get_wkst_rrule);
Datum pg_rrule_get_wkst_rrule(PG_FUNCTION_ARGS);

/* ========================================================================
 * Internal Helper Functions
 * ======================================================================== */

/**
 * pg_rrule_get_occurrences_rrule - Internal occurrence generation helper
 *
 * Core function for generating recurrence occurrences from an RRULE
 * and start time, with optional timezone handling.
 *
 * @param recurrence The icalrecurrencetype structure
 * @param dtstart Starting date/time for the recurrence
 * @param use_tz Whether to preserve timezone information
 * @return Datum containing array of timestamp values
 */
Datum pg_rrule_get_occurrences_rrule(struct icalrecurrencetype recurrence,
                                     struct icaltimetype dtstart,
                                     bool use_tz);

/**
 * pg_rrule_get_occurrences_rrule_until - Internal bounded occurrence generation helper
 *
 * Core function for generating recurrence occurrences within a specific
 * time range, with optional timezone handling.
 *
 * @param recurrence The icalrecurrencetype structure
 * @param dtstart Starting date/time for the recurrence
 * @param until Ending date/time to limit occurrences
 * @param use_tz Whether to preserve timezone information
 * @return Datum containing array of timestamp values within range
 */
Datum pg_rrule_get_occurrences_rrule_until(struct icalrecurrencetype recurrence,
                                           struct icaltimetype dtstart,
                                           struct icaltimetype until,
                                           bool use_tz);

/**
 * pg_rrule_rrule_to_time_t_array - Convert RRULE to time_t array
 *
 * Low-level function that generates occurrences as an array of time_t values.
 * Used internally by the higher-level occurrence generation functions.
 *
 * @param recurrence The icalrecurrencetype structure
 * @param dtstart Starting date/time for the recurrence
 * @param out_array Output parameter for allocated time_t array
 * @param out_count Output parameter for number of occurrences generated
 */
void pg_rrule_rrule_to_time_t_array(struct icalrecurrencetype recurrence,
                                    struct icaltimetype dtstart,
                                    time_t** const out_array,
                                    unsigned int* const out_count);

/**
 * pg_rrule_rrule_to_time_t_array_until - Convert RRULE to bounded time_t array
 *
 * Low-level function that generates occurrences within a time range
 * as an array of time_t values.
 *
 * @param recurrence The icalrecurrencetype structure
 * @param dtstart Starting date/time for the recurrence
 * @param until Ending date/time to limit occurrences
 * @param out_array Output parameter for allocated time_t array
 * @param out_count Output parameter for number of occurrences generated
 */
void pg_rrule_rrule_to_time_t_array_until(struct icalrecurrencetype recurrence,
                                          struct icaltimetype dtstart,
                                          struct icaltimetype until,
                                          time_t** const out_array,
                                          unsigned int* const out_count);

/**
 * pg_rrule_get_bypart_rrule - Generic BY* property extractor
 *
 * Internal helper function for extracting any BY* rule array from
 * an icalrecurrencetype structure. Used by all the specific BY*
 * accessor functions.
 *
 * @param recurrence_ref Pointer to the icalrecurrencetype structure
 * @param part Which BY* rule to extract (ICAL_BY_SECOND, ICAL_BY_MINUTE, etc.)
 * @param max_size Maximum size of the BY* array for bounds checking
 * @return Datum containing integer array or NULL if not specified
 */
Datum pg_rrule_get_bypart_rrule(struct icalrecurrencetype *recurrence_ref, icalrecurrencetype_byrule part, size_t max_size);

#endif // PG_RRULE_H
