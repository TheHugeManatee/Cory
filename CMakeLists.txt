cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "Cory")
set(PROJECT_TARGETS "coryTargets")

project(${PROJECT_NAME} CXX)

#### Setup Installation directories
include(GNUInstallDirs)
set(CMAKE_INSTALL_PDBDIR ${CMAKE_INSTALL_BINDIR}) # typically, we want the PDB files in the same directory

# output all binaries and libs into a common folder across all libraries
# This helps especially on windows when running/debuggin directly from the IDE: The compiled binaries will all
# be in one directory, thus DLLs will that are built with this project do not have to be copied around manually
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

############################
##  Modules and scripts   ##
############################

# Standard CMake modules

include(CMakeDependentOption)# This is a really useful scripts that creates options that depends on other options. It can even be used with generator expressions !

# Custom modules and scripts
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(Warnings)

# disable this to debug conan-related problems
option(CORY_CONAN_CMAKE_SILENT_OUTPUT "Silence conan output in CMake config" ON )
set(CONAN_CMAKE_SILENT_OUTPUT ${CORY_CONAN_CMAKE_SILENT_OUTPUT})

### Run conan install so we don't have to do it manually
if (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake"
            "${CMAKE_BINARY_DIR}/conan.cmake"
            TLS_VERIFY ON)
endif ()
include(${CMAKE_BINARY_DIR}/conan.cmake)
conan_cmake_autodetect(conan_settings)
message("Conan autodetected settings: ${conan_settings}")
# fixup RelWithDebInfo to Release (most packages don't have a RelWithDebInfo build)
string(REGEX REPLACE "RelWithDebInfo" "Release" conan_settings "${conan_settings}")
message("Running CONAN command. This might take a while if libraries need to be built.")
conan_cmake_install(PATH_OR_REFERENCE ${CMAKE_SOURCE_DIR}
        SETTINGS ${conan_settings}
        BUILD missing
        OUTPUT_QUIET)

# add this to find the conan-generated Find*.cmake files
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_BINARY_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_BINARY_DIR})

###############
##  OPTIONS  ##
###############

if (${CMAKE_GENERATOR} MATCHES "Unix Makefiles")
    message(STATUS ${CMAKE_BUILD_TOOL})
    set(CMAKE_MAKE_PROGRAM "${CMAKE_MAKE_PROGRAM} -j${MULTITHREADED_BUILD}")
    message(STATUS "Added arguments to CMAKE_BUILD_TOOL: ${CMAKE_MAKE_PROGRAM}")
elseif (MSVC)
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    #    message(STATUS "Added parallel build arguments to CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
endif ()

# You should try to give as much control over the project setup to the user.
# When modifying compile flags for example, if they are not mandatory, provide an option.

option(CORY_BUILD_USE_DOXYGEN "Add a doxygen target to generate the documentation" ON)

# Use your own option for tests, in case people use your library through add_subdirectory
cmake_dependent_option(CORY_BUILD_TESTS
        "Enable tests targets" ON # By default we want tests if CTest is enabled
        "BUILD_TESTING" ON # Stay coherent with CTest variables
        )
if(CORY_BUILD_TESTS)
    #include(CTest)
    enable_testing()
endif()


# currently, we just map a hardcoded path for our resources - no fancy
# resource bundling or deployment yet
set(CORY_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resource CACHE PATH "Directory to resources (textures/shaders)")


# External dependencies are built into one static library
add_subdirectory(ThirdParty)

# It is always easier to navigate in an IDE when projects are organized in folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(Cory)
add_subdirectory(HelloTriangle)


# Setup our project as the startup project for Visual so that people don't need to do it manually
#set_directory_properties(PROPERTIES VS_STARTUP_PROJECT HelloCory)


#===========#
#   Tests   #
#===========#

# if(BP_BUILD_TESTS)
#     # Let the user add options to the test runner if needed
#     set(TEST_RUNNER_PARAMS "--force-colors=true" CACHE STRING "Options to add to our test runners commands")
#     # In a real project you most likely want to exclude test folders
#     # list(APPEND CUSTOM_COVERAGE_EXCLUDE "/test/")
#     add_subdirectory(tests)
#     # You can setup some custom variables and add them to the CTestCustom.cmake.in template to have custom ctest settings
#     # For example, you can exclude some directories from the coverage reports such as third-parties and tests
#     configure_file(
#         ${CMAKE_CURRENT_LIST_DIR}/cmake/CTestCustom.cmake.in
#         ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake
#         @ONLY
#     )
# endif()

#############
## Doxygen ##
#############

if (CORY_BUILD_USE_DOXYGEN AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.9)
    find_package(Doxygen
            OPTIONAL_COMPONENTS dot mscgen dia
            )
    if (DOXYGEN_FOUND)
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
        doxygen_add_docs(
                doc
                README.md source
                COMMENT "Generate man pages"
        )
    endif ()
endif ()