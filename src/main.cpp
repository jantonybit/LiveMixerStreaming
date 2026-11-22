#include "app/App.h"
#include "platform/AppWindow.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>

#include <string>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // O host ASIO usa COM para localizar e instanciar os drivers.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    AppWindow window(1280, 800, "LiveMixer Streaming");
    std::string error;
    if (!window.initialize(&error)) {
        MessageBoxA(nullptr, error.c_str(), "LiveMixer Streaming", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    App app(window);
    app.setupStyle();

    const int result = window.run(
        [&app]() { app.render(); },
        [&app]() { app.shutdown(); });

    CoUninitialize();
    return result;
}
