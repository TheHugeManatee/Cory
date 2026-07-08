#include <Cory/Tools/VisualReviewProtocol.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Cory::Tools::VisualReview {
namespace {

using json = nlohmann::json;

[[nodiscard]] std::string pathToString(const std::filesystem::path &path)
{
    return path.generic_string();
}

[[nodiscard]] std::filesystem::path pathFromJson(const json &value)
{
    return std::filesystem::path{value.get<std::string>()};
}

[[nodiscard]] json metricsToJson(const VisualReviewMetrics &metrics)
{
    return json{{"mismatchedPixels", metrics.mismatchedPixels},
                {"mismatchRatio", metrics.mismatchRatio},
                {"maxChannelError", metrics.maxChannelError},
                {"meanAbsoluteError", metrics.meanAbsoluteError}};
}

[[nodiscard]] json metadataToJson(const VisualReviewMetadata &metadata)
{
    return json{{"catchTestName", metadata.catchTestName},
                {"sourceFile", metadata.sourceFile},
                {"sourceLine", metadata.sourceLine},
                {"sourceFunction", metadata.sourceFunction}};
}

[[nodiscard]] VisualReviewMetadata metadataFromJson(const json &value)
{
    return VisualReviewMetadata{
        .catchTestName = value.value<std::string>("catchTestName", ""),
        .sourceFile = value.value<std::string>("sourceFile", ""),
        .sourceLine = value.value<uint64_t>("sourceLine", 0),
        .sourceFunction = value.value<std::string>("sourceFunction", ""),
    };
}

[[nodiscard]] VisualReviewMetrics metricsFromJson(const json &value)
{
    return VisualReviewMetrics{
        .mismatchedPixels = value.value<uint64_t>("mismatchedPixels", 0),
        .mismatchRatio = value.value<double>("mismatchRatio", 0.0),
        .maxChannelError = value.value<uint8_t>("maxChannelError", 0),
        .meanAbsoluteError = value.value<double>("meanAbsoluteError", 0.0),
    };
}

} // namespace

std::string makeRequestId(std::string_view caseName)
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

    std::string sanitized;
    sanitized.reserve(caseName.size());
    for (const char ch : caseName) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_') {
            sanitized.push_back(ch);
        }
        else {
            sanitized.push_back('-');
        }
    }
    if (sanitized.empty()) sanitized = "visual-review";
    return sanitized + "-" + std::to_string(micros);
}

void writeRequest(const std::filesystem::path &path, const VisualReviewRequest &request)
{
    const json value{{"schema", "cory.visual-review-request.v1"},
                     {"id", request.id},
                     {"caseName", request.caseName},
                     {"metadata", metadataToJson(request.metadata)},
                     {"baselinePath", pathToString(request.baselinePath)},
                     {"actualPath", pathToString(request.actualPath)},
                     {"diffPath", pathToString(request.diffPath)},
                     {"metricsPath", pathToString(request.metricsPath)},
                     {"requestPath", pathToString(request.requestPath)},
                     {"decisionPath", pathToString(request.decisionPath)},
                     {"metrics", metricsToJson(request.metrics)}};

    std::filesystem::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out << std::setw(2) << value << '\n';
}

std::optional<VisualReviewRequest> readRequest(const std::filesystem::path &path)
{
    std::ifstream in{path, std::ios::binary};
    if (!in) return std::nullopt;

    json value;
    try {
        in >> value;
        return VisualReviewRequest{
            .id = value.value<std::string>("id", ""),
            .caseName = value.value<std::string>("caseName", ""),
            .metadata = metadataFromJson(value.value("metadata", json::object())),
            .baselinePath = pathFromJson(value.at("baselinePath")),
            .actualPath = pathFromJson(value.at("actualPath")),
            .diffPath = pathFromJson(value.at("diffPath")),
            .metricsPath = pathFromJson(value.at("metricsPath")),
            .requestPath = pathFromJson(value.at("requestPath")),
            .decisionPath = pathFromJson(value.at("decisionPath")),
            .metrics = metricsFromJson(value.value("metrics", json::object())),
        };
    }
    catch (...) {
        return std::nullopt;
    }
}

void writeDecision(const std::filesystem::path &path, const VisualReviewDecision &decision)
{
    const json value{{"schema", "cory.visual-review-decision.v1"},
                     {"requestId", decision.requestId},
                     {"accepted", decision.accepted},
                     {"note", decision.note}};

    std::filesystem::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out << std::setw(2) << value << '\n';
}

std::optional<VisualReviewDecision> readDecision(const std::filesystem::path &path)
{
    std::ifstream in{path, std::ios::binary};
    if (!in) return std::nullopt;

    json value;
    try {
        in >> value;
        return VisualReviewDecision{.requestId = value.value<std::string>("requestId", ""),
                                    .accepted = value.value<bool>("accepted", false),
                                    .note = value.value<std::string>("note", "")};
    }
    catch (...) {
        return std::nullopt;
    }
}

} // namespace Cory::Tools::VisualReview
