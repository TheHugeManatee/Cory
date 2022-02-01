set(TARGET_NAME cvk)

find_package(Vulkan REQUIRED FATAL_ERROR)
find_package(spdlog)
find_package(fmt)
find_package(glfw3)

set(CVK_SOURCES
        include/cvk/instance.h
        include/cvk/core.h
        include/cvk/utils.h
        include/cvk/log.h
        src/instance.cpp
        src/log.cpp
        include/cvk/core.h
        )

add_library(${TARGET_NAME} STATIC
        ${CVK_GENERATED_HEADERS}
        ${CVK_GENERATED_SOURCES}
        ${CVK_SOURCES}
        )

target_link_libraries(${TARGET_NAME} PUBLIC
        Vulkan::Vulkan
        ${VULKAN_LIBRARIES}
        spdlog::spdlog
        fmt::fmt
        glfw::glfw
        )
target_include_directories(${TARGET_NAME} PUBLIC
        ${VULKAN_INCLUDE_DIR}
        )
# Set the include directories of this library
target_include_directories(${TARGET_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cvk>
        )

# CMake scripts extensions
target_set_warnings(${TARGET_NAME} ENABLE ALL DISABLE Annoying)
#target_enable_lto(${TARGET_NAME} optimized) #enable lto if available for non-debug configurations
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)


add_library(cvk::cvk ALIAS ${TARGET_NAME})

# ================================================================
find_package(doctest REQUIRED)

add_executable(cvk_test
        test/instance.cpp
        test/test_main.cpp
        test/test_utils.cpp
        )
target_link_libraries(cvk_test ${TARGET_NAME} doctest::doctest)
doctest_force_link_static_lib_in_target(cvk_test cvk)
target_compile_features(cvk_test PRIVATE cxx_std_20)

add_test(NAME cvk_test COMMAND $<TARGET_FILE:cvk_test> --no-version)