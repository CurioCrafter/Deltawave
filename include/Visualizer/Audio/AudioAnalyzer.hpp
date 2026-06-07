#pragma once

#include "Visualizer/Audio/AdaptiveStyleModel.hpp"
#include "Visualizer/Audio/AudioSyncProfile.hpp"

#include <array>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <string_view>
#include <string>
#include <vector>

namespace viz {

constexpr std::size_t kSpectrumBinCount = 64;
constexpr std::size_t kAudioBandCount = 5;
constexpr std::size_t kChromaBinCount = 12;

enum class AudioStyle {
    Silence,
    Ambient,
    Techno,
    BassHeavy,
    Bright,
    Wide
};

enum class MusicalMode {
    Unknown,
    Major,
    Minor
};

enum class ArrangementSection {
    Silence,
    Breakdown,
    Build,
    Drop,
    Groove
};

struct AudioMetrics {
    float rms = 0.0f;
    float peak = 0.0f;
    float bass = 0.0f;
    float lowMid = 0.0f;
    float mid = 0.0f;
    float highMid = 0.0f;
    float treble = 0.0f;
    float spectralCentroid = 0.0f;
    float spectralFlux = 0.0f;
    float stereoWidth = 0.0f;
    float onset = 0.0f;
    float beatConfidence = 0.0f;
    float beatPhase = 0.0f;
    float barPhase = 0.0f;
    float barConfidence = 0.0f;
    float downbeatConfidence = 0.0f;
    float dropIntensity = 0.0f;
    float phraseIntensity = 0.0f;
    float phrasePhase = 0.0f;
    float phraseConfidence = 0.0f;
    float buildTension = 0.0f;
    float bpm = 0.0f;
    float styleConfidence = 0.0f;
    float keyConfidence = 0.0f;
    float harmonicEnergy = 0.0f;
    float sectionConfidence = 0.0f;
    float sectionProgress = 0.0f;
    float styleAdaptation = 0.0f;
    float syncAdaptation = 0.0f;
    float beatSensitivity = 1.0f;
    float sectionSensitivity = 1.0f;
    int keyIndex = -1;
    bool beat = false;
    bool downbeat = false;
    bool phraseBoundary = false;
    double timeSeconds = 0.0;
    AudioStyle style = AudioStyle::Silence;
    MusicalMode keyMode = MusicalMode::Unknown;
    ArrangementSection section = ArrangementSection::Silence;
    std::array<float, kSpectrumBinCount> spectrum{};
    std::array<float, kAudioBandCount> bandOnsets{};
    std::array<float, kChromaBinCount> chroma{};
};

std::string_view toString(AudioStyle style);
std::string_view toString(MusicalMode mode);
std::string_view toString(ArrangementSection section);
std::string_view keyName(int keyIndex);

class AudioAnalyzer {
public:
    AudioAnalyzer(int sampleRate = 48000, int channelCount = 2);

    void configure(int sampleRate, int channelCount);
    void reset();
    void resetStyleProfile();
    void resetSyncProfile();
    bool saveStyleProfile(const std::filesystem::path& path, std::string& error) const;
    bool loadStyleProfile(const std::filesystem::path& path, std::string& error);
    bool saveSyncProfile(const std::filesystem::path& path, std::string& error) const;
    bool loadSyncProfile(const std::filesystem::path& path, std::string& error);
    [[nodiscard]] float learnedStyleWeight(AudioStyle style) const;
    [[nodiscard]] float syncProfileLearnedWeight() const;
    [[nodiscard]] float beatSensitivity() const;
    [[nodiscard]] float sectionSensitivity() const;

    [[nodiscard]] int sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] int channelCount() const noexcept { return channelCount_; }
    [[nodiscard]] const AudioMetrics& lastMetrics() const noexcept { return last_; }

    AudioMetrics analyzeInterleaved(const float* interleavedSamples,
                                    std::size_t frameCount,
                                    double timeSeconds);

private:
    int sampleRate_ = 48000;
    int channelCount_ = 2;
    AudioMetrics last_{};
    std::vector<float> monoScratch_;
    std::deque<float> lowEnergyHistory_;
    std::deque<float> shortEnergyHistory_;
    std::deque<float> longEnergyHistory_;
    std::deque<double> beatTimes_;
    double lastBeatTime_ = -10.0;
    double beatIntervalSeconds_ = 0.0;
    double sectionStartSeconds_ = 0.0;
    double lastSectionSwitchSeconds_ = -10.0;
    double lastDownbeatTime_ = -10.0;
    double lastPhraseBoundaryTime_ = -10.0;
    float previousLowEnergy_ = 0.0f;
    int beatCountInBar_ = 0;
    int barsSincePhraseBoundary_ = 0;
    std::array<float, kAudioBandCount> previousBandEnergies_{};
    ArrangementSection currentSection_ = ArrangementSection::Silence;
    AdaptiveStyleModel styleModel_;
    AudioSyncProfile syncProfile_;

    void updateBeatState(AudioMetrics& metrics, float lowEnergy, double timeSeconds);
    void updateBpm(AudioMetrics& metrics, double timeSeconds);
    void updateChromaMetrics(AudioMetrics& metrics,
                             const std::array<float, kSpectrumBinCount>& frequencies);
    void updateAdvancedSyncMetrics(AudioMetrics& metrics);
    void updateArrangementSection(AudioMetrics& metrics,
                                  float longAverage,
                                  float shortAverage,
                                  float lowBandHit);
    void updateBarState(AudioMetrics& metrics);
    [[nodiscard]] AudioStyle heuristicStyle(const AudioMetrics& metrics) const;
};

} // namespace viz
