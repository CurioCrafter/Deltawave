#include "TestHarness.hpp"
#include "Visualizer/Audio/AdaptiveStyleModel.hpp"
#include "Visualizer/Audio/AudioAnalyzer.hpp"
#include "Visualizer/Audio/AudioFileLoader.hpp"
#include "Visualizer/Audio/AudioSyncProfile.hpp"
#include "Visualizer/Audio/WavFile.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

namespace viz::tests {
namespace {

constexpr float kPi = 3.14159265358979323846f;

void writeU16(std::ofstream& output, std::uint16_t value)
{
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void writeU32(std::ofstream& output, std::uint32_t value)
{
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
    output.put(static_cast<char>((value >> 16U) & 0xFFU));
    output.put(static_cast<char>((value >> 24U) & 0xFFU));
}

void writeTestWav(const std::filesystem::path& path)
{
    constexpr int sampleRate = 48000;
    constexpr int channels = 2;
    constexpr int frames = 4800;
    constexpr int bits = 16;
    constexpr int blockAlign = channels * bits / 8;
    constexpr int dataBytes = frames * blockAlign;

    std::ofstream output(path, std::ios::binary);
    output.write("RIFF", 4);
    writeU32(output, 36U + static_cast<std::uint32_t>(dataBytes));
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    writeU32(output, 16);
    writeU16(output, 1);
    writeU16(output, channels);
    writeU32(output, sampleRate);
    writeU32(output, sampleRate * blockAlign);
    writeU16(output, blockAlign);
    writeU16(output, bits);
    output.write("data", 4);
    writeU32(output, dataBytes);

    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const auto left = static_cast<std::int16_t>(std::sin(2.0f * kPi * 110.0f * t) * 18000.0f);
        const auto right = static_cast<std::int16_t>(std::sin(2.0f * kPi * 880.0f * t) * 12000.0f);
        writeU16(output, static_cast<std::uint16_t>(left));
        writeU16(output, static_cast<std::uint16_t>(right));
    }
}

std::vector<float> makeStyleFrame(float lowHz,
                                  float lowAmp,
                                  float midHz,
                                  float midAmp,
                                  float highHz,
                                  float highAmp,
                                  float stereoSpread,
                                  bool transient)
{
    constexpr int frames = 4096;
    std::vector<float> samples(frames * 2, 0.0f);
    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / 48000.0f;
        const float low = std::sin(2.0f * kPi * lowHz * t) * lowAmp;
        const float mid = std::sin(2.0f * kPi * midHz * t) * midAmp;
        const float high = std::sin(2.0f * kPi * highHz * t) * highAmp;
        const float click = transient && i >= frames - 420
                                ? std::sin(2.0f * kPi * 3600.0f * t) * (0.32f + highAmp * 0.45f)
                                : 0.0f;
        const float spread = std::clamp(stereoSpread, 0.0f, 1.0f);
        const float left = low + mid + high + click;
        const float right = low * (1.0f - spread * 0.28f) -
                            mid * spread +
                            high * (1.0f - spread * 1.55f) -
                            click * spread * 0.75f;
        samples[static_cast<std::size_t>(i) * 2U] = std::clamp(left, -1.0f, 1.0f);
        samples[(static_cast<std::size_t>(i) * 2U) + 1U] = std::clamp(right, -1.0f, 1.0f);
    }
    return samples;
}

AudioMetrics analyzePulsedStyle(float lowHz,
                                float bodyLow,
                                float kickLow,
                                float midAmp,
                                float highAmp,
                                float stereoSpread,
                                double beatPeriodSeconds,
                                bool transient)
{
    AudioAnalyzer analyzer(48000, 2);
    std::vector<float> silence(2048 * 2, 0.0f);
    for (int i = 0; i < 30; ++i) {
        analyzer.analyzeInterleaved(silence.data(), 2048, static_cast<double>(i) * 0.05);
    }

    AudioMetrics metrics;
    const std::vector<float> body = makeStyleFrame(lowHz, bodyLow, 720.0f, midAmp, 3600.0f, highAmp, stereoSpread, false);
    const std::vector<float> kick = makeStyleFrame(lowHz, kickLow, 920.0f, midAmp, 4200.0f, highAmp, stereoSpread, transient);
    for (int beat = 0; beat < 12; ++beat) {
        const double baseTime = 1.0 + static_cast<double>(beat) * beatPeriodSeconds;
        analyzer.analyzeInterleaved(body.data(), 4096, baseTime - 0.08);
        metrics = analyzer.analyzeInterleaved(kick.data(), 4096, baseTime);
    }
    return metrics;
}

void requireRolesNormalized(const AudioMetrics& metrics, const std::string& label)
{
    const float roles[] = {
        metrics.bassRole,
        metrics.drumRole,
        metrics.melodyRole,
        metrics.harmonyRole,
        metrics.spaceRole,
        metrics.fractureRole,
        metrics.shadowRole,
        metrics.convergenceRole,
        metrics.roleSeparation
    };
    for (float role : roles) {
        require(role >= 0.0f && role <= 1.0f, label + " musical role values should stay normalized");
    }
}

} // namespace

