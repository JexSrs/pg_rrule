#include "pg_rrule.h"

#include <utils/timestamp.h>
#include <utils/array.h>
#include <catalog/pg_type.h> // oids
#include <utils/lsyscache.h> // get_typlenbyvalalign
#include "utils/builtins.h" // cstring_to_text

Datum pg_rrule_in(PG_FUNCTION_ARGS) {
    const char* const rrule_str = PG_GETARG_CSTRING(0);
    elog(NOTICE, "pg_rrule_in: Parsing string: '%s'", rrule_str);

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

    // Debug: Show what libical parsed
    elog(NOTICE, "pg_rrule_in: After libical parsing:");
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (recurrence->by[i].size > 0) {
            elog(NOTICE, "  libical by[%d].size: %d", i, recurrence->by[i].size);
            if (recurrence->by[i].data) {
                for (int j = 0; j < recurrence->by[i].size && j < 5; j++) {
                    elog(NOTICE, "    libical by[%d].data[%d]: %d", i, j, recurrence->by[i].data[j]);
                }
            }
        }
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

    elog(NOTICE, "pg_rrule_in: Sizes - base:%zu, arrays:%zu, rscale:%zu, total:%zu",
         base_size, arrays_size, rscale_size, total_size);

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

            elog(NOTICE, "pg_rrule_in: Copying by[%d] - size:%d, bytes:%zu",
                 i, recurrence->by[i].size, array_bytes);

            // Copy array data to our flattened buffer
            memcpy(var_data_pos, recurrence->by[i].data, array_bytes);

            // Store offset from start of VARDATA (not absolute pointer)
            size_t offset = var_data_pos - VARDATA(flattened);
            flat_struct->by[i].data = (short*)offset;
            flat_struct->by[i].size = recurrence->by[i].size;

            elog(NOTICE, "pg_rrule_in: Stored by[%d] at offset %zu", i, offset);

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
        elog(NOTICE, "pg_rrule_in: Stored rscale at offset %zu", offset);
        var_data_pos += rscale_size;
    } else {
        flat_struct->rscale = NULL;
    }

    // Verify what we stored
    elog(NOTICE, "pg_rrule_in: Verification of flattened structure:");
    elog(NOTICE, "  freq: %d, interval: %d", flat_struct->freq, flat_struct->interval);
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (flat_struct->by[i].size > 0) {
            elog(NOTICE, "  flat by[%d].size: %d, data_offset: %p",
                 i, flat_struct->by[i].size, flat_struct->by[i].data);
        }
    }

    icalrecurrencetype_unref(recurrence);

    PG_RETURN_POINTER(flattened);
}

