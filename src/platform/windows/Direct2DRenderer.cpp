#include "Direct2DRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace viz {
namespace {

constexpr float kPi = 3.14159265358979323846f;

template <typename T>
void safeRelease(T*& pointer)
{
    if (pointer != nullptr) {
        pointer->Release();
        pointer = nullptr;
    }
}

D2D1_COLOR_F toD2D(ColorRGBA color)
{
    return D2D1::ColorF(color.r, color.g, color.b, color.a);
}

D2D1_POINT_2F point(Vec2 value)
{
    return D2D1::Point2F(value.x, value.y);
}

Vec2 polar(Vec2 center, float radius, float angle)
{
    return Vec2{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
}

std::wstring widen(std::string_view value)
{
    return std::wstring(value.begin(), value.end());
}

} // namespace

Direct2DRenderer::~Direct2DRenderer()
{
    discardDeviceResources();
    safeRelease(textFormat_);
    safeRelease(writeFactory_);
    safeRelease(factory_);
}

bool Direct2DRenderer::initialize(HWND window, std::wstring& error)
{
    window_ = window;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_);
    if (FAILED(hr)) {
        error = L"Unable to create Direct2D factory.";
        return false;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(&writeFactory_));
    if (FAILED(hr)) {
        error = L"Unable to create DirectWrite factory.";
        return false;
    }

    hr = writeFactory_->CreateTextFormat(L"Consolas",
                                         nullptr,
                                         DWRITE_FONT_WEIGHT_MEDIUM,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL,
                                         15.0f,
                                         L"en-us",
                                         &textFormat_);
    if (FAILED(hr)) {
        error = L"Unable to create HUD text format.";
        return false;
    }
    textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return createDeviceResources(error);
}

bool Direct2DRenderer::createDeviceResources(std::wstring& error)
{
    if (renderTarget_ != nullptr) {
        return true;
    }

    RECT rect{};
    GetClientRect(window_, &rect);
    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max<LONG>(1, rect.right - rect.left)),
        static_cast<UINT32>(std::max<LONG>(1, rect.bottom - rect.top)));

    HRESULT hr = factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_HARDWARE,
                                     D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(window_, size, D2D1_PRESENT_OPTIONS_NONE),
        &renderTarget_);
    if (FAILED(hr)) {
        error = L"Unable to create Direct2D render target.";
        return false;
    }

    hr = renderTarget_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_);
    if (FAILED(hr)) {
        error = L"Unable to create Direct2D brush.";
        discardDeviceResources();
        return false;
    }

    renderTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    return true;
}

void Direct2DRenderer::discardDeviceResources()
{
    safeRelease(brush_);
    safeRelease(renderTarget_);
    hasPresentedFrame_ = false;
}

void Direct2DRenderer::resize(UINT width, UINT height)
{
    if (renderTarget_ != nullptr) {
        renderTarget_->Resize(D2D1::SizeU(std::max<UINT>(1, width), std::max<UINT>(1, height)));
        hasPresentedFrame_ = false;
    }
}

void Direct2DRenderer::render(const GeometryFrame& frame,
                              const AudioMetrics& metrics,
                              const VisualSettings& settings,
                              const PerformanceStats& performance,
                              const RuntimeInspectorState& inspector,
                              const std::wstring& status,
                              bool recording,
                              std::size_t recordedFrames)
{
    std::wstring error;
    if (!createDeviceResources(error)) {
        return;
    }

    renderTarget_->BeginDraw();
    const D2D1_SIZE_F renderSize = renderTarget_->GetSize();
    if (settings.trails && hasPresentedFrame_) {
        brush_->SetColor(D2D1::ColorF(frame.background.r, frame.background.g, frame.background.b, 0.16f));
        renderTarget_->FillRectangle(D2D1::RectF(0.0f, 0.0f, renderSize.width, renderSize.height), brush_);
    } else {
        renderTarget_->Clear(toD2D(frame.background));
    }

    for (const Beam& beam : frame.beams) {
        drawBeam(beam);
    }
    for (const Ring& ring : frame.rings) {
        drawRing(ring);
    }
    for (const Polyline& line : frame.polylines) {
        drawPolyline(line);
    }
    for (const Particle& particle : frame.particles) {
        drawParticle(particle);
    }

    if (frame.flash > 0.0f) {
        const D2D1_SIZE_F size = renderTarget_->GetSize();
        brush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, std::min(frame.flash, 0.32f)));
        renderTarget_->FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, size.height), brush_);
    }

    if (settings.showHud) {
        if (settings.trails) {
            brush_->SetColor(D2D1::ColorF(frame.background.r, frame.background.g, frame.background.b, 0.82f));
            renderTarget_->FillRectangle(D2D1::RectF(0.0f, 0.0f, renderSize.width, 160.0f), brush_);
        }
        drawHud(metrics, settings, performance, inspector, status, recording, recordedFrames);
        drawControlPanel(settings, recording);
        drawRuntimeInspector(inspector);
    }

    const HRESULT hr = renderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        discardDeviceResources();
    } else if (SUCCEEDED(hr)) {
        hasPresentedFrame_ = true;
    }
}