void analyzerReportsSilence()
{
    AudioAnalyzer analyzer(48000, 2);
    std::vector<float> silence(2048 * 2, 0.0f);
    const AudioMetrics metrics = analyzer.analyzeInterleaved(silence.data(), 2048, 0.0);
    require(metrics.rms == 0.0f, "silence RMS should be zero");
    require(metrics.peak == 0.0f, "silence peak should be zero");
    require(metrics.style == AudioStyle::Silence, "silence style should be Silence");
}

void analyzerFindsBassAndStereo()
{
    AudioAnalyzer analyzer(48000, 2);
    std::vector<float> samples(4096 * 2, 0.0f);
    for (int i = 0; i < 4096; ++i) {
        const float t = static_cast<float>(i) / 48000.0f;
        samples[static_cast<std::size_t>(i) * 2U] = std::sin(2.0f * kPi * 95.0f * t) * 0.55f;
        samples[(static_cast<std::size_t>(i) * 2U) + 1U] = std::sin(2.0f * kPi * 1800.0f * t) * 0.35f;
    }

    const AudioMetrics metrics = analyzer.analyzeInterleaved(samples.data(), 4096, 0.2);
    require(metrics.rms > 0.1f, "tone should produce RMS energy");
    require(metrics.peak > 0.45f, "tone should produce peak energy");
    require(metrics.bass > 0.05f, "low tone should appear in bass band");
    require(metrics.stereoWidth > 0.1f, "different left/right tones should produce stereo width");
}

void analyzerDetectsChromaAndKey()
{
    AudioAnalyzer analyzer(48000, 2);
    std::vector<float> samples(8192 * 2, 0.0f);
    constexpr float cBinHz = 1048.4280f;
    constexpr float eBinHz = 1284.4994f;
    constexpr float gBinHz = 1573.7262f;
    for (int i = 0; i < 8192; ++i) {
        const float t = static_cast<float>(i) / 48000.0f;
        const float chord = (std::sin(2.0f * kPi * cBinHz * t) * 0.34f) +
                            (std::sin(2.0f * kPi * eBinHz * t) * 0.28f) +
                            (std::sin(2.0f * kPi * gBinHz * t) * 0.31f);
        samples[static_cast<std::size_t>(i) * 2U] = chord;
        samples[(static_cast<std::size_t>(i) * 2U) + 1U] = chord;
    }

    analyzer.analyzeInterleaved(samples.data(), 8192, 0.4);
    const AudioMetrics metrics = analyzer.analyzeInterleaved(samples.data(), 8192, 0.6);
    require(metrics.harmonicEnergy > 0.1f, "tonal chord should produce harmonic energy");
    require(metrics.keyIndex == 0,
            "C-major chord should detect C as the key root, got " + std::string(keyName(metrics.keyIndex)));
    require(metrics.keyMode == MusicalMode::Major,
            "major chord should detect major mode, got " + std::string(toString(metrics.keyMode)));
    require(metrics.keyConfidence > 0.1f, "detected key should have usable confidence");
    require(metrics.chroma[0] > 0.02f && metrics.chroma[4] > 0.02f && metrics.chroma[7] > 0.02f,
            "C, E, and G chroma bins should be active");
}

void wavLoaderReadsGeneratedPcm()
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "visualizer_test.wav";
    writeTestWav(path);
    std::string error;
    const std::optional<WavAudio> wav = loadWavFile(path, error);
    std::filesystem::remove(path);

    require(wav.has_value(), "generated WAV should load: " + error);
    require(wav->sampleRate == 48000, "sample rate should round-trip");
    require(wav->channelCount == 2, "channel count should round-trip");
    require(wav->bitsPerSample == 16, "bit depth should round-trip");
    require(wav->samples.size() == 4800U * 2U, "sample count should match generated WAV");
    require(wav->durationSeconds > 0.09 && wav->durationSeconds < 0.11, "duration should be near 0.1s");
}

