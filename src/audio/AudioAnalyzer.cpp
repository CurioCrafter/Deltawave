#include "Visualizer/Audio/AudioAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace viz {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::array<float, kChromaBinCount> kMajorProfile{
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};
constexpr std::array<float, kChromaBinCount> kMinorProfile{
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float mean(const std::deque<float>& values)
{
    if (values.empty()) {
        return 0.0f;
    }
    return std::accumulate(values.begin(), values.end(), 0.0f) /
           static_cast<float>(values.size());
}

float standardDeviation(const std::deque<float>& values, float average)
{
    if (values.size() < 2) {
        return 0.0f;
    }

    float variance = 0.0f;
    for (float value : values) {
        const float delta = value - average;
        variance += delta * delta;
    }
    return std::sqrt(variance / static_cast<float>(values.size() - 1));
}

float bandAverage(const std::array<float, kSpectrumBinCount>& spectrum,
                  const std::array<float, kSpectrumBinCount>& frequencies,
                  float low,
                  float high)
{
    float sum = 0.0f;
    float count = 0.0f;
    for (std::size_t i = 0; i < spectrum.size(); ++i) {
        if (frequencies[i] >= low && frequencies[i] < high) {
            sum += spectrum[i];
            count += 1.0f;
        }
    }
    return count > 0.0f ? sum / count : 0.0f;
}

float pitchClassCorrelation(const std::array<float, kChromaBinCount>& chroma,
                            const std::array<float, kChromaBinCount>& profile,
                            int root)
{
    float score = 0.0f;
    for (std::size_t i = 0; i < kChromaBinCount; ++i) {
        const std::size_t rotated = (i + static_cast<std::size_t>(root)) % kChromaBinCount;
        score += chroma[rotated] * profile[i];
    }
    return score;
}

struct SectionCandidate {
    ArrangementSection section = ArrangementSection::Silence;
    float confidence = 0.0f;
};

float rise(float value, float low, float high)
{
    if (std::abs(high - low) <= 0.0001f) {
        return value >= high ? 1.0f : 0.0f;
    }
    return clamp01((value - low) / (high - low));
}

float rangePeak(float value, float low, float high)
{
    if (value <= 0.0f || high <= low) {
        return 0.0f;
    }
    if (value >= low && value <= high) {
        return 1.0f;
    }
    const float falloff = (high - low) * 0.55f;
    if (value < low) {
        return rise(value, low - falloff, low);
    }
    return 1.0f - rise(value, high, high + falloff);
}

float danceTempoScore(float bpm)
{
    const float direct = rangePeak(bpm, 112.0f, 156.0f);
    const float halfTime = rangePeak(bpm * 0.5f, 112.0f, 156.0f);
    const float doubleTime = rangePeak(bpm * 2.0f, 112.0f, 156.0f);
    return std::max({direct, halfTime * 0.88f, doubleTime * 0.78f});
}

float maxBandOnset(const AudioMetrics& metrics)
{
    return *std::max_element(metrics.bandOnsets.begin(), metrics.bandOnsets.end());
}

float averageChroma(const AudioMetrics& metrics)
{
    return std::accumulate(metrics.chroma.begin(), metrics.chroma.end(), 0.0f) /
           static_cast<float>(metrics.chroma.size());
}

float chromaFocus(const AudioMetrics& metrics)
{
    float best = 0.0f;
    float second = 0.0f;
    for (float value : metrics.chroma) {
        if (value > best) {
            second = best;
            best = value;
        } else if (value > second) {
            second = value;
        }
    }

    if (best <= 0.0001f) {
        return 0.0f;
    }

    const float average = averageChroma(metrics);
    const float peakFocus = (best - second) / best;
    return clamp01(peakFocus * 0.74f + rise(best, average * 1.25f, average * 2.85f) * 0.26f);
}

bool isSceneStyle(AudioStyle style)
{
    return style == AudioStyle::Techno ||
           style == AudioStyle::BassHeavy ||
           style == AudioStyle::Bright ||
           style == AudioStyle::Wide;
}

float styleEvidence(const AudioMetrics& metrics, AudioStyle style)
{
    if (metrics.rms < 0.006f && metrics.peak < 0.02f) {
        return style == AudioStyle::Silence ? 1.0f : 0.0f;
    }

    const float highEnergy = clamp01(metrics.highMid * 0.82f + metrics.treble * 1.05f);
    const float highBandHit = std::max(metrics.bandOnsets[3], metrics.bandOnsets[4]);
    const float lowEnergy = clamp01(metrics.bass * 0.72f + metrics.lowMid * 0.42f);
    const float transient = clamp01(metrics.spectralFlux * 0.48f +
                                    metrics.onset * 1.25f +
                                    maxBandOnset(metrics) * 0.36f);
    const float rhythmic = clamp01(metrics.beatConfidence * 0.62f +
                                   metrics.downbeatConfidence * 0.18f +
                                   transient * 0.24f);
    const float tempo = danceTempoScore(metrics.bpm);
    const float bassPressure = rise(metrics.bass, 0.44f, 0.82f);
    const float bassDominance = rise(metrics.bass - highEnergy * 0.56f, 0.03f, 0.30f);
    const float brightDominance = rise(highEnergy +
                                           metrics.spectralFlux * 0.28f -
                                           metrics.bass * 0.42f -
                                           metrics.lowMid * 0.18f,
                                       0.02f,
                                       0.28f);
    const float dropPressure = metrics.dropIntensity * bassPressure;
    const float quietMusical = rise(metrics.rms, 0.012f, 0.08f) *
                               (1.0f - clamp01(metrics.beatConfidence * 0.72f + metrics.dropIntensity * 0.52f));

    switch (style) {
    case AudioStyle::Silence:
        return metrics.rms < 0.014f && metrics.peak < 0.035f
                   ? clamp01(1.0f - metrics.rms * 34.0f - metrics.peak * 9.0f)
                   : 0.0f;
    case AudioStyle::Techno:
        return clamp01(tempo * 0.42f +
                       rhythmic * 0.40f +
                       lowEnergy * 0.22f +
                       metrics.lowMid * 0.12f +
                       metrics.bandOnsets[0] * 0.10f -
                       brightDominance * 0.14f -
                       metrics.stereoWidth * 0.06f);
    case AudioStyle::BassHeavy:
        return clamp01(metrics.bass * 0.22f +
                       lowEnergy * 0.16f +
                       bassDominance * 0.18f +
                       bassPressure * 0.34f +
                       dropPressure * 0.24f +
                       metrics.bandOnsets[0] * bassPressure * 0.10f +
                       rhythmic * 0.05f -
                       brightDominance * 0.18f -
                       tempo * (1.0f - bassPressure) * 0.18f);
    case AudioStyle::Bright:
        return clamp01(highEnergy * 0.62f +
                       rise(highEnergy, 0.10f, 0.28f) * 0.24f +
                       brightDominance * 0.42f +
                       transient * 0.34f +
                       metrics.spectralCentroid * 0.22f +
                       highBandHit * 0.18f -
                       bassDominance * 0.18f);
    case AudioStyle::Wide:
        return clamp01(metrics.stereoWidth * 0.62f +
                       metrics.phraseIntensity * 0.16f +
                       metrics.harmonicEnergy * 0.12f +
                       highEnergy * 0.08f -
                       metrics.dropIntensity * 0.18f -
                       bassDominance * 0.10f);
    case AudioStyle::Ambient:
        return clamp01(quietMusical * 0.38f +
                       (1.0f - rhythmic) * 0.24f +
                       (1.0f - transient) * 0.18f +
                       metrics.harmonicEnergy * 0.14f +
                       metrics.stereoWidth * 0.10f +
                       rise(metrics.rms, 0.02f, 0.22f) * 0.10f -
                       metrics.dropIntensity * 0.18f);
    }
    return 0.0f;
}

} // namespace

