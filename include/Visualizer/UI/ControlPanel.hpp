#pragma once

#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <string>
#include <vector>

namespace viz {

struct RectF {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

enum class PanelControl {
    None,
    OpenAudio,
    Loopback,
    ResetAudioProfiles,
    SavePreset,
    LoadPreset,
    UserPresetPrevious,
    UserPresetNext,
    SaveUserPreset,
    CuratedPresetPrevious,
    CuratedPresetNext,
    Record,
    ModeQuantumTunnel,
    ModeTechnoMandala,
    ModeLissajousMesh,
    ModeFrequencyBloom,
    ModeFractalCathedral,
    ModePolyrhythmLattice,
    ModeSpectralOrigami,
    ModeChromaKaleidoscope,
    ModeHyperspacePolytope,
    ModePhaseWeave,
    ModeResonanceTessellation,
    ModeNeuralConstellation,
    ModeCymaticInterference,
    PaletteNeonVoltage,
    PaletteInfraredChrome,
    PaletteAcidAurora,
    PaletteMonochromeLaser,
    PaletteOceanicPulse,
    IntensitySlider,
    SpeedSlider,
    HueShiftSlider,
    DepthSlider,
    ObjectDensitySlider,
    InteractionDepthSlider,
    LightingGlowSlider,
    ScenePersonalitySlider,
    Response3DSlider,
    ColorImpactSlider,
    ComplexitySlider,
    QualitySlider,
    ToggleHud,
    ToggleInteraction,
    ToggleEnvironment,
    ToggleAdaptiveQuality,
    ToggleAutoScene,
    ToggleTrails
};

struct PanelItem {
    PanelControl control = PanelControl::None;
    RectF rect{};
    std::string label;
    bool active = false;
    bool slider = false;
    float value = 0.0f;
};

struct ControlPanelLayout {
    RectF panel{};
    std::vector<PanelItem> items;
};

ControlPanelLayout buildControlPanelLayout(float width,
                                           float height,
                                           const VisualSettings& settings,
                                           bool recording);

PanelItem hitTestControlPanel(const ControlPanelLayout& layout, float x, float y);
float normalizedSliderValue(const PanelItem& item, float x);

} // namespace viz
