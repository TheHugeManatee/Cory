set(TARGET_NAME Cory)
# where the headers will be installed, relative to the project's header install directory
set(TARGET_HEADER_SUBDIR "Cory/")

find_package(Vulkan REQUIRED FATAL_ERROR)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(GLEW)
find_package(glm REQUIRED)
find_package(glfw3)
find_package(imgui REQUIRED)
find_package(shaderc)
find_package(Magnum REQUIRED Vk)
find_package(Microsoft.GSL REQUIRED)
find_package(magic_enum REQUIRED)
find_package(range-v3 REQUIRED)


set(CORY_SOURCES
        src/Cory.cpp
        include/Cory/Cory.hpp
        include/Cory/Core/CpuBuffer.hpp
        include/Cory/Core/Math.hpp
        include/Cory/Core/SlotMap.hpp
        src/Core/Log.cpp
        include/Cory/Core/Log.hpp
        src/Core/Utils.cpp
        include/Cory/Core/Utils.hpp
        src/Core/Context.cpp 
        include/Cory/Core/Context.hpp 
        include/Cory/Core/FmtUtils.hpp 
        include/Cory/Core/VulkanUtils.hpp 
        src/Core/VulkanUtils.cpp 
        include/Cory/Core/BitField.hpp
        src/UI/Window.cpp 
        include/Cory/UI/Window.hpp 
        src/UI/Application.cpp 
        include/Cory/UI/Application.hpp 
        include/Cory/Core/Common.hpp
        )

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
        spdlog::spdlog
        fmt::fmt
        GLEW::GLEW
        glm::glm
        Microsoft.GSL::GSL
        glfw::glfw
        imgui::imgui
        shaderc::shaderc
        Cory::ThirdParty
        range-v3::range-v3
        magic_enum::magic_enum

        PRIVATE
        Cory::project_warnings
        Cory::project_options
        Magnum::Vk
        )

# Define a macro that allows consumers of this lib to know when they link against it
target_compile_definitions(${TARGET_NAME}
        PUBLIC
        HAVE_CORY
        GLM_FORCE_RADIANS
        GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL)

# for now, we hardcode the resource path until we have a proper resource management in place
target_compile_definitions(${TARGET_NAME} PUBLIC "RESOURCE_DIR=\"${CORY_RESOURCE_DIR}\"")

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