std::string_view toString(AudioStyle style)
{
    switch (style) {
    case AudioStyle::Silence:
        return "Silence";
    case AudioStyle::Ambient:
        return "Ambient";
    case AudioStyle::Techno:
        return "Techno";
    case AudioStyle::BassHeavy:
        return "Bass Heavy";
    case AudioStyle::Bright:
        return "Bright";
    case AudioStyle::Wide:
        return "Wide";
    }
    return "Unknown";
}

std::string_view toString(MusicalMode mode)
{
    switch (mode) {
    case MusicalMode::Unknown:
        return "Unknown";
    case MusicalMode::Major:
        return "Major";
    case MusicalMode::Minor:
        return "Minor";
    }
    return "Unknown";
}

std::string_view toString(ArrangementSection section)
{
    switch (section) {
    case ArrangementSection::Silence:
        return "Silence";
    case ArrangementSection::Breakdown:
        return "Breakdown";
    case ArrangementSection::Build:
        return "Build";
    case ArrangementSection::Drop:
        return "Drop";
    case ArrangementSection::Groove:
        return "Groove";
    }
    return "Unknown";
}

std::string_view keyName(int keyIndex)
{
    if (keyIndex < 0) {
        return "-";
    }
    switch ((keyIndex % 12 + 12) % 12) {
    case 0:
        return "C";
    case 1:
        return "C#";
    case 2:
        return "D";
    case 3:
        return "D#";
    case 4:
        return "E";
    case 5:
        return "F";
    case 6:
        return "F#";
    case 7:
        return "G";
    case 8:
        return "G#";
    case 9:
        return "A";
    case 10:
        return "A#";
    case 11:
        return "B";
    default:
        return "-";
    }
}

AudioAnalyzer::AudioAnalyzer(int sampleRate, int channelCount)
{
    configure(sampleRate, channelCount);
}

void AudioAnalyzer::configure(int sampleRate, int channelCount)
{
    sampleRate_ = std::max(8000, sampleRate);
    channelCount_ = std::max(1, channelCount);
    reset();
}

void AudioAnalyzer::reset()
{
    last_ = {};
    lowEnergyHistory_.clear();
    shortEnergyHistory_.clear();
    longEnergyHistory_.clear();
    beatTimes_.clear();
    lastBeatTime_ = -10.0;
    beatIntervalSeconds_ = 0.0;
    sectionStartSeconds_ = 0.0;
    lastSectionSwitchSeconds_ = -10.0;
    lastDownbeatTime_ = -10.0;
    lastPhraseBoundaryTime_ = -10.0;
    previousLowEnergy_ = 0.0f;
    beatCountInBar_ = 0;
    barsSincePhraseBoundary_ = 0;
    previousBandEnergies_ = {};
    currentSection_ = ArrangementSection::Silence;
}

void AudioAnalyzer::resetStyleProfile()
{
    styleModel_.resetAdaptation();
}

void AudioAnalyzer::resetSyncProfile()
{
    syncProfile_.reset();
}

bool AudioAnalyzer::saveStyleProfile(const std::filesystem::path& path, std::string& error) const
{
    return styleModel_.saveProfile(path, error);
}

bool AudioAnalyzer::loadStyleProfile(const std::filesystem::path& path, std::string& error)
{
    return styleModel_.loadProfile(path, error);
}

bool AudioAnalyzer::saveSyncProfile(const std::filesystem::path& path, std::string& error) const
{
    return syncProfile_.saveProfile(path, error);
}

bool AudioAnalyzer::loadSyncProfile(const std::filesystem::path& path, std::string& error)
{
    return syncProfile_.loadProfile(path, error);
}

float AudioAnalyzer::learnedStyleWeight(AudioStyle style) const
{
    return styleModel_.learnedWeight(style);
}

float AudioAnalyzer::syncProfileLearnedWeight() const
{
    return syncProfile_.learnedWeight();
}

float AudioAnalyzer::beatSensitivity() const
{
    return syncProfile_.beatSensitivity();
}

float AudioAnalyzer::sectionSensitivity() const
{
    return syncProfile_.sectionSensitivity();
}

