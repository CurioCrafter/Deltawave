#include "WindowsApp.hpp"

#include "Visualizer/Export/VideoEncoder.hpp"

#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

namespace viz {
namespace {

constexpr float kTrailPersistence = 0.84f;

std::wstring widen(std::string_view value)
{
    return std::wstring(value.begin(), value.end());
}

std::wstring basename(const std::filesystem::path& path)
{
    return path.filename().wstring();
}

double secondsSince(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

double millisecondsBetween(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

float wrapUnit(float value)
{
    value = std::fmod(value, 1.0f);
    if (value < 0.0f) {
        value += 1.0f;
    }
    return value;
}

std::wstring hueShiftStatus(float hueShift)
{
    return L"Hue shift: " + std::to_wstring(static_cast<int>(std::round(wrapUnit(hueShift) * 360.0f))) + L" deg";
}

MotionStyle nextMotionStyle(MotionStyle current, int direction)
{
    static constexpr std::array<MotionStyle, 7> styles = {
        MotionStyle::Smooth,
        MotionStyle::Mechanical,
        MotionStyle::Liquid,
        MotionStyle::Hyperspace,
        MotionStyle::HeavyBass,
        MotionStyle::AmbientDrift,
        MotionStyle::Breakbeat
    };

    const auto it = std::find(styles.begin(), styles.end(), current);
    const int count = static_cast<int>(styles.size());
    const int index = it == styles.end() ? 0 : static_cast<int>(it - styles.begin());
    return styles[static_cast<std::size_t>((index + direction + count) % count)];
}

int captureFrameRate(std::size_t framesWritten, double durationSeconds)
{
    if (framesWritten == 0 || durationSeconds <= 0.0001) {
        return 60;
    }
    return std::clamp(static_cast<int>(std::round(static_cast<double>(framesWritten) / durationSeconds)), 1, 240);
}

std::filesystem::path adaptiveStyleProfilePath()
{
    return std::filesystem::current_path() / "profiles" / "adaptive_style_profile.vizaudio";
}

std::filesystem::path sourceProfileDirectory()
{
    return std::filesystem::current_path() / "profiles" / "sources";
}

std::filesystem::path sourceStyleProfilePath(std::string_view stem)
{
    return sourceProfileDirectory() / (std::string(stem) + ".vizaudio");
}

std::filesystem::path sourceSyncProfilePath(std::string_view stem)
{
    return sourceProfileDirectory() / (std::string(stem) + ".vizsync");
}

std::uint64_t fnv1a(std::wstring_view value)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (wchar_t ch : value) {
        wchar_t lower = ch;
        if (lower >= L'A' && lower <= L'Z') {
            lower = static_cast<wchar_t>(lower - L'A' + L'a');
        }
        hash ^= static_cast<std::uint64_t>(lower & 0xFF);
        hash *= 1099511628211ULL;
        hash ^= static_cast<std::uint64_t>((lower >> 8) & 0xFF);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string hex64(std::uint64_t value)
{
    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << value;
    return output.str();
}

std::string sanitizeProfileLabel(std::string value)
{
    std::string output;
    output.reserve(value.size());
    for (char ch : value) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if ((byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9')) {
            output.push_back(static_cast<char>(byte));
        } else if (byte >= 'A' && byte <= 'Z') {
            output.push_back(static_cast<char>(byte - 'A' + 'a'));
        } else if (!output.empty() && output.back() != '_') {
            output.push_back('_');
        }
        if (output.size() >= 48) {
            break;
        }
    }
    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }
    return output.empty() ? "audio" : output;
}

std::string audioSourceProfileStem(const std::filesystem::path& path)
{
    const std::string label = sanitizeProfileLabel(path.stem().string());
    return "audio_" + label + "_" + hex64(fnv1a(path.lexically_normal().wstring()));
}

} // namespace

WindowsApp::WindowsApp(HINSTANCE instance)
    : instance_(instance),
      appStart_(std::chrono::steady_clock::now()),
      audioStart_(appStart_),
      lastMouseTime_(appStart_)
{
    userPresetDirectory_ = defaultUserPresetDirectory();
    timeBeginPeriod(1);
}

WindowsApp::~WindowsApp()
{
    if (recorder_.isRecording()) {
        recorder_.stop();
        std::string error;
        finishCapturePackage(error);
    }
    stopFilePlayback();
    loopback_.stop();
    recorder_.stop();
    saveAdaptiveStyleProfile();
    timeEndPeriod(1);
}

int WindowsApp::run(int showCommand)
{
    if (!createMainWindow(showCommand)) {
        return 1;
    }
    refreshUserPresetLibrary();
    setActiveAudioProfiles("live_loopback");
    const bool profileLoaded = loadAdaptiveStyleProfile();
    loopback_.start();
    std::wstring initialStatus = profileLoaded
                                     ? L"Live loopback ready. Source-adaptive audio profile loaded. Press O to open an audio file."
                                     : L"Live loopback ready. Start music in any Windows player, or press O to open an audio file.";
    if (!userPresetLibrary_.empty()) {
        initialStatus += L" User presets: " + std::to_wstring(userPresetLibrary_.size()) + L".";
    }
    setStatus(std::move(initialStatus));
    messageLoop();
    return 0;
}

bool WindowsApp::createMainWindow(int showCommand)
{
    const wchar_t* className = L"VisualizerWindowClass";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = WindowsApp::windowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.lpszClassName = className;
    RegisterClassExW(&windowClass);

    RECT rect{0, 0, 1280, 720};
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
    window_ = CreateWindowExW(0,
                              className,
                              L"Visualizer",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              rect.right - rect.left,
                              rect.bottom - rect.top,
                              nullptr,
                              nullptr,
                              instance_,
                              this);
    if (window_ == nullptr) {
        return false;
    }

    DragAcceptFiles(window_, TRUE);
    std::wstring error;
    if (!renderer_.initialize(window_, error)) {
        MessageBoxW(window_, error.c_str(), L"Visualizer initialization failed", MB_ICONERROR | MB_OK);
        return false;
    }

    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    return true;
}

void WindowsApp::messageLoop()
{
    MSG message{};
    while (running_) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running_ = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        updateAndRender();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

void WindowsApp::updateAndRender()
{
    const auto frameStart = std::chrono::steady_clock::now();
    const double appTimeSeconds = secondsSince(appStart_);
    if (performanceStats_.qualityScale > 0.0f) {
        settings_.qualityScale = performanceStats_.qualityScale;
    }

    const auto analysisStart = std::chrono::steady_clock::now();
    if (sourceMode_ == SourceMode::AudioFile) {
        analyzeAudioFile(appTimeSeconds);
    } else {
        analyzeLoopback(appTimeSeconds);
    }
    const auto analysisEnd = std::chrono::steady_clock::now();

    RECT rect{};
    GetClientRect(window_, &rect);
    const float width = static_cast<float>(std::max<LONG>(1, rect.right - rect.left));
    const float height = static_cast<float>(std::max<LONG>(1, rect.bottom - rect.top));
    const AudioMetrics metrics = analyzer_.lastMetrics();
    const VisualSettings renderSettings = sceneDirector_.resolve(settings_, metrics, appTimeSeconds);
    const EnvironmentState environment = currentEnvironment();
    const auto geometryStart = std::chrono::steady_clock::now();
    const GeometryFrame frame = engine_.buildFrame(metrics,
                                                   renderSettings,
                                                   interaction_,
                                                   environment,
                                                   width,
                                                   height,
                                                   appTimeSeconds);
    const int primitiveCount = countPrimitives(frame);
    const auto geometryEnd = std::chrono::steady_clock::now();

    const bool recordingThisFrame = recorder_.isRecording();
    const RuntimeInspectorState inspector = runtimeInspectorState();
    const auto renderStart = std::chrono::steady_clock::now();
    renderer_.render(frame,
                     metrics,
                     renderSettings,
                     performanceStats_,
                     inspector,
                     status_,
                     recordingThisFrame,
                     recorder_.frameCount());
    const auto renderEnd = std::chrono::steady_clock::now();

    bool captureUpdated = false;
    const auto recordStart = std::chrono::steady_clock::now();
    if (recordingThisFrame) {
        std::string error;
        FrameRenderOptions renderOptions;
        renderOptions.trails = renderSettings.trails;
        renderOptions.trailPersistence = kTrailPersistence;
        if (!recorder_.writeFrame(frame, renderOptions, error)) {
            setStatus(L"Recording stopped: " + widen(error));
        } else {
            updateCapturePackage(metrics, renderSettings, frame, primitiveCount);
            captureUpdated = true;
        }
    }
    const auto recordEnd = std::chrono::steady_clock::now();

    const auto frameEnd = std::chrono::steady_clock::now();
    performanceStats_ = performanceTracker_.recordFrame(FrameTimingBreakdown{
                                                            millisecondsBetween(frameStart, frameEnd),
                                                            millisecondsBetween(analysisStart, analysisEnd),
                                                            millisecondsBetween(geometryStart, geometryEnd),
                                                            millisecondsBetween(renderStart, renderEnd),
                                                            recordingThisFrame ? millisecondsBetween(recordStart, recordEnd) : 0.0
                                                        },
                                                        primitiveCount,
                                                        renderSettings.adaptiveQuality,
                                                        renderSettings.qualityScale);
    settings_.qualityScale = performanceStats_.qualityScale;
    if (captureUpdated) {
        updateCapturePerformance(performanceStats_);
    }
}

void WindowsApp::analyzeLoopback(double appTimeSeconds)
{
    int sampleRate = analyzer_.sampleRate();
    int channels = analyzer_.channelCount();
    std::vector<float> samples = loopback_.latestFrames(2048, sampleRate, channels);
    if (!samples.empty() && channels > 0) {
        if (sampleRate != analyzer_.sampleRate() || channels != analyzer_.channelCount()) {
            analyzer_.configure(sampleRate, channels);
        }
        analyzer_.analyzeInterleaved(samples.data(), samples.size() / static_cast<std::size_t>(channels), appTimeSeconds);
        return;
    }

    const std::wstring error = loopback_.lastError();
    if (!error.empty()) {
        setStatus(error);
    }
}

void WindowsApp::analyzeAudioFile(double appTimeSeconds)
{
    if (!audio_) {
        useLoopback();
        return;
    }

    const double elapsed = secondsSince(audioStart_);
    const std::size_t totalFrames = audio_->samples.size() / static_cast<std::size_t>(audio_->channelCount);
    if (elapsed >= audio_->durationSeconds || totalFrames == 0) {
        useLoopback();
        setStatus(L"Audio playback complete. Returned to live loopback.");
        return;
    }

    if (audio_->sampleRate != analyzer_.sampleRate() || audio_->channelCount != analyzer_.channelCount()) {
        analyzer_.configure(audio_->sampleRate, audio_->channelCount);
    }

    const std::size_t currentFrame = std::min<std::size_t>(
        totalFrames,
        static_cast<std::size_t>(elapsed * static_cast<double>(audio_->sampleRate)));
    const std::size_t analysisFrames = std::min<std::size_t>(2048, std::max<std::size_t>(1, currentFrame));
    const std::size_t startFrame = currentFrame > analysisFrames ? currentFrame - analysisFrames : 0;
    const std::size_t channels = static_cast<std::size_t>(audio_->channelCount);
    const std::size_t offset = startFrame * channels;
    const std::size_t sampleCount = analysisFrames * channels;
    if (offset + sampleCount <= audio_->samples.size()) {
        analyzer_.analyzeInterleaved(audio_->samples.data() + offset, analysisFrames, appTimeSeconds);
    }
}

void WindowsApp::setActiveAudioProfiles(std::string_view stem)
{
    activeStyleProfilePath_ = sourceStyleProfilePath(stem);
    activeSyncProfilePath_ = sourceSyncProfilePath(stem);
}

bool WindowsApp::loadAdaptiveStyleProfile()
{
    if (activeStyleProfilePath_.empty() || activeSyncProfilePath_.empty()) {
        setActiveAudioProfiles("live_loopback");
    }

    std::string error;
    bool loaded = false;
    if (std::filesystem::exists(activeStyleProfilePath_)) {
        if (analyzer_.loadStyleProfile(activeStyleProfilePath_, error)) {
            loaded = true;
        } else {
            setStatus(L"Adaptive style profile ignored: " + widen(error));
        }
    } else if (activeStyleProfilePath_.filename() == L"live_loopback.vizaudio" &&
               std::filesystem::exists(adaptiveStyleProfilePath())) {
        if (analyzer_.loadStyleProfile(adaptiveStyleProfilePath(), error)) {
            loaded = true;
        } else {
            setStatus(L"Legacy adaptive style profile ignored: " + widen(error));
        }
    }

    if (std::filesystem::exists(activeSyncProfilePath_)) {
        if (analyzer_.loadSyncProfile(activeSyncProfilePath_, error)) {
            loaded = true;
        } else {
            setStatus(L"Adaptive sync profile ignored: " + widen(error));
        }
    }
    return loaded;
}

void WindowsApp::saveAdaptiveStyleProfile()
{
    if (activeStyleProfilePath_.empty() || activeSyncProfilePath_.empty()) {
        setActiveAudioProfiles("live_loopback");
    }

    std::string error;
    (void)analyzer_.saveStyleProfile(activeStyleProfilePath_, error);
    (void)analyzer_.saveSyncProfile(activeSyncProfilePath_, error);
}

void WindowsApp::openAudioDialog()
{
    std::array<wchar_t, MAX_PATH> fileName{};
    OPENFILENAMEW openFile{};
    openFile.lStructSize = sizeof(openFile);
    openFile.hwndOwner = window_;
    openFile.lpstrFilter = L"Audio files\0*.wav;*.wave;*.mp3;*.m4a;*.aac;*.wma;*.flac\0WAV files\0*.wav;*.wave\0All files\0*.*\0";
    openFile.lpstrFile = fileName.data();
    openFile.nMaxFile = static_cast<DWORD>(fileName.size());
    openFile.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    openFile.lpstrDefExt = L"wav";
    if (GetOpenFileNameW(&openFile) == TRUE) {
        loadAudioFileFromPath(fileName.data());
    }
}

void WindowsApp::loadAudioFileFromPath(const std::filesystem::path& path)
{
    std::string error;
    std::optional<WavAudio> loaded = loadAudioFile(path, error);
    if (!loaded) {
        setStatus(L"Unable to load audio: " + widen(error));
        return;
    }

    saveAdaptiveStyleProfile();
    stopFilePlayback();
    loopback_.stop();
    audioPath_ = path;
    audio_ = std::move(loaded);
    sourceMode_ = SourceMode::AudioFile;
    audioStart_ = std::chrono::steady_clock::now();
    analyzer_.resetStyleProfile();
    analyzer_.resetSyncProfile();
    analyzer_.configure(audio_->sampleRate, audio_->channelCount);
    setActiveAudioProfiles(audioSourceProfileStem(audioPath_));
    const bool profileLoaded = loadAdaptiveStyleProfile();

    const bool started = startFilePlayback(audioPath_);
    std::wostringstream status;
    status << (started ? L"Playing audio: " : L"Audio loaded for analysis, playback failed: ")
           << basename(audioPath_) << L"  "
           << audio_->sampleRate << L" Hz  "
           << audio_->channelCount << L" ch  "
           << audio_->durationSeconds << L" sec";
    if (profileLoaded) {
        status << L"  source profile loaded";
    }
    setStatus(status.str());
}

bool WindowsApp::startFilePlayback(const std::filesystem::path& path)
{
    stopFilePlayback();

    if (isLikelyWavFile(path)) {
        if (PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT) == TRUE) {
            return true;
        }
    }

    const std::wstring openCommand = L"open \"" + path.wstring() + L"\" alias VisualizerMedia";
    if (mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr) != 0U) {
        mediaAliasOpen_ = false;
        return false;
    }

    mediaAliasOpen_ = true;
    if (mciSendStringW(L"play VisualizerMedia", nullptr, 0, nullptr) != 0U) {
        stopFilePlayback();
        return false;
    }

    return true;
}

void WindowsApp::stopFilePlayback()
{
    PlaySoundW(nullptr, nullptr, 0);
    if (!mediaAliasOpen_) {
        return;
    }

    mciSendStringW(L"stop VisualizerMedia", nullptr, 0, nullptr);
    mciSendStringW(L"close VisualizerMedia", nullptr, 0, nullptr);
    mediaAliasOpen_ = false;
}

void WindowsApp::savePresetDialog()
{
    std::array<wchar_t, MAX_PATH> fileName{};
    OPENFILENAMEW saveFile{};
    saveFile.lStructSize = sizeof(saveFile);
    saveFile.hwndOwner = window_;
    saveFile.lpstrFilter = L"Visualizer presets\0*.vizpreset\0All files\0*.*\0";
    saveFile.lpstrFile = fileName.data();
    saveFile.nMaxFile = static_cast<DWORD>(fileName.size());
    saveFile.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    saveFile.lpstrDefExt = L"vizpreset";

    if (GetSaveFileNameW(&saveFile) != TRUE) {
        return;
    }

    const std::filesystem::path path = fileName.data();
    VisualPreset preset;
    preset.name = path.stem().string();
    preset.settings = settings_;

    std::string error;
    if (!savePreset(path, preset, error)) {
        setStatus(L"Unable to save preset: " + widen(error));
        return;
    }

    refreshUserPresetLibrary();
    setStatus(L"Preset saved: " + path.wstring());
}

void WindowsApp::loadPresetDialog()
{
    std::array<wchar_t, MAX_PATH> fileName{};
    OPENFILENAMEW openFile{};
    openFile.lStructSize = sizeof(openFile);
    openFile.hwndOwner = window_;
    openFile.lpstrFilter = L"Visualizer presets\0*.vizpreset\0All files\0*.*\0";
    openFile.lpstrFile = fileName.data();
    openFile.nMaxFile = static_cast<DWORD>(fileName.size());
    openFile.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    openFile.lpstrDefExt = L"vizpreset";

    if (GetOpenFileNameW(&openFile) != TRUE) {
        return;
    }

    std::string error;
    const std::optional<VisualPreset> preset = loadPreset(fileName.data(), error);
    if (!preset) {
        setStatus(L"Unable to load preset: " + widen(error));
        return;
    }

    applyVisualPreset(*preset);
    activeLookName_ = preset->name;
    curatedPresetApplied_ = false;
    userPresetApplied_ = false;
    setStatus(L"Preset loaded: " + widen(preset->name));
}

void WindowsApp::applyVisualPreset(const VisualPreset& preset)
{
    settings_ = preset.settings;
    interaction_.enabled = settings_.interactiveField;
    sceneDirector_.reset();
    performanceTracker_.reset();
    performanceStats_ = performanceTracker_.stats();
    performanceStats_.qualityScale = settings_.qualityScale;
}

void WindowsApp::applyCuratedPreset(int direction)
{
    const std::vector<VisualPreset>& presets = curatedPresets();
    if (presets.empty()) {
        setStatus(L"No curated looks are available.");
        return;
    }

    if (!curatedPresetApplied_) {
        curatedPresetIndex_ = direction < 0 ? presets.size() - 1U : 0U;
        curatedPresetApplied_ = true;
    } else {
        const int count = static_cast<int>(presets.size());
        const int current = static_cast<int>(curatedPresetIndex_ % presets.size());
        curatedPresetIndex_ = static_cast<std::size_t>((current + direction + count) % count);
    }

    applyVisualPreset(presets[curatedPresetIndex_]);
    activeLookName_ = presets[curatedPresetIndex_].name;
    userPresetApplied_ = false;
    setStatus(L"Look: " + widen(presets[curatedPresetIndex_].name));
}

void WindowsApp::refreshUserPresetLibrary()
{
    userPresetLibrary_ = scanUserPresetLibrary(userPresetDirectory_);
    if (userPresetLibrary_.empty()) {
        userPresetIndex_ = 0;
        userPresetApplied_ = false;
        return;
    }
    if (userPresetIndex_ >= userPresetLibrary_.size()) {
        userPresetIndex_ = 0;
    }
}

void WindowsApp::applyUserPreset(int direction)
{
    refreshUserPresetLibrary();
    if (userPresetLibrary_.empty()) {
        setStatus(L"No user presets in " + userPresetDirectory_.wstring() + L". Press K or click Save User to create one.");
        return;
    }

    if (!userPresetApplied_) {
        userPresetIndex_ = direction < 0 ? userPresetLibrary_.size() - 1U : 0U;
        userPresetApplied_ = true;
    } else {
        const int count = static_cast<int>(userPresetLibrary_.size());
        const int current = static_cast<int>(userPresetIndex_ % userPresetLibrary_.size());
        userPresetIndex_ = static_cast<std::size_t>((current + direction + count) % count);
    }

    std::string error;
    const PresetLibraryEntry& entry = userPresetLibrary_[userPresetIndex_];
    const std::optional<VisualPreset> preset = loadUserPresetEntry(entry, error);
    if (!preset) {
        setStatus(L"Unable to load user preset: " + widen(error));
        userPresetApplied_ = false;
        return;
    }

    applyVisualPreset(*preset);
    activeLookName_ = preset->name;
    curatedPresetApplied_ = false;
    setStatus(L"User preset " + std::to_wstring(userPresetIndex_ + 1U) + L"/" +
              std::to_wstring(userPresetLibrary_.size()) + L": " + widen(preset->name));
}

void WindowsApp::saveCurrentUserPreset()
{
    VisualPreset preset;
    preset.name = activeLookName_.empty() ? std::string(toString(settings_.mode)) : activeLookName_;
    preset.settings = settings_;

    std::filesystem::path savedPath;
    std::string error;
    if (!saveUserPreset(userPresetDirectory_, preset, savedPath, error)) {
        setStatus(L"Unable to save user preset: " + widen(error));
        return;
    }

    refreshUserPresetLibrary();
    for (std::size_t i = 0; i < userPresetLibrary_.size(); ++i) {
        if (userPresetLibrary_[i].path.lexically_normal() == savedPath.lexically_normal()) {
            userPresetIndex_ = i;
            break;
        }
    }

    activeLookName_ = preset.name;
    curatedPresetApplied_ = false;
    userPresetApplied_ = true;
    setStatus(L"User preset saved: " + savedPath.filename().wstring());
}

void WindowsApp::markCustomLook()
{
    activeLookName_.clear();
}

void WindowsApp::resetCurrentAudioProfiles()
{
    if (activeStyleProfilePath_.empty() || activeSyncProfilePath_.empty()) {
        setActiveAudioProfiles("live_loopback");
    }

    analyzer_.resetStyleProfile();
    analyzer_.resetSyncProfile();
    std::error_code ignored;
    std::filesystem::remove(activeStyleProfilePath_, ignored);
    std::filesystem::remove(activeSyncProfilePath_, ignored);
    if (sourceMode_ == SourceMode::Loopback) {
        std::filesystem::remove(adaptiveStyleProfilePath(), ignored);
    }
    saveAdaptiveStyleProfile();
    setStatus(L"Audio AI profile reset for current source.");
}

void WindowsApp::useLoopback()
{
    saveAdaptiveStyleProfile();
    stopFilePlayback();
    audio_.reset();
    sourceMode_ = SourceMode::Loopback;
    analyzer_.resetStyleProfile();
    analyzer_.resetSyncProfile();
    analyzer_.reset();
    setActiveAudioProfiles("live_loopback");
    const bool profileLoaded = loadAdaptiveStyleProfile();
    loopback_.start();
    setStatus(profileLoaded
                  ? L"Live loopback mode. Source-adaptive audio profile loaded."
                  : L"Live loopback mode. Play audio through the default Windows output device.");
}

void WindowsApp::toggleRecording()
{
    if (recorder_.isRecording()) {
        const std::filesystem::path session = recorder_.sessionPath();
        recorder_.stop();
        std::string error;
        if (!finishCapturePackage(error)) {
            setStatus(L"Recording saved, package failed: " + widen(error));
            return;
        }
        if (capturePackage_.videoEncoded && !capturePackage_.videoPath.empty()) {
            setStatus(L"Recording saved with MP4: " + capturePackage_.videoPath.wstring());
        } else {
            setStatus(L"Recording saved with share page: " + (session / "index.html").wstring());
        }
        return;
    }

    RECT rect{};
    GetClientRect(window_, &rect);
    const int width = static_cast<int>(std::max<LONG>(1, rect.right - rect.left));
    const int height = static_cast<int>(std::max<LONG>(1, rect.bottom - rect.top));
    std::string error;
    const std::filesystem::path root = std::filesystem::current_path() / "captures";
    if (!recorder_.start(root, width, height, error)) {
        setStatus(L"Unable to start recording: " + widen(error));
        return;
    }
    beginCapturePackage(width, height);
    setStatus(L"Recording frames to: " + recorder_.sessionPath().wstring());
}

void WindowsApp::beginCapturePackage(int width, int height)
{
    capturePackage_ = CapturePackage{};
    capturePackage_.sessionPath = recorder_.sessionPath();
    capturePackage_.sourceLabel = sourceMode_ == SourceMode::AudioFile && !audioPath_.empty()
                                      ? audioPath_.filename().string()
                                      : "Live loopback";
    capturePackage_.lookName = activeLookName_;
    capturePackage_.styleProfilePath = activeStyleProfilePath_;
    capturePackage_.syncProfilePath = activeSyncProfilePath_;
    capturePackage_.requestedSettings = settings_;
    capturePackage_.finalSettings = settings_;
    capturePackage_.width = width;
    capturePackage_.height = height;
    capturePackage_.framesWritten = 0;
    capturePackage_.durationSeconds = 0.0;
    capturePackage_.timelinePath = capturePackage_.sessionPath / "analysis_timeline.csv";
    std::string error;
    captureTimelineActive_ = captureTimeline_.open(capturePackage_.timelinePath, error);
    if (!captureTimelineActive_) {
        capturePackage_.timelinePath.clear();
        capturePackage_.timelineWriteError = error;
    }
    recordingStart_ = std::chrono::steady_clock::now();
    capturePackageActive_ = true;
}

void WindowsApp::updateCapturePackage(const AudioMetrics& metrics,
                                      const VisualSettings& renderSettings,
                                      const GeometryFrame& geometry,
                                      int primitiveCount)
{
    if (!capturePackageActive_) {
        return;
    }

    capturePackage_.finalSettings = renderSettings;
    capturePackage_.framesWritten = recorder_.frameCount();
    capturePackage_.durationSeconds = secondsSince(recordingStart_);
    if (captureTimelineActive_) {
        std::string error;
        const int frameIndex = capturePackage_.framesWritten > 0
                                   ? static_cast<int>(capturePackage_.framesWritten - 1U)
                                   : 0;
        if (!captureTimeline_.write(AnalysisTimelineEntry{
                frameIndex,
                capturePackage_.durationSeconds,
                metrics,
                renderSettings,
                primitiveCount,
                geometry.scene3DName,
                geometry.sceneIntentName,
                geometry.authored2DPrimitiveCount,
                geometry.retained2DPrimitiveCount,
                geometry.projected3DPrimitiveCount,
                geometry.threeDDominance
            },
            error)) {
            captureTimelineActive_ = false;
            captureTimeline_.close();
            capturePackage_.timelinePath.clear();
            capturePackage_.timelineWritten = false;
            capturePackage_.timelineWriteError = error;
        } else {
            capturePackage_.timelineWritten = true;
        }
    }
    capturePackage_.peakRms = std::max(capturePackage_.peakRms, metrics.rms);
    if (metrics.bpm > 0.0f) {
        capturePackage_.estimatedBpm = metrics.bpm;
    }
    if (metrics.beat) {
        ++capturePackage_.beatsDetected;
    }
    if (metrics.downbeat) {
        ++capturePackage_.downbeatsDetected;
    }
    if (metrics.phraseBoundary) {
        ++capturePackage_.phraseBoundariesDetected;
    }
    const double recordedFrames = static_cast<double>(std::max<std::size_t>(1U, capturePackage_.framesWritten));
    capturePackage_.averagePhraseConfidence =
        ((capturePackage_.averagePhraseConfidence * (recordedFrames - 1.0)) +
         static_cast<double>(metrics.phraseConfidence)) /
        recordedFrames;
    capturePackage_.peakBuildTension = std::max(capturePackage_.peakBuildTension, metrics.buildTension);
    if (metrics.keyConfidence > capturePackage_.keyConfidence) {
        capturePackage_.detectedKeyIndex = metrics.keyIndex;
        capturePackage_.detectedKeyMode = metrics.keyMode;
        capturePackage_.keyConfidence = metrics.keyConfidence;
    }
    if (metrics.sectionConfidence > capturePackage_.sectionConfidence) {
        capturePackage_.dominantSection = metrics.section;
        capturePackage_.sectionConfidence = metrics.sectionConfidence;
    }
}

void WindowsApp::updateCapturePerformance(const PerformanceStats& performance)
{
    if (!capturePackageActive_) {
        return;
    }

    capturePackage_.averageFrameMs = performance.averageFrameMs;
    capturePackage_.averageAnalysisMs = performance.averageAnalysisMs;
    capturePackage_.averageGeometryMs = performance.averageGeometryMs;
    capturePackage_.averageRenderMs = performance.averageRenderMs;
    capturePackage_.averageRecordMs = performance.averageRecordMs;
}

void WindowsApp::encodeCaptureVideo()
{
    if (capturePackage_.sessionPath.empty() || capturePackage_.framesWritten == 0) {
        return;
    }

    VideoEncodeOptions options;
    options.framesDirectory = capturePackage_.sessionPath;
    options.outputMp4 = capturePackage_.sessionPath / "visualizer-live-capture.mp4";
    options.frameRate = captureFrameRate(capturePackage_.framesWritten, capturePackage_.durationSeconds);
    options.crf = 18;
    options.preset = "fast";

    VideoEncodeResult result;
    std::string error;
    if (encodeFrameSequenceToMp4(options, result, error)) {
        capturePackage_.videoEncoded = true;
        capturePackage_.videoPath = result.outputMp4;
        capturePackage_.videoBytes = result.bytesWritten;
        capturePackage_.videoEncodeError.clear();
    } else {
        capturePackage_.videoEncoded = false;
        capturePackage_.videoPath.clear();
        capturePackage_.videoBytes = 0;
        capturePackage_.videoEncodeError = error;
    }
}

bool WindowsApp::finishCapturePackage(std::string& error)
{
    if (!capturePackageActive_) {
        error.clear();
        return true;
    }

    capturePackage_.framesWritten = recorder_.frameCount();
    capturePackage_.durationSeconds = std::max(capturePackage_.durationSeconds, secondsSince(recordingStart_));
    if (captureTimelineActive_) {
        captureTimeline_.close();
        captureTimelineActive_ = false;
        capturePackage_.timelineWritten = !capturePackage_.timelinePath.empty();
    }
    encodeCaptureVideo();
    capturePackageActive_ = false;
    return writeCapturePackage(capturePackage_, error);
}

RuntimeInspectorState WindowsApp::runtimeInspectorState() const
{
    RuntimeInspectorState state;
    state.fileSource = sourceMode_ == SourceMode::AudioFile && audio_.has_value();
    state.sourceLabel = state.fileSource && !audioPath_.empty()
                            ? audioPath_.filename().string()
                            : "Live loopback";
    state.sourceDetail = state.fileSource
                             ? (audioPath_.has_parent_path() ? audioPath_.parent_path().filename().string() : std::string{"Audio file"})
                             : "Default Windows output";
    state.activeLook = activeLookName_.empty() ? "Custom" : activeLookName_;
    state.styleProfileName = activeStyleProfilePath_.empty() ? std::string{} : activeStyleProfilePath_.filename().string();
    state.syncProfileName = activeSyncProfilePath_.empty() ? std::string{} : activeSyncProfilePath_.filename().string();
    state.presetLibraryName = userPresetDirectory_.empty() ? std::string{} : userPresetDirectory_.filename().string();
    state.userPresetCount = static_cast<int>(userPresetLibrary_.size());
    state.sampleRate = analyzer_.sampleRate();
    state.channelCount = analyzer_.channelCount();

    if (state.fileSource && audio_) {
        state.playbackDurationSeconds = audio_->durationSeconds;
        state.playbackPositionSeconds = std::min(secondsSince(audioStart_), audio_->durationSeconds);
    }

    state.recording = recorder_.isRecording();
    if (state.recording) {
        state.captureDurationSeconds = secondsSince(recordingStart_);
        if (!recorder_.sessionPath().empty()) {
            state.captureDirectory = recorder_.sessionPath().filename().string();
        }
    }

    return state;
}

void WindowsApp::toggleFullscreen()
{
    if (!fullscreen_) {
        windowedStyle_ = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE));
        windowedExStyle_ = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_EXSTYLE));
        GetWindowRect(window_, &windowedRect_);

        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitorInfo);
        SetWindowLongPtrW(window_, GWL_STYLE, windowedStyle_ & ~WS_OVERLAPPEDWINDOW);
        SetWindowLongPtrW(window_, GWL_EXSTYLE, windowedExStyle_ & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        SetWindowPos(window_,
                     HWND_TOP,
                     monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.top,
                     monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        fullscreen_ = true;
        setStatus(L"Fullscreen enabled.");
    } else {
        SetWindowLongPtrW(window_, GWL_STYLE, windowedStyle_);
        SetWindowLongPtrW(window_, GWL_EXSTYLE, windowedExStyle_);
        SetWindowPos(window_,
                     nullptr,
                     windowedRect_.left,
                     windowedRect_.top,
                     windowedRect_.right - windowedRect_.left,
                     windowedRect_.bottom - windowedRect_.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        fullscreen_ = false;
        setStatus(L"Fullscreen disabled.");
    }
}

void WindowsApp::toggleInteractiveField()
{
    markCustomLook();
    settings_.interactiveField = !settings_.interactiveField;
    interaction_.enabled = settings_.interactiveField;
    setStatus(settings_.interactiveField ? L"Interactive field enabled." : L"Interactive field disabled.");
}

void WindowsApp::toggleEnvironmentReactive()
{
    markCustomLook();
    settings_.environmentReactive = !settings_.environmentReactive;
    setStatus(settings_.environmentReactive ? L"Environment reactivity enabled." : L"Environment reactivity disabled.");
}

void WindowsApp::toggleAdaptiveQuality()
{
    markCustomLook();
    settings_.adaptiveQuality = !settings_.adaptiveQuality;
    if (!settings_.adaptiveQuality) {
        performanceTracker_.reset();
        performanceStats_ = performanceTracker_.stats();
        settings_.qualityScale = 1.0f;
    }
    setStatus(settings_.adaptiveQuality ? L"Adaptive quality enabled." : L"Adaptive quality disabled.");
}

void WindowsApp::toggleAutoScene()
{
    markCustomLook();
    settings_.autoScene = !settings_.autoScene;
    sceneDirector_.reset();
    setStatus(settings_.autoScene ? L"Auto Scene enabled." : L"Auto Scene disabled.");
}

void WindowsApp::toggleTrails()
{
    markCustomLook();
    settings_.trails = !settings_.trails;
    setStatus(settings_.trails ? L"Trails enabled." : L"Trails disabled.");
}

void WindowsApp::cyclePalette()
{
    markCustomLook();
    settings_.autoScene = false;
    sceneDirector_.reset();
    constexpr int paletteCount = 5;
    const int next = (static_cast<int>(settings_.palette) + 1) % paletteCount;
    settings_.palette = static_cast<Palette>(next);
    setStatus(L"Palette: " + widen(toString(settings_.palette)));
}

void WindowsApp::setMode(VisualMode mode)
{
    markCustomLook();
    settings_.autoScene = false;
    sceneDirector_.reset();
    settings_.mode = mode;
    setStatus(L"Mode: " + widen(toString(settings_.mode)));
}

void WindowsApp::adjustIntensity(float delta)
{
    markCustomLook();
    settings_.intensity = std::clamp(settings_.intensity + delta, 0.15f, 4.0f);
    setStatus(L"Intensity: " + std::to_wstring(settings_.intensity));
}

void WindowsApp::adjustSpeed(float delta)
{
    markCustomLook();
    settings_.speed = std::clamp(settings_.speed + delta, 0.1f, 4.0f);
    setStatus(L"Speed: " + std::to_wstring(settings_.speed));
}

void WindowsApp::adjustHueShift(float delta)
{
    markCustomLook();
    settings_.hueShift = wrapUnit(settings_.hueShift + delta);
    setStatus(hueShiftStatus(settings_.hueShift));
}

void WindowsApp::adjustDepth3D(float delta)
{
    markCustomLook();
    settings_.depth3D = std::clamp(settings_.depth3D + delta, 0.0f, 1.0f);
    setStatus(L"3D depth: " + std::to_wstring(static_cast<int>(std::round(settings_.depth3D * 100.0f))) + L"%");
}

void WindowsApp::adjustColorImpact(float delta)
{
    markCustomLook();
    settings_.colorImpact = std::clamp(settings_.colorImpact + delta, 0.0f, 1.0f);
    setStatus(L"Color impact: " + std::to_wstring(static_cast<int>(std::round(settings_.colorImpact * 100.0f))) + L"%");
}

void WindowsApp::adjustComplexity(float delta)
{
    markCustomLook();
    settings_.complexity = std::clamp(settings_.complexity + delta, 0.35f, 1.8f);
    setStatus(L"Complexity: " + std::to_wstring(settings_.complexity));
}

void WindowsApp::updateMouseInteraction(int x, int y, bool pressed)
{
    RECT rect{};
    GetClientRect(window_, &rect);
    const float width = static_cast<float>(std::max<LONG>(1, rect.right - rect.left));
    const float height = static_cast<float>(std::max<LONG>(1, rect.bottom - rect.top));
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::max(0.001, std::chrono::duration<double>(now - lastMouseTime_).count());
    const float dx = static_cast<float>(x - lastMouse_.x) / width;
    const float dy = static_cast<float>(y - lastMouse_.y) / height;
    const float velocity = std::clamp(static_cast<float>(std::sqrt(dx * dx + dy * dy) / dt), 0.0f, 4.0f);

    interaction_.enabled = settings_.interactiveField;
    interaction_.active = true;
    interaction_.pressed = pressed;
    interaction_.normalizedX = std::clamp(static_cast<float>(x) / width, 0.0f, 1.0f);
    interaction_.normalizedY = std::clamp(static_cast<float>(y) / height, 0.0f, 1.0f);
    interaction_.velocity = velocity;
    interaction_.strength = pressed ? 1.0f : 0.45f;
    lastMouse_ = POINT{x, y};
    lastMouseTime_ = now;

    if (!trackingMouse_) {
        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(event);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = window_;
        trackingMouse_ = TrackMouseEvent(&event) == TRUE;
    }
}

EnvironmentState WindowsApp::currentEnvironment() const
{
    EnvironmentState environment;
    environment.enabled = settings_.environmentReactive;
    if (!environment.enabled) {
        return environment;
    }

    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
    localtime_s(&localTime, &now);
    const int secondsToday = (localTime.tm_hour * 3600) + (localTime.tm_min * 60) + localTime.tm_sec;
    environment.timeOfDay = std::clamp(static_cast<float>(secondsToday) / 86400.0f, 0.0f, 1.0f);
    environment.motion = interaction_.active ? std::clamp(interaction_.velocity / 4.0f, 0.0f, 1.0f) : 0.0f;
    environment.ambient = 0.5f + 0.5f * std::sin((environment.timeOfDay * 2.0f * 3.14159265f) - 1.57079633f);
    return environment;
}

bool WindowsApp::handlePanelPointer(int x, int y, bool pressed, bool commit)
{
    if (!settings_.showHud) {
        return false;
    }

    RECT rect{};
    GetClientRect(window_, &rect);
    const float width = static_cast<float>(std::max<LONG>(1, rect.right - rect.left));
    const float height = static_cast<float>(std::max<LONG>(1, rect.bottom - rect.top));
    const ControlPanelLayout layout = buildControlPanelLayout(width, height, settings_, recorder_.isRecording());

    if (activeSlider_ != PanelControl::None && pressed) {
        applyPanelSlider(activeSlider_, x);
        return true;
    }

    const PanelItem hit = hitTestControlPanel(layout, static_cast<float>(x), static_cast<float>(y));
    if (hit.control == PanelControl::None) {
        return false;
    }

    if (commit) {
        applyPanelControl(hit, x);
        if (hit.slider) {
            activeSlider_ = hit.control;
        }
    }
    return true;
}

void WindowsApp::applyPanelControl(const PanelItem& item, int x)
{
    switch (item.control) {
    case PanelControl::OpenAudio:
        openAudioDialog();
        break;
    case PanelControl::Loopback:
        useLoopback();
        break;
    case PanelControl::ResetAudioProfiles:
        resetCurrentAudioProfiles();
        break;
    case PanelControl::SavePreset:
        savePresetDialog();
        break;
    case PanelControl::LoadPreset:
        loadPresetDialog();
        break;
    case PanelControl::UserPresetPrevious:
        applyUserPreset(-1);
        break;
    case PanelControl::UserPresetNext:
        applyUserPreset(1);
        break;
    case PanelControl::SaveUserPreset:
        saveCurrentUserPreset();
        break;
    case PanelControl::CuratedPresetPrevious:
        applyCuratedPreset(-1);
        break;
    case PanelControl::CuratedPresetNext:
        applyCuratedPreset(1);
        break;
    case PanelControl::MotionStylePrevious:
        markCustomLook();
        settings_.motionStyle = nextMotionStyle(settings_.motionStyle, -1);
        sceneDirector_.reset();
        setStatus(L"Motion style: " + widen(toString(settings_.motionStyle)));
        break;
    case PanelControl::MotionStyleNext:
        markCustomLook();
        settings_.motionStyle = nextMotionStyle(settings_.motionStyle, 1);
        sceneDirector_.reset();
        setStatus(L"Motion style: " + widen(toString(settings_.motionStyle)));
        break;
    case PanelControl::Record:
        toggleRecording();
        break;
    case PanelControl::ModeQuantumTunnel:
        setMode(VisualMode::QuantumTunnel);
        break;
    case PanelControl::ModeTechnoMandala:
        setMode(VisualMode::TechnoMandala);
        break;
    case PanelControl::ModeLissajousMesh:
        setMode(VisualMode::LissajousMesh);
        break;
    case PanelControl::ModeFrequencyBloom:
        setMode(VisualMode::FrequencyBloom);
        break;
    case PanelControl::ModeFractalCathedral:
        setMode(VisualMode::FractalCathedral);
        break;
    case PanelControl::ModePolyrhythmLattice:
        setMode(VisualMode::PolyrhythmLattice);
        break;
    case PanelControl::ModeSpectralOrigami:
        setMode(VisualMode::SpectralOrigami);
        break;
    case PanelControl::ModeChromaKaleidoscope:
        setMode(VisualMode::ChromaKaleidoscope);
        break;
    case PanelControl::ModeHyperspacePolytope:
        setMode(VisualMode::HyperspacePolytope);
        break;
    case PanelControl::ModePhaseWeave:
        setMode(VisualMode::PhaseWeave);
        break;
    case PanelControl::ModeResonanceTessellation:
        setMode(VisualMode::ResonanceTessellation);
        break;
    case PanelControl::ModeNeuralConstellation:
        setMode(VisualMode::NeuralConstellation);
        break;
    case PanelControl::ModeCymaticInterference:
        setMode(VisualMode::CymaticInterference);
        break;
    case PanelControl::PaletteNeonVoltage:
        markCustomLook();
        settings_.palette = Palette::NeonVoltage;
        setStatus(L"Palette: " + widen(toString(settings_.palette)));
        break;
    case PanelControl::PaletteInfraredChrome:
        markCustomLook();
        settings_.palette = Palette::InfraredChrome;
        setStatus(L"Palette: " + widen(toString(settings_.palette)));
        break;
    case PanelControl::PaletteAcidAurora:
        markCustomLook();
        settings_.palette = Palette::AcidAurora;
        setStatus(L"Palette: " + widen(toString(settings_.palette)));
        break;
    case PanelControl::PaletteMonochromeLaser:
        markCustomLook();
        settings_.palette = Palette::MonochromeLaser;
        setStatus(L"Palette: " + widen(toString(settings_.palette)));
        break;
    case PanelControl::PaletteOceanicPulse:
        markCustomLook();
        settings_.palette = Palette::OceanicPulse;
        setStatus(L"Palette: " + widen(toString(settings_.palette)));
        break;
    case PanelControl::IntensitySlider:
    case PanelControl::SpeedSlider:
    case PanelControl::HueShiftSlider:
    case PanelControl::DepthSlider:
    case PanelControl::ObjectDensitySlider:
    case PanelControl::InteractionDepthSlider:
    case PanelControl::LightingGlowSlider:
    case PanelControl::ScenePersonalitySlider:
    case PanelControl::Response3DSlider:
    case PanelControl::MotionStabilitySlider:
    case PanelControl::PatternClaritySlider:
    case PanelControl::ColorImpactSlider:
    case PanelControl::ComplexitySlider:
    case PanelControl::QualitySlider:
        applyPanelSlider(item.control, x);
        break;
    case PanelControl::ToggleHud:
        markCustomLook();
        settings_.showHud = !settings_.showHud;
        setStatus(settings_.showHud ? L"HUD enabled." : L"HUD disabled.");
        break;
    case PanelControl::ToggleInteraction:
        toggleInteractiveField();
        break;
    case PanelControl::ToggleEnvironment:
        toggleEnvironmentReactive();
        break;
    case PanelControl::ToggleAdaptiveQuality:
        toggleAdaptiveQuality();
        break;
    case PanelControl::ToggleAutoScene:
        toggleAutoScene();
        break;
    case PanelControl::ToggleTrails:
        toggleTrails();
        break;
    case PanelControl::None:
        break;
    }
}

void WindowsApp::applyPanelSlider(PanelControl control, int x)
{
    RECT rect{};
    GetClientRect(window_, &rect);
    const float width = static_cast<float>(std::max<LONG>(1, rect.right - rect.left));
    const float height = static_cast<float>(std::max<LONG>(1, rect.bottom - rect.top));
    const ControlPanelLayout layout = buildControlPanelLayout(width, height, settings_, recorder_.isRecording());

    for (const PanelItem& item : layout.items) {
        if (item.control != control) {
            continue;
        }

        const float unit = normalizedSliderValue(item, static_cast<float>(x));
        markCustomLook();
        if (control == PanelControl::IntensitySlider) {
            settings_.intensity = 0.15f + unit * (4.0f - 0.15f);
            setStatus(L"Intensity: " + std::to_wstring(settings_.intensity));
        } else if (control == PanelControl::SpeedSlider) {
            settings_.speed = 0.1f + unit * (4.0f - 0.1f);
            setStatus(L"Speed: " + std::to_wstring(settings_.speed));
        } else if (control == PanelControl::HueShiftSlider) {
            settings_.hueShift = unit;
            setStatus(hueShiftStatus(settings_.hueShift));
        } else if (control == PanelControl::DepthSlider) {
            settings_.depth3D = unit;
            setStatus(L"3D depth: " + std::to_wstring(static_cast<int>(std::round(settings_.depth3D * 100.0f))) + L"%");
        } else if (control == PanelControl::ObjectDensitySlider) {
            settings_.objectDensity3D = unit;
            setStatus(L"3D objects: " +
                      std::to_wstring(static_cast<int>(std::round(settings_.objectDensity3D * 100.0f))) + L"%");
        } else if (control == PanelControl::InteractionDepthSlider) {
            settings_.interactionDepth = unit;
            setStatus(L"Mouse 3D: " +
                      std::to_wstring(static_cast<int>(std::round(settings_.interactionDepth * 100.0f))) + L"%");
        } else if (control == PanelControl::LightingGlowSlider) {
            settings_.lightingGlow = unit;
            setStatus(L"3D glow: " +
                      std::to_wstring(static_cast<int>(std::round(settings_.lightingGlow * 100.0f))) + L"%");
        } else if (control == PanelControl::ColorImpactSlider) {
            settings_.colorImpact = unit;
            setStatus(L"Color impact: " + std::to_wstring(static_cast<int>(std::round(settings_.colorImpact * 100.0f))) + L"%");
        } else if (control == PanelControl::ScenePersonalitySlider) {
            settings_.scenePersonality = unit;
            setStatus(L"Scene personality: " +
                      std::to_wstring(static_cast<int>(std::round(settings_.scenePersonality * 100.0f))) + L"%");
        } else if (control == PanelControl::Response3DSlider) {
            settings_.response3D = unit;
            setStatus(L"3D response: " +
                      std::to_wstring(static_cast<int>(std::round(settings_.response3D * 100.0f))) + L"%");
        } else if (control == PanelControl::MotionStabilitySlider) {
            settings_.motionStability = unit;
            setStatus(L"Motion stability: " +
                      std::to_wstring(static_cast<int>(std::round(settings_.motionStability * 100.0f))) + L"%");
        } else if (control == PanelControl::PatternClaritySlider) {
            settings_.patternClarity = unit;
            setStatus(L"Pattern clarity: " +
                      std::to_wstring(static_cast<int>(std::round(settings_.patternClarity * 100.0f))) + L"%");
        } else if (control == PanelControl::ComplexitySlider) {
            settings_.complexity = 0.35f + unit * (1.8f - 0.35f);
            setStatus(L"Complexity: " + std::to_wstring(settings_.complexity));
        } else if (control == PanelControl::QualitySlider) {
            settings_.adaptiveQuality = false;
            settings_.qualityScale = 0.45f + unit * (1.0f - 0.45f);
            performanceTracker_.reset();
            performanceStats_ = performanceTracker_.stats();
            performanceStats_.qualityScale = settings_.qualityScale;
            setStatus(L"Manual quality: " + std::to_wstring(settings_.qualityScale));
        }
        return;
    }
}

void WindowsApp::setStatus(std::wstring status)
{
    status_ = std::move(status);
}

LRESULT WindowsApp::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_SIZE:
        renderer_.resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE:
            PostQuitMessage(0);
            return 0;
        case VK_F11:
            toggleFullscreen();
            return 0;
        case VK_UP:
            adjustIntensity(0.1f);
            return 0;
        case VK_DOWN:
            adjustIntensity(-0.1f);
            return 0;
        case VK_RIGHT:
            adjustSpeed(0.1f);
            return 0;
        case VK_LEFT:
            adjustSpeed(-0.1f);
            return 0;
        case '1':
            setMode(VisualMode::QuantumTunnel);
            return 0;
        case '2':
            setMode(VisualMode::TechnoMandala);
            return 0;
        case '3':
            setMode(VisualMode::LissajousMesh);
            return 0;
        case '4':
            setMode(VisualMode::FrequencyBloom);
            return 0;
        case '5':
            setMode(VisualMode::FractalCathedral);
            return 0;
        case '6':
            setMode(VisualMode::PolyrhythmLattice);
            return 0;
        case '7':
            setMode(VisualMode::SpectralOrigami);
            return 0;
        case '8':
            setMode(VisualMode::ChromaKaleidoscope);
            return 0;
        case '9':
            setMode(VisualMode::HyperspacePolytope);
            return 0;
        case '0':
            setMode(VisualMode::PhaseWeave);
            return 0;
        case 'M':
            setMode(VisualMode::ResonanceTessellation);
            return 0;
        case 'Y':
            setMode(VisualMode::NeuralConstellation);
            return 0;
        case 'Z':
            setMode(VisualMode::CymaticInterference);
            return 0;
        case 'C':
            cyclePalette();
            return 0;
        case 'U':
            adjustHueShift(1.0f / 12.0f);
            return 0;
        case 'D':
            adjustDepth3D(0.1f);
            return 0;
        case 'F':
            adjustColorImpact(0.1f);
            return 0;
        case 'G':
            markCustomLook();
            settings_.motionStyle = nextMotionStyle(settings_.motionStyle, 1);
            sceneDirector_.reset();
            setStatus(L"Motion style: " + widen(toString(settings_.motionStyle)));
            return 0;
        case 'V':
            resetCurrentAudioProfiles();
            return 0;
        case 'X':
            adjustComplexity(0.1f);
            return 0;
        case 'H':
            markCustomLook();
            settings_.showHud = !settings_.showHud;
            return 0;
        case 'I':
            toggleInteractiveField();
            return 0;
        case 'E':
            toggleEnvironmentReactive();
            return 0;
        case 'Q':
            toggleAdaptiveQuality();
            return 0;
        case 'A':
            toggleAutoScene();
            return 0;
        case 'B':
            applyCuratedPreset(-1);
            return 0;
        case 'N':
            applyCuratedPreset(1);
            return 0;
        case VK_OEM_4:
            applyUserPreset(-1);
            return 0;
        case VK_OEM_6:
            applyUserPreset(1);
            return 0;
        case 'K':
            saveCurrentUserPreset();
            return 0;
        case 'T':
            toggleTrails();
            return 0;
        case 'L':
            useLoopback();
            return 0;
        case 'O':
            openAudioDialog();
            return 0;
        case 'R':
            toggleRecording();
            return 0;
        case 'S':
            savePresetDialog();
            return 0;
        case 'P':
            loadPresetDialog();
            return 0;
        default:
            break;
        }
        break;
    case WM_MOUSEMOVE:
        if (handlePanelPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (wParam & MK_LBUTTON) != 0, false)) {
            return 0;
        }
        updateMouseInteraction(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (wParam & MK_LBUTTON) != 0);
        return 0;
    case WM_LBUTTONDOWN:
        if (handlePanelPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true, true)) {
            SetCapture(window_);
            return 0;
        }
        SetCapture(window_);
        updateMouseInteraction(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true);
        return 0;
    case WM_LBUTTONUP:
        activeSlider_ = PanelControl::None;
        ReleaseCapture();
        if (handlePanelPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false, false)) {
            return 0;
        }
        updateMouseInteraction(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
        return 0;
    case WM_MOUSELEAVE:
        interaction_.active = false;
        interaction_.pressed = false;
        interaction_.velocity = 0.0f;
        trackingMouse_ = false;
        return 0;
    case WM_DROPFILES: {
        const HDROP drop = reinterpret_cast<HDROP>(wParam);
        std::array<wchar_t, MAX_PATH> path{};
        if (DragQueryFileW(drop, 0, path.data(), static_cast<UINT>(path.size())) > 0) {
            loadAudioFileFromPath(path.data());
        }
        DragFinish(drop);
        return 0;
    }
    case WM_DESTROY:
        running_ = false;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}

LRESULT CALLBACK WindowsApp::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowsApp* app = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = reinterpret_cast<WindowsApp*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->window_ = window;
    } else {
        app = reinterpret_cast<WindowsApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (app != nullptr) {
        return app->handleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace viz