void Direct2DRenderer::drawRing(const Ring& ring)
{
    const int sides = std::max(3, ring.sides);
    brush_->SetColor(toD2D(ring.color));
    Vec2 previous = polar(ring.center, ring.radius, ring.rotation);
    for (int i = 1; i <= sides; ++i) {
        const float angle = ring.rotation + (static_cast<float>(i) / static_cast<float>(sides)) * 2.0f * kPi;
        const Vec2 next = polar(ring.center, ring.radius, angle);
        renderTarget_->DrawLine(point(previous), point(next), brush_, ring.strokeWidth);
        previous = next;
    }
}

void Direct2DRenderer::drawPolyline(const Polyline& line)
{
    if (line.points.size() < 2) {
        return;
    }

    brush_->SetColor(toD2D(line.color));
    for (std::size_t i = 1; i < line.points.size(); ++i) {
        renderTarget_->DrawLine(point(line.points[i - 1]), point(line.points[i]), brush_, line.strokeWidth);
    }
    if (line.closed) {
        renderTarget_->DrawLine(point(line.points.back()), point(line.points.front()), brush_, line.strokeWidth);
    }
}

void Direct2DRenderer::drawBeam(const Beam& beam)
{
    const D2D1_SIZE_F size = renderTarget_->GetSize();
    const Vec2 center{size.width * 0.5f, size.height * 0.5f};
    brush_->SetColor(toD2D(beam.color));
    renderTarget_->DrawLine(point(center), point(polar(center, beam.length, beam.angle)), brush_, beam.width);
}

void Direct2DRenderer::drawParticle(const Particle& particle)
{
    brush_->SetColor(toD2D(particle.color));
    renderTarget_->FillEllipse(D2D1::Ellipse(point(particle.position), particle.radius, particle.radius), brush_);
}

