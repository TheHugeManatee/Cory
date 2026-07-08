#pragma once

#include <Cory/IO/Bmp.hpp>
#include <Cory/Tools/VisualReviewProtocol.hpp>

namespace Cory::Tools::VisualReview {

struct VisualReviewUiImages {
    const IO::BmpImageRgba8 *baseline{};
    const IO::BmpImageRgba8 *actual{};
    const IO::BmpImageRgba8 *diff{};
};

struct VisualReviewUiState {
    float zoom{8.0f};
    bool showDiff{false};
};

struct VisualReviewUiActions {
    bool acceptRequested{false};
    bool rejectRequested{false};
};

[[nodiscard]] VisualReviewUiActions drawReviewUi(const VisualReviewRequest &request,
                                                 const VisualReviewUiImages &images,
                                                 VisualReviewUiState &state);

} // namespace Cory::Tools::VisualReview
