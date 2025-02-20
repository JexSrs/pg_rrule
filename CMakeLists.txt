cmake_minimum_required(VERSION 3.10)
project(pg_rrule C)

# ====================================
# PostgreSQL
# ====================================

message(STATUS "Looking for pg_config to find PostgreSQL...")

# Find PostgreSQL
find_program(PG_CONFIG pg_config)
if (NOT PG_CONFIG)
    message(FATAL_ERROR "pg_config not found. Please install PostgreSQL development packages.")
else()
    message(STATUS "pg_config found: ${PG_CONFIG}")
endif ()
# Get PostgreSQL paths
execute_process(
        COMMAND ${PG_CONFIG} --includedir-server
        OUTPUT_VARIABLE PG_SERVER_INCLUDEDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "PostgreSQL server include directory: ${PG_SERVER_INCLUDEDIR}")

# ====================================
# LibICAL
# ====================================
message(STATUS "Setting up LibICAL paths...")

# Set libical paths
set(LIBICAL_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/libical/build/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/libical/src/libical"
        "${CMAKE_CURRENT_SOURCE_DIR}/libical/build/src/libical"
)
# Use the static library
set(LIBICAL_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/libical/build/lib/libical.a")
message(STATUS "Using LibICAL static library: ${LIBICAL_LIBRARY}")

# ====================================
# ICU
# ====================================
message(STATUS "Determining architecture-specific library paths for ICU...")

# Determine architecture-specific library path
message(STATUS "System processor: ${CMAKE_SYSTEM_PROCESSOR}")
#if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
#    set(ARCH_LIB_PATH "/usr/lib/aarch64-linux-gnu")
#    message(STATUS "Using path for aarch64 architecture: ${ARCH_LIB_PATH}")
#elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
#    set(ARCH_LIB_PATH "/usr/lib/x86_64-linux-gnu")
#    message(STATUS "Using path for x86_64 architecture: ${ARCH_LIB_PATH}")
#else()
#    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
#endif()

set(ARCH_LIB_PATH "/usr/local/lib")

# Set ICU library paths using the architecture-specific path
set(ICU_UC_STATIC_LIBRARY "${ARCH_LIB_PATH}/libicuuc.a")
set(ICU_I18N_STATIC_LIBRARY "${ARCH_LIB_PATH}/libicui18n.a")
set(ICU_DATA_STATIC_LIBRARY "${ARCH_LIB_PATH}/libicudata.a")

message(STATUS "ICU library paths:")
message(STATUS "  - ICU UC: ${ICU_UC_STATIC_LIBRARY}")
message(STATUS "  - ICU I18N: ${ICU_I18N_STATIC_LIBRARY}")
message(STATUS "  - ICU DATA: ${ICU_DATA_STATIC_LIBRARY}")

if (NOT EXISTS ${ICU_UC_STATIC_LIBRARY} OR NOT EXISTS ${ICU_I18N_STATIC_LIBRARY} OR NOT EXISTS ${ICU_DATA_STATIC_LIBRARY})
    message(FATAL_ERROR "Could not find static ICU libraries. Please ensure ICU is installed.")
else()
    message(STATUS "Found static ICU libraries.")
endif ()

# ====================================
# Build & Linking
# ====================================
message(STATUS "Creating the shared library: pg_rrule...")

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
message(STATUS "Position Independent Code (PIC) enabled.")

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

message(STATUS "Successfully configured the pg_rrule library.")

# Add compile definitions
target_compile_definitions(pg_rrule PRIVATE
        HAVE_CONFIG_H
)

message(STATUS "pg_rrule build configuration completed.")