void audioFileLoaderReadsGeneratedWav()
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "visualizer_test_loader.WAVE";
    writeTestWav(path);
    std::string error;
    const std::optional<WavAudio> audio = loadAudioFile(path, error, AudioLoadOptions{false});
    std::filesystem::remove(path);

    require(isLikelyWavFile(path), "loader should recognize WAV-style extensions case-insensitively");
    require(audio.has_value(), "audio-file loader should load generated WAV through portable fallback: " + error);
    require(audio->sampleRate == 48000, "audio loader should preserve sample rate");
    require(audio->channelCount == 2, "audio loader should preserve channels");
    require(audio->samples.size() == 4800U * 2U, "audio loader sample count should match generated WAV");
}

void adaptiveStyleModelPredictsTechno()
{
    AudioMetrics metrics{};
    metrics.rms = 0.45f;
    metrics.peak = 0.9f;
    metrics.bass = 0.76f;
    metrics.lowMid = 0.58f;
    metrics.mid = 0.35f;
    metrics.highMid = 0.42f;
    metrics.treble = 0.4f;
    metrics.spectralCentroid = 0.31f;
    metrics.stereoWidth = 0.24f;
    metrics.onset = 0.12f;
    metrics.beatConfidence = 0.8f;
    metrics.bpm = 128.0f;

    AdaptiveStyleModel model;
    const StylePrediction prediction = model.predict(metrics);
    require(prediction.style == AudioStyle::Techno, "techno feature vector should classify as Techno");
    require(prediction.confidence > 0.35f, "techno prediction should have useful confidence");
}

void adaptiveStyleModelPersistsProfile()
{
    AudioMetrics wide{};
    wide.rms = 0.34f;
    wide.peak = 0.76f;
    wide.bass = 0.28f;
    wide.lowMid = 0.36f;
    wide.mid = 0.42f;
    wide.highMid = 0.5f;
    wide.treble = 0.52f;
    wide.spectralCentroid = 0.44f;
    wide.stereoWidth = 0.88f;
    wide.onset = 0.08f;
    wide.beatConfidence = 0.38f;
    wide.bpm = 124.0f;

    AdaptiveStyleModel model;
    for (int i = 0; i < 18; ++i) {
        model.learn(wide, AudioStyle::Wide, 0.92f);
    }
    require(model.learnedWeight(AudioStyle::Wide) > 0.4f, "wide centroid should accumulate learned weight");

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_style_profile_test";
    const std::filesystem::path profile = root / "adaptive_style_profile.vizaudio";
    std::filesystem::remove_all(root);

    std::string error;
    require(model.saveProfile(profile, error), "style profile should save: " + error);
    require(std::filesystem::exists(profile), "style profile file should exist");

    AdaptiveStyleModel loaded;
    require(loaded.loadProfile(profile, error), "style profile should load: " + error);
    require(loaded.learnedWeight(AudioStyle::Wide) > 0.4f, "loaded profile should preserve learned weight");
    const StylePrediction prediction = loaded.predict(wide);
    require(prediction.style == AudioStyle::Wide, "loaded profile should preserve adapted wide classification");

    const std::filesystem::path invalid = root / "invalid.vizaudio";
    {
        std::ofstream output(invalid);
        output << "style=Wide\nfeatures=bad-data\nlearnedWeight=0.9\n";
    }
    const float beforeInvalid = loaded.learnedWeight(AudioStyle::Wide);
    require(!loaded.loadProfile(invalid, error), "invalid style profile should fail to load");
    require(std::fabs(loaded.learnedWeight(AudioStyle::Wide) - beforeInvalid) < 0.001f,
            "invalid profile load should not replace the current model");

    std::filesystem::remove_all(root);
}

