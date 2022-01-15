## This is where you put and configure external dependencies
## A couple of options are:
# * Header-only libraries: Just include directly into your repository

set(TARGET_NAME ThirdPartyLibs)

find_package(VulkanHeaders)
find_package(imgui)
find_package(glfw3)
find_package(glm)

add_library(${TARGET_NAME} STATIC
        src/stb_image.cpp
        src/tiny_obj_loader.cpp
        src/vk_mem_alloc.cpp
        src/imgui_impl_vulkan.cpp
        src/imgui_impl_glfw.cpp
        src/imGuIZMOquat.cpp
        )

# optick
file(GLOB_RECURSE OPTICK_SRC "src/optick/*.cpp")
target_sources(${TARGET_NAME} PRIVATE ${OPTICK_SRC})
target_include_directories(${TARGET_NAME} PRIVATE "include/optick")
if (WIN32)
    target_compile_definitions(${TARGET_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif ()


# Headers
file(GLOB_RECURSE HEADERS "include/*.h")
target_sources(${TARGET_NAME} PRIVATE ${HEADERS})


target_include_directories(${TARGET_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(${TARGET_NAME}
        PUBLIC
        imgui::imgui
        glfw::glfw
        glm::glm
        PRIVATE
        Vulkan::Headers
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