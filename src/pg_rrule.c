#include "pg_rrule.h"

#include <utils/timestamp.h>
#include <utils/array.h>
#include <catalog/pg_type.h>
#include <utils/lsyscache.h>
#include "utils/builtins.h"

Datum pg_rrule_in(PG_FUNCTION_ARGS) {
    const char* const rrule_str = PG_GETARG_CSTRING(0);
    struct icalrecurrencetype *recurrence = icalrecurrencetype_new_from_string(rrule_str);

    const icalerrorenum err = icalerrno;
    if (err != ICAL_NO_ERROR || !recurrence) {
        icalerror_clear_errno();
        if (recurrence) icalrecurrencetype_unref(recurrence);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Can't parse RRULE. iCal error: %s. RRULE \"%s\".", icalerror_strerror(err), rrule_str),
                 errhint("You need to omit \"RRULE:\" part of expression (if present)")));
    }

    // Calculate total size needed for flattened storage
    size_t base_size = sizeof(struct icalrecurrencetype);
    size_t arrays_size = 0;
    size_t rscale_size = 0;

    // Calculate space needed for by arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (recurrence->by[i].size > 0) {
            arrays_size += recurrence->by[i].size * sizeof(short);
        }
    }

    // Calculate space needed for rscale
    if (recurrence->rscale) {
        rscale_size = strlen(recurrence->rscale) + 1;
    }

    size_t total_size = base_size + arrays_size + rscale_size;

    // Allocate flattened structure
    char *flattened = palloc0(VARHDRSZ + total_size);
    SET_VARSIZE(flattened, VARHDRSZ + total_size);

    // Copy base structure (but we'll fix the pointers)
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(flattened);
    memcpy(flat_struct, recurrence, sizeof(struct icalrecurrencetype));
    flat_struct->refcount = 1;

    // Current position for variable data (after the base struct)
    char *var_data_pos = VARDATA(flattened) + base_size;

    // Copy and relocate by arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (recurrence->by[i].size > 0 && recurrence->by[i].data) {
            size_t array_bytes = recurrence->by[i].size * sizeof(short);

            // Copy array data to our flattened buffer
            memcpy(var_data_pos, recurrence->by[i].data, array_bytes);

            // Store offset from start of VARDATA (not absolute pointer)
            size_t offset = var_data_pos - VARDATA(flattened);
            flat_struct->by[i].data = (short*)offset;
            flat_struct->by[i].size = recurrence->by[i].size;

            var_data_pos += array_bytes;
        } else {
            flat_struct->by[i].data = NULL;
            flat_struct->by[i].size = 0;
        }
    }

    // Copy and relocate rscale
    if (recurrence->rscale) {
        memcpy(var_data_pos, recurrence->rscale, rscale_size);
        // Store as offset from start of VARDATA
        size_t offset = var_data_pos - VARDATA(flattened);
        flat_struct->rscale = (char*)offset;
        var_data_pos += rscale_size;
    } else {
        flat_struct->rscale = NULL;
    }

    icalrecurrencetype_unref(recurrence);
    PG_RETURN_POINTER(flattened);
}

Datum pg_rrule_out(PG_FUNCTION_ARGS) {
    char *flattened = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(flattened);

    // Create a temporary struct with real pointers for libical
    struct icalrecurrencetype temp_struct;
    memcpy(&temp_struct, flat_struct, sizeof(struct icalrecurrencetype));

    char *base_addr = VARDATA(flattened);

    // Convert offsets back to real pointers for by arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (flat_struct->by[i].size > 0 && flat_struct->by[i].data != NULL) {
            // Convert offset back to pointer
            size_t offset = (size_t)flat_struct->by[i].data;
            temp_struct.by[i].data = (short*)(base_addr + offset);
        } else {
            temp_struct.by[i].data = NULL;
        }
    }

    // Convert rscale offset back to pointer
    if (flat_struct->rscale != NULL) {
        size_t offset = (size_t)flat_struct->rscale;
        temp_struct.rscale = base_addr + offset;
    }

    char *const rrule_str = icalrecurrencetype_as_string(&temp_struct);
    const icalerrorenum err = icalerrno;
    if (err != ICAL_NO_ERROR) {
        icalerror_clear_errno();
        if (rrule_str) free(rrule_str);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("Can't convert RRULE to string. iCal error: %s", icalerror_strerror(err))));
    }

    if (!rrule_str) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("icalrecurrencetype_as_string returned NULL")));
    }

    const size_t str_bytes = sizeof(char) * (strlen(rrule_str) + 1);
    char *const rrule_str_copy = palloc(str_bytes);
    memcpy(rrule_str_copy, rrule_str, str_bytes);
    free(rrule_str);

    PG_RETURN_CSTRING(rrule_str_copy);
}

