#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Direct2DRenderer.hpp"
#include "WasapiLoopbackCapture.hpp"
#include "Visualizer/Audio/AudioAnalyzer.hpp"
#include "Visualizer/Audio/AudioFileLoader.hpp"
#include "Visualizer/Export/AnalysisTimeline.hpp"
#include "Visualizer/Export/CapturePackage.hpp"
#include "Visualizer/Performance/FramePerformanceTracker.hpp"
#include "Visualizer/UI/ControlPanel.hpp"
#include "Visualizer/Visualization/FrameRecorder.hpp"
#include "Visualizer/Visualization/PresetStore.hpp"
#include "Visualizer/Visualization/SceneDirector.hpp"
#include "Visualizer/Visualization/VisualizerEngine.hpp"

#include <windows.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace viz {

class WindowsApp {
public:
    explicit WindowsApp(HINSTANCE instance);
    ~WindowsApp();

    int run(int showCommand);

private:
    enum class SourceMode {
        Loopback,
        AudioFile
    };

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    bool running_ = true;
    bool fullscreen_ = false;
    RECT windowedRect_{};
    DWORD windowedStyle_ = 0;
    DWORD windowedExStyle_ = 0;

    Direct2DRenderer renderer_;
    WasapiLoopbackCapture loopback_;
    AudioAnalyzer analyzer_;
    VisualizerEngine engine_;
    SceneDirector sceneDirector_;
    VisualSettings settings_;
    InteractionState interaction_;
    FramePerformanceTracker performanceTracker_;
    PerformanceStats performanceStats_;
    FrameRecorder recorder_;
    AnalysisTimelineWriter captureTimeline_;
    CapturePackage capturePackage_;

    SourceMode sourceMode_ = SourceMode::Loopback;
    std::optional<WavAudio> audio_;
    std::filesystem::path audioPath_;
    std::chrono::steady_clock::time_point appStart_;
    std::chrono::steady_clock::time_point audioStart_;
    std::chrono::steady_clock::time_point lastMouseTime_;
    std::chrono::steady_clock::time_point recordingStart_;
    std::wstring status_;
    std::string activeLookName_;
    POINT lastMouse_{};
    bool trackingMouse_ = false;
    bool mediaAliasOpen_ = false;
    bool capturePackageActive_ = false;
    bool captureTimelineActive_ = false;
    bool curatedPresetApplied_ = false;
    std::size_t curatedPresetIndex_ = 0;
    std::filesystem::path userPresetDirectory_;
    std::vector<PresetLibraryEntry> userPresetLibrary_;
    bool userPresetApplied_ = false;
    std::size_t userPresetIndex_ = 0;
    PanelControl activeSlider_ = PanelControl::None;
    std::filesystem::path activeStyleProfilePath_;
    std::filesystem::path activeSyncProfilePath_;

    bool createMainWindow(int showCommand);
    void messageLoop();
    void updateAndRender();
    void analyzeLoopback(double appTimeSeconds);
    void analyzeAudioFile(double appTimeSeconds);
    void setActiveAudioProfiles(std::string_view stem);
    bool loadAdaptiveStyleProfile();
    void saveAdaptiveStyleProfile();
    void openAudioDialog();
    void loadAudioFileFromPath(const std::filesystem::path& path);
    bool startFilePlayback(const std::filesystem::path& path);
    void stopFilePlayback();
    void savePresetDialog();
    void loadPresetDialog();
    void applyVisualPreset(const VisualPreset& preset);
    void applyCuratedPreset(int direction);
    void refreshUserPresetLibrary();
    void applyUserPreset(int direction);
    void saveCurrentUserPreset();
    void markCustomLook();
    void resetCurrentAudioProfiles();
    void useLoopback();
    void toggleRecording();
    void beginCapturePackage(int width, int height);
    void updateCapturePackage(const AudioMetrics& metrics,
                              const VisualSettings& renderSettings,
                              const GeometryFrame& geometry,
                              int primitiveCount);
    void updateCapturePerformance(const PerformanceStats& performance);
    void encodeCaptureVideo();
    bool finishCapturePackage(std::string& error);
    [[nodiscard]] RuntimeInspectorState runtimeInspectorState() const;
    void toggleFullscreen();
    void toggleInteractiveField();
    void toggleEnvironmentReactive();
    void toggleAdaptiveQuality();
    void toggleAutoScene();
    void toggleTrails();
    void cyclePalette();
    void setMode(VisualMode mode);
    void adjustIntensity(float delta);
    void adjustSpeed(float delta);
    void adjustHueShift(float delta);
    void adjustDepth3D(float delta);
    void adjustColorImpact(float delta);
    void adjustComplexity(float delta);
    void updateMouseInteraction(int x, int y, bool pressed);
    EnvironmentState currentEnvironment() const;
    bool handlePanelPointer(int x, int y, bool pressed, bool commit);
    void applyPanelControl(const PanelItem& item, int x);
    void applyPanelSlider(PanelControl control, int x);
    void setStatus(std::wstring status);

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};

} // namespace viz
