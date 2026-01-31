set(TARGET_NAME "DynamicPipeline")

find_package(Vulkan REQUIRED)
find_package(range-v3 REQUIRED)
find_package(CLI11 REQUIRED)

add_executable(${TARGET_NAME}
        src/main.cpp
        src/DynamicPipelineApplication.cpp
        src/DynamicPipelineApplication.hpp
)

target_compile_definitions(${TARGET_NAME} PRIVATE "DYNAMIC_PIPELINE_RESOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\"")

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
add_test(NAME Smoketests.DynamicPipeline COMMAND ${TARGET_NAME} --frames=100)
endif ()

install(TARGETS ${TARGET_NAME}
        EXPORT ${PROJECT_TARGETS}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