void audioAnalyzerLoadsAndSavesStyleProfile()
{
    AudioMetrics bright{};
    bright.rms = 0.28f;
    bright.peak = 0.68f;
    bright.bass = 0.16f;
    bright.lowMid = 0.22f;
    bright.mid = 0.38f;
    bright.highMid = 0.78f;
    bright.treble = 0.9f;
    bright.spectralCentroid = 0.74f;
    bright.stereoWidth = 0.24f;
    bright.onset = 0.08f;
    bright.beatConfidence = 0.25f;
    bright.bpm = 116.0f;

    AdaptiveStyleModel sourceModel;
    for (int i = 0; i < 20; ++i) {
        sourceModel.learn(bright, AudioStyle::Bright, 0.95f);
    }

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_analyzer_profile_test";
    const std::filesystem::path profile = root / "profile.vizaudio";
    std::filesystem::remove_all(root);

    std::string error;
    require(sourceModel.saveProfile(profile, error), "source style profile should save: " + error);

    AudioAnalyzer analyzer(48000, 2);
    require(analyzer.loadStyleProfile(profile, error), "analyzer should load style profile: " + error);
    require(analyzer.learnedStyleWeight(AudioStyle::Bright) > 0.45f,
            "analyzer should expose loaded learned weight");
    analyzer.reset();
    require(analyzer.learnedStyleWeight(AudioStyle::Bright) > 0.45f,
            "audio reset should preserve learned style profile");

    const std::filesystem::path saved = root / "saved.vizaudio";
    require(analyzer.saveStyleProfile(saved, error), "analyzer should save style profile: " + error);
    require(std::filesystem::exists(saved), "saved analyzer style profile should exist");

    analyzer.resetStyleProfile();
    require(analyzer.learnedStyleWeight(AudioStyle::Bright) < 0.01f,
            "explicit profile reset should clear learned weight");

    std::filesystem::remove_all(root);
}

void audioSyncProfilePersistsSensitivity()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_sync_profile_test";
    const std::filesystem::path profilePath = root / "adaptive_sync_profile.vizsync";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    {
        std::ofstream output(profilePath);
        output << "# test sync profile\n";
        output << "beatSensitivity=1.280\n";
        output << "sectionSensitivity=1.160\n";
        output << "learnedWeight=0.420\n";
    }

    std::string error;
    AudioSyncProfile profile;
    require(profile.loadProfile(profilePath, error), "sync profile should load: " + error);
    require(profile.beatSensitivity() > 1.27f && profile.beatSensitivity() < 1.29f,
            "loaded sync profile should preserve beat sensitivity");
    require(profile.sectionSensitivity() > 1.15f && profile.sectionSensitivity() < 1.17f,
            "loaded sync profile should preserve section sensitivity");
    require(profile.learnedWeight() > 0.41f && profile.learnedWeight() < 0.43f,
            "loaded sync profile should preserve learned weight");
    require(profile.beatThresholdScale() < 0.79f,
            "higher beat sensitivity should lower the effective beat threshold");

    const std::filesystem::path savedPath = root / "saved.vizsync";
    require(profile.saveProfile(savedPath, error), "sync profile should save: " + error);
    AudioSyncProfile reloaded;
    require(reloaded.loadProfile(savedPath, error), "saved sync profile should reload: " + error);
    require(std::fabs(reloaded.sectionSensitivity() - profile.sectionSensitivity()) < 0.001f,
            "saved sync profile should round-trip section sensitivity");

    const std::filesystem::path invalidPath = root / "invalid.vizsync";
    {
        std::ofstream output(invalidPath);
        output << "beatSensitivity=bad\nsectionSensitivity=1.0\nlearnedWeight=0.2\n";
    }
    const float beforeInvalid = reloaded.beatSensitivity();
    require(!reloaded.loadProfile(invalidPath, error), "invalid sync profile should fail to load");
    require(std::fabs(reloaded.beatSensitivity() - beforeInvalid) < 0.001f,
            "invalid sync profile load should not replace current calibration");

    std::filesystem::remove_all(root);
}

