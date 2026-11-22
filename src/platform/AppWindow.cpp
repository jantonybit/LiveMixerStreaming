#include "AppWindow.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

AppWindow::AppWindow(int width, int height, const char* title)
    : width_(width), height_(height), title_(title ? title : "LiveMixer Streaming")
{
}

AppWindow::~AppWindow()
{
}

bool AppWindow::createDevice()
{
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Width = static_cast<UINT>(width_);
    desc.BufferDesc.Height = static_cast<UINT>(height_);
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd_;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        &featureLevel, 1, D3D11_SDK_VERSION, &desc,
        &swapChain_, &device_, nullptr, &context_);

    if (FAILED(result)) {
        cleanupDevice();
        return false;
    }
    createRenderTarget();
    return true;
}

void AppWindow::cleanupDevice()
{
    cleanupRenderTarget();
    if (context_) { context_->Release(); context_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
}

void AppWindow::createRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        device_->CreateRenderTargetView(backBuffer, nullptr, &renderTarget_);
        backBuffer->Release();
    }
}

void AppWindow::cleanupRenderTarget()
{
    if (renderTarget_) { renderTarget_->Release(); renderTarget_ = nullptr; }
}

void AppWindow::toggleFullscreen()
{
    if (!hwnd_) return;

    if (!fullscreen_) {
        savedPlacement_.length = sizeof(savedPlacement_);
        GetWindowPlacement(hwnd_, &savedPlacement_);

        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);

        SetWindowLongPtr(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd_, HWND_TOPMOST,
                     monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                     monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        fullscreen_ = true;
    } else {
        SetWindowLongPtr(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPlacement(hwnd_, &savedPlacement_);
        SetWindowPos(hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        fullscreen_ = false;
    }
    UpdateWindow(hwnd_);
}

LRESULT CALLBACK AppWindow::wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<AppWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    // F11 alterna entre a janela maximizada e a tela cheia sem bordas.
    // O bit 0x40000000 do lParam indica repeticao automatica da tecla.
    if (message == WM_KEYDOWN && wParam == VK_F11 && !(lParam & 0x40000000u)) {
        if (self) self->toggleFullscreen();
        return 0;
    }

    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
        return true;

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return 0;
    }
    case WM_SIZE:
        if (self) {
            self->resizeWidth_ = LOWORD(lParam);
            self->resizeHeight_ = HIWORD(lParam);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

bool AppWindow::initialize(std::string* error)
{
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = TEXT("CentralDeAudioWindowClass");
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassEx(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        if (error) *error = "Falha ao registrar a classe da janela.";
        return false;
    }

    hwnd_ = CreateWindowEx(0, wc.lpszClassName, title_.c_str(),
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           width_, height_, nullptr, nullptr, wc.hInstance, this);
    if (!hwnd_) {
        if (error) *error = "Falha ao criar a janela principal.";
        return false;
    }

    if (!createDevice()) {
        if (error) *error = "DirectX 11 nao esta disponivel neste sistema.";
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (!ImGui_ImplWin32_Init(hwnd_) || !ImGui_ImplDX11_Init(device_, context_)) {
        if (error) *error = "Falha ao inicializar a interface grafica.";
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd_);
    return true;
}

int AppWindow::run(const std::function<void()>& onFrame, const std::function<void()>& onShutdown)
{
    bool running = true;
    while (running) {
        MSG message;
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
            if (message.message == WM_QUIT) running = false;
        }
        if (!running) break;

        if (resizeWidth_ > 0 && resizeHeight_ > 0) {
            cleanupRenderTarget();
            swapChain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            createRenderTarget();
            resizeWidth_ = resizeHeight_ = 0;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (onFrame) onFrame();

        ImGui::Render();
        context_->OMSetRenderTargets(1, &renderTarget_, nullptr);
        const float clearColor[4] = { 0.06f, 0.07f, 0.09f, 1.0f };
        context_->ClearRenderTargetView(renderTarget_, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swapChain_->Present(1, 0);
    }

    if (onShutdown) onShutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanupDevice();
    if (hwnd_) DestroyWindow(hwnd_);
    UnregisterClass(TEXT("CentralDeAudioWindowClass"), GetModuleHandle(nullptr));
    return 0;
}