AudioMetrics AudioAnalyzer::analyzeInterleaved(const float* interleavedSamples,
                                               std::size_t frameCount,
                                               double timeSeconds)
{
    AudioMetrics metrics{};
    metrics.timeSeconds = timeSeconds;

    if (interleavedSamples == nullptr || frameCount == 0) {
        last_ = metrics;
        return last_;
    }

    double squareSum = 0.0;
    float peak = 0.0f;
    double stereoDelta = 0.0;
    double stereoReference = 0.0;

    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        for (int channel = 0; channel < channelCount_; ++channel) {
            const float sample = interleavedSamples[(frame * static_cast<std::size_t>(channelCount_)) +
                                                    static_cast<std::size_t>(channel)];
            peak = std::max(peak, std::abs(sample));
            squareSum += static_cast<double>(sample) * static_cast<double>(sample);
        }

        if (channelCount_ >= 2) {
            const float left = interleavedSamples[frame * static_cast<std::size_t>(channelCount_)];
            const float right = interleavedSamples[(frame * static_cast<std::size_t>(channelCount_)) + 1U];
            stereoDelta += std::abs(left - right);
            stereoReference += std::abs(left) + std::abs(right);
        }
    }

    metrics.rms = static_cast<float>(
        std::sqrt(squareSum / static_cast<double>(frameCount * static_cast<std::size_t>(channelCount_))));
    metrics.peak = clamp01(peak);
    metrics.stereoWidth = stereoReference > 0.00001
                              ? clamp01(static_cast<float>(stereoDelta / stereoReference))
                              : 0.0f;

    constexpr std::size_t windowSize = 1024;
    const std::size_t analysisFrames = std::min(frameCount, windowSize);
    const std::size_t startFrame = frameCount - analysisFrames;
    monoScratch_.assign(windowSize, 0.0f);

    for (std::size_t i = 0; i < analysisFrames; ++i) {
        float mono = 0.0f;
        for (int channel = 0; channel < channelCount_; ++channel) {
            mono += interleavedSamples[((startFrame + i) * static_cast<std::size_t>(channelCount_)) +
                                       static_cast<std::size_t>(channel)];
        }
        monoScratch_[i] = mono / static_cast<float>(channelCount_);
    }

    std::array<float, kSpectrumBinCount> frequencies{};
    const float lowFrequency = 30.0f;
    const float highFrequency = std::min(18000.0f, static_cast<float>(sampleRate_) * 0.48f);
    const float logLow = std::log(lowFrequency);
    const float logHigh = std::log(highFrequency);
    double weightedFrequency = 0.0;
    double weightedMagnitude = 0.0;

    for (std::size_t bin = 0; bin < kSpectrumBinCount; ++bin) {
        const float t = static_cast<float>(bin) / static_cast<float>(kSpectrumBinCount - 1);
        const float frequency = std::exp(logLow + (logHigh - logLow) * t);
        frequencies[bin] = frequency;

        const float omega = 2.0f * kPi * frequency / static_cast<float>(sampleRate_);
        double real = 0.0;
        double imaginary = 0.0;
        for (std::size_t i = 0; i < windowSize; ++i) {
            const float window = 0.5f - 0.5f * std::cos((2.0f * kPi * static_cast<float>(i)) /
                                                        static_cast<float>(windowSize - 1));
            const float sample = monoScratch_[i] * window;
            const float phase = omega * static_cast<float>(i);
            real += sample * std::cos(phase);
            imaginary -= sample * std::sin(phase);
        }

        const float magnitude = static_cast<float>(
            std::sqrt((real * real) + (imaginary * imaginary)) / static_cast<double>(windowSize));
        const float normalized = clamp01(std::sqrt(magnitude) * 4.5f);
        metrics.spectrum[bin] = (last_.spectrum[bin] * 0.58f) + (normalized * 0.42f);
        metrics.spectralFlux += std::max(0.0f, metrics.spectrum[bin] - last_.spectrum[bin]);
        weightedFrequency += static_cast<double>(frequency) * normalized;
        weightedMagnitude += normalized;
    }
    metrics.spectralFlux = clamp01((metrics.spectralFlux / static_cast<float>(kSpectrumBinCount)) * 4.0f);

    metrics.bass = bandAverage(metrics.spectrum, frequencies, 30.0f, 160.0f);
    metrics.lowMid = bandAverage(metrics.spectrum, frequencies, 160.0f, 500.0f);
    metrics.mid = bandAverage(metrics.spectrum, frequencies, 500.0f, 2000.0f);
    metrics.highMid = bandAverage(metrics.spectrum, frequencies, 2000.0f, 6000.0f);
    metrics.treble = bandAverage(metrics.spectrum, frequencies, 6000.0f, highFrequency + 1.0f);
    metrics.spectralCentroid = weightedMagnitude > 0.00001
                                   ? clamp01(static_cast<float>((weightedFrequency / weightedMagnitude) /
                                                                static_cast<double>(highFrequency)))
                                   : 0.0f;
    updateChromaMetrics(metrics, frequencies);

    const float lowEnergy = clamp01((metrics.bass * 0.72f) + (metrics.lowMid * 0.28f) + (metrics.rms * 0.35f));
    metrics.onset = std::max(0.0f, lowEnergy - previousLowEnergy_);
    previousLowEnergy_ = (previousLowEnergy_ * 0.82f) + (lowEnergy * 0.18f);

    updateBeatState(metrics, lowEnergy, timeSeconds);
    updateAdvancedSyncMetrics(metrics);
    StylePrediction prediction = styleModel_.predict(metrics);
    const AudioStyle heuristic = heuristicStyle(metrics);
    const float heuristicConfidence = styleEvidence(metrics, heuristic);
    const bool strongHeuristic = heuristicConfidence >= 0.58f &&
                                 (prediction.confidence < 0.58f ||
                                  heuristicConfidence > prediction.confidence + 0.06f);
    const bool nonAmbientOverride = prediction.style == AudioStyle::Ambient &&
                                    heuristic != AudioStyle::Ambient &&
                                    heuristicConfidence >= 0.46f &&
                                    (metrics.beatConfidence > 0.18f ||
                                     metrics.dropIntensity > 0.18f ||
                                     metrics.spectralFlux > 0.12f ||
                                     metrics.bass > 0.20f ||
                                     metrics.highMid + metrics.treble > 0.32f);
    if ((prediction.confidence < 0.34f && heuristic != AudioStyle::Ambient) ||
        strongHeuristic ||
        nonAmbientOverride) {
        prediction.style = heuristic;
        prediction.confidence = std::max({prediction.confidence, heuristicConfidence, 0.42f});
    } else if (heuristic == prediction.style) {
        prediction.confidence = std::max(prediction.confidence, heuristicConfidence);
    }

    const bool activeMusicalFrame = metrics.rms > 0.035f ||
                                    metrics.peak > 0.12f ||
                                    metrics.spectralFlux > 0.045f ||
                                    metrics.beatConfidence > 0.06f ||
                                    metrics.dropIntensity > 0.08f;
    const bool weakCalmPrediction = prediction.style == AudioStyle::Ambient ||
                                    (prediction.style == AudioStyle::Silence && prediction.confidence < 0.78f);
    if (activeMusicalFrame &&
        weakCalmPrediction &&
        isSceneStyle(last_.style) &&
        last_.styleConfidence > 0.40f &&
        !(metrics.rms < 0.006f && metrics.peak < 0.02f)) {
        prediction.style = last_.style;
        prediction.confidence = std::max({prediction.confidence * 0.72f,
                                          last_.styleConfidence * 0.84f,
                                          0.44f});
    }

    metrics.style = prediction.style;
    metrics.styleConfidence = prediction.confidence;
    styleModel_.learn(metrics, metrics.style, metrics.styleConfidence);
    metrics.styleAdaptation = styleModel_.learnedWeight(metrics.style);
    metrics.syncAdaptation = syncProfile_.learnedWeight();
    metrics.beatSensitivity = syncProfile_.beatSensitivity();
    metrics.sectionSensitivity = syncProfile_.sectionSensitivity();
    updateMusicalRoleMetrics(metrics);
    last_ = metrics;
    return last_;
}

void AudioAnalyzer::updateBeatState(AudioMetrics& metrics, float lowEnergy, double timeSeconds)
{
    const float historyMean = mean(lowEnergyHistory_);
    const float historyStdDev = standardDeviation(lowEnergyHistory_, historyMean);
    const float thresholdScale = syncProfile_.beatThresholdScale();
    const float threshold = std::max(0.055f * thresholdScale,
                                     historyMean + (historyStdDev * 1.15f * thresholdScale) +
                                         (0.025f * thresholdScale));
    const double secondsSinceBeat = timeSeconds - lastBeatTime_;

    metrics.beatConfidence = clamp01((lowEnergy - threshold) * 5.0f * syncProfile_.beatConfidenceGain());
    metrics.beat = lowEnergy > threshold &&
                   metrics.onset > 0.015f * thresholdScale &&
                   metrics.rms > 0.008f &&
                   secondsSinceBeat > 0.18;

    if (metrics.beat) {
        updateBpm(metrics, timeSeconds);
        lastBeatTime_ = timeSeconds;
    } else if (!beatTimes_.empty() && timeSeconds - beatTimes_.back() < 4.0) {
        metrics.bpm = last_.bpm;
    }

    lowEnergyHistory_.push_back(lowEnergy);
    while (lowEnergyHistory_.size() > 90) {
        lowEnergyHistory_.pop_front();
    }

    if (metrics.beat) {
        metrics.beatPhase = 0.0f;
    } else if (beatIntervalSeconds_ > 0.001 && lastBeatTime_ > 0.0) {
        metrics.beatPhase = clamp01(static_cast<float>((timeSeconds - lastBeatTime_) / beatIntervalSeconds_));
    }
    syncProfile_.learnBeat(metrics, lowEnergy, threshold);
}

