cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----
if (PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
    find_package(Cory CONFIG REQUIRED) # for intro, project_options, ...

    if (NOT TARGET Cory::project_options)
        message(FATAL_ERROR "Required config package not found!")
        return() # be strictly paranoid for Template Janitor github action! CK
    endif ()
endif ()

# ---- Dependencies ----

find_package(Catch2 REQUIRED)

include(Catch)

add_library(${TARGET_NAME}_TestLib OBJECT
        Cory_Test.cpp
        Utils_Test.cpp
        SlotMap_Test.cpp
        BitField_Test.cpp
        Callback_Test.cpp
        FrameGraph_Test.cpp
        CoroutinePlayground.cpp
        ResourceManager_Test.cpp TestUtils.cpp TestUtils.hpp FmtUtils_Test.cpp RenderTaskDeclaration_Test.cpp)
target_link_libraries(${TARGET_NAME}_TestLib PUBLIC Catch2::Catch2)
target_link_libraries(${TARGET_NAME}_TestLib PRIVATE Cory::project_options)
target_link_libraries(${TARGET_NAME}_TestLib PUBLIC Cory::${TARGET_NAME})

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
        OUTPUT_DIR
        .
        OUTPUT_PREFIX
        "unittests."
        OUTPUT_SUFFIX
        .xml)