Datum pg_rrule_send(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(varlena_data);

    char *base_addr = VARDATA(varlena_data);

    // Now use the binary serialization
    StringInfoData buf;
    pq_begintypsend(&buf);

    // Send basic fields individually
    pq_sendint32(&buf, flat_struct->refcount);
    pq_sendint32(&buf, (int32)flat_struct->freq);
    pq_sendint32(&buf, flat_struct->count);
    pq_sendint16(&buf, flat_struct->interval);
    pq_sendint32(&buf, (int32)flat_struct->week_start);
    pq_sendint32(&buf, (int32)flat_struct->skip);

    // Send until time
    pq_sendint32(&buf, flat_struct->until.year);
    pq_sendint32(&buf, flat_struct->until.month);
    pq_sendint32(&buf, flat_struct->until.day);
    pq_sendint32(&buf, flat_struct->until.hour);
    pq_sendint32(&buf, flat_struct->until.minute);
    pq_sendint32(&buf, flat_struct->until.second);
    pq_sendint32(&buf, flat_struct->until.is_date);

    // Send rscale string (convert offset to pointer if needed)
    if (flat_struct->rscale != NULL) {
        size_t rscale_offset = (size_t)flat_struct->rscale;
        char *real_rscale = base_addr + rscale_offset;
        int32 len = strlen(real_rscale);
        pq_sendint32(&buf, len);
        pq_sendbytes(&buf, real_rscale, len);
    } else {
        pq_sendint32(&buf, -1); // NULL marker
    }

    // Send by arrays (convert offsets to pointers)
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (flat_struct->by[i].size > 0 && flat_struct->by[i].data != NULL) {
            pq_sendint16(&buf, flat_struct->by[i].size);

            // Convert offset to real pointer
            size_t offset = (size_t)flat_struct->by[i].data;
            short *real_data = (short*)(base_addr + offset);

            // Send each short individually
            for (int j = 0; j < flat_struct->by[i].size; j++) {
                pq_sendint16(&buf, real_data[j]);
            }
        } else {
            pq_sendint16(&buf, 0);
        }
    }

    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

