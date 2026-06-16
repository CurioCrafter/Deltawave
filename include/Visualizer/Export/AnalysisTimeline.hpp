#pragma once

#include "Visualizer/Audio/AudioAnalyzer.hpp"
#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace viz {

struct AnalysisTimelineEntry {
    int frameIndex = 0;
    double timeSeconds = 0.0;
    AudioMetrics metrics{};
    VisualSettings settings{};
    int primitiveCount = 0;
    std::string_view scene3DName = "Flat Geometry";
    std::string_view sceneIntentName = "Calm";
    int authored2DPrimitiveCount = 0;
    int retained2DPrimitiveCount = 0;
    int projected3DPrimitiveCount = 0;
    float projected3DFillVisualWeight = 0.0f;
    float projected3DOutlineVisualWeight = 0.0f;
    float projected3DMaterialShare = 0.0f;
    float threeDDominance = 0.0f;
    float projected3DScreenCoverage = 0.0f;
    float projected3DCenterOffset = 0.0f;
    float foreground3DShare = 0.0f;
    float midground3DShare = 0.0f;
    float background3DShare = 0.0f;
    float cameraMotion3D = 0.0f;
    float cameraContinuity3D = 1.0f;
    float sectionNarrative3D = 0.0f;
    float sectionBuild3D = 0.0f;
    float sectionDrop3D = 0.0f;
    float sectionGroove3D = 0.0f;
    float sectionBreakdown3D = 0.0f;
    float sectionRelease3D = 0.0f;
    float sectionTransform3D = 0.0f;
    float sectionDepthMotion3D = 0.0f;
    float sectionMaterialShift3D = 0.0f;
    float songArc3D = 0.0f;
    float songArcAnticipation3D = 0.0f;
    float songArcImpact3D = 0.0f;
    float songArcRecovery3D = 0.0f;
    float songArcContinuity3D = 0.0f;
    float sceneBassRole3D = 0.0f;
    float sceneDrumRole3D = 0.0f;
    float sceneMelodyRole3D = 0.0f;
    float sceneHarmonyRole3D = 0.0f;
    float sceneSpaceRole3D = 0.0f;
    float sceneFractureRole3D = 0.0f;
    float sceneShadowRole3D = 0.0f;
    float sceneConvergence3D = 0.0f;
    float sceneRoleSeparation3D = 0.0f;
    float sceneExplicitRoleShare3D = 0.0f;
    float sceneRoleBridgeShare3D = 0.0f;
    float sceneRoleCrosstalk3D = 0.0f;
    float sceneRoleDistrictSpread3D = 0.0f;
    float sceneRoleBalance3D = 0.0f;
    float sceneRoleVocabulary3D = 0.0f;
    float sceneRoleSilhouetteContrast3D = 0.0f;
    float sceneRoleLegibility3D = 0.0f;
    float sceneRoleMotionContrast3D = 0.0f;
    float sceneMusicalStructure3D = 0.0f;
};

class AnalysisTimelineWriter {
public:
    bool open(const std::filesystem::path& path, std::string& error);
    bool write(const AnalysisTimelineEntry& entry, std::string& error);
    void close();

    [[nodiscard]] bool isOpen() const noexcept { return output_.is_open(); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
    std::ofstream output_;
};

} // namespace viz