Datum pg_rrule_out(PG_FUNCTION_ARGS) {
    elog(NOTICE, "pg_rrule_out: Starting");

    char *flattened = (char*) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *flat_struct = (struct icalrecurrencetype*)VARDATA(flattened);

    elog(NOTICE, "pg_rrule_out: VARSIZE = %d", VARSIZE(flattened));
    elog(NOTICE, "pg_rrule_out: Basic fields - freq:%d, interval:%d",
         flat_struct->freq, flat_struct->interval);

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

            elog(NOTICE, "pg_rrule_out: by[%d] - size:%d, offset:%zu",
                 i, temp_struct.by[i].size, offset);

            // Debug the actual values
            for (int j = 0; j < temp_struct.by[i].size && j < 5; j++) {
                elog(NOTICE, "  by[%d].data[%d]: %d", i, j, temp_struct.by[i].data[j]);
            }
        } else {
            temp_struct.by[i].data = NULL;
        }
    }

    // Convert rscale offset back to pointer
    if (flat_struct->rscale != NULL) {
        size_t offset = (size_t)flat_struct->rscale;
        temp_struct.rscale = base_addr + offset;
        elog(NOTICE, "pg_rrule_out: rscale at offset %zu: '%s'", offset, temp_struct.rscale);
    }

    elog(NOTICE, "pg_rrule_out: Calling icalrecurrencetype_as_string");
    char *const rrule_str = icalrecurrencetype_as_string(&temp_struct);

    if (rrule_str) {
        elog(NOTICE, "pg_rrule_out: Generated string: '%s'", rrule_str);
    } else {
        elog(NOTICE, "pg_rrule_out: icalrecurrencetype_as_string returned NULL");
    }

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
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence = (struct icalrecurrencetype*)VARDATA(varlena_data);

    elog(NOTICE, "pg_rrule_send: Starting send");
    elog(NOTICE, "pg_rrule_send: freq=%d, interval=%d", recurrence->freq, recurrence->interval);

    // Debug the by arrays before sending
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (recurrence->by[i].size > 0) {
            elog(NOTICE, "pg_rrule_send: by[%d].size: %d", i, recurrence->by[i].size);
            if (recurrence->by[i].data) {
                for (int j = 0; j < recurrence->by[i].size && j < 3; j++) {
                    elog(NOTICE, "pg_rrule_send: by[%d].data[%d]: %d", i, j, recurrence->by[i].data[j]);
                }
            }
        }
    }

    // Now use the binary serialization as before...
    StringInfoData buf;
    pq_begintypsend(&buf);

    // Send basic fields individually
    pq_sendint32(&buf, recurrence->refcount);
    pq_sendint32(&buf, (int32)recurrence->freq);
    pq_sendint32(&buf, recurrence->count);
    pq_sendint16(&buf, recurrence->interval);
    pq_sendint32(&buf, (int32)recurrence->week_start);
    pq_sendint32(&buf, (int32)recurrence->skip);

    // Send until time
    pq_sendint32(&buf, recurrence->until.year);
    pq_sendint32(&buf, recurrence->until.month);
    pq_sendint32(&buf, recurrence->until.day);
    pq_sendint32(&buf, recurrence->until.hour);
    pq_sendint32(&buf, recurrence->until.minute);
    pq_sendint32(&buf, recurrence->until.second);
    pq_sendint32(&buf, recurrence->until.is_date);

    // Send rscale string
    if (recurrence->rscale) {
        int32 len = strlen(recurrence->rscale);
        pq_sendint32(&buf, len);
        pq_sendbytes(&buf, recurrence->rscale, len);
    } else {
        pq_sendint32(&buf, -1); // NULL marker
    }

    // Send by arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        if (recurrence->by[i].size > 0 && recurrence->by[i].data) {
            pq_sendint16(&buf, recurrence->by[i].size);
            // Send each short individually
            for (int j = 0; j < recurrence->by[i].size; j++) {
                pq_sendint16(&buf, recurrence->by[i].data[j]);
            }
        } else {
            pq_sendint16(&buf, 0);
        }
    }

    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

Datum pg_rrule_recv(PG_FUNCTION_ARGS) {
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);

    elog(NOTICE, "pg_rrule_recv: Starting receive");

    // Allocate as varlena structure like pg_rrule_in does
    size_t struct_size = sizeof(struct icalrecurrencetype);
    struct icalrecurrencetype* recurrence_ref = (struct icalrecurrencetype*) palloc0(VARHDRSZ + struct_size);
    SET_VARSIZE(recurrence_ref, VARHDRSZ + struct_size);

    // The actual struct data starts after the varlena header
    struct icalrecurrencetype* recurrence = (struct icalrecurrencetype*)VARDATA(recurrence_ref);

    elog(NOTICE, "pg_rrule_recv: Allocated memory, receiving basic fields");

    // Receive basic fields into the actual struct (not the wrapper)
    recurrence->refcount = pq_getmsgint(buf, 4);
    recurrence->freq = (icalrecurrencetype_frequency)pq_getmsgint(buf, 4);
    recurrence->count = pq_getmsgint(buf, 4);
    recurrence->interval = pq_getmsgint(buf, 2);
    recurrence->week_start = (icalrecurrencetype_weekday)pq_getmsgint(buf, 4);
    recurrence->skip = (icalrecurrencetype_skip)pq_getmsgint(buf, 4);

    elog(NOTICE, "pg_rrule_recv: Received basic fields - freq:%d, interval:%d",
         recurrence->freq, recurrence->interval);

    // Receive until time
    recurrence->until.year = pq_getmsgint(buf, 4);
    recurrence->until.month = pq_getmsgint(buf, 4);
    recurrence->until.day = pq_getmsgint(buf, 4);
    recurrence->until.hour = pq_getmsgint(buf, 4);
    recurrence->until.minute = pq_getmsgint(buf, 4);
    recurrence->until.second = pq_getmsgint(buf, 4);
    recurrence->until.is_date = pq_getmsgint(buf, 4);

    elog(NOTICE, "pg_rrule_recv: Received until time");

    // Receive rscale
    int32 rscale_len = pq_getmsgint(buf, 4);
    if (rscale_len >= 0) {
        recurrence->rscale = palloc(rscale_len + 1);
        memcpy(recurrence->rscale, pq_getmsgbytes(buf, rscale_len), rscale_len);
        recurrence->rscale[rscale_len] = '\0';
        elog(NOTICE, "pg_rrule_recv: Received rscale: %s", recurrence->rscale);
    } else {
        recurrence->rscale = NULL;
        elog(NOTICE, "pg_rrule_recv: No rscale");
    }

    elog(NOTICE, "pg_rrule_recv: About to receive by arrays");

    // Receive by arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        short size = pq_getmsgint(buf, 2);
        elog(NOTICE, "pg_rrule_recv: by[%d] size = %d", i, size);

        if (size > 0) {
            recurrence->by[i].size = size;
            recurrence->by[i].data = palloc(size * sizeof(short));
            // Receive each short individually
            for (int j = 0; j < size; j++) {
                recurrence->by[i].data[j] = pq_getmsgint(buf, 2);
                elog(NOTICE, "pg_rrule_recv: by[%d].data[%d] = %d", i, j, recurrence->by[i].data[j]);
            }
        } else {
            recurrence->by[i].size = 0;
            recurrence->by[i].data = NULL;
        }
    }

    elog(NOTICE, "pg_rrule_recv: Finished receiving, returning");

    // Return the varlena wrapper, not the inner struct
    PG_RETURN_POINTER(recurrence_ref);
}

