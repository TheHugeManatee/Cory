# This file is modified from the original by Lectem, published under the following
# license:

# MIT License
# Copyright (c) 2017 Lectem
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


cmake_minimum_required(VERSION 3.1)

option(ENABLE_WARNINGS_SETTINGS "Allow target_set_warnings to add flags and defines. Set this to OFF if you want to provide your own warning parameters." ON)

function(target_set_warnings)
    if (NOT ENABLE_WARNINGS_SETTINGS)
        return()
    endif ()
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set(WMSVC TRUE)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(WGCC TRUE)
    elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        set(WCLANG TRUE)
    endif ()
    set(multiValueArgs ENABLE DISABLE AS_ERROR)
    cmake_parse_arguments(this "" "" "${multiValueArgs}" ${ARGN})
    list(FIND this_ENABLE "ALL" enable_all)
    list(FIND this_DISABLE "ALL" disable_all)
    list(FIND this_AS_ERROR "ALL" as_error_all)
    if (NOT ${enable_all} EQUAL -1)
        if (WMSVC)
            # Not all the warnings, but WAll is unusable when using libraries
            # Unless you'd like to support MSVC in the code with pragmas, this is probably the best option
            list(APPEND WarningFlags "/W4")
            # silence "warning STL4036: <ciso646> is removed in C++20."
            list(APPEND WarningFlags "/D_SILENCE_CXX20_CISO646_REMOVED_WARNING")
        else ()
            if (WGCC)
                list(APPEND WarningFlags "-Wall" "-Wextra" "-Wpedantic")
            elseif (WCLANG)
                list(APPEND WarningFlags "-Wall" "-Weverything" "-Wpedantic")
            endif ()

            ## This list is from Jason Turner's (@lefticus) cmake-starter project
            list(APPEND WarningFlags -Wall
                    -Wextra # reasonable and standard
                    -Wshadow # warn the user if a variable declaration shadows one from a
                    # parent context
                    -Wnon-virtual-dtor # warn the user if a class with virtual functions has a
                    # non-virtual destructor. This helps catch hard to
                    # track down memory errors
                    -Wold-style-cast # warn for c-style casts
                    -Wcast-align # warn for potential performance problem casts
                    -Wunused # warn on anything being unused
                    -Woverloaded-virtual # warn if you overload (not override) a virtual
                    # function
                    -Wpedantic # warn if non-standard C++ is used
                    -Wconversion # warn on type conversions that may lose data
                    -Wsign-conversion # warn on sign conversions
                    -Wmisleading-indentation # warn if identation implies blocks where blocks
                    # do not exist
                    -Wduplicated-cond # warn if if / else chain has duplicated conditions
                    -Wduplicated-branches # warn if if / else branches have duplicated code
                    -Wlogical-op # warn about logical operations being used where bitwise were
                    # probably wanted
                    -Wnull-dereference # warn if a null dereference is detected
                    -Wuseless-cast # warn if you perform a cast to the same type
                    -Wdouble-promotion # warn if float is implicit promoted to double
                    -Wformat=2 # warn on security issues around functions that format output
                    # (ie printf)
                    )
        endif ()
    elseif (NOT ${disable_all} EQUAL -1)
        set(SystemIncludes TRUE) # Treat includes as if coming from system
        if (WMSVC)
            list(APPEND WarningFlags "/w" "/W0")
        elseif (WGCC OR WCLANG)
            list(APPEND WarningFlags "-w")
        endif ()
    endif ()

    list(FIND this_DISABLE "Annoying" disable_annoying)
    if (NOT ${disable_annoying} EQUAL -1)
        if (WMSVC)
            # bounds-checked functions require to set __STDC_WANT_LIB_EXT1__ which we usually don't need/want
            list(APPEND WarningDefinitions -D_CRT_SECURE_NO_WARNINGS)
            # disable C4514 C4710 C4711... Those are useless to add most of the time
            list(APPEND WarningFlags "/wd4201") # warning C4201: nonstandard extension used: nameless struct/union
            list(APPEND WarningFlags "/wd4100") # unreferenced formal parameter
            list(APPEND WarningFlags "/wd4099") # warning LNK4099: PDB <lib.pdb> was not found with <lib.lib>; linking object as if no debug info
            list(APPEND WarningFlags "/wd4324") # structure was padded due to alignment specifier
            list(APPEND WarningFlags "/wd4103") # alignment changed after including header, may be due to missing #pragma(pop) - this one seems to be a problem specific to MSVC at the moment
            #list(APPEND WarningFlags "/wd4514" "/wd4710" "/wd4711")
            #list(APPEND WarningFlags "/wd4365") # conversion from A to B, signed/unsigned mismatch
            list(APPEND WarningFlags "/wd4018") # '>=': signed/unsigned mismatch
            #list(APPEND WarningFlags "/wd4668") # is not defined as a preprocessor macro, replacing with '0' for

        elseif (WGCC OR WCLANG)
            list(APPEND WarningFlags -Wno-switch-enum)
            if (WCLANG)
                list(APPEND WarningFlags -Wno-unknown-warning-option -Wno-padded -Wno-undef -Wno-reserved-id-macro -fcomment-block-commands=test,retval)
                if (NOT CMAKE_CXX_STANDARD EQUAL 98)
                    list(APPEND WarningFlags -Wno-c++98-compat -Wno-c++98-compat-pedantic)
                endif ()
                if ("${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC") # clang-cl has some VCC flags by default that it will not recognize...
                    list(APPEND WarningFlags -Wno-unused-command-line-argument)
                endif ()
            endif (WCLANG)
        endif ()
    endif ()

    if (NOT ${as_error_all} EQUAL -1)
        if (WMSVC)
            list(APPEND WarningFlags "/WX")
        elseif (WGCC OR WCLANG)
            list(APPEND WarningFlags "-Werror")
        endif ()
    endif ()
    foreach (target IN LISTS this_UNPARSED_ARGUMENTS)
        if (WarningFlags)
            target_compile_options(${target} INTERFACE ${WarningFlags})
        endif ()
        if (WarningDefinitions)
            target_compile_definitions(${target} INTERFACE ${WarningDefinitions})
        endif ()
        if (SystemIncludes)
            set_target_properties(${target} PROPERTIES
                    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:${target},INTERFACE_INCLUDE_DIRECTORIES>)
        endif ()
        if (WMSVC)
            target_link_options(${target} INTERFACE
                    # Disable linker warning 4099 (missing pdb file), since conan packages don't bring the pdbs
                    /ignore:4099
                    )
        endif ()
    endforeach ()
endfunction(target_set_warnings)

add_library(project_warnings INTERFACE)
target_set_warnings(project_warnings ENABLE ALL DISABLE Annoying)
add_library(Cory::project_warnings ALIAS project_warnings)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_23)
add_library(Cory::project_options ALIAS project_options)
