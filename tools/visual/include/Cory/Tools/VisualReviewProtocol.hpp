#pragma once

#include <immer/vector.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Cory::Tools::VisualReview {

struct VisualReviewMetrics {
    uint64_t mismatchedPixels{};
    double mismatchRatio{};
    uint8_t maxChannelError{};
    double meanAbsoluteError{};
};

struct VisualReviewMetadata {
    std::string catchTestName;
    std::string sourceFile;
    uint64_t sourceLine{};
    std::string sourceFunction;
};

struct VisualReviewRequest {
    std::string id;
    std::string caseName;
    VisualReviewMetadata metadata;
    std::filesystem::path baselinePath;
    std::filesystem::path actualPath;
    std::filesystem::path diffPath;
    std::filesystem::path metricsPath;
    std::filesystem::path requestPath;
    std::filesystem::path decisionPath;
    VisualReviewMetrics metrics;
};

struct VisualReviewDecision {
    std::string requestId;
    bool accepted{false};
    std::string note;
};

struct VisualReviewQueue {
    immer::vector<VisualReviewRequest> requests;
};

[[nodiscard]] std::string makeRequestId(std::string_view caseName);

void writeRequest(const std::filesystem::path &path, const VisualReviewRequest &request);
[[nodiscard]] std::optional<VisualReviewRequest> readRequest(const std::filesystem::path &path);

void writeDecision(const std::filesystem::path &path, const VisualReviewDecision &decision);
[[nodiscard]] std::optional<VisualReviewDecision> readDecision(const std::filesystem::path &path);

} // namespace Cory::Tools::VisualReview
