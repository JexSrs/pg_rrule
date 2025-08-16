# pg_rrule
PostgreSQL extension for working with iCalendar Recurrence Rules (RRULE).

This project is a fork from [pg_rrule](https://github.com/petropavel13/pg_rrule) which is no longer maintained.

## Overview

PostgreSQL extension that adds features related to parsing [RFC-5545 iCalendar](https://datatracker.ietf.org/doc/html/rfc5545) data from within a PostgreSQL database.

This extension provides functionality to work with RRULE expressions in PostgreSQL, allowing you to:
- Parse RRULE strings
- Extract individual RRULE parameters
- Generate occurrence dates based on RRULE patterns

## Installation

### Build

Clone the repository with submodules:
```sh
git clone --recurse-submodules https://github.com/JexSrs/pg_rrule.git
```

Start a docker container which has all the dependencies installed:
```sh
cd pg_rrule && docker compose build && docker compose run cdev bash
```

Build `libical`:
```sh
cd /app/libical
mkdir build
cd build
cmake \
    -DSTATIC_ONLY=True \
    -DGOBJECT_INTROSPECTION=False \
    -DLIBICAL_BUILD_TESTING=False \
    -DICAL_BUILD_DOCS=False \
    -DICAL_GLIB=False \
    -DCMAKE_CXX_FLAGS="-fPIC -std=c++11" \
    -DCMAKE_C_FLAGS="-fPIC" \
    -DCMAKE_DISABLE_FIND_PACKAGE_ICU=TRUE \
    ..
make
```

Build `pg_rrule`:
```sh
cd /app
mkdir build
cd build
cmake ..
make
```

And exit docker container

### Postgresql

Copy the files to PostgreSQL:
```sh
# Copy build file
cp ./build/pg_rrule.so /usr/lib/postgresql/17/lib/pg_rrule.so

# Copy control file
cp ./pg_rrule.control /usr/share/postgresql/17/extension/pg_rrule.control

# Copy SQL init file
cp ./sql/pg_rrule.sql:/usr/share/postgresql/17/extension/pg_rrule--0.3.0.sql
```

Check if extension has been detected:
```sql
SELECT * FROM pg_available_extensions;
```
and install it:
```sql
CREATE EXTENSION pg_rrule;
```
If the extension is already installed and needs to be updated:
```sql
ALTER EXTENSION pg_rrule UPDATE;
-- or for specific version 
ALTER EXTENSION pg_rrule UPDATE TO '0.3.0';
```

## Functions

### Parameter Extraction Functions

These functions extract specific parameters from an RRULE:

- `get_freq(rrule)` - Returns the frequency (DAILY, WEEKLY, etc.)
- `get_until(rrule)` - Returns the UNTIL timestamp
- `get_untiltz(rrule)` - Returns the UNTIL timestamp with timezone
- `get_count(rrule)` - Returns the COUNT value
- `get_interval(rrule)` - Returns the INTERVAL value
- `get_bysecond(rrule)` - Returns BYSECOND array
- `get_byminute(rrule)` - Returns BYMINUTE array
- `get_byhour(rrule)` - Returns BYHOUR array
- `get_byday(rrule)` - Returns BYDAY array
- `get_bymonthday(rrule)` - Returns BYMONTHDAY array
- `get_byyearday(rrule)` - Returns BYYEARDAY array
- `get_byweekno(rrule)` - Returns BYWEEKNO array
- `get_bymonth(rrule)` - Returns BYMONTH array
- `get_bysetpos(rrule)` - Returns BYSETPOS array
- `get_wkst(rrule)` - Returns week start day

### Occurrence Generation Functions

Functions to generate occurrence dates:

- `get_occurrences(rrule, timestamp with time zone)` - Returns occurrences with timezone
- `get_occurrences(rrule, timestamp with time zone, timestamp with time zone)` - Returns occurrences within a range with timezone
- `get_occurrences(rrule, timestamp)` - Returns occurrences without timezone
- `get_occurrences(rrule, timestamp, timestamp)` - Returns occurrences within a range without timezone

## Usage Examples

### 1. Extract Frequency
```sql
SELECT get_freq('FREQ=WEEKLY;INTERVAL=1;WKST=MO;UNTIL=20200101T045102Z'::rrule);
-- Result: WEEKLY
```

### 2. Get BYDAY Values
```sql
SELECT get_byday('FREQ=WEEKLY;INTERVAL=1;WKST=MO;UNTIL=20200101T045102Z;BYDAY=MO,TH,SU'::rrule);
-- Result: {2,5,1}
```

### 3. Generate Occurrences with Timezone
```sql
SELECT * FROM unnest(
    get_occurrences('FREQ=WEEKLY;INTERVAL=1;WKST=MO;UNTIL=20200101T045102Z;BYDAY=SA;BYHOUR=10;BYMINUTE=51;BYSECOND=2'::rrule,
    '2019-12-07 10:51:02+00'::timestamp with time zone)
);
```

### 4. Generate Occurrences without Timezone
```sql
SELECT * FROM unnest(
    get_occurrences('FREQ=WEEKLY;INTERVAL=1;WKST=MO;UNTIL=20200101T045102Z;BYDAY=SA;BYHOUR=10;BYMINUTE=51;BYSECOND=2'::rrule,
    '2019-12-07 10:51:02'::timestamp)
);
```

## License

This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue if you have any ideas, feature requests, or bugs.