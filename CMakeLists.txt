cmake_minimum_required(VERSION 3.10)
project(pg_rrule C)

# Find PostgreSQL
find_program(PG_CONFIG pg_config)
if (NOT PG_CONFIG)
    message(FATAL_ERROR "pg_config not found")
endif ()

# Get PostgreSQL paths
execute_process(
        COMMAND ${PG_CONFIG} --includedir-server
        OUTPUT_VARIABLE PG_SERVER_INCLUDEDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Set libical paths
set(LIBICAL_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/libical/build/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/libical/src/libical"
        "${CMAKE_CURRENT_SOURCE_DIR}/libical/build/src/libical"
)

# Use the static library
set(LIBICAL_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/libical/build/lib/libical.a")

# Find static ICU libraries explicitly
find_library(ICU_UC_STATIC_LIBRARY libicuuc.a)
find_library(ICU_I18N_STATIC_LIBRARY libicui18n.a)
find_library(ICU_DATA_STATIC_LIBRARY libicudata.a)

if (NOT ICU_UC_STATIC_LIBRARY OR NOT ICU_I18N_STATIC_LIBRARY OR NOT ICU_DATA_STATIC_LIBRARY)
    message(FATAL_ERROR "Could not find static ICU libraries")
endif ()

# Create the extension
add_library(pg_rrule MODULE
        src/pg_rrule.c
)

# Set include directories
target_include_directories(pg_rrule PRIVATE
        ${PG_SERVER_INCLUDEDIR}
        ${LIBICAL_INCLUDE_DIRS}
        /usr/include # ICU headers
)

# Enable PIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Link with libraries
target_link_libraries(pg_rrule PRIVATE
        ${LIBICAL_LIBRARY}
        -Wl,--whole-archive
        ${ICU_UC_STATIC_LIBRARY}
        ${ICU_I18N_STATIC_LIBRARY}
        ${ICU_DATA_STATIC_LIBRARY}
        -Wl,--no-whole-archive
        -ldl # Required for static ICU
        -pthread
)

# Set library properties
set_target_properties(pg_rrule PROPERTIES
        PREFIX ""
        OUTPUT_NAME "pg_rrule"
        POSITION_INDEPENDENT_CODE ON
)

# Add compile definitions
target_compile_definitions(pg_rrule PRIVATE
        HAVE_CONFIG_H
)