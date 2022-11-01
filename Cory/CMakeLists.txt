set(TARGET_NAME Cory)
# where the headers will be installed, relative to the project's header install directory
set(TARGET_HEADER_SUBDIR "Cory/")

find_package(Vulkan REQUIRED FATAL_ERROR)

find_package(GLEW)
find_package(Magnum REQUIRED Vk)
find_package(Microsoft.GSL REQUIRED)
find_package(fmt REQUIRED)
find_package(cppcoro REQUIRED)
find_package(glfw3)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(magic_enum REQUIRED)
find_package(range-v3 REQUIRED)
find_package(shaderc)
find_package(spdlog REQUIRED)

set(CORY_SOURCES
        include/Cory/Application/Application.hpp
        include/Cory/Application/ImGuiLayer.hpp
        include/Cory/Application/Window.hpp
        include/Cory/Base/BitField.hpp
        include/Cory/Base/Common.hpp
        include/Cory/Base/FmtUtils.hpp
        include/Cory/Base/Log.hpp
        include/Cory/Base/Math.hpp
        include/Cory/Base/SlotMap.hpp
        include/Cory/Base/Utils.hpp
        include/Cory/Cory.hpp
        include/Cory/RenderCore/APIConversion.hpp
        include/Cory/RenderCore/Context.hpp
        include/Cory/RenderCore/CpuBuffer.hpp
        include/Cory/Base/ResourceLocator.hpp
        include/Cory/RenderCore/Semaphore.hpp
        include/Cory/RenderCore/VulkanUtils.hpp
        include/Cory/Renderer/Shader.hpp
        include/Cory/Renderer/Swapchain.hpp
        src/Application/Application.cpp
        src/Application/ImGuiLayer.cpp
        src/Application/Window.cpp
        src/Base/Log.cpp
        src/Base/Utils.cpp
        src/Cory.cpp
        src/RenderCore/Context.cpp
        src/Base/ResourceLocator.cpp
        src/RenderCore/VulkanUtils.cpp
        src/Renderer/Shader.cpp
        src/Renderer/Swapchain.cpp
        include/Cory/Base/Callback.hpp
        src/Base/Profiling.cpp
        include/Cory/Base/Profiling.hpp
        src/RenderCore/SingleShotCommandBuffer.cpp
        include/Cory/RenderCore/SingleShotCommandBuffer.hpp
        include/Cory/Framegraph/Framegraph.hpp
        src/Framegraph/Framegraph.cpp
        include/Cory/Framegraph/RenderPassDeclaration.hpp
        include/Cory/Framegraph/TextureManager.hpp)

file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${TARGET_NAME} STATIC
        ${CORY_SOURCES}
        )

target_include_directories(${TARGET_NAME}
        PUBLIC
        ${VULKAN_INCLUDE_DIRS}
        )

target_link_libraries(${TARGET_NAME}
        PUBLIC
        ${VULKAN_LIBRARIES}
        Cory::ThirdParty
        GLEW::GLEW
        Magnum::Vk
        Microsoft.GSL::GSL
        fmt::fmt
        glfw::glfw
        glm::glm
        imgui::imgui
        magic_enum::magic_enum
        range-v3::range-v3
        cppcoro::cppcoro
        shaderc::shaderc
        spdlog::spdlog

        PRIVATE
        Cory::project_warnings
        Cory::project_options
        )

target_compile_definitions(${TARGET_NAME}
        PUBLIC
        # Define a macro that allows consumers of this lib to know when they link against it
        HAVE_CORY
        # set up some GLM defaults that match our project better
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_23)

# for now, we hardcode the resource path until we have a proper resource management in place
target_compile_definitions(${TARGET_NAME} PUBLIC "CORY_RESOURCE_DIR=\"${CORY_RESOURCE_DIR}\"")

# generate the export header for this library
include(GenerateExportHeader)
generate_export_header(${TARGET_NAME} EXPORT_FILE_NAME "${TARGET_HEADER_SUBDIR}${TARGET_NAME}_export.h")


# group this lib into the 'libs' group for some IDEs
#set_target_properties (${TARGET_NAME} PROPERTIES FOLDER libs)

# Set the include directories of this library
target_include_directories(${TARGET_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/Cory>
        )

add_library(Cory::Cory ALIAS Cory)

###### TEST BINARY

if (CORY_BUILD_TESTS)
    add_subdirectory(test)
endif ()

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
if (target_type STREQUAL SHARED_LIBRARY)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> DESTINATION ${CMAKE_INSTALL_PDBDIR} OPTIONAL)
endif ()
# Install the export header
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_HEADER_SUBDIR}${TARGET_NAME}_export.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_HEADER_SUBDIR})