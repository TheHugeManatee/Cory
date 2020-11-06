cmake_minimum_required(VERSION 3.12)
project(VulkanTutorial CXX)

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Vulkan REQUIRED FATAL_ERROR)
add_executable(VkTutorial
    HelloTriangle.cpp 
    HelloTriangle.h 
	Utils.h
    Utils.cpp
    main.cpp
)
conan_target_link_libraries(VkTutorial ${CONAN_LIBS})

set(VKEX_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resource CACHE PATH "Directory to resources (textures/shaders)")

target_compile_definitions(VkTutorial PRIVATE "RESOURCE_DIR=\"${VKEX_RESOURCE_DIR}\"")
target_include_directories (VkTutorial PUBLIC ${VULKAN_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty)
target_link_libraries (VkTutorial ${Vulkan_LIBRARIES})


set_target_properties(VkTutorial
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)