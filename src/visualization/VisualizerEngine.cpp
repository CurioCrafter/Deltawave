#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace viz {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

ColorRGBA withAlpha(ColorRGBA color, float alpha)
{
    color.a = clamp01(alpha);
    return color;
}

ColorRGBA mix(ColorRGBA a, ColorRGBA b, float t)
{
    t = clamp01(t);
    return ColorRGBA{
        a.r + ((b.r - a.r) * t),
        a.g + ((b.g - a.g) * t),
        a.b + ((b.b - a.b) * t),
        a.a + ((b.a - a.a) * t)
    };
}

Vec2 mix(Vec2 a, Vec2 b, float t)
{
    t = clamp01(t);
    return Vec2{
        a.x + ((b.x - a.x) * t),
        a.y + ((b.y - a.y) * t)
    };
}

Vec3 mix(Vec3 a, Vec3 b, float t)
{
    t = clamp01(t);
    return Vec3{
        a.x + ((b.x - a.x) * t),
        a.y + ((b.y - a.y) * t),
        a.z + ((b.z - a.z) * t)
    };
}

Vec2 polar(Vec2 center, float radius, float angle)
{
    return Vec2{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
}

void rotatePlane(float& a, float& b, float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float nextA = a * c - b * s;
    const float nextB = a * s + b * c;
    a = nextA;
    b = nextB;
}

Vec3 rotate3D(Vec3 point, Vec3 rotation)
{
    rotatePlane(point.y, point.z, rotation.x);
    rotatePlane(point.x, point.z, rotation.y);
    rotatePlane(point.x, point.y, rotation.z);
    return point;
}

Vec3 add(Vec3 a, Vec3 b)
{
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec2 add(Vec2 a, Vec2 b)
{
    return Vec2{a.x + b.x, a.y + b.y};
}

Vec3 subtract(Vec3 a, Vec3 b)
{
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 multiply(Vec3 a, Vec3 b)
{
    return Vec3{a.x * b.x, a.y * b.y, a.z * b.z};
}

Vec3 scale(Vec3 value, float amount)
{
    return Vec3{value.x * amount, value.y * amount, value.z * amount};
}

float length(Vec3 value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b)
{
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 normalize(Vec3 value)
{
    const float magnitude = length(value);
    if (magnitude <= 0.0001f) {
        return Vec3{};
    }
    return scale(value, 1.0f / magnitude);
}

float distance2(Vec2 a, Vec2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float spectrumAt(const AudioMetrics& metrics, std::size_t index)
{
    return metrics.spectrum[index % metrics.spectrum.size()];
}

float chromaAt(const AudioMetrics& metrics, std::size_t index)
{
    return metrics.chroma[index % metrics.chroma.size()];
}

float qualityOf(const VisualSettings& settings)
{
    return std::clamp(settings.qualityScale, 0.45f, 1.0f);
}

float complexityOf(const VisualSettings& settings)
{
    return std::clamp(settings.complexity, 0.35f, 1.8f);
}

float depth3DOf(const VisualSettings& settings)
{
    return std::clamp(settings.depth3D, 0.0f, 1.0f);
}

float colorImpactOf(const VisualSettings& settings)
{
    return std::clamp(settings.colorImpact, 0.0f, 1.0f);
}

float objectDensity3DOf(const VisualSettings& settings)
{
    return std::clamp(settings.objectDensity3D, 0.0f, 1.0f);
}

float interactionDepthOf(const VisualSettings& settings)
{
    return std::clamp(settings.interactionDepth, 0.0f, 1.0f);
}

float lightingGlowOf(const VisualSettings& settings)
{
    return std::clamp(settings.lightingGlow, 0.0f, 1.0f);
}

float scenePersonalityOf(const VisualSettings& settings)
{
    return std::clamp(settings.scenePersonality, 0.0f, 1.0f);
}

float response3DOf(const VisualSettings& settings)
{
    return std::clamp(settings.response3D, 0.0f, 1.0f);
}

float motionStabilityOf(const VisualSettings& settings)
{
    return std::clamp(settings.motionStability, 0.0f, 1.0f);
}

float patternClarityOf(const VisualSettings& settings)
{
    return std::clamp(settings.patternClarity, 0.0f, 1.0f);
}

float smootherStep(float edge0, float edge1, float value)
{
    if (std::abs(edge1 - edge0) <= 0.00001f) {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

struct MusicMotionEnvelope {
    float audible = 0.0f;
    float energy = 0.0f;
    float bass = 0.0f;
    float beat = 0.0f;
    float drop = 0.0f;
    float phrase = 0.0f;
    float build = 0.0f;
    float treble = 0.0f;
    float stereo = 0.0f;
    float flux = 0.0f;
    float detail = 0.0f;
    float camera = 0.0f;
    float motion = 0.0f;
    float accent = 0.0f;
    float stability = 0.0f;
    float clarity = 0.0f;
};

struct MusicChoreography {
    float audible = 0.0f;
    float bassPressure = 0.0f;
    float beatPulse = 0.0f;
    float grooveSwing = 0.0f;
    float trebleSparkle = 0.0f;
    float stereoDrift = 0.0f;
    float melodicOrbit = 0.0f;
    float phraseLift = 0.0f;
    float buildTension = 0.0f;
    float dropImpact = 0.0f;
    float inertia = 0.0f;
    float breath = 0.0f;
    float snap = 0.0f;
    float orbit = 0.0f;
    float fold = 0.0f;
    float weave = 0.0f;
    float shimmer = 0.0f;
    float parallax = 0.0f;
    float foreground = 0.0f;
    float midground = 0.0f;
    float background = 0.0f;
    float stability = 0.0f;
    float clarity = 0.0f;
    MotionStyle style = MotionStyle::Liquid;
};

struct SectionNarrative3D {
    float build = 0.0f;
    float drop = 0.0f;
    float groove = 0.0f;
    float breakdown = 0.0f;
    float release = 0.0f;
    float intensity = 0.0f;
};

struct MusicSceneArc3D {
    float intensity = 0.0f;
    float anticipation = 0.0f;
    float impact = 0.0f;
    float recovery = 0.0f;
    float continuity = 0.0f;
};

struct MusicRoleScene3D {
    float bass = 0.0f;
    float drums = 0.0f;
    float melody = 0.0f;
    float harmony = 0.0f;
    float space = 0.0f;
    float fracture = 0.0f;
    float shadow = 0.0f;
    float convergence = 0.0f;
    float separation = 0.0f;
};

enum class MusicalRoleDistrict3D {
    Bass,
    Drums,
    Melody,
    Harmony,
    Space,
    Fracture,
    Shadow
};

struct SceneInterpretation {
    SceneIntent primary = SceneIntent::Calm;
    float calm = 0.0f;
    float groove = 0.0f;
    float tension = 0.0f;
    float drop = 0.0f;
    float release = 0.0f;
    float melodic = 0.0f;
    float industrial = 0.0f;
    float dark = 0.0f;
    float bright = 0.0f;
    float chaotic = 0.0f;
    float spacious = 0.0f;
    float heavy = 0.0f;
    float minimal = 0.0f;
    float mass = 0.0f;
    float architecture = 0.0f;
    float crystal = 0.0f;
    float fracture = 0.0f;
    float orbital = 0.0f;
    float shadow = 0.0f;
    float depthReveal = 0.0f;
    float cameraDrama = 0.0f;
};

float wrapUnit(float value);
float transientEnergy3D(const AudioMetrics& metrics);
float staggeredCutEnergy3D(const AudioMetrics& metrics);

MusicMotionEnvelope musicEnvelope(const AudioMetrics& metrics, const VisualSettings& settings)
{
    MusicMotionEnvelope envelope;
    envelope.stability = motionStabilityOf(settings);
    envelope.clarity = patternClarityOf(settings);

    const float loudness = clamp01(metrics.rms * 1.55f +
                                   metrics.peak * 0.22f +
                                   metrics.bass * 0.16f +
                                   metrics.lowMid * 0.08f +
                                   metrics.mid * 0.04f +
                                   metrics.treble * 0.08f);
    envelope.audible = smootherStep(0.012f, 0.11f, loudness);
    if (metrics.style == AudioStyle::Silence && loudness < 0.06f) {
        envelope.audible *= smootherStep(0.035f, 0.12f, loudness);
    }

    const float beatDecay = std::pow(clamp01(1.0f - metrics.beatPhase), 2.25f) *
                            clamp01(metrics.beatConfidence);
    const float beatHit = metrics.beat ? std::max(0.42f, metrics.beatConfidence) : 0.0f;
    envelope.beat = envelope.audible * clamp01(beatHit * 0.66f + beatDecay * 0.42f);
    envelope.energy = envelope.audible *
                      smootherStep(0.018f, 0.72f, metrics.rms * 1.24f + metrics.lowMid * 0.22f + metrics.mid * 0.18f);
    envelope.bass = envelope.audible *
                    smootherStep(0.018f, 0.92f, metrics.bass * 0.98f + metrics.lowMid * 0.18f + metrics.bandOnsets[0] * 0.12f);
    envelope.treble = envelope.audible *
                      smootherStep(0.012f, 0.82f, metrics.treble * 0.92f + metrics.highMid * 0.22f);
    envelope.stereo = envelope.audible * smootherStep(0.035f, 0.90f, metrics.stereoWidth);
    envelope.flux = envelope.audible * smootherStep(0.012f, 0.76f, metrics.spectralFlux);
    envelope.drop = envelope.audible *
                    smootherStep(0.08f, 0.96f, metrics.dropIntensity + metrics.bass * 0.13f + metrics.rms * 0.10f);
    envelope.phrase = envelope.audible *
                      clamp01(smootherStep(0.04f, 0.86f, metrics.phraseIntensity) +
                              (metrics.phraseBoundary ? 0.18f : 0.0f));
    envelope.build = envelope.audible * smootherStep(0.06f, 0.86f, metrics.buildTension);
    envelope.detail = envelope.audible *
                      clamp01(envelope.treble * 0.44f +
                              envelope.flux * 0.34f +
                              metrics.onset * 0.24f +
                              metrics.bandOnsets[4] * 0.10f);

    const float styleBlend = clamp01(metrics.styleConfidence);
    const auto styleMix = [styleBlend](float neutral, float styled) {
        return neutral + (styled - neutral) * styleBlend;
    };
    switch (metrics.style) {
    case AudioStyle::Ambient:
        envelope.beat *= styleMix(1.0f, 0.46f);
        envelope.drop *= styleMix(1.0f, 0.58f);
        envelope.detail *= styleMix(1.0f, 0.64f);
        envelope.phrase = std::min(1.0f, envelope.phrase * styleMix(1.0f, 1.18f));
        envelope.stereo = std::min(1.0f, envelope.stereo * styleMix(1.0f, 1.10f));
        break;
    case AudioStyle::Techno:
        envelope.beat = std::min(1.0f, envelope.beat * styleMix(1.0f, 1.16f));
        envelope.bass = std::min(1.0f, envelope.bass * styleMix(1.0f, 1.07f));
        envelope.flux *= styleMix(1.0f, 0.88f);
        break;
    case AudioStyle::BassHeavy:
        envelope.bass = std::min(1.0f, envelope.bass * styleMix(1.0f, 1.18f));
        envelope.drop = std::min(1.0f, envelope.drop * styleMix(1.0f, 1.10f));
        envelope.treble *= styleMix(1.0f, 0.82f);
        break;
    case AudioStyle::Bright:
        envelope.treble = std::min(1.0f, envelope.treble * styleMix(1.0f, 1.18f));
        envelope.detail = std::min(1.0f, envelope.detail * styleMix(1.0f, 1.08f));
        envelope.bass *= styleMix(1.0f, 0.86f);
        envelope.drop *= styleMix(1.0f, 0.86f);
        break;
    case AudioStyle::Wide:
        envelope.stereo = std::min(1.0f, envelope.stereo * styleMix(1.0f, 1.22f));
        envelope.phrase = std::min(1.0f, envelope.phrase * styleMix(1.0f, 1.08f));
        envelope.beat *= styleMix(1.0f, 0.86f);
        break;
    case AudioStyle::Silence:
        if (loudness < 0.08f) {
            envelope.beat = 0.0f;
            envelope.drop = 0.0f;
            envelope.flux = 0.0f;
            envelope.detail = 0.0f;
        }
        break;
    }

    const float jitterDamp = 0.42f + (1.0f - envelope.stability) * 0.58f;
    const float clarityDamp = 0.50f + (1.0f - envelope.clarity) * 0.50f;
    envelope.flux *= jitterDamp * clarityDamp;
    envelope.detail *= jitterDamp * clarityDamp;
    envelope.accent = clamp01(envelope.beat * 0.42f +
                              envelope.drop * 0.50f +
                              envelope.phrase * 0.20f +
                              envelope.build * 0.14f);
    envelope.motion = clamp01(envelope.energy * 0.42f +
                              envelope.bass * 0.26f +
                              envelope.beat * 0.24f +
                              envelope.drop * 0.32f +
                              envelope.phrase * 0.16f +
                              envelope.detail * 0.12f);
    envelope.camera = clamp01((envelope.bass * 0.34f +
                               envelope.drop * 0.34f +
                               envelope.stereo * 0.18f +
                               envelope.phrase * 0.14f +
                               envelope.build * 0.12f) *
                              (0.50f + (1.0f - envelope.stability) * 0.50f) *
                              (0.68f + (1.0f - envelope.clarity) * 0.32f));
    return envelope;
}

MusicChoreography buildMusicChoreography(const AudioMetrics& metrics,
                                         const VisualSettings& settings,
                                         VisualMode mode,
                                         double time,
                                         float speed)
{
    const MusicMotionEnvelope envelope = musicEnvelope(metrics, settings);
    const float phase = static_cast<float>(time) * speed;
    const float beatPhase = clamp01(metrics.beatPhase);
    const float phrasePhase = clamp01(metrics.phrasePhase);
    const float barPhase = clamp01(metrics.barPhase);
    const float beatDecay = std::pow(clamp01(1.0f - beatPhase), 2.55f);
    const float phraseWave = 0.5f + 0.5f * std::sin(phrasePhase * 2.0f * kPi);
    const float barSwing = std::sin((barPhase * 2.0f + 0.25f) * kPi);
    const float melodicSeed = metrics.keyIndex >= 0
                                  ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f
                                  : metrics.spectralCentroid;
    const float harmonicOrbit = wrapUnit(melodicSeed +
                                         metrics.harmonicEnergy * 0.18f +
                                         metrics.keyConfidence * 0.13f +
                                         std::sin(phase * 0.045f) * 0.035f);

    MusicChoreography motion;
    motion.audible = envelope.audible;
    motion.style = settings.motionStyle;
    motion.stability = envelope.stability;
    motion.clarity = envelope.clarity;
    motion.bassPressure = envelope.bass * (0.70f + response3DOf(settings) * 0.64f);
    motion.beatPulse = clamp01(envelope.beat * 0.62f + beatDecay * envelope.beat * 0.42f);
    motion.grooveSwing = barSwing * envelope.beat * (0.24f + envelope.bass * 0.22f + envelope.stereo * 0.16f);
    motion.trebleSparkle = clamp01(envelope.treble * 0.66f + envelope.detail * 0.44f);
    motion.stereoDrift = (std::sin(phase * 0.09f + envelope.stereo * kPi) * 0.55f +
                          std::sin(phase * 0.027f + metrics.sectionProgress * kPi) * 0.45f) *
                         envelope.stereo;
    motion.melodicOrbit = harmonicOrbit;
    motion.phraseLift = clamp01(envelope.phrase * 0.72f + phraseWave * envelope.phrase * 0.28f);
    motion.buildTension = envelope.build;
    motion.dropImpact = clamp01(envelope.drop * 0.78f + envelope.accent * 0.28f);
    motion.inertia = clamp01((envelope.energy * 0.28f +
                              envelope.bass * 0.24f +
                              envelope.phrase * 0.24f +
                              envelope.stereo * 0.16f +
                              beatDecay * envelope.beat * 0.18f) *
                             (0.58f + (1.0f - envelope.stability) * 0.42f));
    motion.breath = std::sin(phase * (0.20f + envelope.energy * 0.08f) + phrasePhase * 2.0f * kPi) *
                    (0.22f + motion.bassPressure * 0.28f + motion.phraseLift * 0.20f);
    motion.snap = clamp01(motion.beatPulse * 0.72f + motion.dropImpact * 0.34f);
    motion.orbit = wrapUnit(harmonicOrbit + phase * (0.018f + envelope.stereo * 0.024f + envelope.phrase * 0.016f));
    motion.fold = clamp01(motion.buildTension * 0.52f + motion.dropImpact * 0.38f + metrics.harmonicEnergy * 0.18f);
    motion.weave = std::sin(phase * (0.16f + envelope.stereo * 0.08f) + harmonicOrbit * 2.0f * kPi) *
                   clamp01(envelope.stereo * 0.48f + envelope.phrase * 0.24f + envelope.detail * 0.18f);
    motion.shimmer = clamp01(motion.trebleSparkle * (0.48f + (1.0f - envelope.clarity) * 0.24f) +
                             envelope.flux * 0.32f);
    motion.parallax = clamp01(envelope.stereo * 0.44f + motion.bassPressure * 0.22f + motion.phraseLift * 0.18f);
    motion.foreground = clamp01(motion.dropImpact * 0.40f + motion.beatPulse * 0.28f + motion.trebleSparkle * 0.22f);
    motion.midground = clamp01(envelope.energy * 0.36f + motion.phraseLift * 0.28f + motion.grooveSwing * motion.grooveSwing * 0.20f);
    motion.background = clamp01(motion.bassPressure * 0.24f + motion.stereoDrift * motion.stereoDrift * 0.20f + motion.buildTension * 0.26f);

    switch (motion.style) {
    case MotionStyle::Smooth:
        motion.snap *= 0.56f;
        motion.grooveSwing *= 0.62f;
        motion.inertia = std::min(1.0f, motion.inertia * 1.18f);
        motion.breath = motion.breath * 1.16f;
        motion.shimmer *= 0.72f;
        break;
    case MotionStyle::Mechanical:
        motion.grooveSwing = std::round(motion.grooveSwing * 5.0f) / 5.0f;
        motion.snap = std::min(1.0f, motion.snap * 1.25f);
        motion.breath *= 0.58f;
        motion.fold = std::min(1.0f, motion.fold * 1.10f);
        break;
    case MotionStyle::Liquid:
        motion.weave = std::min(1.0f, motion.weave * 1.20f);
        motion.orbit = wrapUnit(motion.orbit + std::sin(phase * 0.075f) * 0.025f);
        motion.snap *= 0.80f;
        motion.breath *= 1.12f;
        break;
    case MotionStyle::Hyperspace:
        motion.fold = std::min(1.0f, motion.fold * 1.28f);
        motion.parallax = std::min(1.0f, motion.parallax * 1.18f);
        motion.stereoDrift *= 1.18f;
        motion.dropImpact = std::min(1.0f, motion.dropImpact * 1.10f);
        break;
    case MotionStyle::HeavyBass:
        motion.bassPressure = std::min(1.0f, motion.bassPressure * 1.34f);
        motion.dropImpact = std::min(1.0f, motion.dropImpact * 1.16f);
        motion.trebleSparkle *= 0.76f;
        motion.breath *= 1.20f;
        break;
    case MotionStyle::AmbientDrift:
        motion.beatPulse *= 0.42f;
        motion.snap *= 0.36f;
        motion.phraseLift = std::min(1.0f, motion.phraseLift * 1.26f + envelope.energy * 0.10f);
        motion.stereoDrift *= 1.22f;
        motion.inertia = std::min(1.0f, motion.inertia * 1.30f + envelope.energy * 0.08f);
        motion.shimmer *= 0.62f;
        break;
    case MotionStyle::Breakbeat:
        motion.snap = std::min(1.0f, motion.snap * 1.34f + envelope.flux * 0.18f);
        motion.grooveSwing *= 1.22f;
        motion.trebleSparkle = std::min(1.0f, motion.trebleSparkle * 1.18f);
        motion.inertia *= 0.90f;
        break;
    }

    switch (mode) {
    case VisualMode::QuantumTunnel:
        motion.bassPressure = std::min(1.0f, motion.bassPressure * 1.16f);
        motion.fold = std::min(1.0f, motion.fold * 0.80f + motion.bassPressure * 0.18f);
        break;
    case VisualMode::TechnoMandala:
        motion.snap = std::min(1.0f, motion.snap * 1.14f);
        motion.orbit = wrapUnit(motion.orbit + beatPhase * 0.06f);
        break;
    case VisualMode::PolyrhythmLattice:
        motion.grooveSwing *= 1.28f;
        motion.snap = std::min(1.0f, motion.snap * 1.12f);
        break;
    case VisualMode::NeuralConstellation:
        motion.phraseLift = std::min(1.0f, motion.phraseLift * 1.10f + metrics.barConfidence * 0.10f);
        motion.melodicOrbit = wrapUnit(motion.melodicOrbit + metrics.barPhase * 0.10f);
        break;
    case VisualMode::HyperspacePolytope:
        motion.fold = std::min(1.0f, motion.fold * 1.24f);
        motion.parallax = std::min(1.0f, motion.parallax * 1.20f);
        break;
    case VisualMode::CymaticInterference:
        motion.breath = std::sin(phase * 0.12f + metrics.phrasePhase * 2.0f * kPi) *
                        (0.18f + metrics.harmonicEnergy * 0.34f + motion.buildTension * 0.20f);
        motion.shimmer = std::min(1.0f, motion.shimmer * 0.88f + metrics.harmonicEnergy * 0.18f);
        break;
    default:
        break;
    }

    const float readability = 0.62f + (1.0f - motion.clarity) * 0.18f + (1.0f - motion.stability) * 0.20f;
    motion.bassPressure = clamp01(motion.bassPressure * readability);
    motion.dropImpact = clamp01(motion.dropImpact * readability);
    motion.snap = clamp01(motion.snap * readability);
    motion.shimmer = clamp01(motion.shimmer * (0.78f + (1.0f - motion.clarity) * 0.22f));
    return motion;
}

AudioMetrics stabilizedMetrics(const AudioMetrics& metrics, const VisualSettings& settings)
{
    const MusicMotionEnvelope envelope = musicEnvelope(metrics, settings);
    AudioMetrics shaped = metrics;
    if (envelope.audible <= 0.001f) {
        shaped = AudioMetrics{};
        shaped.timeSeconds = metrics.timeSeconds;
        shaped.beatPhase = metrics.beatPhase;
        shaped.barPhase = metrics.barPhase;
        shaped.phrasePhase = metrics.phrasePhase;
        shaped.bpm = metrics.bpm;
        return shaped;
    }

    const float jitterDamp = 0.48f + (1.0f - envelope.stability) * 0.52f;
    const float clarityDamp = 0.56f + (1.0f - envelope.clarity) * 0.44f;
    shaped.rms = envelope.energy;
    shaped.peak = clamp01(envelope.energy + envelope.accent * 0.22f);
    shaped.bass = envelope.bass;
    shaped.lowMid = clamp01(envelope.energy * 0.42f + envelope.bass * 0.34f + metrics.lowMid * 0.18f * envelope.audible);
    shaped.mid = clamp01(envelope.energy * 0.52f + metrics.mid * 0.18f * envelope.audible);
    shaped.highMid = clamp01(envelope.energy * 0.32f + envelope.treble * 0.40f + metrics.highMid * 0.15f * envelope.audible);
    shaped.treble = envelope.treble;
    shaped.spectralFlux = envelope.flux;
    shaped.stereoWidth = envelope.stereo;
    shaped.onset = clamp01((envelope.beat * 0.55f + envelope.drop * 0.25f + envelope.detail * 0.25f) * jitterDamp);
    shaped.beatConfidence = envelope.beat;
    shaped.dropIntensity = envelope.drop;
    shaped.phraseIntensity = envelope.phrase;
    shaped.buildTension = envelope.build;
    shaped.downbeatConfidence *= envelope.audible;
    shaped.phraseConfidence *= envelope.audible;
    shaped.sectionConfidence *= envelope.audible;
    shaped.harmonicEnergy = clamp01(metrics.harmonicEnergy * (0.54f + envelope.energy * 0.46f));
    shaped.keyConfidence *= envelope.audible;
    shaped.beat = metrics.beat && envelope.beat > 0.20f;
    shaped.downbeat = metrics.downbeat && envelope.beat > 0.20f;
    shaped.phraseBoundary = metrics.phraseBoundary && envelope.phrase > 0.16f;
    if (!shaped.beat && shaped.beatConfidence < 0.18f) {
        shaped.beatConfidence = 0.0f;
    }
    for (float& bin : shaped.spectrum) {
        bin = std::clamp(bin * envelope.audible * (0.76f + envelope.energy * 0.24f) * clarityDamp, 0.0f, 1.0f);
    }
    for (float& onset : shaped.bandOnsets) {
        onset = std::clamp(onset * envelope.audible * jitterDamp * clarityDamp, 0.0f, 1.0f);
    }
    for (float& chroma : shaped.chroma) {
        chroma = std::clamp(chroma * (0.68f + envelope.energy * 0.32f), 0.0f, 1.0f);
    }
    return shaped;
}

float musicResponse3D(const AudioMetrics& metrics, const VisualSettings& settings)
{
    const MusicMotionEnvelope envelope = musicEnvelope(metrics, settings);
    const float userGain = 0.42f + response3DOf(settings) * 1.34f;
    const float musicalDrive = envelope.energy * 0.56f +
                               envelope.bass * 0.46f +
                               envelope.beat * 0.30f +
                               envelope.drop * 0.54f +
                               envelope.phrase * 0.24f +
                               envelope.build * 0.18f +
                               envelope.treble * 0.14f +
                               envelope.stereo * 0.12f +
                               envelope.detail * 0.16f;
    const float readabilityDamp = (0.62f + (1.0f - envelope.stability) * 0.38f) *
                                  (0.72f + (1.0f - envelope.clarity) * 0.28f);
    const float cap = 1.72f + (1.0f - envelope.stability) * 0.40f + (1.0f - envelope.clarity) * 0.24f;
    return std::clamp(0.08f + musicalDrive * userGain * readabilityDamp, 0.08f, cap);
}

float averageOnsetEnergy(const AudioMetrics& metrics)
{
    float total = 0.0f;
    for (float onset : metrics.bandOnsets) {
        total += onset;
    }
    return total / static_cast<float>(metrics.bandOnsets.size());
}

float averageChromaEnergy(const AudioMetrics& metrics)
{
    float total = 0.0f;
    for (float chroma : metrics.chroma) {
        total += chroma;
    }
    return total / static_cast<float>(metrics.chroma.size());
}

SceneIntent strongestIntent(const SceneInterpretation& intent)
{
    SceneIntent best = SceneIntent::Calm;
    float value = intent.calm;
    const auto choose = [&](SceneIntent candidate, float score) {
        if (score > value) {
            best = candidate;
            value = score;
        }
    };
    choose(SceneIntent::Groove, intent.groove);
    choose(SceneIntent::Tension, intent.tension);
    choose(SceneIntent::Drop, intent.drop);
    choose(SceneIntent::Release, intent.release);
    choose(SceneIntent::Melodic, intent.melodic);
    choose(SceneIntent::Industrial, intent.industrial);
    choose(SceneIntent::Dark, intent.dark);
    choose(SceneIntent::Bright, intent.bright);
    choose(SceneIntent::Chaotic, intent.chaotic);
    choose(SceneIntent::Spacious, intent.spacious);
    choose(SceneIntent::Heavy, intent.heavy);
    choose(SceneIntent::Minimal, intent.minimal);
    return best;
}

SceneInterpretation interpretSceneIntent(const AudioMetrics& metrics, const VisualSettings& settings)
{
    const MusicMotionEnvelope envelope = musicEnvelope(metrics, settings);
    const float styleConfidence = std::clamp(metrics.styleConfidence, 0.0f, 1.0f);
    const float styleWeight = 0.45f + styleConfidence * 0.55f;
    const float energy = clamp01(metrics.rms * 1.35f +
                                 metrics.peak * 0.10f +
                                 metrics.bass * 0.22f +
                                 metrics.lowMid * 0.10f +
                                 metrics.spectralFlux * 0.16f);
    const float beatRegularity = clamp01(metrics.beatConfidence * 0.68f +
                                         metrics.barConfidence * 0.20f +
                                         metrics.downbeatConfidence * 0.12f);
    const float density = clamp01(energy * 0.48f +
                                  metrics.spectralFlux * 0.34f +
                                  averageOnsetEnergy(metrics) * 0.26f +
                                  metrics.beatConfidence * 0.18f);
    const float harmonic = clamp01(metrics.harmonicEnergy * 0.66f +
                                   metrics.keyConfidence * 0.24f +
                                   averageChromaEnergy(metrics) * 0.18f);
    const float lowColor = clamp01(metrics.bass * 0.54f + metrics.lowMid * 0.24f + (1.0f - metrics.treble) * 0.12f);
    const float highColor = clamp01(metrics.treble * 0.50f + metrics.highMid * 0.25f + metrics.spectralCentroid * 0.20f);
    const float transient = clamp01(metrics.onset * 0.42f +
                                    metrics.spectralFlux * 0.34f +
                                    averageOnsetEnergy(metrics) * 0.30f);

    SceneInterpretation intent;
    intent.calm = clamp01((1.0f - energy) * 0.78f +
                          (metrics.style == AudioStyle::Silence ? 0.45f : 0.0f) +
                          (metrics.style == AudioStyle::Ambient ? 0.18f * styleWeight : 0.0f));
    intent.groove = clamp01(beatRegularity * 0.66f +
                            metrics.bass * 0.22f +
                            (metrics.section == ArrangementSection::Groove ? 0.28f * metrics.sectionConfidence : 0.0f) +
                            (metrics.style == AudioStyle::Techno ? 0.18f * styleWeight : 0.0f));
    intent.tension = clamp01(metrics.buildTension * 0.70f +
                             metrics.phraseIntensity * 0.18f +
                             metrics.sectionProgress * (metrics.section == ArrangementSection::Build ? 0.24f : 0.0f) +
                             metrics.spectralFlux * 0.14f);
    intent.drop = clamp01(metrics.dropIntensity * 0.78f +
                          (metrics.section == ArrangementSection::Drop ? 0.28f * metrics.sectionConfidence : 0.0f) +
                          metrics.bass * 0.18f +
                          metrics.onset * 0.12f);
    intent.release = clamp01((metrics.section == ArrangementSection::Drop ? metrics.sectionProgress * 0.22f : 0.0f) +
                             (metrics.phraseBoundary ? metrics.phraseConfidence * 0.28f : 0.0f) +
                             metrics.phraseIntensity * 0.26f +
                             (1.0f - metrics.buildTension) * energy * 0.18f);
    intent.melodic = clamp01(harmonic * 0.72f +
                             metrics.phraseConfidence * 0.18f +
                             (metrics.keyIndex >= 0 ? metrics.keyConfidence * 0.24f : 0.0f));
    intent.industrial = clamp01((metrics.style == AudioStyle::Techno ? 0.34f * styleWeight : 0.0f) +
                                beatRegularity * 0.36f +
                                lowColor * 0.20f +
                                (1.0f - harmonic) * 0.16f);
    intent.dark = clamp01(lowColor * 0.38f +
                          (metrics.keyMode == MusicalMode::Minor ? metrics.keyConfidence * 0.22f : 0.0f) +
                          (1.0f - highColor) * 0.18f +
                          (settings.palette == Palette::InfraredChrome || settings.palette == Palette::MonochromeLaser ? 0.12f : 0.0f));
    intent.bright = clamp01(highColor * 0.64f +
                            harmonic * 0.18f +
                            (metrics.keyMode == MusicalMode::Major ? metrics.keyConfidence * 0.14f : 0.0f) +
                            (settings.palette == Palette::AcidAurora ? 0.12f : 0.0f));
    intent.chaotic = clamp01(transient * 0.72f +
                             metrics.spectralFlux * 0.34f +
                             (metrics.style == AudioStyle::Bright && metrics.onset > 0.32f ? 0.20f * styleWeight : 0.0f) +
                             (metrics.beatConfidence < 0.28f && density > 0.42f ? 0.12f : 0.0f));
    intent.spacious = clamp01((metrics.stereoWidth * 0.62f +
                               (metrics.style == AudioStyle::Wide ? 0.28f * styleWeight : 0.0f) +
                               (metrics.style == AudioStyle::Ambient ? 0.22f * styleWeight : 0.0f) +
                               (1.0f - density) * 0.16f) *
                              (1.0f - transient * 0.42f));
    intent.heavy = clamp01(metrics.bass * 0.58f +
                           metrics.lowMid * 0.22f +
                           metrics.dropIntensity * 0.24f +
                           (metrics.style == AudioStyle::BassHeavy ? 0.28f * styleWeight : 0.0f));
    intent.minimal = clamp01((1.0f - density) * 0.50f +
                             beatRegularity * 0.18f +
                             (metrics.style == AudioStyle::Techno ? 0.12f * styleWeight : 0.0f) +
                             (metrics.section == ArrangementSection::Breakdown ? 0.20f * metrics.sectionConfidence : 0.0f));

    if (envelope.audible <= 0.02f || metrics.style == AudioStyle::Silence) {
        intent.calm = std::max(intent.calm, 0.88f);
        intent.minimal = std::max(intent.minimal, 0.62f);
        intent.drop = 0.0f;
        intent.chaotic = 0.0f;
        intent.heavy *= 0.25f;
    }

    intent.primary = strongestIntent(intent);
    intent.mass = clamp01(intent.heavy * 0.68f + intent.drop * 0.42f + metrics.bass * 0.28f);
    intent.architecture = clamp01(intent.industrial * 0.58f + intent.groove * 0.36f + intent.tension * 0.22f);
    intent.crystal = clamp01(intent.melodic * 0.46f + intent.bright * 0.42f + harmonic * 0.24f);
    intent.fracture = clamp01(intent.chaotic * 0.58f + transient * 0.30f + intent.drop * 0.16f);
    intent.orbital = clamp01(intent.spacious * 0.48f + intent.melodic * 0.28f + intent.calm * 0.16f);
    intent.shadow = clamp01(intent.dark * 0.52f + intent.minimal * 0.26f + (1.0f - highColor) * 0.18f);
    intent.depthReveal = clamp01(intent.spacious * 0.34f + intent.drop * 0.28f + intent.tension * 0.22f + metrics.stereoWidth * 0.20f);
    intent.cameraDrama = clamp01(intent.drop * 0.40f + intent.tension * 0.24f + intent.spacious * 0.18f + intent.chaotic * 0.12f);
    return intent;
}

float followArcValue(float current, float target, float dt, float attackSeconds, float releaseSeconds)
{
    const float seconds = target > current ? attackSeconds : releaseSeconds;
    const float alpha = 1.0f - std::exp(-dt / std::max(0.001f, seconds));
    return current + (target - current) * std::clamp(alpha, 0.0f, 1.0f);
}

MusicSceneArc3D updateSongArcMemory(SongArcMemory& memory,
                                    const AudioMetrics& metrics,
                                    const VisualSettings& settings,
                                    const SceneInterpretation& intent,
                                    double timeSeconds)
{
    float dt = 1.0f / 60.0f;
    if (memory.lastTimeSeconds >= 0.0) {
        dt = static_cast<float>(timeSeconds - memory.lastTimeSeconds);
    }
    if (memory.lastTimeSeconds < 0.0 ||
        dt <= 0.0001f ||
        dt > 8.0f) {
        memory = SongArcMemory{};
        dt = 1.0f / 60.0f;
    }
    dt = std::clamp(dt, 1.0f / 120.0f, 0.75f);

    const float confidence = clamp01(metrics.sectionConfidence);
    const float progress = clamp01(metrics.sectionProgress);
    const float arcAudibility = (metrics.style == AudioStyle::Silence &&
                                 metrics.rms < 0.025f &&
                                 metrics.peak < 0.055f)
                                    ? 0.0f
                                    : clamp01(0.22f + metrics.rms * 2.4f + metrics.peak * 0.50f);
    const float phrase = clamp01(metrics.phraseIntensity * 0.54f +
                                 metrics.phraseConfidence * 0.28f +
                                 (metrics.phraseBoundary ? metrics.phraseConfidence * 0.24f : 0.0f)) *
                         arcAudibility;
    const float melodicLift = clamp01(metrics.melodyRole * 0.24f +
                                      metrics.harmonyRole * 0.20f +
                                      metrics.harmonicEnergy * 0.22f +
                                      metrics.keyConfidence * 0.12f +
                                      averageChromaEnergy(metrics) * 0.18f +
                                      intent.melodic * 0.22f +
                                      intent.spacious * 0.08f) *
                              arcAudibility;
    const float buildTarget = clamp01(metrics.buildTension * 0.54f +
                                      intent.tension * 0.28f +
                                      phrase * (0.16f + melodicLift * 0.08f) +
                                      melodicLift * 0.035f +
                                      (metrics.section == ArrangementSection::Build
                                           ? confidence * (0.28f + progress * 0.62f)
                                           : 0.0f) -
                                      metrics.dropIntensity * 0.28f);
    const float impactTarget = clamp01(metrics.dropIntensity * 0.58f +
                                       intent.drop * 0.28f +
                                       metrics.bassRole * 0.10f +
                                       metrics.convergenceRole * 0.18f +
                                       (metrics.downbeat ? metrics.downbeatConfidence * 0.12f : 0.0f) +
                                       (metrics.section == ArrangementSection::Drop
                                            ? confidence * (0.34f + (1.0f - progress) * 0.42f)
                                            : 0.0f));
    const float recoveryTarget = clamp01((metrics.section == ArrangementSection::Drop
                                              ? confidence * smootherStep(0.34f, 0.94f, progress) *
                                                    (0.40f + phrase * 0.28f)
                                              : 0.0f) +
                                         (metrics.phraseBoundary ? metrics.phraseConfidence * 0.24f : 0.0f) +
                                         intent.release * 0.32f +
                                         melodicLift * phrase * 0.26f);
    const float continuityTarget = clamp01(confidence * 0.32f +
                                           metrics.barConfidence * 0.18f +
                                           metrics.roleSeparation * 0.20f +
                                           std::max({intent.groove, intent.spacious, intent.melodic, intent.heavy}) * 0.20f +
                                           melodicLift * 0.15f);

    memory.anticipation = followArcValue(memory.anticipation, buildTarget, dt, 0.42f, 0.86f);
    memory.impact = followArcValue(memory.impact, impactTarget, dt, 0.075f, 0.70f);
    memory.recovery = followArcValue(memory.recovery, recoveryTarget, dt, 0.30f, 1.05f);
    memory.continuity = followArcValue(memory.continuity, continuityTarget, dt, 0.54f, 1.35f);

    if (memory.lastSection == ArrangementSection::Build &&
        metrics.section == ArrangementSection::Drop &&
        confidence > 0.35f) {
        memory.impact = std::max(memory.impact, clamp01(0.50f + confidence * 0.34f + metrics.dropIntensity * 0.18f));
        memory.anticipation *= 0.54f;
    }
    if (memory.lastSection == ArrangementSection::Drop &&
        metrics.section != ArrangementSection::Drop &&
        confidence > 0.25f) {
        memory.recovery = std::max(memory.recovery, clamp01(0.34f + memory.impact * 0.34f + phrase * 0.22f));
    }
    if (metrics.phraseBoundary && metrics.phraseConfidence > 0.25f) {
        const float phraseLift = clamp01(0.055f +
                                         phrase * 0.18f +
                                         melodicLift * 0.22f +
                                         intent.release * 0.12f);
        memory.recovery = std::max(memory.recovery, phraseLift);
        memory.anticipation = std::max(memory.anticipation, phraseLift * (0.18f + melodicLift * 0.12f));
    }

    memory.lastTimeSeconds = timeSeconds;
    memory.lastSection = metrics.section;

    const float stabilityDamp = 0.82f + (1.0f - motionStabilityOf(settings)) * 0.18f;
    MusicSceneArc3D arc;
    arc.anticipation = clamp01(memory.anticipation * stabilityDamp);
    arc.impact = clamp01(memory.impact * stabilityDamp);
    arc.recovery = clamp01(memory.recovery * (0.88f + patternClarityOf(settings) * 0.12f));
    arc.continuity = clamp01(memory.continuity);
    arc.intensity = std::max({arc.anticipation, arc.impact, arc.recovery, arc.continuity * 0.72f});
    return arc;
}

MusicRoleScene3D buildMusicRoleScene3D(const SceneInterpretation& intent,
                                       const AudioMetrics& metrics,
                                       const VisualSettings& settings)
{
    const MusicMotionEnvelope envelope = musicEnvelope(metrics, settings);
    const float audible = metrics.style == AudioStyle::Silence
                              ? clamp01(0.28f + metrics.rms * 1.4f)
                              : std::max(envelope.audible, smootherStep(0.025f, 0.16f, metrics.rms));
    const float styleWeight = std::clamp(metrics.styleConfidence, 0.0f, 1.0f);
    const float onsetField = averageOnsetEnergy(metrics);
    const float harmonic = clamp01(metrics.harmonicEnergy * 0.58f +
                                   metrics.keyConfidence * 0.30f +
                                   averageChromaEnergy(metrics) * 0.20f);
    const float highTexture = clamp01(metrics.highMid * 0.38f +
                                      metrics.treble * 0.36f +
                                      metrics.spectralCentroid * 0.22f);
    const float highBandCuts = clamp01(std::max(metrics.bandOnsets[2], std::max(metrics.bandOnsets[3], metrics.bandOnsets[4])) * 0.48f +
                                       metrics.spectralFlux * 0.32f +
                                       metrics.onset * 0.22f +
                                       onsetField * 0.18f);
    const float staggeredCut = staggeredCutEnergy3D(metrics);
    const float drumClock = clamp01(metrics.beatConfidence * 0.48f +
                                    metrics.barConfidence * 0.24f +
                                    metrics.downbeatConfidence * 0.16f +
                                    onsetField * 0.24f +
                                    metrics.onset * 0.12f);
    const float convergenceEvent = clamp01(metrics.convergenceRole * 0.48f +
                                           metrics.dropIntensity * 0.46f +
                                           (metrics.phraseBoundary ? metrics.phraseConfidence * 0.34f : 0.0f) +
                                           (metrics.downbeat ? metrics.downbeatConfidence * 0.24f : 0.0f) +
                                           metrics.buildTension * metrics.phraseConfidence * 0.10f +
                                           transientEnergy3D(metrics) * metrics.dropIntensity * 0.14f);

    MusicRoleScene3D role;
    const float bassContext = clamp01(0.36f +
                                      intent.heavy * 0.24f +
                                      intent.drop * 0.18f +
                                      metrics.dropIntensity * 0.14f +
                                      (metrics.style == AudioStyle::BassHeavy ? 0.28f * styleWeight : 0.0f));
    role.bass = clamp01(audible *
                        (metrics.bass * 0.62f +
                         metrics.lowMid * 0.22f +
                         metrics.dropIntensity * 0.24f +
                         intent.heavy * 0.18f +
                         (metrics.style == AudioStyle::BassHeavy ? 0.20f * styleWeight : 0.0f)) *
                        bassContext);
    role.drums = clamp01(audible *
                         (drumClock * 0.76f +
                          intent.groove * 0.18f +
                          (metrics.style == AudioStyle::Techno ? 0.12f * styleWeight : 0.0f)));
    role.melody = clamp01(audible *
                          (harmonic * 0.48f +
                           highTexture * 0.22f +
                           intent.melodic * 0.22f +
                           intent.bright * 0.12f) *
                          (0.72f + (1.0f - metrics.bass) * 0.28f));
    role.harmony = clamp01(audible *
                           (harmonic * 0.60f +
                            metrics.phraseConfidence * 0.16f +
                            metrics.phraseIntensity * 0.12f +
                            intent.melodic * 0.12f));
    const float spaceDamp = std::clamp(1.0f - highBandCuts * 0.38f - intent.chaotic * 0.20f - metrics.dropIntensity * 0.10f,
                                       0.46f,
                                       1.0f);
    role.space = clamp01((metrics.style == AudioStyle::Silence ? 0.42f : audible) *
                         (metrics.stereoWidth * 0.54f +
                          intent.spacious * 0.28f +
                          depth3DOf(settings) * 0.12f +
                          (metrics.style == AudioStyle::Ambient || metrics.style == AudioStyle::Wide ? 0.18f * styleWeight : 0.0f)) *
                         spaceDamp);
    role.fracture = clamp01(audible *
                            (intent.chaotic * 0.42f +
                             transientEnergy3D(metrics) * 0.34f +
                             staggeredCut * 0.42f +
                             highBandCuts * 0.58f +
                             highTexture * 0.18f +
                             (metrics.style == AudioStyle::Bright ? 0.24f * styleWeight : 0.0f)) *
                            (0.76f + (1.0f - metrics.beatConfidence) * 0.12f));
    role.shadow = clamp01(audible *
                          (intent.dark * 0.44f +
                           intent.minimal * 0.24f +
                           (1.0f - highTexture) * metrics.bass * 0.26f +
                           (metrics.keyMode == MusicalMode::Minor ? metrics.keyConfidence * 0.14f : 0.0f) +
                           (settings.palette == Palette::MonochromeLaser ? 0.10f : 0.0f)));
    role.convergence = clamp01(audible * convergenceEvent +
                               role.bass * metrics.dropIntensity * 0.14f +
                               role.harmony * (metrics.phraseBoundary ? metrics.phraseConfidence * 0.10f : 0.0f));

    const float analyzerRoleSum = metrics.bassRole +
                                  metrics.drumRole +
                                  metrics.melodyRole +
                                  metrics.harmonyRole +
                                  metrics.spaceRole +
                                  metrics.fractureRole +
                                  metrics.shadowRole +
                                  metrics.convergenceRole;
    if (analyzerRoleSum > 0.001f || metrics.roleSeparation > 0.001f) {
        const float roleBlend = std::clamp(0.66f + metrics.roleSeparation * 0.24f, 0.66f, 0.90f);
        const auto blendRole = [roleBlend](float fallback, float analyzed) {
            return clamp01(fallback + (std::clamp(analyzed, 0.0f, 1.0f) - fallback) * roleBlend);
        };
        role.bass = blendRole(role.bass, metrics.bassRole);
        role.drums = blendRole(role.drums, metrics.drumRole);
        role.melody = blendRole(role.melody, metrics.melodyRole);
        role.harmony = blendRole(role.harmony, metrics.harmonyRole);
        role.space = blendRole(role.space, metrics.spaceRole);
        role.fracture = blendRole(role.fracture, metrics.fractureRole);
        role.shadow = blendRole(role.shadow, metrics.shadowRole);
        role.convergence = blendRole(role.convergence, std::max(metrics.convergenceRole, metrics.dropIntensity * 0.45f));
        role.separation = clamp01(std::max(role.separation * 0.44f,
                                           metrics.roleSeparation * 0.92f +
                                               patternClarityOf(settings) * 0.08f));
    }

    const bool lockedDrumArchitecture = metrics.style == AudioStyle::Techno &&
                                        metrics.stereoWidth < 0.36f &&
                                        metrics.spectralFlux < 0.24f &&
                                        role.drums > role.fracture + 0.045f &&
                                        (metrics.beatConfidence > 0.16f ||
                                         metrics.barConfidence > 0.36f ||
                                         metrics.downbeatConfidence > 0.20f);
    if (lockedDrumArchitecture) {
        role.drums = std::max(role.drums,
                              clamp01(audible * (0.20f +
                                                  drumClock * 0.78f +
                                                  metrics.barConfidence * 0.20f +
                                                  metrics.downbeatConfidence * 0.14f)));
        role.fracture *= 0.54f;
        role.space *= 0.78f;
        role.separation = std::max(role.separation, clamp01(role.drums * 0.72f));
    }

    const float activeRoles = (role.bass > 0.16f ? 1.0f : 0.0f) +
                              (role.drums > 0.16f ? 1.0f : 0.0f) +
                              (role.melody > 0.16f ? 1.0f : 0.0f) +
                              (role.harmony > 0.16f ? 1.0f : 0.0f) +
                              (role.space > 0.16f ? 1.0f : 0.0f) +
                              (role.fracture > 0.16f ? 1.0f : 0.0f) +
                              (role.shadow > 0.16f ? 1.0f : 0.0f);
    role.separation = clamp01(0.42f +
                              activeRoles * 0.07f +
                              patternClarityOf(settings) * 0.18f +
                              motionStabilityOf(settings) * 0.12f -
                              role.convergence * 0.08f);
    if (analyzerRoleSum > 0.001f || metrics.roleSeparation > 0.001f) {
        role.separation = clamp01(std::max(role.separation,
                                           metrics.roleSeparation * 0.86f +
                                               patternClarityOf(settings) * 0.10f));
    }
    return role;
}

float roleDistrictStrength(const MusicRoleScene3D& role, MusicalRoleDistrict3D district)
{
    switch (district) {
    case MusicalRoleDistrict3D::Bass:
        return role.bass;
    case MusicalRoleDistrict3D::Drums:
        return role.drums;
    case MusicalRoleDistrict3D::Melody:
        return role.melody;
    case MusicalRoleDistrict3D::Harmony:
        return role.harmony;
    case MusicalRoleDistrict3D::Space:
        return role.space;
    case MusicalRoleDistrict3D::Fracture:
        return role.fracture;
    case MusicalRoleDistrict3D::Shadow:
        return role.shadow;
    }
    return 0.0f;
}

Vec3 roleDistrictUnitOffset(MusicalRoleDistrict3D district)
{
    switch (district) {
    case MusicalRoleDistrict3D::Bass:
        return Vec3{0.00f, 0.28f, -0.66f};
    case MusicalRoleDistrict3D::Drums:
        return Vec3{-0.74f, 0.02f, -0.12f};
    case MusicalRoleDistrict3D::Melody:
        return Vec3{0.68f, -0.34f, 0.10f};
    case MusicalRoleDistrict3D::Harmony:
        return Vec3{0.24f, -0.20f, 0.52f};
    case MusicalRoleDistrict3D::Space:
        return Vec3{-0.12f, -0.04f, 0.92f};
    case MusicalRoleDistrict3D::Fracture:
        return Vec3{0.80f, 0.14f, -0.34f};
    case MusicalRoleDistrict3D::Shadow:
        return Vec3{-0.56f, 0.28f, 0.38f};
    }
    return Vec3{};
}

float activeRoleCount3D(const MusicRoleScene3D& role)
{
    return (role.bass > 0.16f ? 1.0f : 0.0f) +
           (role.drums > 0.16f ? 1.0f : 0.0f) +
           (role.melody > 0.16f ? 1.0f : 0.0f) +
           (role.harmony > 0.16f ? 1.0f : 0.0f) +
           (role.space > 0.16f ? 1.0f : 0.0f) +
           (role.fracture > 0.16f ? 1.0f : 0.0f) +
           (role.shadow > 0.16f ? 1.0f : 0.0f);
}

Object3DRole objectRoleForDistrict(MusicalRoleDistrict3D district)
{
    switch (district) {
    case MusicalRoleDistrict3D::Bass:
        return Object3DRole::Bass;
    case MusicalRoleDistrict3D::Drums:
        return Object3DRole::Drums;
    case MusicalRoleDistrict3D::Melody:
        return Object3DRole::Melody;
    case MusicalRoleDistrict3D::Harmony:
        return Object3DRole::Harmony;
    case MusicalRoleDistrict3D::Space:
        return Object3DRole::Space;
    case MusicalRoleDistrict3D::Fracture:
        return Object3DRole::Fracture;
    case MusicalRoleDistrict3D::Shadow:
        return Object3DRole::Shadow;
    }
    return Object3DRole::Unassigned;
}

bool districtForObjectRole(Object3DRole role, MusicalRoleDistrict3D& district)
{
    switch (role) {
    case Object3DRole::Bass:
        district = MusicalRoleDistrict3D::Bass;
        return true;
    case Object3DRole::Drums:
        district = MusicalRoleDistrict3D::Drums;
        return true;
    case Object3DRole::Melody:
        district = MusicalRoleDistrict3D::Melody;
        return true;
    case Object3DRole::Harmony:
        district = MusicalRoleDistrict3D::Harmony;
        return true;
    case Object3DRole::Space:
        district = MusicalRoleDistrict3D::Space;
        return true;
    case Object3DRole::Fracture:
        district = MusicalRoleDistrict3D::Fracture;
        return true;
    case Object3DRole::Shadow:
        district = MusicalRoleDistrict3D::Shadow;
        return true;
    case Object3DRole::Unassigned:
    case Object3DRole::Bridge:
    case Object3DRole::Convergence:
        break;
    }
    return false;
}

bool isExplicitMusicalPartRole(Object3DRole role)
{
    MusicalRoleDistrict3D district = MusicalRoleDistrict3D::Bass;
    return districtForObjectRole(role, district);
}

Vec3 roleConvergencePoint3D(const MusicRoleScene3D& role, float minimumDimension)
{
    return Vec3{
        0.0f,
        minimumDimension * (-0.02f + role.bass * 0.04f - role.melody * 0.025f),
        minimumDimension * (0.04f - role.bass * 0.18f + role.space * 0.16f)
    };
}

Vec3 roleDistrictCenter3D(MusicalRoleDistrict3D district,
                          const MusicRoleScene3D& role,
                          const SceneInterpretation& intent,
                          const AudioMetrics& metrics,
                          const VisualSettings& settings,
                          float minimumDimension,
                          double time)
{
    const float phase = static_cast<float>(time);
    const float separation = clamp01(role.separation * (1.0f - role.convergence * 0.22f));
    const float stageScale = minimumDimension *
                             (0.32f +
                              separation * 0.48f +
                              scenePersonalityOf(settings) * 0.12f +
                              patternClarityOf(settings) * 0.06f);
    const Vec3 convergence = roleConvergencePoint3D(role, minimumDimension);
    Vec3 center = add(convergence, scale(roleDistrictUnitOffset(district), stageScale));
    const float harmonicPhase = (metrics.keyIndex >= 0
                                     ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f
                                     : metrics.spectralCentroid) *
                                    2.0f * kPi +
                                metrics.phrasePhase * kPi +
                                phase * 0.030f;
    const float beatStep = std::floor(clamp01(metrics.beatPhase) * 8.0f);
    const float beatLane = (static_cast<int>(beatStep) % 2 == 0) ? -1.0f : 1.0f;

    switch (district) {
    case MusicalRoleDistrict3D::Bass:
        center.y += minimumDimension * role.bass * (0.034f + metrics.dropIntensity * 0.030f);
        center.z -= minimumDimension * role.bass * (0.080f + metrics.dropIntensity * 0.060f);
        break;
    case MusicalRoleDistrict3D::Drums:
        center.x += std::round(std::sin(metrics.barPhase * 2.0f * kPi) * 3.0f) *
                    minimumDimension * (0.014f + role.drums * 0.010f);
        center.z += metrics.beatConfidence * minimumDimension * 0.018f;
        break;
    case MusicalRoleDistrict3D::Melody:
        center.x += std::sin(harmonicPhase) * minimumDimension * (0.032f + role.melody * 0.020f);
        center.y -= minimumDimension * (role.melody * 0.034f + intent.melodic * 0.018f);
        center.z += std::cos(harmonicPhase * 0.7f) * minimumDimension * role.melody * 0.030f;
        break;
    case MusicalRoleDistrict3D::Harmony:
        center.x += std::cos(harmonicPhase * 0.5f) * minimumDimension * role.harmony * 0.022f;
        center.z += minimumDimension * (role.harmony * 0.060f + metrics.phraseConfidence * 0.026f);
        break;
    case MusicalRoleDistrict3D::Space:
        center.x += std::sin(phase * 0.028f + metrics.stereoWidth * kPi) *
                    minimumDimension * metrics.stereoWidth * 0.050f;
        center.z += minimumDimension * (metrics.stereoWidth * 0.090f + intent.spacious * 0.050f);
        break;
    case MusicalRoleDistrict3D::Fracture:
        center.x += beatLane * minimumDimension * role.fracture * (0.032f + metrics.spectralFlux * 0.028f);
        center.z += ((static_cast<int>(beatStep) % 3) - 1) * minimumDimension * role.fracture * 0.036f;
        center.y += std::sin(phase * 0.11f + beatStep) * minimumDimension * role.fracture * 0.022f;
        break;
    case MusicalRoleDistrict3D::Shadow:
        center.y += minimumDimension * (role.shadow * 0.052f + intent.dark * 0.018f);
        center.z += minimumDimension * (role.shadow * 0.082f + intent.minimal * 0.038f);
        center.x *= 0.92f - intent.minimal * 0.06f;
        break;
    }

    const float convergencePull = clamp01(role.convergence *
                                          (0.34f +
                                           metrics.dropIntensity * 0.24f +
                                           (metrics.phraseBoundary ? metrics.phraseConfidence * 0.18f : 0.0f)));
    const float preserveDistrict = 1.0f - convergencePull * 0.42f;
    return mix(convergence, center, std::clamp(preserveDistrict, 0.48f, 1.0f));
}

MusicalRoleDistrict3D classifyRoleDistrict3D(const Object3D& object,
                                             const MusicRoleScene3D& role,
                                             float minimumDimension,
                                             std::size_t index)
{
    MusicalRoleDistrict3D explicitDistrict = MusicalRoleDistrict3D::Bass;
    if (districtForObjectRole(object.musicRole, explicitDistrict)) {
        return explicitDistrict;
    }

    std::array<float, 7> score{
        role.bass * 0.52f,
        role.drums * 0.52f,
        role.melody * 0.52f,
        role.harmony * 0.52f,
        role.space * 0.52f,
        role.fracture * 0.52f,
        role.shadow * 0.52f
    };
    const auto addScore = [&score](MusicalRoleDistrict3D district, float amount) {
        score[static_cast<std::size_t>(district)] += amount;
    };

    switch (object.kind) {
    case Object3DKind::TunnelRib:
        addScore(MusicalRoleDistrict3D::Bass, 0.72f);
        addScore(MusicalRoleDistrict3D::Drums, 0.12f);
        break;
    case Object3DKind::Column:
        addScore(MusicalRoleDistrict3D::Drums, 0.34f);
        addScore(MusicalRoleDistrict3D::Bass, object.position.z < 0.0f ? 0.28f : 0.12f);
        addScore(MusicalRoleDistrict3D::Shadow,
                 object.position.y > minimumDimension * 0.06f || role.shadow > role.drums ? 0.46f : 0.16f);
        break;
    case Object3DKind::Ribbon:
        addScore(MusicalRoleDistrict3D::Melody, 0.62f);
        addScore(MusicalRoleDistrict3D::Harmony, 0.24f);
        break;
    case Object3DKind::Node:
        addScore(MusicalRoleDistrict3D::Melody, 0.44f);
        addScore(MusicalRoleDistrict3D::Harmony, 0.28f);
        addScore(MusicalRoleDistrict3D::Drums, 0.12f);
        break;
    case Object3DKind::Link:
        addScore(MusicalRoleDistrict3D::Harmony, 0.34f);
        addScore(MusicalRoleDistrict3D::Drums, 0.18f);
        addScore(MusicalRoleDistrict3D::Space, 0.18f);
        break;
    case Object3DKind::Plate:
        addScore(MusicalRoleDistrict3D::Fracture, 0.66f);
        addScore(MusicalRoleDistrict3D::Drums, 0.14f);
        break;
    case Object3DKind::Shard:
        addScore(MusicalRoleDistrict3D::Fracture, role.fracture > role.melody * 0.82f ? 0.58f : 0.26f);
        addScore(MusicalRoleDistrict3D::Melody, 0.42f);
        break;
    case Object3DKind::DepthPlane:
    case Object3DKind::Orbiter:
    case Object3DKind::WaveSurface:
        addScore(MusicalRoleDistrict3D::Space, 0.62f);
        addScore(MusicalRoleDistrict3D::Harmony, 0.20f);
        break;
    case Object3DKind::Cage:
        addScore(MusicalRoleDistrict3D::Harmony, 0.34f);
        addScore(MusicalRoleDistrict3D::Drums, 0.24f);
        addScore(MusicalRoleDistrict3D::Melody, 0.16f);
        break;
    case Object3DKind::Anchor:
        addScore(MusicalRoleDistrict3D::Shadow, 0.54f);
        addScore(MusicalRoleDistrict3D::Space, 0.18f);
        addScore(MusicalRoleDistrict3D::Bass, 0.12f);
        break;
    case Object3DKind::Polyhedron:
    case Object3DKind::Particle: {
        const std::array<MusicalRoleDistrict3D, 7> districts{
            MusicalRoleDistrict3D::Bass,
            MusicalRoleDistrict3D::Drums,
            MusicalRoleDistrict3D::Melody,
            MusicalRoleDistrict3D::Harmony,
            MusicalRoleDistrict3D::Space,
            MusicalRoleDistrict3D::Fracture,
            MusicalRoleDistrict3D::Shadow
        };
        const MusicalRoleDistrict3D seeded = districts[(index * 5U + static_cast<std::size_t>(object.kind)) % districts.size()];
        addScore(seeded, 0.28f);
        break;
    }
    }

    if (object.position.z < -minimumDimension * 0.12f) {
        addScore(MusicalRoleDistrict3D::Bass, 0.12f);
        addScore(MusicalRoleDistrict3D::Fracture, 0.10f);
    } else if (object.position.z > minimumDimension * 0.28f) {
        addScore(MusicalRoleDistrict3D::Space, 0.16f);
        addScore(MusicalRoleDistrict3D::Harmony, 0.08f);
        addScore(MusicalRoleDistrict3D::Shadow, 0.08f);
    }
    if (object.position.y < -minimumDimension * 0.04f) {
        addScore(MusicalRoleDistrict3D::Melody, 0.12f);
        addScore(MusicalRoleDistrict3D::Harmony, 0.08f);
    } else if (object.position.y > minimumDimension * 0.07f) {
        addScore(MusicalRoleDistrict3D::Bass, 0.10f);
        addScore(MusicalRoleDistrict3D::Shadow, 0.14f);
    }
    if (object.position.x < -minimumDimension * 0.06f) {
        addScore(MusicalRoleDistrict3D::Drums, 0.10f);
        addScore(MusicalRoleDistrict3D::Shadow, 0.08f);
    } else if (object.position.x > minimumDimension * 0.06f) {
        addScore(MusicalRoleDistrict3D::Melody, 0.08f);
        addScore(MusicalRoleDistrict3D::Fracture, 0.10f);
    }

    std::size_t best = 0U;
    for (std::size_t i = 1U; i < score.size(); ++i) {
        if (score[i] > score[best]) {
            best = i;
        }
    }
    return static_cast<MusicalRoleDistrict3D>(best);
}

void applyRoleDialectStaging3D(Object3D& object,
                               MusicalRoleDistrict3D district,
                               const MusicRoleScene3D& role,
                               const AudioMetrics& metrics,
                               float strength,
                               float minimumDimension,
                               double time)
{
    const float roleStrength = roleDistrictStrength(role, district);
    const float phase = static_cast<float>(time);
    switch (district) {
    case MusicalRoleDistrict3D::Bass:
        object.scale.y *= 1.0f + roleStrength * strength * 0.13f;
        object.scale.z *= 1.0f + roleStrength * strength * (0.11f + metrics.dropIntensity * 0.10f);
        object.rotation.z *= 1.0f - strength * 0.28f;
        object.glow += roleStrength * strength * 0.09f;
        break;
    case MusicalRoleDistrict3D::Drums: {
        const float cell = std::max(1.0f, minimumDimension * (0.030f + role.drums * 0.012f));
        const float snappedX = std::round(object.position.x / cell) * cell;
        object.position.x += (snappedX - object.position.x) * clamp01(strength * 0.42f);
        object.rotation.z = std::round(object.rotation.z * 8.0f) / 8.0f;
        object.glow += metrics.beatConfidence * roleStrength * strength * 0.08f;
        break;
    }
    case MusicalRoleDistrict3D::Melody:
        object.rotation.y += std::sin(phase * 0.035f + metrics.phrasePhase * kPi) * roleStrength * strength * 0.10f;
        object.rotation.z += metrics.keyConfidence * roleStrength * strength * 0.07f;
        object.scale = scale(object.scale, 1.0f + metrics.harmonicEnergy * roleStrength * strength * 0.050f);
        object.glow += metrics.harmonicEnergy * roleStrength * strength * 0.08f;
        break;
    case MusicalRoleDistrict3D::Harmony:
        object.scale.x *= 1.0f + metrics.harmonicEnergy * roleStrength * strength * 0.08f;
        object.scale.z *= 1.0f + metrics.phraseConfidence * roleStrength * strength * 0.06f;
        object.rotation.x += roleStrength * strength * 0.04f;
        break;
    case MusicalRoleDistrict3D::Space:
        object.scale.x *= 1.0f + metrics.stereoWidth * roleStrength * strength * 0.12f;
        object.color.a *= std::clamp(0.94f + roleStrength * 0.10f, 0.92f, 1.04f);
        object.glow += metrics.stereoWidth * roleStrength * strength * 0.04f;
        break;
    case MusicalRoleDistrict3D::Fracture:
        object.rotation.y += ((static_cast<int>(std::floor(clamp01(metrics.beatPhase) * 8.0f)) % 2) == 0 ? -1.0f : 1.0f) *
                             roleStrength * strength * 0.16f;
        object.rotation.z += std::sin(phase * 0.12f + roleStrength * 3.0f) * roleStrength * strength * 0.11f;
        object.glow += transientEnergy3D(metrics) * roleStrength * strength * 0.09f;
        break;
    case MusicalRoleDistrict3D::Shadow:
        object.scale.y *= 1.0f + roleStrength * strength * 0.16f;
        object.rotation.x *= 1.0f - strength * 0.20f;
        object.rotation.z *= 1.0f - strength * 0.42f;
        object.color.a = clamp01(object.color.a * (1.0f + roleStrength * 0.08f));
        break;
    }
}

void applyMotionDialectStaging3D(Object3D& object,
                                 MotionStyle style,
                                 const MusicRoleScene3D& role,
                                 const AudioMetrics& metrics,
                                 float strength,
                                 float minimumDimension,
                                 double time,
                                 std::size_t index)
{
    const float phase = static_cast<float>(time);
    const float seed = static_cast<float>(index + 1U) * 0.619f +
                       static_cast<float>(static_cast<int>(object.kind)) * 0.283f;
    const float sign = std::sin(seed * 4.0f) >= 0.0f ? 1.0f : -1.0f;
    const float stableStrength = strength;

    switch (style) {
    case MotionStyle::Smooth:
        object.position.z += std::sin(phase * 0.050f + seed) * minimumDimension * stableStrength * 0.010f;
        object.rotation.x *= 1.0f - stableStrength * 0.16f;
        object.rotation.y *= 1.0f - stableStrength * 0.12f;
        object.rotation.z *= 1.0f - stableStrength * 0.20f;
        break;
    case MotionStyle::Mechanical: {
        const float cell = std::max(1.0f, minimumDimension * (0.026f + metrics.beatConfidence * 0.010f));
        object.position.x = std::round(object.position.x / cell) * cell;
        object.position.z = std::round(object.position.z / (cell * 1.4f)) * (cell * 1.4f);
        object.rotation.x = std::round(object.rotation.x * 10.0f) / 10.0f;
        object.rotation.y = std::round(object.rotation.y * 10.0f) / 10.0f;
        object.rotation.z = std::round(object.rotation.z * 12.0f) / 12.0f;
        object.scale.y *= 1.0f + metrics.beatConfidence * stableStrength * 0.08f;
        break;
    }
    case MotionStyle::Liquid:
        object.position.x += std::sin(phase * 0.072f + seed) *
                             minimumDimension * stableStrength * (0.018f + role.melody * 0.020f);
        object.position.y += std::cos(phase * 0.055f + seed * 0.7f) *
                             minimumDimension * stableStrength * (0.012f + role.space * 0.016f);
        object.rotation.y += std::sin(seed + metrics.phrasePhase * kPi) * stableStrength * 0.050f;
        break;
    case MotionStyle::Hyperspace:
        rotatePlane(object.position.x,
                    object.position.z,
                    sign * stableStrength * (0.030f + role.space * 0.025f + metrics.dropIntensity * 0.020f));
        object.position.z *= 1.0f + stableStrength * (0.018f + metrics.stereoWidth * 0.022f);
        object.scale.z *= 1.0f + stableStrength * 0.070f;
        object.rotation.y += sign * stableStrength * 0.060f;
        break;
    case MotionStyle::HeavyBass:
        object.position.y += minimumDimension * role.bass * stableStrength * 0.022f;
        object.position.z -= minimumDimension * role.bass * stableStrength * (0.044f + metrics.dropIntensity * 0.030f);
        object.scale.y *= 1.0f + role.bass * stableStrength * 0.090f;
        object.scale.z *= 1.0f + role.bass * stableStrength * 0.085f;
        object.glow += role.bass * stableStrength * 0.060f;
        break;
    case MotionStyle::AmbientDrift:
        object.position.x *= 1.0f + metrics.stereoWidth * stableStrength * 0.052f;
        object.position.z += minimumDimension * role.space * stableStrength *
                             (0.050f + 0.018f * std::sin(phase * 0.030f + seed));
        object.rotation.x *= 1.0f - stableStrength * 0.28f;
        object.rotation.z *= 1.0f - stableStrength * 0.22f;
        object.glow *= 1.0f - stableStrength * 0.030f;
        break;
    case MotionStyle::Breakbeat:
        object.position.x += sign * minimumDimension * stableStrength *
                             (0.038f + role.fracture * 0.050f + metrics.spectralFlux * 0.018f);
        object.position.z += std::sin(seed * 5.0f + metrics.beatPhase * kPi * 2.0f) *
                             minimumDimension * stableStrength * (0.026f + role.fracture * 0.044f);
        object.rotation.y += sign * stableStrength * (0.080f + role.fracture * 0.070f);
        object.rotation.z += std::sin(seed * 2.0f) * stableStrength * 0.090f;
        object.glow += transientEnergy3D(metrics) * stableStrength * 0.050f;
        break;
    }
}

void applyMusicalRoleDistrictStaging3D(std::vector<Object3D>& objects,
                                       const MusicRoleScene3D& role,
                                       const SceneInterpretation& intent,
                                       const AudioMetrics& metrics,
                                       const VisualSettings& settings,
                                       float minimumDimension,
                                       double time)
{
    if (objects.empty()) {
        return;
    }

    const float activeCount = activeRoleCount3D(role);
    const float activeMass = role.bass + role.drums + role.melody + role.harmony +
                             role.space + role.fracture + role.shadow;
    if (activeCount < 1.5f && metrics.style == AudioStyle::Silence && metrics.rms < 0.04f) {
        return;
    }

    Vec3 sceneCenter{};
    for (const Object3D& object : objects) {
        sceneCenter = add(sceneCenter, object.position);
    }
    sceneCenter = scale(sceneCenter, 1.0f / static_cast<float>(objects.size()));

    const float separation = clamp01(role.separation * (1.0f - role.convergence * 0.30f));
    const float stagingStrength = std::clamp(0.22f +
                                                 separation * 0.44f +
                                                 scenePersonalityOf(settings) * 0.16f +
                                                 patternClarityOf(settings) * 0.12f +
                                                 std::min(activeMass / 3.8f, 1.0f) * 0.10f,
                                             0.18f,
                                             0.86f);
    const float keepPattern = std::clamp(0.42f +
                                             (1.0f - separation) * 0.12f +
                                             role.convergence * 0.20f +
                                             (1.0f - patternClarityOf(settings)) * 0.10f,
                                         0.38f,
                                         0.78f);
    const Vec3 convergence = roleConvergencePoint3D(role, minimumDimension);

    for (std::size_t i = 0; i < objects.size(); ++i) {
        Object3D& object = objects[i];
        MusicalRoleDistrict3D explicitDistrict = MusicalRoleDistrict3D::Bass;
        const bool explicitRole = districtForObjectRole(object.musicRole, explicitDistrict);
        const bool bridgeRole = object.musicRole == Object3DRole::Bridge;
        const bool convergenceRole = object.musicRole == Object3DRole::Convergence;
        const MusicalRoleDistrict3D district = explicitRole
                                                   ? explicitDistrict
                                                   : classifyRoleDistrict3D(object, role, minimumDimension, i);
        const float districtStrength = roleDistrictStrength(role, district);
        if (districtStrength <= 0.035f && activeCount < 2.0f) {
            continue;
        }

        if (!explicitRole && !bridgeRole && !convergenceRole && activeCount >= 3.0f) {
            const float backgroundYield = clamp01(role.separation * (1.0f - role.convergence * 0.36f));
            object.position.z += minimumDimension * backgroundYield * 0.115f;
            object.color.a *= std::clamp(0.72f - backgroundYield * 0.34f, 0.34f, 0.82f);
            object.glow *= std::clamp(0.70f - backgroundYield * 0.22f, 0.42f, 0.78f);
            object.scale = scale(object.scale, 0.88f + role.convergence * 0.055f);
        }

        const Vec3 center = roleDistrictCenter3D(district, role, intent, metrics, settings, minimumDimension, time);
        const Vec3 relative = subtract(object.position, sceneCenter);
        const float seed = static_cast<float>(i + 1U) * 0.571f +
                           static_cast<float>(static_cast<int>(object.kind)) * 0.317f;
        const Vec3 filament{
            std::sin(seed * 2.3f) * minimumDimension * (0.030f + districtStrength * 0.032f),
            std::cos(seed * 1.7f) * minimumDimension * (0.018f + districtStrength * 0.024f),
            std::sin(seed * 3.1f) * minimumDimension * (0.040f + districtStrength * 0.042f)
        };
        const float authoredKeepPattern = explicitRole ? keepPattern * 0.78f :
                                          (bridgeRole ? keepPattern * 0.92f : keepPattern * 1.10f);
        Vec3 districtTarget = add(add(center, scale(relative, authoredKeepPattern)), filament);
        const float authoredStagingStrength = explicitRole ? stagingStrength * 1.14f :
                                              (bridgeRole ? stagingStrength * 0.58f : stagingStrength * 0.46f);
        const float hold = std::clamp(authoredStagingStrength *
                                          (0.46f + districtStrength * 0.42f + activeCount * 0.018f),
                                      0.14f,
                                      object.kind == Object3DKind::Link || bridgeRole ? 0.52f : 0.78f);
        object.position = mix(object.position, districtTarget, hold);

        if (object.kind == Object3DKind::Link) {
            const Vec3 targetRelative = subtract(object.target, sceneCenter);
            Vec3 target = add(add(center, scale(targetRelative, authoredKeepPattern)), scale(filament, 0.52f));
            object.target = mix(object.target, target, hold * 0.82f);
        }

        const float convergencePull = clamp01(role.convergence *
                                              (0.16f +
                                               metrics.dropIntensity * 0.16f +
                                               (metrics.phraseBoundary ? metrics.phraseConfidence * 0.12f : 0.0f)));
        if (convergencePull > 0.01f) {
            const Vec3 districtRendezvous = add(convergence,
                                                scale(roleDistrictUnitOffset(district),
                                                      minimumDimension * (0.15f + districtStrength * 0.095f) *
                                                          (1.0f - role.convergence * 0.34f)));
            object.position = mix(object.position,
                                  districtRendezvous,
                                  convergencePull * (0.28f + districtStrength * 0.18f));
            if (object.kind == Object3DKind::Link) {
                object.target = mix(object.target, districtRendezvous, convergencePull * 0.22f);
            }
        }

        if (bridgeRole) {
            const float bridgePresence = clamp01(0.34f + role.convergence * 0.56f + metrics.dropIntensity * 0.18f);
            object.color.a *= bridgePresence;
            object.glow *= std::clamp(0.48f + role.convergence * 0.58f, 0.36f, 1.0f);
            object.scale = scale(object.scale, 0.82f + role.convergence * 0.18f);
        } else if (convergenceRole) {
            const float eventPresence = clamp01(0.24f + role.convergence * 1.18f);
            object.color.a *= eventPresence;
            object.glow *= std::clamp(0.30f + role.convergence * 1.28f, 0.20f, 1.25f);
            object.scale = scale(object.scale, 0.72f + role.convergence * 0.44f);
        }

        applyRoleDialectStaging3D(object,
                                  district,
                                  role,
                                  metrics,
                                  hold,
                                  minimumDimension,
                                  time);
        applyMotionDialectStaging3D(object,
                                    settings.motionStyle,
                                    role,
                                    metrics,
                                    hold,
                                    minimumDimension,
                                    time,
                                    i);
        if (bridgeRole) {
            const float bridgePresence = clamp01(0.34f + role.convergence * 0.56f + metrics.dropIntensity * 0.18f);
            object.color.a *= bridgePresence;
            object.glow *= std::clamp(0.48f + role.convergence * 0.58f, 0.36f, 1.0f);
        } else if (convergenceRole) {
            const float eventPresence = clamp01(0.24f + role.convergence * 1.18f);
            object.color.a *= eventPresence;
            object.glow *= std::clamp(0.30f + role.convergence * 1.28f, 0.20f, 1.25f);
        }
    }
}

void applyRoleVisualVocabulary3D(std::vector<Object3D>& objects,
                                 const MusicRoleScene3D& role,
                                 const AudioMetrics& metrics,
                                 const VisualSettings& settings,
                                 float minimumDimension)
{
    if (objects.empty()) {
        return;
    }

    const float clarity = patternClarityOf(settings);
    const float personality = scenePersonalityOf(settings);
    const float vocabularyGain = std::clamp(0.48f + clarity * 0.30f + personality * 0.18f,
                                            0.42f,
                                            0.96f);
    const int beatStep = static_cast<int>(std::floor(clamp01(metrics.beatPhase) * 8.0f));
    const float cutSign = (beatStep % 2 == 0) ? -1.0f : 1.0f;

    for (Object3D& object : objects) {
        MusicalRoleDistrict3D district = MusicalRoleDistrict3D::Bass;
        if (!districtForObjectRole(object.musicRole, district)) {
            continue;
        }

        const float roleStrength = roleDistrictStrength(role, district);
        const float strength = clamp01(roleStrength * vocabularyGain);
        if (strength <= 0.025f) {
            continue;
        }

        const auto tint = [&](ColorRGBA target, float amount) {
            object.color = mix(object.color, target, clamp01(amount * strength));
            object.color.a = clamp01(object.color.a);
        };
        const auto move = [&](Vec3 delta) {
            object.position = add(object.position, delta);
            if (object.kind == Object3DKind::Link) {
                object.target = add(object.target, scale(delta, 0.72f));
            }
        };

        switch (district) {
        case MusicalRoleDistrict3D::Bass: {
            const float pressure = clamp01(metrics.bass * 0.62f + metrics.dropIntensity * 0.36f + role.bass * 0.22f);
            tint(ColorRGBA{1.0f, 0.28f, 0.12f, object.color.a}, 0.34f + pressure * 0.16f);
            object.scale.x *= 0.88f + pressure * 0.04f;
            object.scale.y *= 1.10f + pressure * 0.22f;
            object.scale.z *= 1.14f + pressure * 0.28f;
            move(Vec3{0.0f,
                      minimumDimension * strength * (0.018f + pressure * 0.024f),
                      -minimumDimension * strength * (0.045f + pressure * 0.085f)});
            object.rotation.x *= 0.54f;
            object.rotation.z *= 0.36f;
            object.glow += pressure * strength * 0.16f;
            object.color.a = clamp01(object.color.a * (1.04f + pressure * 0.12f));
            break;
        }
        case MusicalRoleDistrict3D::Drums: {
            const float clock = clamp01(metrics.beatConfidence * 0.54f +
                                        metrics.barConfidence * 0.28f +
                                        metrics.downbeatConfidence * 0.18f);
            tint(ColorRGBA{0.08f, 0.92f, 1.0f, object.color.a}, 0.30f + clock * 0.14f);
            object.scale.x *= 0.68f;
            object.scale.y *= 1.18f + clock * 0.22f;
            object.scale.z *= 0.76f;
            object.rotation.x = std::round(object.rotation.x * 10.0f) / 10.0f;
            object.rotation.y = std::round(object.rotation.y * 10.0f) / 10.0f;
            object.rotation.z = std::round(object.rotation.z * 12.0f) / 12.0f;
            move(Vec3{-minimumDimension * strength * 0.018f,
                      0.0f,
                      minimumDimension * clock * strength * 0.020f});
            object.glow += clock * strength * 0.13f;
            break;
        }
        case MusicalRoleDistrict3D::Melody: {
            const float tonal = clamp01(metrics.harmonicEnergy * 0.58f +
                                        metrics.keyConfidence * 0.28f +
                                        metrics.melodyRole * 0.16f);
            tint(ColorRGBA{1.0f, 0.25f, 0.92f, object.color.a}, 0.34f + tonal * 0.16f);
            object.scale.x *= 0.82f + tonal * 0.05f;
            object.scale.y *= 0.78f + tonal * 0.04f;
            object.scale.z *= 1.22f + tonal * 0.16f;
            move(Vec3{minimumDimension * std::sin(metrics.phrasePhase * kPi) * strength * 0.016f,
                      -minimumDimension * strength * (0.034f + tonal * 0.040f),
                      minimumDimension * tonal * strength * 0.048f});
            object.rotation.y += tonal * strength * 0.16f;
            object.rotation.z += metrics.keyConfidence * strength * 0.12f;
            object.glow += tonal * strength * 0.19f;
            object.color.a = clamp01(object.color.a * (1.05f + tonal * 0.08f));
            break;
        }
        case MusicalRoleDistrict3D::Harmony: {
            const float chord = clamp01(metrics.harmonicEnergy * 0.62f +
                                        metrics.harmonyRole * 0.26f +
                                        metrics.phraseConfidence * 0.12f);
            tint(ColorRGBA{0.32f, 0.78f, 1.0f, object.color.a}, 0.28f + chord * 0.15f);
            object.scale.x *= 1.22f + chord * 0.22f;
            object.scale.y *= 0.90f + chord * 0.06f;
            object.scale.z *= 1.16f + chord * 0.20f;
            move(Vec3{(object.position.x >= 0.0f ? 1.0f : -1.0f) * minimumDimension * chord * strength * 0.018f,
                      -minimumDimension * chord * strength * 0.012f,
                      minimumDimension * chord * strength * 0.064f});
            object.rotation.x += chord * strength * 0.10f;
            object.rotation.z *= 0.68f;
            object.glow += chord * strength * 0.16f;
            object.color.a = clamp01(object.color.a * (1.00f + chord * 0.10f));
            break;
        }
        case MusicalRoleDistrict3D::Space: {
            const float width = clamp01(metrics.stereoWidth * 0.74f + role.space * 0.26f);
            tint(ColorRGBA{0.26f, 0.45f, 1.0f, object.color.a}, 0.30f + width * 0.12f);
            object.scale.x *= 1.30f + width * 0.26f;
            object.scale.y *= 0.78f + width * 0.10f;
            object.scale.z *= 1.22f + width * 0.22f;
            move(Vec3{0.0f,
                      -minimumDimension * width * strength * 0.010f,
                      minimumDimension * strength * (0.095f + width * 0.140f)});
            object.rotation.x *= 0.42f;
            object.rotation.z *= 0.50f;
            object.glow += width * strength * 0.10f;
            object.color.a = clamp01(object.color.a * (0.88f + width * 0.12f));
            break;
        }
        case MusicalRoleDistrict3D::Fracture: {
            const float transient = clamp01(metrics.onset * 0.42f +
                                            metrics.spectralFlux * 0.34f +
                                            transientEnergy3D(metrics) * 0.32f);
            tint(ColorRGBA{0.98f, 1.0f, 0.10f, object.color.a}, 0.36f + transient * 0.18f);
            object.scale.x *= 1.12f + transient * 0.16f;
            object.scale.y *= 0.64f + transient * 0.12f;
            object.scale.z *= 0.68f + transient * 0.14f;
            move(Vec3{cutSign * minimumDimension * strength * (0.040f + transient * 0.045f),
                      minimumDimension * transient * strength * 0.020f,
                      cutSign * minimumDimension * strength * (0.028f + transient * 0.055f)});
            object.rotation.y += cutSign * strength * (0.18f + transient * 0.20f);
            object.rotation.z = std::round((object.rotation.z + metrics.beatPhase * kPi) / (kPi / 6.0f)) * (kPi / 6.0f);
            object.glow += transient * strength * 0.21f;
            object.color.a = clamp01(object.color.a * (1.02f + transient * 0.14f));
            break;
        }
        case MusicalRoleDistrict3D::Shadow: {
            const float mass = clamp01(role.shadow * 0.62f +
                                       role.bass * 0.14f +
                                       (1.0f - metrics.treble) * 0.18f);
            tint(ColorRGBA{0.70f, 0.70f, 0.82f, object.color.a}, 0.34f + mass * 0.10f);
            object.scale.x *= 0.66f + mass * 0.06f;
            object.scale.y *= 1.24f + mass * 0.28f;
            object.scale.z *= 0.72f + mass * 0.10f;
            move(Vec3{-minimumDimension * strength * 0.016f,
                      minimumDimension * strength * (0.042f + mass * 0.060f),
                      minimumDimension * strength * (0.045f + mass * 0.080f)});
            object.rotation.x *= 0.30f;
            object.rotation.y *= 0.38f;
            object.rotation.z *= 0.16f;
            object.glow += mass * strength * 0.07f;
            object.color.a = clamp01(object.color.a * (0.98f + mass * 0.11f));
            break;
        }
        }
    }
}

float environmentDrive(const VisualSettings& settings, const EnvironmentState& environment)
{
    if (!settings.environmentReactive || !environment.enabled) {
        return 0.0f;
    }
    return clamp01(0.45f + environment.ambient * 0.35f + environment.motion * 0.2f);
}

int scaledCount(int baseCount, float quality)
{
    return std::max(3, static_cast<int>(std::round(static_cast<float>(baseCount) * quality)));
}

int scaledStep(float quality)
{
    if (quality < 0.58f) {
        return 3;
    }
    if (quality < 0.82f) {
        return 2;
    }
    return 1;
}

int primitiveFootprint(const GeometryFrame& frame)
{
    int count = static_cast<int>(frame.rings.size() + frame.beams.size() + frame.particles.size());
    for (const Polyline& line : frame.polylines) {
        count += static_cast<int>(line.points.size());
        if (line.filled) {
            count += static_cast<int>(line.points.size()) * 2;
        }
    }
    return count;
}

float primitiveVisualWeight(const GeometryFrame& frame)
{
    float weight = 0.0f;
    for (const Ring& ring : frame.rings) {
        weight += ring.color.a * std::max(0.2f, ring.strokeWidth) * 6.0f;
    }
    for (const Beam& beam : frame.beams) {
        weight += beam.color.a * std::max(0.2f, beam.width) * 4.0f;
    }
    for (const Particle& particle : frame.particles) {
        weight += particle.color.a * std::max(0.6f, particle.radius) * 1.6f;
    }
    for (const Polyline& line : frame.polylines) {
        const float pointWeight = static_cast<float>(std::max<std::size_t>(1U, line.points.size()));
        weight += line.color.a * std::max(0.2f, line.strokeWidth) * pointWeight;
        if (line.filled) {
            weight += line.color.a * std::max(1.0f, line.strokeWidth * 3.0f) * pointWeight * 2.8f;
        }
    }
    return weight;
}

float projectedLineWeight(ColorRGBA color, float strokeWidth, std::size_t pointCount)
{
    return color.a *
           std::max(0.2f, strokeWidth) *
           static_cast<float>(std::max<std::size_t>(1U, pointCount));
}

template <typename T>
void retainEvenlySpaced(std::vector<T>& items, std::size_t maximum)
{
    if (maximum == 0U) {
        items.clear();
        return;
    }
    if (items.size() <= maximum) {
        return;
    }

    std::vector<T> retained;
    retained.reserve(maximum);
    const float last = static_cast<float>(items.size() - 1U);
    for (std::size_t i = 0; i < maximum; ++i) {
        const float unit = maximum > 1U ? static_cast<float>(i) / static_cast<float>(maximum - 1U) : 0.0f;
        const auto index = static_cast<std::size_t>(std::round(unit * last));
        retained.push_back(items[std::min(index, items.size() - 1U)]);
    }
    items = std::move(retained);
}

void decimatePolyline(Polyline& line, std::size_t maximumPoints)
{
    if (line.points.size() <= maximumPoints || maximumPoints < 3U) {
        return;
    }

    std::vector<Vec2> retained;
    retained.reserve(maximumPoints);
    const float last = static_cast<float>(line.points.size() - 1U);
    for (std::size_t i = 0; i < maximumPoints; ++i) {
        const float unit = maximumPoints > 1U ? static_cast<float>(i) / static_cast<float>(maximumPoints - 1U) : 0.0f;
        const auto index = static_cast<std::size_t>(std::round(unit * last));
        retained.push_back(line.points[std::min(index, line.points.size() - 1U)]);
    }
    line.points = std::move(retained);
}

float wrapUnit(float value)
{
    value = std::fmod(value, 1.0f);
    if (value < 0.0f) {
        value += 1.0f;
    }
    return value;
}

ColorRGBA shiftedHue(ColorRGBA color, float shift)
{
    shift = wrapUnit(shift);
    if (shift <= 0.0001f || shift >= 0.9999f) {
        return color;
    }

    const float maximum = std::max({color.r, color.g, color.b});
    const float minimum = std::min({color.r, color.g, color.b});
    const float delta = maximum - minimum;
    if (maximum <= 0.0001f || delta <= 0.0001f) {
        return color;
    }

    float hue = 0.0f;
    if (maximum == color.r) {
        hue = (color.g - color.b) / delta;
        if (hue < 0.0f) {
            hue += 6.0f;
        }
    } else if (maximum == color.g) {
        hue = ((color.b - color.r) / delta) + 2.0f;
    } else {
        hue = ((color.r - color.g) / delta) + 4.0f;
    }
    hue /= 6.0f;

    const float saturation = delta / maximum;
    return hsv(hue + shift, saturation, maximum, color.a);
}

float luminance(ColorRGBA color)
{
    return color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f;
}

ColorRGBA clampColor(ColorRGBA color)
{
    color.r = clamp01(color.r);
    color.g = clamp01(color.g);
    color.b = clamp01(color.b);
    color.a = clamp01(color.a);
    return color;
}

ColorRGBA gradeColor(ColorRGBA color, float saturation, float contrast, float value)
{
    const float luma = luminance(color);
    color.r = luma + (color.r - luma) * saturation;
    color.g = luma + (color.g - luma) * saturation;
    color.b = luma + (color.b - luma) * saturation;
    color.r = (0.5f + (color.r - 0.5f) * contrast) * value;
    color.g = (0.5f + (color.g - 0.5f) * contrast) * value;
    color.b = (0.5f + (color.b - 0.5f) * contrast) * value;
    return clampColor(color);
}

std::array<ColorRGBA, 5> shiftedPalette(std::array<ColorRGBA, 5> colors, float shift)
{
    shift = wrapUnit(shift);
    if (shift <= 0.0001f || shift >= 0.9999f) {
        return colors;
    }

    for (ColorRGBA& color : colors) {
        color = shiftedHue(color, shift);
    }
    return colors;
}

std::array<ColorRGBA, 5> personalityPalette(std::array<ColorRGBA, 5> colors,
                                            const VisualSettings& settings,
                                            const AudioMetrics& metrics)
{
    const float impact = colorImpactOf(settings);
    const float keyHue = metrics.keyIndex >= 0
                             ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f
                             : 0.0f;
    const float harmonicSteer = metrics.keyIndex >= 0
                                    ? metrics.keyConfidence * metrics.harmonicEnergy * keyHue * 0.28f
                                    : 0.0f;
    const float phraseSteer = metrics.phraseConfidence * metrics.phrasePhase * 0.08f;
    const float accentHue = wrapUnit(settings.hueShift +
                                     harmonicSteer * impact +
                                     phraseSteer * impact +
                                     metrics.spectralCentroid * 0.06f +
                                     metrics.dropIntensity * 0.05f);

    colors = shiftedPalette(colors, settings.hueShift);
    const float energy = clamp01(metrics.rms * 1.35f +
                                 metrics.spectralFlux * 0.22f +
                                 metrics.dropIntensity * 0.28f +
                                 metrics.buildTension * 0.18f);
    const float saturation = 1.0f + impact * (0.42f + energy * 0.36f);
    const float contrast = 1.0f + impact * (0.18f + metrics.beatConfidence * 0.16f + metrics.dropIntensity * 0.16f);
    const float value = 0.92f + impact * (0.1f + metrics.treble * 0.08f);

    for (std::size_t i = 0; i < colors.size(); ++i) {
        const float lane = static_cast<float>(i) / static_cast<float>(colors.size());
        const std::size_t chromaIndex = i * 2U + static_cast<std::size_t>(metrics.keyIndex >= 0 ? metrics.keyIndex : 0);
        const float chroma = chromaAt(metrics, chromaIndex);
        const float laneHue = wrapUnit(accentHue + lane * (0.16f + metrics.stereoWidth * 0.08f) + chroma * 0.08f);
        const float laneSaturation = std::clamp(0.68f + impact * 0.28f + metrics.treble * 0.08f, 0.0f, 1.0f);
        const float laneValue = std::clamp(0.58f + energy * 0.28f + impact * 0.3f + chroma * 0.1f, 0.0f, 1.0f);
        const ColorRGBA target = hsv(laneHue, laneSaturation, laneValue, colors[i].a);
        const float mixAmount = impact * (0.24f + lane * 0.12f + energy * 0.16f);
        colors[i] = gradeColor(mix(colors[i], target, mixAmount), saturation, contrast, value);
    }

    const ColorRGBA bassShadow = hsv(wrapUnit(accentHue + 0.5f + metrics.bass * 0.08f),
                                     0.72f + impact * 0.18f,
                                     0.10f + metrics.bass * 0.18f + impact * 0.2f,
                                     1.0f);
    colors[4] = gradeColor(mix(colors[4], bassShadow, impact * (0.36f + metrics.dropIntensity * 0.18f)),
                           1.0f + impact * 0.24f,
                           1.0f + impact * 0.18f,
                           0.88f + impact * 0.08f);
    return colors;
}

std::array<ColorRGBA, 5> paletteColors(Palette palette)
{
    switch (palette) {
    case Palette::NeonVoltage:
        return {ColorRGBA{0.03f, 0.96f, 1.0f, 1.0f},
                ColorRGBA{1.0f, 0.04f, 0.58f, 1.0f},
                ColorRGBA{0.94f, 1.0f, 0.08f, 1.0f},
                ColorRGBA{0.46f, 0.25f, 1.0f, 1.0f},
                ColorRGBA{1.0f, 1.0f, 1.0f, 1.0f}};
    case Palette::InfraredChrome:
        return {ColorRGBA{1.0f, 0.09f, 0.03f, 1.0f},
                ColorRGBA{1.0f, 0.74f, 0.12f, 1.0f},
                ColorRGBA{0.82f, 0.86f, 0.9f, 1.0f},
                ColorRGBA{0.0f, 0.0f, 0.0f, 1.0f},
                ColorRGBA{0.4f, 0.02f, 0.08f, 1.0f}};
    case Palette::AcidAurora:
        return {ColorRGBA{0.48f, 1.0f, 0.07f, 1.0f},
                ColorRGBA{0.0f, 0.76f, 0.42f, 1.0f},
                ColorRGBA{0.98f, 0.15f, 1.0f, 1.0f},
                ColorRGBA{0.08f, 0.2f, 1.0f, 1.0f},
                ColorRGBA{1.0f, 1.0f, 0.72f, 1.0f}};
    case Palette::MonochromeLaser:
        return {ColorRGBA{0.92f, 0.95f, 1.0f, 1.0f},
                ColorRGBA{0.58f, 0.66f, 0.78f, 1.0f},
                ColorRGBA{0.16f, 0.18f, 0.23f, 1.0f},
                ColorRGBA{1.0f, 1.0f, 1.0f, 1.0f},
                ColorRGBA{0.05f, 0.06f, 0.08f, 1.0f}};
    case Palette::OceanicPulse:
        return {ColorRGBA{0.0f, 0.86f, 0.92f, 1.0f},
                ColorRGBA{0.07f, 0.28f, 1.0f, 1.0f},
                ColorRGBA{1.0f, 0.39f, 0.15f, 1.0f},
                ColorRGBA{0.9f, 0.96f, 1.0f, 1.0f},
                ColorRGBA{0.02f, 0.04f, 0.12f, 1.0f}};
    }
    return {ColorRGBA{}, ColorRGBA{}, ColorRGBA{}, ColorRGBA{}, ColorRGBA{}};
}

Vec2 projectDepthPoint(Vec2 point, Vec2 vanishingPoint, Vec2 cameraOffset, float depth, float strength)
{
    depth = clamp01(depth);
    const float perspective = 0.58f + depth * 0.72f;
    Vec2 projected{
        vanishingPoint.x + (point.x - vanishingPoint.x) * perspective + cameraOffset.x * (depth - 0.5f),
        vanishingPoint.y + (point.y - vanishingPoint.y) * perspective + cameraOffset.y * (depth - 0.5f)
    };
    return mix(point, projected, strength);
}

void applyDepthCues(GeometryFrame& frame,
                    const AudioMetrics& metrics,
                    const VisualSettings& settings,
                    const std::array<ColorRGBA, 5>& colors,
                    float width,
                    float height,
                    float speed,
                    double time)
{
    const float settingStrength = depth3DOf(settings);
    if (settingStrength <= 0.001f) {
        return;
    }

    const float beatPulse = metrics.beat ? metrics.beatConfidence : 0.0f;
    const float musicPush = clamp01(metrics.bass * 0.34f +
                                    metrics.dropIntensity * 0.38f +
                                    metrics.buildTension * 0.18f +
                                    beatPulse * 0.26f);
    const float strength = std::clamp(settingStrength * (0.72f + musicPush * 0.38f + metrics.stereoWidth * 0.16f),
                                      0.0f,
                                      1.0f);
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float diagonal = std::hypot(width, height);
    const float cameraPhase = static_cast<float>(time) * speed * (0.22f + metrics.stereoWidth * 0.12f) +
                              metrics.phrasePhase * kPi * 0.35f;
    const Vec2 vanishingPoint{
        center.x + std::cos(cameraPhase) * width * (0.08f + metrics.stereoWidth * 0.08f) * strength,
        center.y + std::sin(cameraPhase * 0.73f + metrics.buildTension) * height *
                       (0.06f + metrics.buildTension * 0.08f) * strength
    };
    const Vec2 cameraOffset{
        (center.x - vanishingPoint.x) * 0.46f * strength,
        (center.y - vanishingPoint.y) * 0.46f * strength
    };

    const std::size_t originalPolylineCount = frame.polylines.size();
    const int guideCount = std::max(4, static_cast<int>(std::round(6.0f + strength * 8.0f)));
    for (int i = 0; i < guideCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(guideCount);
        const float angle = unit * 2.0f * kPi + cameraPhase * 0.24f;
        const Vec2 far = polar(vanishingPoint, minimumDimension * (0.04f + metrics.treble * 0.04f), angle);
        const Vec2 middle = polar(center, diagonal * (0.22f + unit * 0.08f), angle + std::sin(cameraPhase + unit) * 0.05f);
        const Vec2 near = polar(center, diagonal * (0.58f + metrics.bass * 0.08f + strength * 0.06f), angle);
        frame.polylines.push_back(Polyline{
            {far, middle, near},
            0.45f + strength * 0.9f + metrics.beatConfidence * 0.35f,
            withAlpha(colors[i % 4], (0.035f + musicPush * 0.05f) * strength),
            false
        });
    }

    const int shellCount = std::max(3, static_cast<int>(std::round(3.0f + strength * 4.0f)));
    for (int i = 0; i < shellCount; ++i) {
        const float unit = static_cast<float>(i + 1) / static_cast<float>(shellCount + 1);
        frame.rings.push_back(Ring{
            projectDepthPoint(center, vanishingPoint, cameraOffset, unit, strength),
            minimumDimension * (0.08f + unit * 0.34f) * (0.78f + unit * 0.32f + musicPush * 0.12f),
            4 + (i % 5),
            cameraPhase * (0.3f + unit * 0.5f),
            0.45f + unit * 1.1f,
            withAlpha(colors[(i + 1) % 4], (0.035f + unit * 0.055f + musicPush * 0.04f) * strength)
        });
    }

    for (std::size_t i = 0; i < frame.rings.size(); ++i) {
        Ring& ring = frame.rings[i];
        const float radiusDepth = clamp01(ring.radius / std::max(1.0f, diagonal * 0.48f));
        const float layer = clamp01(radiusDepth * 0.78f +
                                    (static_cast<float>(i % 9U) / 8.0f) * 0.22f +
                                    musicPush * 0.14f);
        ring.center = projectDepthPoint(ring.center, vanishingPoint, cameraOffset, layer, strength);
        ring.radius *= 1.0f + strength * (-0.24f + layer * 0.56f + metrics.bass * 0.1f);
        ring.strokeWidth *= 1.0f + strength * (-0.28f + layer * 0.64f + beatPulse * 0.14f);
        ring.rotation += (layer - 0.5f) * strength * (0.45f + metrics.stereoWidth * 0.35f);
        ring.color.a = clamp01(ring.color.a * (0.5f + layer * 0.78f + musicPush * 0.16f));
    }

    for (std::size_t i = 0; i < frame.particles.size(); ++i) {
        Particle& particle = frame.particles[i];
        const float dx = particle.position.x - center.x;
        const float dy = particle.position.y - center.y;
        const float radial = std::sqrt(dx * dx + dy * dy) / std::max(1.0f, diagonal * 0.5f);
        const float layer = clamp01(radial * 0.62f +
                                    (static_cast<float>(i % 11U) / 10.0f) * 0.32f +
                                    metrics.treble * 0.1f +
                                    musicPush * 0.08f);
        particle.position = projectDepthPoint(particle.position, vanishingPoint, cameraOffset, layer, strength);
        particle.radius *= 1.0f + strength * (-0.32f + layer * 0.92f + metrics.treble * 0.08f);
        particle.color.a = clamp01(particle.color.a * (0.42f + layer * 0.92f + metrics.spectralFlux * 0.12f));
    }

    for (std::size_t i = 0; i < frame.polylines.size(); ++i) {
        Polyline& polyline = frame.polylines[i];
        const bool guideLine = i >= originalPolylineCount;
        const float lane = static_cast<float>(i % 13U) / 12.0f;
        for (Vec2& point : polyline.points) {
            const float dx = point.x - center.x;
            const float dy = point.y - center.y;
            const float radial = std::sqrt(dx * dx + dy * dy) / std::max(1.0f, diagonal * 0.5f);
            const float layer = clamp01(radial * 0.54f + lane * 0.36f + metrics.stereoWidth * 0.08f + musicPush * 0.08f);
            point = projectDepthPoint(point, vanishingPoint, cameraOffset, layer, guideLine ? strength * 0.45f : strength);
        }
        polyline.strokeWidth *= 1.0f + strength * (-0.18f + lane * 0.36f + musicPush * 0.08f);
        polyline.color.a = clamp01(polyline.color.a * (guideLine ? 1.0f : (0.58f + lane * 0.62f + musicPush * 0.16f)));
    }

    for (std::size_t i = 0; i < frame.beams.size(); ++i) {
        Beam& beam = frame.beams[i];
        const float layer = clamp01(static_cast<float>(i % 12U) / 11.0f + musicPush * 0.12f);
        beam.angle += (layer - 0.5f) * strength * (0.12f + metrics.stereoWidth * 0.16f);
        beam.length *= 1.0f + strength * (-0.18f + layer * 0.48f + metrics.bass * 0.12f);
        beam.width *= 1.0f + strength * (-0.22f + layer * 0.58f + beatPulse * 0.12f);
        beam.color.a = clamp01(beam.color.a * (0.52f + layer * 0.72f + metrics.dropIntensity * 0.18f));
    }

    std::stable_sort(frame.rings.begin(), frame.rings.end(), [](const Ring& left, const Ring& right) {
        return left.radius < right.radius;
    });
    std::stable_sort(frame.particles.begin(), frame.particles.end(), [](const Particle& left, const Particle& right) {
        return left.radius < right.radius;
    });
}

void suppressScreenSpaceLayerFor3D(GeometryFrame& frame,
                                   const AudioMetrics& metrics,
                                   const VisualSettings& settings)
{
    const float depth = depth3DOf(settings);
    if (depth <= 0.01f) {
        return;
    }

    const float clarity = patternClarityOf(settings);
    const float stability = motionStabilityOf(settings);
    const float highDepth = smootherStep(0.62f, 1.0f, depth);
    const bool strict3DFirst = true;
    const float transitionKeep = clamp01(settings.sceneTransition) * 0.055f;
    const float musicKeep = clamp01(metrics.dropIntensity * 0.06f +
                                    metrics.phraseIntensity * 0.030f +
                                    metrics.buildTension * 0.026f);
    const float alphaScale = std::clamp(0.16f - depth * 0.125f -
                                            highDepth * clarity * 0.015f +
                                            (1.0f - clarity) * 0.032f +
                                            (1.0f - stability) * 0.024f +
                                            transitionKeep + musicKeep,
                                        strict3DFirst ? 0.0f : 0.012f,
                                        strict3DFirst ? 0.035f : 0.16f);
    const float strokeScale = std::clamp(0.34f + (1.0f - clarity) * 0.08f + (1.0f - stability) * 0.045f,
                                         0.28f,
                                         0.54f);

    for (Ring& ring : frame.rings) {
        ring.color.a *= alphaScale;
        ring.strokeWidth = std::max(0.28f, ring.strokeWidth * strokeScale);
    }
    for (Beam& beam : frame.beams) {
        beam.color.a *= alphaScale * 0.85f;
        beam.width = std::max(0.24f, beam.width * strokeScale);
    }
    for (Particle& particle : frame.particles) {
        particle.color.a *= alphaScale * 0.92f;
        particle.radius = std::max(0.65f, particle.radius * (0.58f + (1.0f - depth) * 0.14f));
    }
    for (Polyline& line : frame.polylines) {
        line.color.a *= alphaScale;
        line.strokeWidth = std::max(0.28f, line.strokeWidth * strokeScale);
        const auto maximumPoints = static_cast<std::size_t>(std::clamp(
            static_cast<int>(std::round(7.0f + (1.0f - depth) * 24.0f + (1.0f - clarity) * 9.0f)),
            5,
            42));
        decimatePolyline(line, maximumPoints);
    }

    const std::size_t maxRings = strict3DFirst
                                     ? 0U
                                     : static_cast<std::size_t>(std::clamp(
                                           static_cast<int>(std::round(1.0f + (1.0f - depth) * 6.0f +
                                                                       settings.sceneTransition * 3.0f)),
                                           1,
                                           9));
    const std::size_t maxBeams = strict3DFirst
                                     ? 0U
                                     : static_cast<std::size_t>(std::clamp(
                                           static_cast<int>(std::round(1.0f + (1.0f - depth) * 5.0f +
                                                                       metrics.dropIntensity * 2.0f)),
                                           1,
                                           9));
    const std::size_t maxParticles = strict3DFirst
                                         ? 0U
                                         : static_cast<std::size_t>(std::clamp(
                                               static_cast<int>(std::round(4.0f + (1.0f - depth) * 16.0f +
                                                                           metrics.spectralFlux * 4.0f)),
                                               3,
                                               32));
    const std::size_t maxPolylines = strict3DFirst
                                         ? 0U
                                         : static_cast<std::size_t>(std::clamp(
                                               static_cast<int>(std::round(1.0f + (1.0f - depth) * 6.0f +
                                                                           metrics.phraseIntensity * 2.0f +
                                                                           settings.sceneTransition * 7.0f)),
                                               1,
                                               14));
    retainEvenlySpaced(frame.rings, maxRings);
    retainEvenlySpaced(frame.beams, maxBeams);
    retainEvenlySpaced(frame.particles, maxParticles);
    retainEvenlySpaced(frame.polylines, maxPolylines);
}

enum class Scene3DProfile {
    TechnoMachine,
    CrystalStorm,
    NeuralSpace,
    DimensionalTunnel,
    CymaticSculpture
};

enum class SongSceneIdentity {
    CalmSpace,
    BassPressure,
    TechnoArchitecture,
    AmbientOrbit,
    MelodicCrystal,
    BreakbeatFracture,
    DarkMonolith
};

struct Projected3D {
    Vec2 point{};
    float depth = 0.0f;
    float perspective = 1.0f;
    bool visible = true;
};

struct Camera3D {
    Vec2 center{};
    float focalLength = 600.0f;
    float cameraDistance = 1000.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
};

struct ObjectBounds3D {
    Vec3 minimum{};
    Vec3 maximum{};
    Vec3 center{};
    Vec3 span{};
    bool valid = false;
};

struct ProjectedSceneStats3D {
    float screenCoverage = 0.0f;
    float centerOffset = 0.0f;
    float foregroundShare = 0.0f;
    float midgroundShare = 0.0f;
    float backgroundShare = 0.0f;
};

struct RoleSceneStats3D {
    float explicitRoleShare = 0.0f;
    float bridgeShare = 0.0f;
    float districtSpread = 0.0f;
    float roleBalance = 0.0f;
    float roleVocabulary = 0.0f;
    float roleSilhouetteContrast = 0.0f;
};

Scene3DProfile profileForMode(VisualMode mode)
{
    switch (mode) {
    case VisualMode::TechnoMandala:
    case VisualMode::PolyrhythmLattice:
        return Scene3DProfile::TechnoMachine;
    case VisualMode::SpectralOrigami:
    case VisualMode::ChromaKaleidoscope:
    case VisualMode::FrequencyBloom:
        return Scene3DProfile::CrystalStorm;
    case VisualMode::NeuralConstellation:
        return Scene3DProfile::NeuralSpace;
    case VisualMode::CymaticInterference:
    case VisualMode::ResonanceTessellation:
        return Scene3DProfile::CymaticSculpture;
    case VisualMode::QuantumTunnel:
    case VisualMode::LissajousMesh:
    case VisualMode::FractalCathedral:
    case VisualMode::HyperspacePolytope:
    case VisualMode::PhaseWeave:
        return Scene3DProfile::DimensionalTunnel;
    }
    return Scene3DProfile::DimensionalTunnel;
}

std::string_view scene3DName(Scene3DProfile profile)
{
    switch (profile) {
    case Scene3DProfile::TechnoMachine:
        return "Techno Machine";
    case Scene3DProfile::CrystalStorm:
        return "Crystal Storm";
    case Scene3DProfile::NeuralSpace:
        return "Neural Space";
    case Scene3DProfile::DimensionalTunnel:
        return "Dimensional Tunnel";
    case Scene3DProfile::CymaticSculpture:
        return "Cymatic Sculpture";
    }
    return "3D Scene";
}

std::string_view mode3DName(VisualMode mode)
{
    switch (mode) {
    case VisualMode::QuantumTunnel:
        return "Quantum Tunnel Volume";
    case VisualMode::TechnoMandala:
        return "Techno Mandala Machine";
    case VisualMode::LissajousMesh:
        return "Lissajous Ribbon Mesh";
    case VisualMode::FrequencyBloom:
        return "Frequency Bloom Sculpture";
    case VisualMode::FractalCathedral:
        return "Fractal Cathedral Vault";
    case VisualMode::PolyrhythmLattice:
        return "Polyrhythm Lattice Rig";
    case VisualMode::SpectralOrigami:
        return "Spectral Origami Storm";
    case VisualMode::ChromaKaleidoscope:
        return "Chroma Kaleidoscope Prism";
    case VisualMode::HyperspacePolytope:
        return "Hyperspace Polytope Cage";
    case VisualMode::PhaseWeave:
        return "Phase Weave Current";
    case VisualMode::ResonanceTessellation:
        return "Resonance Tessellation Field";
    case VisualMode::NeuralConstellation:
        return "Neural Constellation Depth";
    case VisualMode::CymaticInterference:
        return "Cymatic Interference Sculpture";
    }
    return "Mode 3D Scene";
}

float transientEnergy3D(const AudioMetrics& metrics)
{
    return clamp01(metrics.spectralFlux * 0.42f +
                   metrics.onset * 0.30f +
                   averageOnsetEnergy(metrics) * 0.28f);
}

float crestTransient3D(const AudioMetrics& metrics)
{
    return smootherStep(0.08f, 0.36f, metrics.peak - metrics.rms * 1.35f);
}

float staggeredCutEnergy3D(const AudioMetrics& metrics)
{
    const float cutTexture = clamp01(metrics.mid * 0.22f +
                                     metrics.highMid * 0.34f +
                                     metrics.treble * 0.38f +
                                     averageOnsetEnergy(metrics) * 0.24f +
                                     metrics.spectralFlux * 0.22f);
    const float offGridStereo = clamp01((1.0f - metrics.beatConfidence) * 0.24f +
                                        metrics.stereoWidth * 0.26f +
                                        metrics.barConfidence * 0.08f);
    return clamp01(crestTransient3D(metrics) * 0.48f +
                   cutTexture * 0.32f +
                   offGridStereo * 0.24f -
                   metrics.dropIntensity * 0.10f -
                   metrics.bass * 0.08f);
}

SongSceneIdentity songSceneIdentityFor(const SceneInterpretation& intent,
                                       const AudioMetrics& metrics,
                                       VisualMode mode)
{
    const float styleWeight = std::clamp(metrics.styleConfidence, 0.0f, 1.0f);
    const float transient = transientEnergy3D(metrics);
    const float staggeredCut = staggeredCutEnergy3D(metrics);
    const float beatRegularity = clamp01(metrics.beatConfidence * 0.54f +
                                         metrics.barConfidence * 0.28f +
                                         metrics.downbeatConfidence * 0.18f);
    const float lowWeight = clamp01(metrics.bass * 0.58f + metrics.lowMid * 0.24f);
    const float highWeight = clamp01(metrics.treble * 0.42f + metrics.highMid * 0.28f +
                                     metrics.spectralCentroid * 0.18f);
    const float harmonic = clamp01(metrics.harmonicEnergy * 0.62f +
                                   metrics.keyConfidence * 0.30f +
                                   averageChromaEnergy(metrics) * 0.16f);

    if ((metrics.style == AudioStyle::Silence && metrics.rms < 0.055f) ||
        (intent.calm > 0.82f && metrics.rms < 0.08f)) {
        return SongSceneIdentity::CalmSpace;
    }
    if (mode == VisualMode::ResonanceTessellation &&
        metrics.style == AudioStyle::BassHeavy &&
        metrics.styleConfidence < 0.82f &&
        metrics.bass > 0.42f &&
        metrics.dropIntensity < 0.25f) {
        return SongSceneIdentity::DarkMonolith;
    }

    const float bassScore = clamp01(intent.mass * 0.58f +
                                    intent.heavy * 0.34f +
                                    intent.drop * 0.28f +
                                    lowWeight * 0.30f +
                                    (metrics.style == AudioStyle::BassHeavy ? 0.34f * styleWeight : 0.0f));
    const float technoScore = clamp01(intent.architecture * 0.55f +
                                      intent.industrial * 0.34f +
                                      intent.groove * 0.26f +
                                      beatRegularity * 0.30f +
                                      (mode == VisualMode::TechnoMandala ||
                                               mode == VisualMode::PolyrhythmLattice
                                           ? 0.16f
                                           : 0.0f) +
                                      (metrics.style == AudioStyle::Techno ? 0.24f * styleWeight : 0.0f));
    const float ambientScore = clamp01(intent.orbital * 0.48f +
                                       intent.spacious * 0.42f +
                                       metrics.stereoWidth * 0.30f +
                                       intent.calm * 0.20f +
                                       (metrics.style == AudioStyle::Ambient ||
                                                metrics.style == AudioStyle::Wide
                                            ? 0.24f * styleWeight
                                            : 0.0f) -
                                       transient * 0.18f);
    const float melodicScore = clamp01(intent.crystal * 0.54f +
                                       intent.melodic * 0.36f +
                                       intent.bright * 0.20f +
                                       harmonic * 0.32f +
                                       (mode == VisualMode::ChromaKaleidoscope ||
                                                mode == VisualMode::FrequencyBloom
                                            ? 0.14f
                                            : 0.0f) -
                                       metrics.bass * 0.14f);
    const float breakScore = clamp01(intent.fracture * 0.54f +
                                     intent.chaotic * 0.40f +
                                     transient * 0.38f +
                                     staggeredCut * 0.32f +
                                     highWeight * 0.18f +
                                     (mode == VisualMode::SpectralOrigami ? 0.18f : 0.0f) -
                                     intent.mass * 0.12f);
    const bool resonanceOrCathedral = mode == VisualMode::FractalCathedral ||
                                      mode == VisualMode::ResonanceTessellation;
    const float darkScore = clamp01(intent.shadow * 0.56f +
                                    intent.dark * 0.34f +
                                    intent.minimal * 0.20f +
                                    (1.0f - highWeight) * lowWeight * 0.30f +
                                    (metrics.keyMode == MusicalMode::Minor ? metrics.keyConfidence * 0.16f : 0.0f) +
                                    (resonanceOrCathedral ? 0.08f : 0.0f));

    const bool decisiveFractureCue = (transient > 0.22f || staggeredCut > 0.42f) &&
                                     metrics.keyConfidence < 0.46f &&
                                     (metrics.style == AudioStyle::Bright ||
                                      metrics.spectralFlux > 0.24f ||
                                      staggeredCut > 0.42f ||
                                      highWeight > lowWeight * 0.88f ||
                                      averageOnsetEnergy(metrics) > 0.46f);
    if (decisiveFractureCue) {
        return SongSceneIdentity::BreakbeatFracture;
    }
    if ((mode == VisualMode::ChromaKaleidoscope ||
         mode == VisualMode::FrequencyBloom ||
         mode == VisualMode::ResonanceTessellation ||
         mode == VisualMode::CymaticInterference) &&
        melodicScore > 0.42f &&
        metrics.keyConfidence > 0.48f &&
        metrics.harmonicEnergy > 0.54f) {
        return SongSceneIdentity::MelodicCrystal;
    }
    if (breakScore > 0.48f && breakScore > bassScore - 0.04f && (transient > 0.28f || staggeredCut > 0.38f)) {
        return SongSceneIdentity::BreakbeatFracture;
    }
    const bool weakBassHeavyDarkCue = metrics.style == AudioStyle::BassHeavy &&
                                      metrics.styleConfidence < 0.82f &&
                                      metrics.dropIntensity < 0.18f &&
                                      metrics.keyMode == MusicalMode::Minor &&
                                      metrics.keyConfidence > 0.18f &&
                                      metrics.harmonicEnergy > 0.30f &&
                                      metrics.highMid + metrics.treble < metrics.bass * 0.025f + 0.004f;
    if (((darkScore > 0.48f &&
          darkScore > ambientScore + 0.06f &&
          metrics.bass > 0.32f &&
          metrics.style != AudioStyle::BassHeavy &&
          metrics.dropIntensity < 0.52f)) ||
        weakBassHeavyDarkCue) {
        return SongSceneIdentity::DarkMonolith;
    }
    if (bassScore > 0.58f && metrics.bass > 0.48f) {
        return SongSceneIdentity::BassPressure;
    }
    if (technoScore > 0.50f && technoScore > ambientScore + 0.04f) {
        return SongSceneIdentity::TechnoArchitecture;
    }
    if (melodicScore > 0.50f && melodicScore > technoScore + 0.02f) {
        return SongSceneIdentity::MelodicCrystal;
    }
    if (ambientScore > 0.42f) {
        return SongSceneIdentity::AmbientOrbit;
    }
    if (darkScore > 0.40f) {
        return SongSceneIdentity::DarkMonolith;
    }
    return SongSceneIdentity::CalmSpace;
}

Camera3D makeCamera3D(const VisualSettings& settings,
                      const AudioMetrics& metrics,
                      const SceneInterpretation& intent,
                      const MusicSceneArc3D& arc,
                      SongSceneIdentity identity,
                      float width,
                      float height,
                      float speed,
                      double time)
{
    const float depth = depth3DOf(settings);
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const float response = musicResponse3D(metrics, settings);
    const MusicMotionEnvelope envelope = musicEnvelope(metrics, settings);
    const MusicChoreography choreography = buildMusicChoreography(metrics, settings, settings.mode, time, speed);
    const float cameraMotion = envelope.camera * (0.58f + (1.0f - envelope.stability) * 0.42f);
    const float rush = (choreography.dropImpact * 0.74f + choreography.bassPressure * 0.40f) * response *
                       (0.62f + (1.0f - envelope.stability) * 0.38f);
    const float cinematic = 0.52f + (1.0f - choreography.stability) * 0.24f + choreography.parallax * 0.24f;
    const float reveal = intent.depthReveal * 0.10f + intent.spacious * 0.06f - intent.heavy * 0.035f;
    const float massDolly = intent.mass * 0.12f + intent.drop * 0.09f;
    const float orbitBias = intent.orbital * 0.10f + intent.architecture * 0.035f - intent.minimal * 0.035f;
    const float pitchBias = intent.tension * 0.045f - intent.calm * 0.022f + intent.spacious * 0.035f;
    const float sectionConfidence = clamp01(metrics.sectionConfidence);
    const float buildReveal = metrics.section == ArrangementSection::Build
                                  ? sectionConfidence * clamp01(metrics.sectionProgress)
                                  : 0.0f;
    const float dropPunch = metrics.section == ArrangementSection::Drop
                                ? sectionConfidence * (0.58f + metrics.dropIntensity * 0.42f)
                                : 0.0f;
    const float breakdownHold = metrics.section == ArrangementSection::Breakdown ? sectionConfidence : 0.0f;
    const float grooveLock = metrics.section == ArrangementSection::Groove
                                 ? sectionConfidence * clamp01(metrics.beatConfidence + metrics.barConfidence * 0.35f)
                                 : 0.0f;
    const int cutIndex = static_cast<int>(std::floor(clamp01(metrics.beatPhase) * 8.0f));
    const float cutStep = static_cast<float>(cutIndex) / 8.0f;
    const float cutLane = (cutIndex % 2 == 0) ? -1.0f : 1.0f;

    Camera3D camera{
        Vec2{width * 0.5f, height * 0.5f},
        minimumDimension * (0.78f + depth * 0.94f + envelope.stereo * 0.12f +
                            choreography.phraseLift * 0.035f + reveal),
        std::max(minimumDimension * 0.72f,
                 minimumDimension * (1.18f + depth * 1.72f - rush * 0.13f +
                                     choreography.phraseLift * 0.10f + choreography.fold * 0.04f +
                                     intent.spacious * 0.14f - massDolly)),
        (std::sin(phase * 0.17f + choreography.orbit * 2.0f * kPi) * 0.65f +
         choreography.stereoDrift * 0.35f) *
            (0.06f + envelope.stereo * 0.12f + depth * 0.07f + response * 0.010f +
             cameraMotion * 0.035f + orbitBias) *
            cinematic,
        (std::cos(phase * 0.13f + metrics.buildTension * kPi + choreography.melodicOrbit * kPi) * 0.72f +
         choreography.breath * 0.28f) *
            (0.035f + choreography.phraseLift * 0.065f + choreography.buildTension * 0.052f +
             cameraMotion * 0.026f + pitchBias) *
            cinematic,
        std::sin(phase * 0.10f + metrics.beatPhase * kPi * 2.0f + choreography.grooveSwing) *
            (envelope.stereo * 0.034f + choreography.dropImpact * 0.020f + choreography.snap * 0.010f +
             response * 0.004f + intent.chaotic * 0.006f - intent.minimal * 0.004f)
    };

    camera.focalLength *= 1.0f + buildReveal * 0.035f + dropPunch * 0.050f - breakdownHold * 0.030f;
    camera.cameraDistance += minimumDimension * (buildReveal * 0.12f - dropPunch * 0.16f + breakdownHold * 0.10f);
    camera.yaw += buildReveal * 0.035f - breakdownHold * 0.030f;
    camera.pitch += buildReveal * 0.030f + breakdownHold * 0.022f - dropPunch * 0.018f;
    camera.roll *= 1.0f - breakdownHold * 0.45f;
    camera.focalLength *= std::clamp(1.0f +
                                         arc.anticipation * 0.035f +
                                         arc.impact * 0.055f -
                                         arc.recovery * 0.030f,
                                     0.94f,
                                     1.12f);
    camera.cameraDistance += minimumDimension *
                              (arc.anticipation * 0.115f -
                               arc.impact * 0.150f +
                               arc.recovery * 0.135f +
                               arc.continuity * 0.028f);
    camera.yaw += std::sin(phase * 0.045f + metrics.phrasePhase * kPi) *
                  (arc.anticipation * 0.030f + arc.recovery * 0.024f);
    camera.pitch += arc.anticipation * 0.026f -
                    arc.impact * 0.022f +
                    arc.recovery * 0.030f;
    camera.roll *= std::clamp(1.0f - arc.recovery * 0.34f - arc.continuity * 0.10f, 0.54f, 1.0f);
    camera.center.y += minimumDimension * (arc.anticipation * 0.010f - arc.impact * 0.020f + arc.recovery * 0.014f);

    switch (identity) {
    case SongSceneIdentity::CalmSpace:
        camera.cameraDistance += minimumDimension * 0.16f;
        camera.focalLength *= 0.96f;
        camera.yaw *= 0.58f;
        camera.pitch *= 0.64f;
        camera.roll *= 0.38f;
        break;
    case SongSceneIdentity::BassPressure:
        camera.cameraDistance -= minimumDimension * (0.12f + intent.mass * 0.10f + dropPunch * 0.05f);
        camera.focalLength *= 1.05f + intent.mass * 0.035f;
        camera.pitch -= 0.026f + intent.mass * 0.020f;
        camera.yaw *= 0.76f;
        camera.roll *= 0.55f;
        camera.center.y -= minimumDimension * (0.018f + intent.mass * 0.018f);
        break;
    case SongSceneIdentity::TechnoArchitecture: {
        const float lockedYaw = std::round(std::sin(metrics.barPhase * 2.0f * kPi) * 4.0f) / 4.0f;
        camera.cameraDistance += minimumDimension * 0.02f;
        camera.focalLength *= 1.08f;
        camera.yaw = camera.yaw * 0.42f + lockedYaw * (0.040f + grooveLock * 0.030f);
        camera.pitch = std::clamp(camera.pitch * 0.42f - 0.026f, -0.070f, 0.040f);
        camera.roll *= 0.28f;
        camera.center.x += lockedYaw * minimumDimension * 0.024f;
        break;
    }
    case SongSceneIdentity::AmbientOrbit:
        camera.cameraDistance += minimumDimension * (0.20f + intent.spacious * 0.12f);
        camera.focalLength *= 0.94f;
        camera.yaw += std::sin(phase * 0.055f + metrics.phrasePhase * kPi) * (0.055f + metrics.stereoWidth * 0.035f);
        camera.pitch += std::cos(phase * 0.045f) * (0.035f + intent.orbital * 0.030f);
        camera.roll += std::sin(phase * 0.036f) * metrics.stereoWidth * 0.018f;
        camera.center.x += std::sin(phase * 0.030f) * minimumDimension * metrics.stereoWidth * 0.026f;
        break;
    case SongSceneIdentity::MelodicCrystal:
        camera.cameraDistance += minimumDimension * (0.04f + intent.melodic * 0.045f);
        camera.focalLength *= 1.02f + intent.crystal * 0.050f;
        camera.yaw += std::sin(choreography.melodicOrbit * 2.0f * kPi) * (0.040f + intent.crystal * 0.030f);
        camera.pitch += 0.052f + intent.melodic * 0.034f + intent.crystal * 0.012f;
        camera.pitch = std::max(camera.pitch, 0.078f + intent.melodic * 0.020f + intent.crystal * 0.010f);
        camera.roll += std::sin(choreography.melodicOrbit * kPi) * 0.020f;
        camera.center.y -= minimumDimension * (0.015f + intent.melodic * 0.014f);
        break;
    case SongSceneIdentity::BreakbeatFracture:
        camera.cameraDistance -= minimumDimension * transientEnergy3D(metrics) * 0.055f;
        camera.focalLength *= 1.03f;
        camera.yaw += (cutLane * 0.105f + (cutStep - 0.5f) * 0.080f) * (0.82f + intent.fracture * 0.34f);
        camera.pitch += cutLane * transientEnergy3D(metrics) * 0.034f;
        camera.roll += (cutLane * 0.050f + (cutStep - 0.5f) * 0.030f);
        camera.center.x += (cutLane * 0.038f + (cutStep - 0.5f) * 0.024f) * minimumDimension;
        break;
    case SongSceneIdentity::DarkMonolith:
        camera.cameraDistance += minimumDimension * (0.10f + intent.shadow * 0.10f);
        camera.focalLength *= 1.08f;
        camera.yaw *= 0.34f;
        camera.pitch = std::min(camera.pitch * 0.42f + 0.055f + intent.dark * 0.018f, 0.14f);
        camera.roll *= 0.12f;
        camera.center.y += minimumDimension * (0.018f + intent.minimal * 0.020f);
        break;
    }

    const float analyzerRoleSum = metrics.bassRole +
                                  metrics.drumRole +
                                  metrics.melodyRole +
                                  metrics.harmonyRole +
                                  metrics.spaceRole +
                                  metrics.fractureRole +
                                  metrics.shadowRole;
    if (analyzerRoleSum > 0.015f || metrics.roleSeparation > 0.015f) {
        const float melodyRole = std::max(metrics.melodyRole, metrics.harmonyRole);
        const float strongestRole = std::max({metrics.bassRole,
                                              metrics.drumRole,
                                              melodyRole,
                                              metrics.spaceRole,
                                              metrics.fractureRole,
                                              metrics.shadowRole});
        const float roleConfidence = clamp01(metrics.roleSeparation * 0.62f + strongestRole * 0.42f);
        const float harmonicSeed = metrics.keyIndex >= 0
                                       ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f
                                       : metrics.spectralCentroid;
        const float harmonicPhase = harmonicSeed * 2.0f * kPi +
                                    metrics.phrasePhase * kPi +
                                    phase * (0.035f + melodyRole * 0.020f);
        const float spaciousOrbit = std::sin(phase * 0.045f + metrics.stereoWidth * kPi + metrics.phrasePhase * kPi);
        const float spaciousLane = metrics.stereoWidth >= 0.5f ? -1.0f : 1.0f;
        const float fractureCut = cutStep - 0.5f;
        const float fractureLane = cutLane * 0.66f + fractureCut * 0.34f;
        const float convergencePull = clamp01(metrics.convergenceRole + metrics.dropIntensity * 0.34f +
                                              (metrics.phraseBoundary ? metrics.phraseConfidence * 0.24f : 0.0f));

        camera.cameraDistance += minimumDimension * roleConfidence *
                                  (metrics.spaceRole * 0.18f +
                                   metrics.shadowRole * 0.10f +
                                   metrics.fractureRole * 0.035f -
                                   metrics.bassRole * 0.16f -
                                   convergencePull * 0.10f);
        camera.focalLength *= std::clamp(1.0f +
                                             roleConfidence *
                                                 (metrics.bassRole * 0.055f +
                                                  metrics.drumRole * 0.030f +
                                                  melodyRole * 0.048f +
                                                  metrics.shadowRole * 0.028f -
                                                  metrics.spaceRole * 0.058f),
                                         0.88f,
                                         1.16f);
        camera.yaw += roleConfidence *
                      (metrics.spaceRole * spaciousOrbit * 0.070f +
                       melodyRole * std::sin(harmonicPhase) * 0.055f +
                       metrics.fractureRole * fractureLane * 0.195f -
                       metrics.shadowRole * camera.yaw * 0.48f);
        camera.pitch += roleConfidence *
                        (melodyRole * 0.060f +
                         metrics.spaceRole * std::cos(phase * 0.038f) * 0.026f +
                         metrics.shadowRole * 0.040f -
                         metrics.bassRole * 0.046f +
                         metrics.fractureRole * (((static_cast<int>(cutStep * 8.0f) % 2) == 0) ? -0.020f : 0.020f));
        camera.roll += roleConfidence *
                       (metrics.fractureRole * fractureLane * 0.090f +
                        melodyRole * std::sin(harmonicPhase * 0.7f) * 0.020f -
                        metrics.drumRole * camera.roll * 0.80f -
                        metrics.shadowRole * camera.roll * 0.88f);
        camera.center.x += minimumDimension * roleConfidence *
                           (metrics.spaceRole * spaciousOrbit * 0.034f +
                            metrics.spaceRole * spaciousLane * (0.028f + metrics.stereoWidth * 0.018f) +
                            melodyRole * std::sin(harmonicPhase) * 0.024f +
                            metrics.fractureRole * fractureLane * 0.058f);
        camera.center.y += minimumDimension * roleConfidence *
                           (metrics.shadowRole * 0.026f -
                            metrics.bassRole * 0.022f -
                            melodyRole * 0.030f +
                            metrics.spaceRole * std::cos(phase * 0.034f) * 0.012f);

        if (metrics.drumRole > 0.34f &&
            metrics.drumRole > melodyRole + 0.04f &&
            metrics.drumRole > metrics.spaceRole + 0.04f) {
            const float lock = roleConfidence * clamp01(0.34f + metrics.drumRole * 0.76f);
            const float lane = std::round(std::sin(metrics.barPhase * 2.0f * kPi) * 3.0f) / 3.0f;
            camera.yaw = camera.yaw * (1.0f - lock * 0.46f) +
                         lane * (0.032f + metrics.drumRole * 0.034f) * lock;
            camera.pitch = camera.pitch * (1.0f - lock * 0.34f) - 0.018f * lock;
            camera.roll *= 1.0f - lock * 0.70f;
        }
        if (metrics.shadowRole > 0.34f &&
            metrics.shadowRole > metrics.fractureRole - 0.02f &&
            metrics.shadowRole > metrics.spaceRole + 0.04f) {
            const float stillness = roleConfidence * clamp01(metrics.shadowRole * 0.82f);
            camera.cameraDistance += minimumDimension * stillness * 0.055f;
            camera.yaw *= 1.0f - stillness * 0.50f;
            camera.roll *= 1.0f - stillness * 0.92f;
            camera.pitch = std::max(camera.pitch, 0.056f * stillness + 0.010f * metrics.shadowRole);
        }
    }

    camera.cameraDistance = std::max(minimumDimension * 0.68f,
                                     std::min(camera.cameraDistance, minimumDimension * 3.45f));
    camera.focalLength = std::clamp(camera.focalLength, minimumDimension * 0.62f, minimumDimension * 2.05f);
    camera.yaw = std::clamp(camera.yaw, -0.34f, 0.34f);
    camera.pitch = std::clamp(camera.pitch, -0.22f, 0.22f);
    camera.roll = std::clamp(camera.roll, -0.16f, 0.16f);
    camera.center.x = std::clamp(camera.center.x, width * 0.40f, width * 0.60f);
    camera.center.y = std::clamp(camera.center.y, height * 0.40f, height * 0.60f);
    return camera;
}

void applyCameraInteraction3D(Camera3D& camera,
                              const InteractionState& interaction,
                              const VisualSettings& settings,
                              float width,
                              float height)
{
    if (!interaction.enabled || !interaction.active || interactionDepthOf(settings) <= 0.001f) {
        return;
    }

    const float depthStrength = interactionDepthOf(settings);
    const float stability = motionStabilityOf(settings);
    const float clarity = patternClarityOf(settings);
    const float normalizedX = std::clamp(interaction.normalizedX, 0.0f, 1.0f) - 0.5f;
    const float normalizedY = std::clamp(interaction.normalizedY, 0.0f, 1.0f) - 0.5f;
    const float pointerReach = std::min(width, height) *
                               (0.030f + depthStrength * 0.040f) *
                               (0.80f + (1.0f - clarity) * 0.20f);
    const float orbitReach = (0.028f + depthStrength * 0.060f) *
                             (0.72f + (1.0f - stability) * 0.28f);
    const float clickDolly = interaction.pressed
                                 ? std::min(width, height) * depthStrength * 0.040f *
                                       (0.80f + (1.0f - clarity) * 0.20f)
                                 : 0.0f;

    camera.center.x += normalizedX * pointerReach;
    camera.center.y += normalizedY * pointerReach * 0.72f;
    camera.yaw += normalizedX * orbitReach;
    camera.pitch -= normalizedY * orbitReach * 0.72f;
    camera.roll += normalizedX * normalizedY * orbitReach * 0.34f;
    camera.cameraDistance = std::max(std::min(width, height) * 0.64f, camera.cameraDistance - clickDolly);

    camera.center.x = std::clamp(camera.center.x, width * 0.38f, width * 0.62f);
    camera.center.y = std::clamp(camera.center.y, height * 0.38f, height * 0.62f);
    camera.yaw = std::clamp(camera.yaw, -0.38f, 0.38f);
    camera.pitch = std::clamp(camera.pitch, -0.26f, 0.26f);
    camera.roll = std::clamp(camera.roll, -0.18f, 0.18f);
}

Projected3D projectPoint3D(Vec3 world, const Camera3D& camera)
{
    Vec3 cameraPoint = world;
    cameraPoint = rotate3D(cameraPoint, Vec3{-camera.pitch, -camera.yaw, -camera.roll});
    const float depth = cameraPoint.z + camera.cameraDistance;
    if (depth <= 24.0f) {
        return Projected3D{{camera.center.x, camera.center.y}, depth, 1.0f, false};
    }
    const float perspective = camera.focalLength / depth;
    return Projected3D{
        Vec2{camera.center.x + cameraPoint.x * perspective,
             camera.center.y + cameraPoint.y * perspective},
        depth,
        perspective,
        true
    };
}

Object3D makeObject3D(Object3DKind kind,
                      Vec3 position,
                      Vec3 scaleValue,
                      Vec3 rotation,
                      ColorRGBA color,
                      float glow)
{
    Object3D object;
    object.kind = kind;
    object.position = position;
    object.scale = scaleValue;
    object.rotation = rotation;
    object.color = color;
    object.glow = glow;
    object.depth = position.z;
    return object;
}

Vec3 objectLocalToWorld(const Object3D& object, Vec3 local)
{
    return add(object.position, rotate3D(multiply(local, object.scale), object.rotation));
}

ObjectBounds3D objectBounds3D(const std::vector<Object3D>& objects)
{
    ObjectBounds3D bounds;
    const auto includePoint = [&bounds](Vec3 point) {
        if (!bounds.valid) {
            bounds.minimum = point;
            bounds.maximum = point;
            bounds.valid = true;
            return;
        }
        bounds.minimum.x = std::min(bounds.minimum.x, point.x);
        bounds.minimum.y = std::min(bounds.minimum.y, point.y);
        bounds.minimum.z = std::min(bounds.minimum.z, point.z);
        bounds.maximum.x = std::max(bounds.maximum.x, point.x);
        bounds.maximum.y = std::max(bounds.maximum.y, point.y);
        bounds.maximum.z = std::max(bounds.maximum.z, point.z);
    };

    for (const Object3D& object : objects) {
        includePoint(object.position);
        if (object.kind == Object3DKind::Link) {
            includePoint(object.target);
            continue;
        }
        includePoint(objectLocalToWorld(object, Vec3{1.0f, 0.0f, 0.0f}));
        includePoint(objectLocalToWorld(object, Vec3{-1.0f, 0.0f, 0.0f}));
        includePoint(objectLocalToWorld(object, Vec3{0.0f, 1.0f, 0.0f}));
        includePoint(objectLocalToWorld(object, Vec3{0.0f, -1.0f, 0.0f}));
        includePoint(objectLocalToWorld(object, Vec3{0.0f, 0.0f, 1.0f}));
        includePoint(objectLocalToWorld(object, Vec3{0.0f, 0.0f, -1.0f}));
    }

    if (bounds.valid) {
        bounds.center = scale(add(bounds.minimum, bounds.maximum), 0.5f);
        bounds.span = subtract(bounds.maximum, bounds.minimum);
    }
    return bounds;
}

void applyCinematicFrameStaging3D(std::vector<Object3D>& objects,
                                  const VisualSettings& settings,
                                  const AudioMetrics& metrics,
                                  const SceneInterpretation& intent,
                                  const MusicRoleScene3D& role,
                                  SongSceneIdentity identity,
                                  float minimumDimension)
{
    if (objects.empty()) {
        return;
    }

    const ObjectBounds3D bounds = objectBounds3D(objects);
    if (!bounds.valid) {
        return;
    }

    const float clarity = patternClarityOf(settings);
    const float personality = scenePersonalityOf(settings);
    const float activeRoles = activeRoleCount3D(role);
    const float desiredSpanX = minimumDimension *
                               (0.78f +
                                role.separation * 0.22f +
                                role.space * 0.16f +
                                intent.spacious * 0.12f +
                                personality * 0.08f);
    const float desiredSpanY = minimumDimension *
                               (0.54f +
                                intent.melodic * 0.10f +
                                role.shadow * 0.08f +
                                role.bass * 0.05f +
                                personality * 0.05f);
    const float desiredSpanZ = minimumDimension *
                               (1.06f +
                                intent.depthReveal * 0.36f +
                                intent.architecture * 0.18f +
                                role.space * 0.30f +
                                role.drums * 0.14f +
                                role.bass * 0.16f +
                                metrics.dropIntensity * 0.18f +
                                std::min(activeRoles / 7.0f, 1.0f) * 0.16f);

    const auto fitScale = [](float desired, float actual, float minimumScale, float maximumScale) {
        if (actual <= 1.0f) {
            return 1.0f;
        }
        return std::clamp(desired / actual, minimumScale, maximumScale);
    };
    const float xScale = fitScale(desiredSpanX, bounds.span.x, 0.86f, 1.13f);
    const float yScale = fitScale(desiredSpanY, bounds.span.y, 0.86f, 1.14f);
    const float zScale = fitScale(desiredSpanZ, bounds.span.z, 0.92f, 1.16f);

    Vec3 targetCenter{
        minimumDimension * ((role.melody + role.fracture - role.drums - role.shadow) * 0.018f),
        minimumDimension * (role.bass * 0.020f + role.shadow * 0.018f - role.melody * 0.024f),
        minimumDimension * (0.08f + role.space * 0.10f - role.bass * 0.035f + intent.depthReveal * 0.030f)
    };
    switch (identity) {
    case SongSceneIdentity::BassPressure:
        targetCenter.y += minimumDimension * 0.018f;
        targetCenter.z -= minimumDimension * 0.040f;
        break;
    case SongSceneIdentity::AmbientOrbit:
        targetCenter.z += minimumDimension * 0.090f;
        break;
    case SongSceneIdentity::MelodicCrystal:
        targetCenter.y -= minimumDimension * 0.026f;
        targetCenter.z += minimumDimension * 0.030f;
        break;
    case SongSceneIdentity::BreakbeatFracture:
        targetCenter.x += minimumDimension * 0.025f;
        break;
    case SongSceneIdentity::DarkMonolith:
        targetCenter.y += minimumDimension * 0.026f;
        targetCenter.z += minimumDimension * 0.050f;
        break;
    case SongSceneIdentity::TechnoArchitecture:
    case SongSceneIdentity::CalmSpace:
        break;
    }

    const float centering = std::clamp(0.18f + clarity * 0.26f + role.separation * 0.12f, 0.18f, 0.52f);
    const Vec3 centerCorrection = scale(subtract(targetCenter, bounds.center), centering);
    const float depthExpansion = std::clamp(0.04f +
                                                intent.depthReveal * 0.10f +
                                                intent.architecture * 0.060f +
                                                role.separation * 0.08f +
                                                role.drums * 0.040f +
                                                metrics.stereoWidth * 0.05f,
                                            0.04f,
                                            0.30f);

    for (std::size_t i = 0; i < objects.size(); ++i) {
        Object3D& object = objects[i];
        const Vec3 relative = subtract(object.position, bounds.center);
        const float seed = static_cast<float>(i + 1U) * 0.389f +
                           static_cast<float>(static_cast<int>(object.kind)) * 0.257f;
        const float depthUnit = bounds.span.z > 1.0f
                                    ? clamp01((object.position.z - bounds.minimum.z) / bounds.span.z)
                                    : wrapUnit(std::sin(seed) * 0.5f + 0.5f);
        Vec3 staged{
            bounds.center.x + relative.x * xScale,
            bounds.center.y + relative.y * yScale,
            bounds.center.z + relative.z * zScale +
                (depthUnit - 0.5f) * minimumDimension * depthExpansion
        };
        object.position = add(staged, centerCorrection);

        if (object.kind == Object3DKind::Link) {
            const Vec3 targetRelative = subtract(object.target, bounds.center);
            const float targetDepthUnit = bounds.span.z > 1.0f
                                              ? clamp01((object.target.z - bounds.minimum.z) / bounds.span.z)
                                              : depthUnit;
            Vec3 stagedTarget{
                bounds.center.x + targetRelative.x * xScale,
                bounds.center.y + targetRelative.y * yScale,
                bounds.center.z + targetRelative.z * zScale +
                    (targetDepthUnit - 0.5f) * minimumDimension * depthExpansion
            };
            object.target = add(stagedTarget, centerCorrection);
        }
    }
}

ProjectedSceneStats3D projectedSceneStats3D(const std::vector<Object3D>& objects,
                                            const Camera3D& camera,
                                            float width,
                                            float height)
{
    ProjectedSceneStats3D stats;
    if (objects.empty()) {
        return stats;
    }

    float minX = width;
    float maxX = 0.0f;
    float minY = height;
    float maxY = 0.0f;
    float minDepth = objects.front().depth;
    float maxDepth = minDepth;
    int visibleSamples = 0;

    const auto includeProjected = [&](Vec3 point) {
        const Projected3D projected = projectPoint3D(point, camera);
        if (!projected.visible) {
            return;
        }
        minX = std::min(minX, std::clamp(projected.point.x, 0.0f, width));
        maxX = std::max(maxX, std::clamp(projected.point.x, 0.0f, width));
        minY = std::min(minY, std::clamp(projected.point.y, 0.0f, height));
        maxY = std::max(maxY, std::clamp(projected.point.y, 0.0f, height));
        ++visibleSamples;
    };

    for (const Object3D& object : objects) {
        minDepth = std::min(minDepth, object.depth);
        maxDepth = std::max(maxDepth, object.depth);
        includeProjected(object.position);
        if (object.kind == Object3DKind::Link) {
            includeProjected(object.target);
            continue;
        }
        includeProjected(objectLocalToWorld(object, Vec3{1.0f, 0.0f, 0.0f}));
        includeProjected(objectLocalToWorld(object, Vec3{-1.0f, 0.0f, 0.0f}));
        includeProjected(objectLocalToWorld(object, Vec3{0.0f, 1.0f, 0.0f}));
        includeProjected(objectLocalToWorld(object, Vec3{0.0f, -1.0f, 0.0f}));
    }

    if (visibleSamples > 0) {
        const float coverageX = std::clamp((maxX - minX) / std::max(1.0f, width), 0.0f, 1.0f);
        const float coverageY = std::clamp((maxY - minY) / std::max(1.0f, height), 0.0f, 1.0f);
        stats.screenCoverage = std::sqrt(coverageX * coverageY);
        const Vec2 center{(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
        const float dx = center.x - width * 0.5f;
        const float dy = center.y - height * 0.5f;
        stats.centerOffset = std::sqrt(dx * dx + dy * dy) / std::max(1.0f, std::min(width, height));
    }

    std::vector<float> sortedDepths;
    sortedDepths.reserve(objects.size());
    for (const Object3D& object : objects) {
        sortedDepths.push_back(object.depth);
    }
    std::sort(sortedDepths.begin(), sortedDepths.end());
    const float foregroundCut = sortedDepths[static_cast<std::size_t>(
        std::clamp(static_cast<int>(std::floor(static_cast<float>(sortedDepths.size() - 1U) * 0.34f)),
                   0,
                   static_cast<int>(sortedDepths.size() - 1U)))];
    const float backgroundCut = sortedDepths[static_cast<std::size_t>(
        std::clamp(static_cast<int>(std::floor(static_cast<float>(sortedDepths.size() - 1U) * 0.67f)),
                   0,
                   static_cast<int>(sortedDepths.size() - 1U)))];
    float foreground = 0.0f;
    float midground = 0.0f;
    float background = 0.0f;
    for (const Object3D& object : objects) {
        if (object.depth <= foregroundCut) {
            foreground += 1.0f;
        } else if (object.depth >= backgroundCut) {
            background += 1.0f;
        } else {
            midground += 1.0f;
        }
    }
    const float total = std::max(1.0f, foreground + midground + background);
    stats.foregroundShare = foreground / total;
    stats.midgroundShare = midground / total;
    stats.backgroundShare = background / total;
    return stats;
}

RoleSceneStats3D roleSceneStats3D(const std::vector<Object3D>& objects,
                                  const MusicRoleScene3D& role,
                                  float minimumDimension)
{
    RoleSceneStats3D stats;
    if (objects.empty()) {
        return stats;
    }

    std::array<Vec3, 7> sums{};
    std::array<int, 7> counts{};
    std::array<float, 7> roleMasses{};
    std::array<std::array<float, 14>, 7> roleKindMasses{};
    std::array<std::array<float, 5>, 7> silhouetteSums{};
    float totalVisualMass = 0.0f;
    float explicitRoleMass = 0.0f;
    float bridgeMass = 0.0f;

    const auto roleIndex = [](Object3DRole objectRole) {
        switch (objectRole) {
        case Object3DRole::Bass:
            return 0;
        case Object3DRole::Drums:
            return 1;
        case Object3DRole::Melody:
            return 2;
        case Object3DRole::Harmony:
            return 3;
        case Object3DRole::Space:
            return 4;
        case Object3DRole::Fracture:
            return 5;
        case Object3DRole::Shadow:
            return 6;
        case Object3DRole::Unassigned:
        case Object3DRole::Bridge:
        case Object3DRole::Convergence:
            break;
        }
        return -1;
    };
    const auto visualMass = [minimumDimension](const Object3D& object) {
        const float averageScale = (std::fabs(object.scale.x) +
                                    std::fabs(object.scale.y) +
                                    std::fabs(object.scale.z)) /
                                   3.0f;
        const float sizeWeight = std::clamp(std::sqrt(std::max(1.0f, averageScale) /
                                                      std::max(1.0f, minimumDimension * 0.10f)),
                                            0.42f,
                                            2.4f);
        return std::max(0.035f, object.color.a) *
               (0.64f + std::clamp(object.glow, 0.0f, 2.2f) * 0.30f) *
               sizeWeight;
    };

    for (const Object3D& object : objects) {
        const float mass = visualMass(object);
        totalVisualMass += mass;
        const int index = roleIndex(object.musicRole);
        if (index >= 0) {
            const std::size_t roleSlot = static_cast<std::size_t>(index);
            sums[roleSlot] = add(sums[roleSlot], object.position);
            ++counts[roleSlot];
            roleMasses[roleSlot] += mass;
            roleKindMasses[roleSlot][static_cast<std::size_t>(object.kind)] += mass;
            const float scaleX = std::fabs(object.scale.x);
            const float scaleY = std::fabs(object.scale.y);
            const float scaleZ = std::fabs(object.scale.z);
            const float averageScale = std::max(1.0f, (scaleX + scaleY + scaleZ) / 3.0f);
            silhouetteSums[roleSlot][0] += mass * std::clamp(scaleX / averageScale, 0.0f, 3.2f);
            silhouetteSums[roleSlot][1] += mass * std::clamp(scaleY / averageScale, 0.0f, 3.2f);
            silhouetteSums[roleSlot][2] += mass * std::clamp(scaleZ / averageScale, 0.0f, 3.2f);
            silhouetteSums[roleSlot][3] += mass * std::clamp((object.position.z / std::max(1.0f, minimumDimension)) * 0.5f + 0.5f,
                                                             0.0f,
                                                             1.6f);
            silhouetteSums[roleSlot][4] += mass * std::clamp(object.glow * 0.55f + object.color.a * 0.65f,
                                                             0.0f,
                                                             2.0f);
            explicitRoleMass += mass;
        } else if (object.musicRole == Object3DRole::Bridge) {
            bridgeMass += mass;
        } else if (object.musicRole == Object3DRole::Convergence) {
            bridgeMass += mass;
        }
    }

    stats.explicitRoleShare = explicitRoleMass / std::max(1.0f, totalVisualMass);
    stats.bridgeShare = bridgeMass / std::max(1.0f, totalVisualMass);

    const std::array<float, 7> strengths{
        role.bass,
        role.drums,
        role.melody,
        role.harmony,
        role.space,
        role.fracture,
        role.shadow
    };
    std::vector<Vec3> centers;
    centers.reserve(7);
    std::vector<std::size_t> activeRoleSlots;
    activeRoleSlots.reserve(7);
    float minimumShare = 1.0f;
    float maximumShare = 0.0f;
    int balancedRoles = 0;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        if (counts[i] <= 0 || strengths[i] <= 0.16f) {
            continue;
        }

        const float invCount = 1.0f / static_cast<float>(counts[i]);
        centers.push_back(scale(sums[i], invCount));
        activeRoleSlots.push_back(i);
        const float share = roleMasses[i] / std::max(1.0f, explicitRoleMass);
        minimumShare = std::min(minimumShare, share);
        maximumShare = std::max(maximumShare, share);
        ++balancedRoles;
    }

    float totalDistance = 0.0f;
    int pairs = 0;
    for (std::size_t i = 0; i < centers.size(); ++i) {
        for (std::size_t j = i + 1U; j < centers.size(); ++j) {
            totalDistance += length(subtract(centers[i], centers[j]));
            ++pairs;
        }
    }
    if (pairs > 0) {
        stats.districtSpread = (totalDistance / static_cast<float>(pairs)) / std::max(1.0f, minimumDimension);
    }
    if (balancedRoles > 1 && maximumShare > 0.0f) {
        stats.roleBalance = minimumShare / maximumShare;
    } else if (balancedRoles == 1) {
        stats.roleBalance = 1.0f;
    }

    float vocabularyDistance = 0.0f;
    float silhouetteDistance = 0.0f;
    int rolePairs = 0;
    for (std::size_t a = 0; a < activeRoleSlots.size(); ++a) {
        for (std::size_t b = a + 1U; b < activeRoleSlots.size(); ++b) {
            const std::size_t left = activeRoleSlots[a];
            const std::size_t right = activeRoleSlots[b];
            const float leftMass = std::max(0.001f, roleMasses[left]);
            const float rightMass = std::max(0.001f, roleMasses[right]);
            float overlap = 0.0f;
            for (std::size_t kind = 0; kind < roleKindMasses[left].size(); ++kind) {
                overlap += std::min(roleKindMasses[left][kind] / leftMass,
                                    roleKindMasses[right][kind] / rightMass);
            }
            vocabularyDistance += clamp01(1.0f - overlap);

            float signatureDistance = 0.0f;
            for (std::size_t feature = 0; feature < silhouetteSums[left].size(); ++feature) {
                const float leftFeature = silhouetteSums[left][feature] / leftMass;
                const float rightFeature = silhouetteSums[right][feature] / rightMass;
                const float delta = leftFeature - rightFeature;
                signatureDistance += delta * delta;
            }
            silhouetteDistance += std::clamp(std::sqrt(signatureDistance) * 0.52f, 0.0f, 1.0f);
            ++rolePairs;
        }
    }
    if (rolePairs > 0) {
        const float invPairs = 1.0f / static_cast<float>(rolePairs);
        stats.roleVocabulary = vocabularyDistance * invPairs;
        stats.roleSilhouetteContrast = silhouetteDistance * invPairs;
    } else if (activeRoleSlots.size() == 1U) {
        stats.roleVocabulary = 1.0f;
        stats.roleSilhouetteContrast = 1.0f;
    }
    return stats;
}

ColorRGBA shade3DColor(ColorRGBA color, float depthUnit, float glow, float light, float lightingGlow)
{
    const float fog = 0.42f + (1.0f - depthUnit) * 0.58f;
    const float value = std::clamp(fog * (0.72f + light * 0.42f + glow * lightingGlow * 0.36f), 0.12f, 1.45f);
    color.r = clamp01(color.r * value + glow * lightingGlow * 0.05f);
    color.g = clamp01(color.g * value + glow * lightingGlow * 0.05f);
    color.b = clamp01(color.b * value + glow * lightingGlow * 0.05f);
    color.a = clamp01(color.a * (0.28f + fog * 0.62f + glow * lightingGlow * 0.2f));
    return color;
}

void addProjectedLine(GeometryFrame& frame,
                      const Camera3D& camera,
                      Vec3 a,
                      Vec3 b,
                      ColorRGBA color,
                      float strokeWidth,
                      bool closed = false)
{
    const Projected3D pa = projectPoint3D(a, camera);
    const Projected3D pb = projectPoint3D(b, camera);
    if (!pa.visible || !pb.visible) {
        return;
    }
    const float width = strokeWidth * std::clamp((pa.perspective + pb.perspective) * 0.5f, 0.24f, 2.8f);
    const Vec2 shadowOffset{std::clamp(width * 1.25f, 1.0f, 4.0f), std::clamp(width * 1.6f, 1.0f, 5.0f)};
    frame.polylines.push_back(Polyline{
        {add(pa.point, shadowOffset), add(pb.point, shadowOffset)},
        width * 1.35f,
        ColorRGBA{0.0f, 0.0f, 0.0f, color.a * 0.06f},
        closed
    });
    frame.polylines.push_back(Polyline{{pa.point, pb.point}, width, color, closed});
    frame.projected3DOutlineVisualWeight += projectedLineWeight(color, width, 2U);
}

void addProjectedPolyline(GeometryFrame& frame,
                          const Camera3D& camera,
                          const std::vector<Vec3>& points,
                          ColorRGBA color,
                          float strokeWidth,
                          bool closed)
{
    std::vector<Vec2> projected;
    projected.reserve(points.size());
    float perspective = 0.0f;
    for (Vec3 point : points) {
        const Projected3D projectedPoint = projectPoint3D(point, camera);
        if (!projectedPoint.visible) {
            return;
        }
        projected.push_back(projectedPoint.point);
        perspective += projectedPoint.perspective;
    }
    if (projected.size() < 2U) {
        return;
    }
    perspective /= static_cast<float>(projected.size());
    const float width = strokeWidth * std::clamp(perspective, 0.22f, 2.8f);
    std::vector<Vec2> shadow;
    shadow.reserve(projected.size());
    const Vec2 shadowOffset{std::clamp(width * 1.15f, 1.0f, 4.0f), std::clamp(width * 1.45f, 1.0f, 5.0f)};
    for (Vec2 point : projected) {
        shadow.push_back(add(point, shadowOffset));
    }
    frame.polylines.push_back(Polyline{
        std::move(shadow),
        width * 1.25f,
        ColorRGBA{0.0f, 0.0f, 0.0f, color.a * 0.05f},
        closed
    });
    frame.polylines.push_back(Polyline{
        std::move(projected),
        width,
        color,
        closed
    });
    frame.projected3DOutlineVisualWeight += projectedLineWeight(color, width, projected.size());
}

float polygonArea(const std::vector<Vec2>& points)
{
    if (points.size() < 3U) {
        return 0.0f;
    }
    float area = 0.0f;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec2 a = points[i];
        const Vec2 b = points[(i + 1U) % points.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return std::fabs(area) * 0.5f;
}

bool addProjectedFace(GeometryFrame& frame,
                      const Camera3D& camera,
                      const std::vector<Vec3>& points,
                      ColorRGBA color,
                      float strokeWidth)
{
    if (points.size() < 3U) {
        return false;
    }

    std::vector<Vec2> projected;
    projected.reserve(points.size());
    float perspective = 0.0f;
    for (Vec3 point : points) {
        const Projected3D projectedPoint = projectPoint3D(point, camera);
        if (!projectedPoint.visible) {
            return false;
        }
        projected.push_back(projectedPoint.point);
        perspective += projectedPoint.perspective;
    }

    const float area = polygonArea(projected);
    if (area < 0.05f) {
        return false;
    }

    perspective /= static_cast<float>(projected.size());
    const float width = std::max(0.35f, strokeWidth * std::clamp(perspective, 0.22f, 2.6f));
    frame.polylines.push_back(Polyline{
        std::move(projected),
        width,
        color,
        true,
        true
    });
    frame.projected3DFaceCount += 1;
    frame.projected3DFillVisualWeight += std::sqrt(area) * color.a;
    frame.projected3DMaterialContrast = std::max(
        frame.projected3DMaterialContrast,
        std::fabs(luminance(color) - luminance(frame.background)) * color.a);
    return true;
}

void renderWireObject3D(GeometryFrame& frame,
                        const Camera3D& camera,
                        const Object3D& object,
                        float depthUnit,
                        float lightingGlow)
{
    const float light = 0.5f + 0.5f * std::cos(object.rotation.x + object.rotation.y + depthUnit * kPi);
    const ColorRGBA color = shade3DColor(object.color, depthUnit, object.glow, light, lightingGlow);
    const float stroke = 1.0f + object.glow * 1.8f;
    const float silhouetteAlpha = std::clamp(0.24f + depthUnit * 0.11f + object.glow * 0.022f,
                                             0.14f,
                                             0.48f);
    const float detailAlpha = std::clamp(silhouetteAlpha * 0.42f, 0.08f, 0.24f);
    const ColorRGBA silhouetteColor = withAlpha(color, color.a * silhouetteAlpha);
    const ColorRGBA detailColor = withAlpha(color, color.a * detailAlpha);
    const float silhouetteStroke = stroke * 0.54f;
    const float detailStroke = stroke * 0.32f;
    const auto world = [&](Vec3 local) { return objectLocalToWorld(object, local); };
    const float largestScale = std::max({object.scale.x, object.scale.y, object.scale.z});
    const Vec3 keyLight = normalize(Vec3{-0.34f, -0.58f, -0.74f});
    const auto addMaterialFace = [&](std::initializer_list<Vec3> locals, float alphaBias, float lightBias = 0.0f) {
        if (locals.size() < 3U || largestScale < 4.0f) {
            return;
        }

        std::vector<Vec3> facePoints;
        facePoints.reserve(locals.size());
        for (Vec3 local : locals) {
            facePoints.push_back(world(local));
        }

        const Vec3 normal = normalize(cross(subtract(facePoints[1], facePoints[0]),
                                           subtract(facePoints[2], facePoints[0])));
        const float faceLight = std::clamp(0.30f + std::fabs(dot(normal, keyLight)) * 0.62f + lightBias,
                                           0.18f,
                                           1.12f);
        ColorRGBA faceColor = shade3DColor(object.color, depthUnit, object.glow, faceLight, lightingGlow);
        faceColor = withAlpha(faceColor,
                              faceColor.a *
                                  std::clamp(alphaBias * (1.22f + lightingGlow * 0.54f + object.glow * 0.080f),
                                             0.08f,
                                             0.96f));
        addProjectedFace(frame, camera, facePoints, faceColor, stroke * 0.28f);
    };

    if (object.kind == Object3DKind::TunnelRib) {
        const int sides = std::max(4, static_cast<int>(std::round(5.0f + object.scale.z * 8.0f)));
        const float innerRadius = 0.62f;
        for (int i = 0; i < sides; i += 2) {
            const float a0 = (static_cast<float>(i) / static_cast<float>(sides)) * 2.0f * kPi;
            const float a1 = (static_cast<float>((i + 1) % sides) / static_cast<float>(sides)) * 2.0f * kPi;
            addMaterialFace({Vec3{std::cos(a0) * innerRadius, std::sin(a0) * innerRadius, 0.0f},
                             Vec3{std::cos(a0), std::sin(a0), 0.0f},
                             Vec3{std::cos(a1), std::sin(a1), 0.0f},
                             Vec3{std::cos(a1) * innerRadius, std::sin(a1) * innerRadius, 0.0f}},
                            0.34f,
                            0.02f);
        }
        std::vector<Vec3> points;
        points.reserve(static_cast<std::size_t>(sides));
        for (int i = 0; i < sides; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(sides);
            const float angle = unit * 2.0f * kPi;
            points.push_back(world(Vec3{std::cos(angle), std::sin(angle), 0.0f}));
        }
        addProjectedPolyline(frame, camera, points, silhouetteColor, silhouetteStroke, true);
        return;
    }

    if (object.kind == Object3DKind::Shard) {
        addMaterialFace({Vec3{0.0f, -1.0f, 0.0f},
                         Vec3{0.58f, 0.05f, 0.28f},
                         Vec3{0.0f, 1.0f, 0.0f},
                         Vec3{-0.42f, 0.1f, -0.34f}},
                        0.55f,
                        0.10f);
        const std::vector<Vec3> points = {
            world(Vec3{0.0f, -1.0f, 0.0f}),
            world(Vec3{0.58f, 0.05f, 0.28f}),
            world(Vec3{0.0f, 1.0f, 0.0f}),
            world(Vec3{-0.42f, 0.1f, -0.34f}),
            world(Vec3{0.0f, -1.0f, 0.0f})
        };
        addProjectedPolyline(frame, camera, points, silhouetteColor, silhouetteStroke, false);
        addProjectedLine(frame, camera, points[1], points[3], detailColor, detailStroke);
        return;
    }

    if (object.kind == Object3DKind::Plate) {
        addMaterialFace({Vec3{-1.0f, -1.0f, 0.0f},
                         Vec3{1.0f, -1.0f, 0.0f},
                         Vec3{1.0f, 1.0f, 0.0f},
                         Vec3{-1.0f, 1.0f, 0.0f}},
                        0.58f,
                        0.04f);
        const int ridges = 4;
        for (int i = -ridges; i <= ridges; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(ridges);
            const float ridge = std::sin((u + object.rotation.z) * kPi * 2.0f) * 0.12f;
            addProjectedLine(frame,
                             camera,
                             world(Vec3{-1.0f, u, ridge}),
                             world(Vec3{1.0f, u, -ridge}),
                             withAlpha(detailColor, detailColor.a * (0.74f + std::fabs(u) * 0.18f)),
                             detailStroke);
        }
        addProjectedPolyline(frame,
                             camera,
                             {world(Vec3{-1.0f, -1.0f, 0.0f}),
                              world(Vec3{1.0f, -1.0f, 0.0f}),
                              world(Vec3{1.0f, 1.0f, 0.0f}),
                              world(Vec3{-1.0f, 1.0f, 0.0f})},
                             silhouetteColor,
                             silhouetteStroke,
                             true);
        return;
    }

    if (object.kind == Object3DKind::DepthPlane) {
        addMaterialFace({Vec3{-1.0f, -1.0f, 0.0f},
                         Vec3{1.0f, -1.0f, 0.0f},
                         Vec3{1.0f, 1.0f, 0.0f},
                         Vec3{-1.0f, 1.0f, 0.0f}},
                        0.42f,
                        -0.03f);
        const int grid = 4;
        for (int i = -grid; i <= grid; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(grid);
            addProjectedLine(frame,
                             camera,
                             world(Vec3{-1.0f, u, 0.0f}),
                             world(Vec3{1.0f, u, 0.0f}),
                             withAlpha(detailColor, detailColor.a * 0.82f),
                             detailStroke * 0.74f);
            addProjectedLine(frame,
                             camera,
                             world(Vec3{u, -1.0f, 0.0f}),
                             world(Vec3{u, 1.0f, 0.0f}),
                             withAlpha(detailColor, detailColor.a * 0.68f),
                             detailStroke * 0.68f);
        }
        addProjectedPolyline(frame,
                             camera,
                             {world(Vec3{-1.0f, -1.0f, 0.0f}),
                              world(Vec3{1.0f, -1.0f, 0.0f}),
                              world(Vec3{1.0f, 1.0f, 0.0f}),
                              world(Vec3{-1.0f, 1.0f, 0.0f})},
                             silhouetteColor,
                             silhouetteStroke,
                             true);
        return;
    }

    if (object.kind == Object3DKind::Column) {
        const int sides = 6;
        std::vector<Vec3> top;
        std::vector<Vec3> bottom;
        top.reserve(sides);
        bottom.reserve(sides);
        for (int i = 0; i < sides; ++i) {
            const float angle = (static_cast<float>(i) / static_cast<float>(sides)) * 2.0f * kPi;
            top.push_back(world(Vec3{std::cos(angle), -1.0f, std::sin(angle)}));
            bottom.push_back(world(Vec3{std::cos(angle), 1.0f, std::sin(angle)}));
        }
        for (int i = 0; i < sides; i += 2) {
            const float a0 = (static_cast<float>(i) / static_cast<float>(sides)) * 2.0f * kPi;
            const float a1 = (static_cast<float>((i + 1) % sides) / static_cast<float>(sides)) * 2.0f * kPi;
            addMaterialFace({Vec3{std::cos(a0), -1.0f, std::sin(a0)},
                             Vec3{std::cos(a1), -1.0f, std::sin(a1)},
                             Vec3{std::cos(a1), 1.0f, std::sin(a1)},
                             Vec3{std::cos(a0), 1.0f, std::sin(a0)}},
                            0.38f,
                            0.02f);
        }
        addProjectedPolyline(frame, camera, top, silhouetteColor, silhouetteStroke, true);
        addProjectedPolyline(frame, camera, bottom, withAlpha(silhouetteColor, silhouetteColor.a * 0.74f), silhouetteStroke * 0.82f, true);
        for (int i = 0; i < sides; ++i) {
            addProjectedLine(frame, camera, top[static_cast<std::size_t>(i)], bottom[static_cast<std::size_t>(i)], detailColor, detailStroke);
        }
        return;
    }

    if (object.kind == Object3DKind::Cage) {
        const std::vector<Vec3> vertices = {
            world(Vec3{-1.0f, -1.0f, -1.0f}),
            world(Vec3{1.0f, -1.0f, -1.0f}),
            world(Vec3{1.0f, 1.0f, -1.0f}),
            world(Vec3{-1.0f, 1.0f, -1.0f}),
            world(Vec3{-1.0f, -1.0f, 1.0f}),
            world(Vec3{1.0f, -1.0f, 1.0f}),
            world(Vec3{1.0f, 1.0f, 1.0f}),
            world(Vec3{-1.0f, 1.0f, 1.0f})
        };
        static constexpr int cageEdges[][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}, {0, 6}, {1, 7}, {2, 4}, {3, 5}
        };
        addMaterialFace({Vec3{-1.0f, -1.0f, -1.0f},
                         Vec3{1.0f, -1.0f, -1.0f},
                         Vec3{1.0f, 1.0f, -1.0f},
                         Vec3{-1.0f, 1.0f, -1.0f}},
                        0.30f,
                        -0.04f);
        addMaterialFace({Vec3{-1.0f, -1.0f, 1.0f},
                         Vec3{1.0f, -1.0f, 1.0f},
                         Vec3{1.0f, 1.0f, 1.0f},
                         Vec3{-1.0f, 1.0f, 1.0f}},
                        0.40f,
                        0.06f);
        for (const auto& edge : cageEdges) {
            addProjectedLine(frame, camera, vertices[edge[0]], vertices[edge[1]], silhouetteColor, silhouetteStroke * 0.86f);
        }
        return;
    }

    if (object.kind == Object3DKind::WaveSurface) {
        addMaterialFace({Vec3{-1.0f, -1.0f, 0.0f},
                         Vec3{1.0f, -1.0f, 0.0f},
                         Vec3{1.0f, 1.0f, 0.0f},
                         Vec3{-1.0f, 1.0f, 0.0f}},
                        0.40f,
                        0.02f);
        constexpr int rows = 5;
        constexpr int columns = 10;
        for (int y = -rows; y <= rows; y += 2) {
            std::vector<Vec3> line;
            line.reserve(columns + 1);
            for (int x = 0; x <= columns; ++x) {
                const float u = (static_cast<float>(x) / static_cast<float>(columns)) * 2.0f - 1.0f;
                const float v = static_cast<float>(y) / static_cast<float>(rows);
                const float wave = std::sin((u * 2.6f + v * 1.8f + object.rotation.z) * kPi) * 0.18f;
                line.push_back(world(Vec3{u, v, wave}));
            }
            addProjectedPolyline(frame, camera, line, withAlpha(detailColor, detailColor.a * 0.82f), detailStroke, false);
        }
        for (int x = -columns; x <= columns; x += 4) {
            std::vector<Vec3> line;
            line.reserve(rows + 1);
            for (int y = 0; y <= rows; ++y) {
                const float u = static_cast<float>(x) / static_cast<float>(columns);
                const float v = (static_cast<float>(y) / static_cast<float>(rows)) * 2.0f - 1.0f;
                const float wave = std::cos((u * 1.7f - v * 2.2f + object.rotation.y) * kPi) * 0.14f;
                line.push_back(world(Vec3{u, v, wave}));
            }
            addProjectedPolyline(frame, camera, line, withAlpha(detailColor, detailColor.a * 0.62f), detailStroke * 0.86f, false);
        }
        return;
    }

    if (object.kind == Object3DKind::Node ||
        object.kind == Object3DKind::Particle ||
        object.kind == Object3DKind::Orbiter ||
        object.kind == Object3DKind::Anchor) {
        const float bodyAlpha = object.kind == Object3DKind::Anchor ? 0.56f :
                                (object.kind == Object3DKind::Orbiter ? 0.48f :
                                 object.kind == Object3DKind::Node ? 0.52f : 0.34f);
        addMaterialFace({Vec3{0.0f, -1.0f, 0.0f},
                         Vec3{1.0f, 0.0f, 0.0f},
                         Vec3{0.0f, 1.0f, 0.0f},
                         Vec3{-1.0f, 0.0f, 0.0f}},
                        bodyAlpha,
                        0.06f);
        addMaterialFace({Vec3{0.0f, -1.0f, 0.0f},
                         Vec3{0.0f, 0.0f, 1.0f},
                         Vec3{0.0f, 1.0f, 0.0f},
                         Vec3{0.0f, 0.0f, -1.0f}},
                        bodyAlpha * 0.82f,
                        -0.01f);
        if (object.kind == Object3DKind::Anchor) {
            addMaterialFace({Vec3{-0.45f, -1.0f, -0.45f},
                             Vec3{0.45f, -1.0f, -0.45f},
                             Vec3{0.45f, 1.0f, -0.45f},
                             Vec3{-0.45f, 1.0f, -0.45f}},
                            0.34f,
                            -0.03f);
        }
        const Projected3D projected = projectPoint3D(object.position, camera);
        if (!projected.visible) {
            return;
        }
        const float particleRadius = std::max(1.0f,
                                              object.scale.x * projected.perspective *
                                                  (object.kind == Object3DKind::Anchor ? 2.1f :
                                                   object.kind == Object3DKind::Node ? 1.55f :
                                                   object.kind == Object3DKind::Orbiter ? 1.25f : 0.88f));
        const ColorRGBA particleColor = withAlpha(color, color.a * 0.60f);
        frame.particles.push_back(Particle{
            projected.point,
            particleRadius,
            particleColor
        });
        frame.projected3DOutlineVisualWeight += particleColor.a * std::max(0.6f, particleRadius) * 1.6f;
        if (object.kind == Object3DKind::Node || object.kind == Object3DKind::Orbiter || object.kind == Object3DKind::Anchor) {
            const float ringRadius = std::max(2.0f,
                                              object.scale.x * projected.perspective *
                                                  (object.kind == Object3DKind::Anchor ? 4.7f :
                                                   object.kind == Object3DKind::Orbiter ? 3.0f : 2.0f));
            const float ringStroke = silhouetteStroke * 0.58f;
            const ColorRGBA ringColor = withAlpha(silhouetteColor, silhouetteColor.a * 0.46f);
            frame.rings.push_back(Ring{
                projected.point,
                ringRadius,
                object.kind == Object3DKind::Anchor ? 4 : 18,
                object.rotation.z,
                ringStroke,
                ringColor
            });
            frame.projected3DOutlineVisualWeight += ringColor.a * std::max(0.2f, ringStroke) * 6.0f;
            if (object.kind == Object3DKind::Anchor) {
                addProjectedLine(frame,
                                 camera,
                                 object.position,
                                 add(object.position, Vec3{object.scale.x * 2.2f, 0.0f, 0.0f}),
                                 detailColor,
                                 detailStroke);
            }
        }
        return;
    }

    if (object.kind == Object3DKind::Link) {
        const Vec3 segment = subtract(object.target, object.position);
        Vec3 ribbonSide = normalize(cross(segment, Vec3{0.0f, 0.0f, 1.0f}));
        if (length(ribbonSide) <= 0.0001f) {
            ribbonSide = normalize(cross(segment, Vec3{0.0f, 1.0f, 0.0f}));
        }
        const float ribbonWidth = std::clamp(0.006f + object.glow * 0.010f, 0.006f, 0.030f) *
                                  std::max({object.scale.x, object.scale.y, object.scale.z, 1.0f});
        const Vec3 offset = scale(ribbonSide, ribbonWidth * 64.0f);
        addProjectedFace(frame,
                         camera,
                         {add(object.position, offset),
                          add(object.target, offset),
                          subtract(object.target, offset),
                          subtract(object.position, offset)},
                         withAlpha(color, color.a * 0.64f),
                         0.25f + object.glow * 0.30f);
        addProjectedLine(frame,
                         camera,
                         object.position,
                         object.target,
                         withAlpha(detailColor, detailColor.a * 0.68f),
                         0.32f + object.glow * 0.44f);
        return;
    }

    if (object.kind == Object3DKind::Ribbon) {
        std::vector<Vec3> points;
        points.reserve(14);
        for (int i = 0; i < 14; ++i) {
            const float unit = static_cast<float>(i) / 13.0f;
            points.push_back(world(Vec3{
                std::cos(unit * kPi * 4.0f + object.rotation.z) * (0.35f + unit * 0.35f),
                (unit - 0.5f) * 2.0f,
                std::sin(unit * kPi * 4.0f + object.rotation.y) * 0.55f
            }));
        }
        for (int i = 0; i < 13; i += 2) {
            const float u0 = static_cast<float>(i) / 13.0f;
            const float u1 = static_cast<float>(i + 1) / 13.0f;
            const auto localRibbonPoint = [&](float unit, float side) {
                const float angle = unit * kPi * 4.0f + object.rotation.z;
                const float radius = 0.35f + unit * 0.35f;
                const Vec3 center{
                    std::cos(angle) * radius,
                    (unit - 0.5f) * 2.0f,
                    std::sin(unit * kPi * 4.0f + object.rotation.y) * 0.55f
                };
                const Vec3 sideVector{
                    std::cos(angle + kPi * 0.5f) * 0.16f,
                    0.0f,
                    std::sin(angle + kPi * 0.5f) * 0.16f
                };
                return add(center, scale(sideVector, side));
            };
            const std::vector<Vec3> ribbonFace = {
                world(localRibbonPoint(u0, -1.0f)),
                world(localRibbonPoint(u1, -1.0f)),
                world(localRibbonPoint(u1, 1.0f)),
                world(localRibbonPoint(u0, 1.0f))
            };
            addProjectedFace(frame,
                             camera,
                             ribbonFace,
                             withAlpha(color, color.a * (0.38f + depthUnit * 0.16f)),
                             stroke * 0.22f);
        }
        addProjectedPolyline(frame, camera, points, silhouetteColor, silhouetteStroke, false);
        return;
    }

    const std::vector<Vec3> vertices = {
        world(Vec3{-1.0f, -1.0f, -1.0f}),
        world(Vec3{1.0f, -1.0f, -1.0f}),
        world(Vec3{1.0f, 1.0f, -1.0f}),
        world(Vec3{-1.0f, 1.0f, -1.0f}),
        world(Vec3{-1.0f, -1.0f, 1.0f}),
        world(Vec3{1.0f, -1.0f, 1.0f}),
        world(Vec3{1.0f, 1.0f, 1.0f}),
        world(Vec3{-1.0f, 1.0f, 1.0f})
    };
    static constexpr int edges[][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    addMaterialFace({Vec3{-1.0f, -1.0f, -1.0f},
                     Vec3{1.0f, -1.0f, -1.0f},
                     Vec3{1.0f, 1.0f, -1.0f},
                     Vec3{-1.0f, 1.0f, -1.0f}},
                    0.28f,
                    -0.02f);
    addMaterialFace({Vec3{-1.0f, -1.0f, 1.0f},
                     Vec3{1.0f, -1.0f, 1.0f},
                     Vec3{1.0f, 1.0f, 1.0f},
                     Vec3{-1.0f, 1.0f, 1.0f}},
                    0.42f,
                    0.08f);
    addMaterialFace({Vec3{1.0f, -1.0f, -1.0f},
                     Vec3{1.0f, -1.0f, 1.0f},
                     Vec3{1.0f, 1.0f, 1.0f},
                     Vec3{1.0f, 1.0f, -1.0f}},
                    0.28f,
                    0.02f);
    for (const auto& edge : edges) {
        addProjectedLine(frame, camera, vertices[edge[0]], vertices[edge[1]], silhouetteColor, silhouetteStroke);
    }
}

void applyObjectInteraction3D(std::vector<Object3D>& objects,
                              const InteractionState& interaction,
                              const VisualSettings& settings,
                              const Camera3D& camera,
                              float width,
                              float height,
                              float time)
{
    if (!interaction.enabled || !interaction.active || objects.empty()) {
        return;
    }

    const Vec2 cursor{
        std::clamp(interaction.normalizedX, 0.0f, 1.0f) * width,
        std::clamp(interaction.normalizedY, 0.0f, 1.0f) * height
    };
    const float depthStrength = interactionDepthOf(settings);
    const float stability = motionStabilityOf(settings);
    const float clarity = patternClarityOf(settings);
    const float readability = 0.54f + (1.0f - clarity) * 0.28f + (1.0f - stability) * 0.18f;
    const float radius = std::min(width, height) *
                         (0.15f + depthStrength * 0.13f + interaction.velocity * 0.025f) *
                         (0.82f + (1.0f - clarity) * 0.18f);
    const float clickBoost = interaction.pressed ? 1.0f : 0.45f;
    const float maximumLift = std::min(width, height) * (0.10f + depthStrength * 0.14f) * readability;
    const float tumble = 0.50f + (1.0f - clarity) * 0.32f + (1.0f - stability) * 0.18f;
    const float glowCap = 1.10f + (1.0f - stability) * 0.42f + (1.0f - clarity) * 0.18f;
    for (Object3D& object : objects) {
        const Projected3D projected = projectPoint3D(object.position, camera);
        if (!projected.visible) {
            continue;
        }
        const float influence = std::exp(-distance2(projected.point, cursor) / std::max(1.0f, radius * radius));
        if (influence <= 0.001f) {
            continue;
        }
        const float ripple = std::sin(time * 8.0f - influence * kPi * 2.0f);
        const float rawLift = influence * depthStrength * (118.0f + interaction.velocity * 34.0f) * clickBoost * readability;
        const float lift = std::min(rawLift, maximumLift);
        object.position.z -= lift * (0.75f + ripple * 0.25f);
        object.velocity.z = -lift;
        object.rotation.x += influence * depthStrength * (0.20f + clickBoost * 0.15f) * tumble;
        object.rotation.y -= influence * depthStrength * 0.16f * tumble;
        object.glow = std::min(glowCap, object.glow + influence * (0.34f + clickBoost * 0.28f) * readability);
        const float scaleBoost = influence * (1.12f + (1.0f - clarity) * 2.20f + clickBoost * 0.48f);
        object.scale = add(object.scale, Vec3{scaleBoost, scaleBoost, scaleBoost});
    }
}

float clampMagnitude(float value, float maximum)
{
    return std::clamp(value, -maximum, maximum);
}

void applyPatternReadability3D(std::vector<Object3D>& objects,
                               const VisualSettings& settings,
                               const AudioMetrics& metrics,
                               const MusicRoleScene3D& role,
                               float minimumDimension)
{
    if (objects.empty()) {
        return;
    }

    const MusicMotionEnvelope envelope = musicEnvelope(metrics, settings);
    const float maxGlow = 0.95f +
                          (1.0f - envelope.stability) * 0.42f +
                          (1.0f - envelope.clarity) * 0.20f +
                          envelope.drop * 0.20f +
                          lightingGlowOf(settings) * 0.18f;
    const float minZ = -minimumDimension * (0.92f + (1.0f - envelope.clarity) * 0.18f);
    const float maxZ = minimumDimension * (2.55f + envelope.drop * 0.26f + (1.0f - envelope.clarity) * 0.30f);
    const float maxVelocityZ = minimumDimension * (0.18f + (1.0f - envelope.stability) * 0.16f);
    const float maxScaleXY = minimumDimension * (0.64f + (1.0f - envelope.clarity) * 0.22f + envelope.drop * 0.08f);
    const float maxScaleZ = minimumDimension * (1.72f + (1.0f - envelope.clarity) * 0.36f + envelope.drop * 0.18f);
    const float tumbleKeep = 0.58f + (1.0f - envelope.clarity) * 0.25f + (1.0f - envelope.stability) * 0.17f;
    const float readableHold = envelope.stability * envelope.clarity;
    const float readableDepthDamp = 1.0f - readableHold * 0.12f;
    const float readableGlowDamp = 1.0f - readableHold * 0.10f;
    const float readableScaleDamp = 1.0f - readableHold * 0.045f;
    const float roleCrowding = clamp01(activeRoleCount3D(role) / 6.0f +
                                       role.separation * 0.20f +
                                       role.convergence * 0.16f);

    for (Object3D& object : objects) {
        object.glow = std::min(object.glow, maxGlow);
        object.position.z = std::clamp(object.position.z, minZ, maxZ);
        object.velocity.z = clampMagnitude(object.velocity.z, maxVelocityZ);
        object.rotation.x *= tumbleKeep;
        object.rotation.y *= tumbleKeep;
        object.position.z *= readableDepthDamp;
        object.glow *= readableGlowDamp;
        object.scale.x = std::clamp(object.scale.x, -maxScaleXY, maxScaleXY);
        object.scale.y = std::clamp(object.scale.y, -maxScaleXY, maxScaleXY);
        object.scale.z = std::clamp(object.scale.z, -maxScaleZ, maxScaleZ);
        object.scale = scale(object.scale, readableScaleDamp);

        if (isExplicitMusicalPartRole(object.musicRole)) {
            const float surfaceDamp = object.kind == Object3DKind::DepthPlane ||
                                      object.kind == Object3DKind::WaveSurface ||
                                      object.kind == Object3DKind::Cage ||
                                      object.kind == Object3DKind::Plate
                                          ? 0.10f
                                          : 0.04f;
            object.color.a *= std::clamp(0.94f - roleCrowding * (0.12f + surfaceDamp), 0.68f, 0.96f);
            object.glow *= std::clamp(0.92f - roleCrowding * 0.16f, 0.66f, 0.94f);
        } else if (object.musicRole == Object3DRole::Bridge || object.musicRole == Object3DRole::Convergence) {
            object.color.a *= std::clamp(0.78f + role.convergence * 0.16f, 0.62f, 0.90f);
            object.glow *= std::clamp(0.74f + role.convergence * 0.20f, 0.58f, 0.92f);
        }
    }
}

void addSongIdentitySetPieces3D(std::vector<Object3D>& objects,
                                SongSceneIdentity identity,
                                const SceneInterpretation& intent,
                                const AudioMetrics& metrics,
                                const std::array<ColorRGBA, 5>& colors,
                                float minimumDimension,
                                float density,
                                float personality,
                                float response,
                                double time)
{
    const float phase = static_cast<float>(time);
    const float transient = transientEnergy3D(metrics);
    const float beat = metrics.beat ? std::max(metrics.beatConfidence, 0.22f) : metrics.beatConfidence * 0.45f;
    const float harmonic = clamp01(metrics.harmonicEnergy * 0.66f +
                                   metrics.keyConfidence * 0.28f +
                                   averageChromaEnergy(metrics) * 0.18f);
    const float stereoSpread = 0.72f + metrics.stereoWidth * 0.62f;
    const float energy = clamp01(metrics.rms * 1.35f +
                                 metrics.peak * 0.16f +
                                 metrics.bass * 0.20f +
                                 metrics.dropIntensity * 0.18f);
    const float scaleBoost = 0.90f + personality * 0.12f + std::min(response, 1.7f) * 0.045f;
    const std::size_t firstIdentityObject = objects.size();
    const auto pushLink = [&](Vec3 from, Vec3 to, ColorRGBA color, float glow) {
        Object3D link = makeObject3D(Object3DKind::Link,
                                     from,
                                     Vec3{1.0f, 1.0f, 1.0f},
                                     Vec3{},
                                     color,
                                     glow);
        link.target = to;
        objects.push_back(link);
    };

    switch (identity) {
    case SongSceneIdentity::CalmSpace: {
        const int planes = scaledCount(4, density * 0.50f);
        for (int i = 0; i < planes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, planes - 1));
            objects.push_back(makeObject3D(Object3DKind::DepthPlane,
                                           Vec3{0.0f,
                                                (unit - 0.5f) * minimumDimension * 0.10f,
                                                minimumDimension * (-0.34f + unit * 0.82f)},
                                           Vec3{minimumDimension * (0.16f + unit * 0.09f),
                                                minimumDimension * (0.08f + unit * 0.05f),
                                                minimumDimension * 0.012f},
                                           Vec3{0.36f + unit * 0.12f,
                                                phase * 0.010f,
                                                phase * 0.014f + unit},
                                           withAlpha(colors[(i + 4) % 5], 0.10f + intent.calm * 0.18f),
                                           0.10f + intent.calm * 0.26f));
        }
        const int orbiters = scaledCount(5, density * 0.48f);
        for (int i = 0; i < orbiters; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, orbiters));
            const float angle = unit * 2.0f * kPi + phase * 0.026f;
            const float radius = minimumDimension * (0.13f + static_cast<float>(i % 3) * 0.075f);
            objects.push_back(makeObject3D(Object3DKind::Orbiter,
                                           Vec3{std::cos(angle) * radius,
                                                std::sin(angle * 0.84f) * radius * 0.52f,
                                                minimumDimension * (-0.18f + unit * 0.56f)},
                                           Vec3{minimumDimension * 0.012f,
                                                minimumDimension * 0.012f,
                                                minimumDimension * 0.012f},
                                           Vec3{0.0f, angle, phase * 0.018f},
                                           withAlpha(colors[(i + 2) % 5], 0.16f + intent.calm * 0.16f),
                                           0.12f + intent.calm * 0.22f));
        }
        break;
    }
    case SongSceneIdentity::BassPressure: {
        const int rings = scaledCount(7, density * (0.70f + intent.mass * 0.32f));
        for (int i = 0; i < rings; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, rings - 1));
            const float compression = std::pow(1.0f - unit, 1.7f) * (0.20f + intent.drop * 0.22f + metrics.bass * 0.18f);
            const float radius = minimumDimension * (0.18f + unit * 0.20f + metrics.bass * 0.07f - compression * 0.05f);
            objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                           Vec3{0.0f,
                                                std::sin(phase * 0.060f + unit * kPi) * minimumDimension * 0.025f,
                                                minimumDimension * (-0.56f + unit * (1.44f + metrics.dropIntensity * 0.28f) -
                                                                    compression)},
                                           Vec3{radius,
                                                radius * (0.42f + metrics.lowMid * 0.20f),
                                                0.72f + intent.mass * 0.80f + unit * 0.34f},
                                           Vec3{compression * 0.26f,
                                                phase * 0.018f,
                                                metrics.beatPhase * kPi * 0.20f + unit * 0.42f},
                                           withAlpha(colors[(i + 1) % 5], 0.24f + metrics.bass * 0.22f),
                                           0.32f + intent.mass * 0.74f + metrics.dropIntensity * 0.34f));
        }
        const int weights = scaledCount(5, density * (0.58f + metrics.bass * 0.26f));
        for (int i = 0; i < weights; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, weights - 1));
            const float side = i % 2 == 0 ? -1.0f : 1.0f;
            const float x = (unit - 0.5f) * minimumDimension * 0.46f * stereoSpread;
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           Vec3{x,
                                                minimumDimension * (0.18f + metrics.bass * 0.04f),
                                                minimumDimension * (-0.24f + unit * 0.34f - metrics.dropIntensity * 0.12f)},
                                           Vec3{minimumDimension * (0.028f + metrics.bass * 0.018f),
                                                minimumDimension * (0.28f + metrics.bass * 0.22f + intent.drop * 0.10f),
                                                minimumDimension * (0.034f + metrics.bass * 0.018f)},
                                           Vec3{0.03f,
                                                side * (0.12f + metrics.bass * 0.08f),
                                                phase * 0.018f + side * metrics.beatPhase * 0.10f},
                                           withAlpha(mix(colors[1], colors[3], 0.28f), 0.24f + metrics.bass * 0.24f),
                                           0.28f + metrics.bass * response * 0.62f));
        }
        break;
    }
    case SongSceneIdentity::TechnoArchitecture: {
        const int lanes = scaledCount(6, density * (0.68f + intent.architecture * 0.28f));
        std::vector<Vec3> front;
        std::vector<Vec3> back;
        front.reserve(static_cast<std::size_t>(lanes * 2));
        back.reserve(static_cast<std::size_t>(lanes * 2));
        const float step = std::floor(metrics.barPhase * 8.0f) / 8.0f;
        for (int lane = 0; lane < lanes; ++lane) {
            const float u = static_cast<float>(lane) / static_cast<float>(std::max(1, lanes - 1));
            const float x = (u - 0.5f) * minimumDimension * 0.62f * stereoSpread;
            for (int side = -1; side <= 1; side += 2) {
                const float pulse = ((lane + (side > 0 ? 1 : 0)) % 4 == static_cast<int>(std::floor(step * 4.0f)))
                                        ? beat
                                        : beat * 0.26f;
                Vec3 a{x,
                       side * minimumDimension * 0.16f,
                       minimumDimension * (-0.24f + pulse * 0.06f)};
                Vec3 b{x,
                       side * minimumDimension * (0.16f + metrics.bass * 0.05f),
                       minimumDimension * (0.52f + metrics.bass * 0.10f)};
                front.push_back(a);
                back.push_back(b);
                objects.push_back(makeObject3D(Object3DKind::Column,
                                               a,
                                               Vec3{minimumDimension * 0.012f,
                                                    minimumDimension * (0.13f + pulse * 0.10f + metrics.bass * 0.05f),
                                                    minimumDimension * 0.014f},
                                               Vec3{0.0f, side * 0.10f, step * kPi + lane * 0.05f},
                                               withAlpha(colors[(lane + side + 7) % 5], 0.22f + pulse * 0.22f),
                                               0.18f + pulse * 0.52f + intent.architecture * 0.24f));
                pushLink(a,
                         b,
                         withAlpha(colors[(lane + 2) % 5], 0.12f + beat * 0.16f),
                         0.14f + intent.architecture * 0.34f);
            }
        }
        for (std::size_t i = 2; i < front.size(); i += 2) {
            pushLink(front[i - 2U],
                     front[i],
                     withAlpha(colors[0], 0.14f + beat * 0.12f),
                     0.12f + intent.architecture * 0.26f);
            pushLink(back[i - 1U],
                     back[std::min(i + 1U, back.size() - 1U)],
                     withAlpha(colors[3], 0.10f + metrics.bass * 0.12f),
                     0.10f + intent.architecture * 0.22f);
        }
        objects.push_back(makeObject3D(Object3DKind::Cage,
                                       Vec3{0.0f, 0.0f, minimumDimension * (0.16f + metrics.buildTension * 0.10f)},
                                       Vec3{minimumDimension * (0.16f + intent.architecture * 0.08f),
                                            minimumDimension * (0.11f + beat * 0.04f),
                                            minimumDimension * (0.14f + metrics.bass * 0.05f)},
                                       Vec3{phase * 0.05f,
                                            metrics.barPhase * kPi * 0.20f,
                                            step * kPi * 0.50f},
                                       withAlpha(colors[0], 0.22f + intent.architecture * 0.24f),
                                       0.22f + intent.architecture * 0.62f));
        break;
    }
    case SongSceneIdentity::AmbientOrbit: {
        const int orbiters = scaledCount(12, density * (0.58f + intent.spacious * 0.30f));
        std::vector<Vec3> anchors;
        anchors.reserve(static_cast<std::size_t>(orbiters));
        for (int i = 0; i < orbiters; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, orbiters));
            const float layer = static_cast<float>(i % 5) / 4.0f;
            const float angle = unit * 2.0f * kPi +
                                phase * (0.018f + metrics.stereoWidth * 0.018f) +
                                metrics.phrasePhase * kPi * 0.44f;
            const float radius = minimumDimension * (0.14f + layer * 0.12f + metrics.stereoWidth * 0.12f);
            Vec3 position{std::cos(angle) * radius * stereoSpread,
                          std::sin(angle * 0.76f) * radius * 0.58f,
                          minimumDimension * (-0.28f + layer * 0.54f + intent.spacious * 0.18f)};
            anchors.push_back(position);
            objects.push_back(makeObject3D(Object3DKind::Orbiter,
                                           position,
                                           Vec3{minimumDimension * (0.010f + intent.orbital * 0.012f),
                                                minimumDimension * (0.010f + intent.orbital * 0.012f),
                                                minimumDimension * 0.010f},
                                           Vec3{0.0f, angle, phase * 0.026f},
                                           withAlpha(colors[(i + 4) % 5], 0.20f + metrics.stereoWidth * 0.20f),
                                           0.18f + intent.orbital * 0.38f + metrics.stereoWidth * 0.18f));
        }
        for (std::size_t i = 0; i < anchors.size(); i += 3) {
            pushLink(anchors[i],
                     anchors[(i + 5U) % anchors.size()],
                     withAlpha(colors[(i + 2U) % 5U], 0.08f + intent.spacious * 0.12f),
                     0.08f + intent.spacious * 0.18f);
        }
        const int surfaces = scaledCount(3, density * (0.48f + metrics.stereoWidth * 0.24f));
        for (int i = 0; i < surfaces; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, surfaces - 1));
            objects.push_back(makeObject3D(Object3DKind::WaveSurface,
                                           Vec3{0.0f,
                                                (unit - 0.5f) * minimumDimension * 0.16f,
                                                minimumDimension * (-0.18f + unit * 0.58f)},
                                           Vec3{minimumDimension * (0.26f + metrics.stereoWidth * 0.10f),
                                                minimumDimension * (0.12f + harmonic * 0.05f),
                                                minimumDimension * 0.028f},
                                           Vec3{0.42f + unit * 0.18f,
                                                phase * 0.015f,
                                                phase * 0.018f + metrics.phrasePhase * kPi * 0.12f},
                                           withAlpha(colors[(i + 2) % 5], 0.13f + intent.spacious * 0.18f),
                                           0.12f + intent.spacious * 0.28f));
        }
        break;
    }
    case SongSceneIdentity::MelodicCrystal: {
        const int notes = scaledCount(18 + static_cast<int>(std::round(harmonic * 6.0f)),
                                      density * (0.66f + intent.crystal * 0.36f + harmonic * 0.18f));
        std::vector<Vec3> notes3D;
        notes3D.reserve(static_cast<std::size_t>(notes));
        for (int i = 0; i < notes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, notes));
            const float chroma = chromaAt(metrics, i);
            const float angle = unit * 2.0f * kPi + metrics.phrasePhase * kPi * 0.56f;
            const float lane = (chroma - 0.5f) * minimumDimension * 0.28f;
            Vec3 position{std::cos(angle) * minimumDimension * (0.12f + chroma * 0.22f) * stereoSpread,
                          lane + std::sin(angle * 1.38f) * minimumDimension * 0.08f,
                          minimumDimension * (-0.16f + chroma * 0.62f + harmonic * 0.16f)};
            notes3D.push_back(position);
            objects.push_back(makeObject3D(i % 4 == 0 ? Object3DKind::Cage : Object3DKind::Shard,
                                           position,
                                           Vec3{minimumDimension * (0.018f + chroma * 0.028f),
                                                minimumDimension * (0.030f + harmonic * 0.036f),
                                                minimumDimension * (0.014f + chroma * 0.018f)},
                                           Vec3{angle * 0.18f,
                                                phase * 0.055f + chroma * kPi,
                                                harmonic * kPi + metrics.keyConfidence * 0.24f},
                                           withAlpha(colors[(i + 2) % 5], 0.26f + chroma * 0.28f + harmonic * 0.10f),
                                           0.22f + chroma * response * 0.54f + harmonic * 0.42f));
            if (i % 3 == 0 || chroma > 0.30f) {
                objects.push_back(makeObject3D(Object3DKind::Node,
                                               add(position, Vec3{0.0f,
                                                                  minimumDimension * (0.018f + chroma * 0.030f),
                                                                  minimumDimension * (0.030f + harmonic * 0.050f)}),
                                               Vec3{minimumDimension * (0.010f + chroma * 0.012f),
                                                    minimumDimension * (0.010f + chroma * 0.012f),
                                                    minimumDimension * 0.010f},
                                               Vec3{0.0f, angle, phase * 0.040f},
                                               withAlpha(colors[(i + 4) % 5], 0.20f + harmonic * 0.22f + chroma * 0.12f),
                                               0.18f + harmonic * 0.46f + chroma * 0.28f));
            }
        }
        for (std::size_t i = 0; i < notes3D.size(); ++i) {
            const std::size_t next = (i + 2U + static_cast<std::size_t>(std::max(0, metrics.keyIndex + 12) % 5)) % notes3D.size();
            pushLink(notes3D[i],
                     notes3D[next],
                     withAlpha(colors[(i + 1U) % 5U], 0.10f + harmonic * 0.18f),
                     0.10f + harmonic * 0.30f);
        }
        break;
    }
    case SongSceneIdentity::BreakbeatFracture: {
        const int shards = scaledCount(24, density * (0.74f + transient * 0.46f));
        std::vector<Vec3> cuts;
        cuts.reserve(static_cast<std::size_t>(shards));
        const float quantizedBeat = std::floor(metrics.beatPhase * 8.0f) / 8.0f;
        for (int i = 0; i < shards; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, shards - 1));
            const float stagger = static_cast<float>((i * 7) % 13) / 12.0f;
            const float angle = (unit + quantizedBeat * 0.50f) * 2.0f * kPi;
            Vec3 position{(unit - 0.5f) * minimumDimension * 0.78f * stereoSpread,
                          (stagger - 0.5f) * minimumDimension * (0.30f + transient * 0.12f),
                          minimumDimension * (-0.28f + stagger * 0.84f + transient * 0.10f)};
            cuts.push_back(position);
            objects.push_back(makeObject3D(i % 2 == 0 ? Object3DKind::Plate : Object3DKind::Shard,
                                           position,
                                           Vec3{minimumDimension * (0.020f + transient * 0.030f),
                                                minimumDimension * (0.050f + metrics.onset * 0.080f),
                                                minimumDimension * (0.012f + transient * 0.024f)},
                                           Vec3{0.38f + stagger * 0.32f,
                                                angle * 0.20f + transient * 0.24f,
                                                quantizedBeat * kPi + stagger * 0.72f},
                                           withAlpha(colors[(i + 3) % 5], 0.24f + transient * 0.30f),
                                           0.24f + transient * 0.68f + metrics.onset * 0.24f));
        }
        for (std::size_t i = 1; i < cuts.size(); i += 3) {
            pushLink(cuts[i - 1U],
                     cuts[i],
                     withAlpha(colors[(i + 2U) % 5U], 0.08f + transient * 0.14f),
                     0.10f + transient * 0.24f);
        }
        break;
    }
    case SongSceneIdentity::DarkMonolith: {
        const int monoliths = scaledCount(5, density * (0.44f + intent.shadow * 0.22f));
        std::vector<Vec3> tops;
        tops.reserve(static_cast<std::size_t>(monoliths));
        for (int i = 0; i < monoliths; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, monoliths - 1));
            const float x = (unit - 0.5f) * minimumDimension * (0.54f + metrics.stereoWidth * 0.18f);
            const float height = 0.30f + intent.shadow * 0.22f + metrics.bass * 0.10f;
            Vec3 position{x,
                          minimumDimension * (0.13f + intent.dark * 0.06f),
                          minimumDimension * (-0.10f + unit * 0.38f)};
            tops.push_back(add(position, Vec3{0.0f, minimumDimension * height, 0.0f}));
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           position,
                                           Vec3{minimumDimension * (0.022f + intent.minimal * 0.012f),
                                                minimumDimension * height,
                                                minimumDimension * (0.032f + intent.shadow * 0.022f)},
                                           Vec3{0.04f + intent.tension * 0.08f,
                                                (unit - 0.5f) * 0.12f,
                                                phase * 0.008f},
                                           withAlpha(mix(colors[3], colors[4], 0.64f), 0.18f + intent.shadow * 0.24f),
                                           0.14f + intent.shadow * 0.44f + energy * 0.08f));
        }
        for (std::size_t i = 1; i < tops.size(); ++i) {
            pushLink(tops[i - 1U],
                     tops[i],
                     withAlpha(colors[4], 0.06f + intent.shadow * 0.10f),
                     0.08f + intent.shadow * 0.16f);
        }
        objects.push_back(makeObject3D(Object3DKind::DepthPlane,
                                       Vec3{0.0f, minimumDimension * 0.03f, minimumDimension * 0.26f},
                                       Vec3{minimumDimension * (0.30f + intent.minimal * 0.10f),
                                            minimumDimension * (0.12f + intent.dark * 0.08f),
                                            minimumDimension * 0.018f},
                                       Vec3{0.58f, phase * 0.006f, phase * 0.010f},
                                       withAlpha(colors[4], 0.08f + intent.shadow * 0.16f),
                                       0.10f + intent.shadow * 0.24f));
        objects.push_back(makeObject3D(Object3DKind::Anchor,
                                       Vec3{0.0f, -minimumDimension * 0.02f, minimumDimension * (0.10f + intent.dark * 0.18f)},
                                       Vec3{minimumDimension * (0.016f + intent.minimal * 0.012f),
                                            minimumDimension * (0.016f + intent.minimal * 0.012f),
                                            minimumDimension * 0.016f},
                                       Vec3{0.0f, phase * 0.014f, phase * 0.022f},
                                       withAlpha(colors[4], 0.22f + intent.shadow * 0.20f),
                                       0.16f + intent.shadow * 0.36f));
        break;
    }
    }

    for (std::size_t i = firstIdentityObject; i < objects.size(); ++i) {
        objects[i].scale = scale(objects[i].scale, scaleBoost);
    }
}

void applySongIdentityComposition3D(std::vector<Object3D>& objects,
                                    SongSceneIdentity identity,
                                    const SceneInterpretation& intent,
                                    const AudioMetrics& metrics,
                                    float minimumDimension)
{
    if (objects.empty()) {
        return;
    }

    const float transient = transientEnergy3D(metrics);
    const float harmonic = clamp01(metrics.harmonicEnergy * 0.66f +
                                   metrics.keyConfidence * 0.28f +
                                   averageChromaEnergy(metrics) * 0.18f);
    const float beat = metrics.beat ? std::max(metrics.beatConfidence, 0.20f) : metrics.beatConfidence * 0.45f;
    const float stereo = clamp01(metrics.stereoWidth);
    const float energy = clamp01(metrics.rms * 1.25f + metrics.peak * 0.14f + metrics.bass * 0.16f);
    const float count = static_cast<float>(objects.size());
    for (std::size_t i = 0; i < objects.size(); ++i) {
        Object3D& object = objects[i];
        const float unit = count > 1.0f ? static_cast<float>(i) / (count - 1.0f) : 0.0f;
        const float seed = unit * 17.173f + static_cast<float>(static_cast<int>(object.kind)) * 0.331f;
        const float sign = std::sin(seed * 2.0f) >= 0.0f ? 1.0f : -1.0f;

        switch (identity) {
        case SongSceneIdentity::CalmSpace:
            object.position.z += std::cos(seed + energy) * energy * minimumDimension * 0.080f;
            object.scale = scale(object.scale, 1.0f + std::clamp(energy * 0.018f, 0.0f, 0.035f));
            object.glow += energy * 0.085f;
            break;
        case SongSceneIdentity::BassPressure:
            object.position.y += std::fabs(std::sin(seed * 1.7f)) * intent.mass * minimumDimension * 0.055f;
            object.position.z -= (0.06f + unit * 0.060f) * metrics.bass * minimumDimension;
            object.scale = scale(object.scale, 1.0f + std::clamp(intent.mass * 0.026f, 0.0f, 0.06f));
            object.glow += intent.mass * 0.085f;
            break;
        case SongSceneIdentity::TechnoArchitecture:
            object.position.z += (std::floor(unit * 8.0f) - 3.5f) * beat * minimumDimension * 0.018f;
            object.rotation.z += sign * beat * 0.075f;
            object.glow += beat * 0.055f;
            break;
        case SongSceneIdentity::AmbientOrbit:
            object.position.x *= 1.0f + stereo * 0.11f;
            object.position.z += ((unit - 0.5f) * stereo * 0.24f + 0.060f) * minimumDimension;
            object.rotation.y += std::sin(seed) * stereo * 0.050f;
            object.glow += stereo * 0.040f + intent.spacious * 0.025f;
            break;
        case SongSceneIdentity::MelodicCrystal:
            object.position.z += harmonic * minimumDimension * (0.080f + unit * 0.160f);
            object.rotation.x += sign * harmonic * 0.075f;
            object.rotation.y += harmonic * 0.055f;
            object.scale = scale(object.scale, 1.0f + std::clamp(harmonic * 0.028f, 0.0f, 0.055f));
            object.glow += harmonic * 0.100f;
            break;
        case SongSceneIdentity::BreakbeatFracture:
            object.position.x += sign * transient * minimumDimension * (0.048f + unit * 0.052f);
            object.position.z += std::sin(seed * 5.0f) * transient * minimumDimension * 0.150f;
            object.rotation.y += sign * transient * 0.120f;
            object.rotation.z += std::sin(seed * 3.0f) * transient * 0.100f;
            object.glow += transient * 0.150f;
            break;
        case SongSceneIdentity::DarkMonolith:
            object.position.x *= 0.74f;
            object.position.y += minimumDimension * (0.022f + intent.dark * 0.050f);
            object.position.z += minimumDimension * (0.035f + unit * 0.170f);
            if (object.kind == Object3DKind::Column || object.kind == Object3DKind::DepthPlane) {
                object.scale.x *= 1.0f + std::clamp(intent.shadow * 0.050f, 0.0f, 0.10f);
                object.scale.y *= 1.0f + std::clamp(intent.shadow * 0.180f, 0.0f, 0.28f);
                object.scale.z *= 1.0f + std::clamp(intent.minimal * 0.080f, 0.0f, 0.14f);
                object.color.a = clamp01(object.color.a * 1.16f + 0.04f);
                object.glow += intent.shadow * 0.090f;
            } else if (object.kind == Object3DKind::Anchor) {
                object.scale = scale(object.scale, 1.0f + std::clamp(intent.shadow * 0.14f, 0.0f, 0.22f));
                object.color.a = clamp01(object.color.a * 1.18f + 0.06f);
                object.glow += intent.shadow * 0.100f;
            } else {
                object.position.z += minimumDimension * (0.34f + unit * 0.22f);
                object.scale = scale(object.scale, 0.42f);
                object.color.a *= 0.16f;
                object.glow *= 0.20f;
            }
            object.rotation.x *= 0.82f;
            object.rotation.y *= 0.78f;
            object.glow += intent.shadow * 0.035f;
            break;
        }
    }
}

void addTechnoMachineObjects(std::vector<Object3D>& objects,
                             const AudioMetrics& metrics,
                             const std::array<ColorRGBA, 5>& colors,
                             float minimumDimension,
                             float density,
                             float intensity,
                             float personality,
                             float response,
                             double time)
{
    const float phase = static_cast<float>(time);
    const int ribs = scaledCount(10, density * (0.86f + response * 0.16f));
    for (int i = 0; i < ribs; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(ribs);
        const float z = unit * minimumDimension * (2.2f + metrics.dropIntensity * response * 0.78f) -
                        minimumDimension * (0.55f + metrics.bass * response * 0.16f);
        objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                       Vec3{0.0f, 0.0f, z},
                                       Vec3{minimumDimension * (0.22f + unit * 0.12f + metrics.bass * response * 0.04f),
                                            minimumDimension * (0.14f + unit * 0.08f + metrics.bass * response * 0.032f),
                                            0.5f + unit},
                                       Vec3{metrics.dropIntensity * response * 0.14f,
                                            metrics.stereoWidth * response * 0.12f,
                                            phase * (0.35f + unit + response * 0.04f) + metrics.beatPhase * kPi},
                                       withAlpha(colors[i % 4], 0.28f + metrics.beatConfidence * 0.22f),
                                       0.34f + metrics.bass * response * 0.82f + metrics.dropIntensity * 0.28f));
    }

    const int machines = scaledCount(12, density * (0.8f + personality * 0.55f + response * 0.16f));
    for (int i = 0; i < machines; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(machines);
        const float angle = unit * 2.0f * kPi + phase * (0.42f + response * 0.08f) + metrics.beatPhase * response * 0.5f;
        const float radius = minimumDimension * (0.18f + metrics.stereoWidth * response * 0.13f + personality * 0.08f);
        const float size = minimumDimension * (0.035f + metrics.bass * response * 0.024f + intensity * 0.004f);
        objects.push_back(makeObject3D(Object3DKind::Polyhedron,
                                       Vec3{std::cos(angle) * radius,
                                            std::sin(angle * 1.3f) * radius * 0.55f,
                                            minimumDimension * (0.18f + std::sin(angle + phase) * (0.22f + response * 0.08f) +
                                                                metrics.dropIntensity * response * 0.18f)},
                                       Vec3{size * (1.0f + metrics.bass), size, size * (0.8f + metrics.lowMid)},
                                       Vec3{phase * (0.9f + unit),
                                            angle + metrics.beatConfidence,
                                            -phase * (0.5f + personality)},
                                       withAlpha(colors[(i + 1) % 4], 0.42f + metrics.dropIntensity * 0.24f),
                                       0.42f + metrics.beatConfidence * response * 0.42f + metrics.bass * response * 0.32f));
    }
}

void addCrystalStormObjects(std::vector<Object3D>& objects,
                            const AudioMetrics& metrics,
                            const std::array<ColorRGBA, 5>& colors,
                            float minimumDimension,
                            float density,
                            float personality,
                            float response,
                            double time)
{
    const float phase = static_cast<float>(time);
    const int shards = scaledCount(26, density * (0.72f + metrics.treble * response * 0.62f + metrics.spectralFlux * 0.2f));
    for (int i = 0; i < shards; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(shards);
        const float angle = unit * 2.3999632f + phase * (0.22f + metrics.spectralFlux * response * 0.18f);
        const float radius = minimumDimension * (0.08f + unit * 0.52f + metrics.stereoWidth * response * 0.14f);
        const float z = minimumDimension *
                        (std::sin(unit * kPi * (3.0f + response * 0.2f) + phase) * (0.48f + response * 0.08f) +
                         metrics.dropIntensity * response * 0.48f);
        const float size = minimumDimension * (0.026f + spectrumAt(metrics, i) * response * 0.04f + metrics.treble * 0.012f);
        objects.push_back(makeObject3D(Object3DKind::Shard,
                                       Vec3{std::cos(angle) * radius,
                                            std::sin(angle * 1.17f) * radius * 0.64f,
                                            z},
                                       Vec3{size * (0.8f + personality), size * (1.8f + metrics.treble), size},
                                       Vec3{angle + phase,
                                            phase * (0.7f + unit) + metrics.harmonicEnergy,
                                            angle * 0.4f},
                                       withAlpha(colors[(i + 2) % 4], 0.34f + metrics.treble * 0.34f),
                                       0.34f + metrics.treble * response * 0.86f + metrics.harmonicEnergy * 0.34f));
    }
}

void addNeuralSpaceObjects(std::vector<Object3D>& objects,
                           const AudioMetrics& metrics,
                           const std::array<ColorRGBA, 5>& colors,
                           float minimumDimension,
                           float density,
                           float personality,
                           float response,
                           double time)
{
    const float phase = static_cast<float>(time);
    const int nodes = scaledCount(18, density * (0.82f + metrics.barConfidence * response * 0.42f + metrics.onset * 0.16f));
    const std::size_t firstNode = objects.size();
    for (int i = 0; i < nodes; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(nodes);
        const float chroma = chromaAt(metrics, i);
        const float angle = unit * 2.0f * kPi + metrics.barPhase * kPi * (0.55f + response * 0.08f);
        const float layer = static_cast<float>(i % 5) / 4.0f;
        const float radius = minimumDimension * (0.12f + layer * 0.12f + metrics.stereoWidth * 0.12f);
        objects.push_back(makeObject3D(Object3DKind::Node,
                                       Vec3{std::cos(angle + phase * 0.12f) * radius,
                                            std::sin(angle * 1.41f + phase * 0.1f) * radius * 0.72f,
                                            minimumDimension * (-0.25f + layer * 0.32f + chroma * 0.32f +
                                                                metrics.downbeatConfidence * response * 0.1f)},
                                       Vec3{minimumDimension * (0.012f + chroma * 0.018f + metrics.beatConfidence * response * 0.009f),
                                            minimumDimension * (0.012f + chroma * 0.018f),
                                            minimumDimension * 0.012f},
                                       Vec3{phase * 0.3f, angle, metrics.phrasePhase * kPi},
                                       withAlpha(colors[i % 4], 0.36f + metrics.barConfidence * 0.28f),
                                       0.3f + metrics.downbeatConfidence * response * 0.82f + chroma * 0.4f));
    }
    for (int i = 0; i < nodes; ++i) {
        const Object3D& a = objects[firstNode + static_cast<std::size_t>(i)];
        const Object3D& b = objects[firstNode + static_cast<std::size_t>((i * 3 + 5) % nodes)];
        Object3D link = makeObject3D(Object3DKind::Link,
                                     a.position,
                                     Vec3{1.0f, 1.0f, 1.0f},
                                     Vec3{},
                                     withAlpha(colors[(i + 1) % 4], 0.18f + metrics.phraseIntensity * 0.2f),
                                     0.18f + metrics.barConfidence * response * 0.42f + personality * 0.18f);
        link.target = b.position;
        objects.push_back(link);
    }
}

void addDimensionalTunnelObjects(std::vector<Object3D>& objects,
                                 const AudioMetrics& metrics,
                                 const std::array<ColorRGBA, 5>& colors,
                                 float minimumDimension,
                                 float density,
                                 float personality,
                                 float response,
                                 double time)
{
    const float phase = static_cast<float>(time);
    const int layers = scaledCount(18, density * (0.72f + metrics.dropIntensity * response * 0.52f + metrics.bass * 0.12f));
    for (int i = 0; i < layers; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(layers);
        const float z = std::fmod(unit * minimumDimension * (2.8f + response * 0.16f) -
                                      phase * minimumDimension * (0.18f + metrics.dropIntensity * response * 0.42f + metrics.bass * 0.08f),
                                  minimumDimension * 2.8f);
        const float radius = minimumDimension * (0.12f + unit * 0.36f + metrics.stereoWidth * response * 0.1f);
        objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                       Vec3{std::sin(phase * 0.21f + unit * kPi) * minimumDimension * 0.04f,
                                            std::cos(phase * 0.17f + unit * kPi) * minimumDimension * 0.03f,
                                            z - minimumDimension * 0.45f},
                                       Vec3{radius, radius * (0.62f + metrics.phraseIntensity * 0.22f), 0.45f + unit * 0.65f},
                                       Vec3{metrics.phrasePhase * (0.35f + response * 0.08f),
                                            phase * 0.08f,
                                            phase * (0.38f + personality * 0.25f + response * 0.04f) + unit * kPi},
                                       withAlpha(colors[i % 4], 0.22f + unit * 0.22f),
                                       0.28f + metrics.dropIntensity * response * 0.76f));
        if (i % 3 == 0) {
            const float angle = unit * 2.0f * kPi + phase;
            objects.push_back(makeObject3D(Object3DKind::Polyhedron,
                                           Vec3{std::cos(angle) * radius * 0.74f,
                                                std::sin(angle) * radius * 0.42f,
                                                z},
                                           Vec3{minimumDimension * (0.026f + metrics.beatConfidence * response * 0.004f),
                                                minimumDimension * (0.026f + metrics.bass * response * 0.024f),
                                                minimumDimension * 0.026f},
                                           Vec3{phase + angle, phase * 0.7f, angle},
                                           withAlpha(colors[(i + 2) % 4], 0.32f + metrics.beatConfidence * 0.24f),
                                           0.24f + metrics.beatConfidence * response * 0.58f));
        }
    }
}

void addCymaticSculptureObjects(std::vector<Object3D>& objects,
                                const AudioMetrics& metrics,
                                const std::array<ColorRGBA, 5>& colors,
                                float minimumDimension,
                                float density,
                                float personality,
                                float response,
                                double time)
{
    const float phase = static_cast<float>(time);
    const int plates = scaledCount(5, density * (0.86f + metrics.harmonicEnergy * response * 0.18f));
    for (int i = 0; i < plates; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, plates - 1));
        const float z = minimumDimension *
                        (-0.24f + unit * 0.42f + metrics.harmonicEnergy * response * 0.13f +
                         metrics.buildTension * response * 0.08f);
        objects.push_back(makeObject3D(Object3DKind::Plate,
                                       Vec3{(unit - 0.5f) * minimumDimension * 0.38f,
                                            std::sin(phase * 0.27f + unit * kPi) * minimumDimension * 0.06f,
                                            z},
                                       Vec3{minimumDimension * (0.14f + metrics.buildTension * 0.05f),
                                            minimumDimension * (0.09f + metrics.harmonicEnergy * 0.04f),
                                            minimumDimension * (0.035f + personality * 0.02f)},
                                       Vec3{0.38f + metrics.phrasePhase * 0.34f,
                                            (unit - 0.5f) * 0.4f,
                                            phase * 0.12f + chromaAt(metrics, i) * kPi},
                                       withAlpha(colors[(i + 1) % 4], 0.32f + metrics.harmonicEnergy * 0.24f),
                                       0.32f + metrics.harmonicEnergy * response * 0.78f + metrics.buildTension * response * 0.34f));
    }

    const int nodal = scaledCount(20, density * (0.72f + metrics.harmonicEnergy * response * 0.46f + metrics.treble * 0.14f));
    for (int i = 0; i < nodal; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(nodal);
        const float angle = unit * 2.0f * kPi + phase * 0.18f;
        const float harmonic = chromaAt(metrics, i + 2U);
        const float radius = minimumDimension * (0.08f + harmonic * response * 0.38f + metrics.buildTension * 0.08f);
        objects.push_back(makeObject3D(Object3DKind::Particle,
                                       Vec3{std::cos(angle) * radius,
                                            std::sin(angle * 1.8f) * radius * 0.62f,
                                            minimumDimension * (std::sin(angle * 2.0f + phase * (1.0f + response * 0.1f)) *
                                                                    (0.2f + response * 0.05f) +
                                                                harmonic * 0.25f)},
                                       Vec3{minimumDimension * (0.01f + harmonic * 0.02f),
                                            minimumDimension * (0.01f + harmonic * 0.02f),
                                            minimumDimension * 0.01f},
                                       Vec3{},
                                       withAlpha(colors[i % 4], 0.25f + harmonic * 0.36f),
                                       0.22f + harmonic * 0.72f));
    }
}

void addModeSpecific3DObjects(std::vector<Object3D>& objects,
                              VisualMode mode,
                              const AudioMetrics& metrics,
                              const std::array<ColorRGBA, 5>& colors,
                              float minimumDimension,
                              float density,
                              float personality,
                              float response,
                              double time)
{
    const float phase = static_cast<float>(time);
    const float beat = metrics.beat ? metrics.beatConfidence : 0.0f;
    const float bassPush = metrics.bass * response;
    const float treblePush = metrics.treble * response;
    const float dropPush = metrics.dropIntensity * response;
    const float phrasePush = metrics.phraseIntensity * response + metrics.buildTension * response * 0.55f;

    switch (mode) {
    case VisualMode::QuantumTunnel: {
        const int planes = scaledCount(7, density * (0.78f + dropPush * 0.18f));
        for (int i = 0; i < planes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(planes);
            const float z = minimumDimension * (-0.48f + unit * (1.82f + dropPush * 0.42f)) -
                            phase * minimumDimension * (0.08f + dropPush * 0.06f);
            objects.push_back(makeObject3D(Object3DKind::DepthPlane,
                                           Vec3{0.0f, 0.0f, z},
                                           Vec3{minimumDimension * (0.18f + unit * 0.16f),
                                                minimumDimension * (0.12f + unit * 0.1f),
                                                minimumDimension * 0.02f},
                                           Vec3{metrics.stereoWidth * 0.12f, phase * 0.04f, phase * (0.12f + dropPush * 0.08f)},
                                           withAlpha(colors[i % 4], 0.16f + unit * 0.14f + dropPush * 0.08f),
                                           0.18f + dropPush * 0.52f));
        }
        const int orbiters = scaledCount(10, density * (0.65f + bassPush * 0.18f));
        for (int i = 0; i < orbiters; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(orbiters);
            const float angle = unit * 2.0f * kPi + phase * (0.56f + dropPush * 0.18f);
            const float radius = minimumDimension * (0.16f + metrics.stereoWidth * 0.16f + unit * 0.1f);
            objects.push_back(makeObject3D(Object3DKind::Orbiter,
                                           Vec3{std::cos(angle) * radius,
                                                std::sin(angle) * radius * 0.62f,
                                                minimumDimension * (0.18f + unit * 0.75f - dropPush * 0.2f)},
                                           Vec3{minimumDimension * (0.012f + bassPush * 0.012f),
                                                minimumDimension * 0.012f,
                                                minimumDimension * 0.012f},
                                           Vec3{0.0f, angle, phase},
                                           withAlpha(colors[(i + 1) % 4], 0.34f + dropPush * 0.2f),
                                           0.28f + bassPush * 0.42f + beat * 0.32f));
        }
        break;
    }
    case VisualMode::TechnoMandala: {
        const int columns = scaledCount(12, density * (0.72f + beat * 0.18f));
        for (int i = 0; i < columns; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(columns);
            const float angle = unit * 2.0f * kPi + metrics.beatPhase * kPi * 0.5f;
            const float radius = minimumDimension * (0.18f + bassPush * 0.04f);
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           Vec3{std::cos(angle) * radius,
                                                std::sin(angle) * radius,
                                                minimumDimension * (0.04f + std::sin(phase + angle) * 0.12f)},
                                           Vec3{minimumDimension * (0.012f + bassPush * 0.006f),
                                                minimumDimension * (0.12f + beat * 0.05f),
                                                minimumDimension * (0.012f + bassPush * 0.006f)},
                                           Vec3{0.0f, angle, phase * (0.2f + bassPush * 0.04f)},
                                           withAlpha(colors[i % 4], 0.34f + beat * 0.18f),
                                           0.28f + bassPush * 0.5f));
        }
        objects.push_back(makeObject3D(Object3DKind::Cage,
                                       Vec3{0.0f, 0.0f, minimumDimension * (0.12f + dropPush * 0.16f)},
                                       Vec3{minimumDimension * (0.11f + bassPush * 0.04f),
                                            minimumDimension * (0.11f + bassPush * 0.04f),
                                            minimumDimension * (0.08f + bassPush * 0.035f)},
                                       Vec3{phase * 0.34f, phase * 0.42f, metrics.beatPhase * 2.0f * kPi},
                                       withAlpha(colors[1], 0.46f + beat * 0.2f),
                                       0.4f + bassPush * 0.55f));
        break;
    }
    case VisualMode::LissajousMesh: {
        const int ribbons = scaledCount(6, density * (0.72f + metrics.stereoWidth * response * 0.22f));
        for (int i = 0; i < ribbons; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(ribbons);
            const float angle = unit * 2.0f * kPi;
            objects.push_back(makeObject3D(Object3DKind::Ribbon,
                                           Vec3{std::cos(angle) * minimumDimension * 0.09f,
                                                std::sin(angle) * minimumDimension * 0.07f,
                                                minimumDimension * (-0.18f + unit * 0.18f + metrics.stereoWidth * response * 0.2f)},
                                           Vec3{minimumDimension * (0.16f + metrics.stereoWidth * 0.08f),
                                                minimumDimension * (0.18f + treblePush * 0.03f),
                                                minimumDimension * (0.1f + bassPush * 0.04f)},
                                           Vec3{phase * (0.22f + unit * 0.08f),
                                                phase * (0.31f + metrics.stereoWidth * 0.08f),
                                                angle + metrics.beatPhase * kPi},
                                           withAlpha(colors[(i + 2) % 4], 0.34f + metrics.stereoWidth * 0.22f),
                                           0.24f + metrics.stereoWidth * response * 0.46f));
        }
        break;
    }
    case VisualMode::FrequencyBloom: {
        const int petals = scaledCount(8, density * (0.68f + treblePush * 0.22f));
        for (int i = 0; i < petals; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(petals);
            const float angle = unit * 2.0f * kPi + phase * 0.12f;
            const float energy = spectrumAt(metrics, i * 5U);
            objects.push_back(makeObject3D(Object3DKind::WaveSurface,
                                           Vec3{std::cos(angle) * minimumDimension * (0.11f + energy * 0.12f),
                                                std::sin(angle) * minimumDimension * (0.08f + energy * 0.08f),
                                                minimumDimension * (energy * response * 0.32f - 0.08f)},
                                           Vec3{minimumDimension * (0.08f + energy * response * 0.05f),
                                                minimumDimension * (0.12f + energy * response * 0.08f),
                                                minimumDimension * 0.04f},
                                           Vec3{phase * 0.1f, angle, phase * (0.18f + energy * 0.12f)},
                                           withAlpha(colors[i % 4], 0.28f + energy * 0.34f),
                                           0.24f + treblePush * 0.44f + energy * 0.3f));
        }
        break;
    }
    case VisualMode::FractalCathedral: {
        const int spires = scaledCount(9, density * (0.7f + phrasePush * 0.18f));
        for (int i = 0; i < spires; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, spires - 1));
            const float x = (unit - 0.5f) * minimumDimension * 0.58f;
            const float height = minimumDimension * (0.12f + phrasePush * 0.08f + std::fabs(unit - 0.5f) * 0.1f);
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           Vec3{x, minimumDimension * 0.08f, minimumDimension * (0.08f + unit * 0.26f)},
                                           Vec3{minimumDimension * 0.012f, height, minimumDimension * 0.014f},
                                           Vec3{0.18f + metrics.phrasePhase * 0.12f, 0.0f, phase * 0.04f},
                                           withAlpha(colors[(i + 3) % 4], 0.26f + phrasePush * 0.18f),
                                           0.2f + phrasePush * 0.45f));
        }
        objects.push_back(makeObject3D(Object3DKind::Cage,
                                       Vec3{0.0f, -minimumDimension * 0.04f, minimumDimension * (0.22f + phrasePush * 0.14f)},
                                       Vec3{minimumDimension * 0.22f, minimumDimension * 0.18f, minimumDimension * 0.16f},
                                       Vec3{0.4f + metrics.phrasePhase * 0.2f, phase * 0.05f, 0.0f},
                                       withAlpha(colors[4], 0.24f + phrasePush * 0.22f),
                                       0.22f + phrasePush * 0.38f));
        break;
    }
    case VisualMode::PolyrhythmLattice: {
        const int grid = scaledCount(5, density);
        const float spacing = minimumDimension * (0.08f + metrics.stereoWidth * 0.025f);
        for (int x = -grid; x <= grid; x += 2) {
            for (int y = -grid; y <= grid; y += 2) {
                const float phaseOffset = static_cast<float>(x * y) * 0.1f;
                objects.push_back(makeObject3D(Object3DKind::Column,
                                               Vec3{static_cast<float>(x) * spacing,
                                                    static_cast<float>(y) * spacing * 0.72f,
                                                    std::sin(phase + phaseOffset + metrics.beatPhase * kPi) *
                                                        minimumDimension * (0.08f + bassPush * 0.04f)},
                                               Vec3{minimumDimension * 0.009f,
                                                    minimumDimension * (0.06f + beat * 0.04f + bassPush * 0.03f),
                                                    minimumDimension * 0.009f},
                                               Vec3{0.0f, phaseOffset, phase * 0.12f},
                                               withAlpha(colors[(x + y + 64) % 4], 0.22f + beat * 0.16f),
                                               0.18f + bassPush * 0.38f));
            }
        }
        break;
    }
    case VisualMode::SpectralOrigami: {
        const int folds = scaledCount(7, density * (0.72f + treblePush * 0.18f));
        for (int i = 0; i < folds; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(folds);
            objects.push_back(makeObject3D(Object3DKind::DepthPlane,
                                           Vec3{(unit - 0.5f) * minimumDimension * 0.44f,
                                                std::sin(phase + unit * kPi) * minimumDimension * 0.06f,
                                                minimumDimension * (-0.12f + unit * 0.38f + treblePush * 0.12f)},
                                           Vec3{minimumDimension * (0.08f + treblePush * 0.03f),
                                                minimumDimension * (0.16f + personality * 0.04f),
                                                minimumDimension * 0.02f},
                                           Vec3{phase * 0.18f + unit, 0.62f + unit * 0.5f, phase * 0.12f},
                                           withAlpha(colors[i % 4], 0.2f + treblePush * 0.18f),
                                           0.2f + treblePush * 0.44f));
        }
        break;
    }
    case VisualMode::ChromaKaleidoscope: {
        const int prisms = scaledCount(10, density * (0.68f + metrics.harmonicEnergy * response * 0.18f));
        for (int i = 0; i < prisms; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(prisms);
            const float angle = unit * 2.0f * kPi + metrics.keyConfidence * response;
            const float chroma = chromaAt(metrics, i);
            objects.push_back(makeObject3D(Object3DKind::Cage,
                                           Vec3{std::cos(angle) * minimumDimension * (0.16f + chroma * 0.12f),
                                                std::sin(angle) * minimumDimension * (0.1f + chroma * 0.08f),
                                                minimumDimension * (-0.16f + chroma * response * 0.5f)},
                                           Vec3{minimumDimension * (0.026f + chroma * 0.024f),
                                                minimumDimension * (0.05f + chroma * 0.036f),
                                                minimumDimension * (0.026f + chroma * 0.024f)},
                                           Vec3{phase * 0.18f, angle + phase * 0.08f, metrics.harmonicEnergy * kPi},
                                           withAlpha(colors[(i + 2) % 4], 0.24f + chroma * 0.42f),
                                           0.2f + chroma * response * 0.7f));
        }
        break;
    }
    case VisualMode::HyperspacePolytope: {
        const int cages = scaledCount(5, density * (0.78f + dropPush * 0.12f));
        for (int i = 0; i < cages; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(cages);
            objects.push_back(makeObject3D(Object3DKind::Cage,
                                           Vec3{std::sin(unit * kPi * 2.0f + phase * 0.12f) * minimumDimension * 0.16f,
                                                std::cos(unit * kPi * 2.0f + phase * 0.16f) * minimumDimension * 0.1f,
                                                minimumDimension * (-0.22f + unit * 0.55f + dropPush * 0.12f)},
                                           Vec3{minimumDimension * (0.06f + unit * 0.018f + dropPush * 0.02f),
                                                minimumDimension * (0.06f + unit * 0.018f),
                                                minimumDimension * (0.06f + unit * 0.018f)},
                                           Vec3{phase * 0.24f + unit, phase * 0.31f, phase * 0.18f + metrics.beatPhase},
                                           withAlpha(colors[i % 4], 0.26f + dropPush * 0.18f),
                                           0.24f + dropPush * 0.46f + metrics.harmonicEnergy * 0.2f));
        }
        break;
    }
    case VisualMode::PhaseWeave: {
        const int currents = scaledCount(8, density * (0.72f + metrics.stereoWidth * response * 0.2f));
        for (int i = 0; i < currents; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(currents);
            objects.push_back(makeObject3D(Object3DKind::Ribbon,
                                           Vec3{(unit - 0.5f) * minimumDimension * 0.5f,
                                                std::sin(phase * 0.2f + unit * kPi) * minimumDimension * 0.12f,
                                                minimumDimension * (std::cos(unit * kPi + phase * 0.11f) * 0.22f)},
                                           Vec3{minimumDimension * (0.22f + metrics.stereoWidth * response * 0.1f),
                                                minimumDimension * 0.2f,
                                                minimumDimension * (0.12f + metrics.spectralFlux * response * 0.04f)},
                                           Vec3{phase * 0.12f, phase * (0.24f + metrics.stereoWidth * 0.08f), unit * kPi + phase * 0.18f},
                                           withAlpha(colors[(i + 1) % 4], 0.24f + metrics.stereoWidth * 0.26f),
                                           0.22f + metrics.stereoWidth * response * 0.5f + metrics.spectralFlux * 0.24f));
        }
        break;
    }
    case VisualMode::ResonanceTessellation: {
        const int planes = scaledCount(6, density * (0.76f + metrics.harmonicEnergy * response * 0.14f));
        for (int i = 0; i < planes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(planes);
            objects.push_back(makeObject3D(Object3DKind::DepthPlane,
                                           Vec3{std::sin(unit * kPi * 2.0f + phase * 0.08f) * minimumDimension * 0.12f,
                                                (unit - 0.5f) * minimumDimension * 0.24f,
                                                minimumDimension * (-0.2f + unit * 0.42f + metrics.harmonicEnergy * response * 0.12f)},
                                           Vec3{minimumDimension * (0.12f + metrics.buildTension * 0.04f),
                                                minimumDimension * (0.1f + metrics.harmonicEnergy * 0.04f),
                                                minimumDimension * 0.02f},
                                           Vec3{0.58f + metrics.phrasePhase * 0.2f, unit * 0.3f, phase * 0.08f},
                                           withAlpha(colors[i % 4], 0.18f + metrics.harmonicEnergy * 0.22f),
                                           0.22f + metrics.harmonicEnergy * response * 0.48f));
        }
        break;
    }
    case VisualMode::NeuralConstellation: {
        const int anchors = scaledCount(5, density * (0.76f + metrics.barConfidence * response * 0.14f));
        for (int i = 0; i < anchors; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(anchors);
            const float angle = unit * 2.0f * kPi + metrics.barPhase * kPi;
            objects.push_back(makeObject3D(Object3DKind::Anchor,
                                           Vec3{std::cos(angle) * minimumDimension * 0.24f,
                                                std::sin(angle) * minimumDimension * 0.15f,
                                                minimumDimension * (-0.12f + unit * 0.28f + metrics.downbeatConfidence * response * 0.12f)},
                                           Vec3{minimumDimension * (0.012f + metrics.downbeatConfidence * response * 0.008f),
                                                minimumDimension * 0.012f,
                                                minimumDimension * 0.012f},
                                           Vec3{0.0f, angle, phase * 0.24f + metrics.beatPhase * kPi},
                                           withAlpha(colors[i % 4], 0.3f + metrics.barConfidence * 0.24f),
                                           0.28f + metrics.downbeatConfidence * response * 0.62f));
        }
        break;
    }
    case VisualMode::CymaticInterference: {
        const int surfaces = scaledCount(4, density * (0.9f + metrics.harmonicEnergy * response * 0.16f));
        for (int i = 0; i < surfaces; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(surfaces);
            objects.push_back(makeObject3D(Object3DKind::WaveSurface,
                                           Vec3{0.0f,
                                                (unit - 0.5f) * minimumDimension * 0.18f,
                                                minimumDimension * (-0.16f + unit * 0.24f + metrics.buildTension * response * 0.1f)},
                                           Vec3{minimumDimension * (0.18f + metrics.harmonicEnergy * response * 0.04f),
                                                minimumDimension * (0.12f + metrics.buildTension * 0.04f),
                                                minimumDimension * 0.04f},
                                           Vec3{0.48f + metrics.phrasePhase * 0.24f, 0.18f * unit, phase * 0.1f + chromaAt(metrics, i) * kPi},
                                           withAlpha(colors[(i + 2) % 4], 0.22f + metrics.harmonicEnergy * 0.24f),
                                           0.28f + metrics.harmonicEnergy * response * 0.64f + metrics.buildTension * 0.22f));
        }
        break;
    }
    }
}

void addModeSilhouetteAnchors3D(std::vector<Object3D>& objects,
                                VisualMode mode,
                                const AudioMetrics& metrics,
                                const std::array<ColorRGBA, 5>& colors,
                                float minimumDimension,
                                float density,
                                float personality,
                                float response,
                                double time)
{
    const float phase = static_cast<float>(time);
    const float beat = metrics.beat ? metrics.beatConfidence : 0.0f;
    const float bassPush = metrics.bass * response;
    const float dropPush = metrics.dropIntensity * response;
    const float treblePush = metrics.treble * response;
    const float harmonicPush = metrics.harmonicEnergy * response;
    const float identity = 0.72f + personality * 0.34f + density * 0.08f;
    const float wide = minimumDimension * identity;

    switch (mode) {
    case VisualMode::QuantumTunnel: {
        for (int i = 0; i < 5; ++i) {
            const float unit = static_cast<float>(i) / 4.0f;
            const float z = minimumDimension * (-0.42f + unit * (1.64f + dropPush * 0.22f));
            const float radius = minimumDimension * (0.20f + unit * 0.18f + bassPush * 0.035f);
            objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                           Vec3{0.0f, 0.0f, z},
                                           Vec3{radius, radius * (0.58f + dropPush * 0.06f), 0.82f + unit * 0.50f},
                                           Vec3{phase * 0.045f, dropPush * 0.08f, phase * (0.16f + unit * 0.05f)},
                                           withAlpha(colors[(i + 1) % 4], 0.34f + unit * 0.08f),
                                           0.42f + dropPush * 0.58f));
        }
        break;
    }
    case VisualMode::TechnoMandala: {
        for (int i = 0; i < 4; ++i) {
            const float side = i < 2 ? -1.0f : 1.0f;
            const float lane = i % 2 == 0 ? -0.42f : 0.42f;
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           Vec3{side * wide * 0.34f,
                                                lane * wide * 0.20f,
                                                minimumDimension * (0.02f + beat * 0.08f)},
                                           Vec3{minimumDimension * 0.018f,
                                                minimumDimension * (0.30f + beat * 0.10f + bassPush * 0.06f),
                                                minimumDimension * 0.022f},
                                           Vec3{0.0f, side * 0.16f, phase * 0.08f + side * beat * 0.10f},
                                           withAlpha(colors[(i + 2) % 4], 0.34f + beat * 0.18f),
                                           0.36f + beat * 0.42f + bassPush * 0.30f));
        }
        break;
    }
    case VisualMode::LissajousMesh: {
        for (int i = 0; i < 5; ++i) {
            const float unit = static_cast<float>(i) / 4.0f;
            const float lane = unit - 0.5f;
            objects.push_back(makeObject3D(Object3DKind::Ribbon,
                                           Vec3{lane * wide * 0.54f,
                                                std::sin(phase * 0.12f + unit * kPi) * minimumDimension * 0.08f,
                                                minimumDimension * (-0.10f + unit * 0.34f + metrics.stereoWidth * 0.10f)},
                                           Vec3{minimumDimension * (0.30f + metrics.stereoWidth * 0.12f),
                                                minimumDimension * (0.18f + treblePush * 0.04f),
                                                minimumDimension * (0.13f + bassPush * 0.03f)},
                                           Vec3{phase * 0.10f + unit,
                                                phase * 0.22f,
                                                unit * kPi + metrics.phrasePhase * kPi * 0.5f},
                                           withAlpha(colors[(i + 1) % 4], 0.30f + metrics.stereoWidth * 0.22f),
                                           0.30f + metrics.stereoWidth * response * 0.42f));
        }
        break;
    }
    case VisualMode::FrequencyBloom: {
        for (int i = 0; i < 7; ++i) {
            const float unit = static_cast<float>(i) / 7.0f;
            const float angle = unit * 2.0f * kPi + phase * 0.06f;
            const float energy = spectrumAt(metrics, static_cast<std::size_t>(i * 6));
            objects.push_back(makeObject3D(Object3DKind::WaveSurface,
                                           Vec3{std::cos(angle) * wide * (0.22f + energy * 0.08f),
                                                std::sin(angle) * wide * (0.16f + energy * 0.06f),
                                                minimumDimension * (-0.04f + energy * 0.28f + treblePush * 0.10f)},
                                           Vec3{minimumDimension * (0.12f + energy * 0.06f),
                                                minimumDimension * (0.24f + energy * 0.10f),
                                                minimumDimension * 0.045f},
                                           Vec3{0.34f + treblePush * 0.08f, angle, phase * 0.08f + unit},
                                           withAlpha(colors[i % 4], 0.28f + energy * 0.30f + treblePush * 0.08f),
                                           0.32f + treblePush * 0.48f + energy * 0.20f));
        }
        break;
    }
    case VisualMode::FractalCathedral: {
        for (int i = 0; i < 7; ++i) {
            const float unit = static_cast<float>(i) / 6.0f;
            const float lane = unit - 0.5f;
            const float height = minimumDimension * (0.26f + std::abs(lane) * 0.18f + metrics.phraseIntensity * response * 0.10f);
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           Vec3{lane * wide * 0.72f,
                                                minimumDimension * 0.16f,
                                                minimumDimension * (0.04f + std::abs(lane) * 0.26f)},
                                           Vec3{minimumDimension * 0.016f, height, minimumDimension * 0.020f},
                                           Vec3{0.10f + metrics.phrasePhase * 0.12f, lane * 0.18f, phase * 0.018f},
                                           withAlpha(colors[(i + 3) % 4], 0.28f + metrics.phraseIntensity * 0.16f),
                                           0.28f + metrics.phraseIntensity * response * 0.44f));
        }
        break;
    }
    case VisualMode::PolyrhythmLattice: {
        for (int x = -3; x <= 3; x += 2) {
            for (int y = -3; y <= 3; y += 2) {
                const float phaseOffset = static_cast<float>(x * y) * 0.13f;
                objects.push_back(makeObject3D(Object3DKind::Column,
                                               Vec3{static_cast<float>(x) * wide * 0.105f,
                                                    static_cast<float>(y) * wide * 0.075f,
                                                    std::sin(phase + phaseOffset + metrics.beatPhase * kPi) *
                                                        minimumDimension * (0.10f + bassPush * 0.04f)},
                                               Vec3{minimumDimension * 0.012f,
                                                    minimumDimension * (0.11f + beat * 0.08f + bassPush * 0.04f),
                                                    minimumDimension * 0.012f},
                                               Vec3{0.0f, phaseOffset, phase * 0.10f},
                                               withAlpha(colors[(x + y + 64) % 4], 0.24f + beat * 0.18f),
                                               0.24f + beat * 0.34f + bassPush * 0.30f));
            }
        }
        break;
    }
    case VisualMode::SpectralOrigami: {
        for (int i = 0; i < 6; ++i) {
            const float unit = static_cast<float>(i) / 5.0f;
            objects.push_back(makeObject3D(Object3DKind::Shard,
                                           Vec3{(unit - 0.5f) * wide * 0.76f,
                                                (0.5f - unit) * wide * 0.24f,
                                                minimumDimension * (-0.14f + unit * 0.56f + treblePush * 0.10f)},
                                           Vec3{minimumDimension * (0.045f + treblePush * 0.018f),
                                                minimumDimension * (0.18f + metrics.onset * 0.06f),
                                                minimumDimension * (0.030f + treblePush * 0.014f)},
                                           Vec3{0.54f + unit * 0.34f, 0.42f + unit, phase * 0.10f},
                                           withAlpha(colors[i % 4], 0.28f + treblePush * 0.18f),
                                           0.30f + treblePush * 0.52f + metrics.onset * 0.20f));
        }
        break;
    }
    case VisualMode::ChromaKaleidoscope: {
        for (int i = 0; i < 8; ++i) {
            const float unit = static_cast<float>(i) / 8.0f;
            const float angle = unit * 2.0f * kPi + metrics.keyConfidence * 0.22f;
            const float chroma = chromaAt(metrics, i);
            objects.push_back(makeObject3D(Object3DKind::Cage,
                                           Vec3{std::cos(angle) * wide * (0.26f + chroma * 0.06f),
                                                std::sin(angle) * wide * (0.18f + chroma * 0.05f),
                                                minimumDimension * (-0.12f + chroma * 0.54f)},
                                           Vec3{minimumDimension * (0.040f + chroma * 0.026f),
                                                minimumDimension * (0.085f + chroma * 0.040f),
                                                minimumDimension * (0.040f + chroma * 0.026f)},
                                           Vec3{phase * 0.06f, angle + phase * 0.04f, metrics.harmonicEnergy * kPi},
                                           withAlpha(colors[(i + 2) % 4], 0.28f + chroma * 0.38f),
                                           0.30f + chroma * response * 0.62f + harmonicPush * 0.12f));
        }
        break;
    }
    case VisualMode::HyperspacePolytope: {
        for (int i = 0; i < 5; ++i) {
            const float unit = static_cast<float>(i) / 4.0f;
            objects.push_back(makeObject3D(Object3DKind::Cage,
                                           Vec3{std::sin(unit * kPi * 2.0f + phase * 0.10f) * wide * 0.28f,
                                                std::cos(unit * kPi * 2.0f + phase * 0.13f) * wide * 0.18f,
                                                minimumDimension * (-0.30f + unit * 0.92f + dropPush * 0.12f)},
                                           Vec3{minimumDimension * (0.085f + unit * 0.032f + dropPush * 0.018f),
                                                minimumDimension * (0.085f + unit * 0.032f),
                                                minimumDimension * (0.12f + unit * 0.040f + dropPush * 0.026f)},
                                           Vec3{phase * 0.16f + unit, phase * 0.24f, phase * 0.12f + metrics.beatPhase},
                                           withAlpha(colors[i % 4], 0.30f + dropPush * 0.18f),
                                           0.34f + dropPush * 0.46f + harmonicPush * 0.14f));
        }
        break;
    }
    case VisualMode::PhaseWeave: {
        for (int i = 0; i < 6; ++i) {
            const float unit = static_cast<float>(i) / 5.0f;
            const float lane = unit - 0.5f;
            objects.push_back(makeObject3D(Object3DKind::Ribbon,
                                           Vec3{lane * wide * 0.86f,
                                                std::sin(phase * 0.16f + unit * kPi) * wide * 0.15f,
                                                std::cos(unit * kPi + phase * 0.08f) * minimumDimension * 0.28f},
                                           Vec3{minimumDimension * (0.34f + metrics.stereoWidth * response * 0.12f),
                                                minimumDimension * 0.22f,
                                                minimumDimension * (0.16f + metrics.spectralFlux * response * 0.05f)},
                                           Vec3{phase * 0.08f, phase * (0.18f + metrics.stereoWidth * 0.06f), unit * kPi},
                                           withAlpha(colors[(i + 1) % 4], 0.28f + metrics.stereoWidth * 0.26f),
                                           0.30f + metrics.stereoWidth * response * 0.52f + metrics.spectralFlux * 0.20f));
        }
        break;
    }
    case VisualMode::ResonanceTessellation: {
        for (int i = 0; i < 7; ++i) {
            const float unit = static_cast<float>(i) / 6.0f;
            const float lane = unit - 0.5f;
            objects.push_back(makeObject3D(Object3DKind::DepthPlane,
                                           Vec3{lane * wide * 0.70f,
                                                std::sin(unit * kPi * 2.0f) * wide * 0.10f,
                                                minimumDimension * (-0.16f + unit * 0.50f + harmonicPush * 0.06f)},
                                           Vec3{minimumDimension * (0.18f + metrics.buildTension * 0.06f),
                                                minimumDimension * (0.14f + metrics.harmonicEnergy * 0.05f),
                                                minimumDimension * 0.025f},
                                           Vec3{0.54f + metrics.phrasePhase * 0.20f, lane * 0.24f, phase * 0.05f},
                                           withAlpha(colors[i % 4], 0.24f + metrics.harmonicEnergy * 0.22f),
                                           0.30f + harmonicPush * 0.46f));
        }
        break;
    }
    case VisualMode::NeuralConstellation: {
        std::vector<Vec3> anchors;
        anchors.reserve(6);
        for (int i = 0; i < 6; ++i) {
            const float unit = static_cast<float>(i) / 6.0f;
            const float angle = unit * 2.0f * kPi + metrics.barPhase * kPi;
            Vec3 position{std::cos(angle) * wide * 0.38f,
                          std::sin(angle) * wide * 0.24f,
                          minimumDimension * (-0.12f + unit * 0.52f + metrics.downbeatConfidence * response * 0.10f)};
            anchors.push_back(position);
            objects.push_back(makeObject3D(Object3DKind::Anchor,
                                           position,
                                           Vec3{minimumDimension * (0.018f + metrics.downbeatConfidence * response * 0.010f),
                                                minimumDimension * 0.018f,
                                                minimumDimension * 0.018f},
                                           Vec3{0.0f, angle, phase * 0.18f + metrics.beatPhase * kPi},
                                           withAlpha(colors[i % 4], 0.32f + metrics.barConfidence * 0.24f),
                                           0.34f + metrics.downbeatConfidence * response * 0.58f));
        }
        for (std::size_t i = 0; i < anchors.size(); ++i) {
            Object3D link = makeObject3D(Object3DKind::Link,
                                         anchors[i],
                                         Vec3{1.0f, 1.0f, 1.0f},
                                         Vec3{},
                                         withAlpha(colors[(i + 1U) % 4U], 0.18f + metrics.barConfidence * 0.16f),
                                         0.20f + metrics.barConfidence * response * 0.32f);
            link.target = anchors[(i + 2U) % anchors.size()];
            objects.push_back(link);
        }
        break;
    }
    case VisualMode::CymaticInterference: {
        for (int i = 0; i < 5; ++i) {
            const float unit = static_cast<float>(i) / 4.0f;
            objects.push_back(makeObject3D(Object3DKind::WaveSurface,
                                           Vec3{0.0f,
                                                (unit - 0.5f) * wide * 0.25f,
                                                minimumDimension * (-0.16f + unit * 0.34f + metrics.buildTension * response * 0.08f)},
                                           Vec3{minimumDimension * (0.34f + harmonicPush * 0.05f),
                                                minimumDimension * (0.12f + metrics.buildTension * 0.04f),
                                                minimumDimension * 0.050f},
                                           Vec3{0.50f + metrics.phrasePhase * 0.22f, 0.12f * unit, phase * 0.06f + chromaAt(metrics, i) * kPi},
                                           withAlpha(colors[(i + 2) % 4], 0.26f + metrics.harmonicEnergy * 0.24f),
                                           0.34f + harmonicPush * 0.58f + metrics.buildTension * 0.18f));
        }
        break;
    }
    }
}

void addSceneHeroAnchors3D(std::vector<Object3D>& objects,
                           VisualMode mode,
                           const AudioMetrics& metrics,
                           const std::array<ColorRGBA, 5>& colors,
                           float minimumDimension,
                           float density,
                           float personality,
                           float response,
                           double time)
{
    const float phase = static_cast<float>(time);
    const float beat = metrics.beat ? metrics.beatConfidence : 0.0f;
    const float bass = metrics.bass * response;
    const float transient = std::clamp(metrics.spectralFlux + metrics.onset * 0.6f, 0.0f, 1.0f);
    const float harmonic = metrics.harmonicEnergy * std::max(metrics.keyConfidence, 0.18f);
    const float glow = 0.34f + personality * 0.22f + response * 0.10f;
    const float wide = minimumDimension * (0.52f + personality * 0.22f + density * 0.04f);
    const auto pushLink = [&](Vec3 from, Vec3 to, ColorRGBA color, float linkGlow) {
        Object3D link = makeObject3D(Object3DKind::Link,
                                     from,
                                     Vec3{1.0f, 1.0f, 1.0f},
                                     Vec3{},
                                     color,
                                     linkGlow);
        link.target = to;
        objects.push_back(link);
    };

    switch (mode) {
    case VisualMode::QuantumTunnel: {
        for (int i = 0; i < 7; ++i) {
            const float unit = static_cast<float>(i) / 6.0f;
            const float radius = minimumDimension * (0.18f + unit * 0.18f + bass * 0.05f);
            objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                           Vec3{0.0f,
                                                std::sin(phase * 0.08f + unit * kPi) * minimumDimension * 0.04f,
                                                minimumDimension * (-0.54f + unit * (1.72f + metrics.dropIntensity * 0.35f))},
                                           Vec3{radius,
                                                radius * (0.48f + metrics.dropIntensity * 0.10f),
                                                1.15f + unit * 0.55f},
                                           Vec3{phase * 0.025f, bass * 0.08f, phase * (0.10f + unit * 0.04f)},
                                           withAlpha(colors[(i + 1) % 4], 0.34f + unit * 0.08f),
                                           glow + bass * 0.42f + metrics.dropIntensity * 0.52f));
        }
        for (int side = -1; side <= 1; side += 2) {
            objects.push_back(makeObject3D(Object3DKind::Polyhedron,
                                           Vec3{side * wide * (0.34f + bass * 0.06f),
                                                minimumDimension * 0.10f,
                                                minimumDimension * (0.02f - metrics.dropIntensity * 0.16f)},
                                           Vec3{minimumDimension * (0.052f + bass * 0.018f),
                                                minimumDimension * (0.24f + bass * 0.10f),
                                                minimumDimension * (0.08f + metrics.dropIntensity * 0.06f)},
                                           Vec3{phase * 0.04f, side * 0.22f, phase * 0.08f},
                                           withAlpha(colors[1], 0.30f + bass * 0.20f),
                                           glow + bass * 0.50f));
        }
        break;
    }
    case VisualMode::TechnoMandala:
    case VisualMode::PolyrhythmLattice: {
        const int lanes = mode == VisualMode::TechnoMandala ? 4 : 5;
        std::vector<Vec3> nearPosts;
        std::vector<Vec3> farPosts;
        nearPosts.reserve(static_cast<std::size_t>(lanes * 2));
        farPosts.reserve(static_cast<std::size_t>(lanes * 2));
        for (int i = 0; i < lanes; ++i) {
            const float lane = static_cast<float>(i) - static_cast<float>(lanes - 1) * 0.5f;
            for (int side = -1; side <= 1; side += 2) {
                Vec3 near{lane * wide * 0.16f,
                          side * minimumDimension * 0.19f,
                          minimumDimension * (-0.16f + beat * 0.06f)};
                Vec3 far{lane * wide * 0.16f,
                         side * minimumDimension * 0.19f,
                         minimumDimension * (0.54f + bass * 0.12f)};
                nearPosts.push_back(near);
                farPosts.push_back(far);
                objects.push_back(makeObject3D(Object3DKind::Column,
                                               near,
                                               Vec3{minimumDimension * 0.014f,
                                                    minimumDimension * (0.24f + beat * 0.10f + bass * 0.08f),
                                                    minimumDimension * 0.016f},
                                               Vec3{0.0f, side * 0.12f, phase * 0.035f + lane * 0.06f},
                                               withAlpha(colors[(i + side + 6) % 4], 0.30f + beat * 0.18f),
                                               glow + beat * 0.36f + bass * 0.28f));
                pushLink(near,
                         far,
                         withAlpha(colors[(i + 2) % 4], 0.18f + beat * 0.16f),
                         glow * 0.55f + bass * 0.22f);
            }
        }
        for (std::size_t i = 2; i < nearPosts.size(); i += 2) {
            pushLink(nearPosts[i - 2U], nearPosts[i], withAlpha(colors[0], 0.16f + beat * 0.12f), glow * 0.45f);
            pushLink(farPosts[i - 1U], farPosts[i + 1U < farPosts.size() ? i + 1U : i - 1U],
                     withAlpha(colors[2], 0.12f + bass * 0.12f), glow * 0.42f);
        }
        break;
    }
    case VisualMode::LissajousMesh:
    case VisualMode::PhaseWeave: {
        for (int i = 0; i < 5; ++i) {
            const float unit = static_cast<float>(i) / 4.0f;
            const float lane = unit - 0.5f;
            objects.push_back(makeObject3D(Object3DKind::Ribbon,
                                           Vec3{lane * wide * 0.86f,
                                                std::sin(phase * 0.10f + unit * kPi) * minimumDimension * 0.13f,
                                                minimumDimension * (-0.28f + unit * 0.68f + metrics.stereoWidth * 0.16f)},
                                           Vec3{minimumDimension * (0.42f + metrics.stereoWidth * 0.16f),
                                                minimumDimension * (0.24f + harmonic * 0.10f),
                                                minimumDimension * (0.18f + metrics.stereoWidth * 0.08f)},
                                           Vec3{phase * 0.055f + unit,
                                                phase * (0.12f + metrics.stereoWidth * 0.04f),
                                                unit * kPi + metrics.phrasePhase * kPi * 0.4f},
                                           withAlpha(colors[(i + 1) % 4], 0.30f + metrics.stereoWidth * 0.22f),
                                           glow + metrics.stereoWidth * response * 0.38f + harmonic * 0.18f));
        }
        break;
    }
    case VisualMode::FrequencyBloom:
    case VisualMode::ChromaKaleidoscope: {
        const int points = mode == VisualMode::ChromaKaleidoscope ? 9 : 7;
        std::vector<Vec3> anchors;
        anchors.reserve(static_cast<std::size_t>(points));
        for (int i = 0; i < points; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(points);
            const float angle = unit * 2.0f * kPi + metrics.keyConfidence * kPi * 0.28f;
            const float chroma = chromaAt(metrics, i);
            const float radius = wide * (0.20f + chroma * 0.14f + harmonic * 0.08f);
            Vec3 position{std::cos(angle) * radius,
                          std::sin(angle) * radius * 0.66f,
                          minimumDimension * (-0.18f + chroma * 0.56f + harmonic * 0.18f)};
            anchors.push_back(position);
            objects.push_back(makeObject3D(mode == VisualMode::ChromaKaleidoscope ? Object3DKind::Cage
                                                                                   : Object3DKind::WaveSurface,
                                           position,
                                           Vec3{minimumDimension * (0.045f + chroma * 0.030f + harmonic * 0.012f),
                                                minimumDimension * (0.070f + chroma * 0.050f + harmonic * 0.030f),
                                                minimumDimension * (0.045f + chroma * 0.020f)},
                                           Vec3{phase * 0.045f + unit,
                                                angle + phase * 0.035f,
                                                metrics.harmonicEnergy * kPi},
                                           withAlpha(colors[(i + 2) % 4], 0.30f + chroma * 0.34f + harmonic * 0.08f),
                                           glow + chroma * response * 0.56f + harmonic * 0.32f));
        }
        for (std::size_t i = 0; i < anchors.size(); ++i) {
            pushLink(anchors[i],
                     anchors[(i + (mode == VisualMode::ChromaKaleidoscope ? 3U : 2U)) % anchors.size()],
                     withAlpha(colors[(i + 1U) % 4U], 0.12f + harmonic * 0.18f),
                     glow * 0.45f + harmonic * 0.24f);
        }
        break;
    }
    case VisualMode::SpectralOrigami: {
        for (int i = 0; i < 9; ++i) {
            const float unit = static_cast<float>(i) / 8.0f;
            const float zig = (i % 2 == 0 ? -1.0f : 1.0f);
            objects.push_back(makeObject3D(i % 3 == 0 ? Object3DKind::Plate : Object3DKind::Shard,
                                           Vec3{(unit - 0.5f) * wide * 1.04f,
                                                zig * minimumDimension * (0.13f + transient * 0.05f),
                                                minimumDimension * (-0.28f + unit * 0.78f + transient * 0.12f)},
                                           Vec3{minimumDimension * (0.052f + transient * 0.026f),
                                                minimumDimension * (0.20f + metrics.onset * 0.10f),
                                                minimumDimension * (0.032f + transient * 0.018f)},
                                           Vec3{0.60f + unit * 0.34f,
                                                zig * (0.42f + transient * 0.20f),
                                                phase * 0.075f + unit * kPi},
                                           withAlpha(colors[i % 4], 0.32f + transient * 0.22f),
                                           glow + transient * 0.62f + metrics.onset * 0.26f));
        }
        break;
    }
    case VisualMode::FractalCathedral: {
        for (int i = 0; i < 5; ++i) {
            const float unit = static_cast<float>(i) / 4.0f;
            const float lane = unit - 0.5f;
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           Vec3{lane * wide * 0.78f,
                                                minimumDimension * (0.14f + std::abs(lane) * 0.04f),
                                                minimumDimension * (-0.08f + std::abs(lane) * 0.32f)},
                                           Vec3{minimumDimension * (0.026f + metrics.bass * 0.014f),
                                                minimumDimension * (0.34f + metrics.bass * 0.18f + metrics.phraseIntensity * 0.10f),
                                                minimumDimension * (0.040f + metrics.bass * 0.018f)},
                                           Vec3{0.05f + metrics.phrasePhase * 0.08f,
                                                lane * 0.16f,
                                                phase * 0.012f},
                                           withAlpha(mix(colors[3], colors[4], 0.38f), 0.28f + metrics.bass * 0.18f),
                                           glow + metrics.bass * 0.34f));
        }
        objects.push_back(makeObject3D(Object3DKind::Cage,
                                       Vec3{0.0f, -minimumDimension * 0.04f, minimumDimension * 0.30f},
                                       Vec3{minimumDimension * 0.30f,
                                            minimumDimension * 0.22f,
                                            minimumDimension * 0.18f},
                                       Vec3{0.36f + metrics.phrasePhase * 0.16f, phase * 0.018f, 0.0f},
                                       withAlpha(colors[4], 0.22f + metrics.harmonicEnergy * 0.14f),
                                       glow + metrics.harmonicEnergy * 0.22f));
        break;
    }
    case VisualMode::HyperspacePolytope: {
        for (int i = 0; i < 6; ++i) {
            const float unit = static_cast<float>(i) / 5.0f;
            objects.push_back(makeObject3D(Object3DKind::Cage,
                                           Vec3{std::sin(unit * kPi * 2.0f + phase * 0.07f) * wide * 0.30f,
                                                std::cos(unit * kPi * 2.0f + phase * 0.09f) * wide * 0.18f,
                                                minimumDimension * (-0.34f + unit * 0.98f + metrics.dropIntensity * 0.12f)},
                                           Vec3{minimumDimension * (0.090f + unit * 0.034f),
                                                minimumDimension * (0.090f + unit * 0.034f),
                                                minimumDimension * (0.15f + unit * 0.050f + metrics.dropIntensity * 0.030f)},
                                           Vec3{phase * 0.12f + unit,
                                                phase * 0.18f,
                                                phase * 0.10f + metrics.beatPhase},
                                           withAlpha(colors[i % 4], 0.30f + metrics.dropIntensity * 0.18f),
                                           glow + metrics.dropIntensity * 0.50f + harmonic * 0.18f));
        }
        break;
    }
    case VisualMode::ResonanceTessellation:
    case VisualMode::CymaticInterference: {
        for (int i = 0; i < 6; ++i) {
            const float unit = static_cast<float>(i) / 5.0f;
            objects.push_back(makeObject3D(mode == VisualMode::CymaticInterference ? Object3DKind::WaveSurface
                                                                                   : Object3DKind::DepthPlane,
                                           Vec3{(unit - 0.5f) * wide * 0.62f,
                                                std::sin(unit * kPi * 2.0f) * minimumDimension * 0.11f,
                                                minimumDimension * (-0.18f + unit * 0.58f + harmonic * 0.10f)},
                                           Vec3{minimumDimension * (0.22f + harmonic * 0.08f),
                                                minimumDimension * (0.14f + metrics.buildTension * 0.06f),
                                                minimumDimension * 0.040f},
                                           Vec3{0.50f + metrics.phrasePhase * 0.22f,
                                                (unit - 0.5f) * 0.34f,
                                                phase * 0.045f + chromaAt(metrics, i) * kPi},
                                           withAlpha(colors[i % 4], 0.26f + harmonic * 0.24f),
                                           glow + harmonic * 0.46f + metrics.buildTension * 0.24f));
        }
        break;
    }
    case VisualMode::NeuralConstellation: {
        std::vector<Vec3> anchors;
        for (int i = 0; i < 7; ++i) {
            const float unit = static_cast<float>(i) / 7.0f;
            const float angle = unit * 2.0f * kPi + metrics.barPhase * kPi;
            Vec3 position{std::cos(angle) * wide * 0.40f,
                          std::sin(angle) * wide * 0.24f,
                          minimumDimension * (-0.18f + unit * 0.62f + metrics.downbeatConfidence * 0.12f)};
            anchors.push_back(position);
            objects.push_back(makeObject3D(Object3DKind::Anchor,
                                           position,
                                           Vec3{minimumDimension * (0.020f + metrics.downbeatConfidence * response * 0.012f),
                                                minimumDimension * 0.020f,
                                                minimumDimension * 0.020f},
                                           Vec3{0.0f, angle, phase * 0.12f},
                                           withAlpha(colors[i % 4], 0.32f + metrics.barConfidence * 0.24f),
                                           glow + metrics.downbeatConfidence * response * 0.54f));
        }
        for (std::size_t i = 0; i < anchors.size(); ++i) {
            pushLink(anchors[i],
                     anchors[(i + 3U) % anchors.size()],
                     withAlpha(colors[(i + 2U) % 4U], 0.15f + metrics.barConfidence * 0.16f),
                     glow * 0.44f + metrics.barConfidence * 0.24f);
        }
        break;
    }
    }
}

void applyModeComposition3D(std::vector<Object3D>& objects,
                            VisualMode mode,
                            const AudioMetrics& metrics,
                            const VisualSettings& settings,
                            float minimumDimension,
                            double time)
{
    if (objects.empty()) {
        return;
    }

    const float phase = static_cast<float>(time);
    const float identity = 0.70f + scenePersonalityOf(settings) * 0.42f + depth3DOf(settings) * 0.18f;
    const float stereo = metrics.stereoWidth;
    const float bass = metrics.bass * response3DOf(settings);
    const float harmonic = metrics.harmonicEnergy;
    const std::size_t count = objects.size();

    for (std::size_t i = 0; i < count; ++i) {
        Object3D& object = objects[i];
        const float unit = count > 1U ? static_cast<float>(i) / static_cast<float>(count - 1U) : 0.0f;
        const float signedUnit = unit * 2.0f - 1.0f;
        const float angle = unit * 2.0f * kPi + phase * 0.045f;

        switch (mode) {
        case VisualMode::QuantumTunnel:
            object.position.x *= 0.78f;
            object.position.y *= 0.86f;
            object.position.z = object.position.z * (1.18f + bass * 0.16f) - minimumDimension * bass * 0.04f;
            object.scale.x *= 1.08f;
            object.scale.y *= 1.02f;
            object.scale.z *= 1.18f + bass * 0.12f;
            break;
        case VisualMode::TechnoMandala:
            object.position.x = object.position.x * (1.28f + identity * 0.22f) +
                                std::round(signedUnit * 4.0f) * minimumDimension * 0.018f;
            object.position.y = object.position.y * (1.10f + identity * 0.10f);
            object.position.z *= 0.78f + bass * 0.08f;
            object.rotation.z += std::round(unit * 16.0f) * (kPi / 8.0f);
            if (object.kind == Object3DKind::Column) {
                object.scale.y *= 1.42f + bass * 0.28f;
            }
            break;
        case VisualMode::LissajousMesh:
            object.position.x = object.position.x * (1.75f + stereo * 0.42f) +
                                std::sin(angle * 1.7f) * minimumDimension * 0.16f;
            object.position.y *= 0.62f + harmonic * 0.08f;
            object.position.z = object.position.z * 1.04f +
                                std::cos(angle * 1.3f) * minimumDimension * 0.08f;
            if (object.kind == Object3DKind::Ribbon) {
                object.scale.x *= 1.70f;
                object.scale.y *= 0.86f;
            }
            break;
        case VisualMode::FrequencyBloom:
            object.position.x += std::cos(angle) * minimumDimension * (0.16f + metrics.treble * 0.14f);
            object.position.y += std::sin(angle) * minimumDimension * (0.12f + metrics.treble * 0.10f);
            object.position.z *= 0.72f + metrics.treble * 0.10f;
            if (object.kind == Object3DKind::WaveSurface) {
                object.scale.x *= 1.34f;
                object.scale.y *= 1.62f;
            }
            break;
        case VisualMode::FractalCathedral:
            object.position.x *= 1.48f + identity * 0.14f;
            object.position.y -= minimumDimension * (0.08f + metrics.phraseIntensity * 0.08f);
            object.position.z *= 0.82f;
            if (object.kind == Object3DKind::Column) {
                object.scale.y *= 1.70f + metrics.phraseIntensity * 0.24f;
                object.scale.x *= 0.82f;
                object.scale.z *= 1.10f;
            }
            if (object.kind == Object3DKind::Cage) {
                object.scale.x *= 1.36f;
                object.scale.y *= 1.28f;
            }
            break;
        case VisualMode::PolyrhythmLattice: {
            const float cell = minimumDimension * (0.045f + stereo * 0.012f);
            object.position.x = std::round((object.position.x * 1.70f) / cell) * cell;
            object.position.y = std::round((object.position.y * 1.28f) / cell) * cell;
            object.position.z += ((static_cast<int>(i) % 2 == 0) ? 1.0f : -1.0f) * minimumDimension * (0.055f + bass * 0.035f);
            if (object.kind == Object3DKind::Column) {
                object.scale.y *= 1.50f + metrics.beatConfidence * 0.30f;
            }
            break;
        }
        case VisualMode::SpectralOrigami:
            object.position.x = object.position.x * 1.22f + signedUnit * minimumDimension * 0.28f;
            object.position.y = object.position.y * 0.80f - signedUnit * minimumDimension * 0.16f;
            object.position.z = object.position.z * 1.14f + std::abs(signedUnit) * minimumDimension * 0.12f;
            object.rotation.x += 0.32f + signedUnit * 0.18f;
            if (object.kind == Object3DKind::Shard || object.kind == Object3DKind::DepthPlane) {
                object.scale.y *= 1.54f;
            }
            break;
        case VisualMode::ChromaKaleidoscope: {
            const float radiusBoost = 1.34f + harmonic * 0.32f;
            const float x = object.position.x;
            const float y = object.position.y;
            const float spin = 0.18f + harmonic * 0.10f;
            object.position.x = (x * std::cos(spin) - y * std::sin(spin)) * radiusBoost;
            object.position.y = (x * std::sin(spin) + y * std::cos(spin)) * (1.20f + harmonic * 0.22f);
            object.position.z *= 0.96f + harmonic * 0.14f;
            if (object.kind == Object3DKind::Cage) {
                object.scale = scale(object.scale, 1.30f + harmonic * 0.20f);
            }
            break;
        }
        case VisualMode::HyperspacePolytope:
            rotatePlane(object.position.x, object.position.z, 0.22f + signedUnit * 0.10f);
            rotatePlane(object.position.y, object.position.z, 0.14f + metrics.dropIntensity * 0.08f);
            object.position.x *= 1.36f;
            object.position.y *= 1.08f;
            object.position.z *= 1.24f + metrics.dropIntensity * 0.12f;
            if (object.kind == Object3DKind::Cage) {
                object.scale.z *= 1.46f;
            }
            break;
        case VisualMode::PhaseWeave:
            object.position.x = object.position.x * (1.90f + stereo * 0.32f) +
                                std::sin(angle * 2.0f) * minimumDimension * 0.18f;
            object.position.y = object.position.y * 0.66f +
                                std::cos(angle * 1.4f) * minimumDimension * 0.10f;
            object.position.z *= 1.08f + stereo * 0.10f;
            if (object.kind == Object3DKind::Ribbon) {
                object.scale.x *= 1.90f;
                object.scale.z *= 1.18f;
            }
            break;
        case VisualMode::ResonanceTessellation:
            object.position.x = object.position.x * 1.72f + signedUnit * minimumDimension * 0.18f;
            object.position.y += std::sin(unit * kPi * 3.0f + harmonic * kPi) * minimumDimension * 0.12f;
            object.position.z *= 0.88f + metrics.buildTension * 0.10f;
            if (object.kind == Object3DKind::DepthPlane || object.kind == Object3DKind::WaveSurface) {
                object.scale.x *= 1.65f;
                object.scale.y *= 1.16f;
            }
            break;
        case VisualMode::NeuralConstellation:
            object.position.x *= 1.78f + metrics.barConfidence * 0.22f;
            object.position.y *= 1.28f + metrics.downbeatConfidence * 0.10f;
            object.position.z *= 1.08f;
            if (object.kind == Object3DKind::Anchor || object.kind == Object3DKind::Node) {
                object.scale = scale(object.scale, 1.42f + metrics.downbeatConfidence * 0.18f);
            }
            break;
        case VisualMode::CymaticInterference:
            object.position.x *= 1.88f + harmonic * 0.18f;
            object.position.y *= 0.62f + metrics.buildTension * 0.10f;
            object.position.z = object.position.z * 0.82f +
                                std::sin(angle * 2.5f + metrics.phrasePhase * kPi) * minimumDimension * 0.08f;
            if (object.kind == Object3DKind::WaveSurface || object.kind == Object3DKind::Plate) {
                object.scale.x *= 1.86f;
                object.scale.y *= 0.92f;
            }
            break;
        }
    }
}

int choreographyKindIndex(Object3DKind kind)
{
    switch (kind) {
    case Object3DKind::Polyhedron:
        return 0;
    case Object3DKind::Shard:
        return 1;
    case Object3DKind::Ribbon:
        return 2;
    case Object3DKind::Node:
        return 3;
    case Object3DKind::Link:
        return 4;
    case Object3DKind::Plate:
        return 5;
    case Object3DKind::TunnelRib:
        return 6;
    case Object3DKind::Particle:
        return 7;
    case Object3DKind::DepthPlane:
        return 8;
    case Object3DKind::Column:
        return 9;
    case Object3DKind::Cage:
        return 10;
    case Object3DKind::WaveSurface:
        return 11;
    case Object3DKind::Orbiter:
        return 12;
    case Object3DKind::Anchor:
        return 13;
    }
    return 0;
}

Vec3 transformChoreographyPoint(Vec3 point,
                                Object3DKind kind,
                                VisualMode mode,
                                const MusicChoreography& motion,
                                float minimumDimension,
                                float unit,
                                float seed)
{
    const float invMin = minimumDimension > 0.0f ? 1.0f / minimumDimension : 1.0f;
    const float depthUnit = clamp01(point.z * invMin * 0.36f + 0.48f);
    const float angle = std::atan2(point.y, point.x);
    const float radius = std::sqrt(point.x * point.x + point.y * point.y);
    const float rhythmicSign = std::sin((unit + seed) * kPi * 8.0f) >= 0.0f ? 1.0f : -1.0f;

    switch (mode) {
    case VisualMode::QuantumTunnel: {
        const float twist = (motion.bassPressure * 0.10f + motion.dropImpact * 0.12f + motion.breath * 0.035f) *
                            (0.45f + depthUnit * 0.75f);
        const float c = std::cos(twist);
        const float s = std::sin(twist);
        const float x = point.x * c - point.y * s;
        const float y = point.x * s + point.y * c;
        const float compression = std::clamp(1.0f - motion.bassPressure * (0.05f + depthUnit * 0.10f) +
                                             motion.breath * 0.035f,
                                             0.72f,
                                             1.22f);
        point.x = x * compression;
        point.y = y * compression;
        point.z -= minimumDimension * (motion.bassPressure * (0.045f + depthUnit * 0.11f) +
                                       motion.dropImpact * 0.055f);
        break;
    }
    case VisualMode::TechnoMandala: {
        const float lock = motion.snap * (0.06f + std::fmod(unit * 8.0f, 1.0f) * 0.025f);
        const float step = std::round((angle + motion.orbit * 2.0f * kPi) / (kPi / 8.0f)) * (kPi / 8.0f);
        const float targetAngle = angle + (step - angle) * lock + motion.grooveSwing * 0.10f;
        const float pulseRadius = radius * (1.0f + motion.beatPulse * 0.08f * rhythmicSign);
        point.x = std::cos(targetAngle) * pulseRadius;
        point.y = std::sin(targetAngle) * pulseRadius;
        point.z += minimumDimension * (motion.phraseLift * 0.035f + motion.snap * 0.045f * rhythmicSign);
        break;
    }
    case VisualMode::PolyrhythmLattice: {
        const float step = std::round((point.x + minimumDimension) / std::max(1.0f, minimumDimension * 0.055f));
        point.x += rhythmicSign * minimumDimension * motion.grooveSwing * 0.045f;
        point.y += std::sin(step * 0.75f + motion.orbit * 2.0f * kPi) * minimumDimension * motion.beatPulse * 0.024f;
        point.z += rhythmicSign * minimumDimension * (motion.snap * 0.055f + motion.bassPressure * 0.035f);
        break;
    }
    case VisualMode::NeuralConstellation: {
        const float clusters = 3.0f + std::round(motion.melodicOrbit * 4.0f);
        const float clusterAngle = std::round((angle / (2.0f * kPi)) * clusters) / clusters * 2.0f * kPi +
                                   motion.melodicOrbit * 2.0f * kPi;
        const Vec3 attractor{
            std::cos(clusterAngle) * minimumDimension * (0.12f + motion.stereoDrift * 0.035f),
            std::sin(clusterAngle) * minimumDimension * (0.09f + motion.phraseLift * 0.035f),
            point.z + minimumDimension * (motion.phraseLift * 0.08f + motion.beatPulse * 0.025f)
        };
        const float clusterStrength = std::clamp(motion.phraseLift * 0.15f + motion.beatPulse * 0.06f, 0.0f, 0.24f);
        point = add(scale(point, 1.0f - clusterStrength), scale(attractor, clusterStrength));
        break;
    }
    case VisualMode::HyperspacePolytope: {
        rotatePlane(point.x, point.z, motion.fold * (0.10f + unit * 0.08f) + motion.stereoDrift * 0.035f);
        rotatePlane(point.y, point.z, motion.dropImpact * 0.07f + motion.parallax * 0.035f);
        point.z += std::sin(seed * 3.0f + motion.orbit * 2.0f * kPi) * minimumDimension * motion.fold * 0.09f;
        break;
    }
    case VisualMode::CymaticInterference: {
        const float wave = std::sin(radius * invMin * 18.0f + motion.melodicOrbit * 2.0f * kPi + seed * 1.7f);
        point.z += wave * minimumDimension * (motion.shimmer * 0.052f + motion.buildTension * 0.048f);
        point.x *= 1.0f + wave * motion.breath * 0.025f;
        point.y *= 1.0f - wave * motion.breath * 0.020f;
        break;
    }
    case VisualMode::PhaseWeave:
    case VisualMode::LissajousMesh: {
        point.x += std::sin(point.z * invMin * 4.0f + motion.orbit * 2.0f * kPi) *
                   minimumDimension * motion.weave * 0.055f;
        point.y += std::cos(point.x * invMin * 3.0f + motion.melodicOrbit * 2.0f * kPi) *
                   minimumDimension * motion.stereoDrift * 0.045f;
        point.z += std::sin(seed + motion.orbit * 2.0f * kPi) * minimumDimension * motion.phraseLift * 0.05f;
        break;
    }
    case VisualMode::SpectralOrigami:
    case VisualMode::FractalCathedral: {
        rotatePlane(point.y, point.z, motion.fold * 0.06f + motion.phraseLift * 0.035f);
        point.y -= minimumDimension * motion.phraseLift * (kind == Object3DKind::Column ? 0.045f : 0.028f);
        point.z += rhythmicSign * minimumDimension * motion.buildTension * 0.035f;
        break;
    }
    case VisualMode::ChromaKaleidoscope:
    case VisualMode::FrequencyBloom:
    case VisualMode::ResonanceTessellation: {
        const float harmonicAngle = angle + motion.melodicOrbit * 2.0f * kPi * 0.12f + motion.shimmer * rhythmicSign * 0.035f;
        const float spread = radius * (1.0f + motion.trebleSparkle * 0.055f + motion.phraseLift * 0.025f);
        point.x = std::cos(harmonicAngle) * spread + motion.stereoDrift * minimumDimension * 0.035f * depthUnit;
        point.y = std::sin(harmonicAngle) * spread;
        point.z += minimumDimension * (motion.shimmer * 0.035f * rhythmicSign + motion.phraseLift * 0.025f);
        break;
    }
    }

    point.x += motion.stereoDrift * minimumDimension * 0.030f * (depthUnit - 0.5f);
    point.z += minimumDimension * (motion.foreground * (0.5f - depthUnit) * 0.050f -
                                   motion.background * depthUnit * 0.030f);
    return point;
}

void applyMotionStyleDisplacement(Vec3& point,
                                  const MusicChoreography& motion,
                                  float minimumDimension,
                                  float unit,
                                  float seed)
{
    const float phase = motion.orbit * 2.0f * kPi + seed;
    const float rhythm = std::sin(unit * kPi * 10.0f + phase) >= 0.0f ? 1.0f : -1.0f;
    switch (motion.style) {
    case MotionStyle::Smooth:
        point.x += std::sin(phase * 0.34f) * minimumDimension * motion.phraseLift * 0.018f;
        point.y += std::cos(phase * 0.27f) * minimumDimension * motion.breath * 0.020f;
        point.z += std::sin(phase * 0.21f) * minimumDimension * motion.inertia * 0.026f;
        break;
    case MotionStyle::Mechanical: {
        const float cell = std::max(1.0f, minimumDimension * (0.022f + motion.snap * 0.012f));
        point.x = std::round(point.x / cell) * cell + rhythm * minimumDimension * motion.grooveSwing * 0.022f;
        point.y = std::round(point.y / cell) * cell;
        point.z += rhythm * minimumDimension * (motion.snap * 0.070f + motion.beatPulse * 0.032f);
        break;
    }
    case MotionStyle::Liquid:
        rotatePlane(point.x, point.y, std::sin(phase) * (0.035f + motion.weave * 0.040f));
        point.z += std::cos(phase * 0.71f) * minimumDimension * (motion.weave * 0.050f + motion.stereoDrift * 0.018f);
        break;
    case MotionStyle::Hyperspace:
        rotatePlane(point.x, point.z, motion.fold * (0.050f + unit * 0.070f));
        rotatePlane(point.y, point.z, motion.parallax * (0.040f + std::fabs(motion.stereoDrift) * 0.030f));
        point.z += std::sin(phase * 1.3f) * minimumDimension * (motion.fold * 0.080f + motion.dropImpact * 0.035f);
        break;
    case MotionStyle::HeavyBass:
        point.z -= minimumDimension * (motion.bassPressure * (0.075f + unit * 0.055f) + motion.dropImpact * 0.050f);
        point.x *= 1.0f - motion.bassPressure * 0.025f;
        point.y *= 1.0f - motion.bassPressure * 0.020f;
        break;
    case MotionStyle::AmbientDrift:
        point.x += std::sin(phase * 0.19f + unit) * minimumDimension * (motion.stereoDrift * 0.045f + motion.phraseLift * 0.018f);
        point.y += std::cos(phase * 0.23f + seed) * minimumDimension * motion.phraseLift * 0.034f;
        point.z += std::sin(phase * 0.17f + seed) * minimumDimension * motion.inertia * 0.045f;
        break;
    case MotionStyle::Breakbeat:
        point.x += rhythm * minimumDimension * (motion.snap * 0.038f + motion.trebleSparkle * 0.018f);
        point.y += std::sin(phase * 2.0f) * minimumDimension * motion.trebleSparkle * 0.026f;
        point.z += rhythm * minimumDimension * (motion.snap * 0.062f + motion.shimmer * 0.040f);
        break;
    }
}

void applyMusicChoreography3D(std::vector<Object3D>& objects,
                              VisualMode mode,
                              const MusicChoreography& motion,
                              float minimumDimension)
{
    if (objects.empty() || motion.audible <= 0.001f) {
        return;
    }

    const float rotationCap = 0.16f + (1.0f - motion.stability) * 0.18f;
    const float scaleCap = 0.10f + (1.0f - motion.clarity) * 0.12f;
    const std::size_t count = objects.size();
    for (std::size_t i = 0; i < count; ++i) {
        Object3D& object = objects[i];
        const float unit = count > 1U ? static_cast<float>(i) / static_cast<float>(count - 1U) : 0.0f;
        const float seed = unit * 13.371f + static_cast<float>(choreographyKindIndex(object.kind)) * 0.217f;
        const Vec3 before = object.position;
        object.position = transformChoreographyPoint(object.position, object.kind, mode, motion, minimumDimension, unit, seed);
        applyMotionStyleDisplacement(object.position, motion, minimumDimension, unit, seed);
        if (object.kind == Object3DKind::Link) {
            object.target = transformChoreographyPoint(object.target, object.kind, mode, motion, minimumDimension, unit + 0.13f, seed + 0.41f);
            applyMotionStyleDisplacement(object.target, motion, minimumDimension, unit + 0.13f, seed + 0.41f);
        }
        object.velocity = subtract(object.position, before);

        const float rhythmicSign = std::sin(seed * 5.0f + motion.orbit * 2.0f * kPi) >= 0.0f ? 1.0f : -1.0f;
        object.rotation.x += std::clamp(motion.fold * 0.08f + motion.phraseLift * 0.05f, -rotationCap, rotationCap);
        object.rotation.y += std::clamp(motion.stereoDrift * 0.06f + motion.weave * 0.04f, -rotationCap, rotationCap);
        object.rotation.z += std::clamp((motion.snap * 0.10f + motion.grooveSwing * 0.12f) * rhythmicSign,
                                        -rotationCap,
                                        rotationCap);

        const float liftScale = 1.0f + std::clamp(motion.bassPressure * 0.055f +
                                                  motion.beatPulse * 0.040f +
                                                  motion.dropImpact * 0.045f +
                                                  motion.shimmer * 0.018f,
                                                  0.0f,
                                                  scaleCap);
        if (object.kind == Object3DKind::Column || object.kind == Object3DKind::TunnelRib) {
            object.scale.y *= 1.0f + std::clamp(motion.bassPressure * 0.075f + motion.snap * 0.035f, 0.0f, scaleCap);
            object.scale.z *= 1.0f + std::clamp(motion.fold * 0.060f + motion.dropImpact * 0.035f, 0.0f, scaleCap);
        } else if (object.kind == Object3DKind::WaveSurface || object.kind == Object3DKind::DepthPlane) {
            object.scale.x *= 1.0f + std::clamp(motion.phraseLift * 0.050f + motion.shimmer * 0.030f, 0.0f, scaleCap);
            object.scale.y *= 1.0f + std::clamp(motion.buildTension * 0.045f + motion.breath * 0.025f, -scaleCap, scaleCap);
        } else {
            object.scale = scale(object.scale, liftScale);
        }

        switch (motion.style) {
        case MotionStyle::Mechanical:
            object.position.z += rhythmicSign * motion.snap * minimumDimension * 0.070f;
            object.scale.y *= 1.0f + std::clamp(motion.snap * 0.050f, 0.0f, scaleCap);
            object.rotation.z += rhythmicSign * motion.snap * 0.055f;
            object.glow += motion.snap * 0.040f;
            break;
        case MotionStyle::Hyperspace:
            object.position.z += (unit - 0.5f) * motion.fold * minimumDimension * 0.220f;
            object.rotation.x += motion.fold * rhythmicSign * 0.120f;
            object.scale.z *= 1.0f + std::clamp(motion.fold * 0.065f, 0.0f, scaleCap);
            object.glow += motion.fold * 0.120f;
            break;
        case MotionStyle::HeavyBass:
            object.position.y += std::fabs(std::sin(seed * 2.0f + motion.orbit * 2.0f * kPi)) *
                                 motion.bassPressure * minimumDimension * 0.095f;
            object.position.z -= motion.bassPressure * minimumDimension * (0.070f + unit * 0.052f);
            object.scale = scale(object.scale, 1.0f + std::clamp(motion.bassPressure * 0.050f, 0.0f, scaleCap));
            object.glow += motion.bassPressure * 0.080f;
            break;
        case MotionStyle::Breakbeat:
            object.position.x += rhythmicSign * motion.snap * minimumDimension * (0.060f + unit * 0.030f);
            object.position.z += std::sin(seed * 7.0f) * motion.snap * minimumDimension * 0.085f;
            object.rotation.y += rhythmicSign * motion.snap * 0.140f;
            object.glow += motion.snap * 0.20f + motion.trebleSparkle * 0.06f;
            break;
        case MotionStyle::AmbientDrift:
            object.position.x += std::sin(seed + motion.orbit * 2.0f * kPi) * motion.phraseLift * minimumDimension * 0.064f;
            object.position.y += std::cos(seed * 0.7f + motion.orbit * 2.0f * kPi) * motion.phraseLift * minimumDimension * 0.052f;
            object.glow += motion.phraseLift * 0.035f;
            break;
        case MotionStyle::Liquid:
            object.position.y += motion.weave * minimumDimension * 0.072f * std::sin(seed * 1.3f);
            object.rotation.y += motion.weave * 0.090f;
            object.glow += std::fabs(motion.weave) * 0.060f;
            break;
        case MotionStyle::Smooth:
            object.position.z += motion.inertia * minimumDimension * 0.026f * std::cos(seed + motion.orbit * kPi);
            object.glow += motion.inertia * 0.025f;
            break;
        }

        object.glow = std::min(1.8f,
                               object.glow +
                                   motion.trebleSparkle * 0.10f +
                                   motion.beatPulse * 0.08f +
                                   motion.dropImpact * 0.11f +
                                   motion.phraseLift * 0.05f);
    }
}

void addChoreographyDepthObjects3D(std::vector<Object3D>& objects,
                                   VisualMode mode,
                                   const MusicChoreography& motion,
                                   const std::array<ColorRGBA, 5>& colors,
                                   float minimumDimension,
                                   float density,
                                   float response,
                                   double time)
{
    if (motion.audible <= 0.001f) {
        return;
    }

    const float phase = static_cast<float>(time);
    const int count = scaledCount(3 + static_cast<int>(std::round(motion.foreground * 4.0f + motion.shimmer * 2.0f)),
                                  density * 0.52f);
    for (int i = 0; i < count; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, count));
        const float angle = unit * 2.0f * kPi + motion.orbit * 2.0f * kPi + phase * (0.08f + motion.stereoDrift * 0.035f);
        const float lane = static_cast<float>(i % 3) - 1.0f;
        Object3DKind kind = Object3DKind::Orbiter;
        switch (mode) {
        case VisualMode::QuantumTunnel:
            kind = Object3DKind::TunnelRib;
            break;
        case VisualMode::PolyrhythmLattice:
        case VisualMode::TechnoMandala:
            kind = Object3DKind::Column;
            break;
        case VisualMode::HyperspacePolytope:
        case VisualMode::ChromaKaleidoscope:
            kind = Object3DKind::Cage;
            break;
        case VisualMode::NeuralConstellation:
            kind = Object3DKind::Anchor;
            break;
        case VisualMode::CymaticInterference:
        case VisualMode::FrequencyBloom:
        case VisualMode::ResonanceTessellation:
            kind = Object3DKind::WaveSurface;
            break;
        case VisualMode::LissajousMesh:
        case VisualMode::PhaseWeave:
        case VisualMode::SpectralOrigami:
        case VisualMode::FractalCathedral:
            kind = Object3DKind::Ribbon;
            break;
        }

        const float radius = minimumDimension * (0.10f + unit * 0.18f + motion.parallax * 0.06f);
        const float z = minimumDimension * (-0.42f + unit * 1.06f +
                                            motion.bassPressure * 0.16f +
                                            motion.dropImpact * (lane < 0.0f ? -0.08f : 0.14f));
        Object3D object = makeObject3D(kind,
                                       Vec3{std::cos(angle) * radius + lane * motion.stereoDrift * minimumDimension * 0.06f,
                                            std::sin(angle * (1.0f + motion.grooveSwing * 0.18f)) * radius * 0.62f,
                                            z},
                                       Vec3{minimumDimension * (0.012f + motion.foreground * 0.020f + motion.shimmer * 0.010f),
                                            minimumDimension * (0.028f + motion.phraseLift * 0.050f + motion.bassPressure * 0.024f),
                                            minimumDimension * (0.012f + motion.fold * 0.030f)},
                                       Vec3{motion.fold * 0.36f + unit,
                                            angle + motion.stereoDrift * 0.20f,
                                            phase * (0.16f + motion.snap * 0.12f)},
                                       withAlpha(colors[(i + 2) % 4], 0.20f + motion.foreground * 0.22f + motion.phraseLift * 0.12f),
                                       0.18f + motion.trebleSparkle * 0.28f + motion.dropImpact * 0.34f + response * 0.08f);
        objects.push_back(object);
    }
}

SectionNarrative3D buildSectionNarrative3D(const AudioMetrics& metrics,
                                           const MusicChoreography& motion,
                                           const SceneInterpretation& intent)
{
    const float confidence = clamp01(metrics.sectionConfidence);
    const float progress = clamp01(metrics.sectionProgress);
    SectionNarrative3D narrative;
    narrative.build = metrics.section == ArrangementSection::Build
                          ? confidence * (0.28f + progress * 0.62f + metrics.buildTension * 0.34f)
                          : metrics.buildTension * confidence * 0.22f;
    narrative.drop = metrics.section == ArrangementSection::Drop
                         ? confidence * (0.45f + metrics.dropIntensity * 0.46f + (1.0f - progress) * 0.16f)
                         : metrics.dropIntensity *
                               clamp01(0.36f +
                                       metrics.bass * 0.22f +
                                       metrics.onset * 0.12f +
                                       metrics.phraseConfidence * 0.10f +
                                       (metrics.style == AudioStyle::BassHeavy ? metrics.styleConfidence * 0.10f : 0.0f));
    narrative.groove = metrics.section == ArrangementSection::Groove
                           ? confidence * clamp01(metrics.beatConfidence * 0.52f +
                                                  metrics.barConfidence * 0.30f +
                                                  metrics.downbeatConfidence * 0.18f)
                           : metrics.beatConfidence * metrics.barConfidence * 0.12f;
    narrative.breakdown = metrics.section == ArrangementSection::Breakdown
                              ? confidence * clamp01(0.30f +
                                                     intent.spacious * 0.28f +
                                                     metrics.stereoWidth * 0.26f +
                                                     metrics.harmonicEnergy * 0.18f +
                                                     (1.0f - metrics.dropIntensity) * 0.14f)
                              : 0.0f;
    narrative.release = metrics.section == ArrangementSection::Drop
                            ? confidence *
                                  smootherStep(0.42f, 0.92f, progress) *
                                  clamp01(0.38f +
                                          motion.dropImpact * 0.30f +
                                          metrics.phraseConfidence * 0.18f +
                                          metrics.phraseIntensity * 0.12f)
                            : (metrics.phraseBoundary ? metrics.phraseConfidence * 0.22f : 0.0f);
    narrative.build = clamp01(narrative.build);
    narrative.drop = clamp01(narrative.drop);
    narrative.groove = clamp01(narrative.groove);
    narrative.breakdown = clamp01(narrative.breakdown);
    narrative.release = clamp01(narrative.release);
    narrative.intensity = std::max({narrative.build, narrative.drop, narrative.groove, narrative.breakdown, narrative.release});
    return narrative;
}

void addSectionNarrativeObjects3D(std::vector<Object3D>& objects,
                                  VisualMode mode,
                                  const SectionNarrative3D& narrative,
                                  const AudioMetrics& metrics,
                                  const MusicChoreography& motion,
                                  const std::array<ColorRGBA, 5>& colors,
                                  float minimumDimension,
                                  float density,
                                  float response,
                                  double time)
{
    if (narrative.intensity <= 0.025f) {
        return;
    }

    const float phase = static_cast<float>(time);
    const float beatPhase = metrics.beatPhase * 2.0f * kPi;
    const float stereo = 0.72f + metrics.stereoWidth * 0.50f;
    const auto pushLink = [&objects](Vec3 from, Vec3 to, ColorRGBA color, float glow) {
        Object3D link = makeObject3D(Object3DKind::Link,
                                     from,
                                     Vec3{1.0f, 1.0f, 1.0f},
                                     Vec3{},
                                     color,
                                     glow);
        link.target = to;
        objects.push_back(link);
    };

    if (narrative.build > 0.06f) {
        const int spineCount = scaledCount(8 + static_cast<int>(std::round(narrative.build * 8.0f)),
                                           density * (0.56f + narrative.build * 0.36f));
        std::vector<Vec3> spine;
        spine.reserve(static_cast<std::size_t>(spineCount));
        for (int i = 0; i < spineCount; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, spineCount - 1));
            const float angle = unit * 4.0f * kPi + beatPhase + phase * (0.10f + narrative.build * 0.08f);
            const float radius = minimumDimension * (0.07f + unit * (0.19f + narrative.build * 0.08f)) * stereo;
            const Vec3 position{
                std::cos(angle) * radius,
                -minimumDimension * (0.22f - unit * (0.46f + narrative.build * 0.16f)),
                minimumDimension * (-0.34f + unit * (1.10f + narrative.build * 0.28f))
            };
            spine.push_back(position);
            objects.push_back(makeObject3D((mode == VisualMode::TechnoMandala || mode == VisualMode::PolyrhythmLattice)
                                               ? Object3DKind::Column
                                               : Object3DKind::Cage,
                                           position,
                                           Vec3{minimumDimension * (0.012f + narrative.build * 0.018f),
                                                minimumDimension * (0.030f + unit * 0.038f + narrative.build * 0.050f),
                                                minimumDimension * (0.012f + metrics.buildTension * 0.025f)},
                                           Vec3{unit * 0.32f,
                                                angle * 0.14f,
                                                phase * 0.038f + narrative.build},
                                           withAlpha(colors[(i + 2) % 5], 0.17f + narrative.build * 0.28f),
                                           0.22f + narrative.build * 0.58f + motion.shimmer * 0.18f));
        }
        for (std::size_t i = 1; i < spine.size(); ++i) {
            pushLink(spine[i - 1U],
                     spine[i],
                     withAlpha(colors[(i + 1U) % 5U], 0.10f + narrative.build * 0.18f),
                     0.12f + narrative.build * 0.34f);
        }
    }

    if (narrative.drop > 0.05f) {
        const int pressureCount = scaledCount(6 + static_cast<int>(std::round(narrative.drop * 6.0f)),
                                              density * (0.62f + narrative.drop * 0.42f));
        for (int i = 0; i < pressureCount; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, pressureCount - 1));
            const float signedUnit = unit * 2.0f - 1.0f;
            const float z = minimumDimension * (-0.62f + unit * 0.94f - narrative.drop * 0.20f);
            const float radius = minimumDimension * (0.18f + narrative.drop * 0.20f + unit * 0.08f);
            objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                           Vec3{0.0f,
                                                signedUnit * minimumDimension * 0.018f,
                                                z},
                                           Vec3{radius * (1.0f - narrative.release * 0.10f),
                                                radius * (0.50f + metrics.bass * 0.14f),
                                                0.72f + narrative.drop * 0.72f},
                                           Vec3{narrative.drop * 0.18f,
                                                phase * 0.035f,
                                                beatPhase * 0.10f + unit * kPi},
                                           withAlpha(colors[i % 5], 0.18f + narrative.drop * 0.28f),
                                           0.30f + narrative.drop * 0.72f + metrics.bass * response * 0.20f));
            if (i % 2 == 0) {
                objects.push_back(makeObject3D(Object3DKind::Plate,
                                               Vec3{signedUnit * minimumDimension * (0.18f + narrative.drop * 0.10f),
                                                    minimumDimension * (0.05f + narrative.release * 0.05f),
                                                    z + minimumDimension * 0.06f},
                                               Vec3{minimumDimension * (0.030f + narrative.drop * 0.035f),
                                                    minimumDimension * (0.13f + narrative.drop * 0.070f),
                                                    minimumDimension * 0.012f},
                                               Vec3{0.54f + narrative.drop * 0.20f,
                                                    signedUnit * 0.22f,
                                                    beatPhase * 0.16f},
                                               withAlpha(colors[(i + 1) % 5], 0.16f + narrative.drop * 0.26f),
                                               0.22f + narrative.drop * 0.60f));
            }
        }
    }

    if (narrative.groove > 0.06f) {
        const int lanes = scaledCount(4, density * (0.86f + narrative.groove * 0.20f));
        const int steps = scaledCount(8, density * (0.76f + narrative.groove * 0.22f));
        std::vector<Vec3> lastLanePoints(static_cast<std::size_t>(lanes));
        for (int lane = 0; lane < lanes; ++lane) {
            const float laneUnit = lanes > 1 ? static_cast<float>(lane) / static_cast<float>(lanes - 1) : 0.0f;
            const float x = (laneUnit - 0.5f) * minimumDimension * (0.68f + metrics.stereoWidth * 0.18f);
            for (int step = 0; step < steps; ++step) {
                const float stepUnit = static_cast<float>(step) / static_cast<float>(std::max(1, steps - 1));
                const float pulse = std::pow(clamp01(1.0f - std::fabs(stepUnit - metrics.barPhase) * 2.0f), 2.0f);
                const Vec3 position{
                    x,
                    (laneUnit - 0.5f) * minimumDimension * 0.16f,
                    minimumDimension * (-0.30f + stepUnit * 0.86f + pulse * narrative.groove * 0.10f)
                };
                objects.push_back(makeObject3D(Object3DKind::Column,
                                               position,
                                               Vec3{minimumDimension * (0.010f + pulse * 0.008f),
                                                    minimumDimension * (0.038f + narrative.groove * 0.050f + pulse * 0.038f),
                                                    minimumDimension * 0.010f},
                                               Vec3{0.0f,
                                                    laneUnit * 0.16f,
                                                    std::round(metrics.beatPhase * 8.0f) * (kPi / 8.0f)},
                                               withAlpha(colors[(lane + step) % 5], 0.14f + narrative.groove * 0.18f + pulse * 0.16f),
                                               0.16f + narrative.groove * 0.42f + pulse * 0.30f));
                if (step > 0) {
                    pushLink(lastLanePoints[static_cast<std::size_t>(lane)],
                             position,
                             withAlpha(colors[lane % 5], 0.07f + narrative.groove * 0.12f),
                             0.08f + narrative.groove * 0.20f);
                }
                lastLanePoints[static_cast<std::size_t>(lane)] = position;
            }
        }
    }

    if (narrative.breakdown > 0.06f) {
        const int planes = scaledCount(5 + static_cast<int>(std::round(narrative.breakdown * 6.0f)),
                                       density * (0.54f + narrative.breakdown * 0.30f));
        for (int i = 0; i < planes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, planes - 1));
            objects.push_back(makeObject3D(Object3DKind::DepthPlane,
                                           Vec3{0.0f,
                                                std::sin(phase * 0.018f + unit * kPi) * minimumDimension * 0.05f,
                                                minimumDimension * (-0.42f + unit * 1.18f)},
                                           Vec3{minimumDimension * (0.24f + metrics.stereoWidth * 0.16f + unit * 0.08f),
                                                minimumDimension * (0.12f + metrics.harmonicEnergy * 0.08f),
                                                minimumDimension * 0.014f},
                                           Vec3{0.50f + unit * 0.16f,
                                                phase * 0.008f,
                                                phase * 0.012f + narrative.breakdown},
                                           withAlpha(mix(colors[3], colors[0], unit), 0.08f + narrative.breakdown * 0.18f),
                                           0.12f + narrative.breakdown * 0.28f));
        }
        const int orbiters = scaledCount(9, density * (0.46f + narrative.breakdown * 0.34f));
        for (int i = 0; i < orbiters; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, orbiters));
            const float angle = unit * 2.0f * kPi + phase * 0.018f;
            objects.push_back(makeObject3D(Object3DKind::Orbiter,
                                           Vec3{std::cos(angle) * minimumDimension * (0.14f + metrics.stereoWidth * 0.10f),
                                                std::sin(angle * 1.3f) * minimumDimension * 0.10f,
                                                minimumDimension * (0.02f + unit * 0.56f)},
                                           Vec3{minimumDimension * (0.012f + narrative.breakdown * 0.012f),
                                                minimumDimension * (0.012f + narrative.breakdown * 0.012f),
                                                minimumDimension * 0.010f},
                                           Vec3{0.0f, angle, phase * 0.014f},
                                           withAlpha(colors[(i + 4) % 5], 0.18f + narrative.breakdown * 0.18f),
                                           0.14f + narrative.breakdown * 0.30f));
        }
    }

    if (narrative.release > 0.05f) {
        const int arcs = scaledCount(5 + static_cast<int>(std::round(narrative.release * 5.0f)),
                                     density * (0.42f + narrative.release * 0.34f));
        std::vector<Vec3> recoveryAnchors;
        recoveryAnchors.reserve(static_cast<std::size_t>(arcs));
        for (int i = 0; i < arcs; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, arcs - 1));
            const float lane = unit * 2.0f - 1.0f;
            const float angle = lane * kPi * (0.30f + narrative.release * 0.16f) +
                                phase * (0.018f + narrative.release * 0.016f);
            const float expansion = std::sin(unit * kPi) * narrative.release;
            const Vec3 position{
                std::sin(angle) * minimumDimension * (0.20f + expansion * 0.18f) * stereo,
                -minimumDimension * (0.06f + expansion * 0.08f),
                minimumDimension * (-0.08f + unit * (0.76f + narrative.release * 0.22f))
            };
            recoveryAnchors.push_back(position);
            objects.push_back(makeObject3D((i % 2 == 0) ? Object3DKind::WaveSurface : Object3DKind::Ribbon,
                                           position,
                                           Vec3{minimumDimension * (0.10f + narrative.release * 0.10f + expansion * 0.08f),
                                                minimumDimension * (0.030f + narrative.release * 0.050f),
                                                minimumDimension * (0.012f + narrative.release * 0.030f)},
                                           Vec3{0.38f + unit * 0.12f,
                                                angle * 0.18f,
                                                metrics.phrasePhase * kPi + unit * 0.42f},
                                           withAlpha(mix(colors[(i + 2) % 5], colors[4], 0.48f),
                                                     0.12f + narrative.release * 0.22f),
                                           0.14f + narrative.release * 0.38f + motion.phraseLift * 0.12f));
        }
        for (std::size_t i = 1; i < recoveryAnchors.size(); ++i) {
            pushLink(recoveryAnchors[i - 1U],
                     recoveryAnchors[i],
                     withAlpha(colors[(i + 3U) % 5U], 0.06f + narrative.release * 0.12f),
                     0.08f + narrative.release * 0.22f);
        }
    }
}

void addSongArcObjects3D(std::vector<Object3D>& objects,
                         const MusicSceneArc3D& arc,
                         const AudioMetrics& metrics,
                         const std::array<ColorRGBA, 5>& colors,
                         float minimumDimension,
                         float density,
                         float response,
                         double time)
{
    if (arc.intensity <= 0.035f) {
        return;
    }

    const float phase = static_cast<float>(time);
    const float stereoSpread = 0.78f + metrics.stereoWidth * 0.56f;
    const auto pushArcObject = [&](Object3D object) {
        object.musicRole = Object3DRole::Convergence;
        objects.push_back(object);
    };
    const auto pushArcLink = [&](Vec3 from, Vec3 to, ColorRGBA color, float glow) {
        Object3D link = makeObject3D(Object3DKind::Link, from, Vec3{1.0f, 1.0f, 1.0f}, Vec3{}, color, glow);
        link.target = to;
        link.musicRole = Object3DRole::Convergence;
        objects.push_back(link);
    };

    if (arc.anticipation > 0.06f) {
        const int spines = scaledCount(5 + static_cast<int>(std::round(arc.anticipation * 6.0f)),
                                       density * (0.36f + arc.anticipation * 0.28f));
        std::vector<Vec3> anchors;
        anchors.reserve(static_cast<std::size_t>(spines));
        for (int i = 0; i < spines; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, spines - 1));
            const float lift = std::pow(unit, 1.25f) * arc.anticipation;
            const Vec3 position{
                std::sin(unit * 2.4f * kPi + phase * 0.030f) * minimumDimension * (0.08f + lift * 0.12f) * stereoSpread,
                -minimumDimension * (0.20f - unit * 0.36f + arc.anticipation * 0.035f),
                minimumDimension * (-0.42f + unit * (1.08f + arc.anticipation * 0.28f))
            };
            anchors.push_back(position);
            pushArcObject(makeObject3D(i % 2 == 0 ? Object3DKind::Cage : Object3DKind::Column,
                                       position,
                                       Vec3{minimumDimension * (0.016f + arc.anticipation * 0.016f),
                                            minimumDimension * (0.052f + lift * 0.080f),
                                            minimumDimension * (0.014f + arc.anticipation * 0.020f)},
                                       Vec3{unit * 0.20f,
                                            phase * 0.018f + unit,
                                            metrics.phrasePhase * kPi + unit * 0.42f},
                                       withAlpha(mix(colors[0], colors[2], unit), 0.12f + arc.anticipation * 0.22f),
                                       0.14f + arc.anticipation * 0.46f + response * 0.040f));
        }
        for (std::size_t i = 1; i < anchors.size(); ++i) {
            pushArcLink(anchors[i - 1U],
                        anchors[i],
                        withAlpha(colors[(i + 2U) % 5U], 0.06f + arc.anticipation * 0.10f),
                        0.06f + arc.anticipation * 0.18f);
        }
    }

    if (arc.impact > 0.05f) {
        const int shocks = scaledCount(4 + static_cast<int>(std::round(arc.impact * 5.0f)),
                                       density * (0.34f + arc.impact * 0.32f));
        for (int i = 0; i < shocks; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, shocks - 1));
            const float pressure = std::pow(1.0f - unit * 0.58f, 1.2f) * arc.impact;
            const float side = i % 2 == 0 ? -1.0f : 1.0f;
            pushArcObject(makeObject3D(Object3DKind::TunnelRib,
                                       Vec3{0.0f,
                                            minimumDimension * (0.10f + pressure * 0.06f),
                                            minimumDimension * (-0.62f + unit * 0.70f - pressure * 0.18f)},
                                       Vec3{minimumDimension * (0.18f + pressure * 0.18f + unit * 0.05f),
                                            minimumDimension * (0.10f + pressure * 0.10f),
                                            0.60f + pressure * 0.92f},
                                       Vec3{pressure * 0.20f,
                                            phase * 0.028f,
                                            metrics.beatPhase * kPi + unit * 0.40f},
                                       withAlpha(mix(colors[1], colors[2], unit), 0.18f + arc.impact * 0.24f),
                                       0.26f + arc.impact * 0.72f + metrics.bass * response * 0.08f));
            pushArcObject(makeObject3D(Object3DKind::Plate,
                                       Vec3{side * minimumDimension * (0.16f + pressure * 0.14f),
                                            minimumDimension * (0.03f + pressure * 0.07f),
                                            minimumDimension * (-0.38f + unit * 0.54f)},
                                       Vec3{minimumDimension * (0.028f + pressure * 0.040f),
                                            minimumDimension * (0.12f + pressure * 0.090f),
                                            minimumDimension * 0.014f},
                                       Vec3{0.62f,
                                            side * (0.22f + pressure * 0.20f),
                                            metrics.beatPhase * kPi + unit},
                                       withAlpha(colors[(i + 1) % 5], 0.13f + arc.impact * 0.24f),
                                       0.20f + arc.impact * 0.56f));
        }
    }

    if (arc.recovery > 0.05f) {
        const int ribbons = scaledCount(4 + static_cast<int>(std::round(arc.recovery * 6.0f)),
                                        density * (0.30f + arc.recovery * 0.28f));
        std::vector<Vec3> anchors;
        anchors.reserve(static_cast<std::size_t>(ribbons));
        for (int i = 0; i < ribbons; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, ribbons - 1));
            const float angle = unit * 2.0f * kPi + phase * (0.014f + arc.recovery * 0.012f);
            const Vec3 position{
                std::cos(angle) * minimumDimension * (0.16f + arc.recovery * 0.18f) * stereoSpread,
                -minimumDimension * (0.12f + std::sin(unit * kPi) * arc.recovery * 0.12f),
                minimumDimension * (0.02f + unit * (0.72f + arc.recovery * 0.22f))
            };
            anchors.push_back(position);
            pushArcObject(makeObject3D(i % 2 == 0 ? Object3DKind::Ribbon : Object3DKind::WaveSurface,
                                       position,
                                       Vec3{minimumDimension * (0.08f + arc.recovery * 0.10f),
                                            minimumDimension * (0.036f + arc.recovery * 0.040f),
                                            minimumDimension * (0.014f + arc.recovery * 0.018f)},
                                       Vec3{0.36f + unit * 0.14f,
                                            angle * 0.16f,
                                            metrics.phrasePhase * kPi + arc.recovery},
                                       withAlpha(mix(colors[3], colors[4], unit), 0.10f + arc.recovery * 0.20f),
                                       0.12f + arc.recovery * 0.36f));
        }
        for (std::size_t i = 1; i < anchors.size(); ++i) {
            pushArcLink(anchors[i - 1U],
                        anchors[i],
                        withAlpha(colors[(i + 3U) % 5U], 0.05f + arc.recovery * 0.10f),
                        0.05f + arc.recovery * 0.16f);
        }
    }
}

void applySongArcComposition3D(std::vector<Object3D>& objects,
                               const MusicSceneArc3D& arc,
                               const AudioMetrics& metrics,
                               const VisualSettings& settings,
                               float minimumDimension)
{
    if (objects.empty() || arc.intensity <= 0.025f) {
        return;
    }

    const float clarityDamp = 0.78f + patternClarityOf(settings) * 0.22f;
    const float stabilityDamp = 0.70f + motionStabilityOf(settings) * 0.30f;
    const float spread = (arc.anticipation * 0.10f + arc.recovery * 0.12f - arc.impact * 0.06f) * clarityDamp;
    const float compress = arc.impact * (0.10f + metrics.bass * 0.06f) * stabilityDamp;
    const float dolly = minimumDimension * (arc.anticipation * 0.080f -
                                            arc.impact * 0.115f +
                                            arc.recovery * 0.105f);
    for (std::size_t i = 0; i < objects.size(); ++i) {
        Object3D& object = objects[i];
        const float seed = static_cast<float>(i + 1U) * 0.417f +
                           static_cast<float>(static_cast<int>(object.kind)) * 0.193f;
        const float lane = std::sin(seed) >= 0.0f ? 1.0f : -1.0f;
        const float depthUnit = wrapUnit(std::sin(seed * 1.7f) * 0.5f + 0.5f);
        object.position.x *= 1.0f + spread - compress;
        object.position.y += minimumDimension * (arc.anticipation * 0.030f -
                                                 arc.impact * 0.026f -
                                                 arc.recovery * 0.034f * depthUnit);
        object.position.z += dolly * (0.48f + depthUnit * 0.58f);
        object.rotation.x += lane * arc.anticipation * 0.030f;
        object.rotation.y += lane * arc.recovery * 0.034f;
        object.rotation.z += lane * arc.impact * 0.045f;
        object.glow += arc.impact * 0.12f + arc.anticipation * 0.040f + arc.recovery * 0.035f;
        if (object.kind == Object3DKind::TunnelRib || object.kind == Object3DKind::Column) {
            object.scale.y *= 1.0f + arc.impact * 0.10f + arc.anticipation * 0.035f;
            object.scale.z *= 1.0f + arc.impact * 0.08f;
        } else if (object.kind == Object3DKind::DepthPlane || object.kind == Object3DKind::WaveSurface) {
            object.scale.x *= 1.0f + arc.recovery * 0.10f + arc.anticipation * 0.045f;
        }
        if (object.kind == Object3DKind::Link) {
            object.target.x *= 1.0f + spread - compress;
            object.target.y += minimumDimension * (arc.anticipation * 0.022f - arc.recovery * 0.026f);
            object.target.z += dolly * (0.50f + depthUnit * 0.42f);
        }
    }
}

void addIntentDrivenSceneObjects3D(std::vector<Object3D>& objects,
                                   const SceneInterpretation& intent,
                                   const AudioMetrics& metrics,
                                   const std::array<ColorRGBA, 5>& colors,
                                   float minimumDimension,
                                   float density,
                                   float personality,
                                   float response,
                                   double time)
{
    const float phase = static_cast<float>(time);
    const float beatPhase = metrics.beatPhase * 2.0f * kPi;
    const float stereoSpread = 0.72f + metrics.stereoWidth * 0.55f;
    const float expressiveScale = 0.92f + personality * 0.10f + std::min(response, 1.6f) * 0.035f;
    const float silenceScale = (intent.primary == SceneIntent::Calm ? 0.74f : 1.0f) * expressiveScale;

    if (intent.mass > 0.16f) {
        const int ribs = scaledCount(5 + static_cast<int>(std::round(intent.mass * 5.0f)),
                                     density * (0.72f + intent.mass * 0.34f));
        for (int i = 0; i < ribs; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, ribs - 1));
            const float z = minimumDimension * (-0.48f + unit * (1.28f + intent.drop * 0.42f) -
                                                intent.drop * 0.20f * std::pow(1.0f - unit, 2.0f));
            const float radius = minimumDimension * (0.18f + unit * 0.18f + intent.mass * 0.10f);
            objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                           Vec3{0.0f,
                                                std::sin(phase * 0.18f + unit * kPi) * minimumDimension * 0.035f,
                                                z},
                                           Vec3{radius,
                                                radius * (0.56f + metrics.lowMid * 0.20f),
                                                0.48f + intent.mass * 0.65f},
                                           Vec3{intent.mass * 0.18f,
                                                phase * 0.04f,
                                                beatPhase * 0.08f + unit * kPi * 0.25f},
                                           withAlpha(colors[(i + 1) % 4], 0.26f + intent.mass * 0.24f),
                                           0.32f + intent.mass * 0.88f + intent.drop * 0.44f));
        }

        const int masses = scaledCount(4, density * (0.62f + intent.mass * 0.34f));
        for (int i = 0; i < masses; ++i) {
            const float lane = static_cast<float>(i) - static_cast<float>(masses - 1) * 0.5f;
            const float side = lane >= 0.0f ? 1.0f : -1.0f;
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           Vec3{lane * minimumDimension * 0.10f * stereoSpread,
                                                minimumDimension * (0.16f + std::fabs(lane) * 0.018f),
                                                minimumDimension * (-0.18f + std::fabs(lane) * 0.10f -
                                                                    intent.drop * 0.10f)},
                                           Vec3{minimumDimension * (0.020f + intent.mass * 0.018f),
                                                minimumDimension * (0.24f + intent.mass * 0.16f),
                                                minimumDimension * (0.028f + intent.mass * 0.020f)},
                                           Vec3{0.0f,
                                                side * (0.10f + intent.mass * 0.08f),
                                                phase * 0.035f + side * intent.drop * 0.18f},
                                           withAlpha(colors[1], 0.24f + intent.mass * 0.24f),
                                           0.30f + intent.mass * 0.72f));
        }
    }

    if (intent.architecture > 0.16f) {
        const int lanes = scaledCount(5, density * (0.64f + intent.architecture * 0.28f));
        const float spacing = minimumDimension * (0.070f + metrics.stereoWidth * 0.020f);
        for (int x = -lanes; x <= lanes; x += 2) {
            for (int z = 0; z < lanes; ++z) {
                const float lane = static_cast<float>(x);
                const float depth = static_cast<float>(z) / static_cast<float>(std::max(1, lanes - 1));
                const float stepPulse = std::sin(metrics.barPhase * 2.0f * kPi + lane * 0.32f + depth * kPi);
                objects.push_back(makeObject3D(Object3DKind::Column,
                                               Vec3{lane * spacing,
                                                    minimumDimension * (0.10f + stepPulse * metrics.beatConfidence * 0.018f),
                                                    minimumDimension * (-0.18f + depth * 0.68f)},
                                               Vec3{minimumDimension * 0.010f,
                                                    minimumDimension * (0.055f + intent.architecture * 0.07f +
                                                                        metrics.bass * 0.035f),
                                                    minimumDimension * 0.010f},
                                               Vec3{0.0f, lane * 0.08f, phase * 0.035f},
                                               withAlpha(colors[(x + z + 64) % 4], 0.18f + intent.architecture * 0.22f),
                                               0.18f + intent.architecture * 0.58f + metrics.beatConfidence * 0.20f));
            }
        }
        objects.push_back(makeObject3D(Object3DKind::Cage,
                                       Vec3{0.0f, 0.0f, minimumDimension * (0.16f + intent.tension * 0.16f)},
                                       Vec3{minimumDimension * (0.18f + intent.architecture * 0.08f),
                                            minimumDimension * (0.12f + intent.architecture * 0.06f),
                                            minimumDimension * (0.16f + intent.depthReveal * 0.12f)},
                                       Vec3{phase * 0.06f,
                                            intent.architecture * 0.32f,
                                            beatPhase * 0.10f},
                                       withAlpha(colors[0], 0.24f + intent.architecture * 0.24f),
                                       0.28f + intent.architecture * 0.66f));
    }

    if (intent.orbital > 0.14f || intent.spacious > 0.20f || intent.calm > 0.62f) {
        const int orbiters = scaledCount(7 + static_cast<int>(std::round(intent.spacious * 5.0f)),
                                         density * (0.54f + intent.orbital * 0.30f) * silenceScale);
        for (int i = 0; i < orbiters; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, orbiters));
            const float layer = static_cast<float>(i % 4) / 3.0f;
            const float angle = unit * 2.0f * kPi + phase * (0.035f + intent.calm * 0.015f) +
                                metrics.phrasePhase * kPi * 0.35f;
            const float radius = minimumDimension * (0.12f + layer * 0.12f + metrics.stereoWidth * 0.13f);
            objects.push_back(makeObject3D(Object3DKind::Orbiter,
                                           Vec3{std::cos(angle) * radius * stereoSpread,
                                                std::sin(angle * 0.82f) * radius * 0.56f,
                                                minimumDimension * (-0.16f + layer * 0.34f + intent.spacious * 0.20f)},
                                           Vec3{minimumDimension * (0.010f + intent.orbital * 0.010f),
                                                minimumDimension * (0.010f + intent.orbital * 0.010f),
                                                minimumDimension * 0.010f},
                                           Vec3{0.0f, angle, phase * 0.06f},
                                           withAlpha(colors[(i + 3) % 4], 0.22f + intent.spacious * 0.22f),
                                           0.20f + intent.orbital * 0.42f + intent.spacious * 0.24f));
        }
        const int planes = scaledCount(3, density * (0.52f + intent.spacious * 0.30f) * silenceScale);
        for (int i = 0; i < planes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, planes - 1));
            objects.push_back(makeObject3D(Object3DKind::DepthPlane,
                                           Vec3{0.0f,
                                                (unit - 0.5f) * minimumDimension * 0.12f,
                                                minimumDimension * (-0.28f + unit * 0.72f)},
                                           Vec3{minimumDimension * (0.18f + intent.spacious * 0.10f),
                                                minimumDimension * (0.10f + intent.spacious * 0.08f),
                                                minimumDimension * 0.016f},
                                           Vec3{0.42f + unit * 0.18f,
                                                phase * 0.018f,
                                                phase * 0.022f + unit},
                                           withAlpha(colors[(i + 2) % 4], 0.12f + intent.spacious * 0.18f),
                                           0.14f + intent.spacious * 0.32f));
        }
    }

    if (intent.crystal > 0.18f) {
        const int nodes = scaledCount(10 + static_cast<int>(std::round(intent.crystal * 8.0f)),
                                      density * (0.58f + intent.crystal * 0.32f));
        const std::size_t firstNode = objects.size();
        for (int i = 0; i < nodes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(nodes);
            const float chroma = chromaAt(metrics, i);
            const float angle = unit * 2.0f * kPi + metrics.phrasePhase * kPi * 0.5f;
            const float radius = minimumDimension * (0.12f + chroma * 0.22f + intent.bright * 0.08f);
            const Object3DKind kind = (i % 3 == 0 || intent.bright > 0.52f) ? Object3DKind::Shard : Object3DKind::Node;
            objects.push_back(makeObject3D(kind,
                                           Vec3{std::cos(angle) * radius * stereoSpread,
                                                std::sin(angle * 1.32f) * radius * 0.64f,
                                                minimumDimension * (-0.12f + chroma * 0.56f + intent.melodic * 0.10f)},
                                           Vec3{minimumDimension * (0.012f + chroma * 0.030f + intent.crystal * 0.010f),
                                                minimumDimension * (0.018f + intent.bright * 0.034f),
                                                minimumDimension * (0.010f + chroma * 0.016f)},
                                           Vec3{angle,
                                                phase * 0.10f + chroma * kPi,
                                                metrics.harmonicEnergy * kPi},
                                           withAlpha(colors[(i + 2) % 4], 0.24f + intent.crystal * 0.30f),
                                           0.24f + intent.crystal * 0.66f + chroma * 0.22f));
        }
        const int links = std::min(nodes, scaledCount(8, density * (0.55f + intent.melodic * 0.30f)));
        for (int i = 0; i < links; ++i) {
            const Object3D& a = objects[firstNode + static_cast<std::size_t>(i)];
            const Object3D& b = objects[firstNode + static_cast<std::size_t>((i * 5 + 3) % nodes)];
            Object3D link = makeObject3D(Object3DKind::Link,
                                         a.position,
                                         Vec3{1.0f, 1.0f, 1.0f},
                                         Vec3{},
                                         withAlpha(colors[(i + 1) % 4], 0.14f + intent.melodic * 0.18f),
                                         0.14f + intent.crystal * 0.34f);
            link.target = b.position;
            objects.push_back(link);
        }
    }

    if (intent.fracture > 0.18f) {
        const int shards = scaledCount(8 + static_cast<int>(std::round(intent.fracture * 9.0f)),
                                       density * (0.58f + intent.fracture * 0.30f));
        for (int i = 0; i < shards; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(shards);
            const float angle = unit * 2.0f * kPi + std::floor(metrics.beatPhase * 8.0f) * (kPi / 4.0f);
            const float stagger = static_cast<float>((i * 7) % 11) / 10.0f;
            const Object3DKind kind = i % 4 == 0 ? Object3DKind::Plate : Object3DKind::Shard;
            objects.push_back(makeObject3D(kind,
                                           Vec3{std::cos(angle) * minimumDimension * (0.10f + stagger * 0.30f),
                                                std::sin(angle * 0.77f) * minimumDimension * (0.08f + stagger * 0.18f),
                                                minimumDimension * (-0.24f + stagger * 0.70f + intent.drop * 0.10f)},
                                           Vec3{minimumDimension * (0.016f + intent.fracture * 0.030f),
                                                minimumDimension * (0.034f + metrics.onset * 0.060f),
                                                minimumDimension * (0.010f + intent.fracture * 0.020f)},
                                           Vec3{angle * 0.25f,
                                                phase * 0.20f + stagger,
                                                beatPhase * 0.18f + stagger * kPi},
                                           withAlpha(colors[(i + 3) % 4], 0.22f + intent.fracture * 0.30f),
                                           0.24f + intent.fracture * 0.70f + metrics.onset * 0.22f));
        }
    }

    if (intent.shadow > 0.16f) {
        const int monoliths = scaledCount(3 + static_cast<int>(std::round(intent.shadow * 5.0f)),
                                          density * (0.48f + intent.shadow * 0.28f));
        for (int i = 0; i < monoliths; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, monoliths - 1));
            const float x = (unit - 0.5f) * minimumDimension * (0.46f + metrics.stereoWidth * 0.20f);
            objects.push_back(makeObject3D(Object3DKind::Column,
                                           Vec3{x,
                                                minimumDimension * (0.12f + intent.dark * 0.04f),
                                                minimumDimension * (-0.10f + unit * 0.34f)},
                                           Vec3{minimumDimension * (0.018f + intent.shadow * 0.018f),
                                                minimumDimension * (0.24f + intent.shadow * 0.20f),
                                                minimumDimension * (0.026f + intent.minimal * 0.025f)},
                                           Vec3{0.04f + intent.tension * 0.10f,
                                                (unit - 0.5f) * 0.16f,
                                                phase * 0.015f},
                                           withAlpha(mix(colors[3], colors[4], 0.55f), 0.20f + intent.shadow * 0.22f),
                                           0.18f + intent.shadow * 0.42f));
        }
        if (intent.minimal > 0.36f || intent.dark > 0.32f) {
            objects.push_back(makeObject3D(Object3DKind::Anchor,
                                           Vec3{0.0f, 0.0f, minimumDimension * (0.16f + intent.dark * 0.16f)},
                                           Vec3{minimumDimension * (0.018f + intent.minimal * 0.014f),
                                                minimumDimension * (0.018f + intent.minimal * 0.014f),
                                                minimumDimension * 0.018f},
                                           Vec3{0.0f, phase * 0.02f, phase * 0.035f},
                                           withAlpha(colors[4], 0.24f + intent.shadow * 0.24f),
                                           0.18f + intent.shadow * 0.44f));
        }
    }

    if (intent.drop > 0.42f || intent.tension > 0.64f) {
        const float transform = std::max(intent.drop, intent.tension * 0.72f);
        objects.push_back(makeObject3D(Object3DKind::Cage,
                                       Vec3{0.0f, 0.0f, minimumDimension * (0.12f - intent.drop * 0.16f)},
                                       Vec3{minimumDimension * (0.20f + transform * 0.14f),
                                            minimumDimension * (0.16f + transform * 0.10f),
                                            minimumDimension * (0.20f + transform * 0.22f)},
                                       Vec3{phase * 0.08f + intent.tension * 0.24f,
                                            beatPhase * 0.14f,
                                            phase * 0.12f},
                                       withAlpha(colors[0], 0.30f + transform * 0.28f),
                                       0.36f + transform * 0.88f));
    }
}

void softenBackgroundMeshForRoleScene(std::vector<Object3D>& objects,
                                       const MusicRoleScene3D& role,
                                       float minimumDimension)
{
    if (objects.empty()) {
        return;
    }

    const float activeMass = role.bass + role.drums + role.melody + role.harmony +
                             role.space + role.fracture + role.shadow;
    const float focus = clamp01(activeMass / 4.15f);
    const float separationYield = clamp01(role.separation * 0.72f + focus * 0.36f);
    if (separationYield < 0.18f) {
        return;
    }

    const float convergenceRelief = 1.0f - role.convergence * 0.28f;
    const float dim = 1.0f - separationYield * convergenceRelief * 0.56f;
    const float pushBack = minimumDimension * separationYield * convergenceRelief *
                           (0.18f + role.space * 0.14f + role.shadow * 0.07f);
    const float count = static_cast<float>(objects.size());
    for (std::size_t i = 0; i < objects.size(); ++i) {
        Object3D& object = objects[i];
        const float unit = count > 1.0f ? static_cast<float>(i) / (count - 1.0f) : 0.0f;
        object.position.z += pushBack * (0.54f + unit * 0.92f);
        object.position.x *= 1.0f + role.space * 0.055f + role.separation * 0.030f;
        object.position.y *= 1.0f + role.shadow * 0.030f;
        object.color.a *= std::clamp(dim + (object.kind == Object3DKind::DepthPlane ? 0.05f : 0.0f), 0.34f, 0.90f);
        object.glow *= std::clamp(0.54f + role.convergence * 0.18f, 0.46f, 0.86f);
        if (object.kind != Object3DKind::Column && object.kind != Object3DKind::TunnelRib) {
            object.scale = scale(object.scale, 0.82f + role.convergence * 0.08f);
        }
    }
}

void addMusicalRoleConvergenceRig3D(std::vector<Object3D>& objects,
                                    const MusicRoleScene3D& role,
                                    const SceneInterpretation& intent,
                                    const AudioMetrics& metrics,
                                    MotionStyle motionStyle,
                                    const std::array<ColorRGBA, 5>& colors,
                                    float minimumDimension,
                                    float density,
                                    float personality,
                                    float response,
                                    float stability,
                                    float clarity,
                                    double time)
{
    const float phase = static_cast<float>(time);
    const float beatPhase = clamp01(metrics.beatPhase);
    const float barPhase = clamp01(metrics.barPhase);
    const float beat = metrics.beat ? std::max(0.28f, metrics.beatConfidence) : metrics.beatConfidence * 0.42f;
    const float harmonic = clamp01(metrics.harmonicEnergy * 0.62f +
                                   metrics.keyConfidence * 0.28f +
                                   averageChromaEnergy(metrics) * 0.16f);
    const float stereoSpread = 0.74f + metrics.stereoWidth * 0.62f;
    const float personalityScale = 0.94f + personality * 0.12f;
    const float phrasePull = clamp01(metrics.dropIntensity * 0.42f +
                                     metrics.phraseIntensity * metrics.phraseConfidence * 0.22f +
                                     (metrics.phraseBoundary ? metrics.phraseConfidence * 0.22f : 0.0f) +
                                     (metrics.downbeat ? metrics.downbeatConfidence * 0.14f : 0.0f) +
                                     metrics.convergenceRole * 0.28f);
    const float activeRoleMass = role.bass + role.drums + role.melody + role.harmony +
                                 role.space + role.fracture + role.shadow;
    const float activeRoleCount = (role.bass > 0.16f ? 1.0f : 0.0f) +
                                  (role.drums > 0.16f ? 1.0f : 0.0f) +
                                  (role.melody > 0.16f ? 1.0f : 0.0f) +
                                  (role.harmony > 0.16f ? 1.0f : 0.0f) +
                                  (role.space > 0.16f ? 1.0f : 0.0f) +
                                  (role.fracture > 0.16f ? 1.0f : 0.0f) +
                                  (role.shadow > 0.16f ? 1.0f : 0.0f);
    const float roleFocus = clamp01(role.separation * 0.64f +
                                    std::min(activeRoleMass / 3.6f, 1.0f) * 0.28f +
                                    std::min(activeRoleCount / 7.0f, 1.0f) * 0.08f);
    density *= std::clamp(0.90f + clarity * 0.06f - roleFocus * 0.08f - role.convergence * 0.02f,
                          0.78f,
                          0.98f);
    const float earnedConvergence = clamp01(metrics.convergenceRole * 0.52f +
                                            phrasePull * 0.34f +
                                            metrics.dropIntensity * 0.18f +
                                            metrics.buildTension * metrics.phraseConfidence * 0.12f);
    const float authoredConvergence = clamp01(role.convergence * (0.20f + earnedConvergence * 0.68f) +
                                              phrasePull * role.convergence * 0.18f);
    const float convergence = clamp01(authoredConvergence *
                                      (0.48f +
                                       earnedConvergence * 0.34f +
                                       metrics.convergenceRole * 0.20f +
                                       metrics.dropIntensity * 0.10f -
                                       roleFocus * 0.10f));
    const float districtHold = clamp01(roleFocus * (1.0f - convergence * 0.42f));
    const float roleMotionGain = std::clamp(0.54f + (1.0f - stability) * 0.28f + (1.0f - clarity) * 0.18f,
                                            0.50f,
                                            1.0f);
    const float roleScaleGain = std::clamp(0.64f + (1.0f - stability) * 0.16f + (1.0f - clarity) * 0.20f,
                                           0.56f,
                                           1.0f);
    const float roleGlowGain = std::clamp(0.62f + (1.0f - stability) * 0.18f + (1.0f - clarity) * 0.20f,
                                          0.54f,
                                          1.0f);
    const Vec3 convergencePoint{
        0.0f,
        minimumDimension * (-0.02f + role.bass * 0.04f - role.melody * 0.025f),
        minimumDimension * (0.04f - role.bass * 0.18f + role.space * 0.16f)
    };
    std::vector<Vec3> bassAnchors;
    std::vector<Vec3> drumAnchors;
    std::vector<Vec3> melodyAnchors;
    std::vector<Vec3> harmonyAnchors;
    std::vector<Vec3> spaceAnchors;
    std::vector<Vec3> fractureAnchors;
    std::vector<Vec3> shadowAnchors;

    enum class RoleDistrict {
        Bass,
        Drums,
        Melody,
        Harmony,
        Space,
        Fracture,
        Shadow
    };

    const auto objectRoleForLocalDistrict = [](RoleDistrict district) {
        switch (district) {
        case RoleDistrict::Bass:
            return Object3DRole::Bass;
        case RoleDistrict::Drums:
            return Object3DRole::Drums;
        case RoleDistrict::Melody:
            return Object3DRole::Melody;
        case RoleDistrict::Harmony:
            return Object3DRole::Harmony;
        case RoleDistrict::Space:
            return Object3DRole::Space;
        case RoleDistrict::Fracture:
            return Object3DRole::Fracture;
        case RoleDistrict::Shadow:
            return Object3DRole::Shadow;
        }
        return Object3DRole::Unassigned;
    };

    const float stabilityTame = std::clamp(0.78f +
                                               (1.0f - stability) * 0.20f +
                                               (1.0f - clarity) * 0.14f,
                                           0.74f,
                                           1.08f);
    const float districtScale = minimumDimension *
                                (0.46f + role.separation * 0.62f + personality * 0.16f) *
                                (1.0f - convergence * 0.045f) *
                                stabilityTame;
    const auto districtOffset = [&](RoleDistrict district) {
        switch (district) {
        case RoleDistrict::Bass:
            return Vec3{0.00f, 0.31f, -0.78f};
        case RoleDistrict::Drums:
            return Vec3{-0.78f, 0.02f, -0.20f};
        case RoleDistrict::Melody:
            return Vec3{0.76f, -0.36f, 0.20f};
        case RoleDistrict::Harmony:
            return Vec3{0.30f, -0.25f, 0.62f};
        case RoleDistrict::Space:
            return Vec3{-0.12f, -0.03f, 1.00f};
        case RoleDistrict::Fracture:
            return Vec3{0.92f, 0.17f, -0.36f};
        case RoleDistrict::Shadow:
            return Vec3{-0.66f, 0.31f, 0.42f};
        }
        return Vec3{};
    };
    const auto rolePosition = [&](Vec3 position, RoleDistrict district, float merge) {
        const float readableMerge = clamp01(merge *
                                            (0.055f + convergence * 0.16f + earnedConvergence * 0.04f) *
                                            (1.0f - districtHold * 0.82f));
        const Vec3 offset = scale(districtOffset(district), districtScale * (1.0f - readableMerge * 0.18f));
        return mix(add(position, offset), convergencePoint, readableMerge);
    };

    const auto applyMotionDialect = [&](Object3D& object) {
        const float seed = static_cast<float>(objects.size() + 1U) * 0.713f +
                           static_cast<float>(static_cast<int>(object.kind)) * 0.271f;
        const float sign = std::sin(seed * 3.0f) >= 0.0f ? 1.0f : -1.0f;
        switch (motionStyle) {
        case MotionStyle::Smooth:
            object.position = mix(object.position, convergencePoint, convergence * 0.10f);
            object.rotation.x *= 0.72f;
            object.rotation.y *= 0.72f;
            object.rotation.z *= 0.82f;
            object.glow *= 0.90f;
            break;
        case MotionStyle::Mechanical: {
            const float cell = minimumDimension * 0.032f;
            object.position.x = std::round(object.position.x / cell) * cell;
            object.position.y = std::round(object.position.y / cell) * cell;
            object.rotation.z = std::round(object.rotation.z * 8.0f) / 8.0f;
            if (object.kind == Object3DKind::Column) {
                object.scale.y *= 1.14f;
            }
            object.glow *= 1.04f;
            break;
        }
        case MotionStyle::Liquid:
            object.position.x += std::sin(seed + phase * 0.10f) * minimumDimension * 0.035f * role.melody;
            object.position.y += std::cos(seed * 0.7f + phase * 0.08f) * minimumDimension * 0.024f * role.space;
            object.rotation.y += std::sin(seed) * 0.06f;
            break;
        case MotionStyle::Hyperspace:
            rotatePlane(object.position.x, object.position.z, 0.10f + role.space * 0.08f + sign * 0.025f);
            object.position.z *= 1.08f + role.convergence * 0.12f;
            object.scale.z *= 1.16f;
            object.rotation.y += sign * 0.10f;
            break;
        case MotionStyle::HeavyBass:
            object.position.y += minimumDimension * role.bass * 0.035f;
            object.position.z -= minimumDimension * role.bass * (0.045f + convergence * 0.035f);
            object.scale.y *= 1.0f + role.bass * 0.12f;
            object.scale.z *= 1.0f + role.bass * 0.10f;
            object.glow *= 1.0f + role.bass * 0.08f;
            break;
        case MotionStyle::AmbientDrift:
            object.position.x *= 1.0f + role.space * 0.08f;
            object.position.z += minimumDimension * role.space * (0.055f + 0.025f * std::sin(seed + phase * 0.03f));
            object.rotation.x *= 0.62f;
            object.rotation.z *= 0.62f;
            object.glow *= 0.88f + role.space * 0.10f;
            break;
        case MotionStyle::Breakbeat:
            object.position.x += sign * minimumDimension * role.fracture * (0.060f + convergence * 0.030f);
            object.position.z += std::sin(seed * 4.0f) * minimumDimension * role.fracture * 0.075f;
            object.rotation.y += sign * role.fracture * 0.16f;
            object.rotation.z += std::sin(seed * 2.0f) * role.fracture * 0.14f;
            object.glow *= 1.0f + role.fracture * 0.12f;
            break;
        }
    };

    const auto applyRoleDialect = [&](Object3D& object, RoleDistrict district, float rawStrength) {
        const float strength = clamp01(rawStrength);
        if (strength <= 0.001f) {
            return;
        }

        const float seed = static_cast<float>(objects.size() + 1U) * 0.439f +
                           static_cast<float>(static_cast<int>(object.kind)) * 0.173f;
        const float sign = std::sin(seed * 5.0f) >= 0.0f ? 1.0f : -1.0f;
        const float harmonicPhase = (metrics.keyIndex >= 0
                                         ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f
                                         : metrics.spectralCentroid) *
                                        2.0f * kPi +
                                    metrics.phrasePhase * kPi +
                                    phase * 0.028f;
        const Vec3 basePosition = object.position;
        const Vec3 baseRotation = object.rotation;
        const Vec3 baseScale = object.scale;
        const float baseGlow = object.glow;

        switch (district) {
        case RoleDistrict::Bass: {
            const float pressure = clamp01(metrics.bass * 0.64f + metrics.lowMid * 0.22f + metrics.dropIntensity * 0.30f);
            object.position.z -= minimumDimension * strength * (0.045f + pressure * 0.080f + beat * 0.020f);
            object.position.y += minimumDimension * strength * (0.016f + pressure * 0.032f);
            object.position.x *= 1.0f - strength * pressure * 0.055f;
            object.scale.y *= 1.0f + strength * (0.12f + pressure * 0.18f);
            object.scale.z *= 1.0f + strength * (0.14f + metrics.dropIntensity * 0.20f);
            object.rotation.z *= 0.55f;
            object.glow += pressure * strength * 0.16f;
            break;
        }
        case RoleDistrict::Drums: {
            const float cell = std::max(1.0f, minimumDimension * (0.032f + role.drums * 0.010f));
            const float snapPulse = clamp01(beat * 0.78f + metrics.downbeatConfidence * 0.22f);
            object.position.x = std::round(object.position.x / cell) * cell;
            object.position.z = std::round(object.position.z / (cell * 1.35f)) * (cell * 1.35f) +
                                snapPulse * minimumDimension * 0.032f;
            object.scale.y *= 1.0f + snapPulse * strength * 0.30f;
            object.rotation.x *= 0.35f;
            object.rotation.y = std::round(object.rotation.y * 10.0f) / 10.0f;
            object.rotation.z = std::round((object.rotation.z + metrics.beatPhase * kPi) / (kPi / 8.0f)) * (kPi / 8.0f);
            object.glow += snapPulse * strength * 0.18f;
            break;
        }
        case RoleDistrict::Melody: {
            const float chromaLift = clamp01(harmonic * 0.70f + metrics.keyConfidence * 0.24f);
            object.position.y -= minimumDimension * strength * (0.034f + chromaLift * 0.054f);
            object.position.z += std::sin(harmonicPhase + seed) * minimumDimension * strength * (0.030f + chromaLift * 0.040f);
            object.position.x += std::cos(harmonicPhase * 0.7f + seed) * minimumDimension * strength * 0.026f;
            object.rotation.y += std::sin(harmonicPhase + seed * 0.5f) * strength * 0.18f;
            object.rotation.z += std::cos(harmonicPhase * 0.8f) * strength * 0.12f;
            object.scale = scale(object.scale, 1.0f + chromaLift * strength * 0.08f);
            object.glow += chromaLift * strength * 0.14f;
            break;
        }
        case RoleDistrict::Harmony: {
            const float chord = clamp01(harmonic * 0.72f + metrics.phraseConfidence * 0.18f);
            const float mirror = object.position.x >= 0.0f ? 1.0f : -1.0f;
            object.position.x = mirror * std::fabs(object.position.x) * (1.0f + chord * strength * 0.050f);
            object.position.z += minimumDimension * strength * (0.026f + chord * 0.052f);
            object.rotation.x += chord * strength * 0.10f;
            object.rotation.z *= 0.72f;
            object.scale.x *= 1.0f + chord * strength * 0.12f;
            object.scale.z *= 1.0f + chord * strength * 0.10f;
            object.glow += chord * strength * 0.10f;
            break;
        }
        case RoleDistrict::Space: {
            const float width = clamp01(metrics.stereoWidth * 0.74f + role.space * 0.26f);
            object.position.x *= 1.0f + width * strength * 0.12f;
            object.position.z += minimumDimension * strength * (0.110f + width * 0.110f);
            object.position.y += std::sin(phase * 0.018f + seed) * minimumDimension * strength * 0.020f;
            object.rotation.x *= 0.58f;
            object.rotation.z *= 0.64f;
            object.scale.x *= 1.0f + width * strength * 0.18f;
            object.glow += width * strength * 0.07f;
            break;
        }
        case RoleDistrict::Fracture: {
            const float cut = std::floor(clamp01(metrics.beatPhase) * 8.0f);
            const float transient = clamp01(metrics.onset * 0.46f + metrics.spectralFlux * 0.38f + transientEnergy3D(metrics) * 0.34f);
            object.position.x += sign * minimumDimension * strength * (0.052f + transient * 0.060f);
            object.position.z += (((static_cast<int>(cut) + static_cast<int>(objects.size())) % 2) == 0 ? -1.0f : 1.0f) *
                                 minimumDimension * strength * (0.030f + transient * 0.070f);
            object.rotation.y += sign * strength * (0.18f + transient * 0.20f);
            object.rotation.z = std::round((object.rotation.z + cut * kPi / 8.0f) / (kPi / 6.0f)) * (kPi / 6.0f);
            object.scale.y *= 1.0f + transient * strength * 0.22f;
            object.glow += transient * strength * 0.20f;
            break;
        }
        case RoleDistrict::Shadow: {
            const float mass = clamp01(role.shadow * 0.62f + role.bass * 0.18f + (1.0f - metrics.treble) * 0.18f);
            object.position.y += minimumDimension * strength * (0.030f + mass * 0.060f);
            object.position.z += minimumDimension * strength * (0.036f + mass * 0.055f);
            object.rotation.x *= 0.42f;
            object.rotation.y *= 0.48f;
            object.rotation.z *= 0.22f;
            object.scale.y *= 1.0f + mass * strength * 0.22f;
            object.scale.x *= 1.0f - mass * strength * 0.035f;
            object.glow += mass * strength * 0.08f;
            break;
        }
        }

        object.position = mix(basePosition, object.position, roleMotionGain);
        object.rotation = mix(baseRotation, object.rotation, roleMotionGain);
        object.scale = mix(baseScale, object.scale, roleScaleGain);
        object.glow = baseGlow + (object.glow - baseGlow) * roleGlowGain;
    };

    const auto pushRoleObject = [&](Object3D object,
                                    RoleDistrict district,
                                    float strength,
                                    std::vector<Vec3>& anchors,
                                    bool anchor = true) {
        object.scale = scale(object.scale, personalityScale);
        object.musicRole = objectRoleForLocalDistrict(district);
        applyMotionDialect(object);
        applyRoleDialect(object, district, strength);
        const float rolePresence = clamp01(roleFocus * (0.30f + strength * 0.34f) + strength * 0.20f);
        object.color.a = clamp01(object.color.a * (1.04f + rolePresence * 0.26f));
        object.glow *= 1.0f + rolePresence * 0.16f;
        if (object.kind == Object3DKind::DepthPlane ||
            object.kind == Object3DKind::WaveSurface ||
            object.kind == Object3DKind::Plate ||
            object.kind == Object3DKind::TunnelRib) {
            object.scale = scale(object.scale, 1.0f + rolePresence * 0.06f);
        }
        if (anchor) {
            anchors.push_back(object.position);
        }
        objects.push_back(object);
    };
    const auto pushLink = [&](Vec3 from, Vec3 to, ColorRGBA color, float glow, Object3DRole roleTag = Object3DRole::Bridge) {
        Object3D link = makeObject3D(Object3DKind::Link,
                                     from,
                                     Vec3{1.0f, 1.0f, 1.0f},
                                     Vec3{},
                                     color,
                                     glow);
        link.target = to;
        link.musicRole = roleTag;
        objects.push_back(link);
    };
    const auto pushRoleKeystone = [&](RoleDistrict district,
                                      float strength,
                                      Object3DKind kind,
                                      std::vector<Vec3>& anchors,
                                      ColorRGBA color,
                                      Vec3 scaleBias,
                                      float spinBias) {
        if (strength <= 0.10f) {
            return;
        }

        Vec3 position = rolePosition(Vec3{
                                        std::sin(phase * 0.014f + spinBias) * minimumDimension * 0.018f * strength,
                                        std::cos(phase * 0.010f + spinBias) * minimumDimension * 0.014f * strength,
                                        minimumDimension * (0.02f + strength * 0.035f)
                                    },
                                    district,
                                    convergence * 0.10f);
        pushRoleObject(makeObject3D(kind,
                                    position,
                                    multiply(scaleBias,
                                             Vec3{minimumDimension * (0.72f + strength * 0.22f),
                                                  minimumDimension * (0.72f + strength * 0.22f),
                                                  minimumDimension * (0.72f + strength * 0.22f)}),
                                    Vec3{spinBias * 0.11f + phase * 0.006f,
                                         spinBias * 0.07f + metrics.phrasePhase * 0.10f,
                                         spinBias + phase * 0.010f},
                                    withAlpha(color, 0.12f + strength * 0.22f),
                                    0.16f + strength * 0.46f),
                       district,
                       strength,
                       anchors);
    };
    const auto pushRoleFieldSurface = [&](RoleDistrict district,
                                          float strength,
                                          Object3DKind kind,
                                          ColorRGBA color,
                                          Vec3 scaleBias,
                                          Vec3 localOffset,
                                          Vec3 rotationBias,
                                          float mergeScale) {
        if (strength <= 0.14f) {
            return;
        }

        Vec3 position = rolePosition(localOffset, district, convergence * mergeScale);
        Object3D surface = makeObject3D(kind,
                                        position,
                                        multiply(scaleBias,
                                                 Vec3{minimumDimension * (0.72f + strength * 0.34f),
                                                      minimumDimension * (0.72f + strength * 0.34f),
                                                      minimumDimension * (0.72f + strength * 0.34f)}),
                                        add(rotationBias,
                                            Vec3{phase * (0.004f + strength * 0.003f),
                                                 metrics.phrasePhase * 0.12f,
                                                 phase * (0.006f + strength * 0.004f)}),
                                        withAlpha(color, 0.095f + strength * 0.175f),
                                        0.10f + strength * 0.28f);
        pushRoleObject(surface, district, strength, spaceAnchors, false);
    };

    pushRoleKeystone(RoleDistrict::Bass,
                     role.bass,
                     Object3DKind::TunnelRib,
                     bassAnchors,
                     colors[1],
                     Vec3{0.18f, 0.08f, 0.18f},
                     0.1f);
    pushRoleKeystone(RoleDistrict::Drums,
                     role.drums,
                     Object3DKind::Cage,
                     drumAnchors,
                     colors[0],
                     Vec3{0.055f, 0.12f, 0.055f},
                     0.7f);
    pushRoleKeystone(RoleDistrict::Melody,
                     role.melody,
                     Object3DKind::Orbiter,
                     melodyAnchors,
                     colors[2],
                     Vec3{0.040f, 0.040f, 0.040f},
                     1.3f);
    pushRoleKeystone(RoleDistrict::Harmony,
                     role.harmony,
                     Object3DKind::Cage,
                     harmonyAnchors,
                     colors[3],
                     Vec3{0.095f, 0.070f, 0.105f},
                     1.9f);
    pushRoleKeystone(RoleDistrict::Space,
                     role.space,
                     Object3DKind::DepthPlane,
                     spaceAnchors,
                     colors[4],
                     Vec3{0.22f, 0.12f, 0.020f},
                     2.5f);
    pushRoleKeystone(RoleDistrict::Fracture,
                     role.fracture,
                     Object3DKind::Shard,
                     fractureAnchors,
                     colors[2],
                     Vec3{0.050f, 0.13f, 0.035f},
                     3.1f);
    pushRoleKeystone(RoleDistrict::Shadow,
                     role.shadow,
                     Object3DKind::Column,
                     shadowAnchors,
                     colors[4],
                     Vec3{0.040f, 0.34f, 0.052f},
                     3.7f);

    pushRoleFieldSurface(RoleDistrict::Bass,
                         role.bass,
                         Object3DKind::DepthPlane,
                         colors[1],
                         Vec3{0.32f, 0.090f, 0.030f},
                         Vec3{0.0f, minimumDimension * 0.09f, minimumDimension * -0.08f},
                         Vec3{0.58f, 0.0f, 0.0f},
                         0.12f);
    pushRoleFieldSurface(RoleDistrict::Drums,
                         role.drums,
                         Object3DKind::Plate,
                         colors[0],
                         Vec3{0.22f, 0.075f, 0.020f},
                         Vec3{0.0f, minimumDimension * 0.025f, minimumDimension * 0.02f},
                         Vec3{0.18f, 0.0f, 0.0f},
                         0.10f);
    pushRoleFieldSurface(RoleDistrict::Melody,
                         role.melody,
                         Object3DKind::WaveSurface,
                         colors[2],
                         Vec3{0.20f, 0.080f, 0.026f},
                         Vec3{0.0f, minimumDimension * -0.08f, minimumDimension * 0.06f},
                         Vec3{0.36f, 0.18f, 0.52f},
                         0.12f);
    pushRoleFieldSurface(RoleDistrict::Harmony,
                         role.harmony,
                         Object3DKind::DepthPlane,
                         colors[3],
                         Vec3{0.24f, 0.095f, 0.024f},
                         Vec3{0.0f, minimumDimension * -0.045f, minimumDimension * 0.10f},
                         Vec3{0.42f, 0.12f, 0.22f},
                         0.14f);
    pushRoleFieldSurface(RoleDistrict::Space,
                         role.space,
                         Object3DKind::WaveSurface,
                         colors[4],
                         Vec3{0.36f, 0.16f, 0.022f},
                         Vec3{0.0f, 0.0f, minimumDimension * 0.22f},
                         Vec3{0.50f, 0.10f, 0.12f},
                         0.08f);
    pushRoleFieldSurface(RoleDistrict::Fracture,
                         role.fracture,
                         Object3DKind::Plate,
                         colors[2],
                         Vec3{0.16f, 0.13f, 0.018f},
                         Vec3{0.0f, 0.0f, minimumDimension * -0.02f},
                         Vec3{0.74f, 0.36f, 0.96f},
                         0.10f);
    pushRoleFieldSurface(RoleDistrict::Shadow,
                         role.shadow,
                         Object3DKind::DepthPlane,
                         colors[4],
                         Vec3{0.22f, 0.18f, 0.024f},
                         Vec3{0.0f, minimumDimension * 0.08f, minimumDimension * 0.10f},
                         Vec3{0.66f, 0.18f, 0.04f},
                         0.08f);

    if (role.space > 0.08f) {
        const int planes = scaledCount(3 + static_cast<int>(std::round(role.space * 3.0f)),
                                       density * (0.42f + role.space * 0.24f));
        for (int i = 0; i < planes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, planes - 1));
            Vec3 position{
                std::sin(phase * 0.012f + unit * kPi) * minimumDimension * metrics.stereoWidth * 0.08f,
                (unit - 0.5f) * minimumDimension * 0.10f,
                minimumDimension * (0.26f + unit * (0.88f + role.space * 0.24f))
            };
            position = rolePosition(position, RoleDistrict::Space, convergence * 0.10f);
            pushRoleObject(makeObject3D(Object3DKind::DepthPlane,
                                        position,
                                        Vec3{minimumDimension * (0.32f + role.space * 0.18f + unit * 0.06f),
                                             minimumDimension * (0.15f + role.space * 0.10f),
                                             minimumDimension * 0.018f},
                                        Vec3{0.46f + unit * 0.12f,
                                             phase * 0.006f,
                                             phase * 0.010f + unit * 0.6f},
                                        withAlpha(mix(colors[3], colors[4], unit), 0.09f + role.space * 0.18f),
                                        0.10f + role.space * 0.30f),
                           RoleDistrict::Space,
                           role.space,
                           spaceAnchors);
        }
        const int orbiters = scaledCount(5, density * (0.38f + role.space * 0.28f));
        for (int i = 0; i < orbiters; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(orbiters);
            const float angle = unit * 2.0f * kPi + phase * (0.018f + role.space * 0.012f);
            Vec3 position{
                std::cos(angle) * minimumDimension * (0.19f + role.space * 0.12f) * stereoSpread,
                std::sin(angle * 0.73f) * minimumDimension * (0.10f + role.space * 0.06f),
                minimumDimension * (0.18f + static_cast<float>(i % 4) * 0.20f)
            };
            position = rolePosition(position, RoleDistrict::Space, convergence * 0.12f);
            pushRoleObject(makeObject3D(Object3DKind::Orbiter,
                                        position,
                                        Vec3{minimumDimension * (0.010f + role.space * 0.010f),
                                             minimumDimension * (0.010f + role.space * 0.010f),
                                             minimumDimension * 0.010f},
                                        Vec3{0.0f, angle, phase * 0.012f},
                                        withAlpha(colors[(i + 2) % 5], 0.16f + role.space * 0.18f),
                                        0.12f + role.space * 0.30f),
                           RoleDistrict::Space,
                           role.space,
                           spaceAnchors);
        }
    }

    if (role.bass > 0.08f) {
        const int waves = scaledCount(4 + static_cast<int>(std::round(role.bass * 5.0f)),
                                      density * (0.54f + role.bass * 0.38f));
        for (int i = 0; i < waves; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, waves - 1));
            const float compression = std::pow(1.0f - unit, 1.65f) * (role.bass * 0.18f + metrics.dropIntensity * 0.20f);
            Vec3 position{
                0.0f,
                minimumDimension * (0.18f + compression * 0.14f),
                minimumDimension * (-0.62f + unit * (1.26f + metrics.dropIntensity * 0.28f) - compression)
            };
            position = rolePosition(position, RoleDistrict::Bass, convergence * (0.22f + role.bass * 0.18f));
            const float radius = minimumDimension * (0.16f + unit * 0.16f + role.bass * 0.11f - compression * 0.035f);
            pushRoleObject(makeObject3D(Object3DKind::TunnelRib,
                                        position,
                                        Vec3{radius,
                                             radius * (0.42f + metrics.lowMid * 0.18f),
                                             0.70f + role.bass * 0.72f + unit * 0.26f},
                                        Vec3{role.bass * 0.12f,
                                             phase * 0.012f,
                                             beatPhase * kPi * 0.16f + unit * 0.52f},
                                        withAlpha(colors[1], 0.24f + role.bass * 0.26f),
                                        0.30f + role.bass * response * 0.72f + metrics.dropIntensity * 0.32f),
                           RoleDistrict::Bass,
                           role.bass,
                           bassAnchors);
        }
        const int pressureShells = scaledCount(2 + static_cast<int>(std::round(role.bass * 3.0f)),
                                               density * (0.34f + role.bass * 0.24f));
        for (int i = 0; i < pressureShells; ++i) {
            const float unit = pressureShells > 1 ? static_cast<float>(i) / static_cast<float>(pressureShells - 1) : 0.0f;
            const float side = i % 2 == 0 ? -1.0f : 1.0f;
            Vec3 position{
                side * minimumDimension * (0.08f + unit * 0.20f + metrics.stereoWidth * 0.04f),
                minimumDimension * (0.16f + role.bass * 0.06f),
                minimumDimension * (-0.54f + unit * 0.72f - metrics.dropIntensity * 0.10f)
            };
            position = rolePosition(position, RoleDistrict::Bass, convergence * 0.08f);
            pushRoleObject(makeObject3D(Object3DKind::WaveSurface,
                                        position,
                                        Vec3{minimumDimension * (0.24f + role.bass * 0.13f + unit * 0.05f),
                                             minimumDimension * (0.060f + metrics.lowMid * 0.050f),
                                             minimumDimension * (0.018f + metrics.dropIntensity * 0.018f)},
                                        Vec3{0.62f + unit * 0.18f,
                                             side * (0.18f + role.bass * 0.12f),
                                             beatPhase * kPi + unit * 0.42f},
                                        withAlpha(mix(colors[1], colors[4], 0.18f), 0.14f + role.bass * 0.20f),
                                        0.18f + role.bass * 0.46f + metrics.dropIntensity * 0.18f),
                           RoleDistrict::Bass,
                           role.bass,
                           bassAnchors,
                           i == 0 || i + 1 == pressureShells);
        }
        const int weights = scaledCount(3 + static_cast<int>(std::round(role.bass * 3.0f)),
                                        density * (0.42f + role.bass * 0.30f));
        for (int i = 0; i < weights; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, weights - 1));
            const float lane = unit - 0.5f;
            Vec3 position{
                lane * minimumDimension * (0.42f + metrics.stereoWidth * 0.18f),
                minimumDimension * (0.19f + std::fabs(lane) * 0.025f),
                minimumDimension * (-0.34f + unit * 0.28f - metrics.dropIntensity * 0.10f)
            };
            position = rolePosition(position, RoleDistrict::Bass, convergence * 0.20f);
            pushRoleObject(makeObject3D(Object3DKind::Column,
                                        position,
                                        Vec3{minimumDimension * (0.026f + role.bass * 0.018f),
                                             minimumDimension * (0.26f + role.bass * 0.22f + metrics.dropIntensity * 0.06f),
                                             minimumDimension * (0.032f + role.bass * 0.018f)},
                                        Vec3{0.03f,
                                             lane * (0.12f + role.bass * 0.08f),
                                             phase * 0.012f},
                                        withAlpha(mix(colors[1], colors[3], 0.28f), 0.24f + role.bass * 0.26f),
                                        0.26f + role.bass * response * 0.64f),
                           RoleDistrict::Bass,
                           role.bass,
                           bassAnchors);
        }
    }

    if (role.drums > 0.08f) {
        const int lanes = scaledCount(4, density * (0.50f + role.drums * 0.26f));
        const int steps = scaledCount(5 + static_cast<int>(std::round(role.drums * 5.0f)),
                                      density * (0.50f + role.drums * 0.30f));
        const int activeStep = std::clamp(static_cast<int>(std::floor(barPhase * static_cast<float>(steps))), 0, steps - 1);
        for (int lane = 0; lane < lanes; ++lane) {
            const float laneUnit = lanes > 1 ? static_cast<float>(lane) / static_cast<float>(lanes - 1) : 0.0f;
            const float x = (laneUnit - 0.5f) * minimumDimension * 0.58f * stereoSpread;
            Vec3 lastPoint{};
            bool haveLast = false;
            for (int step = 0; step < steps; ++step) {
                const float stepUnit = static_cast<float>(step) / static_cast<float>(std::max(1, steps - 1));
                const float stepPulse = step == activeStep ? beat : beat * 0.20f;
                Vec3 position{
                    x,
                    minimumDimension * (-0.01f + (laneUnit - 0.5f) * 0.08f),
                    minimumDimension * (-0.22f + stepUnit * 0.72f + stepPulse * role.drums * 0.05f)
                };
                position = rolePosition(position, RoleDistrict::Drums, convergence * 0.16f);
                pushRoleObject(makeObject3D(Object3DKind::Column,
                                            position,
                                            Vec3{minimumDimension * (0.010f + stepPulse * 0.008f),
                                                 minimumDimension * (0.052f + role.drums * 0.060f + stepPulse * 0.055f),
                                                 minimumDimension * 0.010f},
                                            Vec3{0.0f,
                                                 laneUnit * 0.14f,
                                                 std::round(beatPhase * 8.0f) * (kPi / 8.0f)},
                                            withAlpha(colors[(lane + step) % 5], 0.16f + role.drums * 0.20f + stepPulse * 0.18f),
                                            0.14f + role.drums * 0.42f + stepPulse * 0.34f),
                               RoleDistrict::Drums,
                               role.drums,
                               drumAnchors,
                               step == activeStep || step == 0 || step + 1 == steps);
                if (haveLast && (step % 2 == 0 || step == activeStep)) {
                    pushLink(lastPoint,
                             position,
                             withAlpha(colors[lane % 5], 0.08f + role.drums * 0.10f),
                             0.08f + role.drums * 0.22f,
                             Object3DRole::Drums);
                }
                lastPoint = position;
                haveLast = true;
            }
        }
        const int meters = scaledCount(3 + static_cast<int>(std::round(role.drums * 3.0f)),
                                       density * (0.36f + role.drums * 0.22f));
        for (int i = 0; i < meters; ++i) {
            const float unit = meters > 1 ? static_cast<float>(i) / static_cast<float>(meters - 1) : 0.0f;
            const float lane = unit - 0.5f;
            const float downbeatScale = metrics.downbeat ? std::max(0.35f, metrics.downbeatConfidence) : beat * 0.32f;
            Vec3 position{
                lane * minimumDimension * 0.62f * stereoSpread,
                minimumDimension * (-0.030f + lane * 0.055f),
                minimumDimension * (-0.18f + unit * 0.62f + downbeatScale * 0.04f)
            };
            position = rolePosition(position, RoleDistrict::Drums, convergence * 0.08f);
            pushRoleObject(makeObject3D(i % 2 == 0 ? Object3DKind::Plate : Object3DKind::Cage,
                                        position,
                                        Vec3{minimumDimension * (0.035f + role.drums * 0.018f),
                                             minimumDimension * (0.090f + downbeatScale * 0.090f),
                                             minimumDimension * (0.014f + role.drums * 0.012f)},
                                        Vec3{0.10f,
                                             lane * 0.16f,
                                             std::round((barPhase + unit) * 8.0f) * (kPi / 8.0f)},
                                        withAlpha(colors[i % 5], 0.15f + role.drums * 0.21f + downbeatScale * 0.12f),
                                        0.16f + role.drums * 0.40f + downbeatScale * 0.22f),
                           RoleDistrict::Drums,
                           role.drums,
                           drumAnchors,
                           i == 0 || i + 1 == meters);
        }
    }

    if (role.melody > 0.08f) {
        const int notes = scaledCount(9 + static_cast<int>(std::round(role.melody * 7.0f)),
                                      density * (0.56f + role.melody * 0.34f));
        for (int i = 0; i < notes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(notes);
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(i + std::max(0, metrics.keyIndex)));
            const float angle = unit * 2.0f * kPi + metrics.phrasePhase * kPi * 0.50f + harmonic * 0.28f;
            Vec3 position{
                std::cos(angle) * minimumDimension * (0.20f + chroma * 0.18f + role.melody * 0.08f) * stereoSpread,
                -minimumDimension * (0.14f + role.melody * 0.12f + chroma * 0.05f),
                minimumDimension * (0.00f + chroma * 0.56f + role.harmony * 0.12f)
            };
            position = rolePosition(position, RoleDistrict::Melody, convergence * 0.18f);
            const Object3DKind kind = i % 4 == 0 ? Object3DKind::Cage : (i % 2 == 0 ? Object3DKind::Shard : Object3DKind::Node);
            pushRoleObject(makeObject3D(kind,
                                        position,
                                        Vec3{minimumDimension * (0.014f + chroma * 0.030f + role.melody * 0.010f),
                                             minimumDimension * (0.024f + role.melody * 0.040f),
                                             minimumDimension * (0.012f + chroma * 0.016f)},
                                        Vec3{angle,
                                             phase * 0.045f + chroma * kPi,
                                             harmonic * kPi},
                                        withAlpha(colors[(i + 2) % 5], 0.24f + role.melody * 0.30f + chroma * 0.08f),
                                        0.20f + role.melody * 0.58f + chroma * 0.20f),
                           RoleDistrict::Melody,
                           role.melody,
                           melodyAnchors);
        }
        const int braidNodes = scaledCount(3 + static_cast<int>(std::round(role.melody * 4.0f)),
                                           density * (0.36f + role.melody * 0.24f));
        for (int i = 0; i < braidNodes; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, braidNodes - 1));
            const float angle = unit * 3.0f * kPi + harmonic * kPi + metrics.phrasePhase * kPi;
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(i * 2 + std::max(0, metrics.keyIndex)));
            Vec3 position{
                std::cos(angle) * minimumDimension * (0.11f + role.melody * 0.12f + chroma * 0.06f),
                -minimumDimension * (0.22f + role.melody * 0.12f + unit * 0.06f),
                minimumDimension * (0.12f + unit * 0.44f + chroma * 0.16f)
            };
            position = rolePosition(position, RoleDistrict::Melody, convergence * 0.12f);
            pushRoleObject(makeObject3D(Object3DKind::Node,
                                        position,
                                        Vec3{minimumDimension * (0.014f + chroma * 0.018f),
                                             minimumDimension * (0.014f + role.melody * 0.018f),
                                             minimumDimension * (0.014f + chroma * 0.014f)},
                                        Vec3{angle * 0.24f,
                                             harmonic * kPi + unit,
                                             phase * 0.026f + chroma},
                                        withAlpha(colors[(i + 2) % 5], 0.28f + role.melody * 0.30f + chroma * 0.08f),
                                        0.20f + role.melody * 0.50f + chroma * 0.18f),
                           RoleDistrict::Melody,
                           role.melody,
                           melodyAnchors);
        }
        for (std::size_t i = 1; i < melodyAnchors.size(); ++i) {
            pushLink(melodyAnchors[i - 1U],
                     melodyAnchors[i],
                     withAlpha(colors[(i + 2U) % 5U], 0.10f + role.melody * 0.12f),
                     0.10f + role.melody * 0.24f,
                     Object3DRole::Melody);
        }
        const int ribbons = scaledCount(2 + static_cast<int>(std::round(role.melody * 3.0f)),
                                        density * (0.30f + role.melody * 0.24f));
        for (int i = 0; i < ribbons; ++i) {
            const float unit = ribbons > 1 ? static_cast<float>(i) / static_cast<float>(ribbons - 1) : 0.0f;
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(i * 3 + std::max(0, metrics.keyIndex)));
            const float angle = unit * 2.0f * kPi + harmonic * kPi * 0.42f + metrics.phrasePhase * kPi;
            Vec3 position{
                std::cos(angle) * minimumDimension * (0.10f + role.melody * 0.13f + chroma * 0.08f),
                -minimumDimension * (0.20f + role.melody * 0.15f + unit * 0.035f),
                minimumDimension * (0.18f + unit * 0.44f + chroma * 0.18f)
            };
            position = rolePosition(position, RoleDistrict::Melody, convergence * 0.07f);
            pushRoleObject(makeObject3D(Object3DKind::Ribbon,
                                        position,
                                        Vec3{minimumDimension * (0.040f + chroma * 0.030f),
                                             minimumDimension * (0.070f + role.melody * 0.050f),
                                             minimumDimension * (0.040f + harmonic * 0.020f)},
                                        Vec3{angle * 0.22f,
                                             harmonic * kPi * 0.36f + chroma,
                                             phase * 0.016f + metrics.phrasePhase * kPi},
                                        withAlpha(mix(colors[2], colors[3], chroma), 0.19f + role.melody * 0.24f + chroma * 0.08f),
                                        0.18f + role.melody * 0.44f + chroma * 0.16f),
                           RoleDistrict::Melody,
                           role.melody,
                           melodyAnchors,
                           i == 0 || i + 1 == ribbons);
        }
    }

    if (role.harmony > 0.08f) {
        const int symmetry = scaledCount(3 + static_cast<int>(std::round(role.harmony * 4.0f)),
                                         density * (0.40f + role.harmony * 0.26f));
        for (int i = 0; i < symmetry; ++i) {
            const float unit = symmetry > 1 ? static_cast<float>(i) / static_cast<float>(symmetry - 1) : 0.0f;
            Vec3 position{
                (unit - 0.5f) * minimumDimension * (0.34f + role.harmony * 0.12f) * stereoSpread,
                -minimumDimension * (0.035f + role.harmony * 0.055f),
                minimumDimension * (0.08f + unit * 0.42f + harmonic * 0.12f)
            };
            position = rolePosition(position, RoleDistrict::Harmony, convergence * 0.24f);
            pushRoleObject(makeObject3D(Object3DKind::Cage,
                                        position,
                                        Vec3{minimumDimension * (0.060f + role.harmony * 0.050f),
                                             minimumDimension * (0.042f + role.harmony * 0.040f),
                                             minimumDimension * (0.070f + harmonic * 0.040f)},
                                        Vec3{0.28f + metrics.phrasePhase * 0.20f,
                                             phase * 0.014f,
                                             harmonic * kPi + unit},
                                        withAlpha(colors[(i + 3) % 5], 0.18f + role.harmony * 0.24f),
                                        0.18f + role.harmony * 0.42f),
                           RoleDistrict::Harmony,
                           role.harmony,
                           harmonyAnchors);
        }
        const std::size_t linkCount = std::min(melodyAnchors.size(), harmonyAnchors.size());
        for (std::size_t i = 0; i < linkCount; ++i) {
            pushLink(harmonyAnchors[i],
                     melodyAnchors[(i * 3U + 1U) % melodyAnchors.size()],
                     withAlpha(colors[(i + 1U) % 5U], 0.09f + role.harmony * 0.14f),
                     0.08f + role.harmony * 0.22f,
                     Object3DRole::Harmony);
        }
        const int chordSheets = scaledCount(2 + static_cast<int>(std::round(role.harmony * 3.0f)),
                                            density * (0.34f + role.harmony * 0.22f));
        for (int i = 0; i < chordSheets; ++i) {
            const float unit = chordSheets > 1 ? static_cast<float>(i) / static_cast<float>(chordSheets - 1) : 0.0f;
            const float mirror = i % 2 == 0 ? -1.0f : 1.0f;
            Vec3 position{
                mirror * minimumDimension * (0.10f + role.harmony * 0.14f + unit * 0.10f) * stereoSpread,
                -minimumDimension * (0.08f + role.harmony * 0.08f),
                minimumDimension * (0.20f + unit * 0.40f + harmonic * 0.14f)
            };
            position = rolePosition(position, RoleDistrict::Harmony, convergence * 0.10f);
            pushRoleObject(makeObject3D(Object3DKind::WaveSurface,
                                        position,
                                        Vec3{minimumDimension * (0.18f + role.harmony * 0.13f),
                                             minimumDimension * (0.075f + harmonic * 0.040f),
                                             minimumDimension * (0.020f + role.harmony * 0.018f)},
                                        Vec3{0.38f + unit * 0.20f,
                                             mirror * (0.20f + harmonic * 0.10f),
                                             metrics.phrasePhase * kPi + unit * 0.50f},
                                        withAlpha(colors[(i + 3) % 5], 0.13f + role.harmony * 0.20f),
                                        0.14f + role.harmony * 0.34f + harmonic * 0.14f),
                           RoleDistrict::Harmony,
                           role.harmony,
                           harmonyAnchors,
                           i == 0 || i + 1 == chordSheets);
        }
    }

    if (role.fracture > 0.08f) {
        const int cuts = scaledCount(6 + static_cast<int>(std::round(role.fracture * 7.0f)),
                                     density * (0.48f + role.fracture * 0.34f));
        const float cutPhase = std::floor(beatPhase * 8.0f) * (kPi / 4.0f);
        for (int i = 0; i < cuts; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(cuts);
            const float stagger = static_cast<float>((i * 7) % 11) / 10.0f;
            const float side = i % 2 == 0 ? -1.0f : 1.0f;
            Vec3 position{
                side * minimumDimension * (0.16f + stagger * 0.28f),
                (stagger - 0.5f) * minimumDimension * (0.16f + role.fracture * 0.08f),
                minimumDimension * (-0.26f + unit * 0.76f + role.fracture * 0.12f)
            };
            position = rolePosition(position, RoleDistrict::Fracture, convergence * 0.20f);
            pushRoleObject(makeObject3D(i % 3 == 0 ? Object3DKind::Plate : Object3DKind::Shard,
                                        position,
                                        Vec3{minimumDimension * (0.020f + role.fracture * 0.030f),
                                             minimumDimension * (0.060f + metrics.onset * 0.075f),
                                             minimumDimension * (0.012f + role.fracture * 0.020f)},
                                        Vec3{0.42f + unit * 0.28f,
                                             side * (0.34f + role.fracture * 0.18f),
                                             cutPhase + stagger * kPi},
                                        withAlpha(colors[(i + 3) % 5], 0.22f + role.fracture * 0.30f),
                                        0.22f + role.fracture * 0.62f + metrics.onset * 0.20f),
                           RoleDistrict::Fracture,
                           role.fracture,
                           fractureAnchors);
        }
    }

    if (role.shadow > 0.08f) {
        const int monoliths = scaledCount(6 + static_cast<int>(std::round(role.shadow * 6.0f)),
                                          density * (0.56f + role.shadow * 0.28f));
        for (int i = 0; i < monoliths; ++i) {
            const float unit = monoliths > 1 ? static_cast<float>(i) / static_cast<float>(monoliths - 1) : 0.0f;
            const float lane = unit - 0.5f;
            Vec3 position{
                lane * minimumDimension * (0.36f + metrics.stereoWidth * 0.16f),
                minimumDimension * (0.15f + role.shadow * 0.06f),
                minimumDimension * (-0.06f + unit * 0.42f + role.shadow * 0.10f)
            };
            position = rolePosition(position, RoleDistrict::Shadow, convergence * 0.10f);
            pushRoleObject(makeObject3D(Object3DKind::Column,
                                        position,
                                        Vec3{minimumDimension * (0.020f + role.shadow * 0.018f),
                                             minimumDimension * (0.28f + role.shadow * 0.28f),
                                             minimumDimension * (0.024f + role.shadow * 0.022f)},
                                        Vec3{0.040f + intent.tension * 0.08f,
                                             lane * 0.12f,
                                             phase * 0.006f},
                                        withAlpha(mix(colors[4], colors[3], 0.34f), 0.20f + role.shadow * 0.22f),
                                        0.12f + role.shadow * 0.40f),
                           RoleDistrict::Shadow,
                           role.shadow,
                           shadowAnchors);
        }
        const int anchors = role.shadow > 0.34f ? 3 : (role.shadow > 0.26f ? 2 : 1);
        for (int i = 0; i < anchors; ++i) {
            const float side = anchors > 1 ? (static_cast<float>(i) / static_cast<float>(anchors - 1) * 2.0f - 1.0f) : 0.0f;
            Vec3 anchorPosition{
                side * minimumDimension * (0.055f + role.shadow * 0.025f),
                minimumDimension * (0.02f + role.shadow * 0.04f),
                minimumDimension * (0.16f + role.shadow * 0.18f + static_cast<float>(i) * 0.08f)
            };
            anchorPosition = rolePosition(anchorPosition, RoleDistrict::Shadow, convergence * 0.12f);
            pushRoleObject(makeObject3D(Object3DKind::Anchor,
                                        anchorPosition,
                                        Vec3{minimumDimension * (0.018f + role.shadow * 0.014f),
                                             minimumDimension * (0.018f + role.shadow * 0.014f),
                                             minimumDimension * 0.018f},
                                        Vec3{0.0f, phase * 0.010f + side * 0.08f, phase * 0.018f},
                                        withAlpha(colors[4], 0.24f + role.shadow * 0.22f),
                                        0.16f + role.shadow * 0.42f),
                           RoleDistrict::Shadow,
                           role.shadow,
                           shadowAnchors);
        }
    }

    const auto pushRoleBridge = [&](const std::vector<Vec3>& fromAnchors,
                                    const std::vector<Vec3>& toAnchors,
                                    ColorRGBA color,
                                    float relationship,
                                    int offset) {
        relationship *= std::clamp(0.68f + earnedConvergence * 0.28f + convergence * 0.18f,
                                   0.62f,
                                   1.02f);
        const float bridgeThreshold = std::clamp(0.36f - convergence * 0.045f + districtHold * 0.18f,
                                                 0.30f,
                                                 0.54f);
        if (relationship <= bridgeThreshold || fromAnchors.empty() || toAnchors.empty()) {
            return;
        }

        const std::size_t fromIndex = static_cast<std::size_t>(std::abs(offset)) % fromAnchors.size();
        const std::size_t toIndex = static_cast<std::size_t>(std::abs(offset * 3 + 1)) % toAnchors.size();
        const float bridgePull = clamp01(convergence * 0.28f + earnedConvergence * 0.12f);
        const Vec3 from = mix(fromAnchors[fromIndex], convergencePoint, bridgePull * 0.12f);
        const Vec3 to = mix(toAnchors[toIndex], convergencePoint, bridgePull * 0.12f);
        pushLink(from,
                 to,
                 withAlpha(color, 0.014f + relationship * 0.034f + convergence * 0.044f),
                 0.026f + relationship * 0.080f + convergence * 0.12f);

        if (convergence > 0.34f && relationship > bridgeThreshold + 0.08f) {
            const Vec3 span = subtract(to, from);
            const float spanLength = std::max(1.0f, length(span));
            Object3D surface = makeObject3D((offset % 2 == 0) ? Object3DKind::WaveSurface : Object3DKind::Plate,
                                            mix(mix(from, to, 0.5f), convergencePoint, convergence * 0.16f),
                                            Vec3{spanLength * (0.20f + convergence * 0.13f),
                                                 minimumDimension * (0.018f + relationship * 0.030f),
                                                 minimumDimension * (0.012f + convergence * 0.020f)},
                                            Vec3{std::atan2(span.y, std::max(1.0f, std::fabs(span.x))),
                                                 std::atan2(span.z, std::max(1.0f, std::fabs(span.x))),
                                                 std::atan2(span.y, span.x)},
                                            withAlpha(color, 0.050f + relationship * 0.060f + convergence * 0.080f),
                                            0.08f + relationship * 0.18f + convergence * 0.28f);
            surface.scale = scale(surface.scale, personalityScale);
            surface.musicRole = Object3DRole::Bridge;
            applyMotionDialect(surface);
            objects.push_back(surface);
        }
    };

    pushRoleBridge(bassAnchors,
                   drumAnchors,
                   colors[1],
                   std::min(role.bass, role.drums) * (0.64f + beat * 0.36f),
                   1);
    pushRoleBridge(melodyAnchors,
                   harmonyAnchors,
                   colors[2],
                   std::min(role.melody, role.harmony) * (0.70f + harmonic * 0.30f),
                   2);
    pushRoleBridge(spaceAnchors,
                   shadowAnchors,
                   colors[4],
                   std::min(role.space, role.shadow) * (0.58f + (1.0f - metrics.stereoWidth) * 0.22f),
                   3);
    pushRoleBridge(fractureAnchors,
                   drumAnchors,
                   colors[0],
                   std::min(role.fracture, role.drums) *
                       (0.46f + metrics.spectralFlux * 0.34f + staggeredCutEnergy3D(metrics) * 0.28f) *
                       clamp01(0.50f + (role.fracture - role.drums) * 1.10f),
                   4);
    pushRoleBridge(melodyAnchors,
                   spaceAnchors,
                   colors[3],
                   std::min(role.melody, role.space) * (0.52f + metrics.stereoWidth * 0.28f),
                   5);

    if (convergence > 0.26f) {
        Object3D knot = makeObject3D(Object3DKind::Cage,
                                     convergencePoint,
                                     Vec3{minimumDimension * (0.12f + convergence * 0.12f),
                                          minimumDimension * (0.09f + convergence * 0.08f),
                                          minimumDimension * (0.13f + convergence * 0.16f)},
                                     Vec3{phase * 0.030f + intent.tension * 0.20f,
                                          beatPhase * 0.18f,
                                          metrics.phrasePhase * kPi},
                                     withAlpha(colors[0], 0.24f + convergence * 0.30f),
                                     0.30f + convergence * 0.78f);
        knot.musicRole = Object3DRole::Convergence;
        objects.push_back(knot);

        const auto connectRole = [&](const std::vector<Vec3>& anchors, ColorRGBA color, int stride) {
            for (std::size_t i = 0; i < anchors.size(); i += static_cast<std::size_t>(std::max(1, stride))) {
                pushLink(anchors[i],
                         mix(anchors[i], convergencePoint, 0.46f + convergence * 0.22f),
                         color,
                         0.065f + convergence * 0.24f,
                         Object3DRole::Convergence);
            }
        };
        connectRole(bassAnchors, withAlpha(colors[1], 0.060f + convergence * 0.100f), 3);
        connectRole(drumAnchors, withAlpha(colors[0], 0.054f + convergence * 0.095f), 5);
        connectRole(melodyAnchors, withAlpha(colors[2], 0.066f + convergence * 0.110f), 3);
        connectRole(harmonyAnchors, withAlpha(colors[3], 0.060f + convergence * 0.100f), 2);
        connectRole(spaceAnchors, withAlpha(colors[4], 0.050f + convergence * 0.085f), 3);
        connectRole(fractureAnchors, withAlpha(colors[2], 0.060f + convergence * 0.095f), 4);
        connectRole(shadowAnchors, withAlpha(colors[4], 0.050f + convergence * 0.085f), 3);
    }
}

void addSceneTransitionObjects3D(std::vector<Object3D>& objects,
                                 const VisualSettings& settings,
                                 const AudioMetrics& metrics,
                                 const std::array<ColorRGBA, 5>& colors,
                                 float minimumDimension,
                                 float density,
                                 float response,
                                 double time)
{
    const float transition = clamp01(settings.sceneTransition);
    if (transition <= 0.001f) {
        return;
    }

    const float progress = clamp01(settings.sceneTransitionProgress);
    const float phase = static_cast<float>(time);
    const float foldPhase = progress * 2.0f * kPi;
    const float harmonic = clamp01(metrics.harmonicEnergy * 0.62f +
                                   metrics.keyConfidence * 0.26f +
                                   averageChromaEnergy(metrics) * 0.18f);
    const float pulse = transition * (0.68f + metrics.beatConfidence * 0.20f + metrics.dropIntensity * 0.18f);
    const int folds = scaledCount(4 + static_cast<int>(std::round(transition * 5.0f)),
                                  density * (0.44f + transition * 0.34f));

    for (int i = 0; i < folds; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, folds - 1));
        const float lane = unit * 2.0f - 1.0f;
        const float angle = lane * kPi * (0.42f + transition * 0.22f) + foldPhase + phase * 0.022f;
        const float depthLane = -0.30f + unit * (0.88f + transition * 0.18f);
        const Vec3 position{
            std::sin(angle) * minimumDimension * (0.20f + transition * 0.20f),
            lane * minimumDimension * (0.10f + harmonic * 0.06f),
            minimumDimension * (depthLane + std::cos(angle) * transition * 0.12f)
        };
        const Object3DKind kind = i % 3 == 0 ? Object3DKind::Cage :
                                  (i % 2 == 0 ? Object3DKind::DepthPlane : Object3DKind::Ribbon);
        objects.push_back(makeObject3D(kind,
                                       position,
                                       Vec3{minimumDimension * (0.020f + transition * 0.040f + unit * 0.010f),
                                            minimumDimension * (0.060f + transition * 0.110f + harmonic * 0.040f),
                                            minimumDimension * (0.018f + transition * 0.055f)},
                                       Vec3{foldPhase * 0.12f + unit,
                                            angle * 0.24f,
                                            foldPhase + lane * 0.34f},
                                       withAlpha(mix(colors[i % 5], colors[(i + 2) % 5], harmonic),
                                                 0.18f + transition * 0.28f),
                                       0.20f + pulse * 0.42f + response * 0.06f));
    }

    if (transition > 0.18f) {
        objects.push_back(makeObject3D(Object3DKind::WaveSurface,
                                       Vec3{0.0f,
                                            minimumDimension * std::sin(foldPhase) * 0.035f,
                                            minimumDimension * (0.12f + transition * 0.18f)},
                                       Vec3{minimumDimension * (0.26f + transition * 0.18f),
                                            minimumDimension * (0.080f + transition * 0.080f),
                                            minimumDimension * (0.018f + transition * 0.030f)},
                                       Vec3{0.42f + transition * 0.20f,
                                            foldPhase * 0.16f,
                                            foldPhase},
                                       withAlpha(colors[2], 0.12f + transition * 0.22f),
                                       0.18f + transition * 0.46f));
    }
}

void addEnvironmentContextObjects3D(std::vector<Object3D>& objects,
                                    const VisualSettings& settings,
                                    const EnvironmentState& environment,
                                    const AudioMetrics& metrics,
                                    const std::array<ColorRGBA, 5>& colors,
                                    float minimumDimension,
                                    float density,
                                    float response,
                                    double time)
{
    if (!settings.environmentReactive || !environment.enabled) {
        return;
    }

    const float drive = environmentDrive(settings, environment);
    if (drive <= 0.001f) {
        return;
    }

    const float phase = static_cast<float>(time);
    const float dayPhase = clamp01(environment.timeOfDay) * 2.0f * kPi;
    const float stereo = 0.72f + metrics.stereoWidth * 0.44f;
    const ColorRGBA warm = mix(colors[1], colors[2], 0.42f + std::sin(dayPhase) * 0.24f);
    const ColorRGBA cool = mix(colors[0], colors[3], 0.46f + std::cos(dayPhase) * 0.22f);
    const int shells = scaledCount(3 + static_cast<int>(std::round(drive * 4.0f)),
                                   density * (0.32f + drive * 0.28f));

    for (int i = 0; i < shells; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, shells - 1));
        const float angle = unit * 2.0f * kPi + dayPhase * 0.35f + phase * (0.012f + environment.motion * 0.018f);
        const float radius = minimumDimension * (0.18f + unit * 0.18f + environment.ambient * 0.05f) * stereo;
        objects.push_back(makeObject3D(i % 2 == 0 ? Object3DKind::DepthPlane : Object3DKind::Orbiter,
                                       Vec3{std::cos(angle) * radius,
                                            std::sin(angle * 0.72f) * minimumDimension * (0.06f + drive * 0.05f),
                                            minimumDimension * (0.18f + unit * 0.72f + environment.motion * 0.08f)},
                                       Vec3{minimumDimension * (0.10f + unit * 0.050f + drive * 0.055f),
                                            minimumDimension * (0.045f + environment.ambient * 0.050f),
                                            minimumDimension * (0.012f + drive * 0.012f)},
                                       Vec3{0.46f + unit * 0.16f,
                                            angle * 0.18f,
                                            dayPhase * 0.12f + phase * 0.010f},
                                       withAlpha(mix(cool, warm, unit), 0.10f + drive * 0.20f),
                                       0.10f + drive * 0.32f + response * 0.035f));
    }
}

void addObject3DScene(GeometryFrame& frame,
                      const AudioMetrics& metrics,
                      const VisualSettings& settings,
                      const InteractionState& interaction,
                      const EnvironmentState& environment,
                      const MusicSceneArc3D& songArc,
                      const std::array<ColorRGBA, 5>& colors,
                      float width,
                      float height,
                      float speed,
                      float intensity,
                      float quality,
                      double time)
{
    const float depth = depth3DOf(settings);
    if (depth <= 0.01f) {
        return;
    }

    const Scene3DProfile profile = profileForMode(settings.mode);
    const float minimumDimension = std::min(width, height);
    const SceneInterpretation intent = interpretSceneIntent(metrics, settings);
    const bool trueSilence = metrics.style == AudioStyle::Silence &&
                             metrics.rms < 0.025f &&
                             metrics.peak < 0.055f &&
                             metrics.beatConfidence < 0.08f;
    const float silenceDensityScale = trueSilence ? 0.58f : 1.0f;
    const float objectDensity = std::clamp(quality * (0.42f + objectDensity3DOf(settings) * 1.08f) *
                                               silenceDensityScale,
                                           0.18f,
                                           1.8f);
    const float personality = scenePersonalityOf(settings);
    const float response = musicResponse3D(metrics, settings);
    const SongSceneIdentity songIdentity = songSceneIdentityFor(intent, metrics, settings.mode);
    Camera3D camera = makeCamera3D(settings, metrics, intent, songArc, songIdentity, width, height, speed, time);
    applyCameraInteraction3D(camera, interaction, settings, width, height);
    const MusicChoreography choreography = buildMusicChoreography(metrics, settings, settings.mode, time, speed);
    SectionNarrative3D sectionNarrative = buildSectionNarrative3D(metrics, choreography, intent);
    sectionNarrative.build = std::max(sectionNarrative.build, songArc.anticipation * 0.78f);
    sectionNarrative.drop = std::max(sectionNarrative.drop, songArc.impact * 0.86f);
    sectionNarrative.release = std::max(sectionNarrative.release, songArc.recovery * 0.82f);
    sectionNarrative.intensity = std::max({sectionNarrative.build,
                                           sectionNarrative.drop,
                                           sectionNarrative.groove,
                                           sectionNarrative.breakdown,
                                           sectionNarrative.release});
    const MusicRoleScene3D roleScene = buildMusicRoleScene3D(intent, metrics, settings);
    if (songIdentity == SongSceneIdentity::DarkMonolith) {
        for (Ring& ring : frame.rings) {
            ring.color.a *= 0.10f;
            ring.strokeWidth *= 0.58f;
        }
        for (Beam& beam : frame.beams) {
            beam.color.a *= 0.14f;
            beam.width *= 0.70f;
        }
        for (Particle& particle : frame.particles) {
            particle.color.a *= 0.22f;
            particle.radius *= 0.74f;
        }
        for (Polyline& line : frame.polylines) {
            line.color.a *= 0.18f;
            line.strokeWidth *= 0.68f;
        }
        frame.retained2DPrimitiveCount = primitiveFootprint(frame);
        frame.retained2DVisualWeight = primitiveVisualWeight(frame);
    }
    std::vector<Object3D> objects;
    objects.reserve(520);

    switch (profile) {
    case Scene3DProfile::TechnoMachine:
        addTechnoMachineObjects(objects, metrics, colors, minimumDimension, objectDensity, intensity, personality, response, time);
        break;
    case Scene3DProfile::CrystalStorm:
        addCrystalStormObjects(objects, metrics, colors, minimumDimension, objectDensity, personality, response, time);
        break;
    case Scene3DProfile::NeuralSpace:
        addNeuralSpaceObjects(objects, metrics, colors, minimumDimension, objectDensity, personality, response, time);
        break;
    case Scene3DProfile::DimensionalTunnel:
        addDimensionalTunnelObjects(objects, metrics, colors, minimumDimension, objectDensity, personality, response, time);
        break;
    case Scene3DProfile::CymaticSculpture:
        addCymaticSculptureObjects(objects, metrics, colors, minimumDimension, objectDensity, personality, response, time);
        break;
    }
    addModeSpecific3DObjects(objects, settings.mode, metrics, colors, minimumDimension, objectDensity, personality, response, time);
    addModeSilhouetteAnchors3D(objects, settings.mode, metrics, colors, minimumDimension, objectDensity, personality, response, time);
    addSceneHeroAnchors3D(objects, settings.mode, metrics, colors, minimumDimension, objectDensity, personality, response, time);
    addIntentDrivenSceneObjects3D(objects, intent, metrics, colors, minimumDimension, objectDensity, personality, response, time);
    addSongIdentitySetPieces3D(objects,
                               songIdentity,
                               intent,
                               metrics,
                               colors,
                               minimumDimension,
                               objectDensity,
                               personality,
                               response,
                               time);
    addChoreographyDepthObjects3D(objects, settings.mode, choreography, colors, minimumDimension, objectDensity, response, time);
    addSectionNarrativeObjects3D(objects,
                                 settings.mode,
                                 sectionNarrative,
                                 metrics,
                                 choreography,
                                 colors,
                                 minimumDimension,
                                 objectDensity,
                                 response,
                                 time);
    addSongArcObjects3D(objects,
                        songArc,
                        metrics,
                        colors,
                        minimumDimension,
                        objectDensity,
                        response,
                        time);
    addSceneTransitionObjects3D(objects,
                                settings,
                                metrics,
                                colors,
                                minimumDimension,
                                objectDensity,
                                response,
                                time);
    addEnvironmentContextObjects3D(objects,
                                   settings,
                                   environment,
                                   metrics,
                                   colors,
                                   minimumDimension,
                                   objectDensity,
                                   response,
                                   time);
    applyMusicChoreography3D(objects, settings.mode, choreography, minimumDimension);
    applySongArcComposition3D(objects, songArc, metrics, settings, minimumDimension);
    applySongIdentityComposition3D(objects, songIdentity, intent, metrics, minimumDimension);
    applyModeComposition3D(objects, settings.mode, metrics, settings, minimumDimension, time);
    softenBackgroundMeshForRoleScene(objects, roleScene, minimumDimension);
    addMusicalRoleConvergenceRig3D(objects,
                                   roleScene,
                                   intent,
                                   metrics,
                                   settings.motionStyle,
                                   colors,
                                   minimumDimension,
                                   objectDensity,
                                   personality,
                                   response,
                                   motionStabilityOf(settings),
                                   patternClarityOf(settings),
                                   time);
    applyRoleVisualVocabulary3D(objects, roleScene, metrics, settings, minimumDimension);
    applyMusicalRoleDistrictStaging3D(objects,
                                      roleScene,
                                      intent,
                                      metrics,
                                      settings,
                                      minimumDimension,
                                      time);

    applyObjectInteraction3D(objects, interaction, settings, camera, width, height, static_cast<float>(time));
    applyPatternReadability3D(objects, settings, metrics, roleScene, minimumDimension);
    applyCinematicFrameStaging3D(objects, settings, metrics, intent, roleScene, songIdentity, minimumDimension);

    for (Object3D& object : objects) {
        const Projected3D projected = projectPoint3D(object.position, camera);
        object.depth = projected.depth;
    }
    std::stable_sort(objects.begin(), objects.end(), [](const Object3D& left, const Object3D& right) {
        return left.depth > right.depth;
    });

    float minimumDepth = objects.empty() ? 0.0f : objects.front().depth;
    float maximumDepth = minimumDepth;
    for (const Object3D& object : objects) {
        minimumDepth = std::min(minimumDepth, object.depth);
        maximumDepth = std::max(maximumDepth, object.depth);
    }
    const float range = std::max(1.0f, maximumDepth - minimumDepth);
    const ProjectedSceneStats3D projectedStats = projectedSceneStats3D(objects, camera, width, height);
    const RoleSceneStats3D roleStats = roleSceneStats3D(objects, roleScene, minimumDimension);
    const float lightingGlow = lightingGlowOf(settings);
    const int primitiveFootprintBefore3D = primitiveFootprint(frame);
    const float visualWeightBefore3D = primitiveVisualWeight(frame);
    for (const Object3D& object : objects) {
        const float depthUnit = std::clamp((object.depth - minimumDepth) / range, 0.0f, 1.0f);
        renderWireObject3D(frame, camera, object, depthUnit, lightingGlow);
    }

    frame.projected3DPrimitiveCount = std::max(0, primitiveFootprint(frame) - primitiveFootprintBefore3D);
    frame.projected3DVisualWeight = std::max(0.0f, primitiveVisualWeight(frame) - visualWeightBefore3D);
    frame.projected3DMaterialShare = frame.projected3DFillVisualWeight /
                                     std::max(1.0f,
                                              frame.projected3DFillVisualWeight +
                                                  frame.projected3DOutlineVisualWeight);
    frame.threeDDominance = frame.projected3DVisualWeight / std::max(1.0f, frame.retained2DVisualWeight);
    frame.projected3DScreenCoverage = projectedStats.screenCoverage;
    frame.projected3DCenterOffset = projectedStats.centerOffset;
    frame.foreground3DShare = projectedStats.foregroundShare;
    frame.midground3DShare = projectedStats.midgroundShare;
    frame.background3DShare = projectedStats.backgroundShare;
    frame.depthFogStrength = std::clamp(depth * lightingGlow * (range / std::max(1.0f, minimumDimension * 1.45f)),
                                        0.0f,
                                        1.0f);
    frame.sectionNarrative3D = sectionNarrative.intensity;
    frame.sectionBuild3D = sectionNarrative.build;
    frame.sectionDrop3D = sectionNarrative.drop;
    frame.sectionGroove3D = sectionNarrative.groove;
    frame.sectionBreakdown3D = sectionNarrative.breakdown;
    frame.sectionRelease3D = sectionNarrative.release;
    frame.songArc3D = songArc.intensity;
    frame.songArcAnticipation3D = songArc.anticipation;
    frame.songArcImpact3D = songArc.impact;
    frame.songArcRecovery3D = songArc.recovery;
    frame.songArcContinuity3D = songArc.continuity;
    frame.sceneBassRole3D = roleScene.bass;
    frame.sceneDrumRole3D = roleScene.drums;
    frame.sceneMelodyRole3D = roleScene.melody;
    frame.sceneHarmonyRole3D = roleScene.harmony;
    frame.sceneSpaceRole3D = roleScene.space;
    frame.sceneFractureRole3D = roleScene.fracture;
    frame.sceneShadowRole3D = roleScene.shadow;
    frame.sceneConvergence3D = roleScene.convergence;
    frame.sceneRoleSeparation3D = roleScene.separation;
    frame.sceneExplicitRoleShare3D = roleStats.explicitRoleShare;
    frame.sceneRoleBridgeShare3D = roleStats.bridgeShare;
    frame.sceneRoleDistrictSpread3D = roleStats.districtSpread;
    frame.sceneRoleBalance3D = roleStats.roleBalance;
    frame.sceneRoleVocabulary3D = roleStats.roleVocabulary;
    frame.sceneRoleSilhouetteContrast3D = roleStats.roleSilhouetteContrast;
    frame.objects3D = std::move(objects);
    frame.scene3DName = mode3DName(settings.mode);
    frame.sceneIntent = intent.primary;
    frame.sceneIntentName = toString(intent.primary);
    frame.cameraDepth = camera.cameraDistance;
    frame.cameraYaw = camera.yaw;
    frame.cameraPitch = camera.pitch;
    frame.cameraRoll = camera.roll;
    frame.cameraCenterOffset = Vec2{camera.center.x - width * 0.5f, camera.center.y - height * 0.5f};
    frame.objectDepthRange = range;
}

void addQuantumTunnel(GeometryFrame& frame,
                      const AudioMetrics& metrics,
                      const std::array<ColorRGBA, 5>& colors,
                      float width,
                      float height,
                      float drive,
                      float speed,
                      float intensity,
                      float quality,
                      double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float maxRadius = std::hypot(width, height) * 0.58f;
    const float sweep = static_cast<float>(time) * 84.0f * speed;
    const float bassPush = 1.0f + metrics.bass * 0.55f * intensity;

    const int ringCount = scaledCount(26, quality);
    for (int i = 0; i < ringCount; ++i) {
        const float normalized = static_cast<float>(i) / static_cast<float>(ringCount);
        const float wrapped = std::fmod((normalized * maxRadius) + sweep, maxRadius);
        frame.rings.push_back(Ring{
            center,
            (wrapped * bassPush) + 14.0f,
            3 + ((i + (metrics.beat ? 2 : 0)) % 7),
            static_cast<float>(time) * speed * (0.25f + normalized) + metrics.treble * 1.6f,
            1.2f + metrics.bass * 5.0f * intensity,
            withAlpha(colors[i % 4], 0.22f + normalized * 0.48f + drive * 0.22f)
        });
    }

    const int beamStep = scaledStep(quality);
    for (std::size_t i = 0; i < metrics.spectrum.size(); i += static_cast<std::size_t>(beamStep)) {
        const float energy = spectrumAt(metrics, i);
        const float angle = (static_cast<float>(i) / static_cast<float>(metrics.spectrum.size())) *
                            2.0f * kPi + static_cast<float>(time) * 0.28f * speed;
        frame.beams.push_back(Beam{
            angle,
            (height * 0.18f) + energy * maxRadius * 0.75f * intensity,
            0.8f + energy * 6.0f,
            withAlpha(colors[(i / 9) % 4], 0.12f + energy * 0.62f)
        });
    }
}

void addTechnoMandala(GeometryFrame& frame,
                      const AudioMetrics& metrics,
                      const std::array<ColorRGBA, 5>& colors,
                      float width,
                      float height,
                      float drive,
                      float speed,
                      float intensity,
                      float quality,
                      double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float baseRadius = std::min(width, height) * (0.16f + metrics.bass * 0.1f);
    const int petals = 10 + static_cast<int>(metrics.lowMid * 18.0f);
    const float phase = static_cast<float>(time) * speed;

    const int layerCount = scaledCount(7, quality);
    const int points = scaledCount(240, quality);
    for (int layer = 0; layer < layerCount; ++layer) {
        Polyline line;
        line.closed = true;
        line.strokeWidth = 1.0f + layer * 0.32f + metrics.beatConfidence * 5.0f;
        line.color = withAlpha(colors[layer % 4], 0.35f + drive * 0.42f);
        line.points.reserve(static_cast<std::size_t>(points));

        for (int i = 0; i < points; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(points);
            const float angle = t * 2.0f * kPi;
            const float band = spectrumAt(metrics, static_cast<std::size_t>(i + layer * 7));
            const float rose = std::sin(angle * static_cast<float>(petals + layer) +
                                        phase * (0.8f + layer * 0.12f));
            const float radius = baseRadius +
                                 layer * std::min(width, height) * 0.052f +
                                 rose * (28.0f + band * 120.0f) * intensity +
                                 metrics.treble * 44.0f;
            line.points.push_back(polar(center, radius, angle + phase * (0.11f + layer * 0.04f)));
        }
        frame.polylines.push_back(std::move(line));
    }

    for (int i = 0; i < petals * 2; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(petals * 2)) * 2.0f * kPi +
                            phase * 0.2f;
        frame.rings.push_back(Ring{
            polar(center, baseRadius * (1.35f + metrics.mid), angle),
            10.0f + metrics.highMid * 38.0f,
            3 + (i % 5),
            -phase + angle,
            1.0f + metrics.beatConfidence * 4.0f,
            withAlpha(colors[i % 4], 0.42f + metrics.beatConfidence * 0.44f)
        });
    }
}

void addLissajousMesh(GeometryFrame& frame,
                      const AudioMetrics& metrics,
                      const std::array<ColorRGBA, 5>& colors,
                      float width,
                      float height,
                      float drive,
                      float speed,
                      float intensity,
                      float quality,
                      double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float phase = static_cast<float>(time) * speed;
    const float xAmp = width * (0.22f + metrics.stereoWidth * 0.18f);
    const float yAmp = height * (0.18f + metrics.bass * 0.16f);

    const int layerCount = scaledCount(11, quality);
    const int points = scaledCount(180, quality);
    for (int layer = 0; layer < layerCount; ++layer) {
        Polyline line;
        line.strokeWidth = 0.8f + metrics.treble * 3.0f + layer * 0.05f;
        line.color = withAlpha(mix(colors[layer % 4], colors[(layer + 1) % 4], metrics.mid),
                               0.26f + drive * 0.5f);
        line.points.reserve(static_cast<std::size_t>(points));

        const float a = 2.0f + static_cast<float>(layer % 4);
        const float b = 3.0f + static_cast<float>((layer + 2) % 5);
        for (int i = 0; i < points; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(std::max(1, points - 1));
            const float angle = t * 2.0f * kPi;
            const float band = spectrumAt(metrics, static_cast<std::size_t>(i + layer * 3));
            const float warp = 1.0f + band * intensity * 0.55f;
            line.points.push_back(Vec2{
                center.x + std::sin((a * angle) + phase + layer * 0.24f) * xAmp * warp,
                center.y + std::sin((b * angle) + phase * 0.73f + layer) * yAmp * warp
            });
        }
        frame.polylines.push_back(std::move(line));
    }

    for (int i = 0; i < 36; ++i) {
        const float angle = static_cast<float>(i) / 36.0f * 2.0f * kPi + phase * 0.08f;
        const float energy = spectrumAt(metrics, static_cast<std::size_t>(i * 2));
        frame.particles.push_back(Particle{
            polar(center, std::min(width, height) * (0.12f + energy * 0.36f * intensity), angle),
            2.0f + energy * 12.0f + metrics.beatConfidence * 7.0f,
            withAlpha(colors[i % 4], 0.28f + energy * 0.68f)
        });
    }
}

void addFrequencyBloom(GeometryFrame& frame,
                       const AudioMetrics& metrics,
                       const std::array<ColorRGBA, 5>& colors,
                       float width,
                       float height,
                       float drive,
                       float speed,
                       float intensity,
                       float quality,
                       double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float base = std::min(width, height) * 0.1f;
    const float phase = static_cast<float>(time) * speed * 0.62f;

    const int binStep = scaledStep(quality);
    for (std::size_t i = 0; i < metrics.spectrum.size(); i += static_cast<std::size_t>(binStep)) {
        const float t = static_cast<float>(i) / static_cast<float>(metrics.spectrum.size());
        const float energy = spectrumAt(metrics, i);
        const float angle = t * 2.0f * kPi + phase;
        const float radius = base + energy * std::min(width, height) * 0.46f * intensity;
        frame.beams.push_back(Beam{
            angle,
            radius,
            1.0f + energy * 8.0f,
            withAlpha(colors[(i / 8) % 4], 0.18f + energy * 0.68f)
        });
        frame.particles.push_back(Particle{
            polar(center, radius, angle),
            2.0f + energy * 15.0f + metrics.beatConfidence * 8.0f,
            withAlpha(mix(colors[i % 4], colors[4], energy), 0.35f + energy * 0.58f)
        });
    }

    const int layerCount = scaledCount(8, quality);
    for (int layer = 0; layer < layerCount; ++layer) {
        frame.rings.push_back(Ring{
            center,
            base + layer * std::min(width, height) * 0.043f + metrics.bass * 90.0f * intensity,
            16 + layer * 4,
            phase * (0.25f + layer * 0.06f),
            0.9f + metrics.beatConfidence * 3.5f,
            withAlpha(colors[layer % 4], 0.16f + drive * 0.36f)
        });
    }
}

void addFractalCathedral(GeometryFrame& frame,
                         const AudioMetrics& metrics,
                         const std::array<ColorRGBA, 5>& colors,
                         float width,
                         float height,
                         float drive,
                         float speed,
                         float intensity,
                         float quality,
                         double time)
{
    const Vec2 center{width * 0.5f, height * 0.54f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const int archCount = scaledCount(13, quality);
    const int archPoints = scaledCount(54, quality);
    const float rootRadius = minimumDimension * (0.19f + metrics.bass * 0.09f * intensity);

    for (int arch = 0; arch < archCount; ++arch) {
        const float archUnit = static_cast<float>(arch) / static_cast<float>(std::max(1, archCount - 1));
        const float side = (archUnit * 2.0f) - 1.0f;
        const float span = minimumDimension * (0.11f + archUnit * 0.24f + metrics.lowMid * 0.08f);
        const float heightScale = minimumDimension * (0.16f + metrics.mid * 0.18f + archUnit * 0.14f);
        const float xBase = center.x + side * minimumDimension * 0.42f;
        const float yBase = height * (0.78f - metrics.dropIntensity * 0.08f);

        Polyline archLine;
        archLine.strokeWidth = 0.85f + metrics.beatConfidence * 3.2f + archUnit * 0.9f;
        archLine.color = withAlpha(mix(colors[arch % 4], colors[(arch + 2) % 4], metrics.spectralCentroid),
                                   0.22f + drive * 0.42f);
        archLine.points.reserve(static_cast<std::size_t>(archPoints));

        for (int i = 0; i < archPoints; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(std::max(1, archPoints - 1));
            const float angle = kPi * t;
            const float band = spectrumAt(metrics, static_cast<std::size_t>((arch * 5) + i));
            const float ripple = std::sin(angle * (3.0f + archUnit * 4.0f) + phase + band * 2.8f) *
                                 (8.0f + band * 42.0f) * intensity;
            archLine.points.push_back(Vec2{
                xBase + std::cos(angle) * span * (0.44f + archUnit * 0.34f),
                yBase - std::sin(angle) * heightScale + ripple
            });
        }
        frame.polylines.push_back(std::move(archLine));
    }

    const int recursionLevels = scaledCount(5, quality);
    for (int level = 0; level < recursionLevels; ++level) {
        const int branches = 4 + level * 2;
        const float radius = rootRadius + static_cast<float>(level) * minimumDimension * 0.075f +
                             metrics.dropIntensity * minimumDimension * 0.055f;
        for (int branch = 0; branch < branches; ++branch) {
            const float branchUnit = static_cast<float>(branch) / static_cast<float>(branches);
            const float angle = branchUnit * 2.0f * kPi + phase * (0.08f + level * 0.025f) +
                                metrics.beatPhase * kPi;
            const float band = spectrumAt(metrics, static_cast<std::size_t>(branch * 3 + level * 11));
            const Vec2 anchor = polar(center, radius * (0.72f + band * 0.28f * intensity), angle);
            frame.rings.push_back(Ring{
                anchor,
                9.0f + static_cast<float>(level) * 4.5f + band * 22.0f + metrics.phraseIntensity * 18.0f,
                3 + ((branch + level) % 6),
                -angle + phase * (0.35f + band),
                0.9f + metrics.dropIntensity * 4.5f + band * 2.2f,
                withAlpha(colors[(level + branch) % 4], 0.2f + band * 0.42f + metrics.phraseIntensity * 0.18f)
            });
        }
    }

    const int spireCount = scaledCount(18, quality);
    for (int i = 0; i < spireCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(spireCount);
        const float angle = unit * 2.0f * kPi + phase * 0.11f;
        const float energy = spectrumAt(metrics, static_cast<std::size_t>(i * 4));
        frame.beams.push_back(Beam{
            angle,
            minimumDimension * (0.18f + energy * 0.46f * intensity + metrics.dropIntensity * 0.18f),
            0.7f + energy * 5.5f + metrics.beatConfidence * 2.0f,
            withAlpha(colors[i % 4], 0.12f + energy * 0.5f + metrics.dropIntensity * 0.18f)
        });
    }
}

void addPolyrhythmLattice(GeometryFrame& frame,
                          const AudioMetrics& metrics,
                          const std::array<ColorRGBA, 5>& colors,
                          float width,
                          float height,
                          float drive,
                          float speed,
                          float intensity,
                          float quality,
                          double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const int columns = scaledCount(15, quality);
    const int rows = scaledCount(9, quality);
    const float cell = std::min(width / static_cast<float>(columns + 2),
                                height / static_cast<float>(rows + 2));
    const float xOrigin = center.x - (static_cast<float>(columns - 1) * cell * 0.5f);
    const float yOrigin = center.y - (static_cast<float>(rows - 1) * cell * 0.5f);

    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const std::size_t spectrumIndex = static_cast<std::size_t>((row * 7) + (column * 3));
            const float energy = spectrumAt(metrics, spectrumIndex);
            const float rowUnit = static_cast<float>(row) / static_cast<float>(std::max(1, rows - 1));
            const float columnUnit = static_cast<float>(column) / static_cast<float>(std::max(1, columns - 1));
            const float wave = std::sin(phase * (0.7f + rowUnit * 1.7f) +
                                        columnUnit * kPi * 6.0f +
                                        metrics.beatPhase * 2.0f * kPi);
            const float onset = metrics.bandOnsets[(row + column) % metrics.bandOnsets.size()];
            const float jitter = (energy * 0.42f + onset * 0.65f + metrics.dropIntensity * 0.25f) *
                                 cell * intensity;
            const Vec2 cellCenter{
                xOrigin + static_cast<float>(column) * cell + ((row % 2) != 0 ? cell * 0.5f : 0.0f),
                yOrigin + static_cast<float>(row) * cell * 0.84f + wave * jitter * 0.24f
            };

            if (energy < 0.04f && onset < 0.035f && ((row + column) % 3) != 0) {
                continue;
            }

            Polyline hex;
            hex.closed = true;
            hex.strokeWidth = 0.55f + energy * 3.8f + onset * 3.0f;
            hex.color = withAlpha(colors[(row + column) % 4], 0.08f + energy * 0.45f + onset * 0.4f + drive * 0.12f);
            hex.points.reserve(6);
            const float radius = cell * (0.27f + energy * 0.32f * intensity + onset * 0.16f);
            const float rotation = phase * 0.17f + metrics.beatPhase * kPi + energy * 0.7f;
            for (int side = 0; side < 6; ++side) {
                const float angle = rotation + static_cast<float>(side) / 6.0f * 2.0f * kPi;
                hex.points.push_back(polar(cellCenter, radius, angle));
            }
            frame.polylines.push_back(std::move(hex));

            if (onset > 0.08f || metrics.beat) {
                frame.rings.push_back(Ring{
                    cellCenter,
                    radius * (0.62f + onset * 1.7f),
                    6,
                    rotation,
                    0.8f + onset * 4.2f + metrics.beatConfidence,
                    withAlpha(colors[(row + column + 1) % 4], 0.18f + onset * 0.52f)
                });
            }
        }
    }

    const int rhythmCount = scaledCount(24, quality);
    for (int i = 0; i < rhythmCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(rhythmCount);
        const float angleA = unit * 2.0f * kPi + phase * 0.19f;
        const float angleB = unit * 2.0f * kPi * (1.0f + metrics.beatPhase * 0.08f) - phase * 0.13f;
        const float energy = spectrumAt(metrics, static_cast<std::size_t>(i * 5));
        const Vec2 a = polar(center, minimumDimension * (0.16f + energy * 0.28f), angleA);
        const Vec2 b = polar(center, minimumDimension * (0.43f + metrics.stereoWidth * 0.16f), angleB);
        frame.polylines.push_back(Polyline{
            std::vector<Vec2>{a, b},
            0.45f + energy * 2.5f,
            withAlpha(colors[i % 4], 0.08f + energy * 0.34f + metrics.phraseIntensity * 0.18f),
            false
        });
    }
}

void addSpectralOrigami(GeometryFrame& frame,
                        const AudioMetrics& metrics,
                        const std::array<ColorRGBA, 5>& colors,
                        float width,
                        float height,
                        float drive,
                        float speed,
                        float intensity,
                        float quality,
                        double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const int foldCount = scaledCount(18, quality);
    const int symmetryCount = (quality < 0.62f ? 4 : 6) + (metrics.stereoWidth > 0.34f ? 2 : 0);

    for (int fold = 0; fold < foldCount; ++fold) {
        const float unit = static_cast<float>(fold) / static_cast<float>(std::max(1, foldCount - 1));
        const std::size_t spectrumIndex = static_cast<std::size_t>(fold * 4);
        const float energy = spectrumAt(metrics, spectrumIndex);
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(fold + std::max(0, metrics.keyIndex)));
        const float onset = metrics.bandOnsets[fold % metrics.bandOnsets.size()];
        const float foldWave = std::sin(phase * (0.72f + unit * 1.8f) +
                                        metrics.beatPhase * 2.0f * kPi +
                                        energy * 4.2f +
                                        chroma * 1.6f);
        const float radius = minimumDimension * (0.12f + unit * 0.44f +
                                                 metrics.bass * 0.055f * intensity +
                                                 chroma * 0.035f * intensity);
        const float shardSize = minimumDimension * (0.028f +
                                                    energy * 0.07f * intensity +
                                                    chroma * 0.045f * intensity +
                                                    onset * 0.045f +
                                                    metrics.dropIntensity * 0.018f);
        const float crease = foldWave * shardSize * (0.7f + metrics.stereoWidth * 0.8f);
        const ColorRGBA shardColor = withAlpha(
            mix(colors[fold % 4], colors[(fold + 2) % 4], metrics.spectralCentroid),
            0.14f + energy * 0.48f + onset * 0.28f + drive * 0.12f);

        for (int mirror = 0; mirror < symmetryCount; ++mirror) {
            const float mirrorUnit = static_cast<float>(mirror) / static_cast<float>(symmetryCount);
            const float keyRotation = metrics.keyIndex >= 0
                                          ? (static_cast<float>(metrics.keyIndex) / 12.0f) * 2.0f * kPi
                                          : 0.0f;
            const float modeTwist = metrics.keyMode == MusicalMode::Minor ? -0.18f : 0.18f;
            const float angle = mirrorUnit * 2.0f * kPi +
                                unit * 0.32f +
                                keyRotation * 0.18f +
                                modeTwist * metrics.keyConfidence +
                                phase * (0.08f + metrics.highMid * 0.08f);
            const Vec2 normal{std::cos(angle), std::sin(angle)};
            const Vec2 tangent{-normal.y, normal.x};
            const Vec2 anchor = polar(center,
                                      radius * (0.86f + energy * 0.22f * intensity),
                                      angle + foldWave * 0.05f);

            const Vec2 p0{
                anchor.x - tangent.x * shardSize + normal.x * crease,
                anchor.y - tangent.y * shardSize + normal.y * crease
            };
            const Vec2 p1{
                anchor.x + tangent.x * shardSize - normal.x * crease * 0.45f,
                anchor.y + tangent.y * shardSize - normal.y * crease * 0.45f
            };
            const Vec2 p2{
                anchor.x + normal.x * shardSize * (1.25f + energy * 0.85f) +
                    tangent.x * foldWave * shardSize * 0.52f,
                anchor.y + normal.y * shardSize * (1.25f + energy * 0.85f) +
                    tangent.y * foldWave * shardSize * 0.52f
            };

            frame.polylines.push_back(Polyline{
                std::vector<Vec2>{p0, p1, p2},
                0.8f + energy * 3.2f + onset * 2.4f,
                shardColor,
                true
            });

            frame.polylines.push_back(Polyline{
                std::vector<Vec2>{p0, anchor, p1},
                0.45f + metrics.treble * 1.6f,
                withAlpha(colors[(fold + mirror + 1) % 4], 0.08f + energy * 0.22f + chroma * 0.18f + onset * 0.18f),
                false
            });

            if (onset > 0.07f || metrics.beat) {
                frame.rings.push_back(Ring{
                    p2,
                    shardSize * (0.32f + onset * 1.35f + metrics.beatConfidence * 0.28f),
                    3 + ((fold + mirror) % 5),
                    -angle + phase * 0.34f,
                    0.7f + onset * 4.2f + metrics.beatConfidence * 1.8f,
                    withAlpha(colors[(fold + mirror + 2) % 4], 0.15f + onset * 0.5f + metrics.beatConfidence * 0.2f)
                });
            }
        }
    }

    const int spineCount = scaledCount(10, quality);
    for (int i = 0; i < spineCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(spineCount);
        const float angle = unit * 2.0f * kPi + phase * 0.11f + metrics.beatPhase * kPi;
        const float energy = spectrumAt(metrics, static_cast<std::size_t>(i * 7));
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(i + std::max(0, metrics.keyIndex)));
        frame.beams.push_back(Beam{
            angle,
            minimumDimension * (0.2f + energy * 0.36f * intensity + chroma * 0.18f * intensity + metrics.dropIntensity * 0.18f),
            0.65f + energy * 4.8f + metrics.dropIntensity * 4.0f,
            withAlpha(colors[i % 4], 0.09f + energy * 0.3f + chroma * 0.24f + metrics.dropIntensity * 0.22f)
        });
    }
}

void addChromaKaleidoscope(GeometryFrame& frame,
                            const AudioMetrics& metrics,
                            const std::array<ColorRGBA, 5>& colors,
                           float width,
                           float height,
                           float drive,
                           float speed,
                           float intensity,
                           float quality,
                           double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const float keyPhase = metrics.keyIndex >= 0
                               ? (static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f) * 2.0f * kPi
                               : 0.0f;
    const float tonalConfidence = metrics.keyConfidence * metrics.harmonicEnergy;
    const float modeBias = metrics.keyMode == MusicalMode::Minor ? -0.14f :
                           (metrics.keyMode == MusicalMode::Major ? 0.09f : 0.0f);
    const int symmetryCount = (quality < 0.62f ? 6 : 8) +
                              (metrics.stereoWidth > 0.35f ? 2 : 0) +
                              (metrics.keyMode == MusicalMode::Minor ? 1 : 0);

    const int harmonicRings = scaledCount(7, quality);
    for (int ring = 0; ring < harmonicRings; ++ring) {
        const float unit = static_cast<float>(ring + 1) / static_cast<float>(harmonicRings);
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(ring + std::max(0, metrics.keyIndex)));
        frame.rings.push_back(Ring{
            center,
            minimumDimension * (0.08f + unit * 0.073f + chroma * 0.045f * intensity + metrics.bass * 0.025f),
            5 + ((ring + std::max(0, metrics.keyIndex)) % 8),
            phase * (0.21f + unit * 0.14f) + keyPhase * (0.12f + unit * 0.05f),
            0.72f + tonalConfidence * 3.2f + metrics.beatConfidence * 1.6f,
            withAlpha(mix(colors[ring % 4], colors[(ring + 2) % 4], chroma),
                      0.12f + drive * 0.24f + chroma * 0.22f)
        });
    }

    for (int note = 0; note < static_cast<int>(kChromaBinCount); ++note) {
        const float noteUnit = static_cast<float>(note) / static_cast<float>(kChromaBinCount);
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(note));
        const float energy = std::max(chroma, spectrumAt(metrics, static_cast<std::size_t>(note * 5)) * 0.72f);
        const float onset = metrics.bandOnsets[static_cast<std::size_t>(note) % metrics.bandOnsets.size()];
        const float notePhase = noteUnit * 2.0f * kPi +
                                keyPhase * (0.24f + tonalConfidence * 0.16f) +
                                modeBias * metrics.keyConfidence +
                                phase * (0.08f + energy * 0.18f);
        const float baseRadius = minimumDimension * (0.13f + noteUnit * 0.37f +
                                                     energy * 0.08f * intensity +
                                                     metrics.phraseIntensity * 0.035f);
        const float shard = minimumDimension * (0.015f + energy * 0.065f * intensity +
                                                onset * 0.04f + metrics.beatConfidence * 0.01f);
        const ColorRGBA prismColor = withAlpha(
            mix(colors[note % 4], colors[(note + 1) % 4], std::max(chroma, metrics.spectralCentroid)),
            0.13f + energy * 0.42f + onset * 0.24f + tonalConfidence * 0.18f);

        for (int mirror = 0; mirror < symmetryCount; ++mirror) {
            const float mirrorUnit = static_cast<float>(mirror) / static_cast<float>(symmetryCount);
            const float mirrorAngle = mirrorUnit * 2.0f * kPi;
            const float mirroredNotePhase = ((mirror % 2) == 0 ? notePhase : -notePhase) * 0.18f;
            const float angle = mirrorAngle + mirroredNotePhase + metrics.beatPhase * 0.22f * kPi;
            const Vec2 normal{std::cos(angle), std::sin(angle)};
            const Vec2 tangent{-normal.y, normal.x};
            const float pulse = std::sin(phase * (0.9f + noteUnit * 0.7f) +
                                         static_cast<float>(mirror) * 1.37f +
                                         energy * 2.8f);
            const float skew = pulse * shard * (0.38f + metrics.stereoWidth * 0.72f);
            const Vec2 anchor = polar(center,
                                      baseRadius * (0.9f + pulse * 0.035f + metrics.dropIntensity * 0.08f),
                                      angle + keyPhase * 0.035f);

            const Vec2 p0{anchor.x + normal.x * shard * 1.35f,
                          anchor.y + normal.y * shard * 1.35f};
            const Vec2 p1{anchor.x + tangent.x * shard * 0.95f + normal.x * skew,
                          anchor.y + tangent.y * shard * 0.95f + normal.y * skew};
            const Vec2 p2{anchor.x - normal.x * shard * (1.0f + energy * 0.65f),
                          anchor.y - normal.y * shard * (1.0f + energy * 0.65f)};
            const Vec2 p3{anchor.x - tangent.x * shard * 0.95f - normal.x * skew,
                          anchor.y - tangent.y * shard * 0.95f - normal.y * skew};

            frame.polylines.push_back(Polyline{
                std::vector<Vec2>{p0, p1, p2, p3},
                0.55f + energy * 2.8f + onset * 2.6f,
                prismColor,
                true
            });

            if (chroma > 0.08f || onset > 0.08f || metrics.beat) {
                frame.rings.push_back(Ring{
                    anchor,
                    shard * (0.42f + chroma * 1.35f + onset * 1.15f),
                    3 + ((note + mirror) % 7),
                    -angle + phase * 0.27f,
                    0.6f + onset * 3.8f + chroma * 2.4f,
                    withAlpha(colors[(note + mirror + 2) % 4],
                              0.1f + chroma * 0.36f + onset * 0.38f + metrics.beatConfidence * 0.16f)
                });
            }
        }
    }

    for (int mirror = 0; mirror < symmetryCount; ++mirror) {
        Polyline rail;
        rail.closed = false;
        rail.strokeWidth = 0.5f + metrics.harmonicEnergy * 1.8f + metrics.stereoWidth * 0.9f;
        rail.color = withAlpha(colors[mirror % 4], 0.08f + tonalConfidence * 0.28f + drive * 0.12f);
        rail.points.reserve(kChromaBinCount);
        const float mirrorAngle = static_cast<float>(mirror) / static_cast<float>(symmetryCount) * 2.0f * kPi;
        for (int note = 0; note < static_cast<int>(kChromaBinCount); ++note) {
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(note));
            const float noteUnit = static_cast<float>(note) / static_cast<float>(kChromaBinCount - 1);
            const float ribbon = std::sin(phase * 0.52f + noteUnit * 2.0f * kPi + keyPhase) *
                                 minimumDimension * (0.012f + chroma * 0.028f * intensity);
            const float angle = mirrorAngle + (noteUnit - 0.5f) * (0.35f + metrics.stereoWidth * 0.18f);
            const float radius = minimumDimension * (0.18f + noteUnit * 0.34f + chroma * 0.1f * intensity);
            rail.points.push_back(polar(center, radius + ribbon, angle));
        }
        frame.polylines.push_back(std::move(rail));
    }

    const int beamCount = scaledCount(12, quality);
    for (int i = 0; i < beamCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(beamCount);
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(i));
        frame.beams.push_back(Beam{
            unit * 2.0f * kPi + keyPhase * 0.32f + phase * (0.04f + chroma * 0.08f),
            minimumDimension * (0.18f + chroma * 0.38f * intensity + metrics.dropIntensity * 0.14f),
            0.55f + chroma * 5.8f + metrics.beatConfidence * 2.2f,
            withAlpha(colors[i % 4], 0.06f + chroma * 0.42f + metrics.dropIntensity * 0.18f)
        });
    }
}

void addHyperspacePolytope(GeometryFrame& frame,
                           const AudioMetrics& metrics,
                           const std::array<ColorRGBA, 5>& colors,
                           float width,
                           float height,
                           float drive,
                           float speed,
                           float intensity,
                           float quality,
                           double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const float beatOrbit = metrics.beatPhase * 2.0f * kPi;
    const float dropWarp = 1.0f + metrics.dropIntensity * 0.36f;
    const int shellCount = scaledCount(5, quality);

    for (int shell = 0; shell < shellCount; ++shell) {
        const float shellUnit = static_cast<float>(shell + 1) / static_cast<float>(shellCount);
        const float shellScale = minimumDimension * (0.105f + shellUnit * 0.052f) *
                                 (0.92f + drive * 0.18f + metrics.harmonicEnergy * 0.08f);
        std::array<Vec2, 16> projected{};
        std::array<float, 16> depth{};

        for (int vertex = 0; vertex < 16; ++vertex) {
            Vec4 point{
                (vertex & 1) != 0 ? 1.0f : -1.0f,
                (vertex & 2) != 0 ? 1.0f : -1.0f,
                (vertex & 4) != 0 ? 1.0f : -1.0f,
                (vertex & 8) != 0 ? 1.0f : -1.0f
            };

            const float spectral = spectrumAt(metrics, static_cast<std::size_t>(vertex * 3 + shell));
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(vertex + shell));
            const float localEnergy = 0.35f * spectral + chroma * metrics.harmonicEnergy;
            const float vertexScale = 0.78f + shellUnit * 0.32f + localEnergy * 0.18f * intensity;
            point.x *= vertexScale;
            point.y *= vertexScale * (0.96f + metrics.stereoWidth * 0.12f);
            point.z *= vertexScale * (0.9f + metrics.treble * 0.1f);
            point.w *= vertexScale * dropWarp;

            rotatePlane(point.x, point.w, phase * (0.38f + shellUnit * 0.08f) + beatOrbit * 0.18f + metrics.bass * 0.44f);
            rotatePlane(point.y, point.z, -phase * (0.31f + shellUnit * 0.06f) + metrics.stereoWidth * 0.85f);
            rotatePlane(point.x, point.y, phase * 0.13f + shellUnit * kPi + metrics.treble * 0.34f);
            rotatePlane(point.z, point.w, phase * (0.23f + metrics.spectralFlux * 0.12f) + metrics.dropIntensity * 0.72f);

            const float perspective4 = 1.0f / std::max(0.78f, 2.35f - point.w * 0.36f);
            const float x3 = point.x * perspective4;
            const float y3 = point.y * perspective4;
            const float z3 = point.z * perspective4;
            const float perspective3 = 1.0f / std::max(0.82f, 2.05f - z3 * 0.28f);
            projected[static_cast<std::size_t>(vertex)] = Vec2{
                center.x + x3 * shellScale * perspective3,
                center.y + y3 * shellScale * perspective3
            };
            depth[static_cast<std::size_t>(vertex)] = clamp01(0.5f + z3 * 0.2f + point.w * 0.08f);

            frame.rings.push_back(Ring{
                projected[static_cast<std::size_t>(vertex)],
                2.4f + localEnergy * 8.0f + depth[static_cast<std::size_t>(vertex)] * 2.2f,
                4 + ((vertex + shell) % 5),
                phase * (0.65f + shellUnit) + localEnergy * kPi,
                0.75f + localEnergy * 3.0f + metrics.beatConfidence * 1.2f,
                withAlpha(colors[(vertex + shell) % 4], 0.18f + localEnergy * 0.42f + depth[static_cast<std::size_t>(vertex)] * 0.14f)
            });
        }

        int edgeIndex = 0;
        for (int vertex = 0; vertex < 16; ++vertex) {
            for (int bit = 0; bit < 4; ++bit) {
                const int other = vertex ^ (1 << bit);
                if (other < vertex) {
                    continue;
                }

                const Vec2 a = projected[static_cast<std::size_t>(vertex)];
                const Vec2 b = projected[static_cast<std::size_t>(other)];
                const float dx = b.x - a.x;
                const float dy = b.y - a.y;
                const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
                const float spectral = spectrumAt(metrics, static_cast<std::size_t>(edgeIndex * 5 + shell));
                const float chroma = chromaAt(metrics, static_cast<std::size_t>(edgeIndex + shell));
                const float edgeEnergy = 0.45f * spectral + 0.55f * chroma * metrics.harmonicEnergy;
                const float fold = std::sin(phase * 1.7f + beatOrbit + static_cast<float>(edgeIndex) * 0.37f) *
                                   minimumDimension * (0.004f + edgeEnergy * 0.025f * intensity + metrics.dropIntensity * 0.012f);
                const Vec2 normal{-dy / length, dx / length};

                Polyline line;
                line.closed = false;
                line.strokeWidth = 0.55f + edgeEnergy * 3.2f + metrics.beatConfidence * 0.65f;
                line.color = withAlpha(mix(colors[(edgeIndex + shell) % 4], colors[(edgeIndex + shell + 2) % 4], shellUnit),
                                       0.07f + edgeEnergy * 0.34f + metrics.dropIntensity * 0.1f);
                line.points.reserve(3);
                line.points.push_back(a);
                line.points.push_back(Vec2{
                    (a.x + b.x) * 0.5f + normal.x * fold,
                    (a.y + b.y) * 0.5f + normal.y * fold
                });
                line.points.push_back(b);
                frame.polylines.push_back(std::move(line));

                if (edgeEnergy > 0.28f || metrics.dropIntensity > 0.35f) {
                    frame.particles.push_back(Particle{
                        Vec2{(a.x + b.x) * 0.5f + normal.x * fold * 0.7f,
                             (a.y + b.y) * 0.5f + normal.y * fold * 0.7f},
                        1.4f + edgeEnergy * 5.2f + metrics.dropIntensity * 3.0f,
                        withAlpha(colors[(edgeIndex + 1) % 4], 0.16f + edgeEnergy * 0.42f)
                    });
                }
                ++edgeIndex;
            }
        }

        const int orbitCount = scaledCount(8, quality);
        for (int i = 0; i < orbitCount; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(orbitCount);
            const float angle = unit * 2.0f * kPi + phase * (0.18f + shellUnit * 0.12f) + beatOrbit * 0.08f;
            frame.beams.push_back(Beam{
                angle,
                shellScale * (1.5f + drive * 0.55f + spectrumAt(metrics, static_cast<std::size_t>(i + shell)) * 0.6f),
                0.42f + metrics.treble * 1.5f + metrics.spectralFlux * 2.6f,
                withAlpha(colors[(i + shell) % 4], 0.045f + metrics.spectralFlux * 0.18f + shellUnit * 0.04f)
            });
        }
    }
}

void addPhaseWeave(GeometryFrame& frame,
                   const AudioMetrics& metrics,
                   const std::array<ColorRGBA, 5>& colors,
                   float width,
                   float height,
                   float drive,
                   float speed,
                   float intensity,
                   float quality,
                   double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const float beatOrbit = metrics.beatPhase * 2.0f * kPi;
    const float stereo = clamp01(metrics.stereoWidth);
    const float flux = clamp01(metrics.spectralFlux * 1.35f);
    const float harmonic = clamp01(metrics.harmonicEnergy * metrics.keyConfidence);
    const float rootPhase = metrics.keyIndex >= 0
                                ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f * 2.0f * kPi
                                : 0.0f;
    const int symmetry = 5 + (metrics.keyIndex >= 0 ? ((metrics.keyIndex % 5 + 5) % 5) : 2) +
                         static_cast<int>(std::round(harmonic * 3.0f));

    const int streamCount = scaledCount(20, quality);
    const int steps = scaledCount(42, quality);
    for (int stream = 0; stream < streamCount; ++stream) {
        const float streamUnit = static_cast<float>(stream) / static_cast<float>(std::max(1, streamCount));
        const float spectralSeed = spectrumAt(metrics, static_cast<std::size_t>(stream * 5));
        const float chromaSeed = chromaAt(metrics, static_cast<std::size_t>(stream + metrics.keyIndex + 12));
        const float stereoBias = (stream % 2 == 0 ? -1.0f : 1.0f) * stereo;
        const float seedAngle = streamUnit * 2.0f * kPi +
                                phase * (0.12f + spectralSeed * 0.08f) +
                                rootPhase * (0.35f + harmonic * 0.3f) +
                                stereoBias * 0.18f;
        const float seedRadius = minimumDimension * (0.08f +
                                                     streamUnit * 0.38f +
                                                     spectralSeed * 0.09f * intensity +
                                                     metrics.dropIntensity * 0.06f);

        Vec2 pointValue = polar(center, seedRadius, seedAngle);
        Polyline line;
        line.closed = false;
        line.strokeWidth = 0.62f + spectralSeed * 2.7f + metrics.beatConfidence * 1.1f + harmonic * 1.2f;
        line.color = withAlpha(mix(colors[stream % 4],
                                   colors[(stream + 2) % 4],
                                   clamp01(chromaSeed * harmonic + flux * 0.28f)),
                               0.1f + drive * 0.16f + spectralSeed * 0.24f + harmonic * 0.1f);
        line.points.reserve(static_cast<std::size_t>(steps));

        for (int step = 0; step < steps; ++step) {
            const float stepUnit = static_cast<float>(step) / static_cast<float>(std::max(1, steps - 1));
            const std::size_t spectralIndex = static_cast<std::size_t>(stream * 7 + step * 3);
            const float spectral = spectrumAt(metrics, spectralIndex);
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(stream + step));
            const float localEnergy = clamp01(0.52f * spectral +
                                              0.24f * chroma * metrics.harmonicEnergy +
                                              0.16f * flux +
                                              0.08f * metrics.dropIntensity);

            const float dx = (pointValue.x - center.x) / std::max(1.0f, minimumDimension);
            const float dy = (pointValue.y - center.y) / std::max(1.0f, minimumDimension);
            const float radialAngle = std::atan2(dy, dx);
            const float braid = std::sin(stepUnit * static_cast<float>(symmetry) * 2.0f * kPi +
                                         phase * (0.8f + spectral * 0.6f) +
                                         beatOrbit +
                                         streamUnit * 2.0f * kPi);
            const float counterBraid = std::cos(stepUnit * 4.0f * kPi +
                                                phase * 0.55f -
                                                rootPhase +
                                                stereoBias * 1.1f);
            const float flowAngle = radialAngle +
                                    kPi * 0.5f +
                                    braid * (0.32f + flux * 0.72f + localEnergy * 0.38f) +
                                    counterBraid * stereo * 0.32f +
                                    metrics.beatConfidence * 0.18f * std::sin(beatOrbit + stepUnit * 2.0f * kPi);
            const float outward = std::sin(stepUnit * kPi + beatOrbit + spectral * 2.0f) *
                                  minimumDimension * (0.0015f + metrics.phraseIntensity * 0.0045f);
            const float stepLength = minimumDimension * (0.0055f +
                                                         localEnergy * 0.017f * intensity +
                                                         drive * 0.0035f +
                                                         metrics.dropIntensity * 0.003f);

            pointValue.x += std::cos(flowAngle) * stepLength + std::cos(radialAngle) * outward;
            pointValue.y += std::sin(flowAngle) * stepLength + std::sin(radialAngle) * outward;
            pointValue.x = std::clamp(pointValue.x, -width * 0.08f, width * 1.08f);
            pointValue.y = std::clamp(pointValue.y, -height * 0.08f, height * 1.08f);
            line.points.push_back(pointValue);
        }

        frame.polylines.push_back(std::move(line));

        if (spectralSeed > 0.26f || metrics.dropIntensity > 0.22f || harmonic > 0.22f) {
            frame.rings.push_back(Ring{
                pointValue,
                5.0f + spectralSeed * 20.0f + harmonic * 16.0f + metrics.dropIntensity * 18.0f,
                3 + ((stream + symmetry) % 7),
                phase * (0.42f + streamUnit * 0.4f) + beatOrbit,
                0.6f + spectralSeed * 2.8f + metrics.beatConfidence * 2.2f,
                withAlpha(colors[(stream + 1) % 4], 0.13f + spectralSeed * 0.22f + harmonic * 0.18f)
            });
        }
        if (stream % 3 == 0 || metrics.spectralFlux > 0.18f) {
            frame.particles.push_back(Particle{
                pointValue,
                1.6f + spectralSeed * 7.0f + metrics.beatConfidence * 4.0f,
                withAlpha(colors[(stream + 3) % 4], 0.18f + spectralSeed * 0.34f + flux * 0.16f)
            });
        }
    }

    const int knotCount = scaledCount(12, quality);
    for (int knot = 0; knot < knotCount; ++knot) {
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(knot));
        const float unit = static_cast<float>(knot) / static_cast<float>(knotCount);
        const float angle = unit * 2.0f * kPi + rootPhase + beatOrbit * 0.12f + phase * 0.04f;
        const float harmonicWeight = clamp01(chroma * (0.35f + metrics.harmonicEnergy));
        const float radius = minimumDimension * (0.18f + unit * 0.18f + harmonicWeight * 0.13f * intensity);
        if (harmonicWeight > 0.035f || harmonic > 0.18f) {
            frame.particles.push_back(Particle{
                polar(center, radius, angle),
                1.4f + harmonicWeight * 13.0f + metrics.keyConfidence * 3.0f,
                withAlpha(colors[(knot + 2) % 4], 0.1f + harmonicWeight * 0.42f + harmonic * 0.12f)
            });
        }
        if (harmonicWeight > 0.22f) {
            frame.rings.push_back(Ring{
                polar(center, radius, angle),
                7.0f + harmonicWeight * 18.0f,
                3 + (knot % 5),
                -angle + phase * 0.22f,
                0.55f + harmonicWeight * 2.5f,
                withAlpha(colors[knot % 4], 0.08f + harmonicWeight * 0.22f)
            });
        }
    }

    const int attractorCount = 2;
    for (int i = 0; i < attractorCount; ++i) {
        const float side = i == 0 ? -1.0f : 1.0f;
        const Vec2 attractor{
            center.x + side * minimumDimension * (0.16f + stereo * 0.24f),
            center.y + std::sin(beatOrbit + side * kPi * 0.35f + phase * 0.18f) * minimumDimension *
                           (0.05f + flux * 0.08f)
        };
        frame.rings.push_back(Ring{
            attractor,
            minimumDimension * (0.035f + stereo * 0.075f + metrics.beatConfidence * 0.025f),
            40,
            phase * (0.15f + side * 0.03f) + beatOrbit * side,
            0.6f + stereo * 2.2f + metrics.beatConfidence * 1.4f,
            withAlpha(colors[i], 0.04f + stereo * 0.14f + flux * 0.08f)
        });
    }

    if (metrics.dropIntensity > 0.05f || metrics.section == ArrangementSection::Build) {
        const int ribbons = scaledCount(3 + static_cast<int>(metrics.dropIntensity * 4.0f), quality);
        const int ribbonPoints = scaledCount(72, quality);
        for (int ribbon = 0; ribbon < ribbons; ++ribbon) {
            const float ribbonUnit = static_cast<float>(ribbon) / static_cast<float>(std::max(1, ribbons));
            Polyline line;
            line.closed = true;
            line.strokeWidth = 0.7f + metrics.dropIntensity * 4.6f + metrics.sectionConfidence * 1.2f;
            line.color = withAlpha(colors[(ribbon + 1) % 4],
                                   0.06f + metrics.dropIntensity * 0.24f + metrics.sectionConfidence * 0.08f);
            line.points.reserve(static_cast<std::size_t>(ribbonPoints));
            for (int pointIndex = 0; pointIndex < ribbonPoints; ++pointIndex) {
                const float unit = static_cast<float>(pointIndex) / static_cast<float>(ribbonPoints);
                const float angle = unit * 2.0f * kPi + phase * (0.1f + ribbonUnit * 0.12f) + rootPhase;
                const float fold = std::sin(unit * static_cast<float>(symmetry) * 2.0f * kPi +
                                            beatOrbit +
                                            ribbonUnit * kPi) *
                                   minimumDimension * (0.012f + flux * 0.03f + metrics.dropIntensity * 0.026f);
                const float radius = minimumDimension * (0.2f +
                                                         ribbonUnit * 0.09f +
                                                         metrics.dropIntensity * 0.18f +
                                                         metrics.sectionProgress * 0.08f);
                line.points.push_back(polar(center, radius + fold, angle));
            }
            frame.polylines.push_back(std::move(line));
        }
    }

    frame.flash = std::max(frame.flash, clamp01(metrics.dropIntensity * 0.26f + flux * 0.14f + harmonic * 0.08f));
}

void addResonanceTessellation(GeometryFrame& frame,
                              const AudioMetrics& metrics,
                              const std::array<ColorRGBA, 5>& colors,
                              float width,
                              float height,
                              float drive,
                              float speed,
                              float intensity,
                              float quality,
                              double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const float beatPhase = metrics.beatPhase * 2.0f * kPi;
    const float harmonic = clamp01(metrics.harmonicEnergy * metrics.keyConfidence);
    const float flux = clamp01(metrics.spectralFlux * 1.45f);
    const float sectionLift = metrics.section == ArrangementSection::Build
                                  ? metrics.sectionProgress * metrics.sectionConfidence
                                  : (metrics.section == ArrangementSection::Drop ? metrics.sectionConfidence : 0.0f);
    const float rootPhase = metrics.keyIndex >= 0
                                ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f * 2.0f * kPi
                                : 0.0f;

    const int columns = scaledCount(14 + static_cast<int>(std::round((flux + metrics.dropIntensity + sectionLift) * 5.0f)),
                                    quality);
    const float aspect = std::clamp(height / std::max(width, 1.0f), 0.42f, 1.2f);
    const int rows = std::max(5, static_cast<int>(std::round(static_cast<float>(columns) * aspect * 0.72f)));
    const float cellWidth = width / static_cast<float>(std::max(1, columns));
    const float cellHeight = height / static_cast<float>(std::max(1, rows));
    const float cellRadius = std::min(cellWidth, cellHeight) * 0.58f;
    const int symmetry = 3 + (metrics.keyIndex >= 0 ? ((metrics.keyIndex % 5 + 5) % 5) : 2);

    for (int row = 0; row < rows; ++row) {
        const float rowUnit = (static_cast<float>(row) + 0.5f) / static_cast<float>(rows);
        for (int column = 0; column < columns; ++column) {
            const float columnUnit = (static_cast<float>(column) + 0.5f) / static_cast<float>(columns);
            const std::size_t spectralIndex = static_cast<std::size_t>(row * 7 + column * 5);
            const float spectral = spectrumAt(metrics, spectralIndex);
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(row + column + metrics.keyIndex + 12));
            const float onset = metrics.bandOnsets[static_cast<std::size_t>(row + column) % metrics.bandOnsets.size()];
            const float localEnergy = clamp01(spectral * 0.48f +
                                              chroma * metrics.harmonicEnergy * 0.24f +
                                              onset * 0.16f +
                                              flux * 0.08f +
                                              metrics.dropIntensity * 0.12f);
            const float checker = ((row + column) % 2 == 0) ? 1.0f : -1.0f;
            const float latticeWarp = std::sin(rowUnit * static_cast<float>(symmetry) * 2.0f * kPi +
                                               columnUnit * 2.0f * kPi +
                                               phase * (0.55f + spectral * 0.35f) +
                                               beatPhase + rootPhase) *
                                      cellRadius * (0.2f + localEnergy * 0.85f + metrics.stereoWidth * 0.38f);
            const float shear = std::cos(columnUnit * 5.0f * kPi +
                                         phase * 0.34f -
                                         rootPhase +
                                         checker * metrics.stereoWidth) *
                                cellRadius * (0.08f + harmonic * 0.34f + sectionLift * 0.22f);

            const Vec2 cellCenter{
                columnUnit * width + (row % 2 == 0 ? -0.18f : 0.18f) * cellWidth + shear,
                rowUnit * height + latticeWarp
            };

            Polyline tile;
            tile.closed = true;
            tile.strokeWidth = 0.46f + localEnergy * 3.2f + metrics.beatConfidence * 1.05f + sectionLift * 1.1f;
            tile.color = withAlpha(mix(colors[(row + column) % 4],
                                       colors[(row + column + 2) % 4],
                                       clamp01(chroma * metrics.harmonicEnergy + onset * 0.25f + flux * 0.2f)),
                                   0.08f + localEnergy * 0.3f + drive * 0.08f + harmonic * 0.08f);
            tile.points.reserve(3);

            const float tileRadius = cellRadius * (0.58f + localEnergy * 0.78f + metrics.dropIntensity * 0.24f);
            const float rotation = checker * 0.52f +
                                   phase * (0.12f + spectral * 0.16f) +
                                   rootPhase * 0.25f +
                                   beatPhase * (0.08f + metrics.beatConfidence * 0.08f);
            for (int vertex = 0; vertex < 3; ++vertex) {
                const float vertexUnit = static_cast<float>(vertex) / 3.0f;
                const float angle = rotation + vertexUnit * 2.0f * kPi;
                const float cornerWarp = std::sin(phase * 0.9f +
                                                  beatPhase +
                                                  vertexUnit * 2.0f * kPi +
                                                  localEnergy * 3.0f +
                                                  static_cast<float>(row - column) * 0.13f) *
                                         cellRadius * (0.05f + localEnergy * 0.24f + harmonic * 0.18f);
                tile.points.push_back(polar(cellCenter, tileRadius + cornerWarp, angle));
            }
            frame.polylines.push_back(std::move(tile));

            if (localEnergy > 0.38f || onset > 0.42f || metrics.dropIntensity > 0.48f) {
                frame.rings.push_back(Ring{
                    cellCenter,
                    cellRadius * (0.38f + localEnergy * 1.15f + metrics.dropIntensity * 0.42f),
                    3 + ((row + column + symmetry) % 6),
                    rotation + phase * 0.18f,
                    0.55f + localEnergy * 2.7f + metrics.beatConfidence * 1.5f,
                    withAlpha(colors[(row + column + 1) % 4], 0.11f + localEnergy * 0.28f + onset * 0.12f)
                });
            }

            if (onset > 0.22f || (metrics.beat && (row + column) % 5 == 0)) {
                frame.particles.push_back(Particle{
                    cellCenter,
                    1.25f + onset * 6.8f + localEnergy * 4.5f + metrics.beatConfidence * 2.4f,
                    withAlpha(colors[(row + column + 3) % 4], 0.16f + onset * 0.32f + localEnergy * 0.16f)
                });
            }
        }
    }

    const int faultLineCount = scaledCount(5 + static_cast<int>(std::round(sectionLift * 5.0f + harmonic * 3.0f)),
                                           quality);
    const int faultPoints = scaledCount(44, quality);
    for (int lineIndex = 0; lineIndex < faultLineCount; ++lineIndex) {
        const float lineUnit = static_cast<float>(lineIndex) / static_cast<float>(std::max(1, faultLineCount - 1));
        Polyline line;
        line.closed = false;
        line.strokeWidth = 0.75f + metrics.spectralFlux * 3.4f + sectionLift * 1.8f + harmonic * 1.1f;
        line.color = withAlpha(colors[(lineIndex + 2) % 4],
                               0.06f + metrics.spectralFlux * 0.22f + sectionLift * 0.18f + harmonic * 0.08f);
        line.points.reserve(static_cast<std::size_t>(faultPoints));
        for (int pointIndex = 0; pointIndex < faultPoints; ++pointIndex) {
            const float unit = static_cast<float>(pointIndex) / static_cast<float>(std::max(1, faultPoints - 1));
            const float spectral = spectrumAt(metrics, static_cast<std::size_t>(pointIndex * 3 + lineIndex * 11));
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(pointIndex + lineIndex + metrics.keyIndex + 12));
            const float diagonal = std::sin((unit + lineUnit) * static_cast<float>(symmetry) * kPi +
                                            phase * (0.72f + spectral * 0.3f) +
                                            beatPhase + rootPhase);
            line.points.push_back(Vec2{
                unit * width,
                height * (0.14f + lineUnit * 0.72f) +
                    diagonal * minimumDimension * (0.012f + metrics.spectralFlux * 0.04f + chroma * harmonic * 0.035f) +
                    std::sin(unit * 2.0f * kPi + phase * 0.2f + lineUnit * kPi) * sectionLift * minimumDimension * 0.04f
            });
        }
        frame.polylines.push_back(std::move(line));
    }

    const int rootGlyphs = scaledCount(12, quality);
    for (int glyph = 0; glyph < rootGlyphs; ++glyph) {
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(glyph));
        const float weight = clamp01(chroma * (0.35f + metrics.harmonicEnergy) + metrics.beatConfidence * 0.04f);
        if (weight <= 0.045f && harmonic <= 0.12f) {
            continue;
        }
        const float unit = static_cast<float>(glyph) / static_cast<float>(rootGlyphs);
        const float angle = unit * 2.0f * kPi + rootPhase + phase * 0.045f + beatPhase * 0.08f;
        const Vec2 glyphCenter = polar(center,
                                       minimumDimension * (0.12f + weight * 0.34f + metrics.dropIntensity * 0.08f),
                                       angle);
        frame.rings.push_back(Ring{
            glyphCenter,
            minimumDimension * (0.008f + weight * 0.025f + harmonic * 0.01f),
            3 + (glyph % 6),
            -angle + phase * 0.24f,
            0.55f + weight * 2.7f + metrics.keyConfidence * 0.9f,
            withAlpha(colors[glyph % 4], 0.08f + weight * 0.28f + harmonic * 0.12f)
        });
    }

    if (metrics.dropIntensity > 0.12f || metrics.section == ArrangementSection::Drop) {
        const int beamCount = scaledCount(16, quality);
        for (int beam = 0; beam < beamCount; ++beam) {
            const float unit = static_cast<float>(beam) / static_cast<float>(beamCount);
            const float spectral = spectrumAt(metrics, static_cast<std::size_t>(beam * 4));
            frame.beams.push_back(Beam{
                unit * 2.0f * kPi + rootPhase * 0.5f + beatPhase * 0.1f + phase * 0.05f,
                minimumDimension * (0.2f + metrics.dropIntensity * 0.58f + spectral * 0.26f * intensity),
                0.6f + metrics.dropIntensity * 5.2f + spectral * 2.0f,
                withAlpha(colors[(beam + 1) % 4], 0.045f + metrics.dropIntensity * 0.22f + spectral * 0.12f)
            });
        }
    }

    frame.flash = std::max(frame.flash,
                           clamp01(metrics.dropIntensity * 0.3f +
                                   metrics.spectralFlux * 0.18f +
                                   harmonic * 0.12f +
                                   sectionLift * 0.08f));
}

void addNeuralConstellation(GeometryFrame& frame,
                            const AudioMetrics& metrics,
                            const std::array<ColorRGBA, 5>& colors,
                            float width,
                            float height,
                            float drive,
                            float speed,
                            float intensity,
                            float quality,
                            double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const float beatOrbit = metrics.beatPhase * 2.0f * kPi;
    const float barOrbit = metrics.barPhase * 2.0f * kPi;
    const float harmonic = clamp01(metrics.harmonicEnergy * (0.45f + metrics.keyConfidence));
    const float flux = clamp01(metrics.spectralFlux * 1.35f + metrics.onset * 0.28f);
    const float rootPhase = metrics.keyIndex >= 0
                                ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f * 2.0f * kPi
                                : 0.0f;
    const float downbeatPulse = metrics.downbeat
                                    ? std::max(metrics.downbeatConfidence, metrics.barConfidence)
                                    : metrics.downbeatConfidence * 0.45f;
    const float sectionLift = metrics.section == ArrangementSection::Build
                                  ? metrics.sectionProgress * metrics.sectionConfidence
                                  : (metrics.section == ArrangementSection::Drop ? metrics.sectionConfidence : 0.0f);

    const int nodeCount = scaledCount(
        24 + static_cast<int>(std::round((flux + harmonic + metrics.dropIntensity + downbeatPulse) * 10.0f)),
        quality);
    std::vector<Vec2> nodes;
    std::vector<float> energies;
    nodes.reserve(static_cast<std::size_t>(nodeCount));
    energies.reserve(static_cast<std::size_t>(nodeCount));

    for (int i = 0; i < nodeCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, nodeCount));
        const std::size_t spectralIndex = static_cast<std::size_t>(i * 5);
        const float spectral = spectrumAt(metrics, spectralIndex);
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(i + metrics.keyIndex + 12));
        const float onset = metrics.bandOnsets[static_cast<std::size_t>(i) % metrics.bandOnsets.size()];
        const float localEnergy = clamp01(spectral * 0.48f +
                                          chroma * metrics.harmonicEnergy * 0.23f +
                                          onset * 0.15f +
                                          metrics.beatConfidence * 0.06f +
                                          metrics.dropIntensity * 0.08f);
        const float golden = static_cast<float>(i) * 2.39996323f;
        const float orbit = golden +
                            rootPhase * (0.45f + harmonic * 0.2f) +
                            barOrbit * (0.12f + metrics.barConfidence * 0.16f) +
                            phase * (0.045f + spectral * 0.06f);
        const float spiral = std::sqrt(unit);
        const float stereoBias = ((i % 2 == 0) ? -1.0f : 1.0f) * metrics.stereoWidth * minimumDimension * 0.12f;
        const float neuralJitter = std::sin(phase * (0.8f + chroma) +
                                            beatOrbit +
                                            static_cast<float>(i) * 0.71f) *
                                   minimumDimension * (0.012f + localEnergy * 0.035f + flux * 0.02f);
        const float radius = minimumDimension * (0.08f +
                                                 spiral * (0.38f + harmonic * 0.08f) +
                                                 localEnergy * 0.12f * intensity +
                                                 sectionLift * 0.04f);
        Vec2 node = polar(center, radius + neuralJitter, orbit);
        node.x += stereoBias * (0.45f + spectral);
        node.y += std::cos(phase * 0.42f + unit * 4.0f * kPi + barOrbit) *
                  minimumDimension * metrics.stereoWidth * 0.035f;
        node.x = std::clamp(node.x, -width * 0.08f, width * 1.08f);
        node.y = std::clamp(node.y, -height * 0.08f, height * 1.08f);
        nodes.push_back(node);
        energies.push_back(localEnergy);
    }

    const int connectionStride = quality < 0.7f ? 3 : 2;
    for (int i = 0; i < nodeCount; ++i) {
        const float sourceEnergy = energies[static_cast<std::size_t>(i)];
        for (int hop = 1; hop <= 3; ++hop) {
            const int target = (i + hop * connectionStride + (metrics.downbeat ? 1 : 0)) % nodeCount;
            const float targetEnergy = energies[static_cast<std::size_t>(target)];
            const float linkEnergy = clamp01((sourceEnergy + targetEnergy) * 0.5f +
                                             metrics.barConfidence * 0.12f +
                                             downbeatPulse * 0.08f +
                                             metrics.dropIntensity * 0.08f);
            if (linkEnergy < 0.13f && hop > 1 && metrics.dropIntensity < 0.25f) {
                continue;
            }

            const Vec2 a = nodes[static_cast<std::size_t>(i)];
            const Vec2 b = nodes[static_cast<std::size_t>(target)];
            const Vec2 midpoint{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            const float bendAngle = std::atan2(midpoint.y - center.y, midpoint.x - center.x) +
                                    kPi * 0.5f +
                                    std::sin(phase + static_cast<float>(i + target) * 0.17f + beatOrbit) * 0.38f;
            const float bend = minimumDimension * (0.006f + linkEnergy * 0.028f + harmonic * 0.014f);

            Polyline link;
            link.closed = false;
            link.strokeWidth = 0.42f + linkEnergy * 2.8f + downbeatPulse * 1.2f;
            link.color = withAlpha(mix(colors[(i + hop) % 4],
                                       colors[(target + 2) % 4],
                                       clamp01(harmonic + targetEnergy * 0.35f)),
                                   0.045f + linkEnergy * 0.24f + downbeatPulse * 0.06f + drive * 0.035f);
            link.points.reserve(3);
            link.points.push_back(a);
            link.points.push_back(Vec2{
                midpoint.x + std::cos(bendAngle) * bend,
                midpoint.y + std::sin(bendAngle) * bend
            });
            link.points.push_back(b);
            frame.polylines.push_back(std::move(link));
        }
    }

    for (int i = 0; i < nodeCount; ++i) {
        const float energy = energies[static_cast<std::size_t>(i)];
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(i + metrics.keyIndex + 12));
        const float onset = metrics.bandOnsets[static_cast<std::size_t>(i) % metrics.bandOnsets.size()];
        const float nodePulse = clamp01(energy + onset * 0.35f + downbeatPulse * 0.2f + harmonic * 0.12f);
        frame.particles.push_back(Particle{
            nodes[static_cast<std::size_t>(i)],
            1.5f + nodePulse * 8.8f + metrics.beatConfidence * 2.6f,
            withAlpha(mix(colors[i % 4], colors[4], clamp01(chroma * metrics.harmonicEnergy + nodePulse * 0.18f)),
                      0.18f + nodePulse * 0.46f + drive * 0.06f)
        });

        if (nodePulse > 0.32f || (metrics.beat && i % 5 == 0)) {
            frame.rings.push_back(Ring{
                nodes[static_cast<std::size_t>(i)],
                minimumDimension * (0.006f + nodePulse * 0.03f + downbeatPulse * 0.012f),
                5 + ((i + metrics.keyIndex + 12) % 8),
                phase * (0.28f + energy * 0.22f) + beatOrbit,
                0.55f + nodePulse * 2.8f + downbeatPulse * 1.2f,
                withAlpha(colors[(i + 1) % 4], 0.08f + nodePulse * 0.24f + downbeatPulse * 0.1f)
            });
        }
    }

    const int chromaCount = scaledCount(12, quality);
    for (int i = 0; i < chromaCount; ++i) {
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(i));
        const float harmonicWeight = clamp01(chroma * (0.3f + metrics.harmonicEnergy) + metrics.keyConfidence * 0.035f);
        if (harmonicWeight < 0.045f && harmonic < 0.12f) {
            continue;
        }
        const float unit = static_cast<float>(i) / static_cast<float>(chromaCount);
        const float angle = unit * 2.0f * kPi + rootPhase + phase * 0.035f + barOrbit * 0.08f;
        const Vec2 attractor = polar(center,
                                     minimumDimension * (0.16f + harmonicWeight * 0.24f + metrics.stereoWidth * 0.05f),
                                     angle);
        frame.rings.push_back(Ring{
            attractor,
            minimumDimension * (0.012f + harmonicWeight * 0.035f + downbeatPulse * 0.015f),
            6 + (i % 6),
            -angle + phase * 0.18f,
            0.55f + harmonicWeight * 3.0f + metrics.keyConfidence * 0.8f,
            withAlpha(colors[(i + 2) % 4], 0.08f + harmonicWeight * 0.3f + harmonic * 0.1f)
        });
        if (harmonicWeight > 0.16f) {
            Polyline spoke;
            spoke.closed = false;
            spoke.strokeWidth = 0.42f + harmonicWeight * 1.8f;
            spoke.color = withAlpha(colors[i % 4], 0.045f + harmonicWeight * 0.16f);
            spoke.points = {center, attractor};
            frame.polylines.push_back(std::move(spoke));
        }
    }

    if (metrics.barConfidence > 0.08f || downbeatPulse > 0.02f) {
        const int ringCount = scaledCount(metrics.downbeat ? 6 : 3, quality);
        for (int i = 0; i < ringCount; ++i) {
            const float layer = static_cast<float>(i + 1) / static_cast<float>(ringCount);
            frame.rings.push_back(Ring{
                center,
                minimumDimension * (0.11f + layer * 0.08f + metrics.barConfidence * 0.04f + downbeatPulse * 0.06f),
                metrics.downbeat ? 4 + (i % 4) : 48,
                barOrbit + phase * (0.06f + layer * 0.04f),
                0.55f + metrics.barConfidence * 1.8f + downbeatPulse * 4.0f,
                withAlpha(colors[(i + 3) % 4], 0.04f + metrics.barConfidence * 0.12f + downbeatPulse * 0.22f)
            });
        }
    }

    if (metrics.dropIntensity > 0.16f || metrics.section == ArrangementSection::Drop) {
        const int beams = scaledCount(18, quality);
        for (int i = 0; i < beams; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(beams);
            const float spectral = spectrumAt(metrics, static_cast<std::size_t>(i * 4));
            frame.beams.push_back(Beam{
                unit * 2.0f * kPi + rootPhase * 0.3f + barOrbit * 0.14f + phase * 0.05f,
                minimumDimension * (0.18f + metrics.dropIntensity * 0.52f + spectral * 0.24f * intensity),
                0.5f + metrics.dropIntensity * 5.0f + spectral * 2.2f,
                withAlpha(colors[(i + 1) % 4], 0.045f + metrics.dropIntensity * 0.2f + spectral * 0.14f)
            });
        }
    }

    frame.flash = std::max(frame.flash,
                           clamp01(downbeatPulse * 0.26f +
                                   metrics.dropIntensity * 0.26f +
                                   flux * 0.12f +
                                   harmonic * 0.1f));
}

void addSceneTransitionOverlay(GeometryFrame& frame,
                               const AudioMetrics& metrics,
                               const std::array<ColorRGBA, 5>& colors,
                               float width,
                               float height,
                               float speed,
                               float intensity,
                               float quality,
                               float transition,
                               float progress,
                               double time)
{
    transition = clamp01(transition);
    if (transition <= 0.001f) {
        return;
    }

    progress = clamp01(progress);
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float beatPhase = metrics.beatPhase * 2.0f * kPi;
    const float transitionPhase = progress * 2.0f * kPi;
    const float pulse = transition * (0.72f + metrics.beatConfidence * 0.22f + (metrics.beat ? 0.16f : 0.0f));
    const float swirl = static_cast<float>(time) * speed * (0.28f + pulse * 0.26f) + beatPhase;

    const int ringCount = scaledCount(7, quality);
    for (int i = 0; i < ringCount; ++i) {
        const float layer = static_cast<float>(i + 1) / static_cast<float>(ringCount);
        const float radius = minimumDimension * (0.1f + layer * 0.42f + pulse * 0.055f);
        frame.rings.push_back(Ring{
            center,
            radius,
            3 + ((i + static_cast<int>(metrics.keyIndex + 12)) % 7),
            swirl * (0.18f + layer * 0.28f) + transitionPhase,
            0.9f + pulse * 5.2f + layer * 1.4f,
            withAlpha(mix(colors[i % 4], colors[(i + 2) % 4], progress), 0.08f + pulse * 0.28f)
        });
    }

    const int beamCount = scaledCount(18, quality);
    for (int i = 0; i < beamCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(beamCount);
        const float harmonic = chromaAt(metrics, static_cast<std::size_t>(i)) * metrics.harmonicEnergy;
        const float angle = unit * 2.0f * kPi + swirl * 0.16f +
                            std::sin(transitionPhase + unit * 6.0f * kPi) * 0.12f;
        frame.beams.push_back(Beam{
            angle,
            minimumDimension * (0.18f + transition * 0.42f * intensity + harmonic * 0.18f),
            0.65f + transition * 4.4f + harmonic * 3.0f,
            withAlpha(colors[(i + 1) % 4], 0.05f + pulse * 0.22f + harmonic * 0.16f)
        });
    }

    const int ribbonCount = scaledCount(5, quality);
    const int points = scaledCount(72, quality);
    for (int ribbon = 0; ribbon < ribbonCount; ++ribbon) {
        Polyline line;
        line.closed = true;
        line.strokeWidth = 0.7f + transition * 3.4f;
        line.color = withAlpha(colors[(ribbon + 2) % 4], 0.08f + pulse * 0.26f);
        line.points.reserve(static_cast<std::size_t>(points));

        const float ribbonOffset = static_cast<float>(ribbon) / static_cast<float>(ribbonCount);
        for (int pointIndex = 0; pointIndex < points; ++pointIndex) {
            const float unit = static_cast<float>(pointIndex) / static_cast<float>(points);
            const float angle = unit * 2.0f * kPi + ribbonOffset * 2.0f * kPi + swirl * 0.08f;
            const float fold = std::sin(angle * (3.0f + ribbonOffset * 5.0f) + transitionPhase + beatPhase) *
                               minimumDimension * (0.018f + transition * 0.035f);
            const float radius = minimumDimension * (0.22f + ribbonOffset * 0.07f +
                                                     std::sin(unit * 2.0f * kPi + transitionPhase) * 0.035f +
                                                     metrics.spectralFlux * 0.05f);
            line.points.push_back(polar(center, radius + fold, angle));
        }

        frame.polylines.push_back(std::move(line));
    }

    frame.flash = std::max(frame.flash, clamp01(transition * 0.34f + metrics.dropIntensity * 0.12f));
}

void addArrangementSectionAccents(GeometryFrame& frame,
                                  const AudioMetrics& metrics,
                                  const std::array<ColorRGBA, 5>& colors,
                                  float width,
                                  float height,
                                  float speed,
                                  float intensity,
                                  float quality,
                                  double time)
{
    const float confidence = clamp01(metrics.sectionConfidence);
    if (confidence <= 0.04f || metrics.section == ArrangementSection::Silence) {
        return;
    }

    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float progress = clamp01(metrics.sectionProgress);
    const float phase = static_cast<float>(time) * speed;
    const float beatOrbit = metrics.beatPhase * 2.0f * kPi;

    switch (metrics.section) {
    case ArrangementSection::Silence:
        break;
    case ArrangementSection::Breakdown: {
        const int rings = scaledCount(4, quality);
        for (int i = 0; i < rings; ++i) {
            const float layer = static_cast<float>(i + 1) / static_cast<float>(rings);
            frame.rings.push_back(Ring{
                center,
                minimumDimension * (0.24f + layer * 0.12f + metrics.stereoWidth * 0.05f),
                96 + i * 24,
                -phase * (0.025f + layer * 0.018f),
                0.45f + confidence * 1.1f + metrics.harmonicEnergy * 0.9f,
                withAlpha(mix(colors[3], colors[0], layer), 0.035f + confidence * 0.085f)
            });
        }
        break;
    }
    case ArrangementSection::Build: {
        const int points = scaledCount(96, quality);
        Polyline spiral;
        spiral.closed = false;
        spiral.strokeWidth = 0.8f + confidence * 3.2f + progress * 1.4f;
        spiral.color = withAlpha(colors[2], 0.12f + confidence * 0.32f);
        spiral.points.reserve(static_cast<std::size_t>(points));
        for (int i = 0; i < points; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, points - 1));
            const float angle = unit * 5.5f * kPi + beatOrbit + phase * 0.12f;
            const float radius = minimumDimension * (0.08f + unit * (0.42f + progress * 0.12f));
            const float lift = std::sin(unit * kPi + progress * kPi) * metrics.spectralFlux * minimumDimension * 0.04f;
            spiral.points.push_back(polar(center, radius + lift, angle));
        }
        frame.polylines.push_back(std::move(spiral));

        const int fans = scaledCount(8, quality);
        for (int i = 0; i < fans; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(fans);
            frame.beams.push_back(Beam{
                unit * 2.0f * kPi + phase * 0.09f + progress * kPi,
                minimumDimension * (0.22f + progress * 0.34f + metrics.phraseIntensity * 0.16f),
                0.55f + confidence * 2.8f + metrics.spectralFlux * 2.2f,
                withAlpha(colors[(i + 2) % 4], 0.05f + confidence * 0.18f + progress * 0.08f)
            });
        }
        break;
    }
    case ArrangementSection::Drop: {
        const int rings = scaledCount(5, quality);
        for (int i = 0; i < rings; ++i) {
            const float layer = static_cast<float>(i + 1) / static_cast<float>(rings);
            frame.rings.push_back(Ring{
                center,
                minimumDimension * (0.14f + layer * 0.11f + metrics.dropIntensity * 0.12f),
                3 + ((i + (metrics.beat ? 1 : 0)) % 5),
                beatOrbit + phase * (0.22f + layer * 0.08f),
                1.2f + confidence * 5.0f + metrics.dropIntensity * 3.0f,
                withAlpha(colors[i % 4], 0.14f + confidence * 0.3f)
            });
        }

        const int particles = scaledCount(36, quality);
        for (int i = 0; i < particles; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(particles);
            const float energy = spectrumAt(metrics, static_cast<std::size_t>(i * 3));
            frame.particles.push_back(Particle{
                polar(center,
                      minimumDimension * (0.13f + metrics.dropIntensity * 0.24f + energy * 0.16f * intensity),
                      unit * 2.0f * kPi + beatOrbit),
                1.8f + confidence * 7.0f + energy * 5.0f,
                withAlpha(colors[(i + 1) % 4], 0.18f + confidence * 0.38f)
            });
        }
        frame.flash = std::max(frame.flash, clamp01(0.18f + confidence * 0.32f + metrics.dropIntensity * 0.18f));
        break;
    }
    case ArrangementSection::Groove: {
        if (confidence < 0.45f || metrics.beatConfidence < 0.24f) {
            break;
        }
        const int anchors = 4;
        for (int i = 0; i < anchors; ++i) {
            const float angle = static_cast<float>(i) / static_cast<float>(anchors) * 2.0f * kPi + beatOrbit;
            frame.rings.push_back(Ring{
                polar(center, minimumDimension * 0.32f, angle),
                8.0f + metrics.beatConfidence * 18.0f,
                4,
                -angle + phase * 0.08f,
                0.7f + confidence * 1.6f,
                withAlpha(colors[i % 4], 0.08f + confidence * 0.12f)
            });
        }
        break;
    }
    }
}

void addSyncAccents(GeometryFrame& frame,
                    const AudioMetrics& metrics,
                    const std::array<ColorRGBA, 5>& colors,
                    float width,
                    float height,
                    float speed,
                    float intensity,
                    float quality,
                    double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float beatOrbit = metrics.beatPhase * 2.0f * kPi;
    const float barOrbit = metrics.barPhase * 2.0f * kPi;
    const float phraseOrbit = metrics.phrasePhase * 2.0f * kPi;

    const int particleSpokes = quality < 0.7f ? 2 : 4;
    for (std::size_t band = 0; band < metrics.bandOnsets.size(); ++band) {
        const float onset = metrics.bandOnsets[band];
        if (onset < 0.045f) {
            continue;
        }

        const float bandT = static_cast<float>(band) / static_cast<float>(metrics.bandOnsets.size());
        const float angle = bandT * 2.0f * kPi + beatOrbit + static_cast<float>(time) * 0.08f * speed;
        const float radius = minimumDimension * (0.18f + bandT * 0.11f + onset * 0.1f * intensity);
        const Vec2 anchor = polar(center, radius, angle);
        const int sides = 3 + static_cast<int>(band) * 2;

        frame.rings.push_back(Ring{
            anchor,
            12.0f + onset * minimumDimension * 0.075f * intensity,
            sides,
            -angle + static_cast<float>(time) * speed,
            1.0f + onset * 5.0f,
            withAlpha(colors[(band + 1) % 4], 0.22f + onset * 0.52f)
        });

        for (int spoke = 0; spoke < particleSpokes; ++spoke) {
            const float spokeAngle = angle + static_cast<float>(spoke) * (2.0f * kPi / static_cast<float>(particleSpokes));
            frame.particles.push_back(Particle{
                polar(anchor, onset * minimumDimension * 0.055f * intensity, spokeAngle),
                1.8f + onset * 10.0f,
                withAlpha(colors[(band + spoke) % 4], 0.28f + onset * 0.58f)
            });
        }
    }

    if (metrics.barConfidence > 0.08f) {
        const float barPulse = metrics.downbeat
                                   ? std::max(metrics.downbeatConfidence, metrics.barConfidence)
                                   : metrics.downbeatConfidence * 0.55f;
        const int barRings = scaledCount(metrics.downbeat ? 4 : 2, quality);
        for (int i = 0; i < barRings; ++i) {
            const float layer = static_cast<float>(i + 1) / static_cast<float>(barRings);
            frame.rings.push_back(Ring{
                center,
                minimumDimension * (0.18f + layer * 0.13f + barPulse * 0.06f * intensity),
                metrics.downbeat ? 4 + (i % 3) : 32,
                barOrbit + static_cast<float>(i) * 0.22f + static_cast<float>(time) * speed * 0.04f,
                0.55f + metrics.barConfidence * 1.7f + barPulse * 4.2f,
                withAlpha(colors[(i + 3) % 4], 0.045f + metrics.barConfidence * 0.12f + barPulse * 0.26f)
            });
        }

        if (barPulse > 0.18f) {
            const int spokes = scaledCount(8, quality);
            for (int i = 0; i < spokes; ++i) {
                const float unit = static_cast<float>(i) / static_cast<float>(spokes);
                frame.beams.push_back(Beam{
                    unit * 2.0f * kPi + barOrbit * 0.12f,
                    minimumDimension * (0.22f + barPulse * 0.34f * intensity),
                    0.55f + barPulse * 4.0f,
                    withAlpha(colors[(i + 1) % 4], 0.035f + barPulse * 0.2f)
                });
            }
        }
    }

    if (metrics.phraseConfidence > 0.08f || metrics.buildTension > 0.06f) {
        const float phrasePulse = metrics.phraseBoundary
                                      ? std::max(metrics.phraseConfidence, metrics.downbeatConfidence)
                                      : metrics.phraseConfidence * (0.35f + metrics.buildTension * 0.45f);
        const int phraseRings = scaledCount(metrics.phraseBoundary ? 4 : 2, quality);
        for (int i = 0; i < phraseRings; ++i) {
            const float layer = static_cast<float>(i + 1) / static_cast<float>(phraseRings);
            frame.rings.push_back(Ring{
                center,
                minimumDimension * (0.28f + layer * 0.16f + metrics.buildTension * 0.055f * intensity),
                metrics.phraseBoundary ? 16 + (i * 4) : 64,
                phraseOrbit + static_cast<float>(i) * 0.18f + static_cast<float>(time) * speed * 0.025f,
                0.42f + metrics.phraseConfidence * 1.8f + metrics.buildTension * 2.4f + phrasePulse * 2.0f,
                withAlpha(colors[(i + 2) % 4],
                          0.035f + metrics.phraseConfidence * 0.08f + metrics.buildTension * 0.18f + phrasePulse * 0.12f)
            });
        }

        if (metrics.buildTension > 0.18f) {
            const int spokes = scaledCount(6 + static_cast<int>(metrics.buildTension * 8.0f), quality);
            for (int i = 0; i < spokes; ++i) {
                const float unit = static_cast<float>(i) / static_cast<float>(spokes);
                const float angle = unit * 2.0f * kPi + phraseOrbit + static_cast<float>(time) * speed * 0.05f;
                frame.beams.push_back(Beam{
                    angle,
                    minimumDimension * (0.2f + metrics.buildTension * 0.38f * intensity),
                    0.42f + metrics.buildTension * 4.2f,
                    withAlpha(colors[(i + 3) % 4], 0.025f + metrics.buildTension * 0.19f)
                });
            }
        }
    }

    if (metrics.dropIntensity > 0.05f) {
        const int dropRings = scaledCount(6, quality);
        for (int i = 0; i < dropRings; ++i) {
            const float layer = static_cast<float>(i + 1) / static_cast<float>(dropRings);
            frame.rings.push_back(Ring{
                center,
                minimumDimension * (0.12f + layer * 0.12f + metrics.dropIntensity * 0.08f),
                3 + (i % 4),
                beatOrbit + static_cast<float>(time) * speed * (0.18f + layer),
                1.2f + metrics.dropIntensity * 7.0f,
                withAlpha(colors[i % 4], 0.12f + metrics.dropIntensity * 0.42f)
            });
        }
    }

    if (metrics.phraseIntensity > 0.04f || metrics.buildTension > 0.08f) {
        Polyline phraseLine;
        phraseLine.closed = true;
        phraseLine.strokeWidth = 0.8f + metrics.phraseIntensity * 3.4f + metrics.buildTension * 2.2f;
        phraseLine.color = withAlpha(colors[2], 0.1f + metrics.phraseIntensity * 0.26f + metrics.buildTension * 0.18f);
        const int points = scaledCount(96, quality);
        phraseLine.points.reserve(static_cast<std::size_t>(points));
        for (int i = 0; i < points; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(points);
            const float angle = t * 2.0f * kPi + phraseOrbit * 0.45f + static_cast<float>(time) * speed * 0.06f;
            const float radius = minimumDimension * (0.36f + std::sin(angle * 5.0f + beatOrbit) *
                                                     (metrics.phraseIntensity * 0.04f + metrics.buildTension * 0.035f));
            phraseLine.points.push_back(polar(center, radius, angle));
        }
        frame.polylines.push_back(std::move(phraseLine));
    }
}

void addCymaticInterference(GeometryFrame& frame,
                            const AudioMetrics& metrics,
                            const std::array<ColorRGBA, 5>& colors,
                            float width,
                            float height,
                            float drive,
                            float speed,
                            float intensity,
                            float quality,
                            double time)
{
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    const float beatOrbit = metrics.beatPhase * 2.0f * kPi;
    const float barOrbit = metrics.barPhase * 2.0f * kPi;
    const float phraseOrbit = metrics.phrasePhase * 2.0f * kPi;
    const float keyRotation = metrics.keyIndex >= 0
                                  ? static_cast<float>((metrics.keyIndex % 12 + 12) % 12) / 12.0f * 2.0f * kPi
                                  : 0.0f;
    const float modalBend = metrics.keyMode == MusicalMode::Minor ? -0.38f :
                            (metrics.keyMode == MusicalMode::Major ? 0.22f : 0.0f);

    const int contourLayers = scaledCount(10, quality);
    const int contourPoints = scaledCount(210, quality);
    for (int layer = 0; layer < contourLayers; ++layer) {
        const float layerUnit = static_cast<float>(layer) / static_cast<float>(std::max(1, contourLayers - 1));
        const int harmonicA = 3 + (layer % 6) + static_cast<int>(std::round(metrics.bass * 3.0f));
        const int harmonicB = 5 + ((layer + 2) % 7) + static_cast<int>(std::round(metrics.treble * 4.0f));
        const float baseRadius = minimumDimension * (0.075f + layerUnit * 0.41f +
                                                     metrics.lowMid * 0.035f * intensity);

        Polyline contour;
        contour.closed = true;
        contour.strokeWidth = 0.55f + layerUnit * 1.15f + metrics.beatConfidence * 2.8f +
                              metrics.buildTension * 1.6f;
        contour.color = withAlpha(mix(colors[layer % 4],
                                      colors[(layer + 2) % 4],
                                      clamp01(metrics.harmonicEnergy + layerUnit * 0.25f)),
                                  0.12f + drive * 0.18f + metrics.phraseConfidence * 0.16f +
                                      metrics.dropIntensity * 0.16f);
        contour.points.reserve(static_cast<std::size_t>(contourPoints));

        for (int i = 0; i < contourPoints; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(contourPoints);
            const float angle = unit * 2.0f * kPi;
            const float spectrum = spectrumAt(metrics, static_cast<std::size_t>(i + layer * 9));
            const float chroma = chromaAt(metrics, static_cast<std::size_t>(i + layer + std::max(0, metrics.keyIndex)));
            const float nodalA = std::sin(angle * static_cast<float>(harmonicA) +
                                          phase * (0.42f + layerUnit * 0.42f) +
                                          beatOrbit * (0.35f + metrics.beatConfidence * 0.35f));
            const float nodalB = std::cos(angle * static_cast<float>(harmonicB) -
                                          phase * (0.26f + metrics.stereoWidth * 0.38f) +
                                          keyRotation + modalBend * metrics.keyConfidence);
            const float nodalC = std::sin((angle + phraseOrbit * 0.35f) *
                                          (2.0f + metrics.phraseConfidence * 4.0f) +
                                          barOrbit * metrics.barConfidence);
            const float interference = (nodalA * (0.56f + spectrum * 0.5f)) +
                                       (nodalB * (0.34f + chroma * 0.44f)) +
                                       (nodalC * (0.16f + metrics.buildTension * 0.34f));
            const float radius = baseRadius +
                                 interference * minimumDimension * (0.018f + spectrum * 0.07f) * intensity +
                                 metrics.dropIntensity * minimumDimension * 0.025f * (1.0f - layerUnit);
            contour.points.push_back(polar(center,
                                           std::max(4.0f, radius),
                                           angle + phase * 0.035f + metrics.stereoWidth * 0.06f * nodalB));
        }
        frame.polylines.push_back(std::move(contour));
    }

    const int spokeCount = scaledCount(24, quality);
    for (int i = 0; i < spokeCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(spokeCount);
        const float chroma = chromaAt(metrics, static_cast<std::size_t>(i));
        const float energy = spectrumAt(metrics, static_cast<std::size_t>(i * 3));
        if (energy < 0.045f && chroma < 0.045f && ((i % 3) != 0)) {
            continue;
        }
        const float angle = unit * 2.0f * kPi + keyRotation * 0.18f + phase * 0.065f;
        frame.beams.push_back(Beam{
            angle,
            minimumDimension * (0.18f + energy * 0.44f * intensity + chroma * 0.18f +
                                metrics.downbeatConfidence * 0.08f),
            0.5f + energy * 4.2f + chroma * 3.2f + metrics.beatConfidence * 1.8f,
            withAlpha(mix(colors[i % 4], colors[4], chroma),
                      0.08f + energy * 0.34f + chroma * 0.24f + metrics.downbeatConfidence * 0.12f)
        });
    }

    const int nodeCount = scaledCount(82, quality);
    for (int i = 0; i < nodeCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(nodeCount);
        const float ring = static_cast<float>((i % 7) + 1) / 7.0f;
        const float angle = unit * 2.0f * kPi * (3.0f + metrics.stereoWidth) +
                            phase * (0.18f + ring * 0.12f) + barOrbit * 0.18f;
        const float spectrum = spectrumAt(metrics, static_cast<std::size_t>(i * 5));
        const float onset = metrics.bandOnsets[i % metrics.bandOnsets.size()];
        const float nodalGate = std::abs(std::sin(angle * (2.0f + ring * 5.0f) + beatOrbit));
        if (nodalGate < 0.36f && onset < 0.08f && !metrics.beat) {
            continue;
        }
        const float radius = minimumDimension * (0.08f + ring * 0.42f +
                                                 spectrum * 0.06f * intensity +
                                                 metrics.buildTension * 0.025f);
        frame.particles.push_back(Particle{
            polar(center, radius, angle + nodalGate * 0.08f),
            1.5f + nodalGate * 4.5f + spectrum * 8.0f + onset * 9.0f +
                metrics.dropIntensity * 5.0f,
            withAlpha(colors[(i + static_cast<int>(ring * 7.0f)) % 4],
                      0.12f + nodalGate * 0.24f + spectrum * 0.34f + onset * 0.28f)
        });
    }

    const float accent = std::max({metrics.downbeatConfidence,
                                   metrics.phraseBoundary ? metrics.phraseConfidence : 0.0f,
                                   metrics.dropIntensity,
                                   metrics.buildTension * 0.72f});
    if (accent > 0.05f) {
        const int accentRings = scaledCount(4 + static_cast<int>(accent * 5.0f), quality);
        for (int i = 0; i < accentRings; ++i) {
            const float layer = static_cast<float>(i + 1) / static_cast<float>(accentRings);
            frame.rings.push_back(Ring{
                center,
                minimumDimension * (0.1f + layer * 0.12f + accent * 0.06f),
                24 + i * 8,
                phase * (0.16f + layer * 0.22f) + beatOrbit * 0.22f + phraseOrbit * 0.12f,
                0.8f + accent * 5.4f + metrics.beatConfidence * 1.2f,
                withAlpha(colors[(i + 1) % 4], 0.1f + accent * 0.32f)
            });
        }
    }
}

void addEnvironmentField(GeometryFrame& frame,
                         const AudioMetrics& metrics,
                         const std::array<ColorRGBA, 5>& colors,
                         const EnvironmentState& environment,
                         float width,
                         float height,
                         float speed,
                         float intensity,
                         float quality,
                         double time)
{
    const float drive = clamp01(0.32f + environment.ambient * 0.42f + environment.motion * 0.34f);
    if (!environment.enabled || drive <= 0.001f) {
        return;
    }

    const Vec2 center{width * 0.5f, height * 0.5f};
    const float minimumDimension = std::min(width, height);
    const float dayPhase = std::clamp(environment.timeOfDay, 0.0f, 1.0f) * 2.0f * kPi;
    const float orbitPhase = dayPhase + static_cast<float>(time) * speed * 0.035f;
    const float mood = 0.5f + 0.5f * std::sin(dayPhase - kPi * 0.5f);
    const ColorRGBA warm = mix(colors[1], colors[2], mood);
    const ColorRGBA cool = mix(colors[0], colors[3], 1.0f - mood);

    const int orbitCount = scaledCount(5, quality);
    for (int i = 0; i < orbitCount; ++i) {
        const float layer = static_cast<float>(i + 1) / static_cast<float>(orbitCount);
        frame.rings.push_back(Ring{
            center,
            minimumDimension * (0.28f + layer * 0.13f + environment.motion * 0.035f * intensity),
            48 + i * 16,
            orbitPhase * (0.16f + layer * 0.11f),
            0.55f + drive * 1.15f + metrics.phraseIntensity * 1.2f,
            withAlpha(mix(cool, warm, layer), 0.035f + drive * 0.08f + metrics.harmonicEnergy * 0.035f)
        });
    }

    const int beamCount = scaledCount(10, quality);
    for (int i = 0; i < beamCount; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(beamCount);
        const float angle = orbitPhase + unit * 2.0f * kPi + std::sin(dayPhase + unit * kPi) * 0.22f;
        frame.beams.push_back(Beam{
            angle,
            minimumDimension * (0.2f + drive * 0.22f * intensity + metrics.stereoWidth * 0.12f),
            0.45f + drive * 2.2f + environment.motion * 2.8f,
            withAlpha(mix(warm, cool, unit), 0.035f + drive * 0.12f)
        });
    }
}

void bendTowardInteraction(GeometryFrame& frame,
                           const InteractionState& interaction,
                           float width,
                           float height,
                           float intensity,
                           const std::array<ColorRGBA, 5>& colors)
{
    if (!interaction.enabled || !interaction.active) {
        return;
    }

    const Vec2 attractor{
        std::clamp(interaction.normalizedX, 0.0f, 1.0f) * width,
        std::clamp(interaction.normalizedY, 0.0f, 1.0f) * height
    };
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float rawStrength = 0.24f + interaction.velocity * 0.55f +
                              interaction.strength * 0.35f +
                              (interaction.pressed ? 0.75f : 0.0f);
    const float strength = std::clamp(rawStrength * intensity, 0.08f, 1.85f);
    const float radius = std::min(width, height) * (0.18f + strength * 0.16f);

    for (Ring& ring : frame.rings) {
        const float dx = attractor.x - ring.center.x;
        const float dy = attractor.y - ring.center.y;
        ring.center.x += dx * 0.015f * strength;
        ring.center.y += dy * 0.015f * strength;
        ring.rotation += strength * 0.025f;
    }

    for (Polyline& line : frame.polylines) {
        for (Vec2& pointValue : line.points) {
            const float dx = attractor.x - pointValue.x;
            const float dy = attractor.y - pointValue.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float influence = std::exp(-(distance * distance) / (radius * radius * 1.8f)) * strength;
            pointValue.x += dx * influence * 0.07f;
            pointValue.y += dy * influence * 0.07f;
        }
    }

    for (Particle& particle : frame.particles) {
        const float dx = attractor.x - particle.position.x;
        const float dy = attractor.y - particle.position.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance < radius * 1.6f) {
            const float influence = (1.0f - std::clamp(distance / (radius * 1.6f), 0.0f, 1.0f)) * strength;
            particle.position.x += dx * influence * 0.12f;
            particle.position.y += dy * influence * 0.12f;
            particle.radius *= 1.0f + influence * 0.2f;
        }
    }

    for (int i = 0; i < 5; ++i) {
        frame.rings.push_back(Ring{
            attractor,
            radius * (0.25f + static_cast<float>(i) * 0.18f),
            interaction.pressed ? 3 + i : 24 + i * 8,
            strength * 0.45f + static_cast<float>(i) * 0.2f,
            1.0f + strength * 2.0f,
            withAlpha(colors[(i + 1) % 4], 0.18f + strength * 0.14f)
        });
    }

    for (int i = 0; i < 12; ++i) {
        const float angle = static_cast<float>(i) / 12.0f * 2.0f * kPi + strength;
        frame.beams.push_back(Beam{
            std::atan2(attractor.y - center.y, attractor.x - center.x) + angle * 0.06f,
            std::hypot(attractor.x - center.x, attractor.y - center.y) + radius * 0.22f,
            0.8f + strength * 2.2f,
            withAlpha(colors[i % 4], 0.1f + strength * 0.2f)
        });
    }
}

} // namespace

std::string_view toString(VisualMode mode)
{
    switch (mode) {
    case VisualMode::QuantumTunnel:
        return "Quantum Tunnel";
    case VisualMode::TechnoMandala:
        return "Techno Mandala";
    case VisualMode::LissajousMesh:
        return "Lissajous Mesh";
    case VisualMode::FrequencyBloom:
        return "Frequency Bloom";
    case VisualMode::FractalCathedral:
        return "Fractal Cathedral";
    case VisualMode::PolyrhythmLattice:
        return "Polyrhythm Lattice";
    case VisualMode::SpectralOrigami:
        return "Spectral Origami";
    case VisualMode::ChromaKaleidoscope:
        return "Chroma Kaleidoscope";
    case VisualMode::HyperspacePolytope:
        return "Hyperspace Polytope";
    case VisualMode::PhaseWeave:
        return "Phase Weave";
    case VisualMode::ResonanceTessellation:
        return "Resonance Tessellation";
    case VisualMode::NeuralConstellation:
        return "Neural Constellation";
    case VisualMode::CymaticInterference:
        return "Cymatic Interference";
    }
    return "Unknown";
}

std::string_view toString(Palette palette)
{
    switch (palette) {
    case Palette::NeonVoltage:
        return "Neon Voltage";
    case Palette::InfraredChrome:
        return "Infrared Chrome";
    case Palette::AcidAurora:
        return "Acid Aurora";
    case Palette::MonochromeLaser:
        return "Monochrome Laser";
    case Palette::OceanicPulse:
        return "Oceanic Pulse";
    }
    return "Unknown";
}

std::string_view toString(MotionStyle style)
{
    switch (style) {
    case MotionStyle::Smooth:
        return "Smooth";
    case MotionStyle::Mechanical:
        return "Mechanical";
    case MotionStyle::Liquid:
        return "Liquid";
    case MotionStyle::Hyperspace:
        return "Hyperspace";
    case MotionStyle::HeavyBass:
        return "Heavy Bass";
    case MotionStyle::AmbientDrift:
        return "Ambient Drift";
    case MotionStyle::Breakbeat:
        return "Breakbeat";
    }
    return "Unknown";
}

std::string_view toString(SceneIntent intent)
{
    switch (intent) {
    case SceneIntent::Calm:
        return "Calm";
    case SceneIntent::Groove:
        return "Groove";
    case SceneIntent::Tension:
        return "Tension";
    case SceneIntent::Drop:
        return "Drop";
    case SceneIntent::Release:
        return "Release";
    case SceneIntent::Melodic:
        return "Melodic";
    case SceneIntent::Industrial:
        return "Industrial";
    case SceneIntent::Dark:
        return "Dark";
    case SceneIntent::Bright:
        return "Bright";
    case SceneIntent::Chaotic:
        return "Chaotic";
    case SceneIntent::Spacious:
        return "Spacious";
    case SceneIntent::Heavy:
        return "Heavy";
    case SceneIntent::Minimal:
        return "Minimal";
    }
    return "Unknown";
}

ColorRGBA hsv(float hue, float saturation, float value, float alpha)
{
    hue = hue - std::floor(hue);
    saturation = clamp01(saturation);
    value = clamp01(value);
    const float scaled = hue * 6.0f;
    const int sector = static_cast<int>(std::floor(scaled));
    const float fraction = scaled - static_cast<float>(sector);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - saturation * fraction);
    const float t = value * (1.0f - saturation * (1.0f - fraction));

    switch (sector % 6) {
    case 0:
        return ColorRGBA{value, t, p, alpha};
    case 1:
        return ColorRGBA{q, value, p, alpha};
    case 2:
        return ColorRGBA{p, value, t, alpha};
    case 3:
        return ColorRGBA{p, q, value, alpha};
    case 4:
        return ColorRGBA{t, p, value, alpha};
    default:
        return ColorRGBA{value, p, q, alpha};
    }
}

GeometryFrame VisualizerEngine::buildFrame(const AudioMetrics& metrics,
                                           const VisualSettings& settings,
                                           float width,
                                           float height,
                                           double timeSeconds) const
{
    return buildFrame(metrics, settings, InteractionState{}, EnvironmentState{}, width, height, timeSeconds);
}

GeometryFrame VisualizerEngine::buildFrame(const AudioMetrics& metrics,
                                           const VisualSettings& settings,
                                           const InteractionState& interaction,
                                           float width,
                                           float height,
                                           double timeSeconds) const
{
    return buildFrame(metrics, settings, interaction, EnvironmentState{}, width, height, timeSeconds);
}

GeometryFrame VisualizerEngine::buildFrame(const AudioMetrics& metrics,
                                           const VisualSettings& settings,
                                           const InteractionState& interaction,
                                           const EnvironmentState& environment,
                                           float width,
                                           float height,
                                           double timeSeconds) const
{
    GeometryFrame frame;
    const AudioMetrics visualMetrics = stabilizedMetrics(metrics, settings);
    const auto colors = personalityPalette(paletteColors(settings.palette), settings, visualMetrics);
    const float intensity = std::clamp(settings.intensity, 0.15f, 4.0f);
    const float speed = std::clamp(settings.speed, 0.1f, 4.0f) *
                        (0.84f + (1.0f - motionStabilityOf(settings)) * 0.16f);
    const float quality = qualityOf(settings);
    const float density = quality * complexityOf(settings);
    const float environmentStrength = environmentDrive(settings, environment);
    const float drive = clamp01((visualMetrics.rms * 2.0f) +
                                (visualMetrics.bass * 0.85f) +
                                visualMetrics.beatConfidence +
                                visualMetrics.spectralFlux * 0.35f +
                                visualMetrics.dropIntensity * 0.4f +
                                environmentStrength * 0.16f);
    const float idlePulse = 0.5f + 0.5f * std::sin(static_cast<float>(timeSeconds) * 0.9f * speed);
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float tonalLift = visualMetrics.harmonicEnergy * visualMetrics.keyConfidence;
    const ColorRGBA backgroundTint = mix(colors[4],
                                         colors[0],
                                         0.18f + visualMetrics.spectralCentroid * 0.1f + environmentStrength * 0.08f);

    const SceneInterpretation frameIntent = interpretSceneIntent(visualMetrics, settings);
    const MusicSceneArc3D songArc = updateSongArcMemory(songArc_, visualMetrics, settings, frameIntent, timeSeconds);
    const ColorRGBA arcTint = mix(mix(colors[1], colors[2], songArc.impact),
                                  mix(colors[3], colors[4], songArc.recovery),
                                  songArc.recovery);
    frame.background = mix(ColorRGBA{0.006f, 0.008f, 0.013f, 1.0f},
                           withAlpha(backgroundTint, 1.0f),
                           0.08f + visualMetrics.spectralCentroid * 0.1f + drive * 0.08f +
                               tonalLift * 0.035f + environmentStrength * 0.045f);
    frame.background = mix(frame.background,
                           withAlpha(arcTint, 1.0f),
                           songArc.anticipation * 0.026f + songArc.impact * 0.045f + songArc.recovery * 0.032f);
    frame.flash = std::max(std::max(visualMetrics.beat ? clamp01(0.16f + visualMetrics.beatConfidence * 0.46f) : 0.0f,
                                    clamp01(visualMetrics.dropIntensity * 0.26f)),
                           clamp01(songArc.impact * 0.16f + songArc.anticipation * 0.045f));
    frame.sceneIntent = frameIntent.primary;
    frame.sceneIntentName = toString(frameIntent.primary);

    const float gridRadius = std::hypot(width, height) * 0.42f;
    const int gridCount = scaledCount(12, density);
    for (int i = 0; i < gridCount; ++i) {
        const float ringDrive = drive > 0.001f ? drive : idlePulse * 0.15f;
        frame.rings.push_back(Ring{
            center,
            (static_cast<float>(i + 1) / static_cast<float>(gridCount)) * gridRadius * (0.92f + ringDrive * 0.14f),
            64,
            static_cast<float>(timeSeconds) * 0.02f * speed,
            0.55f,
            withAlpha(colors[i % 4], 0.035f + ringDrive * 0.05f)
        });
    }

    switch (settings.mode) {
    case VisualMode::QuantumTunnel:
        addQuantumTunnel(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::TechnoMandala:
        addTechnoMandala(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::LissajousMesh:
        addLissajousMesh(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::FrequencyBloom:
        addFrequencyBloom(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::FractalCathedral:
        addFractalCathedral(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::PolyrhythmLattice:
        addPolyrhythmLattice(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::SpectralOrigami:
        addSpectralOrigami(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::ChromaKaleidoscope:
        addChromaKaleidoscope(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::HyperspacePolytope:
        addHyperspacePolytope(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::PhaseWeave:
        addPhaseWeave(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::ResonanceTessellation:
        addResonanceTessellation(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::NeuralConstellation:
        addNeuralConstellation(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::CymaticInterference:
        addCymaticInterference(frame, visualMetrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    }

    addSceneTransitionOverlay(frame,
                              visualMetrics,
                              colors,
                              width,
                              height,
                              speed,
                              intensity,
                              density,
                              settings.sceneTransition,
                              settings.sceneTransitionProgress,
                              timeSeconds);
    addSyncAccents(frame, visualMetrics, colors, width, height, speed, intensity, density, timeSeconds);
    addArrangementSectionAccents(frame, visualMetrics, colors, width, height, speed, intensity, density, timeSeconds);
    if (environmentStrength > 0.0f) {
        addEnvironmentField(frame, visualMetrics, colors, environment, width, height, speed, intensity, density, timeSeconds);
    }

    if (settings.interactiveField) {
        bendTowardInteraction(frame, interaction, width, height, intensity, colors);
    }

    applyDepthCues(frame, visualMetrics, settings, colors, width, height, speed, timeSeconds);
    frame.authored2DPrimitiveCount = primitiveFootprint(frame);
    frame.authored2DVisualWeight = primitiveVisualWeight(frame);
    suppressScreenSpaceLayerFor3D(frame, visualMetrics, settings);
    frame.retained2DPrimitiveCount = primitiveFootprint(frame);
    frame.retained2DVisualWeight = primitiveVisualWeight(frame);
    frame.retained2DPrimitiveRatio = frame.authored2DPrimitiveCount > 0
                                         ? static_cast<float>(frame.retained2DPrimitiveCount) /
                                               static_cast<float>(frame.authored2DPrimitiveCount)
                                         : 0.0f;
    frame.retained2DVisualRatio = frame.authored2DVisualWeight > 0.0f
                                      ? frame.retained2DVisualWeight / frame.authored2DVisualWeight
                                      : 0.0f;
    addObject3DScene(frame, visualMetrics, settings, interaction, environment, songArc, colors, width, height, speed, intensity, density, timeSeconds);
    frame.threeDDominance = frame.projected3DVisualWeight / std::max(1.0f, frame.retained2DVisualWeight);

    return frame;
}

} // namespace viz
