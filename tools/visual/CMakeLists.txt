find_package(CLI11 REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(CoryVisualReviewProtocol STATIC
        src/VisualReviewProtocol.cpp
        include/Cory/Tools/VisualReviewProtocol.hpp
)

add_library(CoryVisualReviewUi STATIC
        src/VisualReviewUi.cpp
        include/Cory/Tools/VisualReviewUi.hpp
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

target_include_directories(CoryVisualReviewUi
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(CoryVisualReviewUi
        PUBLIC
        Cory::Cory
        Cory::VisualReviewProtocol
        Cory::project_options
        Cory::project_warnings
)

add_library(Cory::VisualReviewProtocol ALIAS CoryVisualReviewProtocol)
add_library(Cory::VisualReviewUi ALIAS CoryVisualReviewUi)

add_executable(VisualDiffReviewer
        src/VisualDiffReviewer.cpp
)

target_link_libraries(VisualDiffReviewer
        PRIVATE
        Cory::Cory
        Cory::VisualReviewProtocol
        Cory::VisualReviewUi
        CLI11::CLI11
        Cory::project_options
        Cory::project_warnings
)
