#include "Visualizer/Audio/AudioFileLoader.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace viz {

#if defined(_WIN32)
std::optional<WavAudio> loadAudioFileWithMediaFoundation(const std::filesystem::path& path,
                                                         std::string& error);
#endif

namespace {

std::string lowerExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

} // namespace

bool isLikelyWavFile(const std::filesystem::path& path)
{
    const std::string extension = lowerExtension(path);
    return extension == ".wav" || extension == ".wave";
}

std::optional<WavAudio> loadAudioFile(const std::filesystem::path& path,
                                      std::string& error,
                                      AudioLoadOptions options)
{
    error.clear();

    if (path.empty()) {
        error = "Audio path is required.";
        return std::nullopt;
    }

    if (isLikelyWavFile(path)) {
        std::optional<WavAudio> wav = loadWavFile(path, error);
        if (wav || !options.allowPlatformDecoders) {
            return wav;
        }
    }

#if defined(_WIN32)
    if (options.allowPlatformDecoders) {
        std::string platformError;
        std::optional<WavAudio> decoded = loadAudioFileWithMediaFoundation(path, platformError);
        if (decoded) {
            error.clear();
            return decoded;
        }

        if (error.empty()) {
            error = platformError.empty() ? "Platform audio decoder failed." : platformError;
        } else if (!platformError.empty()) {
            error += " Platform decoder: " + platformError;
        }
        return std::nullopt;
    }
#else
    (void)options;
#endif

    if (error.empty()) {
        error = "Unsupported audio format on this platform. Use PCM or IEEE-float WAV.";
    }
    return std::nullopt;
}

} // namespace viz
