set(TARGET_NAME Cory)
# where the headers will be installed, relative to the project's header install directory
set(TARGET_HEADER_SUBDIR "Cory/") 

find_package(Vulkan REQUIRED FATAL_ERROR)

set(CORY_VK_SOURCES   
  src/vk/buffer.cpp
  src/vk/command_buffer.cpp
  src/vk/command_pool.cpp
  src/vk/graphics_context.cpp
  src/vk/image.cpp
  src/vk/image_view.cpp
  src/vk/instance.cpp
  src/vk/swapchain.cpp
  src/vk/test_utils.cpp
  src/vk/utils.cpp
)

set(CORY_SOURCES 
  src/Application.cpp
  src/Buffer.cpp
  src/CameraManipulator.cpp
  src/Context.cpp
  src/Descriptor.cpp
  src/Image.cpp
  src/ImGuiLayer.cpp
  src/Log.cpp
  src/Mesh.cpp
  src/Profiling.cpp
  src/Shader.cpp
  src/Utils.cpp
  src/VkBuilders.cpp
  src/VkUtils.cpp
)

file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${TARGET_NAME} STATIC
  ${CORY_VK_SOURCES} ${CORY_SOURCES} ${HEADERS})

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src PREFIX src FILES ${CORY_VK_SOURCES} ${CORY_SOURCES})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/include/Cory PREFIX include FILES ${HEADERS})

target_include_directories (${TARGET_NAME} 
  PUBLIC 
    ${VULKAN_INCLUDE_DIRS} 
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty
)

target_link_libraries(${TARGET_NAME} 
  PUBLIC 
    CONAN_PKG::spdlog
    CONAN_PKG::fmt
    CONAN_PKG::glew
    CONAN_PKG::glm
    CONAN_PKG::glfw
    CONAN_PKG::vulkan-headers
    CONAN_PKG::imgui
    CONAN_PKG::shaderc
    CONAN_PKG::doctest
    ThirdPartyLibs
  PRIVATE
    ${Vulkan_LIBRARIES}
)

# Define a macro that allows consumers of this lib to know they link against it
target_compile_definitions(${TARGET_NAME} PUBLIC HAVE_CORY)
# for now, we hardcode the resource path until we have a proper resource management in place
target_compile_definitions(${TARGET_NAME} PUBLIC "RESOURCE_DIR=\"${CORY_RESOURCE_DIR}\"")

# generate the export header for this library
include(GenerateExportHeader)
generate_export_header(${TARGET_NAME} EXPORT_FILE_NAME "${TARGET_HEADER_SUBDIR}${TARGET_NAME}_export.h")

# CMake scripts extensions
target_set_warnings(${TARGET_NAME} ENABLE ALL DISABLE Annoying)
#target_enable_lto(${TARGET_NAME} optimized) #enable lto if available for non-debug configurations
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

# group this lib into the 'libs' group for some IDEs
#set_target_properties (${TARGET_NAME} PROPERTIES FOLDER libs)

# Set the include directories of this library
target_include_directories(${TARGET_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/Cory>
)

###### TEST BINARY

add_executable(${TARGET_NAME}_Test test_main.cpp)
target_link_libraries(${TARGET_NAME}_Test ${TARGET_NAME})
doctest_force_link_static_lib_in_target(${TARGET_NAME}_Test ${TARGET_NAME})
add_test(NAME ${TARGET_NAME}_Test COMMAND $<TARGET_FILE:${TARGET_NAME}_Test> --no-version)
target_compile_features(${TARGET_NAME}_Test PRIVATE cxx_std_20)
#doctest_add_test(NAME CoryTesting COMMAND $<TARGET_FILE:Cory_Test> --no-version)


###### INSTALLATION

# Install in the desired folder
install(TARGETS ${TARGET_NAME}
  EXPORT ${PROJECT_TARGETS}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install includes
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# Install PDB file (MSVC debug symbols) for consumers of this library, if this library is 
# a shared library
get_target_property(target_type ${TARGET_NAME} TYPE)
if(target_type STREQUAL SHARED_LIBRARY)
  install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> DESTINATION ${CMAKE_INSTALL_PDBDIR} OPTIONAL)
endif()
# Install the export header
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_HEADER_SUBDIR}${TARGET_NAME}_export.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_HEADER_SUBDIR})