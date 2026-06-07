#pragma once

#include <string>
#include <vector>

namespace viz {

struct RuntimeInspectorState {
    std::string sourceLabel = "Live loopback";
    std::string sourceDetail;
    std::string activeLook = "Custom";
    std::string styleProfileName;
    std::string syncProfileName;
    std::string presetLibraryName;
    std::string captureDirectory;
    double playbackPositionSeconds = 0.0;
    double playbackDurationSeconds = 0.0;
    double captureDurationSeconds = 0.0;
    int sampleRate = 0;
    int channelCount = 0;
    int userPresetCount = 0;
    bool fileSource = false;
    bool recording = false;
};

[[nodiscard]] std::vector<std::string> formatRuntimeInspectorLines(const RuntimeInspectorState& state);

} // namespace viz
