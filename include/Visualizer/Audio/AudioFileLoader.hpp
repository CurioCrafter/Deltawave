#pragma once

#include "Visualizer/Audio/WavFile.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace viz {

struct AudioLoadOptions {
    bool allowPlatformDecoders = true;
};

[[nodiscard]] bool isLikelyWavFile(const std::filesystem::path& path);

[[nodiscard]] std::optional<WavAudio> loadAudioFile(const std::filesystem::path& path,
                                                    std::string& error,
                                                    AudioLoadOptions options = {});

} // namespace viz
