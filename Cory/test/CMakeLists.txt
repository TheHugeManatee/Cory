cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(Cory CONFIG REQUIRED) # for intro, project_options, ...

  if(NOT TARGET Cory::project_options)
    message(FATAL_ERROR "Required config package not found!")
    return() # be strictly paranoid for Template Janitor github action! CK
  endif()
endif()

# ---- Dependencies ----

find_package(Catch2 REQUIRED)

include(Catch)

add_library(${TARGET_NAME}_TestLib OBJECT 
    Cory_Test.cpp
)
target_link_libraries(${TARGET_NAME}_TestLib PUBLIC Catch2::Catch2)
target_link_libraries(${TARGET_NAME}_TestLib PRIVATE Cory::project_options)
target_link_libraries(${TARGET_NAME}_TestLib PUBLIC Cory::${TARGET_NAME})

# Provide a simple smoke test to make sure that the CLI works and can display a --help message
add_test(NAME cli.has_help COMMAND ${TARGET_NAME} --help)

# Provide a test to verify that the version being reported from the application
# matches the version given to CMake. This will be important once you package
# your program. Real world shows that this is the kind of simple mistake that is easy
# to make, but also easy to test for.
add_test(NAME cli.version_matches COMMAND ${TARGET_NAME} --version)
set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

add_executable(${TARGET_NAME}_Tests catch_main.cpp)
target_link_libraries(${TARGET_NAME}_Tests PRIVATE 
    Cory::project_warnings 
    Cory::project_options 
    ${TARGET_NAME}_TestLib
    )

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
catch_discover_tests(
  ${TARGET_NAME}_Tests
  TEST_PREFIX
  "unittests."
  REPORTER
  xml
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "unittests."
  OUTPUT_SUFFIX
  .xml)