void Direct2DRenderer::drawHud(const AudioMetrics& metrics,
                               const VisualSettings& settings,
                               const PerformanceStats& performance,
                               const RuntimeInspectorState& inspector,
                               const std::wstring& status,
                               bool recording,
                               std::size_t recordedFrames)
{
    const D2D1_SIZE_F size = renderTarget_->GetSize();
    std::wostringstream text;
    text << L"Visualizer  |  " << widen(toString(settings.mode)) << L"  |  "
         << widen(toString(settings.palette)) << L"\n";
    text << L"RMS " << std::fixed << std::setprecision(2) << metrics.rms
         << L"  Bass " << metrics.bass
         << L"  Mid " << metrics.mid
         << L"  Treble " << metrics.treble
         << L"  BPM " << std::setprecision(1) << metrics.bpm
         << L"  Style " << widen(toString(metrics.style))
         << L" " << std::setprecision(0) << (metrics.styleConfidence * 100.0f) << L"%";
    if (metrics.keyIndex >= 0) {
        text << L"  Key " << widen(keyName(metrics.keyIndex))
             << L" " << widen(toString(metrics.keyMode))
             << L" " << std::setprecision(0) << (metrics.keyConfidence * 100.0f) << L"%";
    }
    text << L"  Section " << widen(toString(metrics.section))
         << L" " << std::setprecision(0) << (metrics.sectionConfidence * 100.0f) << L"%";
    text << (metrics.beat ? L"  BEAT" : L"") << L"\n";
    text << L"Intensity " << std::setprecision(2) << settings.intensity
         << L"  Speed " << settings.speed
         << L"  Hue " << std::setprecision(0) << (settings.hueShift * 360.0f)
         << L"  Complexity " << std::setprecision(2) << settings.complexity
         << L"  Flux " << std::setprecision(2) << metrics.spectralFlux
         << L"  Drop " << metrics.dropIntensity
         << L"  Beat " << metrics.beatPhase
         << L"  Bar " << metrics.barPhase
         << L"  Phrase " << metrics.phrasePhase
         << L"  Tension " << metrics.buildTension
         << (metrics.downbeat ? L"  DOWNBEAT" : L"")
         << (metrics.phraseBoundary ? L"  PHRASE" : L"") << L"\n";
    text << L"Audio AI Style " << std::setprecision(0) << (metrics.styleAdaptation * 100.0f) << L"%"
         << L"  Sync " << (metrics.syncAdaptation * 100.0f) << L"%"
         << L"  BeatSens " << std::setprecision(2) << metrics.beatSensitivity
         << L"  SectionSens " << metrics.sectionSensitivity << L"\n";
    text << L"FPS " << std::setprecision(1) << performance.fps
         << L"  Frame " << performance.averageFrameMs << L"ms"
         << L"  Core " << performance.averageCoreMs << L"ms"
         << L"  D2D " << performance.averageRenderMs << L"ms"
         << L"  Prims " << performance.primitiveCount
         << L"  Quality " << std::setprecision(0) << (settings.qualityScale * 100.0f) << L"%"
         << (performance.adaptiveQualityActive ? L" AUTO" : L"")
         << (settings.autoScene ? L"  SCENE" : L"")
         << (settings.sceneTransition > 0.01f ? L"  MORPH" : L"")
         << (settings.environmentReactive ? L"  ENV" : L"")
         << (settings.trails ? L"  TRAILS" : L"");
    if (recording) {
        text << L"  REC " << recordedFrames;
    }
    text << L"\nO audio  L loopback  V reset AI  0-9 modes  M tessellate  Y neural  Z cymatic  B/N looks  [/] user looks  K save user  A scene  E env  T trails  C palette  U hue  X complexity  I interact  S/P files  R record  H HUD  F11 fullscreen\n";
    const std::string hudLook = inspector.activeLook.empty() ? "Custom" : inspector.activeLook;
    text << L"Source " << widen(inspector.sourceLabel)
         << L"  Look " << widen(hudLook)
         << L"\n";
    if (!status.empty()) {
        text << status;
    }

    const std::wstring output = text.str();
    brush_->SetColor(D2D1::ColorF(0.94f, 0.96f, 1.0f, 0.88f));
    renderTarget_->DrawTextW(output.c_str(),
                             static_cast<UINT32>(output.size()),
                             textFormat_,
                             D2D1::RectF(16.0f, 14.0f, size.width - 16.0f, 178.0f),
                             brush_);
}

