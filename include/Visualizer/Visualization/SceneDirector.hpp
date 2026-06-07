#pragma once

#include "Visualizer/Visualization/VisualizerEngine.hpp"

namespace viz {

class SceneDirector {
public:
    void reset();

    [[nodiscard]] VisualSettings resolve(const VisualSettings& base,
                                         const AudioMetrics& metrics,
                                         double timeSeconds);

private:
    bool initialized_ = false;
    double lastTimeSeconds_ = 0.0;
    double lastModeSwitchSeconds_ = -100.0;
    double transitionStartSeconds_ = -100.0;
    double transitionDurationSeconds_ = 0.0;
    VisualMode currentMode_ = VisualMode::QuantumTunnel;
    Palette currentPalette_ = Palette::NeonVoltage;
    float transitionStrength_ = 0.0f;
    float smoothedHueShift_ = 0.0f;
    float smoothedDepth3D_ = 0.55f;
    float smoothedColorImpact_ = 0.65f;
    float smoothedIntensity_ = 1.0f;
    float smoothedSpeed_ = 1.0f;

    void initialize(const VisualSettings& base, double timeSeconds);
};

} // namespace viz
