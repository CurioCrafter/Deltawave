#include "Visualizer/UI/ControlPanel.hpp"

#include <algorithm>

namespace viz {
namespace {

bool contains(RectF rect, float x, float y)
{
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void addItem(ControlPanelLayout& layout,
             PanelControl control,
             float x,
             float y,
             float width,
             float height,
             std::string label,
             bool active = false,
             bool slider = false,
             float value = 0.0f)
{
    layout.items.push_back(PanelItem{
        control,
        RectF{x, y, x + width, y + height},
        std::move(label),
        active,
        slider,
        std::clamp(value, 0.0f, 1.0f)
    });
}

float settingToUnit(float value, float minimum, float maximum)
{
    return std::clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
}

} // namespace

ControlPanelLayout buildControlPanelLayout(float width,
                                           float height,
                                           const VisualSettings& settings,
                                           bool recording)
{
    ControlPanelLayout layout;
    const float panelWidth = std::clamp(width * 0.24f, 292.0f, 350.0f);
    const float panelLeft = 16.0f;
    const float panelTop = 184.0f;
    const float itemHeight = 19.0f;
    const float gap = 1.0f;
    const float innerLeft = panelLeft + 14.0f;
    const float innerWidth = panelWidth - 28.0f;
    float y = panelTop + 16.0f;

    const auto advance = [&]() { y += itemHeight + gap; };
    const auto addFull = [&](PanelControl control, std::string label, bool active = false) {
        addItem(layout, control, innerLeft, y, innerWidth, itemHeight, std::move(label), active);
        advance();
    };
    const auto addPair = [&](PanelControl leftControl,
                             std::string leftLabel,
                             bool leftActive,
                             PanelControl rightControl,
                             std::string rightLabel,
                             bool rightActive) {
        const float half = (innerWidth - gap) * 0.5f;
        addItem(layout, leftControl, innerLeft, y, half, itemHeight, std::move(leftLabel), leftActive);
        addItem(layout, rightControl, innerLeft + half + gap, y, half, itemHeight, std::move(rightLabel), rightActive);
        advance();
    };
    const auto addTriple = [&](PanelControl firstControl,
                               std::string firstLabel,
                               bool firstActive,
                               PanelControl secondControl,
                               std::string secondLabel,
                               bool secondActive,
                               PanelControl thirdControl,
                               std::string thirdLabel,
                               bool thirdActive) {
        const float third = (innerWidth - gap * 2.0f) / 3.0f;
        addItem(layout, firstControl, innerLeft, y, third, itemHeight, std::move(firstLabel), firstActive);
        addItem(layout, secondControl, innerLeft + third + gap, y, third, itemHeight, std::move(secondLabel), secondActive);
        addItem(layout,
                thirdControl,
                innerLeft + (third + gap) * 2.0f,
                y,
                third,
                itemHeight,
                std::move(thirdLabel),
                thirdActive);
        advance();
    };

    addPair(PanelControl::OpenAudio, "Open Audio", false, PanelControl::Loopback, "Loopback", false);
    addFull(PanelControl::ResetAudioProfiles, "Reset Audio AI", false);
    addPair(PanelControl::SavePreset, "Save", false, PanelControl::LoadPreset, "Load", false);
    addPair(PanelControl::CuratedPresetPrevious, "Look <", false, PanelControl::CuratedPresetNext, "Look >", false);
    addTriple(PanelControl::UserPresetPrevious,
              "User <",
              false,
              PanelControl::UserPresetNext,
              "User >",
              false,
              PanelControl::SaveUserPreset,
              "Save User",
              false);
    addFull(PanelControl::Record, recording ? "Stop Recording" : "Record Frames", recording);

    y += 6.0f;
    addTriple(PanelControl::ModeQuantumTunnel,
              "Tunnel",
              settings.mode == VisualMode::QuantumTunnel,
              PanelControl::ModeTechnoMandala,
              "Mandala",
              settings.mode == VisualMode::TechnoMandala,
              PanelControl::ModeLissajousMesh,
              "Mesh",
              settings.mode == VisualMode::LissajousMesh);
    addTriple(PanelControl::ModeFrequencyBloom,
              "Bloom",
              settings.mode == VisualMode::FrequencyBloom,
              PanelControl::ModeFractalCathedral,
              "Cathed",
              settings.mode == VisualMode::FractalCathedral,
              PanelControl::ModePolyrhythmLattice,
              "Lattice",
              settings.mode == VisualMode::PolyrhythmLattice);
    addTriple(PanelControl::ModeSpectralOrigami,
              "Origami",
              settings.mode == VisualMode::SpectralOrigami,
              PanelControl::ModeChromaKaleidoscope,
              "Kaleid",
              settings.mode == VisualMode::ChromaKaleidoscope,
              PanelControl::ModeHyperspacePolytope,
              "4D",
              settings.mode == VisualMode::HyperspacePolytope);
    addTriple(PanelControl::ModePhaseWeave,
              "Phase",
              settings.mode == VisualMode::PhaseWeave,
              PanelControl::ModeResonanceTessellation,
              "Tess",
              settings.mode == VisualMode::ResonanceTessellation,
              PanelControl::ModeNeuralConstellation,
              "Neural",
              settings.mode == VisualMode::NeuralConstellation);
    addFull(PanelControl::ModeCymaticInterference,
            "Cymatic",
            settings.mode == VisualMode::CymaticInterference);

    y += 6.0f;
    addPair(PanelControl::PaletteNeonVoltage,
            "Neon",
            settings.palette == Palette::NeonVoltage,
            PanelControl::PaletteInfraredChrome,
            "Infrared",
            settings.palette == Palette::InfraredChrome);
    addPair(PanelControl::PaletteAcidAurora,
            "Acid",
            settings.palette == Palette::AcidAurora,
            PanelControl::PaletteMonochromeLaser,
            "Mono",
            settings.palette == Palette::MonochromeLaser);
    addFull(PanelControl::PaletteOceanicPulse, "Oceanic", settings.palette == Palette::OceanicPulse);

    y += 6.0f;
    addItem(layout,
            PanelControl::IntensitySlider,
            innerLeft,
            y,
            innerWidth,
            itemHeight,
            "Intensity",
            false,
            true,
            settingToUnit(settings.intensity, 0.15f, 4.0f));
    advance();
    addItem(layout,
            PanelControl::SpeedSlider,
            innerLeft,
            y,
            innerWidth,
            itemHeight,
            "Speed",
            false,
            true,
            settingToUnit(settings.speed, 0.1f, 4.0f));
    advance();
    addItem(layout,
            PanelControl::HueShiftSlider,
            innerLeft,
            y,
            innerWidth,
            itemHeight,
            "Hue Shift",
            false,
            true,
            std::clamp(settings.hueShift, 0.0f, 1.0f));
    advance();
    addItem(layout,
            PanelControl::DepthSlider,
            innerLeft,
            y,
            innerWidth,
            itemHeight,
            "Depth 3D",
            false,
            true,
            std::clamp(settings.depth3D, 0.0f, 1.0f));
    advance();
    addItem(layout,
            PanelControl::ColorImpactSlider,
            innerLeft,
            y,
            innerWidth,
            itemHeight,
            "Color Impact",
            false,
            true,
            std::clamp(settings.colorImpact, 0.0f, 1.0f));
    advance();
    addItem(layout,
            PanelControl::ComplexitySlider,
            innerLeft,
            y,
            innerWidth,
            itemHeight,
            "Complexity",
            false,
            true,
            settingToUnit(settings.complexity, 0.35f, 1.8f));
    advance();
    addItem(layout,
            PanelControl::QualitySlider,
            innerLeft,
            y,
            innerWidth,
            itemHeight,
            "Quality",
            settings.adaptiveQuality,
            true,
            settingToUnit(settings.qualityScale, 0.45f, 1.0f));
    advance();

    y += 6.0f;
    addPair(PanelControl::ToggleInteraction,
            "Interact",
            settings.interactiveField,
            PanelControl::ToggleAutoScene,
            "Auto Scene",
            settings.autoScene);
    addPair(PanelControl::ToggleAdaptiveQuality,
            "Auto Quality",
            settings.adaptiveQuality,
            PanelControl::ToggleTrails,
            "Trails",
            settings.trails);
    addPair(PanelControl::ToggleEnvironment,
            "Env",
            settings.environmentReactive,
            PanelControl::ToggleHud,
            "HUD",
            settings.showHud);

    layout.panel = RectF{
        panelLeft,
        panelTop,
        panelLeft + panelWidth,
        std::min(height - 16.0f, y + 16.0f)
    };
    return layout;
}

PanelItem hitTestControlPanel(const ControlPanelLayout& layout, float x, float y)
{
    if (!contains(layout.panel, x, y)) {
        return {};
    }

    for (const PanelItem& item : layout.items) {
        if (contains(item.rect, x, y)) {
            return item;
        }
    }
    return {};
}

float normalizedSliderValue(const PanelItem& item, float x)
{
    const float width = std::max(1.0f, item.rect.right - item.rect.left);
    return std::clamp((x - item.rect.left) / width, 0.0f, 1.0f);
}

} // namespace viz
