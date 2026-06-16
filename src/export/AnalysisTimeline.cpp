#include "Visualizer/Export/AnalysisTimeline.hpp"

#include <algorithm>
#include <iomanip>

namespace viz {
namespace {

std::string keyLabel(const AudioMetrics& metrics)
{
    if (metrics.keyIndex < 0) {
        return "Unknown";
    }
    std::string label(keyName(metrics.keyIndex));
    label += " ";
    label += std::string(toString(metrics.keyMode));
    return label;
}

} // namespace

bool AnalysisTimelineWriter::open(const std::filesystem::path& path, std::string& error)
{
    close();
    error.clear();

    if (path.empty()) {
        error = "Timeline path is required.";
        return false;
    }

    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "Unable to create timeline directory: " + ec.message();
            return false;
        }
    }

    output_.open(path);
    if (!output_) {
        error = "Unable to write analysis timeline.";
        return false;
    }

    path_ = path;
    output_ << "frame,timeSeconds,mode,palette,motionStyle,hueShift,depth3D,objectDensity3D,interactionDepth,lightingGlow,"
            << "colorImpact,scenePersonality,response3D,motionStability,patternClarity,complexity,intensity,speed,qualityScale,"
            << "rms,peak,bass,lowMid,mid,highMid,treble,stereoWidth,spectralFlux,onset,"
            << "beat,beatConfidence,beatPhase,barPhase,barConfidence,downbeat,downbeatConfidence,bpm,dropIntensity,phraseIntensity,"
            << "phraseBoundary,phrasePhase,phraseConfidence,buildTension,"
            << "section,sectionConfidence,sectionProgress,style,styleConfidence,"
            << "styleAdaptation,syncAdaptation,beatSensitivity,sectionSensitivity,"
            << "key,keyConfidence,harmonicEnergy,"
            << "audioBassRole,audioDrumRole,audioMelodyRole,audioHarmonyRole,audioSpaceRole,audioFractureRole,"
            << "audioShadowRole,audioConvergenceRole,audioRoleSeparation,"
            << "scene3DName,sceneIntent,"
            << "authored2DPrimitiveCount,retained2DPrimitiveCount,projected3DPrimitiveCount,"
            << "projected3DFillVisualWeight,projected3DOutlineVisualWeight,projected3DMaterialShare,threeDDominance,"
            << "projected3DScreenCoverage,projected3DCenterOffset,foreground3DShare,midground3DShare,background3DShare,"
            << "cameraMotion3D,cameraContinuity3D,"
            << "sectionNarrative3D,sectionBuild3D,sectionDrop3D,sectionGroove3D,sectionBreakdown3D,sectionRelease3D,"
            << "sectionTransform3D,sectionDepthMotion3D,sectionMaterialShift3D,"
            << "songArc3D,songArcAnticipation3D,songArcImpact3D,songArcRecovery3D,songArcContinuity3D,"
            << "bassRole3D,drumRole3D,melodyRole3D,harmonyRole3D,spaceRole3D,fractureRole3D,shadowRole3D,convergence3D,roleSeparation3D,"
            << "explicitRoleShare3D,roleBridgeShare3D,roleCrosstalk3D,roleDistrictSpread3D,roleBalance3D,roleVocabulary3D,roleSilhouetteContrast3D,roleLegibility3D,roleMotionContrast3D,musicalStructure3D,"
            << "primitiveCount\n";
    if (!output_) {
        error = "Failed while writing timeline header.";
        close();
        return false;
    }
    return true;
}