Datum pg_rrule_recv(PG_FUNCTION_ARGS) {
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);

    // First, receive all data into temporary variables
    struct icalrecurrencetype temp_struct;
    memset(&temp_struct, 0, sizeof(temp_struct));

    // Receive basic fields
    temp_struct.refcount = pq_getmsgint(buf, 4);
    temp_struct.freq = (icalrecurrencetype_frequency)pq_getmsgint(buf, 4);
    temp_struct.count = pq_getmsgint(buf, 4);
    temp_struct.interval = pq_getmsgint(buf, 2);
    temp_struct.week_start = (icalrecurrencetype_weekday)pq_getmsgint(buf, 4);
    temp_struct.skip = (icalrecurrencetype_skip)pq_getmsgint(buf, 4);

    // Receive until time
    temp_struct.until.year = pq_getmsgint(buf, 4);
    temp_struct.until.month = pq_getmsgint(buf, 4);
    temp_struct.until.day = pq_getmsgint(buf, 4);
    temp_struct.until.hour = pq_getmsgint(buf, 4);
    temp_struct.until.minute = pq_getmsgint(buf, 4);
    temp_struct.until.second = pq_getmsgint(buf, 4);
    temp_struct.until.is_date = pq_getmsgint(buf, 4);

    // Receive rscale
    char *temp_rscale = NULL;
    size_t rscale_size = 0;
    int32 rscale_len = pq_getmsgint(buf, 4);
    if (rscale_len >= 0) {
        temp_rscale = palloc(rscale_len + 1);
        memcpy(temp_rscale, pq_getmsgbytes(buf, rscale_len), rscale_len);
        temp_rscale[rscale_len] = '\0';
        rscale_size = rscale_len + 1;
    }

    // Receive by arrays into temporary storage
    short *temp_arrays[ICAL_BY_NUM_PARTS];
    size_t arrays_size = 0;

    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        short size = pq_getmsgint(buf, 2);
        temp_struct.by[i].size = size;

        if (size > 0) {
            temp_arrays[i] = palloc(size * sizeof(short));
            arrays_size += size * sizeof(short);

            // Receive each short individually
            for (int j = 0; j < size; j++) {
                temp_arrays[i][j] = pq_getmsgint(buf, 2);
            }
        } else {
            temp_arrays[i] = NULL;
        }
    }

    // Now create the flattened structure (same logic as pg_rrule_in)
    size_t base_size = sizeof(struct icalrecurrencetype);
    size_t total_size = base_size + arrays_size + rscale_size;

    // Allocate flattened structure
    char *flattened = palloc0(VARHDRSZ + total_size);
    SET_VARSIZE(flattened, VARHDRSZ + total_size);

    // Copy base structure
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(flattened);
    memcpy(flat_struct, &temp_struct, sizeof(struct icalrecurrencetype));

    // Current position for variable data (after the base struct)
    char *var_data_pos = VARDATA(flattened) + base_size;

    // Copy and relocate by arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (temp_struct.by[i].size > 0 && temp_arrays[i]) {
            size_t array_bytes = temp_struct.by[i].size * sizeof(short);

            // Copy array data to our flattened buffer
            memcpy(var_data_pos, temp_arrays[i], array_bytes);

            // Store offset from start of VARDATA (not absolute pointer)
            size_t offset = var_data_pos - VARDATA(flattened);
            flat_struct->by[i].data = (short*)offset;
            flat_struct->by[i].size = temp_struct.by[i].size;

            var_data_pos += array_bytes;

            // Free temporary array
            pfree(temp_arrays[i]);
        } else {
            flat_struct->by[i].data = NULL;
            flat_struct->by[i].size = 0;
        }
    }

    // Copy and relocate rscale
    if (temp_rscale) {
        memcpy(var_data_pos, temp_rscale, rscale_size);
        // Store as offset from start of VARDATA
        size_t offset = var_data_pos - VARDATA(flattened);
        flat_struct->rscale = (char*)offset;
        var_data_pos += rscale_size;

        // Free temporary rscale
        pfree(temp_rscale);
    } else {
        flat_struct->rscale = NULL;
    }

    // Return the flattened varlena structure
    PG_RETURN_POINTER(flattened);
}

/* occurrences */
Datum pg_rrule_get_occurrences_dtstart_tz(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    TimestampTz dtstart_ts = PG_GETARG_TIMESTAMPTZ(1);

    long int gmtoff = 0;
    icaltimezone *ical_tz = NULL;
    if (pg_get_timezone_offset(session_timezone, &gmtoff)) {
        ical_tz = icaltimezone_get_builtin_timezone_from_offset(gmtoff, pg_get_timezone_name(session_timezone));
    }

    if (ical_tz == NULL) {
        elog(WARNING, "Can't get timezone from current session! Fallback to UTC.");
        ical_tz = icaltimezone_get_utc_timezone();
    }

    pg_time_t dtstart_ts_pg_time_t = timestamptz_to_time_t(dtstart_ts);

    struct icaltimetype dtstart = icaltime_from_timet_with_zone((time_t) dtstart_ts_pg_time_t, 0, ical_tz);

    return pg_rrule_get_occurrences_rrule(temp_struct, dtstart, true);
}

