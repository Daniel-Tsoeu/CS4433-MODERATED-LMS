cmake_minimum_required(VERSION 3.16)
project(ModeratedLMS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LMS_BUILD_TESTS "Build the GoogleTest suite" ON)
option(LMS_COVERAGE "Instrument for gcov/lcov coverage" OFF)
option(LMS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# ---- dependencies ---------------------------------------------------------
find_package(SQLite3 REQUIRED)
if(TARGET SQLite3::SQLite3)  # CMake >= 4.3 renamed the imported target
  set(LMS_SQLITE SQLite3::SQLite3)
else()
  set(LMS_SQLITE SQLite::SQLite3)
endif()
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SODIUM QUIET IMPORTED_TARGET libsodium)
endif()
if(TARGET PkgConfig::SODIUM)
  set(LMS_SODIUM PkgConfig::SODIUM)
else()
  find_library(SODIUM_LIBRARY sodium REQUIRED)
  find_path(SODIUM_INCLUDE_DIR sodium.h REQUIRED)
  add_library(lms_sodium_import INTERFACE)
  target_link_libraries(lms_sodium_import INTERFACE ${SODIUM_LIBRARY})
  target_include_directories(lms_sodium_import INTERFACE ${SODIUM_INCLUDE_DIR})
  set(LMS_SODIUM lms_sodium_import)
endif()

# ---- embed sql/schema.sql so the SQL file is the single source of truth ---
file(READ ${CMAKE_SOURCE_DIR}/sql/schema.sql LMS_SCHEMA_SQL)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/sql/schema.sql)
configure_file(${CMAKE_SOURCE_DIR}/cmake/SchemaSql.cpp.in
               ${CMAKE_BINARY_DIR}/generated/SchemaSql.cpp @ONLY)

# ---- common compile options ----------------------------------------------
add_library(lms_options INTERFACE)
target_compile_options(lms_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow
                       -Wnon-virtual-dtor -Woverloaded-virtual)
if(LMS_WARNINGS_AS_ERRORS)
  target_compile_options(lms_options INTERFACE -Werror)
endif()
if(LMS_COVERAGE)
  target_compile_options(lms_options INTERFACE --coverage -O0 -g)
  target_link_options(lms_options INTERFACE --coverage)
endif()

# ---- layers: link dependencies enforce the dependency rule ---------------
#   presentation -> application -> domain <- infrastructure
file(GLOB DOMAIN_SRC CONFIGURE_DEPENDS src/domain/*.cpp)
add_library(lms_domain STATIC ${DOMAIN_SRC})
target_include_directories(lms_domain PUBLIC src)
target_link_libraries(lms_domain PUBLIC lms_options)

file(GLOB APPLICATION_SRC CONFIGURE_DEPENDS src/application/*.cpp)
add_library(lms_application STATIC ${APPLICATION_SRC})
target_link_libraries(lms_application PUBLIC lms_domain)

file(GLOB INFRASTRUCTURE_SRC CONFIGURE_DEPENDS src/infrastructure/*.cpp)
add_library(lms_infrastructure STATIC ${INFRASTRUCTURE_SRC}
            ${CMAKE_BINARY_DIR}/generated/SchemaSql.cpp)
target_link_libraries(lms_infrastructure PUBLIC lms_domain ${LMS_SQLITE} ${LMS_SODIUM})

file(GLOB PRESENTATION_SRC CONFIGURE_DEPENDS src/presentation/*.cpp)
add_library(lms_presentation STATIC ${PRESENTATION_SRC})
target_link_libraries(lms_presentation PUBLIC lms_application)

add_executable(lms src/main.cpp)
target_compile_definitions(lms PRIVATE LMS_VERSION="${PROJECT_VERSION}")
target_link_libraries(lms PRIVATE lms_presentation lms_infrastructure)
install(TARGETS lms RUNTIME DESTINATION bin)

# ---- tests ---------------------------------------------------------------
if(LMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
