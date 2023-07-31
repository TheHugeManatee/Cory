set(TARGET_NAME Cory)
# where the headers will be installed, relative to the project's header install directory
set(TARGET_HEADER_SUBDIR "Cory/")

find_package(Vulkan REQUIRED FATAL_ERROR)

find_package(GLEW)
find_package(Magnum REQUIRED Vk)
find_package(Microsoft.GSL REQUIRED)
find_package(fmt REQUIRED)
find_package(cppcoro REQUIRED)
find_package(EnTT REQUIRED)
find_package(glfw3)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(magic_enum REQUIRED)
find_package(range-v3 REQUIRED)
find_package(shaderc)
find_package(spdlog REQUIRED)

set(CORY_SOURCES
        Application/Application.cpp
        Application/Application.hpp
        Application/ApplicationLayer.hpp
        Application/CameraManipulator.cpp
        Application/CameraManipulator.hpp
        Application/Common.hpp
        Application/DepthDebugLayer.cpp
        Application/DepthDebugLayer.hpp
        Application/DynamicGeometry.cpp
        Application/DynamicGeometry.hpp
        Application/Event.hpp
        Application/ImGuiLayer.cpp
        Application/ImGuiLayer.hpp
        Application/LayerStack.cpp
        Application/LayerStack.hpp
        Application/Window.cpp
        Application/Window.hpp
        Base/BitField.hpp
        Base/Callback.hpp
        Base/Common.hpp
        Base/CpuBuffer.hpp
        Base/FmtUtils.hpp
        Base/Log.cpp
        Base/Log.hpp
        Base/Math.hpp
        Base/Profiling.cpp
        Base/Profiling.hpp
        Base/Random.hpp
        Base/ResourceLocator.cpp
        Base/ResourceLocator.hpp
        Base/SlotMap.hpp
        Base/SlotMapHandle.hpp
        Base/Time.cpp
        Base/Time.hpp
        Base/Utils.cpp
        Base/Utils.hpp
        Cory.cpp
        Cory.hpp
        Framegraph/Builder.cpp
        Framegraph/CommandList.cpp
        Framegraph/CommandList.hpp
        Framegraph/Common.hpp
        Framegraph/Framegraph.cpp
        Framegraph/Framegraph.hpp
        Framegraph/FramegraphVisualizer.cpp
        Framegraph/FramegraphVisualizer.h
        Framegraph/RenderTaskBuilder.hpp
        Framegraph/RenderTaskDeclaration.hpp
        Framegraph/TextureManager.cpp
        Framegraph/TextureManager.hpp
        Framegraph/TransientRenderPass.cpp
        Framegraph/TransientRenderPass.hpp
        ImGui/Inputs.hpp
        Renderer/APIConversion.hpp
        Renderer/Common.cpp
        Renderer/Common.hpp
        Renderer/Context.cpp
        Renderer/Context.hpp
        Renderer/DescriptorSets.cpp
        Renderer/DescriptorSets.hpp
        Renderer/ResourceManager.cpp
        Renderer/ResourceManager.hpp
        Renderer/Semaphore.hpp
        Renderer/Shader.cpp
        Renderer/Shader.hpp
        Renderer/SingleShotCommandBuffer.cpp
        Renderer/SingleShotCommandBuffer.hpp
        Renderer/Swapchain.cpp
        Renderer/Swapchain.hpp
        Renderer/Synchronization.cpp
        Renderer/Synchronization.hpp
        Renderer/UniformBufferObject.cpp
        Renderer/UniformBufferObject.hpp
        Renderer/VulkanUtils.cpp
        Renderer/VulkanUtils.hpp
        Renderer/flextVkExt.h
        SceneGraph/System.cpp
        SceneGraph/System.hpp
        SceneGraph/SceneGraph.cpp
        SceneGraph/SceneGraph.hpp
        SceneGraph/Common.hpp
        Base/SimulationClock.cpp
        Base/SimulationClock.hpp
        Base/FutureFrameQueue.hpp
        Systems/TransformSystem.cpp Systems/TransformSystem.hpp Systems/SystemCoordinator.hpp Base/Concepts.hpp
        ImGui/Widgets.hpp
        ImGui/Widgets.cpp
        Systems/CommonComponents.hpp)

# determine includes from CORY_SOURCES by the .hpp suffix
set(CORY_INCLUDES "")
foreach (file ${CORY_SOURCES})
    if (file MATCHES ".*\\.hpp")
        list(APPEND CORY_INCLUDES ${file})
    endif ()
endforeach ()

add_library(${TARGET_NAME} STATIC
        ${CORY_SOURCES}
        )

target_include_directories(${TARGET_NAME}
        PUBLIC
        ${VULKAN_INCLUDE_DIRS}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
        $<INSTALL_INTERFACE:include>
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
        EnTT::EnTT
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
        GLM_FORCE_XYZW_ONLY
        GLM_ENABLE_EXPERIMENTAL)

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

###### INSTALLATION

# Install in the desired folder
install(TARGETS ${TARGET_NAME}
        EXPORT ${PROJECT_TARGETS}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install includes
install(FILES ${CORY_INCLUDES} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_HEADER_SUBDIR})
# Install PDB file (MSVC debug symbols) for consumers of this library, if this library is 
# a shared library
get_target_property(target_type ${TARGET_NAME} TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> DESTINATION ${CMAKE_INSTALL_PDBDIR} OPTIONAL)
endif ()
# Install the export header
install(FILES ${TARGET_NAME}_export.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_HEADER_SUBDIR})