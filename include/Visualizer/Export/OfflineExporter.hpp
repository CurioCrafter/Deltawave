#pragma once

#include "Visualizer/Visualization/VisualizerEngine.hpp"
#include "Visualizer/Audio/AudioAnalyzer.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace viz {

struct OfflineExportOptions {
    std::filesystem::path inputAudio;
    std::filesystem::path outputDirectory;
    VisualSettings settings{};
    int width = 1280;
    int height = 720;
    int frameRate = 60;
    double maxSeconds = 0.0;
    std::filesystem::path outputVideo;
    std::filesystem::path styleProfile;
    std::filesystem::path syncProfile;
    std::filesystem::path ffmpegExecutable = "ffmpeg";
    std::string lookName;
    int videoCrf = 18;
    std::string videoPreset = "medium";
    bool autoScene = false;
    float environmentTimeOfDay = 0.5f;
    bool sharePackage = false;
};

struct TrackIntelligenceSummary {
    int downbeatsDetected = 0;
    int phraseBoundariesDetected = 0;
    float averageBarConfidence = 0.0f;
    float averagePhraseConfidence = 0.0f;
    float peakDownbeatConfidence = 0.0f;
    float peakDropIntensity = 0.0f;
    float peakPhraseIntensity = 0.0f;
    float peakBuildTension = 0.0f;
    float averageHarmonicEnergy = 0.0f;
    AudioStyle dominantStyle = AudioStyle::Silence;
    float dominantStyleConfidence = 0.0f;
    int maxPrimitiveCount = 0;
};

struct OfflineExportResult {
    std::filesystem::path outputDirectory;
    int framesWritten = 0;
    double durationSeconds = 0.0;
    float peakRms = 0.0f;
    float estimatedBpm = 0.0f;
    float finalHueShift = 0.0f;
    float minimumHueShift = 1.0f;
    float maximumHueShift = 0.0f;
    float keyConfidence = 0.0f;
    float sectionConfidence = 0.0f;
    int detectedKeyIndex = -1;
    MusicalMode detectedKeyMode = MusicalMode::Unknown;
    ArrangementSection dominantSection = ArrangementSection::Silence;
    int beatsDetected = 0;
    std::filesystem::path outputVideo;
    std::filesystem::path timelinePath;
    bool videoEncoded = false;
    std::uintmax_t videoBytes = 0;
    bool timelineWritten = false;
    bool sharePackageGenerated = false;
    std::filesystem::path shareManifest;
    std::filesystem::path sharePage;
    std::filesystem::path previewImage;
    int previewFramesUsed = 0;
    int previewWidth = 0;
    int previewHeight = 0;
    TrackIntelligenceSummary trackSummary;
};

bool exportAudioToFrames(const OfflineExportOptions& options,
                         OfflineExportResult& result,
                         std::string& error);

bool exportWavToFrames(const OfflineExportOptions& options,
                       OfflineExportResult& result,
                       std::string& error);

} // namespace viz