void audioAnalyzerLoadsAndSavesSyncProfile()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "visualizer_analyzer_sync_profile_test";
    const std::filesystem::path profilePath = root / "source.vizsync";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    {
        std::ofstream output(profilePath);
        output << "beatSensitivity=1.180\n";
        output << "sectionSensitivity=0.920\n";
        output << "learnedWeight=0.360\n";
    }

    std::string error;
    AudioAnalyzer analyzer(48000, 2);
    require(analyzer.loadSyncProfile(profilePath, error), "analyzer should load sync profile: " + error);
    require(analyzer.beatSensitivity() > 1.17f && analyzer.beatSensitivity() < 1.19f,
            "analyzer should expose loaded beat sensitivity");
    require(analyzer.sectionSensitivity() > 0.91f && analyzer.sectionSensitivity() < 0.93f,
            "analyzer should expose loaded section sensitivity");
    analyzer.reset();
    require(analyzer.beatSensitivity() > 1.17f && analyzer.beatSensitivity() < 1.19f,
            "audio reset should preserve sync profile calibration");

    std::vector<float> samples(2048 * 2, 0.0f);
    for (int i = 0; i < 2048; ++i) {
        const float t = static_cast<float>(i) / 48000.0f;
        const float low = std::sin(2.0f * kPi * 96.0f * t) * 0.42f;
        samples[static_cast<std::size_t>(i) * 2U] = low;
        samples[(static_cast<std::size_t>(i) * 2U) + 1U] = low;
    }
    const AudioMetrics metrics = analyzer.analyzeInterleaved(samples.data(), 2048, 0.1);
    require(metrics.beatSensitivity > 1.17f && metrics.beatSensitivity < 1.19f,
            "analyzer metrics should expose active beat sensitivity for the HUD");
    require(metrics.sectionSensitivity > 0.91f && metrics.sectionSensitivity < 0.93f,
            "analyzer metrics should expose active section sensitivity for the HUD");

    const std::filesystem::path savedPath = root / "saved.vizsync";
    require(analyzer.saveSyncProfile(savedPath, error), "analyzer should save sync profile: " + error);
    require(std::filesystem::exists(savedPath), "saved analyzer sync profile should exist");

    analyzer.resetSyncProfile();
    require(analyzer.syncProfileLearnedWeight() < 0.01f,
            "explicit sync profile reset should clear learned sync weight");
    require(std::fabs(analyzer.beatSensitivity() - 1.0f) < 0.001f,
            "explicit sync profile reset should restore default beat sensitivity");

    std::filesystem::remove_all(root);
}

void analyzerReportsAdvancedSyncMetrics()
{
    AudioAnalyzer analyzer(48000, 2);
    std::vector<float> silence(2048 * 2, 0.0f);
    for (int i = 0; i < 30; ++i) {
        analyzer.analyzeInterleaved(silence.data(), 2048, static_cast<double>(i) * 0.05);
    }

    std::vector<float> burst(4096 * 2, 0.0f);
    for (int i = 0; i < 4096; ++i) {
        const float t = static_cast<float>(i) / 48000.0f;
        const float low = std::sin(2.0f * kPi * 92.0f * t) * 0.82f;
        const float high = std::sin(2.0f * kPi * 4200.0f * t) * 0.26f;
        burst[static_cast<std::size_t>(i) * 2U] = low + high;
        burst[(static_cast<std::size_t>(i) * 2U) + 1U] = low - high;
    }

    const AudioMetrics metrics = analyzer.analyzeInterleaved(burst.data(), 4096, 1.6);
    require(metrics.spectralFlux > 0.02f, "burst should produce spectral flux");
    require(metrics.bandOnsets[0] > 0.05f, "bass burst should produce a bass-band onset");
    require(metrics.dropIntensity > 0.05f, "burst after quiet history should produce drop intensity");
    require(metrics.phraseIntensity >= 0.0f && metrics.phraseIntensity <= 1.0f, "phrase intensity should be normalized");
    require(metrics.beatPhase >= 0.0f && metrics.beatPhase <= 1.0f, "beat phase should be normalized");
    require(metrics.barPhase >= 0.0f && metrics.barPhase <= 1.0f, "bar phase should be normalized");
    require(metrics.barConfidence >= 0.0f && metrics.barConfidence <= 1.0f, "bar confidence should be normalized");
    require(metrics.downbeatConfidence >= 0.0f && metrics.downbeatConfidence <= 1.0f,
            "downbeat confidence should be normalized");
    require(metrics.phrasePhase >= 0.0f && metrics.phrasePhase <= 1.0f,
            "phrase phase should be normalized");
    require(metrics.phraseConfidence >= 0.0f && metrics.phraseConfidence <= 1.0f,
            "phrase confidence should be normalized");
    require(metrics.buildTension >= 0.0f && metrics.buildTension <= 1.0f,
            "build tension should be normalized");
}

