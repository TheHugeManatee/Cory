# FindSlang.cmake
#
# Detects Slang as shipped with the Vulkan SDK or a custom install.
#
# Provides:
#   Slang_FOUND
#   Slang_VERSION
#   Slang_VERSION_STRING
#   Slang_SLANGC_EXECUTABLE
#   Slang_INCLUDE_DIR
#   Slang_LIBRARY
#   Slang_RUNTIME_LIBRARIES
#
# Imported targets:
#   Slang::slangc - slangc executable
#   Slang::slang - linkable slang runtime library
#
# ------------------------------------------------------------

include(FindPackageHandleStandardArgs)

# ------------------------------------------------------------
# Hints
# ------------------------------------------------------------
set(_SLANG_HINTS)

if (DEFINED ENV{VULKAN_SDK})
    list(APPEND _SLANG_HINTS "$ENV{VULKAN_SDK}")
endif ()

if (Slang_ROOT)
    list(APPEND _SLANG_HINTS "${Slang_ROOT}")
endif ()

# ------------------------------------------------------------
# Find slangc executable
# ------------------------------------------------------------
find_program(Slang_SLANGC_EXECUTABLE
        NAMES slangc
        HINTS ${_SLANG_HINTS}
        PATH_SUFFIXES
        Bin
        bin
)

# ------------------------------------------------------------
# Detect Slang version (robust: -version or --version)
# ------------------------------------------------------------
set(Slang_VERSION_STRING "")
set(Slang_VERSION "")

if (Slang_SLANGC_EXECUTABLE)

    # Try legacy "-version" first (Vulkan SDK Slang)
    execute_process(
            COMMAND "${Slang_SLANGC_EXECUTABLE}" -version
            OUTPUT_VARIABLE _SLANG_VERSION_STDOUT
            ERROR_VARIABLE _SLANG_VERSION_OUTPUT # it seems like versions that react to '-version' write to stderr
            RESULT_VARIABLE _SLANG_VERSION_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Fallback to "--version" if needed
    if (NOT _SLANG_VERSION_RESULT EQUAL 0)
        execute_process(
                COMMAND "${Slang_SLANGC_EXECUTABLE}" --version
                OUTPUT_VARIABLE _SLANG_VERSION_OUTPUT
                ERROR_VARIABLE _SLANG_VERSION_ERROR
                RESULT_VARIABLE _SLANG_VERSION_RESULT
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif ()

    if (_SLANG_VERSION_RESULT EQUAL 0)
        # Example outputs:
        #   "2025.11-12-gc5295eae2"
        #   "slang version 2024.17"
        #   "2024.6"
        #
        # 1) Full version string (keep git suffix)
        string(REGEX MATCH
                "([0-9]+\\.[0-9]+([.-][^ ]+)?)"
                Slang_VERSION_STRING
                "${_SLANG_VERSION_OUTPUT}"
        )

        # 2) CMake-safe numeric version (major.minor)
        #    Needed for VERSION_LESS, etc.
        string(REGEX MATCH
                "([0-9]+\\.[0-9]+)"
                Slang_VERSION
                "${Slang_VERSION_STRING}"
        )
    endif ()
endif ()

# ------------------------------------------------------------
# Find headers
# ------------------------------------------------------------
find_path(Slang_INCLUDE_DIR
        NAMES slang.h
        HINTS ${_SLANG_HINTS}
        PATH_SUFFIXES
        include
        Include
        slang/include
        slang/Include
        slang
        Include/slang
        include/slang

        NO_DEFAULT_PATH
)

if (NOT Slang_INCLUDE_DIR)
    message(STATUS "Could NOT find Slang_INCLUDE_DIR. Searched in: ${_SLANG_HINTS} with suffixes include, Include, slang/include, slang/Include, slang, Include/slang, include/slang.")
endif ()

# ------------------------------------------------------------
# Find runtime library
# ------------------------------------------------------------
find_library(Slang_LIBRARY
        NAMES slang
        HINTS ${_SLANG_HINTS}
        PATH_SUFFIXES
        lib
        Lib
        lib64
        Bin
        bin
)

# ------------------------------------------------------------
# Runtime deployment libraries
# ------------------------------------------------------------
set(Slang_RUNTIME_LIBRARIES "")

if (Slang_LIBRARY)
    #list(APPEND Slang_RUNTIME_LIBRARIES "${Slang_LIBRARY}")

    # Windows: slang.dll may depend on extra DLLs shipped in Bin/ or bin/
    if (WIN32)
        get_filename_component(_SLANG_BIN_DIR "${Slang_LIBRARY}" DIRECTORY)

        # Check both Bin and bin for runtime DLLs
        set(_SLANG_BIN_DIRS "${_SLANG_BIN_DIR}")
        if (DEFINED ENV{VULKAN_SDK})
            list(APPEND _SLANG_BIN_DIRS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
        endif ()

        foreach (_dir IN LISTS _SLANG_BIN_DIRS)
            foreach (_dll
                    slang.dll
                    slang-glslang.dll
            )
                if (EXISTS "${_dir}/${_dll}")
                    list(APPEND Slang_RUNTIME_LIBRARIES "${_dir}/${_dll}")
                endif ()
            endforeach ()
        endforeach ()
    endif ()
endif ()

list(REMOVE_DUPLICATES Slang_RUNTIME_LIBRARIES)

# ------------------------------------------------------------
# Handle result
# ------------------------------------------------------------
find_package_handle_standard_args(Slang
        REQUIRED_VARS
        Slang_SLANGC_EXECUTABLE
        Slang_INCLUDE_DIR
        Slang_LIBRARY
        VERSION_VAR Slang_VERSION
)

# ------------------------------------------------------------
# Imported targets
# ------------------------------------------------------------
if (Slang_FOUND)

    # slangc tool
    if (NOT TARGET Slang::slangc)
        add_library(Slang::slangc INTERFACE IMPORTED)
        set_target_properties(Slang::slangc PROPERTIES
                SLANGC_EXECUTABLE "${Slang_SLANGC_EXECUTABLE}"
        )
    endif ()

    # slang runtime
    if (NOT TARGET Slang::slang)
        add_library(Slang::slang INTERFACE IMPORTED)
        set_target_properties(Slang::slang PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${Slang_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES "${Slang_LIBRARY}"
        )
    endif ()

endif ()
