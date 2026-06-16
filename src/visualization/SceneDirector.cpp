#include "Visualizer/Visualization/SceneDirector.hpp"

#include <algorithm>
#include <cmath>

namespace viz {
namespace {

float clampSetting(float value, float minimum, float maximum)
{
    return std::clamp(value, minimum, maximum);
}

float wrapUnit(float value)
{
    value = std::fmod(value, 1.0f);
    if (value < 0.0f) {
        value += 1.0f;
    }
    return value;
}

float bpmSpeedScale(float bpm)
{
    if (bpm <= 1.0f) {
        return 1.0f;
    }
    return std::clamp(bpm / 128.0f, 0.72f, 1.55f);
}

float highTexture(const AudioMetrics& metrics)
{
    return metrics.highMid + metrics.treble;
}

float highBandOnset(const AudioMetrics& metrics)
{
    return std::max(metrics.bandOnsets[3], metrics.bandOnsets[4]);
}

float musicalRoleSum(const AudioMetrics& metrics)
{
    return metrics.bassRole +
           metrics.drumRole +
           metrics.melodyRole +
           metrics.harmonyRole +
           metrics.spaceRole +
           metrics.fractureRole +
           metrics.shadowRole +
           metrics.convergenceRole;
}

bool hasMusicalRoles(const AudioMetrics& metrics)
{
    return musicalRoleSum(metrics) > 0.015f || metrics.roleSeparation > 0.015f;
}

float melodicRole(const AudioMetrics& metrics)
{
    return std::max(metrics.melodyRole, metrics.harmonyRole);
}

float strongestMusicalRole(const AudioMetrics& metrics)
{
    return std::max({metrics.bassRole,
                     metrics.drumRole,
                     melodicRole(metrics),
                     metrics.spaceRole,
                     metrics.fractureRole,
                     metrics.shadowRole});
}

bool transientBreakCue(const AudioMetrics& metrics)
{
    const bool lockedTechno = metrics.style == AudioStyle::Techno &&
                              metrics.bass > 0.42f &&
                              metrics.beatConfidence > 0.62f;
    const bool wideBuild = metrics.style == AudioStyle::Wide &&
                           metrics.section == ArrangementSection::Build &&
                           metrics.stereoWidth > 0.55f;
    const bool harmonicBuild = metrics.section == ArrangementSection::Build &&
                               metrics.keyConfidence > 0.48f &&
                               metrics.harmonicEnergy > 0.56f;
    const bool barLockedHarmony = metrics.section == ArrangementSection::Groove &&
                                  metrics.downbeatConfidence > 0.70f &&
                                  metrics.barConfidence > 0.52f &&
                                  metrics.keyConfidence > 0.48f &&
                                  metrics.harmonicEnergy > 0.40f;
    return metrics.rms > 0.045f &&
           metrics.spectralFlux > 0.24f &&
           metrics.dropIntensity < 0.58f &&
           metrics.bass < 0.74f &&
           !lockedTechno &&
           !wideBuild &&
           !harmonicBuild &&
           !barLockedHarmony &&
           (metrics.onset > 0.14f || metrics.beatConfidence > 0.22f || metrics.section == ArrangementSection::Drop) &&
           (highTexture(metrics) > 0.045f || highBandOnset(metrics) > 0.20f);
}

bool melodicCue(const AudioMetrics& metrics)
{
    return metrics.rms > 0.075f &&
           metrics.dropIntensity < 0.38f &&
           metrics.bass < 0.28f &&
           metrics.harmonicEnergy > 0.30f &&
           metrics.keyConfidence > 0.08f &&
           metrics.spectralFlux < 0.30f;
}

bool spaciousCalmCue(const AudioMetrics& metrics)
{
    return (metrics.style == AudioStyle::Ambient || metrics.style == AudioStyle::Wide) &&
           metrics.dropIntensity < 0.36f &&
           metrics.bass < 0.42f &&
           metrics.spectralFlux < 0.26f &&
           metrics.beatConfidence < 0.56f;
}

bool softHarmonicFieldCue(const AudioMetrics& metrics)
{
    return metrics.rms > 0.07f &&
           metrics.rms < 0.30f &&
           metrics.dropIntensity < 0.32f &&
           metrics.spectralFlux < 0.20f &&
           metrics.beatConfidence < 0.42f &&
           metrics.harmonicEnergy > 0.46f &&
           (metrics.keyConfidence > 0.12f || metrics.stereoWidth > 0.48f);
}

bool structuralTechnoCue(const AudioMetrics& metrics)
{
    return metrics.style == AudioStyle::Techno &&
           metrics.bass > 0.34f &&
           metrics.dropIntensity < 0.48f &&
           metrics.spectralFlux < 0.38f &&
           (metrics.beatConfidence > 0.28f ||
            metrics.downbeatConfidence > 0.20f ||
            metrics.barConfidence > 0.40f);
}

bool darkMinimalCue(const AudioMetrics& metrics)
{
    const bool heavyDrop = metrics.style == AudioStyle::BassHeavy &&
                           (metrics.dropIntensity > 0.34f ||
                            (metrics.bass > 0.82f && metrics.beatConfidence > 0.58f));
    const bool lockedTechnoGroove = metrics.style == AudioStyle::Techno &&
                                    metrics.bass > 0.38f &&
                                    (metrics.downbeatConfidence > 0.20f ||
                                     metrics.barConfidence > 0.40f ||
                                     (metrics.beatConfidence > 0.68f && metrics.spectralFlux > 0.14f));
    const bool softHarmonicSpace = softHarmonicFieldCue(metrics) &&
                                   metrics.rms > 0.18f &&
                                   metrics.bass < 0.56f;
    const float darkBassFloor = metrics.style == AudioStyle::Techno ? 0.45f : 0.56f;
    return metrics.rms > 0.08f &&
           metrics.style != AudioStyle::Ambient &&
           metrics.bass > darkBassFloor &&
           highTexture(metrics) < metrics.bass * 0.12f + 0.006f &&
           metrics.spectralFlux < 0.42f &&
           metrics.dropIntensity < 0.50f &&
           !heavyDrop &&
           !lockedTechnoGroove &&
           !softHarmonicSpace;
}

struct SceneTarget {
    VisualMode mode = VisualMode::QuantumTunnel;
    Palette palette = Palette::NeonVoltage;
    MotionStyle motionStyle = MotionStyle::Liquid;
    float hueShift = 0.0f;
    float depth3D = 0.55f;
    float colorImpact = 0.65f;
    float objectDensity3D = 0.65f;
    float lightingGlow = 0.62f;
    float scenePersonality = 0.5f;
    float response3D = 0.88f;
    float motionStability = 0.72f;
    float patternClarity = 0.78f;
    float intensity = 1.0f;
    float speed = 1.0f;
};

struct ContinuityScores {
    float quiet = 0.0f;
    float ambient = 0.0f;
    float techno = 0.0f;
    float bass = 0.0f;
    float melodic = 0.0f;
    float broken = 0.0f;
    float dark = 0.0f;
};

ContinuityScores scoreContinuity(const AudioMetrics& metrics)
{
    const float styleWeight = std::clamp(metrics.styleConfidence, 0.0f, 1.0f);
    const float loudness = std::clamp(metrics.rms * 1.42f +
                                      metrics.peak * 0.10f +
                                      metrics.bass * 0.12f +
                                      metrics.lowMid * 0.07f,
                                      0.0f,
                                      1.0f);
    const float beatRegularity = std::clamp(metrics.beatConfidence * 0.46f +
                                            metrics.barConfidence * 0.32f +
                                            metrics.downbeatConfidence * 0.22f,
                                            0.0f,
                                            1.0f);
    const float texture = highTexture(metrics);
    const float transient = std::clamp(metrics.spectralFlux * 0.42f +
                                       metrics.onset * 0.30f +
                                       highBandOnset(metrics) * 0.28f,
                                       0.0f,
                                       1.0f);
    const bool softField = softHarmonicFieldCue(metrics) || spaciousCalmCue(metrics);
    const bool structuralTechno = structuralTechnoCue(metrics);
    const bool darkMinimal = darkMinimalCue(metrics);
    const bool breakCue = transientBreakCue(metrics);
    const bool melody = melodicCue(metrics);

    ContinuityScores scores;
    scores.quiet = std::clamp((1.0f - loudness) * 0.58f +
                                  (metrics.style == AudioStyle::Silence ? 0.42f + styleWeight * 0.18f : 0.0f) -
                                  metrics.beatConfidence * 0.20f,
                              0.0f,
                              1.0f);
    scores.ambient = std::clamp((metrics.style == AudioStyle::Ambient ? 0.34f + styleWeight * 0.18f : 0.0f) +
                                    (metrics.style == AudioStyle::Wide ? 0.26f + styleWeight * 0.14f : 0.0f) +
                                    metrics.stereoWidth * 0.28f +
                                    metrics.harmonicEnergy * 0.18f +
                                    (softField ? 0.30f : 0.0f) +
                                    (1.0f - transient) * 0.10f -
                                    metrics.dropIntensity * 0.30f -
                                    beatRegularity * 0.10f,
                                0.0f,
                                1.0f);
    scores.techno = std::clamp((metrics.style == AudioStyle::Techno ? 0.34f + styleWeight * 0.18f : 0.0f) +
                                   beatRegularity * 0.36f +
                                   metrics.bass * 0.16f +
                                   (structuralTechno ? 0.32f : 0.0f) -
                                   (softField && metrics.beatConfidence < 0.30f ? 0.18f : 0.0f) -
                                   (breakCue ? 0.18f : 0.0f),
                               0.0f,
                               1.0f);
    scores.bass = std::clamp((metrics.style == AudioStyle::BassHeavy ? 0.34f + styleWeight * 0.16f : 0.0f) +
                                 metrics.bass * 0.42f +
                                 metrics.dropIntensity * 0.30f +
                                 metrics.bandOnsets[0] * 0.12f -
                                 (darkMinimal ? 0.18f : 0.0f),
                             0.0f,
                             1.0f);
    scores.melodic = std::clamp((melody ? 0.42f : 0.0f) +
                                    metrics.harmonicEnergy * 0.30f +
                                    metrics.keyConfidence * 0.22f +
                                    (metrics.keyIndex >= 0 ? 0.08f : 0.0f) -
                                    metrics.bass * 0.16f -
                                    metrics.dropIntensity * 0.18f,
                                0.0f,
                                1.0f);
    scores.broken = std::clamp((breakCue ? 0.58f : 0.0f) +
                                   transient * 0.38f +
                                   (metrics.style == AudioStyle::Bright ? styleWeight * 0.16f : 0.0f) -
                                   metrics.bass * 0.10f,
                               0.0f,
                               1.0f);
    scores.dark = std::clamp((darkMinimal ? 0.60f : 0.0f) +
                                 metrics.bass * 0.18f +
                                 (1.0f - std::clamp(texture * 3.0f, 0.0f, 1.0f)) * 0.20f +
                                 (metrics.keyMode == MusicalMode::Minor ? metrics.keyConfidence * 0.16f : 0.0f) -
                                 (metrics.style == AudioStyle::Ambient ? 0.30f : 0.0f),
                             0.0f,
                             1.0f);

    if (hasMusicalRoles(metrics)) {
        const float melodyRoleScore = melodicRole(metrics);
        const float roleConfidence = std::clamp(metrics.roleSeparation * 0.72f +
                                                strongestMusicalRole(metrics) * 0.42f,
                                                0.0f,
                                                1.0f);
        scores.ambient = std::clamp(scores.ambient +
                                        (metrics.spaceRole * 0.48f + metrics.harmonyRole * 0.12f -
                                         metrics.bassRole * 0.16f - metrics.drumRole * 0.10f -
                                         metrics.fractureRole * 0.12f) *
                                            roleConfidence,
                                    0.0f,
                                    1.0f);
        scores.techno = std::clamp(scores.techno +
                                       (metrics.drumRole * 0.54f + metrics.convergenceRole * 0.10f -
                                        metrics.spaceRole * 0.14f - melodyRoleScore * 0.08f) *
                                           roleConfidence,
                                   0.0f,
                                   1.0f);
        scores.bass = std::clamp(scores.bass +
                                     (metrics.bassRole * 0.58f + metrics.convergenceRole * 0.12f -
                                      metrics.spaceRole * 0.12f - metrics.harmonyRole * 0.08f) *
                                         roleConfidence,
                                 0.0f,
                                 1.0f);
        scores.melodic = std::clamp(scores.melodic +
                                        (melodyRoleScore * 0.58f + metrics.spaceRole * 0.06f -
                                         metrics.bassRole * 0.14f - metrics.fractureRole * 0.08f) *
                                            roleConfidence,
                                    0.0f,
                                    1.0f);
        scores.broken = std::clamp(scores.broken +
                                       (metrics.fractureRole * 0.58f + metrics.convergenceRole * 0.18f -
                                        metrics.spaceRole * 0.10f - metrics.harmonyRole * 0.08f) *
                                           roleConfidence,
                                   0.0f,
                                   1.0f);
        scores.dark = std::clamp(scores.dark +
                                     (metrics.shadowRole * 0.58f + metrics.bassRole * 0.08f -
                                      metrics.spaceRole * 0.12f - metrics.fractureRole * 0.08f) *
                                         roleConfidence,
                                 0.0f,
                                 1.0f);
    }
    return scores;
}

void reinforce3DSettings(SceneTarget& target)
{
    target.depth3D = std::max(target.depth3D, 0.78f);
    target.objectDensity3D = std::max(target.objectDensity3D, 0.64f);
    target.lightingGlow = std::max(target.lightingGlow, 0.66f);
    target.scenePersonality = std::max(target.scenePersonality, 0.72f);
    target.response3D = std::max(target.response3D, 0.72f);
    target.patternClarity = std::max(target.patternClarity, 0.86f);
}

float harmonicHueTarget(const VisualSettings& base, const AudioMetrics& metrics)
{
    const float confidence = std::clamp((metrics.keyConfidence - 0.18f) / 0.42f, 0.0f, 1.0f);
    const float harmonicWeight = std::clamp(metrics.harmonicEnergy * 1.25f, 0.0f, 1.0f);
    const float weight = confidence * harmonicWeight;
    if (metrics.keyIndex < 0 || weight <= 0.001f) {
        return wrapUnit(base.hueShift);
    }

    const float rootHue = static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f;
    const float modeBias = metrics.keyMode == MusicalMode::Minor ? 0.075f :
                           (metrics.keyMode == MusicalMode::Major ? -0.025f : 0.0f);
    const float target = wrapUnit(base.hueShift + rootHue + modeBias);
    float delta = target - wrapUnit(base.hueShift);
    if (delta > 0.5f) {
        delta -= 1.0f;
    } else if (delta < -0.5f) {
        delta += 1.0f;
    }
    return wrapUnit(base.hueShift + delta * weight);
}

void applyRoleDirectedTarget(SceneTarget& target, const AudioMetrics& metrics)
{
    if (!hasMusicalRoles(metrics) ||
        (metrics.style == AudioStyle::Silence && metrics.rms < 0.035f && metrics.peak < 0.08f)) {
        return;
    }

    const float roleConfidence = std::clamp(metrics.roleSeparation * 0.70f +
                                            strongestMusicalRole(metrics) * 0.42f,
                                            0.0f,
                                            1.0f);
    if (roleConfidence < 0.26f) {
        return;
    }

    const float melody = melodicRole(metrics);
    const float bass = metrics.bassRole;
    const float drums = metrics.drumRole;
    const float space = metrics.spaceRole;
    const float fracture = metrics.fractureRole;
    const float shadow = metrics.shadowRole;
    const float convergence = metrics.convergenceRole;

    const auto liftRole3D = [&]() {
        target.depth3D = std::max(target.depth3D, 0.78f + roleConfidence * 0.16f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.64f + roleConfidence * 0.18f);
        target.lightingGlow = std::max(target.lightingGlow, 0.64f + roleConfidence * 0.18f);
        target.scenePersonality = std::max(target.scenePersonality, 0.72f + roleConfidence * 0.16f);
        target.response3D = std::max(target.response3D, 0.74f + roleConfidence * 0.18f);
        target.patternClarity = std::max(target.patternClarity, 0.86f);
    };

    if (fracture > 0.42f &&
        fracture > space + 0.08f &&
        fracture > melody + 0.04f) {
        target.mode = VisualMode::SpectralOrigami;
        target.palette = Palette::AcidAurora;
        target.motionStyle = MotionStyle::Breakbeat;
        target.colorImpact = std::max(target.colorImpact, 0.86f);
        target.motionStability = std::max(target.motionStability, 0.80f);
        target.intensity *= 1.02f + fracture * 0.22f + convergence * 0.12f;
        target.speed *= 0.96f + fracture * 0.20f;
        liftRole3D();
        return;
    }

    if ((bass > 0.42f || (bass > 0.34f && convergence > 0.28f)) &&
        bass > space + 0.12f &&
        bass > melody + 0.10f) {
        target.mode = convergence > 0.56f && metrics.stereoWidth > 0.50f
                          ? VisualMode::HyperspacePolytope
                          : VisualMode::QuantumTunnel;
        target.palette = Palette::InfraredChrome;
        target.motionStyle = MotionStyle::HeavyBass;
        target.colorImpact = std::max(target.colorImpact, 0.78f + bass * 0.12f);
        target.motionStability = std::max(target.motionStability, 0.84f);
        target.intensity *= 1.04f + bass * 0.30f + convergence * 0.18f;
        target.speed *= 0.88f + std::max(metrics.beatConfidence, drums) * 0.22f;
        liftRole3D();
        return;
    }

    if (shadow > 0.36f &&
        shadow > space + 0.08f &&
        shadow > fracture - 0.02f) {
        target.mode = metrics.keyConfidence > 0.24f ? VisualMode::ResonanceTessellation : VisualMode::FractalCathedral;
        target.palette = Palette::MonochromeLaser;
        target.motionStyle = MotionStyle::Smooth;
        target.colorImpact = std::min(target.colorImpact, 0.64f);
        target.objectDensity3D = std::clamp(target.objectDensity3D, 0.42f, 0.68f);
        target.motionStability = std::max(target.motionStability, 0.92f);
        target.speed *= 0.72f;
        liftRole3D();
        return;
    }

    if (drums > 0.34f &&
        drums > space + 0.08f &&
        drums > melody + 0.06f &&
        fracture < drums + 0.18f) {
        target.mode = metrics.downbeatConfidence > 0.50f || metrics.barConfidence > 0.58f || drums > 0.62f
                          ? VisualMode::TechnoMandala
                          : VisualMode::PolyrhythmLattice;
        target.palette = Palette::NeonVoltage;
        target.motionStyle = MotionStyle::Mechanical;
        target.motionStability = std::max(target.motionStability, 0.86f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.76f);
        target.intensity *= 1.0f + drums * 0.16f + convergence * 0.10f;
        target.speed *= 0.94f + bpmSpeedScale(metrics.bpm) * 0.14f + drums * 0.10f;
        liftRole3D();
        return;
    }

    if (melody > 0.30f &&
        melody > bass + 0.08f &&
        melody > fracture + 0.02f) {
        target.mode = metrics.harmonyRole > metrics.melodyRole + 0.08f && metrics.keyConfidence > 0.34f
                          ? VisualMode::ResonanceTessellation
                          : VisualMode::ChromaKaleidoscope;
        target.palette = Palette::AcidAurora;
        target.motionStyle = MotionStyle::Liquid;
        target.colorImpact = std::max(target.colorImpact, 0.84f);
        target.motionStability = std::max(target.motionStability, 0.84f);
        target.intensity *= 0.96f + melody * 0.18f + metrics.harmonyRole * 0.12f;
        target.speed *= 0.86f + melody * 0.16f;
        liftRole3D();
        return;
    }

    if (space > 0.34f &&
        space > bass + 0.10f &&
        space > drums + 0.08f &&
        space > fracture + 0.08f) {
        target.mode = VisualMode::PhaseWeave;
        target.palette = Palette::OceanicPulse;
        target.motionStyle = MotionStyle::AmbientDrift;
        target.colorImpact = std::max(target.colorImpact, 0.62f + metrics.harmonyRole * 0.10f);
        target.motionStability = std::max(target.motionStability, 0.90f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.58f + space * 0.14f);
        target.intensity *= 0.78f + space * 0.18f;
        target.speed *= 0.64f + space * 0.18f;
        liftRole3D();
    }
}

SceneTarget targetFor(const VisualSettings& base, const AudioMetrics& metrics)
{
    SceneTarget target;
    target.mode = base.mode;
    target.palette = base.palette;
    target.motionStyle = base.motionStyle;
    target.hueShift = wrapUnit(base.hueShift);
    target.depth3D = base.depth3D;
    target.colorImpact = base.colorImpact;
    target.objectDensity3D = base.objectDensity3D;
    target.lightingGlow = base.lightingGlow;
    target.scenePersonality = base.scenePersonality;
    target.response3D = base.response3D;
    target.motionStability = base.motionStability;
    target.patternClarity = base.patternClarity;
    target.intensity = base.intensity;
    target.speed = base.speed;

    const float bpmScale = bpmSpeedScale(metrics.bpm);
    const bool hasBreakCue = transientBreakCue(metrics);
    const bool hasMelodicCue = melodicCue(metrics);
    const bool hasSpaciousCalmCue = spaciousCalmCue(metrics);
    const bool hasSoftHarmonicFieldCue = softHarmonicFieldCue(metrics);
    const bool hasStructuralTechnoCue = structuralTechnoCue(metrics);
    const bool hasDarkMinimalCue = darkMinimalCue(metrics);
    const float phraseBuild = std::clamp(metrics.buildTension * metrics.phraseConfidence, 0.0f, 1.0f);
    const float energy = std::clamp(metrics.rms * 2.0f +
                                    metrics.beatConfidence * 0.45f +
                                    phraseBuild * 0.28f,
                                    0.0f,
                                    1.8f);

    switch (metrics.style) {
    case AudioStyle::Silence:
        target.mode = base.mode;
        target.palette = Palette::MonochromeLaser;
        target.motionStyle = MotionStyle::Smooth;
        target.depth3D = base.depth3D * 0.55f;
        target.colorImpact = base.colorImpact * 0.45f;
        target.objectDensity3D = std::min(base.objectDensity3D, 0.42f);
        target.lightingGlow = std::min(base.lightingGlow, 0.38f);
        target.scenePersonality = std::min(base.scenePersonality, 0.34f);
        target.response3D = std::min(base.response3D, 0.40f);
        target.motionStability = std::max(base.motionStability, 0.90f);
        target.patternClarity = std::max(base.patternClarity, 0.92f);
        target.intensity = base.intensity * 0.48f;
        target.speed = base.speed * 0.62f;
        break;
    case AudioStyle::Ambient:
        target.mode = hasMelodicCue
                          ? VisualMode::ChromaKaleidoscope
                          : (metrics.stereoWidth > 0.50f || metrics.harmonicEnergy > 0.42f
                                 ? VisualMode::PhaseWeave
                                 : VisualMode::LissajousMesh);
        target.palette = Palette::OceanicPulse;
        target.motionStyle = MotionStyle::AmbientDrift;
        target.depth3D = std::max(base.depth3D, 0.62f + metrics.stereoWidth * 0.16f);
        target.colorImpact = std::max(base.colorImpact, 0.52f + metrics.harmonicEnergy * 0.16f);
        target.objectDensity3D = std::max(base.objectDensity3D, 0.56f + metrics.stereoWidth * 0.14f);
        target.lightingGlow = std::max(base.lightingGlow, 0.58f + metrics.harmonicEnergy * 0.14f);
        target.scenePersonality = std::max(base.scenePersonality, 0.58f + metrics.stereoWidth * 0.12f);
        target.response3D = std::max(base.response3D, 0.62f + metrics.phraseIntensity * 0.16f);
        target.motionStability = std::max(base.motionStability, 0.84f);
        target.patternClarity = std::max(base.patternClarity, 0.86f);
        target.intensity = base.intensity * (0.72f + metrics.phraseIntensity * 0.55f);
        target.speed = base.speed * (0.62f + metrics.stereoWidth * 0.32f);
        break;
    case AudioStyle::Techno:
        target.mode = metrics.spectralFlux > 0.54f && metrics.stereoWidth > 0.34f
                          ? VisualMode::HyperspacePolytope
                          : ((metrics.phraseIntensity > 0.38f ||
                              metrics.buildTension > 0.32f ||
                              metrics.beatConfidence > 0.78f)
                                 ? VisualMode::TechnoMandala
                                 : VisualMode::PolyrhythmLattice);
        target.palette = metrics.treble > 0.42f ? Palette::AcidAurora : Palette::NeonVoltage;
        target.motionStyle = metrics.spectralFlux > 0.52f ? MotionStyle::Breakbeat : MotionStyle::Mechanical;
        target.depth3D = std::max(base.depth3D, 0.68f + metrics.stereoWidth * 0.18f + metrics.dropIntensity * 0.12f);
        target.colorImpact = std::max(base.colorImpact, 0.76f + metrics.treble * 0.12f + metrics.dropIntensity * 0.08f);
        target.objectDensity3D = std::max(base.objectDensity3D, 0.74f + metrics.beatConfidence * 0.10f);
        target.lightingGlow = std::max(base.lightingGlow, 0.68f + metrics.beatConfidence * 0.10f);
        target.scenePersonality = std::max(base.scenePersonality, 0.70f + metrics.spectralFlux * 0.12f);
        target.response3D = std::max(base.response3D, 0.78f + metrics.beatConfidence * 0.12f);
        target.motionStability = std::max(base.motionStability, 0.78f);
        target.patternClarity = std::max(base.patternClarity, 0.84f);
        target.intensity = base.intensity * (1.08f + energy * 0.5f + metrics.dropIntensity * 0.42f);
        target.speed = base.speed * (0.92f + bpmScale * 0.28f + metrics.beatConfidence * 0.22f);
        break;
    case AudioStyle::BassHeavy:
        target.mode = (metrics.dropIntensity > 0.34f ||
                       metrics.bass > 0.62f ||
                       metrics.bandOnsets[0] > 0.38f ||
                       metrics.section == ArrangementSection::Drop)
                          ? VisualMode::QuantumTunnel
                          : VisualMode::PolyrhythmLattice;
        target.palette = Palette::InfraredChrome;
        target.motionStyle = MotionStyle::HeavyBass;
        target.depth3D = std::max(base.depth3D, 0.78f + metrics.bass * 0.18f + metrics.dropIntensity * 0.12f);
        target.colorImpact = std::max(base.colorImpact, 0.72f + metrics.bass * 0.12f);
        target.objectDensity3D = std::max(base.objectDensity3D, 0.72f + metrics.dropIntensity * 0.16f);
        target.lightingGlow = std::max(base.lightingGlow, 0.72f + metrics.dropIntensity * 0.16f);
        target.scenePersonality = std::max(base.scenePersonality, 0.72f + metrics.bass * 0.16f);
        target.response3D = std::max(base.response3D, 0.84f + metrics.dropIntensity * 0.12f);
        target.motionStability = std::max(base.motionStability, 0.80f);
        target.patternClarity = std::max(base.patternClarity, 0.82f);
        target.intensity = base.intensity * (1.2f + metrics.bass * 0.78f + metrics.dropIntensity * 0.55f);
        target.speed = base.speed * (0.9f + bpmScale * 0.22f + metrics.onset * 0.36f);
        break;
    case AudioStyle::Bright:
        target.mode = (metrics.spectralFlux > 0.28f ||
                       metrics.onset > 0.34f ||
                       metrics.bandOnsets[4] > 0.32f)
                          ? VisualMode::SpectralOrigami
                          : (metrics.harmonicEnergy > 0.56f && metrics.keyConfidence > 0.46f
                                 ? VisualMode::ChromaKaleidoscope
                                 : VisualMode::FrequencyBloom);
        target.palette = Palette::AcidAurora;
        target.motionStyle = metrics.onset > 0.42f ? MotionStyle::Breakbeat : MotionStyle::Liquid;
        target.depth3D = std::max(base.depth3D, 0.58f + metrics.spectralFlux * 0.16f);
        target.colorImpact = std::max(base.colorImpact, 0.84f + metrics.treble * 0.12f);
        target.objectDensity3D = std::max(base.objectDensity3D, 0.68f + metrics.spectralFlux * 0.16f);
        target.lightingGlow = std::max(base.lightingGlow, 0.78f + metrics.treble * 0.14f);
        target.scenePersonality = std::max(base.scenePersonality, 0.70f + metrics.onset * 0.12f);
        target.response3D = std::max(base.response3D, 0.76f + metrics.onset * 0.14f);
        target.motionStability = std::max(base.motionStability, metrics.onset > 0.42f ? 0.76f : 0.82f);
        target.patternClarity = std::max(base.patternClarity, 0.86f);
        target.intensity = base.intensity * (0.95f + metrics.treble * 0.75f + metrics.spectralFlux * 0.45f);
        target.speed = base.speed * (0.95f + bpmScale * 0.18f + metrics.highMid * 0.3f);
        break;
    case AudioStyle::Wide:
        target.mode = metrics.spectralFlux > 0.36f && metrics.stereoWidth > 0.42f ? VisualMode::PhaseWeave :
                      (metrics.treble > 0.42f ? VisualMode::SpectralOrigami :
                       (metrics.phraseIntensity > 0.35f || metrics.harmonicEnergy > 0.46f
                            ? VisualMode::PhaseWeave
                            : VisualMode::LissajousMesh));
        target.palette = Palette::OceanicPulse;
        target.motionStyle = MotionStyle::Liquid;
        target.depth3D = std::max(base.depth3D, 0.76f + metrics.stereoWidth * 0.2f);
        target.colorImpact = std::max(base.colorImpact, 0.62f + metrics.stereoWidth * 0.12f + metrics.harmonicEnergy * 0.1f);
        target.objectDensity3D = std::max(base.objectDensity3D, 0.62f + metrics.stereoWidth * 0.18f);
        target.lightingGlow = std::max(base.lightingGlow, 0.62f + metrics.harmonicEnergy * 0.16f);
        target.scenePersonality = std::max(base.scenePersonality, 0.66f + metrics.stereoWidth * 0.18f);
        target.response3D = std::max(base.response3D, 0.70f + metrics.stereoWidth * 0.16f);
        target.motionStability = std::max(base.motionStability, 0.82f);
        target.patternClarity = std::max(base.patternClarity, 0.84f);
        target.intensity = base.intensity * (0.92f + metrics.stereoWidth * 0.52f + metrics.phraseIntensity * 0.3f);
        target.speed = base.speed * (0.82f + bpmScale * 0.2f + metrics.stereoWidth * 0.26f);
        break;
    }

    if (hasMelodicCue) {
        target.mode = metrics.harmonicEnergy > 0.46f ? VisualMode::ChromaKaleidoscope : VisualMode::FrequencyBloom;
        target.palette = Palette::AcidAurora;
        target.motionStyle = MotionStyle::Liquid;
        target.depth3D = std::max(target.depth3D, 0.72f + metrics.harmonicEnergy * 0.12f);
        target.colorImpact = std::max(target.colorImpact, 0.80f + metrics.harmonicEnergy * 0.12f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.68f + metrics.harmonicEnergy * 0.12f);
        target.lightingGlow = std::max(target.lightingGlow, 0.76f + metrics.harmonicEnergy * 0.14f);
        target.scenePersonality = std::max(target.scenePersonality, 0.76f + metrics.keyConfidence * 0.12f);
        target.response3D = std::max(target.response3D, 0.74f + metrics.phraseIntensity * 0.10f);
        target.motionStability = std::max(target.motionStability, 0.84f);
        target.patternClarity = std::max(target.patternClarity, 0.88f);
    }

    if (hasSoftHarmonicFieldCue && !hasMelodicCue) {
        target.mode = metrics.stereoWidth > 0.44f ? VisualMode::PhaseWeave : VisualMode::LissajousMesh;
        target.palette = Palette::OceanicPulse;
        target.motionStyle = MotionStyle::AmbientDrift;
        target.depth3D = std::max(target.depth3D, 0.80f + metrics.stereoWidth * 0.08f);
        target.colorImpact = std::max(target.colorImpact, 0.64f + metrics.harmonicEnergy * 0.12f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.60f + metrics.harmonicEnergy * 0.08f);
        target.lightingGlow = std::max(target.lightingGlow, 0.64f + metrics.harmonicEnergy * 0.12f);
        target.scenePersonality = std::max(target.scenePersonality, 0.72f);
        target.response3D = std::max(target.response3D, 0.66f + metrics.phraseIntensity * 0.10f);
        target.motionStability = std::max(target.motionStability, 0.90f);
        target.patternClarity = std::max(target.patternClarity, 0.90f);
        target.intensity *= 0.82f + metrics.harmonicEnergy * 0.18f;
        target.speed *= 0.68f + metrics.stereoWidth * 0.16f;
    }

    if (hasStructuralTechnoCue && !hasBreakCue && !hasDarkMinimalCue) {
        target.mode = metrics.downbeatConfidence > 0.54f || metrics.beatConfidence > 0.66f
                          ? VisualMode::TechnoMandala
                          : VisualMode::PolyrhythmLattice;
        target.palette = metrics.treble > 0.34f ? Palette::AcidAurora : Palette::NeonVoltage;
        target.motionStyle = MotionStyle::Mechanical;
        target.depth3D = std::max(target.depth3D, 0.76f + metrics.bass * 0.08f);
        target.colorImpact = std::max(target.colorImpact, 0.76f + metrics.beatConfidence * 0.08f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.76f + metrics.barConfidence * 0.10f);
        target.lightingGlow = std::max(target.lightingGlow, 0.70f + metrics.downbeatConfidence * 0.10f);
        target.scenePersonality = std::max(target.scenePersonality, 0.78f);
        target.response3D = std::max(target.response3D, 0.82f + metrics.beatConfidence * 0.08f);
        target.motionStability = std::max(target.motionStability, 0.84f);
        target.patternClarity = std::max(target.patternClarity, 0.88f);
        target.intensity *= 1.02f + metrics.barConfidence * 0.12f + metrics.beatConfidence * 0.08f;
        target.speed *= 0.96f + bpmSpeedScale(metrics.bpm) * 0.18f;
    }

    if (metrics.keyConfidence > 0.2f) {
        target.hueShift = harmonicHueTarget(base, metrics);
    }

    if (metrics.keyConfidence > 0.48f) {
        target.intensity *= 1.0f + metrics.harmonicEnergy * 0.12f;
        if (metrics.harmonicEnergy > 0.62f &&
            (metrics.stereoWidth > 0.24f || metrics.phraseIntensity > 0.28f || metrics.spectralFlux > 0.24f)) {
            target.mode = VisualMode::ChromaKaleidoscope;
        }
        target.colorImpact = std::max(target.colorImpact, 0.66f + metrics.harmonicEnergy * 0.26f);
        if (metrics.keyMode == MusicalMode::Minor && target.palette == Palette::AcidAurora) {
            target.palette = Palette::InfraredChrome;
        } else if (metrics.keyMode == MusicalMode::Major && metrics.harmonicEnergy > 0.52f &&
                   target.palette == Palette::MonochromeLaser) {
            target.palette = Palette::NeonVoltage;
        }
    }

    switch (metrics.section) {
    case ArrangementSection::Silence:
        break;
    case ArrangementSection::Breakdown:
        if (metrics.sectionConfidence > 0.42f) {
            target.mode = hasDarkMinimalCue
                              ? VisualMode::FractalCathedral
                              : (metrics.harmonicEnergy > 0.48f ? VisualMode::ChromaKaleidoscope : VisualMode::FractalCathedral);
            target.palette = Palette::OceanicPulse;
            target.motionStyle = MotionStyle::AmbientDrift;
            target.depth3D = std::max(target.depth3D, 0.68f + metrics.stereoWidth * 0.12f);
            target.colorImpact = std::max(target.colorImpact, 0.58f + metrics.harmonicEnergy * 0.18f);
            target.objectDensity3D = std::min(target.objectDensity3D, 0.62f + metrics.harmonicEnergy * 0.10f);
            target.lightingGlow = std::max(target.lightingGlow, 0.58f + metrics.harmonicEnergy * 0.16f);
            target.scenePersonality = std::max(target.scenePersonality, 0.56f + metrics.sectionProgress * 0.10f);
            target.response3D = std::min(target.response3D, 0.74f + metrics.phraseIntensity * 0.10f);
            target.motionStability = std::max(target.motionStability, 0.88f);
            target.patternClarity = std::max(target.patternClarity, 0.90f);
            target.intensity *= 0.72f + metrics.sectionProgress * 0.18f;
            target.speed *= 0.68f + metrics.stereoWidth * 0.18f;
        }
        break;
    case ArrangementSection::Build:
        if (metrics.sectionConfidence > 0.42f) {
            if (hasBreakCue) {
                target.mode = VisualMode::SpectralOrigami;
            } else if (metrics.keyConfidence > 0.48f && metrics.harmonicEnergy > 0.56f &&
                (metrics.spectralFlux > 0.34f || metrics.bandOnsets[2] > 0.36f || metrics.bandOnsets[3] > 0.36f)) {
                target.mode = VisualMode::ResonanceTessellation;
            } else {
                target.mode = metrics.stereoWidth > 0.5f && metrics.spectralFlux > 0.46f
                                  ? VisualMode::PhaseWeave
                                  : (metrics.stereoWidth > 0.38f || metrics.spectralFlux > 0.52f
                                         ? VisualMode::HyperspacePolytope
                                         : VisualMode::TechnoMandala);
            }
            target.palette = metrics.treble > 0.32f ? Palette::AcidAurora : target.palette;
            target.motionStyle = metrics.stereoWidth > 0.42f ? MotionStyle::Hyperspace : MotionStyle::Mechanical;
            target.depth3D = std::max(target.depth3D, 0.72f + metrics.sectionProgress * 0.12f + metrics.buildTension * 0.14f);
            target.colorImpact = std::max(target.colorImpact, 0.74f + metrics.buildTension * 0.12f + metrics.treble * 0.08f);
            target.objectDensity3D = std::max(target.objectDensity3D, 0.70f + metrics.sectionProgress * 0.14f);
            target.lightingGlow = std::max(target.lightingGlow, 0.70f + metrics.buildTension * 0.18f);
            target.scenePersonality = std::max(target.scenePersonality, 0.72f + metrics.buildTension * 0.16f);
            target.response3D = std::max(target.response3D, 0.78f + metrics.buildTension * 0.16f);
            target.motionStability = std::max(target.motionStability, 0.80f);
            target.patternClarity = std::max(target.patternClarity, 0.84f);
            target.intensity *= 1.0f + metrics.sectionProgress * 0.32f + metrics.buildTension * 0.24f + metrics.phraseIntensity * 0.18f;
            target.speed *= 1.0f + metrics.sectionProgress * 0.16f + metrics.buildTension * 0.16f;
        }
        break;
    case ArrangementSection::Drop:
        if (metrics.sectionConfidence > 0.45f) {
            if (metrics.keyConfidence > 0.5f && metrics.harmonicEnergy > 0.54f && metrics.spectralFlux > 0.34f) {
                target.mode = VisualMode::ResonanceTessellation;
            } else {
                target.mode = metrics.stereoWidth > 0.46f || metrics.spectralFlux > 0.42f
                                  ? VisualMode::HyperspacePolytope
                                  : VisualMode::QuantumTunnel;
            }
            target.palette = metrics.treble > 0.4f ? Palette::AcidAurora : Palette::NeonVoltage;
            target.motionStyle = metrics.bass > 0.58f ? MotionStyle::HeavyBass : MotionStyle::Hyperspace;
            target.depth3D = std::max(target.depth3D, 0.86f + metrics.dropIntensity * 0.1f);
            target.colorImpact = std::max(target.colorImpact, 0.82f + metrics.dropIntensity * 0.1f);
            target.objectDensity3D = std::max(target.objectDensity3D, 0.82f + metrics.dropIntensity * 0.10f);
            target.lightingGlow = std::max(target.lightingGlow, 0.80f + metrics.dropIntensity * 0.14f);
            target.scenePersonality = std::max(target.scenePersonality, 0.82f + metrics.dropIntensity * 0.10f);
            target.response3D = std::max(target.response3D, 0.88f + metrics.dropIntensity * 0.10f);
            target.motionStability = std::max(target.motionStability, 0.80f);
            target.patternClarity = std::max(target.patternClarity, 0.82f);
            target.intensity *= 1.16f + metrics.dropIntensity * 0.34f;
            target.speed *= 1.06f + metrics.beatConfidence * 0.14f;
        }
        break;
    case ArrangementSection::Groove:
        if (metrics.sectionConfidence > 0.58f && metrics.bpm >= 112.0f && metrics.bpm <= 156.0f &&
            metrics.style == AudioStyle::Techno && metrics.dropIntensity < 0.4f) {
            target.mode = VisualMode::PolyrhythmLattice;
        }
        if (metrics.sectionConfidence > 0.48f) {
            target.objectDensity3D = std::max(target.objectDensity3D, 0.72f + metrics.beatConfidence * 0.08f);
            target.scenePersonality = std::max(target.scenePersonality, 0.66f + metrics.beatConfidence * 0.08f);
            target.response3D = std::max(target.response3D, 0.74f + metrics.beatConfidence * 0.10f);
            target.motionStability = std::max(target.motionStability, 0.82f);
            target.patternClarity = std::max(target.patternClarity, 0.86f);
        }
        break;
    }

    if (metrics.buildTension > 0.72f &&
        metrics.phraseConfidence > 0.42f &&
        metrics.dropIntensity < 0.56f &&
        metrics.section != ArrangementSection::Drop) {
        target.mode = metrics.harmonicEnergy > 0.5f && metrics.keyConfidence > 0.42f
                          ? VisualMode::ResonanceTessellation
                          : (metrics.stereoWidth > 0.46f ? VisualMode::PhaseWeave : VisualMode::TechnoMandala);
        target.motionStyle = metrics.stereoWidth > 0.46f ? MotionStyle::Hyperspace : MotionStyle::Mechanical;
        target.depth3D = std::max(target.depth3D, 0.78f + metrics.buildTension * 0.14f);
        target.colorImpact = std::max(target.colorImpact, 0.78f + metrics.buildTension * 0.1f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.76f + metrics.buildTension * 0.12f);
        target.lightingGlow = std::max(target.lightingGlow, 0.74f + metrics.buildTension * 0.14f);
        target.scenePersonality = std::max(target.scenePersonality, 0.76f + metrics.buildTension * 0.12f);
        target.response3D = std::max(target.response3D, 0.82f + metrics.buildTension * 0.12f);
        target.motionStability = std::max(target.motionStability, 0.80f);
        target.patternClarity = std::max(target.patternClarity, 0.84f);
        target.intensity *= 1.04f + metrics.buildTension * 0.2f;
        target.speed *= 1.02f + metrics.buildTension * 0.14f;
    }

    if (metrics.buildTension > 0.58f &&
        metrics.keyConfidence > 0.52f &&
        metrics.harmonicEnergy > 0.62f &&
        metrics.spectralFlux > 0.26f &&
        metrics.dropIntensity < 0.58f &&
        metrics.section != ArrangementSection::Drop) {
        target.mode = VisualMode::CymaticInterference;
        target.palette = metrics.treble > 0.36f ? Palette::AcidAurora : target.palette;
        target.motionStyle = MotionStyle::Smooth;
        target.depth3D = std::max(target.depth3D, 0.72f + metrics.buildTension * 0.12f);
        target.colorImpact = std::max(target.colorImpact, 0.84f + metrics.harmonicEnergy * 0.1f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.72f + metrics.harmonicEnergy * 0.12f);
        target.lightingGlow = std::max(target.lightingGlow, 0.80f + metrics.harmonicEnergy * 0.14f);
        target.scenePersonality = std::max(target.scenePersonality, 0.78f + metrics.harmonicEnergy * 0.12f);
        target.response3D = std::max(target.response3D, 0.80f + metrics.buildTension * 0.10f);
        target.motionStability = std::max(target.motionStability, 0.86f);
        target.patternClarity = std::max(target.patternClarity, 0.90f);
        target.intensity *= 1.05f + metrics.harmonicEnergy * 0.12f + metrics.buildTension * 0.08f;
    }

    if (hasDarkMinimalCue) {
        target.mode = metrics.keyConfidence > 0.24f ? VisualMode::ResonanceTessellation : VisualMode::FractalCathedral;
        target.palette = Palette::MonochromeLaser;
        target.motionStyle = MotionStyle::Smooth;
        target.depth3D = std::max(target.depth3D, 0.88f);
        target.colorImpact = std::min(target.colorImpact, 0.66f);
        target.objectDensity3D = std::clamp(0.42f + metrics.bass * 0.22f, 0.42f, 0.64f);
        target.lightingGlow = std::max(target.lightingGlow, 0.58f + metrics.harmonicEnergy * 0.10f);
        target.scenePersonality = std::max(target.scenePersonality, 0.82f);
        target.response3D = std::max(target.response3D, 0.70f + metrics.beatConfidence * 0.10f);
        target.motionStability = std::max(target.motionStability, 0.92f);
        target.patternClarity = std::max(target.patternClarity, 0.92f);
        target.intensity *= 0.74f + metrics.bass * 0.22f;
        target.speed *= 0.58f + metrics.beatConfidence * 0.14f;
    }

    if (hasBreakCue) {
        target.mode = VisualMode::SpectralOrigami;
        target.palette = Palette::AcidAurora;
        target.motionStyle = MotionStyle::Breakbeat;
        target.depth3D = std::max(target.depth3D, 0.78f + metrics.spectralFlux * 0.10f);
        target.colorImpact = std::max(target.colorImpact, 0.86f + highTexture(metrics) * 0.10f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.78f + metrics.spectralFlux * 0.12f);
        target.lightingGlow = std::max(target.lightingGlow, 0.84f + highTexture(metrics) * 0.14f);
        target.scenePersonality = std::max(target.scenePersonality, 0.82f + metrics.onset * 0.10f);
        target.response3D = std::max(target.response3D, 0.84f + metrics.spectralFlux * 0.10f);
        target.motionStability = std::max(target.motionStability, 0.78f);
        target.patternClarity = std::max(target.patternClarity, 0.86f);
    }

    if (metrics.dropIntensity > 0.68f) {
        target.mode = metrics.spectralFlux > 0.54f && metrics.stereoWidth > 0.52f
                          ? VisualMode::PhaseWeave
                          : ((metrics.spectralFlux > 0.42f || metrics.stereoWidth > 0.5f)
                                 ? VisualMode::HyperspacePolytope
                                 : VisualMode::QuantumTunnel);
        target.motionStyle = metrics.bass > 0.58f ? MotionStyle::HeavyBass : MotionStyle::Hyperspace;
        target.depth3D = std::max(target.depth3D, 0.92f);
        target.colorImpact = std::max(target.colorImpact, 0.88f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.88f + metrics.dropIntensity * 0.08f);
        target.lightingGlow = std::max(target.lightingGlow, 0.86f + metrics.dropIntensity * 0.10f);
        target.scenePersonality = std::max(target.scenePersonality, 0.86f + metrics.dropIntensity * 0.08f);
        target.response3D = std::max(target.response3D, 0.92f + metrics.dropIntensity * 0.08f);
        target.motionStability = std::max(target.motionStability, 0.80f);
        target.patternClarity = std::max(target.patternClarity, 0.82f);
        target.intensity *= 1.18f;
        target.speed *= 1.08f;
    } else if (!hasDarkMinimalCue &&
               metrics.style == AudioStyle::BassHeavy &&
               (metrics.dropIntensity > 0.34f || metrics.bass > 0.70f) &&
               metrics.treble < metrics.bass * 0.82f) {
        target.mode = VisualMode::QuantumTunnel;
        target.motionStyle = MotionStyle::HeavyBass;
        target.depth3D = std::max(target.depth3D, 0.86f + metrics.bass * 0.10f);
        target.colorImpact = std::max(target.colorImpact, 0.78f + metrics.dropIntensity * 0.08f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.82f + metrics.bass * 0.08f);
        target.lightingGlow = std::max(target.lightingGlow, 0.78f + metrics.dropIntensity * 0.10f);
        target.scenePersonality = std::max(target.scenePersonality, 0.78f + metrics.bass * 0.10f);
        target.response3D = std::max(target.response3D, 0.88f + metrics.dropIntensity * 0.10f);
    } else if (!hasBreakCue && metrics.bandOnsets[0] > 0.55f && metrics.beatConfidence > 0.5f) {
        target.mode = VisualMode::PolyrhythmLattice;
        target.motionStyle = MotionStyle::Mechanical;
        target.objectDensity3D = std::max(target.objectDensity3D, 0.76f + metrics.beatConfidence * 0.10f);
        target.response3D = std::max(target.response3D, 0.78f + metrics.beatConfidence * 0.10f);
        target.motionStability = std::max(target.motionStability, 0.84f);
        target.patternClarity = std::max(target.patternClarity, 0.86f);
    } else if (target.mode != VisualMode::CymaticInterference &&
               (metrics.phraseIntensity > 0.62f || metrics.buildTension > 0.7f) &&
               metrics.stereoWidth > 0.25f) {
        target.mode = VisualMode::FractalCathedral;
        target.motionStyle = MotionStyle::AmbientDrift;
        target.objectDensity3D = std::max(target.objectDensity3D, 0.66f + metrics.phraseIntensity * 0.12f);
        target.lightingGlow = std::max(target.lightingGlow, 0.66f + metrics.harmonicEnergy * 0.16f);
        target.scenePersonality = std::max(target.scenePersonality, 0.68f + metrics.phraseIntensity * 0.14f);
        target.response3D = std::max(target.response3D, 0.70f + metrics.phraseIntensity * 0.10f);
        target.motionStability = std::max(target.motionStability, 0.86f);
        target.patternClarity = std::max(target.patternClarity, 0.88f);
    } else if (metrics.bandOnsets[4] > 0.44f && metrics.treble > 0.38f) {
        target.mode = VisualMode::SpectralOrigami;
        target.motionStyle = MotionStyle::Breakbeat;
        target.objectDensity3D = std::max(target.objectDensity3D, 0.72f + metrics.spectralFlux * 0.14f);
        target.lightingGlow = std::max(target.lightingGlow, 0.80f + metrics.treble * 0.12f);
        target.scenePersonality = std::max(target.scenePersonality, 0.76f + metrics.onset * 0.12f);
        target.response3D = std::max(target.response3D, 0.78f + metrics.onset * 0.14f);
        target.motionStability = std::max(target.motionStability, 0.76f);
        target.patternClarity = std::max(target.patternClarity, 0.86f);
    } else if (metrics.style == AudioStyle::Bright &&
               (metrics.spectralFlux > 0.24f || metrics.onset > 0.30f) &&
               metrics.treble + metrics.highMid > metrics.bass + metrics.lowMid * 0.5f) {
        target.mode = VisualMode::SpectralOrigami;
        target.motionStyle = MotionStyle::Breakbeat;
        target.objectDensity3D = std::max(target.objectDensity3D, 0.70f + metrics.spectralFlux * 0.14f);
        target.lightingGlow = std::max(target.lightingGlow, 0.78f + metrics.treble * 0.12f);
        target.scenePersonality = std::max(target.scenePersonality, 0.74f + metrics.onset * 0.12f);
        target.response3D = std::max(target.response3D, 0.78f + metrics.onset * 0.12f);
    } else if (target.mode != VisualMode::ResonanceTessellation &&
               target.mode != VisualMode::CymaticInterference &&
               metrics.keyConfidence > 0.58f &&
               metrics.harmonicEnergy > 0.58f) {
        target.mode = VisualMode::ChromaKaleidoscope;
        target.motionStyle = MotionStyle::Liquid;
        target.lightingGlow = std::max(target.lightingGlow, 0.72f + metrics.harmonicEnergy * 0.14f);
        target.scenePersonality = std::max(target.scenePersonality, 0.78f + metrics.harmonicEnergy * 0.12f);
        target.response3D = std::max(target.response3D, 0.76f + metrics.phraseIntensity * 0.12f);
        target.motionStability = std::max(target.motionStability, 0.82f);
        target.patternClarity = std::max(target.patternClarity, 0.88f);
    }

    if (metrics.downbeatConfidence > 0.58f &&
        metrics.barConfidence > 0.34f &&
        metrics.dropIntensity < 0.62f &&
        !hasBreakCue &&
        !hasDarkMinimalCue &&
        !hasSpaciousCalmCue &&
        metrics.style != AudioStyle::Techno &&
        metrics.style != AudioStyle::BassHeavy &&
        metrics.style != AudioStyle::Bright &&
        (metrics.harmonicEnergy > 0.42f || metrics.stereoWidth > 0.44f) &&
        (metrics.spectralFlux > 0.18f || metrics.phraseIntensity > 0.32f || metrics.section == ArrangementSection::Groove)) {
        target.mode = VisualMode::NeuralConstellation;
        target.motionStyle = metrics.stereoWidth > 0.52f ? MotionStyle::Liquid : MotionStyle::Smooth;
        target.depth3D = std::max(target.depth3D, 0.72f + metrics.stereoWidth * 0.12f);
        target.colorImpact = std::max(target.colorImpact, 0.70f + metrics.harmonicEnergy * 0.12f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.68f + metrics.barConfidence * 0.12f);
        target.lightingGlow = std::max(target.lightingGlow, 0.70f + metrics.downbeatConfidence * 0.12f);
        target.scenePersonality = std::max(target.scenePersonality, 0.74f + metrics.harmonicEnergy * 0.12f);
        target.response3D = std::max(target.response3D, 0.76f + metrics.downbeatConfidence * 0.12f);
        target.motionStability = std::max(target.motionStability, 0.84f);
        target.patternClarity = std::max(target.patternClarity, 0.88f);
        target.intensity *= 1.0f + metrics.barConfidence * 0.14f + metrics.downbeatConfidence * 0.08f;
    }

    applyRoleDirectedTarget(target, metrics);

    target.depth3D = clampSetting(target.depth3D, 0.0f, 1.0f);
    target.colorImpact = clampSetting(target.colorImpact, 0.0f, 1.0f);
    target.objectDensity3D = clampSetting(target.objectDensity3D, 0.08f, 1.0f);
    target.lightingGlow = clampSetting(target.lightingGlow, 0.05f, 1.0f);
    target.scenePersonality = clampSetting(target.scenePersonality, 0.0f, 1.0f);
    target.response3D = clampSetting(target.response3D, 0.05f, 1.0f);
    target.motionStability = clampSetting(target.motionStability, 0.25f, 1.0f);
    target.patternClarity = clampSetting(target.patternClarity, 0.25f, 1.0f);
    target.intensity = clampSetting(target.intensity, 0.15f, 4.0f);
    target.speed = clampSetting(target.speed, 0.1f, 4.0f);
    return target;
}

float smooth(float current, float target, float alpha)
{
    return current + (target - current) * alpha;
}

float smoothHue(float current, float target, float alpha)
{
    current = wrapUnit(current);
    target = wrapUnit(target);
    float delta = target - current;
    if (delta > 0.5f) {
        delta -= 1.0f;
    } else if (delta < -0.5f) {
        delta += 1.0f;
    }
    return wrapUnit(current + delta * alpha);
}

} // namespace

void SceneDirector::reset()
{
    initialized_ = false;
    continuityInitialized_ = false;
    lastTimeSeconds_ = 0.0;
    lastModeSwitchSeconds_ = -100.0;
    transitionStartSeconds_ = -100.0;
    transitionDurationSeconds_ = 0.0;
    currentMode_ = VisualMode::QuantumTunnel;
    currentPalette_ = Palette::NeonVoltage;
    transitionStrength_ = 0.0f;
    smoothedHueShift_ = 0.0f;
    smoothedDepth3D_ = 0.55f;
    smoothedColorImpact_ = 0.65f;
    smoothedObjectDensity3D_ = 0.65f;
    smoothedLightingGlow_ = 0.62f;
    smoothedScenePersonality_ = 0.5f;
    smoothedResponse3D_ = 0.88f;
    smoothedMotionStability_ = 0.72f;
    smoothedPatternClarity_ = 0.78f;
    smoothedIntensity_ = 1.0f;
    smoothedSpeed_ = 1.0f;
    quietMemory_ = 0.0f;
    ambientMemory_ = 0.0f;
    technoMemory_ = 0.0f;
    bassMemory_ = 0.0f;
    melodicMemory_ = 0.0f;
    breakMemory_ = 0.0f;
    darkMemory_ = 0.0f;
}

void SceneDirector::initialize(const VisualSettings& base, double timeSeconds)
{
    initialized_ = true;
    lastTimeSeconds_ = timeSeconds;
    currentMode_ = base.mode;
    currentPalette_ = base.palette;
    transitionStartSeconds_ = -100.0;
    transitionDurationSeconds_ = 0.0;
    transitionStrength_ = 0.0f;
    smoothedHueShift_ = wrapUnit(base.hueShift);
    smoothedDepth3D_ = base.depth3D;
    smoothedColorImpact_ = base.colorImpact;
    smoothedObjectDensity3D_ = base.objectDensity3D;
    smoothedLightingGlow_ = base.lightingGlow;
    smoothedScenePersonality_ = base.scenePersonality;
    smoothedResponse3D_ = base.response3D;
    smoothedMotionStability_ = base.motionStability;
    smoothedPatternClarity_ = base.patternClarity;
    smoothedIntensity_ = base.intensity;
    smoothedSpeed_ = base.speed;
    continuityInitialized_ = false;
    quietMemory_ = 0.0f;
    ambientMemory_ = 0.0f;
    technoMemory_ = 0.0f;
    bassMemory_ = 0.0f;
    melodicMemory_ = 0.0f;
    breakMemory_ = 0.0f;
    darkMemory_ = 0.0f;
}

VisualSettings SceneDirector::resolve(const VisualSettings& base,
                                      const AudioMetrics& metrics,
                                      double timeSeconds)
{
    if (!base.autoScene) {
        reset();
        VisualSettings manual = base;
        manual.sceneTransition = 0.0f;
        manual.sceneTransitionProgress = 1.0f;
        return manual;
    }

    if (!initialized_) {
        initialize(base, timeSeconds);
    }

    const double dt = std::max(0.0, timeSeconds - lastTimeSeconds_);
    lastTimeSeconds_ = timeSeconds;
    const float alpha = std::clamp(1.0f - std::exp(static_cast<float>(-dt * 3.2)), 0.08f, 0.38f);
    const ContinuityScores scores = scoreContinuity(metrics);
    const auto updateMemory = [this, dt](float& memory, float score, float riseBonus) {
        if (!continuityInitialized_) {
            memory = score;
            return;
        }
        const bool rising = score > memory;
        const float rate = rising ? (1.55f + riseBonus) : 0.38f;
        const float memoryAlpha = std::clamp(1.0f - std::exp(static_cast<float>(-dt * rate)),
                                             rising ? 0.10f : 0.04f,
                                             rising ? 0.44f : 0.16f);
        memory = smooth(memory, score, memoryAlpha);
    };
    updateMemory(quietMemory_, scores.quiet, 0.10f);
    updateMemory(ambientMemory_, scores.ambient, 0.18f);
    updateMemory(technoMemory_, scores.techno, 0.28f);
    updateMemory(bassMemory_, scores.bass, metrics.dropIntensity * 0.80f);
    updateMemory(melodicMemory_, scores.melodic, 0.22f);
    updateMemory(breakMemory_, scores.broken, transientBreakCue(metrics) ? 0.90f : 0.18f);
    updateMemory(darkMemory_, scores.dark, darkMinimalCue(metrics) ? 0.36f : 0.08f);
    continuityInitialized_ = true;

    SceneTarget target = targetFor(base, metrics);

    const bool roleAware = hasMusicalRoles(metrics);
    const float melodyRoleScore = melodicRole(metrics);
    const bool roleBassDominant = roleAware &&
                                  metrics.bassRole > 0.40f &&
                                  metrics.bassRole > metrics.spaceRole + 0.10f &&
                                  metrics.bassRole > melodyRoleScore + 0.08f;
    const bool roleDrumDominant = roleAware &&
                                  metrics.drumRole > 0.34f &&
                                  metrics.drumRole > metrics.spaceRole + 0.06f &&
                                  metrics.drumRole > melodyRoleScore + 0.04f;
    const bool roleMelodicDominant = roleAware &&
                                     melodyRoleScore > 0.30f &&
                                     melodyRoleScore > metrics.bassRole + 0.06f &&
                                     melodyRoleScore > metrics.fractureRole + 0.02f;
    const bool roleSpaceDominant = roleAware &&
                                   metrics.spaceRole > 0.34f &&
                                   metrics.spaceRole > metrics.bassRole + 0.08f &&
                                   metrics.spaceRole > metrics.drumRole + 0.06f &&
                                   metrics.spaceRole > metrics.fractureRole + 0.06f;
    const bool roleFractureDominant = roleAware &&
                                      metrics.fractureRole > 0.40f &&
                                      metrics.fractureRole > metrics.spaceRole + 0.06f &&
                                      metrics.fractureRole > metrics.bassRole + 0.06f &&
                                      metrics.fractureRole > metrics.shadowRole + 0.03f &&
                                      metrics.fractureRole > melodyRoleScore;
    const bool roleShadowDominant = roleAware &&
                                    metrics.shadowRole > 0.34f &&
                                    metrics.shadowRole > metrics.spaceRole + 0.06f &&
                                    metrics.shadowRole > metrics.fractureRole - 0.04f;

    const bool hardDropNow = metrics.dropIntensity > 0.66f ||
                             (metrics.section == ArrangementSection::Drop && metrics.sectionConfidence > 0.70f);
    const bool hardBreakNow = transientBreakCue(metrics) &&
                              (metrics.spectralFlux > 0.32f || metrics.onset > 0.30f || highBandOnset(metrics) > 0.40f);
    const bool hardRoleBreakNow = roleFractureDominant &&
                                  (metrics.convergenceRole > 0.24f || metrics.roleSeparation > 0.52f);
    const bool dominantBreak = (breakMemory_ > 0.46f &&
                                breakMemory_ > bassMemory_ + 0.05f &&
                                breakMemory_ > technoMemory_ - 0.02f &&
                                breakMemory_ > darkMemory_ + 0.04f) ||
                               roleFractureDominant;
    const bool dominantBass = (bassMemory_ > 0.54f &&
                               bassMemory_ > darkMemory_ + 0.08f &&
                               (metrics.bass > 0.50f || metrics.bassRole > 0.40f)) ||
                              roleBassDominant;
    const bool dominantDark = (darkMemory_ > 0.46f &&
                               darkMemory_ > ambientMemory_ + 0.08f &&
                               (metrics.bass > 0.42f || metrics.shadowRole > 0.34f)) ||
                              roleShadowDominant;
    const bool dominantMelodic = (melodicMemory_ > 0.46f &&
                                  melodicMemory_ > technoMemory_ + 0.04f &&
                                  metrics.bass < 0.38f &&
                                  metrics.dropIntensity < 0.46f) ||
                                 roleMelodicDominant;
    const bool explicitDimensionalTarget =
        (target.mode == VisualMode::HyperspacePolytope &&
         metrics.spectralFlux > 0.48f &&
         metrics.stereoWidth > 0.34f) ||
        (target.mode == VisualMode::PhaseWeave &&
         metrics.spectralFlux > 0.42f &&
         metrics.stereoWidth > 0.46f);
    const bool explicitHarmonicStructure =
        (target.mode == VisualMode::ResonanceTessellation ||
         target.mode == VisualMode::CymaticInterference) &&
        metrics.keyConfidence > 0.46f &&
        metrics.harmonicEnergy > 0.52f;
    const bool explicitNeuralTarget =
        target.mode == VisualMode::NeuralConstellation &&
        metrics.downbeatConfidence > 0.55f &&
        metrics.barConfidence > 0.34f &&
        (metrics.harmonicEnergy > 0.38f || metrics.stereoWidth > 0.40f);
    const bool explicitHarmonicColor =
        target.mode == VisualMode::ChromaKaleidoscope &&
        metrics.keyConfidence > 0.52f &&
        metrics.harmonicEnergy > 0.56f;
    const bool explicitPhraseArchitecture =
        target.mode == VisualMode::FractalCathedral &&
        (metrics.phraseIntensity > 0.56f || metrics.buildTension > 0.64f) &&
        metrics.stereoWidth > 0.24f;
    const bool dominantAmbient = (ambientMemory_ > 0.42f &&
                                  ambientMemory_ > technoMemory_ + 0.04f &&
                                  metrics.dropIntensity < 0.52f &&
                                  !hardBreakNow &&
                                  !hardRoleBreakNow &&
                                  !hardDropNow &&
                                  !roleBassDominant &&
                                  !roleDrumDominant &&
                                  !roleFractureDominant &&
                                  !roleShadowDominant &&
                                  !roleMelodicDominant &&
                                  !dominantDark &&
                                  !explicitHarmonicStructure &&
                                  !explicitNeuralTarget &&
                                  !explicitHarmonicColor &&
                                  !explicitPhraseArchitecture) ||
                                 roleSpaceDominant;
    const bool dominantTechno = (technoMemory_ > 0.42f &&
                                 technoMemory_ > ambientMemory_ + 0.04f &&
                                 technoMemory_ > melodicMemory_ + 0.02f &&
                                 !hardBreakNow &&
                                 !hardRoleBreakNow &&
                                 !hardDropNow &&
                                 !dominantDark &&
                                 !roleBassDominant &&
                                 !roleFractureDominant &&
                                 !roleShadowDominant &&
                                 !roleSpaceDominant &&
                                 !explicitDimensionalTarget &&
                                 !explicitHarmonicStructure &&
                                 !explicitNeuralTarget &&
                                 !explicitHarmonicColor &&
                                 !explicitPhraseArchitecture) ||
                                roleDrumDominant;

    if (hardRoleBreakNow || hardBreakNow || dominantBreak) {
        target.mode = VisualMode::SpectralOrigami;
        target.palette = Palette::AcidAurora;
        target.motionStyle = MotionStyle::Breakbeat;
        target.colorImpact = std::max(target.colorImpact, 0.86f);
        target.motionStability = std::max(target.motionStability, 0.80f);
        reinforce3DSettings(target);
    } else if (hardDropNow || dominantBass) {
        target.mode = metrics.stereoWidth > 0.52f && metrics.spectralFlux > 0.36f && !roleBassDominant
                          ? VisualMode::HyperspacePolytope
                          : VisualMode::QuantumTunnel;
        target.palette = Palette::InfraredChrome;
        target.motionStyle = MotionStyle::HeavyBass;
        target.depth3D = std::max(target.depth3D, 0.90f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.82f);
        target.response3D = std::max(target.response3D, 0.90f);
        target.motionStability = std::max(target.motionStability, 0.82f);
        reinforce3DSettings(target);
    } else if (dominantDark) {
        target.mode = metrics.keyConfidence > 0.20f ? VisualMode::ResonanceTessellation : VisualMode::FractalCathedral;
        target.palette = Palette::MonochromeLaser;
        target.motionStyle = MotionStyle::Smooth;
        target.colorImpact = std::min(target.colorImpact, 0.62f);
        target.objectDensity3D = std::clamp(target.objectDensity3D, 0.42f, 0.66f);
        target.motionStability = std::max(target.motionStability, 0.92f);
        reinforce3DSettings(target);
    } else if (dominantMelodic) {
        target.mode = metrics.harmonyRole > metrics.melodyRole + 0.08f && metrics.keyConfidence > 0.34f
                          ? VisualMode::ResonanceTessellation
                          : (melodicMemory_ > 0.58f ? VisualMode::ChromaKaleidoscope : VisualMode::FrequencyBloom);
        target.palette = Palette::AcidAurora;
        target.motionStyle = MotionStyle::Liquid;
        target.colorImpact = std::max(target.colorImpact, 0.84f);
        target.motionStability = std::max(target.motionStability, 0.84f);
        reinforce3DSettings(target);
    } else if (dominantTechno) {
        target.mode = technoMemory_ > 0.62f || metrics.downbeatConfidence > 0.52f || metrics.drumRole > 0.62f
                          ? VisualMode::TechnoMandala
                          : VisualMode::PolyrhythmLattice;
        target.palette = Palette::NeonVoltage;
        target.motionStyle = MotionStyle::Mechanical;
        target.objectDensity3D = std::max(target.objectDensity3D, 0.76f);
        target.motionStability = std::max(target.motionStability, 0.84f);
        reinforce3DSettings(target);
    } else if (dominantAmbient) {
        target.mode = VisualMode::PhaseWeave;
        target.palette = Palette::OceanicPulse;
        target.motionStyle = MotionStyle::AmbientDrift;
        target.depth3D = std::max(target.depth3D, 0.82f);
        target.objectDensity3D = std::max(target.objectDensity3D, 0.58f);
        target.motionStability = std::max(target.motionStability, 0.90f);
        reinforce3DSettings(target);
    } else if (quietMemory_ > 0.70f && metrics.rms < 0.055f) {
        target.motionStyle = MotionStyle::Smooth;
        target.motionStability = std::max(target.motionStability, 0.94f);
        target.patternClarity = std::max(target.patternClarity, 0.94f);
    }
    const bool targetIsSoftField = target.mode == VisualMode::PhaseWeave ||
                                   target.mode == VisualMode::LissajousMesh;
    const bool targetIsSoftOrCathedral = targetIsSoftField ||
                                         target.mode == VisualMode::FractalCathedral;
    const bool ambiguousLowMotionFrame = metrics.dropIntensity < 0.34f &&
                                         metrics.spectralFlux < 0.18f &&
                                         metrics.beatConfidence < 0.34f;
    if ((currentMode_ == VisualMode::TechnoMandala ||
         currentMode_ == VisualMode::PolyrhythmLattice) &&
        targetIsSoftField &&
        ambiguousLowMotionFrame &&
        technoMemory_ > 0.34f &&
        metrics.bass > 0.40f) {
        target.mode = currentMode_;
        target.palette = Palette::NeonVoltage;
        target.motionStyle = MotionStyle::Mechanical;
        target.motionStability = std::max(target.motionStability, 0.86f);
        target.patternClarity = std::max(target.patternClarity, 0.88f);
        reinforce3DSettings(target);
    }
    if ((currentMode_ == VisualMode::PhaseWeave ||
         currentMode_ == VisualMode::LissajousMesh) &&
        (target.mode == VisualMode::TechnoMandala ||
         target.mode == VisualMode::PolyrhythmLattice) &&
        ambiguousLowMotionFrame &&
        ambientMemory_ > 0.34f &&
        metrics.beatConfidence < 0.18f) {
        target.mode = currentMode_;
        target.palette = Palette::OceanicPulse;
        target.motionStyle = MotionStyle::AmbientDrift;
        target.motionStability = std::max(target.motionStability, 0.90f);
        target.patternClarity = std::max(target.patternClarity, 0.90f);
        reinforce3DSettings(target);
    }
    if (currentMode_ == VisualMode::SpectralOrigami &&
        targetIsSoftOrCathedral &&
        metrics.dropIntensity < 0.34f &&
        breakMemory_ > 0.24f &&
        (timeSeconds - lastModeSwitchSeconds_) < 1.45) {
        target.mode = VisualMode::SpectralOrigami;
        target.palette = Palette::AcidAurora;
        target.motionStyle = metrics.spectralFlux > 0.18f ? MotionStyle::Breakbeat : MotionStyle::AmbientDrift;
        target.motionStability = std::max(target.motionStability, 0.84f);
        target.patternClarity = std::max(target.patternClarity, 0.88f);
        reinforce3DSettings(target);
    }

    const bool strongDrop = metrics.dropIntensity > 0.68f ||
                            (metrics.section == ArrangementSection::Drop && metrics.sectionConfidence > 0.62f);
    const bool strongBreak = transientBreakCue(metrics) &&
                             (metrics.spectralFlux > 0.30f ||
                              highBandOnset(metrics) > 0.38f ||
                              metrics.onset > 0.26f);
    const bool strongRoleIdentity = (roleBassDominant ||
                                     roleDrumDominant ||
                                     roleMelodicDominant ||
                                     roleSpaceDominant ||
                                     roleFractureDominant ||
                                     roleShadowDominant) &&
                                    (metrics.roleSeparation > 0.32f || strongestMusicalRole(metrics) > 0.46f);
    const bool strongMusicIdentity = strongRoleIdentity ||
                                     strongBreak ||
                                     ((target.mode == VisualMode::ChromaKaleidoscope ||
                                       target.mode == VisualMode::FrequencyBloom) &&
                                      melodicCue(metrics)) ||
                                     ((target.mode == VisualMode::PhaseWeave ||
                                       target.mode == VisualMode::LissajousMesh) &&
                                      softHarmonicFieldCue(metrics)) ||
                                     ((target.mode == VisualMode::TechnoMandala ||
                                       target.mode == VisualMode::PolyrhythmLattice) &&
                                      structuralTechnoCue(metrics)) ||
                                     (target.palette == Palette::MonochromeLaser && darkMinimalCue(metrics));
    const bool phraseBoundary = (metrics.phraseBoundary && metrics.phraseConfidence > 0.42f) ||
                                (metrics.downbeat && metrics.downbeatConfidence > 0.45f) ||
                                (metrics.phraseIntensity > 0.55f && metrics.beatPhase < 0.22f) ||
                                (metrics.section == ArrangementSection::Build &&
                                 metrics.sectionConfidence > 0.58f &&
                                 metrics.sectionProgress < 0.2f);
    const bool switchWindowElapsed = (timeSeconds - lastModeSwitchSeconds_) > 1.35;
    if (target.mode != currentMode_ && (strongDrop || strongMusicIdentity || phraseBoundary || switchWindowElapsed)) {
        currentMode_ = target.mode;
        lastModeSwitchSeconds_ = timeSeconds;
        transitionStartSeconds_ = timeSeconds;
        transitionDurationSeconds_ = strongDrop ? 0.72 : (strongBreak ? 0.58 : (phraseBoundary ? 1.05 : 0.86));
        transitionStrength_ = std::clamp(0.48f +
                                             metrics.dropIntensity * 0.36f +
                                             metrics.phraseIntensity * 0.22f +
                                             metrics.buildTension * 0.16f +
                                             metrics.roleSeparation * 0.10f +
                                             metrics.beatConfidence * 0.12f +
                                             metrics.downbeatConfidence * 0.08f,
                                         0.45f,
                                         1.0f);
    }
    if (target.palette != currentPalette_ && (strongDrop || strongMusicIdentity || switchWindowElapsed)) {
        currentPalette_ = target.palette;
    }

    smoothedIntensity_ = smooth(smoothedIntensity_, target.intensity, alpha);
    smoothedSpeed_ = smooth(smoothedSpeed_, target.speed, alpha);
    smoothedHueShift_ = smoothHue(smoothedHueShift_, target.hueShift, alpha);
    smoothedDepth3D_ = smooth(smoothedDepth3D_, target.depth3D, alpha);
    smoothedColorImpact_ = smooth(smoothedColorImpact_, target.colorImpact, alpha);
    const float alpha3D = std::clamp(alpha +
                                         metrics.dropIntensity * 0.12f +
                                         metrics.onset * 0.06f +
                                         metrics.downbeatConfidence * 0.04f,
                                     0.10f,
                                     0.56f);
    smoothedObjectDensity3D_ = smooth(smoothedObjectDensity3D_, target.objectDensity3D, alpha3D);
    smoothedLightingGlow_ = smooth(smoothedLightingGlow_, target.lightingGlow, alpha3D);
    smoothedScenePersonality_ = smooth(smoothedScenePersonality_, target.scenePersonality, alpha3D);
    smoothedResponse3D_ = smooth(smoothedResponse3D_, target.response3D, alpha3D);
    smoothedMotionStability_ = smooth(smoothedMotionStability_, target.motionStability, alpha);
    smoothedPatternClarity_ = smooth(smoothedPatternClarity_, target.patternClarity, alpha);

    VisualSettings resolved = base;
    resolved.mode = currentMode_;
    resolved.palette = currentPalette_;
    resolved.motionStyle = target.motionStyle;
    resolved.hueShift = smoothedHueShift_;
    resolved.depth3D = clampSetting(smoothedDepth3D_, 0.0f, 1.0f);
    resolved.colorImpact = clampSetting(smoothedColorImpact_, 0.0f, 1.0f);
    resolved.objectDensity3D = clampSetting(smoothedObjectDensity3D_, 0.08f, 1.0f);
    resolved.lightingGlow = clampSetting(smoothedLightingGlow_, 0.05f, 1.0f);
    resolved.scenePersonality = clampSetting(smoothedScenePersonality_, 0.0f, 1.0f);
    resolved.response3D = clampSetting(smoothedResponse3D_, 0.05f, 1.0f);
    resolved.motionStability = clampSetting(smoothedMotionStability_, 0.25f, 1.0f);
    resolved.patternClarity = clampSetting(smoothedPatternClarity_, 0.25f, 1.0f);
    resolved.intensity = clampSetting(smoothedIntensity_, 0.15f, 4.0f);
    resolved.speed = clampSetting(smoothedSpeed_, 0.1f, 4.0f);
    if (transitionDurationSeconds_ > 0.0) {
        const float progress = std::clamp(static_cast<float>((timeSeconds - transitionStartSeconds_) /
                                                             transitionDurationSeconds_),
                                          0.0f,
                                          1.0f);
        const float remaining = 1.0f - progress;
        resolved.sceneTransitionProgress = progress;
        resolved.sceneTransition = transitionStrength_ * remaining * remaining * (3.0f - 2.0f * remaining);
    } else {
        resolved.sceneTransitionProgress = 1.0f;
        resolved.sceneTransition = 0.0f;
    }
    return resolved;
}

} // namespace viz