void AudioAnalyzer::updateBpm(AudioMetrics& metrics, double timeSeconds)
{
    if (!beatTimes_.empty()) {
        const double interval = timeSeconds - beatTimes_.back();
        if (interval >= 0.25 && interval <= 1.5) {
            beatTimes_.push_back(timeSeconds);
        } else if (interval > 1.5) {
            beatTimes_.clear();
            beatTimes_.push_back(timeSeconds);
        }
    } else {
        beatTimes_.push_back(timeSeconds);
    }

    while (beatTimes_.size() > 10) {
        beatTimes_.pop_front();
    }

    if (beatTimes_.size() < 3) {
        metrics.bpm = last_.bpm;
        return;
    }

    std::vector<double> intervals;
    intervals.reserve(beatTimes_.size() - 1);
    for (std::size_t i = 1; i < beatTimes_.size(); ++i) {
        intervals.push_back(beatTimes_[i] - beatTimes_[i - 1]);
    }

    std::sort(intervals.begin(), intervals.end());
    const double medianInterval = intervals[intervals.size() / 2];
    const float bpm = static_cast<float>(60.0 / medianInterval);
    beatIntervalSeconds_ = medianInterval;
    metrics.bpm = (bpm >= 40.0f && bpm <= 240.0f) ? bpm : last_.bpm;
}

void AudioAnalyzer::updateChromaMetrics(AudioMetrics& metrics,
                                        const std::array<float, kSpectrumBinCount>& frequencies)
{
    float tonalSum = 0.0f;
    float chromaPeak = 0.0f;
    for (std::size_t bin = 0; bin < kSpectrumBinCount; ++bin) {
        const float frequency = frequencies[bin];
        if (frequency < 55.0f || frequency > 5200.0f) {
            continue;
        }

        const float magnitude = metrics.spectrum[bin];
        if (magnitude <= 0.0001f) {
            continue;
        }

        const float midi = 69.0f + 12.0f * (std::log(frequency / 440.0f) / std::log(2.0f));
        const int pitchClass = ((static_cast<int>(std::lround(midi)) % 12) + 12) % 12;
        const float tuningOffset = std::abs(midi - std::round(midi));
        const float tuningWeight = std::clamp(1.0f - tuningOffset * 0.45f, 0.55f, 1.0f);
        const float weight = std::sqrt(magnitude) * tuningWeight;
        metrics.chroma[static_cast<std::size_t>(pitchClass)] += weight;
        tonalSum += weight;
    }

    if (tonalSum <= 0.0001f) {
        metrics.keyIndex = -1;
        metrics.keyMode = MusicalMode::Unknown;
        metrics.keyConfidence = 0.0f;
        metrics.harmonicEnergy = 0.0f;
        return;
    }

    for (std::size_t i = 0; i < metrics.chroma.size(); ++i) {
        metrics.chroma[i] = (last_.chroma[i] * 0.5f) + ((metrics.chroma[i] / tonalSum) * 0.5f);
        chromaPeak = std::max(chromaPeak, metrics.chroma[i]);
    }
    metrics.harmonicEnergy = clamp01(chromaPeak * 4.0f);

    float bestScore = -1.0f;
    float secondScore = -1.0f;
    int bestRoot = -1;
    MusicalMode bestMode = MusicalMode::Unknown;
    for (int root = 0; root < 12; ++root) {
        const std::size_t tonic = static_cast<std::size_t>(root);
        const float majorTriad = metrics.chroma[tonic] * 3.2f +
                                 metrics.chroma[(tonic + 4U) % kChromaBinCount] * 1.8f +
                                 metrics.chroma[(tonic + 7U) % kChromaBinCount] * 2.1f;
        const float minorTriad = metrics.chroma[tonic] * 3.2f +
                                 metrics.chroma[(tonic + 3U) % kChromaBinCount] * 1.8f +
                                 metrics.chroma[(tonic + 7U) % kChromaBinCount] * 2.1f;
        const float majorScore = pitchClassCorrelation(metrics.chroma, kMajorProfile, root) + majorTriad;
        const float minorScore = pitchClassCorrelation(metrics.chroma, kMinorProfile, root) + minorTriad;
        if (majorScore > bestScore) {
            secondScore = bestScore;
            bestScore = majorScore;
            bestRoot = root;
            bestMode = MusicalMode::Major;
        } else {
            secondScore = std::max(secondScore, majorScore);
        }
        if (minorScore > bestScore) {
            secondScore = bestScore;
            bestScore = minorScore;
            bestRoot = root;
            bestMode = MusicalMode::Minor;
        } else {
            secondScore = std::max(secondScore, minorScore);
        }
    }

    const float separation = bestScore > 0.0001f ? (bestScore - std::max(0.0f, secondScore)) / bestScore : 0.0f;
    metrics.keyIndex = bestRoot;
    metrics.keyMode = bestMode;
    metrics.keyConfidence = clamp01((separation * 5.5f + metrics.harmonicEnergy * 0.45f) *
                                    std::clamp(metrics.rms * 3.0f, 0.35f, 1.0f));
    if (metrics.keyConfidence < 0.08f) {
        metrics.keyIndex = -1;
        metrics.keyMode = MusicalMode::Unknown;
    }
}

void AudioAnalyzer::updateAdvancedSyncMetrics(AudioMetrics& metrics)
{
    const std::array<float, kAudioBandCount> bands{
        metrics.bass,
        metrics.lowMid,
        metrics.mid,
        metrics.highMid,
        metrics.treble
    };

    for (std::size_t i = 0; i < bands.size(); ++i) {
        const float delta = std::max(0.0f, bands[i] - previousBandEnergies_[i]);
        metrics.bandOnsets[i] = clamp01(delta * 3.4f + metrics.spectralFlux * 0.18f);
        previousBandEnergies_[i] = (previousBandEnergies_[i] * 0.72f) + (bands[i] * 0.28f);
    }

    const float longAverage = mean(longEnergyHistory_);
    const float shortAverage = mean(shortEnergyHistory_);
    const float lowBandHit = clamp01(metrics.bandOnsets[0] * 0.6f + metrics.bandOnsets[1] * 0.4f);
    const bool enoughHistory = longEnergyHistory_.size() >= 24;
    if (enoughHistory && longAverage > 0.004f) {
        const float shortLift = std::max(0.0f, shortAverage - longAverage);
        const float sustainedFloor = std::max(longAverage, shortAverage * 0.82f);
        const float punchLift = std::max(0.0f, metrics.rms - sustainedFloor);
        const float pressureLift = std::max(0.0f, metrics.rms - longAverage);
        const float pressureWeight = rise(metrics.bass, 0.42f, 0.82f);
        metrics.dropIntensity = clamp01(punchLift * 4.8f +
                                        pressureLift * pressureWeight * 2.2f +
                                        lowBandHit * (0.24f + pressureWeight * 0.28f) +
                                        metrics.beatConfidence * 0.18f);
        metrics.phraseIntensity = clamp01(shortLift * 3.8f + metrics.spectralFlux * 0.62f + metrics.beatConfidence * 0.16f);
    } else {
        metrics.dropIntensity = clamp01(lowBandHit * 0.35f + metrics.beatConfidence * 0.18f);
        metrics.phraseIntensity = clamp01(metrics.spectralFlux * 0.45f);
    }

    updateArrangementSection(metrics, longAverage, shortAverage, lowBandHit);
    updateBarState(metrics);

    shortEnergyHistory_.push_back(metrics.rms);
    longEnergyHistory_.push_back(metrics.rms);
    while (shortEnergyHistory_.size() > 18) {
        shortEnergyHistory_.pop_front();
    }
    while (longEnergyHistory_.size() > 220) {
        longEnergyHistory_.pop_front();
    }
}

