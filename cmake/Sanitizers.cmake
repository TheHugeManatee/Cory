include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

function(cory_normalize_sanitizers out_var)
    set(normalized)
    foreach (entry IN LISTS ARGN)
        string(STRIP "${entry}" entry)
        if (entry STREQUAL "")
            continue()
        endif ()

        string(TOUPPER "${entry}" upper)
        if (upper STREQUAL "ASAN" OR upper STREQUAL "ADDRESS")
            list(APPEND normalized "ASAN")
        elseif (upper STREQUAL "TSAN" OR upper STREQUAL "THREAD")
            list(APPEND normalized "TSAN")
        elseif (upper STREQUAL "UBSAN" OR upper STREQUAL "UNDEFINED")
            list(APPEND normalized "UBSAN")
        else ()
            message(FATAL_ERROR
                    "Unknown sanitizer '${entry}'. Supported values are ASAN, TSAN, and UBSAN.")
        endif ()
    endforeach ()

    if (normalized)
        list(REMOVE_DUPLICATES normalized)
    endif ()

    set(${out_var} "${normalized}" PARENT_SCOPE)
endfunction()

function(cory_default_sanitizers out_var config)
    if (config STREQUAL "Debug")
        if (WIN32)
            set(defaults "ASAN")
        elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(defaults "ASAN;UBSAN")
        endif ()
    endif ()

    set(${out_var} "${defaults}" PARENT_SCOPE)
endfunction()

function(cory_check_flag out_var flag)
    string(MAKE_C_IDENTIFIER "${flag}" flag_id)
    set(cache_var "CORY_HAS_FLAG_${flag_id}")
    unset(${cache_var} CACHE)

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_QUIET TRUE)
    set(CMAKE_REQUIRED_FLAGS "${flag}")
    if (MSVC)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    else ()
        set(CMAKE_REQUIRED_LINK_OPTIONS "${flag}")
    endif ()
    check_cxx_source_compiles("int main() { return 0; }" ${cache_var})
    cmake_pop_check_state()

    set(${out_var} "${${cache_var}}" PARENT_SCOPE)
endfunction()

function(cory_collect_sanitizer_flags out_compile_flags out_link_flags out_requested out_supported config)
    cory_default_sanitizers(defaults "${config}")
    set(cache_name "CORY_SANITIZERS_${config}")
    set(${cache_name} "${defaults}" CACHE STRING
            "Semicolon-separated sanitizer list for ${config} builds (ASAN;TSAN;UBSAN).")

    cory_normalize_sanitizers(requested ${${cache_name}})

    if (NOT requested)
        set(${out_compile_flags} "" PARENT_SCOPE)
        set(${out_link_flags} "" PARENT_SCOPE)
        set(${out_requested} "" PARENT_SCOPE)
        set(${out_supported} "" PARENT_SCOPE)
        return()
    endif ()

    list(FIND requested "ASAN" has_asan)
    list(FIND requested "TSAN" has_tsan)
    if (NOT has_asan EQUAL -1 AND NOT has_tsan EQUAL -1)
        message(FATAL_ERROR
                "${cache_name} cannot enable ASAN and TSAN at the same time.")
    endif ()

    set(compile_flags)
    set(link_flags)
    set(supported)

    if (MSVC)
        foreach (sanitizer IN LISTS requested)
            if (sanitizer STREQUAL "ASAN")
                cory_check_flag(has_msvc_asan "/fsanitize=address")
                if (has_msvc_asan)
                    list(APPEND compile_flags "/fsanitize=address")
                    list(APPEND supported "${sanitizer}")
                else ()
                    message(WARNING
                            "${cache_name} requested ASAN for ${config}, but ${CMAKE_CXX_COMPILER_ID} "
                            "does not accept /fsanitize=address. Disabling it for that configuration.")
                endif ()
            else ()
                message(WARNING
                        "${cache_name} requested ${sanitizer} for ${config}, but only ASAN is supported "
                        "with MSVC-style Windows builds. Disabling it for that configuration.")
            endif ()
        endforeach ()
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        foreach (sanitizer IN LISTS requested)
            if (sanitizer STREQUAL "ASAN")
                set(flag "-fsanitize=address")
            elseif (sanitizer STREQUAL "TSAN")
                set(flag "-fsanitize=thread")
            elseif (sanitizer STREQUAL "UBSAN")
                set(flag "-fsanitize=undefined")
            endif ()

            list(APPEND compile_flags "${flag}")
            list(APPEND link_flags "${flag}")
            list(APPEND supported "${sanitizer}")
        endforeach ()

        if (supported)
            list(FIND supported "ASAN" supported_asan)
            list(FIND supported "TSAN" supported_tsan)
            if (NOT supported_asan EQUAL -1 OR NOT supported_tsan EQUAL -1)
                list(APPEND compile_flags "-fno-omit-frame-pointer")
            endif ()
        endif ()
    else ()
        message(WARNING
                "${cache_name} requested ${requested} for ${config}, but compiler "
                "${CMAKE_CXX_COMPILER_ID} is not recognized for sanitizer setup. Disabling them.")
    endif ()

    if (compile_flags)
        list(REMOVE_DUPLICATES compile_flags)
    endif ()
    if (link_flags)
        list(REMOVE_DUPLICATES link_flags)
    endif ()
    if (supported)
        list(REMOVE_DUPLICATES supported)
    endif ()

    set(${out_compile_flags} "${compile_flags}" PARENT_SCOPE)
    set(${out_link_flags} "${link_flags}" PARENT_SCOPE)
    set(${out_requested} "${requested}" PARENT_SCOPE)
    set(${out_supported} "${supported}" PARENT_SCOPE)
endfunction()

add_library(project_sanitizers INTERFACE)

foreach (config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    cory_collect_sanitizer_flags(compile_flags link_flags requested supported "${config}")

    if (requested AND NOT supported)
        message(STATUS "Cory sanitizers (${config}): requested ${requested}, no supported sanitizers enabled")
    elseif (supported)
        message(STATUS "Cory sanitizers (${config}): enabling ${supported}")
    endif ()

    foreach (flag IN LISTS compile_flags)
        target_compile_options(project_sanitizers INTERFACE "$<$<CONFIG:${config}>:${flag}>")
    endforeach ()

    foreach (flag IN LISTS link_flags)
        target_link_options(project_sanitizers INTERFACE "$<$<CONFIG:${config}>:${flag}>")
    endforeach ()
endforeach ()

add_library(Cory::project_sanitizers ALIAS project_sanitizers)
