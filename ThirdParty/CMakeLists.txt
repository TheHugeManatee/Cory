## This is where you put and configure external dependencies
## A couple of options are:
# * Header-only libraries: Just include directly into your repository

set(TARGET_NAME ThirdPartyLibs)

add_library(${TARGET_NAME} STATIC 
  src/stb_image.cpp
  src/tiny_obj_loader.cpp
  src/vk_mem_alloc.cpp
  src/imgui_impl_vulkan.cpp
  src/imgui_impl_glfw.cpp
  src/imGuIZMOquat.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_link_libraries(${TARGET_NAME} 
  PRIVATE
    CONAN_PKG::vulkan-headers
    CONAN_PKG::imgui
)

# We don't install our external headers, ideally we keep the interfaces clean of 
# stuff provided in this module
# ###### INSTALLATION

# # Install in the desired folder
# install(TARGETS ${TARGET_NAME}
#   EXPORT ${PROJECT_TARGETS}
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# # Install includes
# install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})