void analyzerClassifiesMusicalStyleCues()
{
    const AudioMetrics techno = analyzePulsedStyle(142.0f, 0.06f, 0.20f, 0.48f, 0.26f, 0.18f, 60.0 / 128.0, true);
    require(techno.style == AudioStyle::Techno,
            "regular kick-driven dance content should classify as Techno, got " + std::string(toString(techno.style)) +
                " bpm=" + std::to_string(techno.bpm) +
                " bass=" + std::to_string(techno.bass) +
                " lowMid=" + std::to_string(techno.lowMid) +
                " highMid=" + std::to_string(techno.highMid) +
                " treble=" + std::to_string(techno.treble) +
                " beat=" + std::to_string(techno.beatConfidence) +
                " drop=" + std::to_string(techno.dropIntensity) +
                " flux=" + std::to_string(techno.spectralFlux));
    require(techno.styleConfidence > 0.42f, "techno cue classification should carry usable confidence");

    const AudioMetrics bass = analyzePulsedStyle(72.0f, 0.18f, 0.88f, 0.12f, 0.04f, 0.14f, 60.0 / 140.0, false);
    require(bass.style == AudioStyle::BassHeavy,
            "dominant low-frequency pressure should classify as Bass Heavy, got " + std::string(toString(bass.style)));
    require(bass.styleConfidence > 0.42f, "bass-heavy cue classification should carry usable confidence");

    const AudioMetrics bright = analyzePulsedStyle(260.0f, 0.01f, 0.025f, 0.06f, 0.95f, 0.34f, 60.0 / 104.0, true);
    require(bright.style == AudioStyle::Bright,
            "bright transient-heavy content should classify as Bright, got " + std::string(toString(bright.style)) +
                " bpm=" + std::to_string(bright.bpm) +
                " bass=" + std::to_string(bright.bass) +
                " lowMid=" + std::to_string(bright.lowMid) +
                " highMid=" + std::to_string(bright.highMid) +
                " treble=" + std::to_string(bright.treble) +
                " centroid=" + std::to_string(bright.spectralCentroid) +
                " onset=" + std::to_string(bright.onset) +
                " beat=" + std::to_string(bright.beatConfidence) +
                " drop=" + std::to_string(bright.dropIntensity) +
                " flux=" + std::to_string(bright.spectralFlux));
    require(bright.styleConfidence > 0.42f, "bright cue classification should carry usable confidence");

    AudioAnalyzer wideAnalyzer(48000, 2);
    AudioMetrics wide;
    const std::vector<float> wideFrame = makeStyleFrame(180.0f, 0.12f, 620.0f, 0.22f, 2800.0f, 0.24f, 0.95f, false);
    for (int frame = 0; frame < 18; ++frame) {
        wide = wideAnalyzer.analyzeInterleaved(wideFrame.data(), 4096, static_cast<double>(frame) * 0.12);
    }
    require(wide.style == AudioStyle::Wide,
            "strong stereo spread should classify as Wide when bass/drop cues are modest, got " +
                std::string(toString(wide.style)));

    AudioAnalyzer quietAnalyzer(48000, 2);
    AudioMetrics quiet;
    const std::vector<float> quietFrame = makeStyleFrame(220.0f, 0.016f, 880.0f, 0.020f, 1800.0f, 0.010f, 0.28f, false);
    for (int frame = 0; frame < 12; ++frame) {
        quiet = quietAnalyzer.analyzeInterleaved(quietFrame.data(), 4096, static_cast<double>(frame) * 0.15);
    }
    require(quiet.style == AudioStyle::Ambient || quiet.style == AudioStyle::Silence,
            "very quiet musical content should stay calm instead of becoming a dance/drop style");
}

