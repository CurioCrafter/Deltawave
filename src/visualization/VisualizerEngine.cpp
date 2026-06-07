#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

enum class Scene3DProfile {
    TechnoMachine,
    CrystalStorm,
    NeuralSpace,
    DimensionalTunnel,
    CymaticSculpture
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

Camera3D makeCamera3D(const VisualSettings& settings,
                      const AudioMetrics& metrics,
                      float width,
                      float height,
                      float speed,
                      double time)
{
    const float depth = depth3DOf(settings);
    const float minimumDimension = std::min(width, height);
    const float phase = static_cast<float>(time) * speed;
    return Camera3D{
        Vec2{width * 0.5f, height * 0.5f},
        minimumDimension * (0.82f + depth * 0.82f),
        minimumDimension * (1.05f + depth * 1.85f + metrics.bass * 0.35f + metrics.dropIntensity * 0.4f),
        std::sin(phase * 0.22f + metrics.phrasePhase * kPi) * (0.12f + metrics.stereoWidth * 0.14f + depth * 0.1f),
        std::cos(phase * 0.17f + metrics.buildTension) * (0.07f + metrics.phraseIntensity * 0.08f)
    };
}

Projected3D projectPoint3D(Vec3 world, const Camera3D& camera)
{
    Vec3 cameraPoint = world;
    cameraPoint = rotate3D(cameraPoint, Vec3{-camera.pitch, -camera.yaw, 0.0f});
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
    frame.polylines.push_back(Polyline{{pa.point, pb.point}, width, color, closed});
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
    frame.polylines.push_back(Polyline{
        std::move(projected),
        strokeWidth * std::clamp(perspective, 0.22f, 2.8f),
        color,
        closed
    });
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
    const auto world = [&](Vec3 local) { return objectLocalToWorld(object, local); };

    if (object.kind == Object3DKind::TunnelRib) {
        const int sides = std::max(4, static_cast<int>(std::round(5.0f + object.scale.z * 8.0f)));
        std::vector<Vec3> points;
        points.reserve(static_cast<std::size_t>(sides));
        for (int i = 0; i < sides; ++i) {
            const float unit = static_cast<float>(i) / static_cast<float>(sides);
            const float angle = unit * 2.0f * kPi;
            points.push_back(world(Vec3{std::cos(angle), std::sin(angle), 0.0f}));
        }
        addProjectedPolyline(frame, camera, points, color, stroke, true);
        return;
    }

    if (object.kind == Object3DKind::Shard) {
        const std::vector<Vec3> points = {
            world(Vec3{0.0f, -1.0f, 0.0f}),
            world(Vec3{0.58f, 0.05f, 0.28f}),
            world(Vec3{0.0f, 1.0f, 0.0f}),
            world(Vec3{-0.42f, 0.1f, -0.34f}),
            world(Vec3{0.0f, -1.0f, 0.0f})
        };
        addProjectedPolyline(frame, camera, points, color, stroke, false);
        addProjectedLine(frame, camera, points[1], points[3], withAlpha(color, color.a * 0.72f), stroke * 0.72f);
        return;
    }

    if (object.kind == Object3DKind::Plate) {
        const int ridges = 4;
        for (int i = -ridges; i <= ridges; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(ridges);
            const float ridge = std::sin((u + object.rotation.z) * kPi * 2.0f) * 0.12f;
            addProjectedLine(frame,
                             camera,
                             world(Vec3{-1.0f, u, ridge}),
                             world(Vec3{1.0f, u, -ridge}),
                             withAlpha(color, color.a * (0.55f + std::fabs(u) * 0.2f)),
                             stroke * 0.55f);
        }
        addProjectedPolyline(frame,
                             camera,
                             {world(Vec3{-1.0f, -1.0f, 0.0f}),
                              world(Vec3{1.0f, -1.0f, 0.0f}),
                              world(Vec3{1.0f, 1.0f, 0.0f}),
                              world(Vec3{-1.0f, 1.0f, 0.0f})},
                             color,
                             stroke,
                             true);
        return;
    }

    if (object.kind == Object3DKind::Node || object.kind == Object3DKind::Particle) {
        const Projected3D projected = projectPoint3D(object.position, camera);
        if (!projected.visible) {
            return;
        }
        frame.particles.push_back(Particle{
            projected.point,
            std::max(1.0f, object.scale.x * projected.perspective * (object.kind == Object3DKind::Node ? 1.8f : 1.0f)),
            color
        });
        if (object.kind == Object3DKind::Node) {
            frame.rings.push_back(Ring{
                projected.point,
                std::max(2.0f, object.scale.x * projected.perspective * 2.4f),
                18,
                object.rotation.z,
                stroke * 0.7f,
                withAlpha(color, color.a * 0.48f)
            });
        }
        return;
    }

    if (object.kind == Object3DKind::Link) {
        addProjectedLine(frame,
                         camera,
                         object.position,
                         object.target,
                         withAlpha(color, color.a * 0.72f),
                         0.7f + object.glow * 1.2f);
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
        addProjectedPolyline(frame, camera, points, color, stroke, false);
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
    for (const auto& edge : edges) {
        addProjectedLine(frame, camera, vertices[edge[0]], vertices[edge[1]], color, stroke);
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
    const float radius = std::min(width, height) * (0.18f + depthStrength * 0.18f + interaction.velocity * 0.04f);
    const float clickBoost = interaction.pressed ? 1.0f : 0.45f;
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
        const float lift = influence * depthStrength * (140.0f + interaction.velocity * 48.0f) * clickBoost;
        object.position.z -= lift * (0.75f + ripple * 0.25f);
        object.velocity.z = -lift;
        object.rotation.x += influence * depthStrength * (0.28f + clickBoost * 0.22f);
        object.rotation.y -= influence * depthStrength * 0.22f;
        object.glow = std::min(1.8f, object.glow + influence * (0.55f + clickBoost * 0.45f));
        object.scale = add(object.scale, Vec3{influence * 5.0f, influence * 5.0f, influence * 5.0f});
    }
}

void addTechnoMachineObjects(std::vector<Object3D>& objects,
                             const AudioMetrics& metrics,
                             const std::array<ColorRGBA, 5>& colors,
                             float minimumDimension,
                             float density,
                             float intensity,
                             float personality,
                             double time)
{
    const float phase = static_cast<float>(time);
    const int ribs = scaledCount(10, density);
    for (int i = 0; i < ribs; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(ribs);
        const float z = unit * minimumDimension * (2.2f + metrics.dropIntensity * 0.7f) - minimumDimension * 0.55f;
        objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                       Vec3{0.0f, 0.0f, z},
                                       Vec3{minimumDimension * (0.22f + unit * 0.12f + metrics.bass * 0.035f),
                                            minimumDimension * (0.14f + unit * 0.08f + metrics.bass * 0.025f),
                                            0.5f + unit},
                                       Vec3{0.0f, 0.0f, phase * (0.35f + unit) + metrics.beatPhase * kPi},
                                       withAlpha(colors[i % 4], 0.28f + metrics.beatConfidence * 0.22f),
                                       0.34f + metrics.bass * 0.8f));
    }

