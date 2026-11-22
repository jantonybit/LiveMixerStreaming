#pragma once

#include <functional>
#include <memory>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;

// Janela Win32 + DirectX 11 usada apenas como superficie de renderizacao da ImGui.
// Nao instala nada no sistema.
class AppWindow
{
public:
    AppWindow(int width, int height, const char* title);
    ~AppWindow();

    bool initialize(std::string* error);
    int run(const std::function<void()>& onFrame, const std::function<void()>& onShutdown);

    HWND handle() const { return hwnd_; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void toggleFullscreen();

    bool createDevice();
    void cleanupDevice();
    void createRenderTarget();
    void cleanupRenderTarget();

    int width_ = 1280;
    int height_ = 800;
    std::string title_;
    HWND hwnd_ = nullptr;

    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11RenderTargetView* renderTarget_ = nullptr;

    int resizeWidth_ = 0;
    int resizeHeight_ = 0;

    bool fullscreen_ = false;
    WINDOWPLACEMENT savedPlacement_{};
};