void AudioAnalyzer::updateMusicalRoleMetrics(AudioMetrics& metrics)
{
    if (metrics.rms < 0.006f && metrics.peak < 0.02f) {
        metrics.bassRole = 0.0f;
        metrics.drumRole = 0.0f;
        metrics.melodyRole = 0.0f;
        metrics.harmonyRole = 0.0f;
        metrics.spaceRole = 0.0f;
        metrics.fractureRole = 0.0f;
        metrics.shadowRole = 0.0f;
        metrics.convergenceRole = 0.0f;
        metrics.roleSeparation = 0.0f;
        return;
    }

    const float audible = clamp01(rise(metrics.rms, 0.010f, 0.18f) * 0.66f +
                                  rise(metrics.peak, 0.035f, 0.62f) * 0.22f +
                                  (metrics.style == AudioStyle::Silence ? 0.0f : 0.12f));
    const float highEnergy = clamp01(metrics.highMid * 0.58f + metrics.treble * 0.64f);
    const float lowEnergy = clamp01(metrics.bass * 0.68f + metrics.lowMid * 0.34f);
    const float lowBandHit = clamp01(metrics.bandOnsets[0] * 0.62f + metrics.bandOnsets[1] * 0.38f);
    const float midBandHit = clamp01(metrics.bandOnsets[2] * 0.68f + metrics.bandOnsets[3] * 0.32f);
    const float highBandHit = std::max(metrics.bandOnsets[3], metrics.bandOnsets[4]);
    const float transient = clamp01(metrics.spectralFlux * 0.48f +
                                    metrics.onset * 0.92f +
                                    maxBandOnset(metrics) * 0.30f);
    const float tempo = danceTempoScore(metrics.bpm);
    const float rhythmGrid = clamp01(metrics.beatConfidence * 0.46f +
                                     metrics.barConfidence * 0.24f +
                                     metrics.downbeatConfidence * 0.12f +
                                     lowBandHit * 0.22f +
                                     tempo * metrics.beatConfidence * 0.20f);
    const float harmonicFocus = clamp01(metrics.harmonicEnergy * 0.54f +
                                        metrics.keyConfidence * 0.30f +
                                        chromaFocus(metrics) * 0.34f +
                                        averageChroma(metrics) * 0.12f);
    const float bassDominance = rise(metrics.bass +
                                         metrics.lowMid * 0.22f -
                                         highEnergy * 0.46f -
                                         metrics.mid * 0.08f,
                                     0.02f,
                                     0.34f);
    const float highDominance = rise(highEnergy +
                                         metrics.spectralFlux * 0.18f -
                                         metrics.bass * 0.34f -
                                         metrics.lowMid * 0.12f,
                                     0.02f,
                                     0.30f);
    const float bassPressure = rise(metrics.bass, 0.20f, 0.78f);
    const float articulatedBassPressure = clamp01(rise(metrics.peak, 0.42f, 0.72f) * 0.42f +
                                                  rise(metrics.lowMid, 0.07f, 0.20f) * 0.28f +
                                                  rise(metrics.bass, 0.78f, 0.94f) * 0.24f +
                                                  lowBandHit * 0.14f +
                                                  metrics.dropIntensity * 0.10f);
    const float spatialCalm = clamp01(metrics.stereoWidth * 0.48f +
                                      (1.0f - transient) * 0.24f +
                                      styleEvidence(metrics, AudioStyle::Wide) * 0.18f +
                                      styleEvidence(metrics, AudioStyle::Ambient) * 0.20f);
    const float sectionDrop = metrics.section == ArrangementSection::Drop
                                  ? metrics.sectionConfidence
                                  : 0.0f;
    const float sectionBuild = metrics.section == ArrangementSection::Build
                                   ? metrics.sectionConfidence
                                   : 0.0f;
    const float sectionBreakdown = metrics.section == ArrangementSection::Breakdown
                                       ? metrics.sectionConfidence
                                       : 0.0f;
    const float darkTexture = clamp01((1.0f - highEnergy) * 0.42f +
                                      (1.0f - transient) * 0.18f +
                                      (1.0f - metrics.stereoWidth) * 0.12f +
                                      (metrics.keyMode == MusicalMode::Minor ? metrics.keyConfidence * 0.18f : 0.0f) +
                                      metrics.harmonicEnergy * 0.10f -
                                      articulatedBassPressure * 0.28f -
                                      metrics.dropIntensity * 0.20f -
                                      rhythmGrid * 0.08f);
    const float sparseLowMass = clamp01(rise(lowEnergy, 0.22f, 0.62f) *
                                        (1.0f - rise(metrics.bass, 0.76f, 0.94f)) *
                                        (1.0f - rise(metrics.dropIntensity, 0.30f, 0.62f)) *
                                        (1.0f - rise(lowBandHit, 0.42f, 0.82f)) *
                                        (1.0f - rise(metrics.peak, 0.42f, 0.72f)) *
                                        (1.0f - rise(metrics.lowMid, 0.07f, 0.20f)));
    const float darkMinimalPressure = clamp01(darkTexture * 0.60f +
                                              sparseLowMass * 0.36f +
                                              sectionBreakdown * 0.16f);

    float bassRole = audible * clamp01(metrics.bass * 0.46f +
                                       metrics.lowMid * 0.20f +
                                       metrics.dropIntensity * 0.28f +
                                       lowBandHit * 0.16f +
                                       bassDominance * 0.34f +
                                       bassPressure * 0.22f +
                                       styleEvidence(metrics, AudioStyle::BassHeavy) * 0.22f -
                                       highDominance * 0.16f -
                                       spatialCalm * 0.08f);
    float drumRole = audible * clamp01(rhythmGrid * 0.70f +
                                       lowBandHit * 0.18f +
                                       highBandHit * 0.10f +
                                       tempo * metrics.beatConfidence * 0.18f +
                                       styleEvidence(metrics, AudioStyle::Techno) * 0.24f -
                                       metrics.stereoWidth * 0.08f -
                                       harmonicFocus * 0.08f);
    float melodyRole = audible * clamp01(harmonicFocus * 0.46f +
                                         metrics.mid * 0.20f +
                                         metrics.highMid * 0.16f +
                                         midBandHit * 0.08f +
                                         styleEvidence(metrics, AudioStyle::Bright) * 0.06f +
                                         styleEvidence(metrics, AudioStyle::Wide) * 0.07f -
                                         bassDominance * 0.22f -
                                         metrics.dropIntensity * 0.14f);
    float harmonyRole = audible * clamp01(metrics.harmonicEnergy * 0.48f +
                                          metrics.keyConfidence * 0.30f +
                                          chromaFocus(metrics) * 0.20f +
                                          metrics.phraseConfidence * 0.16f +
                                          metrics.phraseIntensity * 0.10f +
                                          metrics.stereoWidth * 0.08f -
                                          transient * 0.10f -
                                          bassDominance * 0.12f);
    float spaceRole = audible * clamp01(metrics.stereoWidth * 0.54f +
                                        styleEvidence(metrics, AudioStyle::Wide) * 0.25f +
                                        styleEvidence(metrics, AudioStyle::Ambient) * 0.26f +
                                        (1.0f - transient) * 0.18f +
                                        sectionBreakdown * 0.14f -
                                        metrics.dropIntensity * 0.22f -
                                        highBandHit * 0.14f -
                                        bassDominance * 0.12f -
                                        rhythmGrid * 0.14f -
                                        highDominance * 0.18f);
    float fractureRole = audible * clamp01(transient * 0.48f +
                                           metrics.spectralFlux * 0.34f +
                                           highBandHit * 0.30f +
                                           midBandHit * 0.12f +
                                           metrics.onset * 0.16f +
                                           styleEvidence(metrics, AudioStyle::Bright) * 0.28f -
                                           rhythmGrid * tempo * 0.14f -
                                           spatialCalm * 0.08f);
    float shadowRole = audible * clamp01((1.0f - highEnergy) * lowEnergy * 0.34f +
                                         bassDominance * 0.18f +
                                         sectionBreakdown * 0.20f +
                                         darkMinimalPressure * 0.36f +
                                         (1.0f - metrics.stereoWidth) * 0.10f +
                                         (metrics.keyMode == MusicalMode::Minor ? metrics.keyConfidence * 0.18f : 0.0f) +
                                         styleEvidence(metrics, AudioStyle::BassHeavy) * 0.08f -
                                         highBandHit * 0.12f -
                                         transient * 0.08f);

    if (metrics.style == AudioStyle::Techno) {
        drumRole = std::max(drumRole,
                            audible * clamp01(0.14f +
                                              rhythmGrid * 0.56f +
                                              metrics.barConfidence * 0.28f +
                                              metrics.downbeatConfidence * 0.12f +
                                              tempo * 0.18f));
        spaceRole *= 0.72f;
        fractureRole *= 0.88f;
    } else if (metrics.style == AudioStyle::BassHeavy) {
        bassRole = std::max(bassRole, audible * clamp01(0.20f + bassPressure * 0.58f + metrics.dropIntensity * 0.22f));
        melodyRole *= 0.88f;
        spaceRole *= 0.80f;
    } else if (metrics.style == AudioStyle::Wide || metrics.style == AudioStyle::Ambient) {
        spaceRole = std::max(spaceRole, audible * clamp01(0.18f + spatialCalm * 0.64f));
        drumRole *= 0.76f;
        fractureRole *= 0.82f;
    } else if (metrics.style == AudioStyle::Bright) {
        fractureRole = std::max(fractureRole,
                                audible * clamp01(0.24f +
                                                  highDominance * 0.54f +
                                                  transient * 0.42f +
                                                  highBandHit * 0.20f));
        melodyRole = std::max(melodyRole, audible * clamp01(harmonicFocus * 0.36f + highEnergy * 0.22f));
        spaceRole *= 0.48f;
    }

    const float bassArticulation = clamp01(0.44f +
                                           metrics.dropIntensity * 0.28f +
                                           lowBandHit * 0.20f +
                                           rhythmGrid * 0.18f +
                                           styleEvidence(metrics, AudioStyle::BassHeavy) * 0.24f -
                                           darkMinimalPressure * 0.18f -
                                           spatialCalm * 0.16f -
                                           harmonicFocus * 0.12f);
    const float harmonicLift = clamp01((1.0f - rhythmGrid) * 0.28f +
                                       harmonicFocus * 0.24f +
                                       metrics.phraseConfidence * 0.08f);
    bassRole *= bassArticulation;
    melodyRole = std::max(melodyRole,
                          audible * clamp01((harmonicFocus * 0.24f +
                                             metrics.mid * 0.20f +
                                             metrics.lowMid * 0.08f +
                                             metrics.keyConfidence * 0.14f) *
                                            harmonicLift -
                                            bassDominance * 0.10f));
    harmonyRole = std::max(harmonyRole,
                           audible * clamp01((harmonicFocus * 0.26f +
                                              metrics.lowMid * 0.10f +
                                              metrics.phraseConfidence * 0.08f) *
                                             (0.58f + harmonicLift * 0.52f) -
                                             transient * 0.08f));

    if (darkMinimalPressure > 0.34f) {
        shadowRole = std::max(shadowRole,
                              audible * clamp01(0.18f +
                                                darkMinimalPressure * 0.58f +
                                                lowEnergy * 0.12f +
                                                (metrics.keyMode == MusicalMode::Minor ? metrics.keyConfidence * 0.08f : 0.0f)));
        const float restraint = clamp01((darkMinimalPressure - 0.28f) / 0.50f);
        const float sparseRestraint = restraint * (1.0f - articulatedBassPressure * 0.72f);
        bassRole *= std::clamp(1.0f - sparseRestraint * 0.52f, 0.40f, 1.0f);
        drumRole *= std::clamp(1.0f - restraint * 0.18f, 0.70f, 1.0f);
        fractureRole *= std::clamp(1.0f - restraint * 0.14f, 0.72f, 1.0f);
        spaceRole *= std::clamp(1.0f - restraint * 0.10f, 0.76f, 1.0f);
    }
    if (articulatedBassPressure > 0.18f && metrics.bass > 0.70f) {
        shadowRole *= std::clamp(1.0f - articulatedBassPressure * 0.34f, 0.60f, 1.0f);
    }

    const float impactRoles = clamp01(bassRole * 0.58f + drumRole * 0.50f + fractureRole * 0.30f);
    const float lyricalRoles = clamp01(melodyRole * 0.54f + harmonyRole * 0.48f + spaceRole * 0.24f);
    bassRole *= std::clamp(1.0f - spaceRole * 0.18f, 0.70f, 1.0f);
    drumRole *= std::clamp(1.0f - harmonyRole * 0.12f, 0.78f, 1.0f);
    melodyRole *= std::clamp(1.0f - bassRole * 0.20f - fractureRole * 0.10f, 0.66f, 1.0f);
    harmonyRole *= std::clamp(1.0f - fractureRole * 0.18f, 0.70f, 1.0f);
    spaceRole *= std::clamp(1.0f - impactRoles * 0.18f, 0.62f, 1.0f);
    fractureRole *= std::clamp(1.0f - lyricalRoles * 0.12f, 0.72f, 1.0f);
    shadowRole *= std::clamp(1.0f - spaceRole * 0.12f - fractureRole * 0.10f, 0.70f, 1.0f);

    metrics.bassRole = clamp01(bassRole);
    metrics.drumRole = clamp01(drumRole);
    metrics.melodyRole = clamp01(melodyRole);
    metrics.harmonyRole = clamp01(harmonyRole);
    metrics.spaceRole = clamp01(spaceRole);
    metrics.fractureRole = clamp01(fractureRole);
    metrics.shadowRole = clamp01(shadowRole);
    metrics.convergenceRole = audible * clamp01(metrics.dropIntensity * 0.42f +
                                                sectionDrop * 0.22f +
                                                sectionBuild * 0.14f +
                                                (metrics.downbeat ? metrics.downbeatConfidence * 0.18f : 0.0f) +
                                                (metrics.phraseBoundary ? metrics.phraseConfidence * 0.24f : 0.0f) +
                                                metrics.phraseIntensity * metrics.phraseConfidence * 0.14f +
                                                std::min(metrics.bassRole, metrics.drumRole) * 0.20f +
                                                std::min(metrics.melodyRole, metrics.harmonyRole) * 0.12f);

    std::array<float, 7> roles{
        metrics.bassRole,
        metrics.drumRole,
        metrics.melodyRole,
        metrics.harmonyRole,
        metrics.spaceRole,
        metrics.fractureRole,
        metrics.shadowRole
    };
    std::sort(roles.begin(), roles.end(), [](float left, float right) {
        return left > right;
    });

    float activeRoles = 0.0f;
    for (float role : roles) {
        activeRoles += role > 0.18f ? 1.0f : 0.0f;
    }

    const float topRole = roles[0];
    const float secondRole = roles[1];
    const float thirdRole = roles[2];
    const float dominance = topRole > 0.001f ? clamp01((topRole - secondRole) / topRole) : 0.0f;
    const float blendedFamilies = clamp01((secondRole + thirdRole) * 0.55f);
    metrics.roleSeparation = audible * clamp01(0.36f +
                                               dominance * 0.32f +
                                               topRole * 0.24f +
                                               blendedFamilies * 0.14f +
                                               (1.0f - std::min(activeRoles / 6.0f, 1.0f)) * 0.12f);
}