    const int machines = scaledCount(12, density * (0.8f + personality * 0.55f));
    for (int i = 0; i < machines; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(machines);
        const float angle = unit * 2.0f * kPi + phase * 0.42f;
        const float radius = minimumDimension * (0.18f + metrics.stereoWidth * 0.12f + personality * 0.08f);
        const float size = minimumDimension * (0.035f + metrics.bass * 0.018f + intensity * 0.004f);
        objects.push_back(makeObject3D(Object3DKind::Polyhedron,
                                       Vec3{std::cos(angle) * radius,
                                            std::sin(angle * 1.3f) * radius * 0.55f,
                                            minimumDimension * (0.18f + std::sin(angle + phase) * 0.25f)},
                                       Vec3{size * (1.0f + metrics.bass), size, size * (0.8f + metrics.lowMid)},
                                       Vec3{phase * (0.9f + unit),
                                            angle + metrics.beatConfidence,
                                            -phase * (0.5f + personality)},
                                       withAlpha(colors[(i + 1) % 4], 0.42f + metrics.dropIntensity * 0.24f),
                                       0.42f + metrics.beatConfidence * 0.42f + metrics.bass * 0.28f));
    }
}

void addCrystalStormObjects(std::vector<Object3D>& objects,
                            const AudioMetrics& metrics,
                            const std::array<ColorRGBA, 5>& colors,
                            float minimumDimension,
                            float density,
                            float personality,
                            double time)
{
    const float phase = static_cast<float>(time);
    const int shards = scaledCount(26, density * (0.75f + metrics.treble * 0.65f));
    for (int i = 0; i < shards; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(shards);
        const float angle = unit * 2.3999632f + phase * (0.22f + metrics.spectralFlux * 0.12f);
        const float radius = minimumDimension * (0.08f + unit * 0.52f + metrics.stereoWidth * 0.12f);
        const float z = minimumDimension * (std::sin(unit * kPi * 3.0f + phase) * 0.52f + metrics.dropIntensity * 0.46f);
        const float size = minimumDimension * (0.026f + spectrumAt(metrics, i) * 0.038f + metrics.treble * 0.012f);
        objects.push_back(makeObject3D(Object3DKind::Shard,
                                       Vec3{std::cos(angle) * radius,
                                            std::sin(angle * 1.17f) * radius * 0.64f,
                                            z},
                                       Vec3{size * (0.8f + personality), size * (1.8f + metrics.treble), size},
                                       Vec3{angle + phase,
                                            phase * (0.7f + unit) + metrics.harmonicEnergy,
                                            angle * 0.4f},
                                       withAlpha(colors[(i + 2) % 4], 0.34f + metrics.treble * 0.34f),
                                       0.34f + metrics.treble * 0.86f + metrics.harmonicEnergy * 0.3f));
    }
}