Datum pg_rrule_get_occurrences_dtstart_until_tz(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    TimestampTz dtstart_ts = PG_GETARG_TIMESTAMPTZ(1);
    TimestampTz until_ts = PG_GETARG_TIMESTAMPTZ(2);

    long int gmtoff = 0;
    icaltimezone *ical_tz = NULL;
    if (pg_get_timezone_offset(session_timezone, &gmtoff)) {
        ical_tz = icaltimezone_get_builtin_timezone_from_offset(gmtoff, pg_get_timezone_name(session_timezone));
    }

    if (ical_tz == NULL) {
        elog(WARNING, "Can't get timezone from current session! Fallback to UTC.");
        ical_tz = icaltimezone_get_utc_timezone();
    }

    pg_time_t dtstart_ts_pg_time_t = timestamptz_to_time_t(dtstart_ts);
    pg_time_t until_ts_pg_time_t = timestamptz_to_time_t(until_ts);

    struct icaltimetype dtstart = icaltime_from_timet_with_zone((time_t) dtstart_ts_pg_time_t, 0, ical_tz);
    struct icaltimetype until = icaltime_from_timet_with_zone((time_t) until_ts_pg_time_t, 0, ical_tz);

    return pg_rrule_get_occurrences_rrule_until(temp_struct, dtstart, until, true);
}

Datum pg_rrule_get_occurrences_dtstart(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    Timestamp dtstart_ts = PG_GETARG_TIMESTAMP(1);

    pg_time_t dtstart_ts_pg_time_t = timestamptz_to_time_t(dtstart_ts);

    struct icaltimetype dtstart = icaltime_from_timet_with_zone((time_t) dtstart_ts_pg_time_t, 0,
                                                                icaltimezone_get_utc_timezone());

    return pg_rrule_get_occurrences_rrule(temp_struct, dtstart, false);
}

Datum pg_rrule_get_occurrences_dtstart_until(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    Timestamp dtstart_ts = PG_GETARG_TIMESTAMP(1);
    Timestamp until_ts = PG_GETARG_TIMESTAMP(2);

    pg_time_t dtstart_ts_pg_time_t = timestamptz_to_time_t(dtstart_ts);
    pg_time_t until_ts_pg_time_t = timestamptz_to_time_t(until_ts);

    struct icaltimetype dtstart = icaltime_from_timet_with_zone((time_t) dtstart_ts_pg_time_t, 0,
                                                                icaltimezone_get_utc_timezone());
    struct icaltimetype until = icaltime_from_timet_with_zone((time_t) until_ts_pg_time_t, 0,
                                                              icaltimezone_get_utc_timezone());

    return pg_rrule_get_occurrences_rrule_until(temp_struct, dtstart, until, false);
}

/* operators */
Datum pg_rrule_eq(PG_FUNCTION_ARGS) {
    char *varlena_data1 = (char*) PG_GETARG_POINTER(0);
    char *varlena_data2 = (char*) PG_GETARG_POINTER(1);  // Fixed: was using arg 0 twice

    struct icalrecurrencetype temp_struct1, temp_struct2;

    // Convert both flattened structures to temporary structs with real pointers
    flatten_to_temp_struct(varlena_data1, &temp_struct1);
    flatten_to_temp_struct(varlena_data2, &temp_struct2);

    // Compare basic fields
    if (temp_struct1.freq != temp_struct2.freq ||
        temp_struct1.interval != temp_struct2.interval ||
        temp_struct1.count != temp_struct2.count ||
        temp_struct1.week_start != temp_struct2.week_start) {
        PG_RETURN_BOOL(false);
    }

    // Compare UNTIL
    if (icaltime_compare(temp_struct1.until, temp_struct2.until) != 0) {
        PG_RETURN_BOOL(false);
    }

    // Compare RSCALE
    if ((temp_struct1.rscale == NULL && temp_struct2.rscale != NULL) ||
        (temp_struct1.rscale != NULL && temp_struct2.rscale == NULL) ||
        (temp_struct1.rscale != NULL && temp_struct2.rscale != NULL &&
         strcmp(temp_struct1.rscale, temp_struct2.rscale) != 0)) {
        PG_RETURN_BOOL(false);
    }

    // Compare BY* arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        // First compare the size field
        if (temp_struct1.by[i].size != temp_struct2.by[i].size) {
            PG_RETURN_BOOL(false);
        }

        // If both sizes are 0, continue to next array
        if (temp_struct1.by[i].size == 0) {
            continue;
        }

        short *array1 = temp_struct1.by[i].data;
        short *array2 = temp_struct2.by[i].data;

        // Both should have data if size > 0, but double-check
        if ((array1 == NULL && array2 != NULL) ||
            (array1 != NULL && array2 == NULL)) {
            PG_RETURN_BOOL(false);
        }

        if (array1 != NULL && array2 != NULL) {
            int max_size;
            switch (i) {
                case ICAL_BY_SECOND:
                    max_size = ICAL_BY_SECOND_SIZE;
                    break;
                case ICAL_BY_MINUTE:
                    max_size = ICAL_BY_MINUTE_SIZE;
                    break;
                case ICAL_BY_HOUR:
                    max_size = ICAL_BY_HOUR_SIZE;
                    break;
                case ICAL_BY_DAY:
                    max_size = ICAL_BY_DAY_SIZE;
                    break;
                case ICAL_BY_MONTH_DAY:
                    max_size = ICAL_BY_MONTHDAY_SIZE;
                    break;
                case ICAL_BY_WEEK_NO:
                    max_size = ICAL_BY_WEEKNO_SIZE;
                    break;
                case ICAL_BY_YEAR_DAY:
                    max_size = ICAL_BY_YEARDAY_SIZE;
                    break;
                case ICAL_BY_MONTH:
                    max_size = ICAL_BY_MONTH_SIZE;
                    break;
                case ICAL_BY_SET_POS:
                    max_size = ICAL_BY_SETPOS_SIZE;
                    break;
                default:
                    continue; // Skip unknown by-rule types
            }

            // Compare each element up to the size stored in the structure
            for (int j = 0; j < temp_struct1.by[i].size && j < max_size; j++) {
                if (array1[j] != array2[j]) {
                    PG_RETURN_BOOL(false);
                }
            }
        }
    }

    PG_RETURN_BOOL(true);
}

