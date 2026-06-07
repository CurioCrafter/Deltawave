#pragma once

#include "Visualizer/Audio/AudioAnalyzer.hpp"
#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace viz {

struct AnalysisTimelineEntry {
    int frameIndex = 0;
    double timeSeconds = 0.0;
    AudioMetrics metrics{};
    VisualSettings settings{};
    int primitiveCount = 0;
};

class AnalysisTimelineWriter {
public:
    bool open(const std::filesystem::path& path, std::string& error);
    bool write(const AnalysisTimelineEntry& entry, std::string& error);
    void close();

    [[nodiscard]] bool isOpen() const noexcept { return output_.is_open(); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
    std::ofstream output_;
};

} // namespace viz