/* occurrences */
Datum pg_rrule_get_occurrences_dtstart_tz(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);
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
    // it's safe ? time_t may be double, float, etc...

    return pg_rrule_get_occurrences_rrule(*recurrence_ref, dtstart, true);
}

Datum pg_rrule_get_occurrences_dtstart_until_tz(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);
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
    // it's safe ? time_t may be double, float, etc...
    struct icaltimetype until = icaltime_from_timet_with_zone((time_t) until_ts_pg_time_t, 0, ical_tz);
    // it's safe ? time_t may be double, float, etc...

    return pg_rrule_get_occurrences_rrule_until(*recurrence_ref, dtstart, until, true);
}

Datum pg_rrule_get_occurrences_dtstart(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);
    Timestamp dtstart_ts = PG_GETARG_TIMESTAMP(1);

    pg_time_t dtstart_ts_pg_time_t = timestamptz_to_time_t(dtstart_ts);

    struct icaltimetype dtstart = icaltime_from_timet_with_zone((time_t) dtstart_ts_pg_time_t, 0,
                                                                icaltimezone_get_utc_timezone());
    // it's safe ? time_t may be double, float, etc...

    return pg_rrule_get_occurrences_rrule(*recurrence_ref, dtstart, false);
}

Datum pg_rrule_get_occurrences_dtstart_until(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    Timestamp dtstart_ts = PG_GETARG_TIMESTAMP(1);
    Timestamp until_ts = PG_GETARG_TIMESTAMP(2);

    pg_time_t dtstart_ts_pg_time_t = timestamptz_to_time_t(dtstart_ts);
    pg_time_t until_ts_pg_time_t = timestamptz_to_time_t(until_ts);

    struct icaltimetype dtstart = icaltime_from_timet_with_zone((time_t) dtstart_ts_pg_time_t, 0,
                                                                icaltimezone_get_utc_timezone());
    // it's safe ? time_t may be double, float, etc...
    struct icaltimetype until = icaltime_from_timet_with_zone((time_t) until_ts_pg_time_t, 0,
                                                              icaltimezone_get_utc_timezone());
    // it's safe ? time_t may be double, float, etc...

    return pg_rrule_get_occurrences_rrule_until(*recurrence_ref, dtstart, until, false);
}