Datum pg_rrule_ne(PG_FUNCTION_ARGS) {
    // Reuse pg_rrule_eq and negate its result
    bool result = DatumGetBool(DirectFunctionCall2(pg_rrule_eq, PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)));
    PG_RETURN_BOOL(!result);
}

/* Other functions */
Datum pg_rrule_get_freq_rrule(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(varlena_data);

    if (flat_struct->freq == ICAL_NO_RECURRENCE) {
        PG_RETURN_NULL();
    }

    const char *const freq_string = icalrecur_freq_to_string(flat_struct->freq);
    PG_RETURN_TEXT_P(cstring_to_text(freq_string));
}

Datum pg_rrule_get_until_rrule(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(varlena_data);

    if (icaltime_is_null_time(flat_struct->until)) {
        PG_RETURN_NULL();
    }

    pg_time_t until_pg_time_t = (pg_time_t) icaltime_as_timet_with_zone(
        flat_struct->until, icaltimezone_get_utc_timezone());
    PG_RETURN_TIMESTAMP(time_t_to_timestamptz(until_pg_time_t));
}

Datum pg_rrule_get_untiltz_rrule(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(varlena_data);

    if (icaltime_is_null_time(flat_struct->until)) {
        PG_RETURN_NULL();
    }

    long int gmtoff = 0;
    icaltimezone *ical_tz = NULL;
    if (pg_get_timezone_offset(session_timezone, &gmtoff)) {
        ical_tz = icaltimezone_get_builtin_timezone_from_offset(gmtoff, pg_get_timezone_name(session_timezone));
    }
    if (ical_tz == NULL) {
        elog(WARNING, "Can't get timezone from current session! Fallback to UTC.");
        ical_tz = icaltimezone_get_utc_timezone();
    }

    pg_time_t until_pg_time_t = (pg_time_t) icaltime_as_timet_with_zone(flat_struct->until, ical_tz);
    PG_RETURN_TIMESTAMP(time_t_to_timestamptz(until_pg_time_t));
}

Datum pg_rrule_get_count_rrule(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(varlena_data);

    PG_RETURN_INT32(flat_struct->count);
}

Datum pg_rrule_get_interval_rrule(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(varlena_data);

    PG_RETURN_INT16(flat_struct->interval);
}

