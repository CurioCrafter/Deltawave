#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace viz {

struct SupportBundleOptions {
    std::filesystem::path workspaceRoot;
    std::filesystem::path outputDirectory;
    int maxCaptures = 5;
    int maxArtifacts = 5;
    std::uintmax_t maxCopiedFileBytes = 256U * 1024U;
    bool copySmallFiles = true;
};

struct SupportBundleResult {
    std::filesystem::path bundleDirectory;
    std::filesystem::path manifestPath;
    std::filesystem::path summaryPath;
    int profilesFound = 0;
    int profilesCopied = 0;
    int captureDirectoriesFound = 0;
    int artifactDirectoriesFound = 0;
    int metadataFilesCopied = 0;
    int filesSkippedForSize = 0;
    std::uintmax_t copiedBytes = 0;
};

bool writeSupportBundle(const SupportBundleOptions& options,
                        SupportBundleResult& result,
                        std::string& error);

} // namespace viz
