#include <Cory/Tools/VisualReviewProtocol.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace {

[[nodiscard]] std::filesystem::path protocolScratchRoot(std::string_view name)
{
    auto root = std::filesystem::temp_directory_path() / "Cory" / "VisualReviewProtocol_Test" /
                std::filesystem::path{name};
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

} // namespace

TEST_CASE("VisualReview protocol roundtrips requests and decisions", "[visual][VisualReviewProtocol]")
{
    using namespace Cory::Tools::VisualReview;

    const auto scratch = protocolScratchRoot("roundtrip");
    const auto requestPath = scratch / "request.json";
    const auto decisionPath = scratch / "decision.json";

    const auto request = VisualReviewRequest{
        .id = "review-123",
        .caseName = "case-name",
        .metadata = VisualReviewMetadata{.catchTestName = "protocol roundtrip",
                                         .sourceFile = "source.cpp",
                                         .sourceLine = 42,
                                         .sourceFunction = "test"},
        .baselinePath = scratch / "baseline.bmp",
        .actualPath = scratch / "actual.bmp",
        .diffPath = scratch / "diff.bmp",
        .metricsPath = scratch / "metrics.json",
        .requestPath = requestPath,
        .decisionPath = decisionPath,
        .metrics = VisualReviewMetrics{.mismatchedPixels = 12,
                                       .mismatchRatio = 0.125,
                                       .maxChannelError = 7,
                                       .meanAbsoluteError = 1.5},
    };

    writeRequest(requestPath, request);
    const auto roundTrippedRequest = readRequest(requestPath);
    REQUIRE(roundTrippedRequest);
    CHECK(roundTrippedRequest->id == request.id);
    CHECK(roundTrippedRequest->caseName == request.caseName);
    CHECK(roundTrippedRequest->metadata.catchTestName == request.metadata.catchTestName);
    CHECK(roundTrippedRequest->metadata.sourceFile == request.metadata.sourceFile);
    CHECK(roundTrippedRequest->metadata.sourceLine == request.metadata.sourceLine);
    CHECK(roundTrippedRequest->metadata.sourceFunction == request.metadata.sourceFunction);
    CHECK(roundTrippedRequest->baselinePath == request.baselinePath);
    CHECK(roundTrippedRequest->actualPath == request.actualPath);
    CHECK(roundTrippedRequest->diffPath == request.diffPath);
    CHECK(roundTrippedRequest->decisionPath == request.decisionPath);
    CHECK(roundTrippedRequest->metrics.mismatchedPixels == request.metrics.mismatchedPixels);
    CHECK(roundTrippedRequest->metrics.mismatchRatio == request.metrics.mismatchRatio);
    CHECK(roundTrippedRequest->metrics.maxChannelError == request.metrics.maxChannelError);
    CHECK(roundTrippedRequest->metrics.meanAbsoluteError == request.metrics.meanAbsoluteError);

    const auto decision = VisualReviewDecision{.requestId = request.id,
                                               .accepted = true,
                                               .note = "accepted in test"};
    writeDecision(decisionPath, decision);
    const auto roundTrippedDecision = readDecision(decisionPath);
    REQUIRE(roundTrippedDecision);
    CHECK(roundTrippedDecision->requestId == decision.requestId);
    CHECK(roundTrippedDecision->accepted == decision.accepted);
    CHECK(roundTrippedDecision->note == decision.note);
}

TEST_CASE("VisualReview protocol rejects malformed schema", "[visual][VisualReviewProtocol]")
{
    using namespace Cory::Tools::VisualReview;

    const auto scratch = protocolScratchRoot("schema");
    const auto requestPath = scratch / "request.json";
    const auto decisionPath = scratch / "decision.json";

    {
        std::ofstream out{requestPath, std::ios::binary | std::ios::trunc};
        out << "{\n  \"schema\": \"bad\",\n  \"id\": \"x\"\n}\n";
    }
    {
        std::ofstream out{decisionPath, std::ios::binary | std::ios::trunc};
        out << "{\n  \"schema\": \"bad\",\n  \"requestId\": \"x\"\n}\n";
    }

    CHECK_FALSE(readRequest(requestPath).has_value());
    CHECK_FALSE(readDecision(decisionPath).has_value());
}
