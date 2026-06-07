#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace viz {

struct WavAudio {
    int sampleRate = 0;
    int channelCount = 0;
    int bitsPerSample = 0;
    double durationSeconds = 0.0;
    std::filesystem::path sourcePath;
    std::vector<float> samples;
};

[[nodiscard]] std::optional<WavAudio> loadWavFile(const std::filesystem::path& path,
                                                  std::string& error);

} // namespace viz