bool AnalysisTimelineWriter::write(const AnalysisTimelineEntry& entry, std::string& error)
{
    error.clear();
    if (!output_) {
        error = "Analysis timeline is not open.";
        return false;
    }

    const AudioMetrics& metrics = entry.metrics;
    const VisualSettings& settings = entry.settings;
    output_ << entry.frameIndex << ","
            << std::fixed << std::setprecision(4) << entry.timeSeconds << ","
            << '"' << toString(settings.mode) << '"' << ","
            << '"' << toString(settings.palette) << '"' << ","
            << '"' << toString(settings.motionStyle) << '"' << ","
            << std::setprecision(3) << std::clamp(settings.hueShift, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.depth3D, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.objectDensity3D, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.interactionDepth, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.lightingGlow, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.colorImpact, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.scenePersonality, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.response3D, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.motionStability, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.patternClarity, 0.0f, 1.0f) << ","
            << std::setprecision(3) << std::clamp(settings.complexity, 0.35f, 1.8f) << ","
            << std::setprecision(3) << std::clamp(settings.intensity, 0.15f, 4.0f) << ","
            << std::setprecision(3) << std::clamp(settings.speed, 0.1f, 4.0f) << ","
            << std::setprecision(3) << std::clamp(settings.qualityScale, 0.45f, 1.0f) << ","
            << std::setprecision(5) << metrics.rms << ","
            << std::setprecision(5) << metrics.peak << ","
            << std::setprecision(5) << metrics.bass << ","
            << std::setprecision(5) << metrics.lowMid << ","
            << std::setprecision(5) << metrics.mid << ","
            << std::setprecision(5) << metrics.highMid << ","
            << std::setprecision(5) << metrics.treble << ","
            << std::setprecision(5) << metrics.stereoWidth << ","
            << std::setprecision(5) << metrics.spectralFlux << ","
            << std::setprecision(5) << metrics.onset << ","
            << (metrics.beat ? 1 : 0) << ","
            << std::setprecision(5) << metrics.beatConfidence << ","
            << std::setprecision(5) << metrics.beatPhase << ","
            << std::setprecision(5) << metrics.barPhase << ","
            << std::setprecision(5) << metrics.barConfidence << ","
            << (metrics.downbeat ? 1 : 0) << ","
            << std::setprecision(5) << metrics.downbeatConfidence << ","
            << std::setprecision(3) << metrics.bpm << ","
            << std::setprecision(5) << metrics.dropIntensity << ","
            << std::setprecision(5) << metrics.phraseIntensity << ","
            << (metrics.phraseBoundary ? 1 : 0) << ","
            << std::setprecision(5) << metrics.phrasePhase << ","
            << std::setprecision(5) << metrics.phraseConfidence << ","
            << std::setprecision(5) << metrics.buildTension << ","
            << '"' << toString(metrics.section) << '"' << ","
            << std::setprecision(5) << metrics.sectionConfidence << ","
            << std::setprecision(5) << metrics.sectionProgress << ","
            << '"' << toString(metrics.style) << '"' << ","
            << std::setprecision(5) << metrics.styleConfidence << ","
            << std::setprecision(5) << metrics.styleAdaptation << ","
            << std::setprecision(5) << metrics.syncAdaptation << ","
            << std::setprecision(5) << metrics.beatSensitivity << ","
            << std::setprecision(5) << metrics.sectionSensitivity << ","
            << '"' << keyLabel(metrics) << '"' << ","
            << std::setprecision(5) << metrics.keyConfidence << ","
            << std::setprecision(5) << metrics.harmonicEnergy << ","
            << std::setprecision(5) << metrics.bassRole << ","
            << std::setprecision(5) << metrics.drumRole << ","
            << std::setprecision(5) << metrics.melodyRole << ","
            << std::setprecision(5) << metrics.harmonyRole << ","
            << std::setprecision(5) << metrics.spaceRole << ","
            << std::setprecision(5) << metrics.fractureRole << ","
            << std::setprecision(5) << metrics.shadowRole << ","
            << std::setprecision(5) << metrics.convergenceRole << ","
            << std::setprecision(5) << metrics.roleSeparation << ","
            << '"' << entry.scene3DName << '"' << ","
            << '"' << entry.sceneIntentName << '"' << ","
            << entry.authored2DPrimitiveCount << ","
            << entry.retained2DPrimitiveCount << ","
            << entry.projected3DPrimitiveCount << ","
            << std::setprecision(3) << entry.projected3DFillVisualWeight << ","
            << std::setprecision(3) << entry.projected3DOutlineVisualWeight << ","
            << std::setprecision(3) << entry.projected3DMaterialShare << ","
            << std::setprecision(3) << entry.threeDDominance << ","
            << std::setprecision(3) << entry.projected3DScreenCoverage << ","
            << std::setprecision(3) << entry.projected3DCenterOffset << ","
            << std::setprecision(3) << entry.foreground3DShare << ","
            << std::setprecision(3) << entry.midground3DShare << ","
            << std::setprecision(3) << entry.background3DShare << ","
            << std::setprecision(3) << entry.cameraMotion3D << ","
            << std::setprecision(3) << entry.cameraContinuity3D << ","
            << std::setprecision(3) << entry.sectionNarrative3D << ","
            << std::setprecision(3) << entry.sectionBuild3D << ","
            << std::setprecision(3) << entry.sectionDrop3D << ","
            << std::setprecision(3) << entry.sectionGroove3D << ","
            << std::setprecision(3) << entry.sectionBreakdown3D << ","
            << std::setprecision(3) << entry.sectionRelease3D << ","
            << std::setprecision(3) << entry.sectionTransform3D << ","
            << std::setprecision(3) << entry.sectionDepthMotion3D << ","
            << std::setprecision(3) << entry.sectionMaterialShift3D << ","
            << std::setprecision(3) << entry.songArc3D << ","
            << std::setprecision(3) << entry.songArcAnticipation3D << ","
            << std::setprecision(3) << entry.songArcImpact3D << ","
            << std::setprecision(3) << entry.songArcRecovery3D << ","
            << std::setprecision(3) << entry.songArcContinuity3D << ","
            << std::setprecision(3) << entry.sceneBassRole3D << ","
            << std::setprecision(3) << entry.sceneDrumRole3D << ","
            << std::setprecision(3) << entry.sceneMelodyRole3D << ","
            << std::setprecision(3) << entry.sceneHarmonyRole3D << ","
            << std::setprecision(3) << entry.sceneSpaceRole3D << ","
            << std::setprecision(3) << entry.sceneFractureRole3D << ","
            << std::setprecision(3) << entry.sceneShadowRole3D << ","
            << std::setprecision(3) << entry.sceneConvergence3D << ","
            << std::setprecision(3) << entry.sceneRoleSeparation3D << ","
            << std::setprecision(3) << entry.sceneExplicitRoleShare3D << ","
            << std::setprecision(3) << entry.sceneRoleBridgeShare3D << ","
            << std::setprecision(3) << entry.sceneRoleCrosstalk3D << ","
            << std::setprecision(3) << entry.sceneRoleDistrictSpread3D << ","
            << std::setprecision(3) << entry.sceneRoleBalance3D << ","
            << std::setprecision(3) << entry.sceneRoleVocabulary3D << ","
            << std::setprecision(3) << entry.sceneRoleSilhouetteContrast3D << ","
            << std::setprecision(3) << entry.sceneRoleLegibility3D << ","
            << std::setprecision(3) << entry.sceneRoleMotionContrast3D << ","
            << std::setprecision(3) << entry.sceneMusicalStructure3D << ","
            << entry.primitiveCount << "\n";

    if (!output_) {
        error = "Failed while writing timeline row.";
        return false;
    }
    return true;
}

void AnalysisTimelineWriter::close()
{
    if (output_.is_open()) {
        output_.close();
    }
}

} // namespace viz
