#pragma once

#include "Visualizer/Audio/AudioAnalyzer.hpp"
#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace viz {

struct CapturePackage {
    std::filesystem::path sessionPath;
    std::filesystem::path videoPath;
    std::filesystem::path timelinePath;
    std::filesystem::path styleProfilePath;
    std::filesystem::path syncProfilePath;
    std::string sourceLabel = "Live loopback";
    std::string lookName;
    VisualSettings requestedSettings{};
    VisualSettings finalSettings{};
    int width = 0;
    int height = 0;
    std::size_t framesWritten = 0;
    double durationSeconds = 0.0;
    double averageFrameMs = 0.0;
    double averageAnalysisMs = 0.0;
    double averageGeometryMs = 0.0;
    double averageRenderMs = 0.0;
    double averageRecordMs = 0.0;
    float peakRms = 0.0f;
    float estimatedBpm = 0.0f;
    int beatsDetected = 0;
    int downbeatsDetected = 0;
    int phraseBoundariesDetected = 0;
    double averagePhraseConfidence = 0.0;
    float peakBuildTension = 0.0f;
    int detectedKeyIndex = -1;
    MusicalMode detectedKeyMode = MusicalMode::Unknown;
    float keyConfidence = 0.0f;
    ArrangementSection dominantSection = ArrangementSection::Silence;
    float sectionConfidence = 0.0f;
    bool timelineWritten = false;
    bool videoEncoded = false;
    std::uintmax_t videoBytes = 0;
    std::string timelineWriteError;
    std::string videoEncodeError;
};

bool writeCapturePackage(const CapturePackage& package, std::string& error);

} // namespace viz
