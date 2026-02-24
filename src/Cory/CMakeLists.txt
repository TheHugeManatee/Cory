set(TARGET_NAME Cory)
# where the headers will be installed, relative to the project's header install directory
set(TARGET_HEADER_SUBDIR "Cory/")

find_package(Vulkan REQUIRED)
find_package(Slang REQUIRED)
message(STATUS "Found Slang version ${Slang_VERSION} at ${Slang_LIBRARY}")

find_package(glfw3 REQUIRED)
find_package(Microsoft.GSL REQUIRED)
find_package(fmt REQUIRED)
find_package(cppcoro REQUIRED)

if (MSVC)
    # Conan currently provides cppcoro binaries with mismatched coroutine ABIs between
    # Release and Debug on Windows for this workspace:
    # - Release cppcoro.lib exports std::experimental::coroutine_handle (needs /await)
    # - Debug cppcoro.lib exports std::coroutine_handle (must not use /await)
    # Match consumer compile options per config to avoid _COROUTINE_ABI linker mismatches.
    set_property(TARGET cppcoro::cppcoro PROPERTY INTERFACE_COMPILE_OPTIONS
            "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/await>")
endif ()

find_package(EnTT REQUIRED)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(magic_enum REQUIRED)
find_package(range-v3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(efsw REQUIRED)

set(CORY_SOURCES

        Application/Application.cpp
        Application/Application.hpp
        Application/ApplicationLayer.hpp
        Application/CameraLayer.cpp
        Application/CameraLayer.hpp
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
        Base/Concepts.hpp
        Base/CoroBatch.cpp
        Base/CoroBatch.hpp
        Base/Coro.cpp
        Base/Coro.hpp
        Base/CoroOps.hpp
        Base/CoroThreadPool.cpp
        Base/CoroThreadPool.hpp
        Base/CpuBuffer.hpp
        Base/Debugger.cpp
        Base/Debugger.hpp
        Base/EnumMap.hpp
        Base/FileWatchManager.cpp
        Base/FileWatchManager.hpp
        Base/FmtUtils.hpp
        Base/Function.hpp
        Base/FutureFrameQueue.hpp
        Base/GlmUtils.hpp
        Base/Locked.hpp
        Base/Log.cpp
        Base/Log.hpp
        Base/Math.hpp
        Base/Primitives.hpp
        Base/Profiling.cpp
        Base/Profiling.hpp
        Base/Random.hpp
        Base/ResourceLocator.cpp
        Base/ResourceLocator.hpp
        Base/Result.hpp
        Base/SignalTree.cpp
        Base/SignalTree.hpp
        Base/SimulationClock.cpp
        Base/SimulationClock.hpp
        Base/SlotMap.hpp
        Base/SlotMapHandle.hpp
        Base/Time.cpp
        Base/Time.hpp
        Base/Utils.cpp
        Base/Utils.hpp
        Base/ValOptional.hpp
        Base/WorkContractGroup.cpp
        Base/WorkContractGroup.hpp
        Cory.cpp
        Cory.hpp
        Framegraph/Builder.cpp
        Framegraph/Common.hpp
        Framegraph/Framegraph.cpp
        Framegraph/Framegraph.hpp
        Framegraph/FramegraphResourceManager.cpp
        Framegraph/FramegraphResourceManager.hpp
        Framegraph/FramegraphVisualizer.cpp
        Framegraph/FramegraphVisualizer.h
        Framegraph/RenderTaskBuilder.hpp
        Framegraph/RenderTaskDeclaration.hpp
        Framegraph/ShaderBindingContext.cpp
        Framegraph/ShaderBindingContext.hpp
        Framegraph/TransientComputePass.cpp
        Framegraph/TransientComputePass.hpp
        Framegraph/TransientRenderPass.cpp
        Framegraph/TransientRenderPass.hpp
        IO/Bmp.cpp
        IO/Bmp.hpp
        ImGui/ImguiRenderer.cpp
        ImGui/ImguiRenderer.hpp
        ImGui/Inputs.hpp
        ImGui/Widgets.cpp
        ImGui/Widgets.hpp
        RenderTasks/StandardRenderTasks.cpp
        RenderTasks/StandardRenderTasks.hpp
        Renderer/Common.cpp
        Renderer/Common.hpp
        Renderer/AsyncUploader.cpp
        Renderer/AsyncUploader.hpp
        Renderer/Context.cpp
        Renderer/Context.hpp
        Renderer/DescriptorSets.cpp
        Renderer/DescriptorSets.hpp
        Renderer/FrameGenerator.cpp
        Renderer/FrameGenerator.hpp
        Renderer/FrameContext.hpp
        Renderer/FrameResourceLifetimeHelper.hpp
        Renderer/HeadlessFrameSource.cpp
        Renderer/HeadlessFrameSource.hpp
        Renderer/FrameSource.hpp
        Renderer/Gpu.cpp
        Renderer/Gpu.hpp
        Renderer/GpuBumpAllocator.cpp
        Renderer/GpuBumpAllocator.hpp
        Renderer/MappedCoherentDeviceBuffer.cpp
        Renderer/MappedCoherentDeviceBuffer.hpp
        Renderer/PipelineCache.cpp
        Renderer/PipelineCache.hpp
        Renderer/RadixSorter.cpp
        Renderer/RadixSorter.hpp
        Renderer/ThreadScheduler.cpp
        Renderer/ThreadScheduler.hpp
        Renderer/Shader.cpp
        Renderer/Shader.hpp
        Renderer/ShaderHotReloader.cpp
        Renderer/ShaderHotReloader.hpp
        Renderer/ShaderManager.cpp
        Renderer/ShaderManager.hpp
        Renderer/SingleShotCommandRecorder.cpp
        Renderer/SingleShotCommandRecorder.hpp
        Renderer/SlangCompiler.cpp
        Renderer/SlangCompiler.hpp
        Renderer/Swapchain.cpp
        Renderer/Swapchain.hpp
        Renderer/Synchronization.cpp
        Renderer/Synchronization.hpp
        SceneGraph/Common.hpp
        SceneGraph/SceneGraph.cpp
        SceneGraph/SceneGraph.hpp
        SceneGraph/System.cpp
        SceneGraph/System.hpp
        Systems/CommonComponents.hpp
        Systems/ImGuizmoTransformSystem.cpp
        Systems/ImGuizmoTransformSystem.hpp
        Systems/SystemCoordinator.cpp
        Systems/SystemCoordinator.hpp
        Systems/TransformSystem.cpp
        Systems/TransformSystem.hpp
        Renderer/SlangCompilerTools.cpp
        Renderer/SlangCompilerTools.hpp
        Base/Prop.hpp
        Systems/ComponentEditorSystem.cpp
        Systems/ComponentEditorSystem.hpp
        Proper/Properties.cpp
        Proper/Properties.hpp
        Proper/PropertySet.cpp
        Proper/PropertySet.hpp
)


# Use ImGui GLFW backend from local imgui module's res/bindings folder
set(IMGUI_BACKEND_DIR "${imgui_INCLUDE_DIR}/../res/bindings")
set(IMGUI_GLFW_IMPL_CPP "${IMGUI_BACKEND_DIR}/imgui_impl_glfw.cpp")
set(IMGUI_GLFW_IMPL_H "${IMGUI_BACKEND_DIR}/imgui_impl_glfw.h")

list(APPEND CORY_SOURCES
        ${IMGUI_GLFW_IMPL_CPP}
)

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
        PRIVATE
        ${IMGUI_BACKEND_DIR}
)

target_link_libraries(${TARGET_NAME}
        PUBLIC
        ${VULKAN_LIBRARIES}
        Cory::ThirdParty
        Microsoft.GSL::GSL
        fmt::fmt
        glm::glm
        imgui::imgui
        EnTT::EnTT
        magic_enum::magic_enum
        range-v3::range-v3
        cppcoro::cppcoro
        spdlog::spdlog
        KDGpu::KDGpu
        KDGpu::KDGpuUtils
        glfw
        KDUtils::KDGui
        Slang::slang
        efsw::efsw

        PRIVATE

        KDGpu::KDGpuExample
        KDGpuExample::Resources # for the embedded roboto font
        KDGpuExample::ShaderResources # for the embedded imgui shaders
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
target_compile_definitions(${TARGET_NAME} PUBLIC "CORY_DATA_DIR=\"${CORY_DATA_DIR}\"")

# generate the export header for this library
include(GenerateExportHeader)
generate_export_header(${TARGET_NAME} EXPORT_FILE_NAME "${TARGET_HEADER_SUBDIR}${TARGET_NAME}_export.h")


# group this lib into the 'libs' group for some IDEs
#set_target_properties (${TARGET_NAME} PROPERTIES FOLDER libs)

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
