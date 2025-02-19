# pg_rrule
PostgreSQL extension for working with iCalendar Recurrence Rules (RRULE)

## Build

Start docker container which has all the dependencies installed:
```sh
docker compose build && docker compose run cdev bash 
```

Build `libical`:
```sh
cd /app/libical
mkdir build
cd build
cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DICAL_GLIB=OFF \
    -DICAL_BUILD_DOCS=OFF \
    -DLIBICAL_BUILD_TESTING=OFF \
    -DCMAKE_CXX_FLAGS="-fPIC" \
    -DCMAKE_C_FLAGS="-fPIC" \
    -DCMAKE_EXE_LINKER_FLAGS="-lm -lstdc++" \
    ..
make
```

Build `pg_rrule`:
```sh
cd /app/pg_rrule
mkdir build
cd build
cmake ..
make
```

And exit docker container

## Install

Copy the files to PostgreSQL:
```sh
# Copy build file
cp ./build/pg_rrule.so /usr/lib/postgresql/17/lib/pg_rrule.so

# Copy control file
cp ./pg_rrule.control /usr/share/postgresql/17/extension/pg_rrule.control

# Copy SQL init file
cp ./sql/pg_rrule.sql:/usr/share/postgresql/17/extension/pg_rrule--0.2.0.sql
```