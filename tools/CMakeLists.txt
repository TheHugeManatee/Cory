cmake_minimum_required(VERSION 3.15)

add_subdirectory(visual)

# Tools CMake helpers
# Copy graphviz javascript assets and preview template into the build/runtime
# output directory at configure time so HTML previews can be opened from there.

set(GRAPHVIZ_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/graphviz")

# Prefer the runtime output directory if the project sets it, otherwise fall
# back to the top-level binary dir
if(DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(DEPLOY_ROOT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
else()
    set(DEPLOY_ROOT "${CMAKE_BINARY_DIR}")
endif()

set(GRAPHVIZ_OUT_DIR "${DEPLOY_ROOT}/graphviz")

if(EXISTS "${GRAPHVIZ_SRC_DIR}/js")
    file(MAKE_DIRECTORY "${GRAPHVIZ_OUT_DIR}")
    file(COPY "${GRAPHVIZ_SRC_DIR}/js" DESTINATION "${GRAPHVIZ_OUT_DIR}")
    message(STATUS "Copied graphviz/js -> ${GRAPHVIZ_OUT_DIR}/js")
else()
    message(STATUS "No graphviz/js folder found in ${GRAPHVIZ_SRC_DIR}; nothing copied")
endif()

# Also copy the preview template if present so the runtime folder contains the
# same `preview.html` used for editing.
if(EXISTS "${GRAPHVIZ_SRC_DIR}/preview.html")
    file(COPY "${GRAPHVIZ_SRC_DIR}/preview.html" DESTINATION "${GRAPHVIZ_OUT_DIR}")
    message(STATUS "Copied preview.html -> ${GRAPHVIZ_OUT_DIR}/preview.html")
endif()
