#include "Visualizer/UI/RuntimeInspector.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace viz {
namespace {

std::string fallback(std::string value, std::string fallbackValue)
{
    return value.empty() ? std::move(fallbackValue) : std::move(value);
}

std::string formatDuration(double seconds)
{
    if (seconds <= 0.0 || !std::isfinite(seconds)) {
        return "0:00";
    }

    const int rounded = static_cast<int>(std::round(seconds));
    const int minutes = rounded / 60;
    const int remainingSeconds = rounded % 60;
    std::ostringstream output;
    output << minutes << ":" << std::setw(2) << std::setfill('0') << remainingSeconds;
    return output.str();
}

std::string formatPercent(double position, double duration)
{
    if (duration <= 0.0 || !std::isfinite(position) || !std::isfinite(duration)) {
        return "0%";
    }
    const double unit = std::clamp(position / duration, 0.0, 1.0);
    std::ostringstream output;
    output << static_cast<int>(std::round(unit * 100.0)) << "%";
    return output.str();
}

} // namespace

std::vector<std::string> formatRuntimeInspectorLines(const RuntimeInspectorState& state)
{
    std::vector<std::string> lines;
    lines.reserve(10);

    lines.push_back("SOURCE");
    lines.push_back(fallback(state.sourceLabel, state.fileSource ? "Audio file" : "Live loopback"));
    if (!state.sourceDetail.empty()) {
        lines.push_back(state.sourceDetail);
    }

    if (state.fileSource) {
        lines.push_back("Time " + formatDuration(state.playbackPositionSeconds) + " / " +
                        formatDuration(state.playbackDurationSeconds) + "  " +
                        formatPercent(state.playbackPositionSeconds, state.playbackDurationSeconds));
    } else {
        lines.push_back("Mode live loopback");
    }

    if (state.sampleRate > 0 && state.channelCount > 0) {
        lines.push_back("Audio " + std::to_string(state.sampleRate) + " Hz  " +
                        std::to_string(state.channelCount) + " ch");
    }

    lines.push_back("LOOK " + fallback(state.activeLook, "Custom"));
    if (state.userPresetCount > 0 || !state.presetLibraryName.empty()) {
        lines.push_back("User presets " + std::to_string(std::max(0, state.userPresetCount)) +
                        (state.presetLibraryName.empty() ? std::string{} : "  " + state.presetLibraryName));
    }
    lines.push_back("Style " + fallback(state.styleProfileName, "none"));
    lines.push_back("Sync " + fallback(state.syncProfileName, "none"));

    if (state.recording) {
        lines.push_back("REC " + formatDuration(state.captureDurationSeconds) +
                        (state.captureDirectory.empty() ? std::string{} : "  " + state.captureDirectory));
    } else {
        lines.push_back("Capture idle");
    }

    return lines;
}

} // namespace viz