void Direct2DRenderer::drawControlPanel(const VisualSettings& settings, bool recording)
{
    const D2D1_SIZE_F size = renderTarget_->GetSize();
    const ControlPanelLayout layout = buildControlPanelLayout(size.width, size.height, settings, recording);

    brush_->SetColor(D2D1::ColorF(0.015f, 0.018f, 0.026f, 0.78f));
    renderTarget_->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(layout.panel.left, layout.panel.top, layout.panel.right, layout.panel.bottom),
                          6.0f,
                          6.0f),
        brush_);
    brush_->SetColor(D2D1::ColorF(0.28f, 0.34f, 0.42f, 0.72f));
    renderTarget_->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(layout.panel.left, layout.panel.top, layout.panel.right, layout.panel.bottom),
                          6.0f,
                          6.0f),
        brush_,
        1.0f);

    brush_->SetColor(D2D1::ColorF(0.94f, 0.96f, 1.0f, 0.92f));
    const std::wstring title = L"CONTROL";
    renderTarget_->DrawTextW(title.c_str(),
                             static_cast<UINT32>(title.size()),
                             textFormat_,
                             D2D1::RectF(layout.panel.left + 14.0f,
                                         layout.panel.top + 4.0f,
                                         layout.panel.right - 14.0f,
                                         layout.panel.top + 26.0f),
                             brush_);

    for (const PanelItem& item : layout.items) {
        const D2D1_RECT_F rect = D2D1::RectF(item.rect.left, item.rect.top, item.rect.right, item.rect.bottom);
        if (item.slider) {
            brush_->SetColor(D2D1::ColorF(0.07f, 0.085f, 0.12f, 0.82f));
            renderTarget_->FillRoundedRectangle(D2D1::RoundedRect(rect, 4.0f, 4.0f), brush_);
            const float filledRight = item.rect.left + (item.rect.right - item.rect.left) * item.value;
            brush_->SetColor(D2D1::ColorF(0.03f, 0.96f, 1.0f, 0.62f));
            renderTarget_->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(item.rect.left, item.rect.top, filledRight, item.rect.bottom),
                                  4.0f,
                                  4.0f),
                brush_);
        } else {
            brush_->SetColor(item.active
                                 ? D2D1::ColorF(0.03f, 0.96f, 1.0f, 0.32f)
                                 : D2D1::ColorF(0.06f, 0.07f, 0.095f, 0.78f));
            renderTarget_->FillRoundedRectangle(D2D1::RoundedRect(rect, 4.0f, 4.0f), brush_);
        }

        brush_->SetColor(item.active
                             ? D2D1::ColorF(0.58f, 1.0f, 1.0f, 0.9f)
                             : D2D1::ColorF(0.30f, 0.36f, 0.46f, 0.74f));
        renderTarget_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 4.0f, 4.0f), brush_, 1.0f);

        const std::wstring label(item.label.begin(), item.label.end());
        brush_->SetColor(D2D1::ColorF(0.94f, 0.96f, 1.0f, 0.9f));
        renderTarget_->DrawTextW(label.c_str(),
                                 static_cast<UINT32>(label.size()),
                                 textFormat_,
                                 D2D1::RectF(item.rect.left + 9.0f,
                                             item.rect.top + 4.0f,
                                             item.rect.right - 9.0f,
                                             item.rect.bottom + 2.0f),
                                 brush_);
    }
}

void Direct2DRenderer::drawRuntimeInspector(const RuntimeInspectorState& inspector)
{
    const D2D1_SIZE_F size = renderTarget_->GetSize();
    if (size.width < 980.0f || size.height < 520.0f) {
        return;
    }

    const std::vector<std::string> lines = formatRuntimeInspectorLines(inspector);
    if (lines.empty()) {
        return;
    }

    const float panelWidth = std::clamp(size.width * 0.25f, 300.0f, 390.0f);
    const float left = size.width - panelWidth - 16.0f;
    const float top = 184.0f;
    const float lineHeight = 19.0f;
    const float panelHeight = 34.0f + lineHeight * static_cast<float>(lines.size());
    const float bottom = std::min(size.height - 16.0f, top + panelHeight);

    brush_->SetColor(D2D1::ColorF(0.015f, 0.018f, 0.026f, 0.76f));
    renderTarget_->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(left, top, size.width - 16.0f, bottom), 6.0f, 6.0f),
        brush_);
    brush_->SetColor(D2D1::ColorF(0.28f, 0.34f, 0.42f, 0.7f));
    renderTarget_->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(left, top, size.width - 16.0f, bottom), 6.0f, 6.0f),
        brush_,
        1.0f);

    brush_->SetColor(D2D1::ColorF(0.94f, 0.96f, 1.0f, 0.92f));
    const std::wstring title = L"INSPECTOR";
    renderTarget_->DrawTextW(title.c_str(),
                             static_cast<UINT32>(title.size()),
                             textFormat_,
                             D2D1::RectF(left + 14.0f, top + 5.0f, size.width - 30.0f, top + 28.0f),
                             brush_);

    float y = top + 30.0f;
    for (std::size_t i = 0; i < lines.size() && y + lineHeight <= bottom; ++i) {
        const bool heading = i == 0 || lines[i].rfind("LOOK ", 0) == 0 ||
                             lines[i].rfind("Style ", 0) == 0 ||
                             lines[i].rfind("REC ", 0) == 0;
        brush_->SetColor(heading
                             ? D2D1::ColorF(0.58f, 1.0f, 1.0f, 0.9f)
                             : D2D1::ColorF(0.86f, 0.9f, 1.0f, 0.84f));
        const std::wstring line = widen(lines[i]);
        renderTarget_->DrawTextW(line.c_str(),
                                 static_cast<UINT32>(line.size()),
                                 textFormat_,
                                 D2D1::RectF(left + 14.0f, y, size.width - 30.0f, y + lineHeight),
                                 brush_);
        y += lineHeight;
    }
}

} // namespace viz
