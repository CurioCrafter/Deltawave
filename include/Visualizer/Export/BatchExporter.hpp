#pragma once

#include "Visualizer/Export/OfflineExporter.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace viz {

struct BatchExportOptions {
    std::filesystem::path inputDirectory;
    std::filesystem::path outputDirectory;
    VisualSettings settings{};
    int width = 1280;
    int height = 720;
    int frameRate = 60;
    double maxSeconds = 0.0;
    std::filesystem::path styleProfile;
    std::filesystem::path syncProfile;
    std::filesystem::path ffmpegExecutable = "ffmpeg";
    std::string lookName;
    int videoCrf = 18;
    std::string videoPreset = "medium";
    bool autoScene = false;
    float environmentTimeOfDay = 0.5f;
    bool sharePackage = true;
    bool encodeMp4 = false;
    bool recursive = false;
    int maxFiles = 0;
};

struct BatchExportItemResult {
    std::filesystem::path inputAudio;
    std::filesystem::path outputDirectory;
    std::filesystem::path outputVideo;
    std::filesystem::path shareManifest;
    std::filesystem::path sharePage;
    std::filesystem::path previewImage;
    int previewFramesUsed = 0;
    int previewWidth = 0;
    int previewHeight = 0;
    int framesWritten = 0;
    double durationSeconds = 0.0;
    float peakRms = 0.0f;
    float estimatedBpm = 0.0f;
    int beatsDetected = 0;
    bool videoEncoded = false;
    bool sharePackageGenerated = false;
    bool success = false;
    std::string error;
    TrackIntelligenceSummary trackSummary;
};

struct BatchExportResult {
    std::filesystem::path outputDirectory;
    std::filesystem::path manifestPath;
    std::filesystem::path indexPath;
    int filesDiscovered = 0;
    int filesExported = 0;
    int filesFailed = 0;
    std::vector<BatchExportItemResult> items;
};

bool exportAudioBatch(const BatchExportOptions& options,
                      BatchExportResult& result,
                      std::string& error);

} // namespace viz