void AudioAnalyzer::updateArrangementSection(AudioMetrics& metrics,
                                             float longAverage,
                                             float shortAverage,
                                             float lowBandHit)
{
    const bool silent = metrics.rms < 0.006f && metrics.peak < 0.02f;
    SectionCandidate candidate;

    if (silent) {
        candidate = SectionCandidate{ArrangementSection::Silence, 1.0f};
    } else {
        const bool enoughHistory = longEnergyHistory_.size() >= 24;
        const float sectionThresholdScale = syncProfile_.sectionThresholdScale();
        const float sectionConfidenceGain = syncProfile_.sectionConfidenceGain();
        const float relativeEnergy = enoughHistory && longAverage > 0.004f
                                         ? metrics.rms / std::max(0.004f, longAverage)
                                         : 1.0f;
        const float shortRelative = enoughHistory && longAverage > 0.004f
                                        ? shortAverage / std::max(0.004f, longAverage)
                                        : 1.0f;
        const float buildScore = clamp01((metrics.phraseIntensity * 0.55f +
                                          metrics.spectralFlux * 0.65f +
                                          metrics.beatConfidence * 0.16f +
                                          std::max(0.0f, shortRelative - 1.0f) * 0.35f) *
                                         sectionConfidenceGain);
        const float dropScore = clamp01((metrics.dropIntensity * 0.72f +
                                         lowBandHit * 0.3f +
                                         metrics.beatConfidence * 0.22f) *
                                        sectionConfidenceGain);
        const float breakdownScore = enoughHistory
                                         ? clamp01((1.0f - relativeEnergy) * 1.35f +
                                                   (0.28f - metrics.bass) * 0.65f +
                                                   metrics.stereoWidth * 0.12f)
                                         : 0.0f;
        const float grooveScore = clamp01(0.36f +
                                          metrics.beatConfidence * 0.32f +
                                          metrics.rms * 1.35f +
                                          metrics.bass * 0.16f -
                                          metrics.dropIntensity * 0.18f -
                                          metrics.phraseIntensity * 0.14f);

        candidate = SectionCandidate{ArrangementSection::Groove, grooveScore};
        if (breakdownScore > candidate.confidence && metrics.rms > 0.006f &&
            metrics.dropIntensity < 0.38f * sectionThresholdScale) {
            candidate = SectionCandidate{ArrangementSection::Breakdown, breakdownScore};
        }
        if (buildScore > candidate.confidence && metrics.dropIntensity < 0.62f * sectionThresholdScale) {
            candidate = SectionCandidate{ArrangementSection::Build, buildScore};
        }
        if (dropScore > candidate.confidence && metrics.dropIntensity > 0.38f * sectionThresholdScale) {
            candidate = SectionCandidate{ArrangementSection::Drop, dropScore};
        }
    }

    const double secondsSinceSwitch = metrics.timeSeconds - lastSectionSwitchSeconds_;
    const bool sameSection = candidate.section == currentSection_;
    const bool forceSilence = candidate.section == ArrangementSection::Silence;
    const bool forceDrop = candidate.section == ArrangementSection::Drop && candidate.confidence > 0.62f;
    const bool confidentSwitch = candidate.confidence > metrics.sectionConfidence + 0.12f;
    const bool switchAllowed = sameSection ||
                               forceSilence ||
                               forceDrop ||
                               secondsSinceSwitch > 0.75 ||
                               confidentSwitch;

    if (!sameSection && switchAllowed) {
        currentSection_ = candidate.section;
        sectionStartSeconds_ = metrics.timeSeconds;
        lastSectionSwitchSeconds_ = metrics.timeSeconds;
    }

    metrics.section = currentSection_;
    metrics.sectionConfidence = currentSection_ == candidate.section
                                    ? candidate.confidence
                                    : std::max(0.2f, candidate.confidence * 0.72f);
    const float beatPhraseSeconds = metrics.bpm > 1.0f
                                        ? std::clamp((60.0f / metrics.bpm) * 16.0f, 3.0f, 12.0f)
                                        : 8.0f;
    metrics.sectionProgress = clamp01(static_cast<float>((metrics.timeSeconds - sectionStartSeconds_) /
                                                         static_cast<double>(beatPhraseSeconds)));
    syncProfile_.learnSection(metrics);
}