void analyzerReportsSeparatedMusicalRoles()
{
    const AudioMetrics techno = analyzePulsedStyle(142.0f, 0.06f, 0.20f, 0.48f, 0.18f, 0.14f, 60.0 / 128.0, true);
    requireRolesNormalized(techno, "techno");
    require(techno.drumRole > 0.30f &&
                techno.drumRole > techno.spaceRole + 0.08f &&
                techno.drumRole > techno.melodyRole,
            "techno should expose the drum/sequencer role instead of a generic cloud; drum=" +
                std::to_string(techno.drumRole) +
                " space=" + std::to_string(techno.spaceRole) +
                " melody=" + std::to_string(techno.melodyRole));

    const AudioMetrics bass = analyzePulsedStyle(72.0f, 0.18f, 0.88f, 0.12f, 0.04f, 0.14f, 60.0 / 140.0, false);
    requireRolesNormalized(bass, "bass");
    require(bass.bassRole > 0.44f &&
                bass.bassRole > bass.melodyRole + 0.18f &&
                bass.bassRole > bass.spaceRole + 0.18f,
            "bass-heavy material should drive bass pressure separately; bass=" +
                std::to_string(bass.bassRole) +
                " melody=" + std::to_string(bass.melodyRole) +
                " space=" + std::to_string(bass.spaceRole));

    const AudioMetrics bright = analyzePulsedStyle(260.0f, 0.01f, 0.025f, 0.06f, 0.95f, 0.34f, 60.0 / 104.0, true);
    requireRolesNormalized(bright, "bright");
    require(bright.fractureRole > 0.32f &&
                bright.fractureRole > bright.spaceRole + 0.10f,
            "bright transient material should drive fracture/cut-plane motion; fracture=" +
                std::to_string(bright.fractureRole) +
                " space=" + std::to_string(bright.spaceRole));

    AudioAnalyzer wideAnalyzer(48000, 2);
    AudioMetrics wide;
    const std::vector<float> wideFrame = makeStyleFrame(180.0f, 0.10f, 620.0f, 0.21f, 2800.0f, 0.22f, 0.96f, false);
    for (int frame = 0; frame < 18; ++frame) {
        wide = wideAnalyzer.analyzeInterleaved(wideFrame.data(), 4096, static_cast<double>(frame) * 0.12);
    }
    requireRolesNormalized(wide, "wide");
    require(wide.spaceRole > 0.32f &&
                wide.spaceRole > wide.drumRole + 0.14f &&
                wide.spaceRole > wide.bassRole + 0.12f,
            "wide/ambient material should read as spatial depth; space=" +
                std::to_string(wide.spaceRole) +
                " drum=" + std::to_string(wide.drumRole) +
                " bass=" + std::to_string(wide.bassRole));

    AudioAnalyzer melodicAnalyzer(48000, 2);
    AudioMetrics melodic;
    const std::vector<float> melodicFrame = makeStyleFrame(261.63f, 0.035f, 659.25f, 0.26f, 1046.50f, 0.18f, 0.42f, false);
    for (int frame = 0; frame < 18; ++frame) {
        melodic = melodicAnalyzer.analyzeInterleaved(melodicFrame.data(), 4096, static_cast<double>(frame) * 0.12);
    }
    requireRolesNormalized(melodic, "melodic");
    require(melodic.melodyRole > melodic.bassRole + 0.12f &&
                melodic.harmonyRole > melodic.bassRole + 0.08f,
            "harmonic material should drive melody/harmony roles instead of bass mass; melody=" +
                std::to_string(melodic.melodyRole) +
                " harmony=" + std::to_string(melodic.harmonyRole) +
                " bass=" + std::to_string(melodic.bassRole));

    const AudioMetrics dark = analyzePulsedStyle(86.0f, 0.26f, 0.50f, 0.08f, 0.01f, 0.05f, 60.0 / 124.0, false);
    requireRolesNormalized(dark, "dark");
    require(dark.shadowRole > 0.14f && dark.shadowRole > bright.shadowRole + 0.04f,
            "dark sparse low material should expose shadow/monolith role; dark shadow=" +
                std::to_string(dark.shadowRole) +
                " bright shadow=" + std::to_string(bright.shadowRole));

    require(std::min({techno.roleSeparation,
                      bass.roleSeparation,
                      bright.roleSeparation,
                      wide.roleSeparation,
                      melodic.roleSeparation,
                      dark.roleSeparation}) > 0.26f,
            "musical role interpreter should keep non-silent profiles separable");
}

