set(TARGET_NAME "HelloTriangle")

find_package(Vulkan REQUIRED)
find_package(GLEW REQUIRED)
find_package(range-v3 REQUIRED)

# Always list the files explicitly
add_executable(${TARGET_NAME}
        src/main.cpp
        src/HelloTriangleApplication.cpp src/HelloTriangleApplication.hpp)

# add headers as sources automatically - this makes them show up in some IDEs
# but is not strictly necessary (unless you are using CMAKE_AUTOMOC)
file(GLOB_RECURSE HEADERS "include/*.h")
target_sources(${TARGET_NAME} PRIVATE ${HEADERS})

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET_NAME}
        PRIVATE
        project_options
        project_warnings
        Cory::Cory
        Cory::ThirdParty
        Vulkan::Vulkan
        GLEW::GLEW
        range-v3::range-v3
        )


# Install in the desired folder
install(TARGETS ${TARGET_NAME}
        EXPORT ${PROJECT_TARGETS}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