Datum pg_rrule_get_wkst_rrule(PG_FUNCTION_ARGS) {
    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(varlena_data);

    if (flat_struct->week_start == ICAL_NO_WEEKDAY) {
        PG_RETURN_NULL();
    }

    const char *const wkst_string = icalrecur_weekday_to_string(flat_struct->week_start);
    PG_RETURN_TEXT_P(cstring_to_text(wkst_string));
}

Datum pg_rrule_get_bysecond_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_SECOND, ICAL_BY_SECOND_SIZE);
}

Datum pg_rrule_get_byminute_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_MINUTE, ICAL_BY_MINUTE_SIZE);
}

Datum pg_rrule_get_byhour_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_HOUR, ICAL_BY_HOUR_SIZE);
}

Datum pg_rrule_get_byday_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_DAY, ICAL_BY_DAY_SIZE);
}

Datum pg_rrule_get_bymonthday_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_MONTH_DAY, ICAL_BY_MONTHDAY_SIZE);
}

Datum pg_rrule_get_byyearday_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_YEAR_DAY, ICAL_BY_YEARDAY_SIZE);
}

Datum pg_rrule_get_byweekno_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_WEEK_NO, ICAL_BY_WEEKNO_SIZE);
}

Datum pg_rrule_get_bymonth_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_MONTH, ICAL_BY_MONTH_SIZE);
}

Datum pg_rrule_get_bysetpos_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    char *varlena_data = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype temp_struct;

    flatten_to_temp_struct(varlena_data, &temp_struct);

    return pg_rrule_get_bypart_rrule(&temp_struct, ICAL_BY_SET_POS, ICAL_BY_SETPOS_SIZE);
}

/* Helpers */
Datum pg_rrule_get_occurrences_rrule(struct icalrecurrencetype recurrence, struct icaltimetype dtstart, bool use_tz) {
    return pg_rrule_get_occurrences_rrule_until(recurrence, dtstart, icaltime_null_time(), use_tz);
}

Datum pg_rrule_get_occurrences_rrule_until(struct icalrecurrencetype recurrence, struct icaltimetype dtstart, struct icaltimetype until, bool use_tz) {
    time_t *times_array = NULL;
    unsigned int cnt = 0;

    pg_rrule_rrule_to_time_t_array_until(recurrence, dtstart, until, &times_array, &cnt);
    pg_time_t *pg_times_array = palloc(sizeof(pg_time_t) * cnt);

    unsigned int i;

    for (i = 0; i < cnt; ++i) {
        pg_times_array[i] = (pg_time_t) times_array[i]; // it's safe ? time_t may be double, float, etc...
    }

    free(times_array);

    Datum *const datum_elems = palloc(sizeof(Datum) * cnt);

    if (use_tz) {
        for (i = 0; i < cnt; ++i) {
            datum_elems[i] = TimestampTzGetDatum(time_t_to_timestamptz(pg_times_array[i]));
        }
    } else {
        for (i = 0; i < cnt; ++i) {
            datum_elems[i] = TimestampGetDatum(time_t_to_timestamptz(pg_times_array[i]));
        }
    }

    pfree(pg_times_array);

    int16 typlen;
    bool typbyval;
    char typalign;

    const Oid ts_oid = use_tz ? TIMESTAMPTZOID : TIMESTAMPOID;

    get_typlenbyvalalign(ts_oid, &typlen, &typbyval, &typalign);

    ArrayType *result_array = construct_array(datum_elems, cnt, ts_oid, typlen, typbyval, typalign);

    PG_RETURN_ARRAYTYPE_P(result_array);
}

void pg_rrule_rrule_to_time_t_array(struct icalrecurrencetype recurrence, struct icaltimetype dtstart, time_t **const out_array, unsigned int *const out_count) {
    pg_rrule_rrule_to_time_t_array_until(recurrence, dtstart, icaltime_null_time(), out_array, out_count);
}

