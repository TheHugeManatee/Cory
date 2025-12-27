## KDGPU Does not support conan, so we consume it via FetchContent here
include(FetchContent)

find_package(KDBindings REQUIRED)
find_package(imgui REQUIRED)
if(NOT TARGET Vulkan::Vulkan)
    find_package(Vulkan REQUIRED)
endif()
find_package(glm REQUIRED)

# disable some features to reduce configure & build times
set(KDGPU_BUILD_KDXR OFF)
set(KDGPU_OPENXR_SUPPORT OFF)
set(KDGPU_BUILD_TESTS OFF)
set(VMA_STATIC_VULKAN_FUNCTIONS ON)

FetchContent_Declare(
        KDGpu
        #GIT_REPOSITORY https://github.com/KDAB/KDGpu.git
        #GIT_TAG v0.8.0
        SOURCE_DIR "C:/dev/KDGpu"
        #GIT_REPOSITORY https://github.com/TheHugeManatee/KDGpu.git
        #GIT_TAG 4f2ebb0a4a63f97bbb4ef8e21cdbe8a7b0b3cebf
)
FetchContent_MakeAvailable(KDGpu)

#include(${KDGpu_SOURCE_DIR}/cmake/CMakeRC.cmake)

set(TARGET_NAME ThirdPartyLibs)

add_library(${TARGET_NAME} STATIC
        src/stb_image.cpp
        src/tiny_obj_loader.cpp
        src/vk_mem_alloc.cpp
        src/imGuIZMOquat.cpp
)

# optick
set(OPTICK_SRC
        src/optick/optick_capi.cpp
        src/optick/optick_core.cpp
        src/optick/optick_gpu.cpp
        src/optick/optick_gpu.d3d12.cpp
        src/optick/optick_gpu.vulkan.cpp
        src/optick/optick_message.cpp
        src/optick/optick_miniz.cpp
        src/optick/optick_serialization.cpp
        src/optick/optick_server.cpp
)
target_sources(${TARGET_NAME} PRIVATE ${OPTICK_SRC})
target_include_directories(${TARGET_NAME} PRIVATE "include/optick")
if (WIN32)
    target_compile_definitions(${TARGET_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif ()
target_link_libraries(${TARGET_NAME} PRIVATE Vulkan::Vulkan)

# Corrected HEADERS file list
set(HEADERS
        include/optick/optick.config.h
        include/optick/optick.h
        include/optick/optick_capi.h
        include/optick/optick_common.h
        include/optick/optick_core.freebsd.h
        include/optick/optick_core.h
        include/optick/optick_core.linux.h
        include/optick/optick_core.macos.h
        include/optick/optick_core.platform.h
        include/optick/optick_core.win.h
        include/optick/optick_gpu.h
        include/optick/optick_memory.h
        include/optick/optick_message.h
        include/optick/optick_miniz.h
        include/optick/optick_serialization.h
        include/optick/optick_server.h
)

target_sources(${TARGET_NAME} PRIVATE ${HEADERS})


target_include_directories(${TARGET_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(${TARGET_NAME}
        PUBLIC
        imgui::imgui
        glm::glm
)

add_library(Cory::ThirdParty ALIAS ThirdPartyLibs)

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