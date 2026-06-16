#include "Visualizer/Export/OfflineExporter.hpp"

#include "Visualizer/Audio/AudioAnalyzer.hpp"
#include "Visualizer/Audio/AudioFileLoader.hpp"
#include "Visualizer/Export/AnalysisTimeline.hpp"
#include "Visualizer/Export/SharePackage.hpp"
#include "Visualizer/Export/VideoEncoder.hpp"
#include "Visualizer/Performance/FramePerformanceTracker.hpp"
#include "Visualizer/Visualization/FrameRecorder.hpp"
#include "Visualizer/Visualization/SceneDirector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>

namespace viz {
namespace {

constexpr float kTrailPersistence = 0.84f;
constexpr std::size_t kTrackedStyleCount = 6;

std::size_t styleIndex(AudioStyle style)
{
    switch (style) {
    case AudioStyle::Silence:
        return 0;
    case AudioStyle::Ambient:
        return 1;
    case AudioStyle::Techno:
        return 2;
    case AudioStyle::BassHeavy:
        return 3;
    case AudioStyle::Bright:
        return 4;
    case AudioStyle::Wide:
        return 5;
    }
    return 0;
}

AudioStyle styleFromIndex(std::size_t index)
{
    switch (index) {
    case 1:
        return AudioStyle::Ambient;
    case 2:
        return AudioStyle::Techno;
    case 3:
        return AudioStyle::BassHeavy;
    case 4:
        return AudioStyle::Bright;
    case 5:
        return AudioStyle::Wide;
    default:
        return AudioStyle::Silence;
    }
}

bool writeManifest(const OfflineExportOptions& options,
                   const OfflineExportResult& result,
                   std::string& error)
{
    const std::filesystem::path manifestPath = result.outputDirectory / "export_manifest.txt";
    std::ofstream output(manifestPath);
    if (!output) {
        error = "Unable to write export manifest.";
        return false;
    }

    output << "Visualizer offline export\n";
    output << "input=" << options.inputAudio.string() << "\n";
    output << "output=" << result.outputDirectory.string() << "\n";
    output << "styleProfile=" << options.styleProfile.string() << "\n";
    output << "syncProfile=" << options.syncProfile.string() << "\n";
    output << "look=" << (options.lookName.empty() ? "Custom" : options.lookName) << "\n";
    output << "mode=" << toString(options.settings.mode) << "\n";
    output << "palette=" << toString(options.settings.palette) << "\n";
    output << "motionStyle=" << toString(options.settings.motionStyle) << "\n";
    output << "finalMotionStyle=" << toString(result.finalMotionStyle) << "\n";
    output << "hueShift=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.hueShift, 0.0f, 1.0f) << "\n";
    output << "finalHueShift=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalHueShift, 0.0f, 1.0f) << "\n";
    output << "depth3D=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.depth3D, 0.0f, 1.0f) << "\n";
    output << "finalDepth3D=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalDepth3D, 0.0f, 1.0f) << "\n";
    output << "colorImpact=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.colorImpact, 0.0f, 1.0f) << "\n";
    output << "finalColorImpact=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalColorImpact, 0.0f, 1.0f) << "\n";
    output << "objectDensity3D=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.objectDensity3D, 0.0f, 1.0f) << "\n";
    output << "finalObjectDensity3D=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalObjectDensity3D, 0.0f, 1.0f) << "\n";
    output << "interactionDepth=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.interactionDepth, 0.0f, 1.0f) << "\n";
    output << "finalInteractionDepth=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalInteractionDepth, 0.0f, 1.0f) << "\n";
    output << "lightingGlow=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.lightingGlow, 0.0f, 1.0f) << "\n";
    output << "finalLightingGlow=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalLightingGlow, 0.0f, 1.0f) << "\n";
    output << "scenePersonality=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.scenePersonality, 0.0f, 1.0f) << "\n";
    output << "finalScenePersonality=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalScenePersonality, 0.0f, 1.0f) << "\n";
    output << "response3D=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.response3D, 0.0f, 1.0f) << "\n";
    output << "finalResponse3D=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalResponse3D, 0.0f, 1.0f) << "\n";
    output << "motionStability=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.motionStability, 0.0f, 1.0f) << "\n";
    output << "finalMotionStability=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalMotionStability, 0.0f, 1.0f) << "\n";
    output << "patternClarity=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.patternClarity, 0.0f, 1.0f) << "\n";
    output << "finalPatternClarity=" << std::fixed << std::setprecision(3)
           << std::clamp(result.finalPatternClarity, 0.0f, 1.0f) << "\n";
    output << "minimumHueShift=" << std::fixed << std::setprecision(3)
           << std::clamp(result.minimumHueShift, 0.0f, 1.0f) << "\n";
    output << "maximumHueShift=" << std::fixed << std::setprecision(3)
           << std::clamp(result.maximumHueShift, 0.0f, 1.0f) << "\n";
    output << "complexity=" << std::fixed << std::setprecision(3)
           << std::clamp(options.settings.complexity, 0.35f, 1.8f) << "\n";
    output << "autoScene=" << (options.settings.autoScene || options.autoScene ? "true" : "false") << "\n";
    output << "environmentReactive=" << (options.settings.environmentReactive ? "true" : "false") << "\n";
    output << "environmentTimeOfDay=" << std::fixed << std::setprecision(3)
           << std::clamp(options.environmentTimeOfDay, 0.0f, 1.0f) << "\n";
    output << "trails=" << (options.settings.trails ? "true" : "false") << "\n";
    output << "width=" << options.width << "\n";
    output << "height=" << options.height << "\n";
    output << "frameRate=" << options.frameRate << "\n";
    output << "framesWritten=" << result.framesWritten << "\n";
    output << "durationSeconds=" << std::fixed << std::setprecision(3) << result.durationSeconds << "\n";
    output << "timeline=" << result.timelinePath.string() << "\n";
    output << "peakRms=" << std::setprecision(4) << result.peakRms << "\n";
    output << "estimatedBpm=" << std::setprecision(2) << result.estimatedBpm << "\n";
    output << "dominantSection=" << toString(result.dominantSection) << "\n";
    output << "sectionConfidence=" << std::setprecision(3) << result.sectionConfidence << "\n";
    if (result.detectedKeyIndex >= 0) {
        output << "detectedKey=" << keyName(result.detectedKeyIndex) << "\n";
        output << "detectedKeyMode=" << toString(result.detectedKeyMode) << "\n";
        output << "keyConfidence=" << std::setprecision(3) << result.keyConfidence << "\n";
    } else {
        output << "detectedKey=unknown\n";
        output << "detectedKeyMode=Unknown\n";
        output << "keyConfidence=0\n";
    }
    output << "beatsDetected=" << result.beatsDetected << "\n";
    output << "downbeatsDetected=" << result.trackSummary.downbeatsDetected << "\n";
    output << "phraseBoundariesDetected=" << result.trackSummary.phraseBoundariesDetected << "\n";
    output << "averageBarConfidence=" << std::fixed << std::setprecision(3)
           << result.trackSummary.averageBarConfidence << "\n";
    output << "averagePhraseConfidence=" << std::fixed << std::setprecision(3)
           << result.trackSummary.averagePhraseConfidence << "\n";
    output << "peakDownbeatConfidence=" << std::fixed << std::setprecision(3)
           << result.trackSummary.peakDownbeatConfidence << "\n";
    output << "peakDropIntensity=" << std::fixed << std::setprecision(3)
           << result.trackSummary.peakDropIntensity << "\n";
    output << "peakPhraseIntensity=" << std::fixed << std::setprecision(3)
           << result.trackSummary.peakPhraseIntensity << "\n";
    output << "peakBuildTension=" << std::fixed << std::setprecision(3)
           << result.trackSummary.peakBuildTension << "\n";
    output << "averageHarmonicEnergy=" << std::fixed << std::setprecision(3)
           << result.trackSummary.averageHarmonicEnergy << "\n";
    output << "dominantStyle=" << toString(result.trackSummary.dominantStyle) << "\n";
    output << "dominantStyleConfidence=" << std::fixed << std::setprecision(3)
           << result.trackSummary.dominantStyleConfidence << "\n";
    output << "maxPrimitiveCount=" << result.trackSummary.maxPrimitiveCount << "\n";
    output << "videoEncoded=" << (result.videoEncoded ? "true" : "false") << "\n";
    if (result.videoEncoded) {
        output << "outputVideo=" << result.outputVideo.string() << "\n";
        output << "videoBytes=" << result.videoBytes << "\n";
    }
    if (!result.previewImage.empty()) {
        output << "previewImage=" << result.previewImage.string() << "\n";
        output << "previewFrames=" << result.previewFramesUsed << "\n";
        output << "previewWidth=" << result.previewWidth << "\n";
        output << "previewHeight=" << result.previewHeight << "\n";
    }
    return true;
}

} // namespace

bool exportAudioToFrames(const OfflineExportOptions& options,
                         OfflineExportResult& result,
                         std::string& error)
{
    error.clear();
    result = {};

    if (options.inputAudio.empty()) {
        error = "Input audio path is required.";
        return false;
    }
    if (options.outputDirectory.empty()) {
        error = "Output directory is required.";
        return false;
    }
    if (options.width <= 0 || options.height <= 0) {
        error = "Export dimensions must be positive.";
        return false;
    }
    if (options.frameRate <= 0 || options.frameRate > 240) {
        error = "Frame rate must be between 1 and 240.";
        return false;
    }
    if (options.videoCrf < 0 || options.videoCrf > 51) {
        error = "Video CRF must be between 0 and 51.";
        return false;
    }

    std::optional<WavAudio> audio = loadAudioFile(options.inputAudio, error);
    if (!audio) {
        return false;
    }

    const double duration = options.maxSeconds > 0.0
                                ? std::min(options.maxSeconds, audio->durationSeconds)
                                : audio->durationSeconds;
    if (duration <= 0.0) {
        error = "Input audio has no exportable duration.";
        return false;
    }

    FrameRecorder recorder;
    if (!recorder.startSession(options.outputDirectory, options.width, options.height, error)) {
        return false;
    }

    AudioAnalyzer analyzer(audio->sampleRate, audio->channelCount);
    if (!options.styleProfile.empty() && !analyzer.loadStyleProfile(options.styleProfile, error)) {
        error = "Unable to load style profile: " + error;
        return false;
    }
    if (!options.syncProfile.empty() && !analyzer.loadSyncProfile(options.syncProfile, error)) {
        error = "Unable to load sync profile: " + error;
        return false;
    }
    VisualizerEngine engine;
    SceneDirector sceneDirector;
    AnalysisTimelineWriter timeline;
    VisualSettings baseSettings = options.settings;
    std::array<double, kTrackedStyleCount> styleScores{};
    double totalBarConfidence = 0.0;
    double totalPhraseConfidence = 0.0;
    double totalHarmonicEnergy = 0.0;
    int analyzedFrames = 0;
    if (options.autoScene) {
        baseSettings.autoScene = true;
    }
    const std::size_t channelCount = static_cast<std::size_t>(audio->channelCount);
    const std::size_t totalFrames = audio->samples.size() / channelCount;
    const int framesToWrite = std::max(1, static_cast<int>(std::ceil(duration * static_cast<double>(options.frameRate))));
    const std::filesystem::path timelinePath = recorder.sessionPath() / "analysis_timeline.csv";
    if (!timeline.open(timelinePath, error)) {
        return false;
    }

    for (int frameIndex = 0; frameIndex < framesToWrite; ++frameIndex) {
        const double timeSeconds = std::min(
            duration,
            (static_cast<double>(frameIndex) + 0.5) / static_cast<double>(options.frameRate));
        const std::size_t currentFrame = std::min<std::size_t>(
            totalFrames,
            static_cast<std::size_t>(timeSeconds * static_cast<double>(audio->sampleRate)));
        const std::size_t safeCurrent = std::min<std::size_t>(std::max<std::size_t>(1, currentFrame), totalFrames);
        const std::size_t analysisFrames = std::min<std::size_t>(2048, safeCurrent);
        const std::size_t startFrame = safeCurrent > analysisFrames ? safeCurrent - analysisFrames : 0;
        const std::size_t offset = startFrame * channelCount;

        const AudioMetrics metrics = analyzer.analyzeInterleaved(
            audio->samples.data() + offset,
            analysisFrames,
            timeSeconds);
        const VisualSettings renderSettings = sceneDirector.resolve(baseSettings, metrics, timeSeconds);
        EnvironmentState environment;
        environment.enabled = renderSettings.environmentReactive;
        environment.timeOfDay = std::clamp(options.environmentTimeOfDay, 0.0f, 1.0f);
        environment.motion = 0.0f;
        environment.ambient = 0.5f + 0.5f * std::sin((environment.timeOfDay * 2.0f * 3.14159265f) - 1.57079633f);
        const GeometryFrame geometry = engine.buildFrame(metrics,
                                                         renderSettings,
                                                         InteractionState{},
                                                         environment,
                                                         static_cast<float>(options.width),
                                                         static_cast<float>(options.height),
                                                         timeSeconds);
        const int primitiveCount = countPrimitives(geometry);
        FrameRenderOptions renderOptions;
        renderOptions.trails = renderSettings.trails;
        renderOptions.trailPersistence = kTrailPersistence;
        if (!recorder.writeFrame(geometry, renderOptions, error)) {
            return false;
        }
        if (!timeline.write(AnalysisTimelineEntry{
                frameIndex,
                timeSeconds,
                metrics,
                renderSettings,
                primitiveCount,
                geometry.scene3DName,
                geometry.sceneIntentName,
                geometry.authored2DPrimitiveCount,
                geometry.retained2DPrimitiveCount,
                geometry.projected3DPrimitiveCount,
                geometry.projected3DFillVisualWeight,
                geometry.projected3DOutlineVisualWeight,
                geometry.projected3DMaterialShare,
                geometry.threeDDominance,
                geometry.projected3DScreenCoverage,
                geometry.projected3DCenterOffset,
                geometry.foreground3DShare,
                geometry.midground3DShare,
                geometry.background3DShare,
                geometry.cameraMotion3D,
                geometry.cameraContinuity3D,
                geometry.sectionNarrative3D,
                geometry.sectionBuild3D,
                geometry.sectionDrop3D,
                geometry.sectionGroove3D,
                geometry.sectionBreakdown3D,
                geometry.sectionRelease3D,
                geometry.sectionTransform3D,
                geometry.sectionDepthMotion3D,
                geometry.sectionMaterialShift3D,
                geometry.songArc3D,
                geometry.songArcAnticipation3D,
                geometry.songArcImpact3D,
                geometry.songArcRecovery3D,
                geometry.songArcContinuity3D,
                geometry.sceneBassRole3D,
                geometry.sceneDrumRole3D,
                geometry.sceneMelodyRole3D,
                geometry.sceneHarmonyRole3D,
                geometry.sceneSpaceRole3D,
                geometry.sceneFractureRole3D,
                geometry.sceneShadowRole3D,
                geometry.sceneConvergence3D,
                geometry.sceneRoleSeparation3D,
                geometry.sceneExplicitRoleShare3D,
                geometry.sceneRoleBridgeShare3D,
                geometry.sceneRoleCrosstalk3D,
                geometry.sceneRoleDistrictSpread3D,
                geometry.sceneRoleBalance3D,
                geometry.sceneRoleVocabulary3D,
                geometry.sceneRoleSilhouetteContrast3D,
                geometry.sceneRoleLegibility3D,
                geometry.sceneRoleMotionContrast3D,
                geometry.sceneMusicalStructure3D
            },
            error)) {
            return false;
        }

        result.peakRms = std::max(result.peakRms, metrics.rms);
        result.finalHueShift = renderSettings.hueShift;
        result.finalDepth3D = renderSettings.depth3D;
        result.finalColorImpact = renderSettings.colorImpact;
        result.finalObjectDensity3D = renderSettings.objectDensity3D;
        result.finalInteractionDepth = renderSettings.interactionDepth;
        result.finalLightingGlow = renderSettings.lightingGlow;
        result.finalScenePersonality = renderSettings.scenePersonality;
        result.finalResponse3D = renderSettings.response3D;
        result.finalMotionStability = renderSettings.motionStability;
        result.finalPatternClarity = renderSettings.patternClarity;
        result.finalMotionStyle = renderSettings.motionStyle;
        result.minimumHueShift = std::min(result.minimumHueShift, renderSettings.hueShift);
        result.maximumHueShift = std::max(result.maximumHueShift, renderSettings.hueShift);
        if (metrics.beat) {
            ++result.beatsDetected;
        }
        if (metrics.downbeat) {
            ++result.trackSummary.downbeatsDetected;
        }
        if (metrics.phraseBoundary) {
            ++result.trackSummary.phraseBoundariesDetected;
        }
        totalBarConfidence += metrics.barConfidence;
        totalPhraseConfidence += metrics.phraseConfidence;
        totalHarmonicEnergy += metrics.harmonicEnergy;
        result.trackSummary.peakDownbeatConfidence = std::max(result.trackSummary.peakDownbeatConfidence,
                                                              metrics.downbeatConfidence);
        result.trackSummary.peakDropIntensity = std::max(result.trackSummary.peakDropIntensity,
                                                         metrics.dropIntensity);
        result.trackSummary.peakPhraseIntensity = std::max(result.trackSummary.peakPhraseIntensity,
                                                           metrics.phraseIntensity);
        result.trackSummary.peakBuildTension = std::max(result.trackSummary.peakBuildTension,
                                                        metrics.buildTension);
        result.trackSummary.maxPrimitiveCount = std::max(result.trackSummary.maxPrimitiveCount, primitiveCount);
        styleScores[styleIndex(metrics.style)] += std::max(0.01f, metrics.styleConfidence);
        ++analyzedFrames;
        if (metrics.bpm > 0.0f) {
            result.estimatedBpm = metrics.bpm;
        }
        if (metrics.keyConfidence > result.keyConfidence) {
            result.detectedKeyIndex = metrics.keyIndex;
            result.detectedKeyMode = metrics.keyMode;
            result.keyConfidence = metrics.keyConfidence;
        }
        if (metrics.sectionConfidence > result.sectionConfidence) {
            result.dominantSection = metrics.section;
            result.sectionConfidence = metrics.sectionConfidence;
        }
    }

    recorder.stop();
    timeline.close();
    result.outputDirectory = recorder.sessionPath();
    result.framesWritten = static_cast<int>(recorder.frameCount());
    result.durationSeconds = duration;
    result.timelinePath = timelinePath;
    result.timelineWritten = true;
    if (analyzedFrames > 0) {
        result.trackSummary.averageBarConfidence =
            static_cast<float>(totalBarConfidence / static_cast<double>(analyzedFrames));
        result.trackSummary.averagePhraseConfidence =
            static_cast<float>(totalPhraseConfidence / static_cast<double>(analyzedFrames));
        result.trackSummary.averageHarmonicEnergy =
            static_cast<float>(totalHarmonicEnergy / static_cast<double>(analyzedFrames));
        const auto dominantIt = std::max_element(styleScores.begin(), styleScores.end());
        const std::size_t dominantIndex = static_cast<std::size_t>(std::distance(styleScores.begin(), dominantIt));
        result.trackSummary.dominantStyle = styleFromIndex(dominantIndex);
        result.trackSummary.dominantStyleConfidence =
            std::clamp(static_cast<float>(*dominantIt / static_cast<double>(analyzedFrames)), 0.0f, 1.0f);
    }

    if (!options.outputVideo.empty()) {
        VideoEncodeOptions encodeOptions;
        encodeOptions.framesDirectory = result.outputDirectory;
        encodeOptions.outputMp4 = options.outputVideo;
        encodeOptions.ffmpegExecutable = options.ffmpegExecutable;
        encodeOptions.frameRate = options.frameRate;
        encodeOptions.crf = options.videoCrf;
        encodeOptions.preset = options.videoPreset;

        VideoEncodeResult encodeResult;
        if (!encodeFrameSequenceToMp4(encodeOptions, encodeResult, error)) {
            return false;
        }
        result.outputVideo = encodeResult.outputMp4;
        result.videoEncoded = true;
        result.videoBytes = encodeResult.bytesWritten;
    }

    if (options.sharePackage) {
        result.shareManifest = result.outputDirectory / "share_manifest.json";
        result.sharePage = result.outputDirectory / "index.html";
        if (!writeSharePackage(options, result, error)) {
            return false;
        }
        result.sharePackageGenerated = true;
    }

    if (!writeManifest(options, result, error)) {
        return false;
    }

    return true;
}

bool exportWavToFrames(const OfflineExportOptions& options,
                       OfflineExportResult& result,
                       std::string& error)
{
    return exportAudioToFrames(options, result, error);
}

} // namespace viz
