# Standalone build of the Qt SQL PostgreSQL (QPSQL) driver plugin.
#
# Homebrew's qtbase bottle ships only libqsqlite, so we compile the stock
# Qt 6.9.3 psql driver sources (vendored next to this file) against the
# installed Qt + Homebrew libpq and produce libqsqlpsql.dylib. The driver
# loads libpq at runtime via QLibrary, so nothing here is baked into QuickDb
# itself: if this build is skipped or fails, the app simply does not list the
# QPSQL driver and degrades with its existing "driver unavailable" warning.
cmake_minimum_required(VERSION 3.21)
project(qsqlpsql_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Reuse the exact Qt QuickDb already links against.
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Sql)

# Homebrew libpq. Qt 6.9's psql driver links libpq symbols directly (the
# PQ* functions are referenced, not dlopen'd), so we must link against it.
find_path(PSQL_LIBPQ_INCLUDE_DIR NAMES libpq-fe.h
          HINTS /opt/homebrew/opt/libpq/include /usr/local/opt/libpq/include)
find_library(PSQL_LIBPQ_LIBRARY NAMES pq libpq
             HINTS /opt/homebrew/opt/libpq/lib /usr/local/opt/libpq/lib)
if(NOT PSQL_LIBPQ_INCLUDE_DIR OR NOT PSQL_LIBPQ_LIBRARY)
    message(FATAL_ERROR "libpq not found; install `brew install libpq`.")
endif()

add_library(qsqlpsql MODULE
    main.cpp
    qsql_psql.cpp
    qsql_psql_p.h
)

set_target_properties(qsqlpsql PROPERTIES
    OUTPUT_NAME "qsqlpsql"
    PREFIX "lib"
    SUFFIX ".dylib"
)

target_compile_definitions(qsqlpsql PRIVATE
    QT_PLUGIN
    QT_NO_CAST_FROM_ASCII
)

target_include_directories(qsqlpsql PRIVATE
    ${PSQL_LIBPQ_INCLUDE_DIR}
)

# QtSql/QtCore private headers (qsqldriver_p.h, qsqlresult_p.h, ...) resolve
# through the *Private packages that carry the versioned header directories.
target_link_libraries(qsqlpsql PRIVATE
    Qt6::Sql
    Qt6::Core
    "${PSQL_LIBPQ_LIBRARY}"
)

# Manually attach the private header search paths so <QtSql/private/...> and
# <QtCore/private/...> resolve against the installed frameworks.
get_target_property(_sql_loc Qt6::Sql LOCATION)
get_filename_component(_sql_fw "${_sql_loc}" DIRECTORY)
set(_sql_hdr "${_sql_fw}/Headers")
set(_ver "")
file(GLOB _vers LIST DIRECTORIES "${_sql_hdr}/6.*")
if(_vers)
    list(GET _vers 0 _ver)
endif()
if(_ver)
    target_include_directories(qsqlpsql SYSTEM PRIVATE "${_ver}")
endif()

find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
target_link_libraries(qsqlpsql PRIVATE Qt6::CorePrivate)
