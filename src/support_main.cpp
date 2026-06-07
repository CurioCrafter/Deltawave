#include "Visualizer/Support/SupportBundle.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void printUsage()
{
    std::cout
        << "VisualizerSupport [options]\n\n"
        << "Options:\n"
        << "  --workspace DIR       Visualizer workspace root, default current directory\n"
        << "  --output DIR          Bundle output directory, default artifacts/support_bundle_YYYYMMDD_HHMMSS\n"
        << "  --max-captures N      Recent live capture directories to summarize, default 5\n"
        << "  --max-artifacts N     Recent export/artifact directories to summarize, default 5\n"
        << "  --max-file-bytes N    Largest metadata/profile file to copy, default 262144\n"
        << "  --no-copy             Write manifest/summary only; do not copy metadata/profile files\n"
        << "  --help, -h            Show this help\n";
}

bool nextValue(int& index, int argc, char** argv, std::string& value)
{
    if (index + 1 >= argc) {
        return false;
    }
    value = argv[++index];
    return true;
}

int parseInt(const std::string& value, int fallback)
{
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

std::uintmax_t parseSize(const std::string& value, std::uintmax_t fallback)
{
    try {
        return static_cast<std::uintmax_t>(std::stoull(value));
    } catch (...) {
        return fallback;
    }
}

} // namespace

int main(int argc, char** argv)
{
    viz::SupportBundleOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        std::string value;
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
        if (arg == "--workspace" && nextValue(i, argc, argv, value)) {
            options.workspaceRoot = value;
        } else if (arg == "--output" && nextValue(i, argc, argv, value)) {
            options.outputDirectory = value;
        } else if (arg == "--max-captures" && nextValue(i, argc, argv, value)) {
            options.maxCaptures = parseInt(value, options.maxCaptures);
        } else if (arg == "--max-artifacts" && nextValue(i, argc, argv, value)) {
            options.maxArtifacts = parseInt(value, options.maxArtifacts);
        } else if (arg == "--max-file-bytes" && nextValue(i, argc, argv, value)) {
            options.maxCopiedFileBytes = parseSize(value, options.maxCopiedFileBytes);
        } else if (arg == "--no-copy") {
            options.copySmallFiles = false;
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            printUsage();
            return 2;
        }
    }

    viz::SupportBundleResult result;
    std::string error;
    if (!viz::writeSupportBundle(options, result, error)) {
        std::cerr << "Support bundle failed: " << error << "\n";
        return 1;
    }

    std::cout << "Support bundle " << result.bundleDirectory.string() << "\n";
    std::cout << "Manifest " << result.manifestPath.string() << "\n";
    std::cout << "Summary " << result.summaryPath.string() << "\n";
    std::cout << "Profiles " << result.profilesFound << " found, "
              << result.profilesCopied << " copied\n";
    std::cout << "Recent captures " << result.captureDirectoriesFound
              << " recent artifacts " << result.artifactDirectoriesFound << "\n";
    std::cout << "Metadata files copied " << result.metadataFilesCopied
              << " skipped for size " << result.filesSkippedForSize << "\n";
    return 0;
}
