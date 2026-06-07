cmake_minimum_required(VERSION 3.24)

project(Visualizer VERSION 0.1.0 LANGUAGES CXX)

option(VISUALIZER_BUILD_APP "Build the native Windows visualizer application" ON)
option(VISUALIZER_BUILD_TESTS "Build portable core tests" ON)

add_library(visualizer_core STATIC
    src/audio/AdaptiveStyleModel.cpp
    src/audio/AudioAnalyzer.cpp
    src/audio/AudioFileLoader.cpp
    src/audio/AudioSyncProfile.cpp
    src/audio/WavFile.cpp
    src/export/AnalysisTimeline.cpp
    src/export/BatchExporter.cpp
    src/export/CapturePackage.cpp
    src/export/OfflineExporter.cpp
    src/export/PreviewImage.cpp
    src/export/SharePackage.cpp
    src/export/VideoEncoder.cpp
    src/performance/FramePerformanceTracker.cpp
    src/support/SupportBundle.cpp
    src/ui/ControlPanel.cpp
    src/ui/RuntimeInspector.cpp
    src/visualization/FrameRecorder.cpp
    src/visualization/PresetStore.cpp
    src/visualization/SceneDirector.cpp
    src/visualization/VisualizerEngine.cpp
)

target_include_directories(visualizer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(visualizer_core PUBLIC cxx_std_20)

if(WIN32)
    target_sources(visualizer_core PRIVATE src/platform/windows/MediaFoundationAudioDecoder.cpp)
    target_link_libraries(visualizer_core PUBLIC mfplat mfreadwrite mfuuid ole32)
    target_compile_definitions(visualizer_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    target_compile_options(visualizer_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(visualizer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32 AND VISUALIZER_BUILD_APP)
    add_executable(Visualizer WIN32
        src/main_windows.cpp
        src/platform/windows/Direct2DRenderer.cpp
        src/platform/windows/WasapiLoopbackCapture.cpp
        src/platform/windows/WindowsApp.cpp
    )

    target_link_libraries(Visualizer PRIVATE
        visualizer_core
        d2d1
        dwrite
        ole32
        uuid
        winmm
        comdlg32
        shell32
    )

    target_compile_definitions(Visualizer PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )

    if(MSVC)
        target_compile_options(Visualizer PRIVATE /W4 /permissive-)
    else()
        target_compile_options(Visualizer PRIVATE -Wall -Wextra -Wpedantic)
        target_link_options(Visualizer PRIVATE -municode)
    endif()
endif()

add_executable(VisualizerExport
    src/export_main.cpp
)

target_link_libraries(VisualizerExport PRIVATE visualizer_core)
target_compile_features(VisualizerExport PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(VisualizerExport PRIVATE /W4 /permissive-)
else()
    target_compile_options(VisualizerExport PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(VisualizerBatch
    src/batch_main.cpp
)

target_link_libraries(VisualizerBatch PRIVATE visualizer_core)
target_compile_features(VisualizerBatch PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(VisualizerBatch PRIVATE /W4 /permissive-)
else()
    target_compile_options(VisualizerBatch PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(VisualizerBenchmark
    src/benchmark_main.cpp
)

target_link_libraries(VisualizerBenchmark PRIVATE visualizer_core)
target_compile_features(VisualizerBenchmark PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(VisualizerBenchmark PRIVATE /W4 /permissive-)
else()
    target_compile_options(VisualizerBenchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(VisualizerSupport
    src/support_main.cpp
)

target_link_libraries(VisualizerSupport PRIVATE visualizer_core)
target_compile_features(VisualizerSupport PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(VisualizerSupport PRIVATE /W4 /permissive-)
else()
    target_compile_options(VisualizerSupport PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(VISUALIZER_BUILD_TESTS)
    enable_testing()

    add_executable(visualizer_tests
        tests/AudioAnalyzerTests.cpp
        tests/SupportBundleTests.cpp
        tests/VisualizerEngineTests.cpp
    )

    target_link_libraries(visualizer_tests PRIVATE visualizer_core)
    target_compile_features(visualizer_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(visualizer_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(visualizer_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME visualizer_core_tests COMMAND visualizer_tests)
endif()
