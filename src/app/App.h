#pragma once

#include "../audio/AppAudioCapture.h"
#include "../audio/AsioDevice.h"
#include "../audio/WasapiEngine.h"
#include "../midi/MidiEngine.h"
#include "../mixer/Mixer.h"
#include "../platform/AppWindow.h"
#include "../ui/Widgets.h"

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

enum class AppLanguage { Portuguese = 0, English = 1 };
enum class AppTheme { Midnight = 0, Gold = 1, Ocean = 2, Light = 3 };

class App
{
public:
    struct Scene
    {
        std::string name;
        std::string path;
    };

    explicit App(AppWindow& window);
    ~App();

    void setupStyle();
    void render();
    void shutdown();

    void setLanguage(AppLanguage language);
    void setTheme(AppTheme theme);

private:
    struct PromoEntry
    {
        std::string name;
        std::string productId;
        std::string description;
    };

    void refreshDrivers();
    void refreshWasapiDevices();
    void refreshAppCapture();
    void prepareAudioBuffers();
    std::vector<std::string> inputNames() const;
    bool loopbackWouldFeedback(int deviceIndex) const;
    void audioCallback(float* const* deviceInputs, float** deviceOutputs,
                       int deviceInputCount, int deviceOutputCount, int frames);
    bool startEngine();
    void stopEngine(bool keepStatus = false);
    void drawBranding();
    void drawToolbar();
    void drawFxRack();
    void drawChannel(ChannelStrip& strip, int index);
    void drawMaster();
    void drawStatusBar();
    void drawMidiPanel();
    void drawConflictDialog();
    void drawScenesPanel();
    bool drawGearButton();
    void drawSettingsPanel();
    void applyTheme();
    void loadUiConfig();
    void saveUiConfig() const;
    void loadPromoConfig();
    void openStorePublisher();
    void openStoreProduct(const std::string& productId) const;
    void openManual(AppLanguage language);
    void refreshScenes();
    bool saveScene(const std::string& name);
    bool loadScene(size_t index);
    void deleteScene(size_t index);
    void markMidi(const MidiTarget& target);
    widgets::RightClickCallback learnCallback(const MidiTarget& target,
                                              float minimum, float maximum);

    AppWindow& window_;
    Mixer mixer_;
    AsioDevice device_;
    WasapiEngine wasapi_;
    AppAudioCapture appCapture_;
    MidiEngine midi_;

    std::vector<std::string> drivers_;
    int selectedDriver_ = 0;
    double sampleRate_ = 48000.0;
    long bufferSize_ = 256;
    int requestedInputs_ = 8;
    int requestedOutputs_ = 2;

    // Roteamento WASAPI (Fase 1).
    std::vector<WasapiEngine::DeviceInfo> wasapiDevices_;
    int selectedLoopback_ = -1; // -1 = desativado
    bool muteLoopback_ = true;  // captura o PC sem ouvir na saida fisica
    bool divertApps_ = false;   // silencia a sessao Windows dos apps capturados
    int selectedOutput_ = -1;   // -1 = desativado
    bool wasapiClock_ = false;  // true = WASAPI e o relogio mestre (sem ASIO)
    int maxAudioFrames_ = 8192;

    // Entrada fisica via WASAPI (microfone). O nome e salvo para sobreviver a
    // mudanca de indice dos dispositivos no Windows.
    std::vector<WasapiEngine::DeviceInfo> captureDevices_;
    int selectedMicrophone_ = -1; // -1 = desativado
    std::string microphoneName_;

    // Buffers pre-alocados: a thread de audio nunca deve alocar memoria.
    std::vector<float> loopbackLeft_;
    std::vector<float> loopbackRight_;
    std::vector<float> micLeft_;
    std::vector<float> micRight_;
    std::vector<std::vector<float>> appLeft_;
    std::vector<std::vector<float>> appRight_;
    std::vector<float> silentChannel_;
    std::vector<float> clockOutLeft_;
    std::vector<float> clockOutRight_;
    std::vector<float*> mixInputs_;
    std::vector<float*> mixOutputs_;

    ImFont* titleFont_ = nullptr;
    // Painel da engrenagem: 14 pt convertidos para pixels (96 dpi).
    ImFont* settingsFont_ = nullptr;  // texto normal
    ImFont* sectionFont_ = nullptr;   // titulos em negrito
    ImFont* subtitleFont_ = nullptr;

    // Fator aplicado aos controles dos canais/Master quando a janela tem pouca
    // altura (768p, ou Windows com escala 125/150%). Evita cortar o fader.
    float controlScale_ = 1.0f;
    // Y (relativo ao canal) onde comeca a faixa de faders. Todos os canais e o
    // Master usam o mesmo valor, deixando os faders alinhados na base da janela.
    float faderTopY_ = 0.0f;

    bool running_ = false;
    std::atomic<bool> dspReady_{false}; // evita processar ate taxas/streams estarem prontos
    std::chrono::steady_clock::time_point lastAppRefresh_{};
    bool showMidi_ = false;
    bool showScenes_ = false;
    bool showSettings_ = false;
    AppLanguage language_ = AppLanguage::Portuguese;
    AppTheme theme_ = AppTheme::Midnight;
    std::string status_;
    std::string uiConfigPath_;
    std::string promoConfigPath_;
    std::string publisherName_ = "Jantonybit";
    std::vector<PromoEntry> promos_;
    std::string configPath_;
    std::string midiPath_;
    std::string scenesDir_;
    std::string currentScene_;
    std::vector<Scene> scenes_;
    char sceneNameBuffer_[128] = {};
};
