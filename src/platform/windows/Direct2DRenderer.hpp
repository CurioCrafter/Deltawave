#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Visualizer/Audio/AudioAnalyzer.hpp"
#include "Visualizer/Performance/FramePerformanceTracker.hpp"
#include "Visualizer/UI/ControlPanel.hpp"
#include "Visualizer/UI/RuntimeInspector.hpp"
#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include <string>

namespace viz {

class Direct2DRenderer {
public:
    Direct2DRenderer() = default;
    ~Direct2DRenderer();

    Direct2DRenderer(const Direct2DRenderer&) = delete;
    Direct2DRenderer& operator=(const Direct2DRenderer&) = delete;

    bool initialize(HWND window, std::wstring& error);
    void resize(UINT width, UINT height);
    void render(const GeometryFrame& frame,
                const AudioMetrics& metrics,
                const VisualSettings& settings,
                const PerformanceStats& performance,
                const RuntimeInspectorState& inspector,
                const std::wstring& status,
                bool recording,
                std::size_t recordedFrames);

private:
    HWND window_ = nullptr;
    ID2D1Factory* factory_ = nullptr;
    IDWriteFactory* writeFactory_ = nullptr;
    ID2D1HwndRenderTarget* renderTarget_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    IDWriteTextFormat* textFormat_ = nullptr;
    bool hasPresentedFrame_ = false;

    bool createDeviceResources(std::wstring& error);
    void discardDeviceResources();
    void drawRing(const Ring& ring);
    void drawPolyline(const Polyline& line);
    void drawBeam(const Beam& beam);
    void drawParticle(const Particle& particle);
    void drawHud(const AudioMetrics& metrics,
                 const VisualSettings& settings,
                 const PerformanceStats& performance,
                 const RuntimeInspectorState& inspector,
                 const std::wstring& status,
                 bool recording,
                 std::size_t recordedFrames);
    void drawControlPanel(const VisualSettings& settings, bool recording);
    void drawRuntimeInspector(const RuntimeInspectorState& inspector);
};

} // namespace viz
