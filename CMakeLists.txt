cmake_minimum_required(VERSION 2.8.12)
project(VulkanTutorial)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(VkTutorial main.cpp)
target_link_libraries(VkTutorial ${CONAN_LIBS})

set_target_properties(VkTutorial
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)