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
        target.mode = VisualMode::FractalCathedral;
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
        target.mode = metrics.spectralFlux > 0.48f && metrics.stereoWidth > 0.32f
                          ? VisualMode::HyperspacePolytope
                          : (metrics.phraseIntensity > 0.45f ? VisualMode::TechnoMandala : VisualMode::PolyrhythmLattice);
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
        target.mode = metrics.dropIntensity > 0.5f ? VisualMode::QuantumTunnel : VisualMode::PolyrhythmLattice;
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
        target.mode = metrics.spectralFlux > 0.34f ? VisualMode::SpectralOrigami : VisualMode::FrequencyBloom;
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
                       (metrics.phraseIntensity > 0.35f ? VisualMode::FractalCathedral : VisualMode::LissajousMesh));
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
            target.mode = metrics.harmonicEnergy > 0.48f ? VisualMode::ChromaKaleidoscope : VisualMode::FractalCathedral;
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
            if (metrics.keyConfidence > 0.48f && metrics.harmonicEnergy > 0.56f &&
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
    } else if (metrics.bandOnsets[0] > 0.55f && metrics.beatConfidence > 0.5f) {
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

    const SceneTarget target = targetFor(base, metrics);
    const double dt = std::max(0.0, timeSeconds - lastTimeSeconds_);
    lastTimeSeconds_ = timeSeconds;
    const float alpha = std::clamp(1.0f - std::exp(static_cast<float>(-dt * 3.2)), 0.08f, 0.38f);

    const bool strongDrop = metrics.dropIntensity > 0.68f ||
                            (metrics.section == ArrangementSection::Drop && metrics.sectionConfidence > 0.62f);
    const bool phraseBoundary = (metrics.phraseBoundary && metrics.phraseConfidence > 0.42f) ||
                                (metrics.downbeat && metrics.downbeatConfidence > 0.45f) ||
                                (metrics.phraseIntensity > 0.55f && metrics.beatPhase < 0.22f) ||
                                (metrics.section == ArrangementSection::Build &&
                                 metrics.sectionConfidence > 0.58f &&
                                 metrics.sectionProgress < 0.2f);
    const bool switchWindowElapsed = (timeSeconds - lastModeSwitchSeconds_) > 1.35;
    if (target.mode != currentMode_ && (strongDrop || phraseBoundary || switchWindowElapsed)) {
        currentMode_ = target.mode;
        lastModeSwitchSeconds_ = timeSeconds;
        transitionStartSeconds_ = timeSeconds;
        transitionDurationSeconds_ = strongDrop ? 0.72 : (phraseBoundary ? 1.05 : 0.86);
        transitionStrength_ = std::clamp(0.48f +
                                             metrics.dropIntensity * 0.36f +
                                             metrics.phraseIntensity * 0.22f +
                                             metrics.buildTension * 0.16f +
                                             metrics.beatConfidence * 0.12f +
                                             metrics.downbeatConfidence * 0.08f,
                                         0.45f,
                                         1.0f);
    }
    if (target.palette != currentPalette_ && (strongDrop || switchWindowElapsed)) {
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