void analyzerTracksBarPhaseAndDownbeats()
{
    AudioAnalyzer analyzer(48000, 2);
    std::vector<float> silence(2048 * 2, 0.0f);
    for (int i = 0; i < 30; ++i) {
        analyzer.analyzeInterleaved(silence.data(), 2048, static_cast<double>(i) * 0.05);
    }

    std::vector<float> burst(4096 * 2, 0.0f);
    bool sawBeat = false;
    bool sawDownbeat = false;
    bool sawPhraseBoundary = false;
    float bestBarConfidence = 0.0f;
    float bestPhraseConfidence = 0.0f;
    float bestBuildTension = 0.0f;
    for (int beat = 0; beat < 10; ++beat) {
        analyzer.analyzeInterleaved(silence.data(), 2048, 1.0 + static_cast<double>(beat) * 0.5 - 0.08);
        for (int i = 0; i < 4096; ++i) {
            const float t = static_cast<float>(beat * 4096 + i) / 48000.0f;
            const float low = std::sin(2.0f * kPi * 88.0f * t) * 0.92f;
            const float click = (i < 320 ? 0.42f : 0.0f) * std::sin(2.0f * kPi * 1800.0f * t);
            burst[static_cast<std::size_t>(i) * 2U] = low + click;
            burst[(static_cast<std::size_t>(i) * 2U) + 1U] = low - click * 0.45f;
        }

        const AudioMetrics metrics = analyzer.analyzeInterleaved(burst.data(), 4096, 1.0 + static_cast<double>(beat) * 0.5);
        sawBeat = sawBeat || metrics.beat;
        sawDownbeat = sawDownbeat || metrics.downbeat;
        sawPhraseBoundary = sawPhraseBoundary || metrics.phraseBoundary;
        bestBarConfidence = std::max(bestBarConfidence, metrics.barConfidence);
        bestPhraseConfidence = std::max(bestPhraseConfidence, metrics.phraseConfidence);
        bestBuildTension = std::max(bestBuildTension, metrics.buildTension);
        require(metrics.barPhase >= 0.0f && metrics.barPhase <= 1.0f, "tracked bar phase should stay normalized");
        require(metrics.phrasePhase >= 0.0f && metrics.phrasePhase <= 1.0f,
                "tracked phrase phase should stay normalized");
    }

    require(sawBeat, "repeated low-frequency pulses should produce beat events");
    require(sawDownbeat, "bar tracker should mark at least one downbeat");
    require(sawPhraseBoundary, "phrase tracker should mark at least one phrase boundary");
    require(bestBarConfidence > 0.15f, "bar tracker should accumulate usable confidence");
    require(bestPhraseConfidence > 0.08f, "phrase tracker should accumulate usable confidence");
    require(bestBuildTension >= 0.0f && bestBuildTension <= 1.0f,
            "phrase tracker should keep build tension normalized");
}

void analyzerDetectsArrangementSections()
{
    AudioAnalyzer analyzer(48000, 2);
    std::vector<float> silence(2048 * 2, 0.0f);
    AudioMetrics metrics;
    for (int i = 0; i < 30; ++i) {
        metrics = analyzer.analyzeInterleaved(silence.data(), 2048, static_cast<double>(i) * 0.05);
    }
    require(metrics.section == ArrangementSection::Silence, "silence should report the Silence section");
    require(metrics.sectionConfidence > 0.9f, "silence section confidence should be high");

    std::vector<float> groove(2048 * 2, 0.0f);
    for (int frame = 0; frame < 24; ++frame) {
        for (int i = 0; i < 2048; ++i) {
            const float t = static_cast<float>(frame * 2048 + i) / 48000.0f;
            const float low = std::sin(2.0f * kPi * 118.0f * t) * 0.18f;
            const float mid = std::sin(2.0f * kPi * 880.0f * t) * 0.06f;
            groove[static_cast<std::size_t>(i) * 2U] = low + mid;
            groove[(static_cast<std::size_t>(i) * 2U) + 1U] = low - mid * 0.4f;
        }
        metrics = analyzer.analyzeInterleaved(groove.data(), 2048, 1.5 + static_cast<double>(frame) * 0.05);
    }
    require(metrics.section == ArrangementSection::Groove || metrics.section == ArrangementSection::Build,
            "steady musical content should leave silence for a musical section");
    require(metrics.sectionProgress >= 0.0f && metrics.sectionProgress <= 1.0f,
            "section progress should be normalized");
    require(metrics.phraseConfidence >= 0.0f && metrics.phraseConfidence <= 1.0f,
            "section analysis should keep phrase confidence normalized");
    require(metrics.buildTension >= 0.0f && metrics.buildTension <= 1.0f,
            "section analysis should keep build tension normalized");

    std::vector<float> burst(4096 * 2, 0.0f);
    for (int i = 0; i < 4096; ++i) {
        const float t = static_cast<float>(i) / 48000.0f;
        const float low = std::sin(2.0f * kPi * 92.0f * t) * 0.86f;
        const float high = std::sin(2.0f * kPi * 5100.0f * t) * 0.34f;
        burst[static_cast<std::size_t>(i) * 2U] = low + high;
        burst[(static_cast<std::size_t>(i) * 2U) + 1U] = low - high;
    }
    metrics = analyzer.analyzeInterleaved(burst.data(), 4096, 3.2);
    require(metrics.section == ArrangementSection::Drop || metrics.dropIntensity > 0.35f,
            "large lift after established history should report a drop-like section");
    require(metrics.sectionConfidence > 0.25f, "detected section should carry confidence");
}

} // namespace viz::tests
