#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace viz {

struct AudioMetrics;

class AudioSyncProfile {
public:
    [[nodiscard]] float beatSensitivity() const noexcept { return beatSensitivity_; }
    [[nodiscard]] float sectionSensitivity() const noexcept { return sectionSensitivity_; }
    [[nodiscard]] float learnedWeight() const noexcept { return learnedWeight_; }

    [[nodiscard]] float beatThresholdScale() const noexcept;
    [[nodiscard]] float beatConfidenceGain() const noexcept;
    [[nodiscard]] float sectionThresholdScale() const noexcept;
    [[nodiscard]] float sectionConfidenceGain() const noexcept;

    void reset();
    void learnBeat(const AudioMetrics& metrics, float lowEnergy, float threshold);
    void learnSection(const AudioMetrics& metrics);

    bool saveProfile(const std::filesystem::path& path, std::string& error) const;
    bool loadProfile(const std::filesystem::path& path, std::string& error);

private:
    float beatSensitivity_ = 1.0f;
    float sectionSensitivity_ = 1.0f;
    float learnedWeight_ = 0.0f;
};

} // namespace viz
