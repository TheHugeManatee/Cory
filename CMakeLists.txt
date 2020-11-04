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

target_include_directories (VkTutorial PUBLIC ${VULKAN_INCLUDE_DIRS})
target_link_libraries (VkTutorial ${Vulkan_LIBRARIES})


set_target_properties(VkTutorial
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)