void AudioAnalyzer::updateBarState(AudioMetrics& metrics)
{
    const bool hasTempo = beatIntervalSeconds_ > 0.001 || metrics.bpm > 1.0f;
    const double beatInterval = beatIntervalSeconds_ > 0.001
                                    ? beatIntervalSeconds_
                                    : (metrics.bpm > 1.0f ? 60.0 / static_cast<double>(metrics.bpm) : 0.0);
    const double barInterval = beatInterval > 0.001 ? beatInterval * 4.0 : 0.0;
    const bool sectionBoundary = std::abs(metrics.timeSeconds - lastSectionSwitchSeconds_) < 0.03 &&
                                 metrics.sectionConfidence > 0.48f &&
                                 metrics.section != ArrangementSection::Silence;
    const bool dropBoundary = metrics.section == ArrangementSection::Drop &&
                              metrics.sectionConfidence > 0.54f &&
                              metrics.dropIntensity > 0.35f;
    const bool firstDownbeat = lastDownbeatTime_ < -1.0;
    const bool periodicDownbeat = metrics.beat && beatCountInBar_ >= 3;
    const bool firstPhrase = lastPhraseBoundaryTime_ < -1.0;

    if (metrics.beat) {
        const bool likelyDownbeat = firstDownbeat || sectionBoundary || dropBoundary || periodicDownbeat;
        if (likelyDownbeat) {
            metrics.downbeat = true;
            beatCountInBar_ = 0;
            lastDownbeatTime_ = metrics.timeSeconds;
            const bool phraseReset = firstPhrase ||
                                     sectionBoundary ||
                                     dropBoundary ||
                                     barsSincePhraseBoundary_ >= 3;
            if (phraseReset) {
                metrics.phraseBoundary = true;
                barsSincePhraseBoundary_ = 0;
                lastPhraseBoundaryTime_ = metrics.timeSeconds;
            } else {
                ++barsSincePhraseBoundary_;
            }
        } else {
            ++beatCountInBar_;
        }
    } else if (sectionBoundary && firstDownbeat) {
        lastDownbeatTime_ = metrics.timeSeconds;
        beatCountInBar_ = 0;
        metrics.phraseBoundary = true;
        barsSincePhraseBoundary_ = 0;
        lastPhraseBoundaryTime_ = metrics.timeSeconds;
    }

    const float tempoConfidence = hasTempo
                                      ? std::clamp(static_cast<float>(beatTimes_.size()) / 6.0f, 0.2f, 1.0f)
                                      : 0.0f;
    metrics.barConfidence = clamp01(tempoConfidence * 0.44f +
                                    metrics.sectionConfidence * 0.22f +
                                    metrics.beatConfidence * 0.18f +
                                    metrics.dropIntensity * 0.1f +
                                    syncProfile_.learnedWeight() * 0.06f);

    if (barInterval > 0.001 && lastDownbeatTime_ > -1.0) {
        double elapsed = metrics.timeSeconds - lastDownbeatTime_;
        if (elapsed < 0.0) {
            elapsed = 0.0;
        }
        metrics.barPhase = clamp01(static_cast<float>(std::fmod(elapsed, barInterval) / barInterval));
    } else if (hasTempo) {
        const float beatUnit = static_cast<float>(std::max(0, beatCountInBar_)) * 0.25f;
        metrics.barPhase = clamp01(beatUnit + metrics.beatPhase * 0.25f);
    } else {
        metrics.barPhase = 0.0f;
    }

    const float phaseProximity = 1.0f - std::min(1.0f, std::abs(metrics.barPhase) * 8.0f);
    metrics.downbeatConfidence = metrics.downbeat
                                     ? clamp01(0.42f +
                                               metrics.barConfidence * 0.42f +
                                               metrics.beatConfidence * 0.18f +
                                               metrics.dropIntensity * 0.16f)
                                     : clamp01(phaseProximity * metrics.barConfidence * 0.5f);

    const float phrasePhaseRaw = (static_cast<float>(std::max(0, barsSincePhraseBoundary_)) +
                                  metrics.barPhase) /
                                 4.0f;
    metrics.phrasePhase = clamp01(phrasePhaseRaw);
    metrics.phraseConfidence = clamp01(metrics.barConfidence * 0.48f +
                                       metrics.downbeatConfidence * 0.18f +
                                       metrics.sectionConfidence * 0.16f +
                                       tempoConfidence * 0.12f +
                                       syncProfile_.learnedWeight() * 0.06f);
    if (metrics.phraseBoundary) {
        metrics.phraseConfidence = std::max(metrics.phraseConfidence,
                                            clamp01(0.42f +
                                                    metrics.downbeatConfidence * 0.24f +
                                                    metrics.sectionConfidence * 0.18f +
                                                    metrics.dropIntensity * 0.16f));
    }

    const float phraseRamp = metrics.phrasePhase * metrics.phrasePhase * (3.0f - 2.0f * metrics.phrasePhase);
    float tension = phraseRamp * 0.42f +
                    metrics.phraseIntensity * 0.28f +
                    metrics.spectralFlux * 0.18f +
                    metrics.beatConfidence * 0.08f;
    if (metrics.section == ArrangementSection::Build) {
        tension += metrics.sectionProgress * metrics.sectionConfidence * 0.42f;
    } else if (metrics.section == ArrangementSection::Breakdown) {
        tension += std::max(0.0f, metrics.phrasePhase - 0.48f) * metrics.sectionConfidence * 0.2f;
    } else if (metrics.section == ArrangementSection::Drop) {
        tension = std::max(tension * 0.35f, metrics.dropIntensity * 0.36f);
    }
    if (metrics.phraseBoundary && metrics.section == ArrangementSection::Drop) {
        tension = std::max(tension, metrics.dropIntensity);
    }
    metrics.buildTension = clamp01(tension * (0.45f + metrics.phraseConfidence * 0.8f));
}

