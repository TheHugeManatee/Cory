cmake_minimum_required(VERSION 3.12)
project(CMAKE_DIRECTORY_LABELS CXX)

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")
find_package(Vulkan REQUIRED FATAL_ERROR)

set(CORY_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resource CACHE PATH "Directory to resources (textures/shaders)")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

## Cory Core
add_library(CoryCore STATIC 
    Utils.h
    Utils.cpp
)

target_compile_features(CoryCore PRIVATE cxx_std_17)

target_compile_definitions(CoryCore PUBLIC "RESOURCE_DIR=\"${CORY_RESOURCE_DIR}\"")
target_include_directories (CoryCore PUBLIC ${VULKAN_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty)
conan_target_link_libraries(CoryCore ${CONAN_LIBS})

## Hello Triangle 
add_executable(HelloTriangle
    HelloTriangle.cpp 
    HelloTriangle.h 
    main.cpp
)

target_compile_features(HelloTriangle PRIVATE cxx_std_17)

conan_target_link_libraries(HelloTriangle ${CONAN_LIBS})
target_link_libraries (HelloTriangle CoryCore ${Vulkan_LIBRARIES})

