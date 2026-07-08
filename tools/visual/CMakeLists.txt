find_package(CLI11 REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(CoryVisualReviewProtocol STATIC
        src/VisualReviewProtocol.cpp
        include/Cory/Tools/VisualReviewProtocol.hpp
)

target_include_directories(CoryVisualReviewProtocol
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(CoryVisualReviewProtocol
        PUBLIC
        nlohmann_json::nlohmann_json
        immer
)

add_library(Cory::VisualReviewProtocol ALIAS CoryVisualReviewProtocol)

add_executable(VisualDiffReviewer
        src/VisualDiffReviewer.cpp
)

target_link_libraries(VisualDiffReviewer
        PRIVATE
        Cory::Cory
        Cory::VisualReviewProtocol
        CLI11::CLI11
        Cory::project_options
        Cory::project_warnings
)