AudioStyle AudioAnalyzer::heuristicStyle(const AudioMetrics& metrics) const
{
    if (metrics.rms < 0.006f && metrics.peak < 0.02f) {
        return AudioStyle::Silence;
    }

    const float technoScore = styleEvidence(metrics, AudioStyle::Techno);
    const float bassScore = styleEvidence(metrics, AudioStyle::BassHeavy);
    const float brightScore = styleEvidence(metrics, AudioStyle::Bright);
    const float wideScore = styleEvidence(metrics, AudioStyle::Wide);
    const float bassPressure = rise(metrics.bass, 0.50f, 0.78f);
    if (bassScore > 0.42f &&
        bassPressure > 0.38f &&
        metrics.bass > metrics.lowMid * 2.4f &&
        metrics.highMid + metrics.treble < metrics.bass * 0.36f &&
        (metrics.dropIntensity > 0.16f || metrics.bandOnsets[0] > 0.18f || metrics.beatConfidence > 0.14f)) {
        return AudioStyle::BassHeavy;
    }
    if (brightScore > 0.42f &&
        (metrics.highMid + metrics.treble > metrics.bass * 0.92f ||
         metrics.spectralFlux > 0.09f) &&
        brightScore + 0.08f >= technoScore &&
        brightScore + 0.10f >= bassScore) {
        return AudioStyle::Bright;
    }
    if (danceTempoScore(metrics.bpm) > 0.55f &&
        (metrics.beatConfidence > 0.08f || metrics.spectralFlux > 0.08f) &&
        (metrics.dropIntensity < 0.72f || metrics.bass < 0.60f) &&
        technoScore + (metrics.bass < 0.60f ? 0.24f : 0.08f) >= bassScore) {
        return AudioStyle::Techno;
    }
    if (metrics.stereoWidth > 0.52f &&
        metrics.rms > 0.02f &&
        metrics.dropIntensity < 0.52f &&
        metrics.bass < 0.62f &&
        danceTempoScore(metrics.bpm) < 0.55f &&
        wideScore + 0.12f >= bassScore) {
        return AudioStyle::Wide;
    }

    AudioStyle best = AudioStyle::Ambient;
    float bestScore = styleEvidence(metrics, best);
    for (AudioStyle candidate : {AudioStyle::Techno,
                                 AudioStyle::BassHeavy,
                                 AudioStyle::Bright,
                                 AudioStyle::Wide}) {
        const float score = styleEvidence(metrics, candidate);
        if (score > bestScore + 0.025f) {
            best = candidate;
            bestScore = score;
        }
    }
    return best;
}

} // namespace viz
