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

add_library(Cory_TestLib OBJECT
        Utils_Test.cpp
        SlotMap_Test.cpp
        BitField_Test.cpp
        Callback_Test.cpp
        FrameGraph_Test.cpp
        CoroutinePlayground.cpp
        ShaderManager_Test.cpp
        TestUtils.cpp
        TestUtils.hpp
        FmtUtils_Test.cpp
        RenderTaskDeclaration_Test.cpp
        DescriptorSetManager_Test.cpp
        SimulationClock_Test.cpp
        FutureFrameQueue_Test.cpp
        LayerStack_Test.cpp
        SceneGraph_Test.cpp
        CoroutineGameLogic.cpp
        TransformSystem_Test.cpp
        SystemCoordinator_Test.cpp
        Math_Test.cpp
        SignalTree_Test.cpp
        WorkContractGroup_Test.cpp
        ValOptional_Test.cpp
        Function_Test.cpp
        SlangCompiler_Test.cpp
        FileWatchManager_Test.cpp
        FrameResourceLifetimeHelper_Test.cpp
        Locked_Test.cpp
        RadixSort_Test.cpp
        ../examples/05-ParticleCompute/src/RadixSorter.cpp
)

target_include_directories(Cory_TestLib PRIVATE ${CMAKE_SOURCE_DIR}/examples/05-ParticleCompute/src)

target_link_libraries(Cory_TestLib PUBLIC Catch2::Catch2)
target_link_libraries(Cory_TestLib PRIVATE Cory::project_options)
target_link_libraries(Cory_TestLib PUBLIC Cory::Cory)

add_executable(Cory_Tests catch_main.cpp)
target_link_libraries(Cory_Tests PRIVATE
        Cory::project_warnings
        Cory::project_options
        Cory_TestLib
)

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
catch_discover_tests(
        Cory_Tests
        TEST_PREFIX
        "unittests."
        OUTPUT_DIR
        ../Cory/test
        OUTPUT_PREFIX
        "unittests."
        OUTPUT_SUFFIX
        .xml)