void addNeuralSpaceObjects(std::vector<Object3D>& objects,
                           const AudioMetrics& metrics,
                           const std::array<ColorRGBA, 5>& colors,
                           float minimumDimension,
                           float density,
                           float personality,
                           double time)
{
    const float phase = static_cast<float>(time);
    const int nodes = scaledCount(18, density * (0.85f + metrics.barConfidence * 0.45f));
    const std::size_t firstNode = objects.size();
    for (int i = 0; i < nodes; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(nodes);
        const float chroma = chromaAt(metrics, i);
        const float angle = unit * 2.0f * kPi + metrics.barPhase * kPi * 0.55f;
        const float layer = static_cast<float>(i % 5) / 4.0f;
        const float radius = minimumDimension * (0.12f + layer * 0.12f + metrics.stereoWidth * 0.12f);
        objects.push_back(makeObject3D(Object3DKind::Node,
                                       Vec3{std::cos(angle + phase * 0.12f) * radius,
                                            std::sin(angle * 1.41f + phase * 0.1f) * radius * 0.72f,
                                            minimumDimension * (-0.25f + layer * 0.32f + chroma * 0.36f)},
                                       Vec3{minimumDimension * (0.012f + chroma * 0.018f + metrics.beatConfidence * 0.008f),
                                            minimumDimension * (0.012f + chroma * 0.018f),
                                            minimumDimension * 0.012f},
                                       Vec3{phase * 0.3f, angle, metrics.phrasePhase * kPi},
                                       withAlpha(colors[i % 4], 0.36f + metrics.barConfidence * 0.28f),
                                       0.3f + metrics.downbeatConfidence * 0.8f + chroma * 0.4f));
    }
    for (int i = 0; i < nodes; ++i) {
        const Object3D& a = objects[firstNode + static_cast<std::size_t>(i)];
        const Object3D& b = objects[firstNode + static_cast<std::size_t>((i * 3 + 5) % nodes)];
        Object3D link = makeObject3D(Object3DKind::Link,
                                     a.position,
                                     Vec3{1.0f, 1.0f, 1.0f},
                                     Vec3{},
                                     withAlpha(colors[(i + 1) % 4], 0.18f + metrics.phraseIntensity * 0.2f),
                                     0.18f + metrics.barConfidence * 0.42f + personality * 0.18f);
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
                                 double time)
{
    const float phase = static_cast<float>(time);
    const int layers = scaledCount(18, density * (0.75f + metrics.dropIntensity * 0.5f));
    for (int i = 0; i < layers; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(layers);
        const float z = std::fmod(unit * minimumDimension * 2.8f - phase * minimumDimension * (0.18f + metrics.dropIntensity * 0.38f),
                                  minimumDimension * 2.8f);
        const float radius = minimumDimension * (0.12f + unit * 0.36f + metrics.stereoWidth * 0.08f);
        objects.push_back(makeObject3D(Object3DKind::TunnelRib,
                                       Vec3{std::sin(phase * 0.21f + unit * kPi) * minimumDimension * 0.04f,
                                            std::cos(phase * 0.17f + unit * kPi) * minimumDimension * 0.03f,
                                            z - minimumDimension * 0.45f},
                                       Vec3{radius, radius * (0.62f + metrics.phraseIntensity * 0.22f), 0.45f + unit * 0.65f},
                                       Vec3{metrics.phrasePhase * 0.35f, phase * 0.08f, phase * (0.38f + personality * 0.25f) + unit * kPi},
                                       withAlpha(colors[i % 4], 0.22f + unit * 0.22f),
                                       0.28f + metrics.dropIntensity * 0.72f));
        if (i % 3 == 0) {
            const float angle = unit * 2.0f * kPi + phase;
            objects.push_back(makeObject3D(Object3DKind::Polyhedron,
                                           Vec3{std::cos(angle) * radius * 0.74f,
                                                std::sin(angle) * radius * 0.42f,
                                                z},
                                           Vec3{minimumDimension * 0.026f,
                                                minimumDimension * (0.026f + metrics.bass * 0.02f),
                                                minimumDimension * 0.026f},
                                           Vec3{phase + angle, phase * 0.7f, angle},
                                           withAlpha(colors[(i + 2) % 4], 0.32f + metrics.beatConfidence * 0.24f),
                                           0.24f + metrics.beatConfidence * 0.54f));
        }
    }
}

void addCymaticSculptureObjects(std::vector<Object3D>& objects,
                                const AudioMetrics& metrics,
                                const std::array<ColorRGBA, 5>& colors,
                                float minimumDimension,
                                float density,
                                float personality,
                                double time)
{
    const float phase = static_cast<float>(time);
    const int plates = scaledCount(5, density);
    for (int i = 0; i < plates; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(std::max(1, plates - 1));
        const float z = minimumDimension * (-0.24f + unit * 0.42f + metrics.harmonicEnergy * 0.12f);
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
                                       0.32f + metrics.harmonicEnergy * 0.74f + metrics.buildTension * 0.32f));
    }

    const int nodal = scaledCount(20, density * (0.72f + metrics.harmonicEnergy * 0.46f));
    for (int i = 0; i < nodal; ++i) {
        const float unit = static_cast<float>(i) / static_cast<float>(nodal);
        const float angle = unit * 2.0f * kPi + phase * 0.18f;
        const float harmonic = chromaAt(metrics, i + 2U);
        const float radius = minimumDimension * (0.08f + harmonic * 0.38f + metrics.buildTension * 0.08f);
        objects.push_back(makeObject3D(Object3DKind::Particle,
                                       Vec3{std::cos(angle) * radius,
                                            std::sin(angle * 1.8f) * radius * 0.62f,
                                            minimumDimension * (std::sin(angle * 2.0f + phase) * 0.22f + harmonic * 0.25f)},
                                       Vec3{minimumDimension * (0.01f + harmonic * 0.02f),
                                            minimumDimension * (0.01f + harmonic * 0.02f),
                                            minimumDimension * 0.01f},
                                       Vec3{},
                                       withAlpha(colors[i % 4], 0.25f + harmonic * 0.36f),
                                       0.22f + harmonic * 0.72f));
    }
}

