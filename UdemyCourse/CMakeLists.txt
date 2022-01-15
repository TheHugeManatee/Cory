set(TARGET_NAME "00_CompatibilityTest")

# Always list the files explicitly
add_executable(${TARGET_NAME} 
    00_CompatibilityTest.cpp
)

find_package(Vulkan REQUIRED)
find_package(glfw3)
find_package(spdlog)

# add headers as sources automatically - this makes them show up in some IDEs
# but is not strictly necessary (unless you are using CMAKE_AUTOMOC)
file(GLOB_RECURSE HEADERS "include/*.h")
target_sources(${TARGET_NAME} PRIVATE ${HEADERS})
 
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${TARGET_NAME} 
    PRIVATE
        ThirdPartyLibs
        Vulkan::Vulkan
        glfw::glfw
        spdlog::spdlog
)

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

if(WIN32)
    target_link_options(${TARGET_NAME} PUBLIC /ignore:4099)
endif()

# CMake scripts extensions
target_set_warnings(${TARGET_NAME} ENABLE ALL DISABLE Annoying)


# Install in the desired folder
install(TARGETS ${TARGET_NAME}
  EXPORT ${PROJECT_TARGETS}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
