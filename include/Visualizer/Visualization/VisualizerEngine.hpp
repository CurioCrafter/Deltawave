#pragma once

#include "Visualizer/Audio/AudioAnalyzer.hpp"

#include <string_view>
#include <vector>

namespace viz {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ColorRGBA {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Ring {
    Vec2 center{};
    float radius = 0.0f;
    int sides = 64;
    float rotation = 0.0f;
    float strokeWidth = 1.0f;
    ColorRGBA color{};
};

struct Beam {
    float angle = 0.0f;
    float length = 0.0f;
    float width = 1.0f;
    ColorRGBA color{};
};

struct Particle {
    Vec2 position{};
    float radius = 1.0f;
    ColorRGBA color{};
};

struct Polyline {
    std::vector<Vec2> points;
    float strokeWidth = 1.0f;
    ColorRGBA color{};
    bool closed = false;
};

enum class Object3DKind {
    Polyhedron,
    Shard,
    Ribbon,
    Node,
    Link,
    Plate,
    TunnelRib,
    Particle,
    DepthPlane,
    Column,
    Cage,
    WaveSurface,
    Orbiter,
    Anchor
};

struct Object3D {
    Object3DKind kind = Object3DKind::Polyhedron;
    Vec3 position{};
    Vec3 rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Vec3 velocity{};
    Vec3 target{};
    float depth = 0.0f;
    float glow = 0.0f;
    ColorRGBA color{};
};

struct GeometryFrame {
    ColorRGBA background{0.01f, 0.012f, 0.018f, 1.0f};
    std::vector<Ring> rings;
    std::vector<Beam> beams;
    std::vector<Particle> particles;
    std::vector<Polyline> polylines;
    std::vector<Object3D> objects3D;
    std::string_view scene3DName = "Flat Geometry";
    float cameraDepth = 0.0f;
    float objectDepthRange = 0.0f;
    float flash = 0.0f;
};

struct InteractionState {
    bool enabled = true;
    bool active = false;
    bool pressed = false;
    float normalizedX = 0.5f;
    float normalizedY = 0.5f;
    float velocity = 0.0f;
    float strength = 0.0f;
};

struct EnvironmentState {
    bool enabled = false;
    float timeOfDay = 0.5f;
    float motion = 0.0f;
    float ambient = 0.5f;
};

enum class VisualMode {
    QuantumTunnel,
    TechnoMandala,
    LissajousMesh,
    FrequencyBloom,
    FractalCathedral,
    PolyrhythmLattice,
    SpectralOrigami,
    ChromaKaleidoscope,
    HyperspacePolytope,
    PhaseWeave,
    ResonanceTessellation,
    NeuralConstellation,
    CymaticInterference
};

enum class Palette {
    NeonVoltage,
    InfraredChrome,
    AcidAurora,
    MonochromeLaser,
    OceanicPulse
};

enum class MotionStyle {
    Smooth,
    Mechanical,
    Liquid,
    Hyperspace,
    HeavyBass,
    AmbientDrift,
    Breakbeat
};

struct VisualSettings {
    VisualMode mode = VisualMode::QuantumTunnel;
    Palette palette = Palette::NeonVoltage;
    MotionStyle motionStyle = MotionStyle::Liquid;
    float hueShift = 0.0f;
    float depth3D = 0.55f;
    float colorImpact = 0.65f;
    float objectDensity3D = 0.65f;
    float interactionDepth = 0.65f;
    float lightingGlow = 0.62f;
    float scenePersonality = 0.5f;
    float response3D = 0.88f;
    float motionStability = 0.72f;
    float patternClarity = 0.78f;
    float complexity = 1.0f;
    float intensity = 1.0f;
    float speed = 1.0f;
    float sceneTransition = 0.0f;
    float sceneTransitionProgress = 1.0f;
    float qualityScale = 1.0f;
    bool trails = true;
    bool showHud = true;
    bool interactiveField = true;
    bool environmentReactive = true;
    bool adaptiveQuality = true;
    bool autoScene = false;
};

std::string_view toString(VisualMode mode);
std::string_view toString(Palette palette);
std::string_view toString(MotionStyle style);
ColorRGBA hsv(float hue, float saturation, float value, float alpha = 1.0f);

class VisualizerEngine {
public:
    GeometryFrame buildFrame(const AudioMetrics& metrics,
                             const VisualSettings& settings,
                             float width,
                             float height,
                             double timeSeconds) const;

    GeometryFrame buildFrame(const AudioMetrics& metrics,
                             const VisualSettings& settings,
                             const InteractionState& interaction,
                             float width,
                             float height,
                             double timeSeconds) const;

    GeometryFrame buildFrame(const AudioMetrics& metrics,
                             const VisualSettings& settings,
                             const InteractionState& interaction,
                             const EnvironmentState& environment,
                             float width,
                             float height,
                             double timeSeconds) const;
};

} // namespace viz
