set(TARGET_NAME "SceneGraphDemo")

find_package(Vulkan REQUIRED)
find_package(range-v3 REQUIRED)
find_package(CLI11 REQUIRED)

# Always list the files explicitly
add_executable(${TARGET_NAME}
        src/main.cpp
        src/SceneGraphDemo.cpp
        src/SceneGraphDemo.hpp
        src/CubeRenderSystem.hpp
        src/Common.hpp
        src/CubeAnimationSystem.hpp
        src/CubeRenderSystem.cpp
        src/CubeAnimationSystem.cpp
)

target_sources(${TARGET_NAME} PRIVATE ${HEADERS})

set(SCENE_GRAPH_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if (EXISTS "${SCENE_GRAPH_INCLUDE_DIR}")
    target_include_directories(${TARGET_NAME} PRIVATE ${SCENE_GRAPH_INCLUDE_DIR})
endif ()

target_compile_definitions(${TARGET_NAME} PRIVATE "SCENEGRAPHDEMO_RESOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\"")

target_link_libraries(${TARGET_NAME}
        PRIVATE
        project_sanitizers
        project_options
        project_warnings
        Cory::Cory
        Cory::ThirdParty
        CLI11::CLI11
)

if (CORY_BUILD_TESTS)
    enable_testing()
add_test(NAME Smoketests.SceneGraph COMMAND 
        ${TARGET_NAME} --frames=20 --headless)
endif ()

# Install in the desired folder
install(TARGETS ${TARGET_NAME}
        EXPORT ${PROJECT_TARGETS}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