/* operators */
Datum pg_rrule_eq(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data1 = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *rrule1 = (struct icalrecurrencetype*)VARDATA(varlena_data1);

    struct icalrecurrencetype *varlena_data2 = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *rrule2 = (struct icalrecurrencetype*)VARDATA(varlena_data2);

    // Compare basic fields
    if (rrule1->freq != rrule2->freq ||
        rrule1->interval != rrule2->interval ||
        rrule1->count != rrule2->count ||
        rrule1->week_start != rrule2->week_start) {
        PG_RETURN_BOOL(false);
    }

    // Compare UNTIL
    if (icaltime_compare(rrule1->until, rrule2->until) != 0) {
        PG_RETURN_BOOL(false);
    }

    // Compare RSCALE
    if ((rrule1->rscale == NULL && rrule2->rscale != NULL) ||
        (rrule1->rscale != NULL && rrule2->rscale == NULL) ||
        (rrule1->rscale != NULL && rrule2->rscale != NULL &&
         strcmp(rrule1->rscale, rrule2->rscale) != 0)) {
        PG_RETURN_BOOL(false);
    }

    // Compare BY* arrays
    for (int i = 0; i < ICAL_BY_NUM_PARTS; i++) {
        // First compare the size field
        if (rrule1->by[i].size != rrule2->by[i].size) {
            PG_RETURN_BOOL(false);
        }

        short *array1 = rrule1->by[i].data;
        short *array2 = rrule2->by[i].data;

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
        for (int j = 0; j < rrule1->by[i].size && j < max_size; j++) {
            if (array1[j] != array2[j]) {
                PG_RETURN_BOOL(false);
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

/* FREQ */
Datum pg_rrule_get_freq_rrule(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    if (recurrence_ref->freq == ICAL_NO_RECURRENCE) {
        PG_RETURN_NULL();
    }

    const char *const freq_string = icalrecur_freq_to_string(recurrence_ref->freq);

    PG_RETURN_TEXT_P(cstring_to_text(freq_string));
}

/* UNTIL */
Datum pg_rrule_get_until_rrule(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    if (icaltime_is_null_time(recurrence_ref->until)) {
        PG_RETURN_NULL();
    }

    pg_time_t until_pg_time_t = (pg_time_t) icaltime_as_timet_with_zone(
        recurrence_ref->until, icaltimezone_get_utc_timezone()); // it's safe ? time_t may be double, float, etc...

    PG_RETURN_TIMESTAMP(time_t_to_timestamptz(until_pg_time_t));
}

/* UNTIL TZ */
Datum pg_rrule_get_untiltz_rrule(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    if (icaltime_is_null_time(recurrence_ref->until)) {
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

    pg_time_t until_pg_time_t = (pg_time_t) icaltime_as_timet_with_zone(recurrence_ref->until, ical_tz);
    // it's safe ? time_t may be double, float, etc...

    PG_RETURN_TIMESTAMP(time_t_to_timestamptz(until_pg_time_t));
}

/* COUNT */
Datum pg_rrule_get_count_rrule(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);
    PG_RETURN_INT32(recurrence_ref->count);
}

/* INTERVAL */
Datum pg_rrule_get_interval_rrule(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);
    PG_RETURN_INT16(recurrence_ref->interval);
}

/* BYSECOND */
Datum pg_rrule_get_bysecond_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_SECOND, ICAL_BY_SECOND_SIZE);
}

/* BYMINUTE */
Datum pg_rrule_get_byminute_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_MINUTE, ICAL_BY_MINUTE_SIZE);
}

/* BYHOUR */
Datum pg_rrule_get_byhour_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_HOUR, ICAL_BY_HOUR_SIZE);
}

/* BYDAY */
Datum pg_rrule_get_byday_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_DAY, ICAL_BY_DAY_SIZE);
}

/* BYMONTHDAY */
Datum pg_rrule_get_bymonthday_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_MONTH_DAY, ICAL_BY_MONTHDAY_SIZE);
}

/* BYYEARDAY */
Datum pg_rrule_get_byyearday_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_YEAR_DAY, ICAL_BY_YEARDAY_SIZE);
}

/* BYWEEKNO */
Datum pg_rrule_get_byweekno_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_WEEK_NO, ICAL_BY_WEEKNO_SIZE);
}

/* BYMONTH */
Datum pg_rrule_get_bymonth_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_MONTH, ICAL_BY_MONTH_SIZE);
}

/* BYSETPOS */
Datum pg_rrule_get_bysetpos_rrule(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) PG_RETURN_NULL();

    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    return pg_rrule_get_bypart_rrule(recurrence_ref, ICAL_BY_SET_POS, ICAL_BY_SETPOS_SIZE);
}

/* WKST */
Datum pg_rrule_get_wkst_rrule(PG_FUNCTION_ARGS) {
    struct icalrecurrencetype *varlena_data = (struct icalrecurrencetype *) PG_GETARG_POINTER(0);
    struct icalrecurrencetype *recurrence_ref = (struct icalrecurrencetype*)VARDATA(varlena_data);

    if (recurrence_ref->week_start == ICAL_NO_WEEKDAY) {
        PG_RETURN_NULL();
    }

    const char *const wkst_string = icalrecur_weekday_to_string(recurrence_ref->week_start);

    PG_RETURN_TEXT_P(cstring_to_text(wkst_string));
}

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
