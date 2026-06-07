#pragma once

#include <filesystem>
#include <string>

namespace viz {

struct PreviewImageResult {
    std::filesystem::path outputPath;
    int sourceFramesFound = 0;
    int previewFramesUsed = 0;
    int width = 0;
    int height = 0;
    bool written = false;
};

bool writeFramePreviewContactSheet(const std::filesystem::path& framesDirectory,
                                   const std::filesystem::path& outputBmp,
                                   int maxFrames,
                                   int thumbnailWidth,
                                   PreviewImageResult& result,
                                   std::string& error);

} // namespace viz