void addObject3DScene(GeometryFrame& frame,
                      const AudioMetrics& metrics,
                      const VisualSettings& settings,
                      const InteractionState& interaction,
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
    const float objectDensity = std::clamp(quality * (0.42f + objectDensity3DOf(settings) * 1.08f), 0.18f, 1.8f);
    const float personality = scenePersonalityOf(settings);
    const Camera3D camera = makeCamera3D(settings, metrics, width, height, speed, time);
    std::vector<Object3D> objects;
    objects.reserve(160);

    switch (profile) {
    case Scene3DProfile::TechnoMachine:
        addTechnoMachineObjects(objects, metrics, colors, minimumDimension, objectDensity, intensity, personality, time);
        break;
    case Scene3DProfile::CrystalStorm:
        addCrystalStormObjects(objects, metrics, colors, minimumDimension, objectDensity, personality, time);
        break;
    case Scene3DProfile::NeuralSpace:
        addNeuralSpaceObjects(objects, metrics, colors, minimumDimension, objectDensity, personality, time);
        break;
    case Scene3DProfile::DimensionalTunnel:
        addDimensionalTunnelObjects(objects, metrics, colors, minimumDimension, objectDensity, personality, time);
        break;
    case Scene3DProfile::CymaticSculpture:
        addCymaticSculptureObjects(objects, metrics, colors, minimumDimension, objectDensity, personality, time);
        break;
    }

    applyObjectInteraction3D(objects, interaction, settings, camera, width, height, static_cast<float>(time));

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
    const float lightingGlow = lightingGlowOf(settings);
    for (const Object3D& object : objects) {
        const float depthUnit = std::clamp((object.depth - minimumDepth) / range, 0.0f, 1.0f);
        renderWireObject3D(frame, camera, object, depthUnit, lightingGlow);
    }

    frame.objects3D = std::move(objects);
    frame.scene3DName = scene3DName(profile);
    frame.cameraDepth = camera.cameraDistance;
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

        const int particles = scaledCount(20, quality);
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
    const auto colors = personalityPalette(paletteColors(settings.palette), settings, metrics);
    const float intensity = std::clamp(settings.intensity, 0.15f, 4.0f);
    const float speed = std::clamp(settings.speed, 0.1f, 4.0f);
    const float quality = qualityOf(settings);
    const float density = quality * complexityOf(settings);
    const float environmentStrength = environmentDrive(settings, environment);
    const float drive = clamp01((metrics.rms * 2.0f) +
                                (metrics.bass * 0.85f) +
                                metrics.beatConfidence +
                                metrics.spectralFlux * 0.35f +
                                metrics.dropIntensity * 0.4f +
                                environmentStrength * 0.16f);
    const float idlePulse = 0.5f + 0.5f * std::sin(static_cast<float>(timeSeconds) * 0.9f * speed);
    const Vec2 center{width * 0.5f, height * 0.5f};
    const float tonalLift = metrics.harmonicEnergy * metrics.keyConfidence;
    const ColorRGBA backgroundTint = mix(colors[4],
                                         colors[0],
                                         0.18f + metrics.spectralCentroid * 0.1f + environmentStrength * 0.08f);

    frame.background = mix(ColorRGBA{0.006f, 0.008f, 0.013f, 1.0f},
                           withAlpha(backgroundTint, 1.0f),
                           0.08f + metrics.spectralCentroid * 0.1f + drive * 0.08f +
                               tonalLift * 0.035f + environmentStrength * 0.045f);
    frame.flash = std::max(metrics.beat ? clamp01(0.18f + metrics.beatConfidence * 0.55f) : 0.0f,
                           clamp01(metrics.dropIntensity * 0.32f));

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
        addQuantumTunnel(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::TechnoMandala:
        addTechnoMandala(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::LissajousMesh:
        addLissajousMesh(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::FrequencyBloom:
        addFrequencyBloom(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::FractalCathedral:
        addFractalCathedral(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::PolyrhythmLattice:
        addPolyrhythmLattice(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::SpectralOrigami:
        addSpectralOrigami(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::ChromaKaleidoscope:
        addChromaKaleidoscope(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::HyperspacePolytope:
        addHyperspacePolytope(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::PhaseWeave:
        addPhaseWeave(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::ResonanceTessellation:
        addResonanceTessellation(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::NeuralConstellation:
        addNeuralConstellation(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    case VisualMode::CymaticInterference:
        addCymaticInterference(frame, metrics, colors, width, height, drive, speed, intensity, density, timeSeconds);
        break;
    }

    addSceneTransitionOverlay(frame,
                              metrics,
                              colors,
                              width,
                              height,
                              speed,
                              intensity,
                              density,
                              settings.sceneTransition,
                              settings.sceneTransitionProgress,
                              timeSeconds);
    addSyncAccents(frame, metrics, colors, width, height, speed, intensity, density, timeSeconds);
    addArrangementSectionAccents(frame, metrics, colors, width, height, speed, intensity, density, timeSeconds);
    if (environmentStrength > 0.0f) {
        addEnvironmentField(frame, metrics, colors, environment, width, height, speed, intensity, density, timeSeconds);
    }

    if (settings.interactiveField) {
        bendTowardInteraction(frame, interaction, width, height, intensity, colors);
    }

    applyDepthCues(frame, metrics, settings, colors, width, height, speed, timeSeconds);
    addObject3DScene(frame, metrics, settings, interaction, colors, width, height, speed, intensity, density, timeSeconds);

    return frame;
}

} // namespace viz
