#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

namespace viz {

constexpr std::size_t kAudioFeatureCount = 10;

enum class AudioStyle;
struct AudioMetrics;

struct AudioFeatureVector {
    std::array<float, kAudioFeatureCount> values{};
};

struct StylePrediction {
    AudioStyle style{};
    float confidence = 0.0f;
};

class AdaptiveStyleModel {
public:
    AdaptiveStyleModel();

    [[nodiscard]] StylePrediction predict(const AudioMetrics& metrics) const;
    void learn(const AudioMetrics& metrics, AudioStyle style, float confidence);
    void resetAdaptation();
    [[nodiscard]] float learnedWeight(AudioStyle style) const;
    bool saveProfile(const std::filesystem::path& path, std::string& error) const;
    bool loadProfile(const std::filesystem::path& path, std::string& error);

private:
    struct Centroid {
        AudioStyle style{};
        AudioFeatureVector features{};
        float learnedWeight = 0.0f;
    };

    std::array<Centroid, 6> centroids_{};

    [[nodiscard]] static AudioFeatureVector featuresFrom(const AudioMetrics& metrics);
    [[nodiscard]] static float distance(const AudioFeatureVector& a, const AudioFeatureVector& b);
};

} // namespace viz