void pg_rrule_rrule_to_time_t_array_until(struct icalrecurrencetype recurrence, struct icaltimetype dtstart, struct icaltimetype until, time_t **const out_array, unsigned int *const out_count) {
    icalrecur_iterator *const recur_iterator = icalrecur_iterator_new(&recurrence, dtstart);
    if (recur_iterator == NULL) {
        const icalerrorenum err = icalerrno;
        icalerror_clear_errno();

        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("iCal error: %s.", icalerror_strerror(err))));
    }

    icalarray *const icaltimes_list = icalarray_new(sizeof(icaltimetype), 32);

    struct icaltimetype ical_time = icalrecur_iterator_next(recur_iterator);

    if (icaltime_is_null_time(until)) {
        while (icaltime_is_null_time(ical_time) == false) {
            icalarray_append(icaltimes_list, &ical_time);
            ical_time = icalrecur_iterator_next(recur_iterator);
        }
    } else {
        while (icaltime_is_null_time(ical_time) == false
               && icaltime_compare(ical_time, until) != 1) {
            // while ical_time <= until
            icalarray_append(icaltimes_list, &ical_time);
            ical_time = icalrecur_iterator_next(recur_iterator);
        }
    }

    icalrecur_iterator_free(recur_iterator);

    const unsigned int cnt = (*out_count) = icaltimes_list->num_elements;

    time_t *times_array = (*out_array) = malloc(sizeof(time_t) * cnt);

    unsigned int i = 0;

    for (i = 0; i < cnt; ++i) {
        ical_time = (*(icaltimetype *) icalarray_element_at(icaltimes_list, i));
        times_array[i] = icaltime_as_timet_with_zone(ical_time, dtstart.zone);
    }

    icalarray_free(icaltimes_list);
}

Datum pg_rrule_get_bypart_rrule(struct icalrecurrencetype *recurrence_ref, icalrecurrencetype_byrule part, size_t max_size) {
    // Return empty array if the by-rule part doesn't exist
    if (!recurrence_ref->by[part].data) {
        Datum *empty_array = (Datum *) palloc(0);
        int16 typlen;
        bool typbyval;
        char typalign;
        get_typlenbyvalalign(INT2OID, &typlen, &typbyval, &typalign);
        ArrayType *result = construct_array(empty_array, 0, INT2OID, typlen, typbyval, typalign);
        PG_RETURN_ARRAYTYPE_P(result);
    }

    unsigned int cnt = 0;
    for (; cnt < max_size && recurrence_ref->by[part].data[cnt] != 0; cnt++);

    if (cnt == 0) {
        Datum *zero_array = (Datum *) palloc(sizeof(Datum));
        zero_array[0] = Int16GetDatum(0);
        int16 typlen;
        bool typbyval;
        char typalign;
        get_typlenbyvalalign(INT2OID, &typlen, &typbyval, &typalign);
        ArrayType *result = construct_array(zero_array, 1, INT2OID, typlen, typbyval, typalign);
        PG_RETURN_ARRAYTYPE_P(result);
    }

    Datum *datum_elems = (Datum *) palloc(sizeof(Datum) * cnt);

    for (unsigned int i = 0; i < cnt; i++) {
        datum_elems[i] = Int16GetDatum(recurrence_ref->by[part].data[i]);
    }

    int16 typlen;
    bool typbyval;
    char typalign;
    get_typlenbyvalalign(INT2OID, &typlen, &typbyval, &typalign);

    ArrayType *result_array = construct_array(
        datum_elems,
        cnt,
        INT2OID,
        typlen,
        typbyval,
        typalign
    );

    PG_RETURN_ARRAYTYPE_P(result_array);
}

void flatten_to_temp_struct(char *varlena_data, struct icalrecurrencetype *temp_struct) {
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(varlena_data);
    char *base_addr = VARDATA(varlena_data);

    // Copy the base structure
    memcpy(temp_struct, flat_struct, sizeof(struct icalrecurrencetype));

    // Convert offsets back to real pointers for by arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (flat_struct->by[i].size > 0 && flat_struct->by[i].data != NULL) {
            size_t offset = (size_t)flat_struct->by[i].data;
            temp_struct->by[i].data = (short*)(base_addr + offset);
        } else {
            temp_struct->by[i].data = NULL;
        }
    }

    // Convert rscale offset back to pointer
    if (flat_struct->rscale != NULL) {
        size_t offset = (size_t)flat_struct->rscale;
        temp_struct->rscale = base_addr + offset;
    } else {
        temp_struct->rscale = NULL;
    }
}