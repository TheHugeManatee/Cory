set(TARGET_NAME "ParticleComputeDemo")

find_package(Vulkan REQUIRED)
find_package(range-v3 REQUIRED)
find_package(CLI11 REQUIRED)

# Always list the files explicitly
add_executable(${TARGET_NAME}
        src/main.cpp
        src/ParticleComputeDemo.cpp
        src/ParticleComputeDemo.hpp
        src/PointSpriteRenderSystem.hpp
        src/PointSpriteRenderSystem.cpp
        src/Common.hpp
        src/CubeAnimationSystem.hpp
        src/CubeAnimationSystem.cpp
)

target_sources(${TARGET_NAME} PRIVATE ${HEADERS})

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(${TARGET_NAME} PRIVATE "PARTICLECOMPUTEDEMO_RESOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\"")

target_link_libraries(${TARGET_NAME}
        PRIVATE
        project_options
        project_warnings
        Cory::Cory
        Cory::ThirdParty
        CLI11::CLI11
)

if (CORY_BUILD_TESTS)
    enable_testing()
    add_test(NAME Smoketests.Particle COMMAND ${TARGET_NAME} --frames=100)
endif ()

# Install in the desired folder
install(TARGETS ${TARGET_NAME}
        EXPORT ${PROJECT_TARGETS}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
