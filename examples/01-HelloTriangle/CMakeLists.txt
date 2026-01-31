set(TARGET_NAME "HelloTriangle")

find_package(Vulkan REQUIRED)
find_package(range-v3 REQUIRED)
find_package(CLI11 REQUIRED)

# Always list the files explicitly
add_executable(${TARGET_NAME}
        src/main.cpp
        src/HelloTriangleApplication.cpp
        src/HelloTriangleApplication.hpp
        src/TrianglePipeline.cpp
        src/TrianglePipeline.hpp
)

set(HELLO_TRIANGLE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if (EXISTS "${HELLO_TRIANGLE_INCLUDE_DIR}")
    target_include_directories(${TARGET_NAME} PRIVATE ${HELLO_TRIANGLE_INCLUDE_DIR})
endif ()

target_compile_definitions(${TARGET_NAME} PRIVATE "TRIANGLE_RESOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\"")

target_link_libraries(${TARGET_NAME}
        PRIVATE
        project_options
        project_warnings
        Cory::Cory
        Cory::ThirdParty
        Vulkan::Vulkan
        range-v3::range-v3
        CLI11::CLI11
)

if (CORY_BUILD_TESTS)
    enable_testing()
add_test(NAME Smoketests.HelloTriangle COMMAND ${TARGET_NAME} --frames=100 --headless)
endif ()

# Install in the desired folder
install(TARGETS ${TARGET_NAME}
        EXPORT ${PROJECT_TARGETS}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
