CREATE TYPE rrule;

CREATE
OR REPLACE FUNCTION rrule_in(cstring)
    RETURNS rrule
    AS 'MODULE_PATHNAME', 'pg_rrule_in'
    LANGUAGE C IMMUTABLE STRICT;

CREATE
OR REPLACE FUNCTION rrule_out(rrule)
    RETURNS cstring
    AS 'MODULE_PATHNAME', 'pg_rrule_out'
    LANGUAGE C IMMUTABLE STRICT;

CREATE
OR REPLACE FUNCTION rrule_send(rrule)
    RETURNS bytea
    AS 'MODULE_PATHNAME', 'pg_rrule_send'
    LANGUAGE C IMMUTABLE STRICT;

CREATE
OR REPLACE FUNCTION rrule_recv(internal)
    RETURNS rrule
    AS 'MODULE_PATHNAME', 'pg_rrule_recv'
    LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE rrule (
    input = rrule_in,
    output = rrule_out,
    send = rrule_send,
    receive = rrule_recv,
    internallength = VARIABLE
);

CREATE CAST (text AS rrule)
    WITH INOUT;

CREATE CAST (varchar AS rrule)
    WITH INOUT;


/* occurrences */
CREATE
OR REPLACE FUNCTION get_occurrences(rrule, timestamp with time zone)
    RETURNS timestamp with time zone[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_occurrences_dtstart_tz'
    LANGUAGE C IMMUTABLE STRICT;

CREATE
OR REPLACE FUNCTION get_occurrences(rrule, timestamp with time zone, timestamp with time zone)
    RETURNS timestamp with time zone[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_occurrences_dtstart_until_tz'
    LANGUAGE C IMMUTABLE STRICT;


CREATE
OR REPLACE FUNCTION get_occurrences(rrule, timestamp)
    RETURNS timestamp[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_occurrences_dtstart'
    LANGUAGE C IMMUTABLE STRICT;

CREATE
OR REPLACE FUNCTION get_occurrences(rrule, timestamp, timestamp)
    RETURNS timestamp[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_occurrences_dtstart_until'
    LANGUAGE C IMMUTABLE STRICT;

/* operators */
CREATE
OR REPLACE FUNCTION rrule_eq(rrule, rrule)
RETURNS boolean
AS 'MODULE_PATHNAME', 'pg_rrule_eq'
LANGUAGE C IMMUTABLE STRICT;

CREATE
OR REPLACE FUNCTION rrule_ne(rrule, rrule)
RETURNS boolean
AS 'MODULE_PATHNAME', 'pg_rrule_ne'
LANGUAGE C IMMUTABLE STRICT;

CREATE
OPERATOR = (
    LEFTARG = rrule,
    RIGHTARG = rrule,
    PROCEDURE = rrule_eq,
    COMMUTATOR = =,
    NEGATOR = <>
);

CREATE
OPERATOR <> (
    LEFTARG = rrule,
    RIGHTARG = rrule,
    PROCEDURE = rrule_ne,
    COMMUTATOR = <>,
    NEGATOR = =
);

/* FREQ */
CREATE
OR REPLACE FUNCTION get_freq(rrule)
    RETURNS text
    AS 'MODULE_PATHNAME', 'pg_rrule_get_freq'
    LANGUAGE C IMMUTABLE STRICT;


/* UNTIL */
CREATE
OR REPLACE FUNCTION get_until(rrule)
    RETURNS timestamp
    AS 'MODULE_PATHNAME', 'pg_rrule_get_until'
    LANGUAGE C IMMUTABLE STRICT;


/* UNTIL TZ */
CREATE
OR REPLACE FUNCTION get_untiltz(rrule)
    RETURNS timestamp with time zone
    AS 'MODULE_PATHNAME', 'pg_rrule_get_untiltz'
    LANGUAGE C IMMUTABLE STRICT;


/* COUNT */
CREATE
OR REPLACE FUNCTION get_count(rrule)
    RETURNS int4
    AS 'MODULE_PATHNAME', 'pg_rrule_get_count'
    LANGUAGE C IMMUTABLE STRICT;


/* INTERVAL */
CREATE
OR REPLACE FUNCTION get_interval(rrule)
    RETURNS int2
    AS 'MODULE_PATHNAME', 'pg_rrule_get_interval'
    LANGUAGE C IMMUTABLE STRICT;


/* BYSECOND */
CREATE
OR REPLACE FUNCTION get_bysecond(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_bysecond'
    LANGUAGE C IMMUTABLE STRICT;


/* BYMINUTE */
CREATE
OR REPLACE FUNCTION get_byminute(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_byminute'
    LANGUAGE C IMMUTABLE STRICT;


/* BYHOUR */
CREATE
OR REPLACE FUNCTION get_byhour(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_byhour'
    LANGUAGE C IMMUTABLE STRICT;


/* BYDAY */
CREATE
OR REPLACE FUNCTION get_byday(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_byday'
    LANGUAGE C IMMUTABLE STRICT;


/* BYMONTHDAY */
CREATE
OR REPLACE FUNCTION get_bymonthday(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_bymonthday'
    LANGUAGE C IMMUTABLE STRICT;


/* BYYEARDAY */
CREATE
OR REPLACE FUNCTION get_byyearday(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_byyearday'
    LANGUAGE C IMMUTABLE STRICT;


/* BYWEEKNO */
CREATE
OR REPLACE FUNCTION get_byweekno(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_byweekno'
    LANGUAGE C IMMUTABLE STRICT;


/* BYMONTH */
CREATE
OR REPLACE FUNCTION get_bymonth(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_bymonth'
    LANGUAGE C IMMUTABLE STRICT;


/* BYSETPOS */
CREATE
OR REPLACE FUNCTION get_bysetpos(rrule)
    RETURNS int2[]
    AS 'MODULE_PATHNAME', 'pg_rrule_get_bysetpos'
    LANGUAGE C IMMUTABLE STRICT;


/* WKST */
CREATE
OR REPLACE FUNCTION get_wkst(rrule)
    RETURNS text
    AS 'MODULE_PATHNAME', 'pg_rrule_get_wkst'
    LANGUAGE C IMMUTABLE STRICT;



