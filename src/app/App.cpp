#include "App.h"

#include "../ui/Widgets.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace {

constexpr int kMaxAppCaptures = 16;

// Dourado medio usado no titulo e no subtitulo do aplicativo.
const ImVec4 kBrandGold = ImVec4(0.85f, 0.70f, 0.30f, 1.0f);
const ImVec4 kBrandGoldDark = ImVec4(0.52f, 0.38f, 0.10f, 1.0f);

// Cores dos knobs por funcao (legenda do mixer). Valores fixos: a cor
// acompanha a funcao do controle e nao e editavel pelo usuario.
const ImVec4 kKnobGain      = ImVec4(235.0f / 255.0f,  65.0f / 255.0f,  55.0f / 255.0f, 1.0f); // vermelho
const ImVec4 kKnobEqLowHigh = ImVec4(255.0f / 255.0f, 205.0f / 255.0f,  45.0f / 255.0f, 1.0f); // amarelo
const ImVec4 kKnobEqMid     = ImVec4(255.0f / 255.0f, 150.0f / 255.0f,  40.0f / 255.0f, 1.0f); // laranja (medios)
const ImVec4 kKnobDynamics  = ImVec4( 60.0f / 255.0f, 150.0f / 255.0f, 255.0f / 255.0f, 1.0f); // azul
const ImVec4 kKnobFx        = ImVec4(190.0f / 255.0f,  90.0f / 255.0f, 255.0f / 255.0f, 1.0f); // roxo/magenta
const ImVec4 kKnobSend      = ImVec4( 60.0f / 255.0f, 220.0f / 255.0f,  90.0f / 255.0f, 1.0f); // verde
const ImVec4 kKnobNeutral   = ImVec4(220.0f / 255.0f, 220.0f / 255.0f, 220.0f / 255.0f, 1.0f); // branco/cinza

AppLanguage g_language = AppLanguage::Portuguese;
AppTheme g_theme = AppTheme::Midnight;

const ImVec4& brandColor()
{
    return g_theme == AppTheme::Light ? kBrandGoldDark : kBrandGold;
}

// Traducao leve: a chave e o texto original em portugues. Isso preserva as
// mensagens em PT e permite alternar para EN sem mexer na logica da interface.
const char* tr(const char* portuguese)
{
    if (g_language == AppLanguage::Portuguese) return portuguese;
    static const std::unordered_map<std::string, const char*> english = {
        { "Driver ASIO", "ASIO Driver" },
        { "Nenhum driver ASIO", "No ASIO driver" },
        { "Taxa", "Rate" },
        { "Buffer", "Buffer" },
        { "Entradas", "Inputs" },
        { "Saídas", "Outputs" },
        { "Canais", "Channels" },
        { "Modo", "Mode" },
        { "Audio do PC", "PC Audio" },
        { "Nao ouvir no PC", "Mute PC output" },
        { "Desviar apps do Windows", "Divert Windows apps" },
        { "Atualizar disp.", "Refresh devices" },
        { "Atualizar apps", "Refresh apps" },
        { "INICIAR", "START" },
        { "PARAR", "STOP" },
        { "Entrada", "Input" },
        { "Ganho", "Gain" },
        { "Pan", "Pan" },
        { "HPF", "HPF" },
        { "Equalizacao", "EQ" },
        { "Envios p/ efeitos", "FX sends" },
        { "Fader", "Fader" },
        { "Retorno", "Return" },
        { "Tempo", "Time" },
        { "Feedback", "Feedback" },
        { "Mix", "Mix" },
        { "Tamanho", "Size" },
        { "Damp", "Damp" },
        { "Profund.", "Depth" },
        { "Selecione um efeito", "Select an effect" },
        { "MASTER", "MASTER" },
        { "Master", "Master" },
        { "Limiter ativo (soft clip)", "Limiter active (soft clip)" },
        { "MIDI - controladores", "MIDI - controllers" },
        { "Atualizar lista", "Refresh list" },
        { "Cancelar learn", "Cancel learn" },
        { "Limpar mapa", "Clear map" },
        { "Dispositivo", "Device" },
        { "Evento", "Event" },
        { "Alvo", "Target" },
        { "Remover", "Remove" },
        { "Nenhum dispositivo MIDI", "No MIDI device" },
        { "Conflito de MIDI", "MIDI conflict" },
        { "Comando atual", "Current command" },
        { "Novo comando", "New command" },
        { "Substituir", "Replace" },
        { "Usar em conjunto", "Use together" },
        { "Cancelar", "Cancel" },
        { "Cenas", "Scenes" },
        { "Salvar cena", "Save scene" },
        { "Carregar", "Load" },
        { "Excluir", "Delete" },
        { "Nome da cena", "Scene name" },
        { "Nova cena", "New scene" },
        { "Configurações", "Settings" },
        { "Idioma", "Language" },
        { "Português", "Portuguese" },
        { "English", "English" },
        { "Manual do usuario", "User manual" },
        { "Abrir manual (Português)", "Open manual (Portuguese)" },
        { "Abrir manual (English)", "Open manual (English)" },
        { "Tema", "Theme" },
        { "Meia-noite", "Midnight" },
        { "Dourado", "Gold" },
        { "Oceano", "Ocean" },
        { "Claro", "Light" },
        { "Aplicativos do mesmo Segmento", "Same-category apps" },
        { "Outros Aplicativos do Desenvolvedor", "Other apps by the developer" },
        { "Ver todos os meus apps", "See all my apps" },
        { "Abre a Microsoft Store com todos os apps do autor", "Opens Microsoft Store with all apps by the author" },
        { "Nao foi possivel abrir a Store", "Could not open the Store" },
        { "Fechar", "Close" },
        { "Versao", "Version" },
        { "Manual nao encontrado: ", "Manual not found: " },
        { "Pronto", "Ready" },
        { "Solo", "Solo" },
        { "Mute", "Mute" },
        { "Fase", "Phase" },
        { "Frequencia", "Frequency" },
        { "Send Delay", "Send Delay" },
        { "Send Reverb", "Send Reverb" },
        { "Send Chorus", "Send Chorus" },
        { "Retorno FX", "FX return" },
        { "Parametro FX", "FX parameter" },
        { "Canal", "Channel" },
        { "Medio graves", "Low-mid" },
        { "Medio agudos", "High-mid" },
        { "Graves", "Low" },
        { "Agudos", "High" },
        { "Profundidade", "Depth" },
        { "Master - Fader", "Master - Fader" },
        { "%s - Retorno", "%s - Return" },
        { "Canal %d - EQ %s", "Channel %d - EQ %s" },
        { "Canal %d - %s", "Channel %d - %s" },
        { "canal MIDI %d", "MIDI channel %d" },
        { "%s %d (canal MIDI %d)", "%s %d (MIDI channel %d)" }
    };
    auto it = english.find(portuguese);
    return it != english.end() ? it->second : portuguese;
}

// Traducao direta para textos novos: evita depender de chaves exatas do mapa.
const char* tr(const char* portuguese, const char* english)
{
    return g_language == AppLanguage::Portuguese ? portuguese : english;
}

std::string executableDirectory()
{
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string full(path, length > 0 ? static_cast<size_t>(length) : 0u);
    const size_t slash = full.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : full.substr(0, slash);
}

std::string trimCopy(const std::string& text)
{
    size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

// Codifica uma string UTF-8 para uso em query de URI. CARACTERES NAO
// reservados permanecem como estao; todo o resto vira %HH.
std::string percentEncode(const std::string& text)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        const bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                          c == '.' || c == '~';
        if (safe) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) return std::wstring();
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &out[0], size);
    out.resize(static_cast<size_t>(size - 1));
    return out;
}

bool launchUri(const std::wstring& uri)
{
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", uri.c_str(),
                                           nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

void coloredText(const ImVec4& color, const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

std::string wideToUtf8Local(const std::wstring& text)
{
    if (text.empty()) return std::string();
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0,
                                         nullptr, nullptr);
    std::string result(static_cast<size_t>(size > 0 ? size : 0), '\0');
    if (size > 0)
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                            result.data(), size, nullptr, nullptr);
    return result;
}

// Nomes dos tres efeitos fixos do rack (indice = indice do barramento no Mixer).
const char* fxName(int index)
{
    static const char* names[] = { "Delay", "Reverb", "Chorus" };
    return index >= 0 && index < kNumAux ? names[index] : "FX";
}

const char* midiTargetTypeName(MidiTargetType type)
{
    switch (type) {
    case MidiTargetType::ChannelFader: return tr("Fader");
    case MidiTargetType::ChannelGain: return tr("Ganho");
    case MidiTargetType::ChannelPan: return tr("Pan");
    case MidiTargetType::ChannelMute: return tr("Mute");
    case MidiTargetType::ChannelSolo: return tr("Solo");
    case MidiTargetType::ChannelSend1: return tr("Send Delay");
    case MidiTargetType::ChannelSend2: return tr("Send Reverb");
    case MidiTargetType::ChannelSend3: return tr("Send Chorus");
    case MidiTargetType::ChannelHighpass: return tr("HPF");
    case MidiTargetType::ChannelPhase: return tr("Fase");
    case MidiTargetType::ChannelEqGain: return tr("EQ");
    case MidiTargetType::MasterFader: return tr("Master");
    case MidiTargetType::AuxReturn: return tr("Retorno FX");
    case MidiTargetType::FxParam: return tr("Parametro FX");
    case MidiTargetType::None:
    default: return "?";
    }
}

// Nome legivel do parametro, incluindo canal/banda, para o dialogo de conflito.
std::string describeMidiTarget(const MidiTarget& target)
{
    char text[160] = {};
    if (target.type == MidiTargetType::MasterFader) {
        std::snprintf(text, sizeof(text), tr("Master - Fader"));
    } else if (target.type == MidiTargetType::AuxReturn) {
        std::snprintf(text, sizeof(text), tr("%s - Retorno"), fxName(target.index));
    } else if (target.type == MidiTargetType::FxParam) {
        static const char* names[] = { "Tempo", "Feedback", "Mix", "Tamanho", "Damp",
                                       "Mix", "Taxa", "Profundidade", "Mix" };
        const char* parameter = target.sub >= 0 && target.sub < 9 ? tr(names[target.sub]) : "?";
        std::snprintf(text, sizeof(text), "%s - %s", fxName(target.index), parameter);
    } else if (target.type == MidiTargetType::ChannelEqGain) {
        static const char* bands[] = { "Graves", "Medio graves", "Medio agudos", "Agudos" };
        const char* band = target.sub >= 0 && target.sub < 4 ? tr(bands[target.sub]) : "?";
        std::snprintf(text, sizeof(text), tr("Canal %d - EQ %s"), target.index + 1, band);
    } else {
        std::snprintf(text, sizeof(text), tr("Canal %d - %s"), target.index + 1,
                      midiTargetTypeName(target.type));
    }
    return text;
}

std::string describeMidiEvent(const MidiBinding& binding)
{
    const char* kind = binding.kind == 0 ? "CC" : (binding.kind == 1 ? "NOTE" : "PITCH");
    char text[128] = {};
    std::snprintf(text, sizeof(text), tr("%s %d (canal MIDI %d)"), kind, binding.data,
                  binding.channel + 1);
    return text;
}

std::vector<std::string> deviceTokens(const std::string& text)
{
    std::vector<std::string> result;
    std::string current;
    for (const char c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u)) {
            current += static_cast<char>(std::tolower(u));
        } else if (!current.empty()) {
            result.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) result.push_back(current);
    return result;
}

bool isGenericDeviceToken(const std::string& token)
{
    if (token.size() < 3) return false;
    static const char* generic[] = { "asio", "driver", "audio", "device", "sound",
                                     "speakers", "output", "input", "the" };
    for (const char* word : generic)
        if (token == word) return true;
    return false;
}

// Compara heuristically o nome do driver ASIO com o nome do endpoint WASAPI.
// Ex.: "UMC ASIO Driver" x "OUT 1-2 (BEHRINGER UMC 204HD 192k)" -> mesmo hardware.
bool samePhysicalDevice(const std::string& asioName, const std::string& wasapiName)
{
    const auto a = deviceTokens(asioName);
    const auto b = deviceTokens(wasapiName);
    for (const auto& left : a) {
        if (isGenericDeviceToken(left)) continue;
        for (const auto& right : b)
            if (left == right) return true;
    }
    return false;
}

std::string sanitizeSceneName(const std::string& name)
{
    std::string result;
    result.reserve(name.size());
    for (const char c : name) {
        const bool invalid = c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
                             c == '"' || c == '<' || c == '>' || c == '|';
        result += invalid ? '_' : c;
    }
    while (!result.empty() && result.back() == ' ') result.pop_back();
    const size_t first = result.find_first_not_of(' ');
    return first == std::string::npos ? std::string() : result.substr(first);
}

} // namespace

App::App(AppWindow& window) : window_(window), midi_(mixer_)
{
    const std::string base = executableDirectory();
    const std::string configDir = base + "\\config";
    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);
    configPath_ = configDir + "\\mixer.cfg";
    midiPath_ = configDir + "\\midi.map";
    uiConfigPath_ = configDir + "\\ui.cfg";
    promoConfigPath_ = configDir + "\\promos.cfg";
    scenesDir_ = configDir + "\\scenes";
    std::filesystem::create_directories(scenesDir_, ec);
    loadUiConfig();
    saveUiConfig(); // garante que o ui.cfg exista com idioma/tema/microfone
    loadPromoConfig();
    if (std::filesystem::exists(configPath_, ec))
        mixer_.load(configPath_);
    if (std::filesystem::exists(midiPath_, ec))
        midi_.load(midiPath_);
    midi_.refresh();
    refreshScenes();

    refreshDrivers();
    if (!drivers_.empty()) selectedDriver_ = 0;
    refreshWasapiDevices();
}

App::~App()
{
    stopEngine();
}

void App::setupStyle()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.WindowPadding = ImVec2(8.0f, 8.0f);

    applyTheme();

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFont* uiFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.0f);
    if (!uiFont) uiFont = io.Fonts->AddFontDefault();
    titleFont_ = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 26.0f);
    if (!titleFont_) titleFont_ = uiFont;

    // 14 pt a 96 dpi = 18,67 px. Usado em todo o painel de configuracoes;
    // a variante negrito identifica os titulos de cada secao.
    const float settingsFontSizePx = 14.0f * 4.0f / 3.0f;
    settingsFont_ = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",
                                                 settingsFontSizePx);
    if (!settingsFont_) settingsFont_ = uiFont;
    sectionFont_ = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf",
                                                settingsFontSizePx);
    if (!sectionFont_) sectionFont_ = settingsFont_;

    subtitleFont_ = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 11.0f);
    if (!subtitleFont_) subtitleFont_ = uiFont;
    io.FontDefault = uiFont;
}

void App::applyTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 windowBg, childBg, header, headerHover, headerActive;
    ImVec4 button, buttonHover, buttonActive, grab, grabActive, text, frameBg;

    switch (theme_) {
    case AppTheme::Gold:
        windowBg = ImVec4(0.10f, 0.08f, 0.06f, 1.0f);
        childBg = ImVec4(0.15f, 0.12f, 0.09f, 1.0f);
        header = ImVec4(0.35f, 0.26f, 0.12f, 1.0f);
        headerHover = ImVec4(0.45f, 0.34f, 0.16f, 1.0f);
        headerActive = ImVec4(0.55f, 0.42f, 0.20f, 1.0f);
        button = ImVec4(0.30f, 0.23f, 0.13f, 1.0f);
        buttonHover = ImVec4(0.40f, 0.31f, 0.17f, 1.0f);
        buttonActive = ImVec4(0.52f, 0.40f, 0.22f, 1.0f);
        grab = ImVec4(0.85f, 0.70f, 0.30f, 1.0f);
        grabActive = ImVec4(0.95f, 0.82f, 0.42f, 1.0f);
        text = ImVec4(0.95f, 0.91f, 0.82f, 1.0f);
        frameBg = ImVec4(0.20f, 0.16f, 0.11f, 1.0f);
        break;
    case AppTheme::Ocean:
        windowBg = ImVec4(0.05f, 0.09f, 0.10f, 1.0f);
        childBg = ImVec4(0.08f, 0.14f, 0.15f, 1.0f);
        header = ImVec4(0.10f, 0.30f, 0.32f, 1.0f);
        headerHover = ImVec4(0.13f, 0.40f, 0.42f, 1.0f);
        headerActive = ImVec4(0.17f, 0.52f, 0.54f, 1.0f);
        button = ImVec4(0.12f, 0.28f, 0.30f, 1.0f);
        buttonHover = ImVec4(0.17f, 0.38f, 0.40f, 1.0f);
        buttonActive = ImVec4(0.23f, 0.50f, 0.52f, 1.0f);
        grab = ImVec4(0.20f, 0.70f, 0.72f, 1.0f);
        grabActive = ImVec4(0.30f, 0.86f, 0.88f, 1.0f);
        text = ImVec4(0.86f, 0.95f, 0.95f, 1.0f);
        frameBg = ImVec4(0.10f, 0.20f, 0.21f, 1.0f);
        break;
    case AppTheme::Light:
        windowBg = ImVec4(0.92f, 0.93f, 0.95f, 1.0f);
        childBg = ImVec4(0.86f, 0.88f, 0.91f, 1.0f);
        header = ImVec4(0.70f, 0.78f, 0.88f, 1.0f);
        headerHover = ImVec4(0.62f, 0.72f, 0.84f, 1.0f);
        headerActive = ImVec4(0.54f, 0.66f, 0.80f, 1.0f);
        button = ImVec4(0.78f, 0.82f, 0.88f, 1.0f);
        buttonHover = ImVec4(0.70f, 0.76f, 0.85f, 1.0f);
        buttonActive = ImVec4(0.62f, 0.70f, 0.81f, 1.0f);
        grab = ImVec4(0.20f, 0.45f, 0.75f, 1.0f);
        grabActive = ImVec4(0.12f, 0.34f, 0.65f, 1.0f);
        text = ImVec4(0.10f, 0.12f, 0.15f, 1.0f);
        frameBg = ImVec4(0.80f, 0.83f, 0.87f, 1.0f);
        break;
    case AppTheme::Midnight:
    default:
        windowBg = ImVec4(0.070f, 0.080f, 0.100f, 1.0f);
        childBg = ImVec4(0.100f, 0.115f, 0.140f, 1.0f);
        header = ImVec4(0.170f, 0.230f, 0.330f, 1.0f);
        headerHover = ImVec4(0.220f, 0.320f, 0.450f, 1.0f);
        headerActive = ImVec4(0.260f, 0.390f, 0.550f, 1.0f);
        button = ImVec4(0.180f, 0.210f, 0.260f, 1.0f);
        buttonHover = ImVec4(0.240f, 0.290f, 0.370f, 1.0f);
        buttonActive = ImVec4(0.300f, 0.370f, 0.470f, 1.0f);
        grab = ImVec4(0.320f, 0.560f, 0.820f, 1.0f);
        grabActive = ImVec4(0.420f, 0.680f, 0.950f, 1.0f);
        text = ImVec4(0.92f, 0.94f, 0.98f, 1.0f);
        frameBg = ImVec4(0.13f, 0.15f, 0.19f, 1.0f);
        break;
    }

    style.Colors[ImGuiCol_WindowBg] = windowBg;
    style.Colors[ImGuiCol_ChildBg] = childBg;
    style.Colors[ImGuiCol_Header] = header;
    style.Colors[ImGuiCol_HeaderHovered] = headerHover;
    style.Colors[ImGuiCol_HeaderActive] = headerActive;
    style.Colors[ImGuiCol_Button] = button;
    style.Colors[ImGuiCol_ButtonHovered] = buttonHover;
    style.Colors[ImGuiCol_ButtonActive] = buttonActive;
    style.Colors[ImGuiCol_SliderGrab] = grab;
    style.Colors[ImGuiCol_SliderGrabActive] = grabActive;
    style.Colors[ImGuiCol_Text] = text;
    style.Colors[ImGuiCol_FrameBg] = frameBg;
    style.Colors[ImGuiCol_FrameBgHovered] = frameBg;
    style.Colors[ImGuiCol_FrameBgActive] = frameBg;
}

void App::loadUiConfig()
{
    std::ifstream in(uiConfigPath_);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("language=", 0) == 0) {
            const int value = std::atoi(line.c_str() + 9);
            language_ = value == 1 ? AppLanguage::English : AppLanguage::Portuguese;
        } else if (line.rfind("theme=", 0) == 0) {
            const int value = std::atoi(line.c_str() + 6);
            theme_ = value >= 0 && value <= 3 ? static_cast<AppTheme>(value) : AppTheme::Midnight;
        } else if (line.rfind("microphone=", 0) == 0) {
            microphoneName_ = line.substr(11);
        }
    }
    g_language = language_;
    g_theme = theme_;
}

void App::saveUiConfig() const
{
    std::ofstream out(uiConfigPath_);
    if (!out.is_open()) return;
    out << "language=" << static_cast<int>(language_) << '\n';
    out << "theme=" << static_cast<int>(theme_) << '\n';
    out << "microphone=" << microphoneName_ << '\n';
}

void App::loadPromoConfig()
{
    promos_.clear();
    publisherName_ = "Jantonybit";

    std::ifstream in(promoConfigPath_);
    if (!in.is_open()) {
        std::ofstream out(promoConfigPath_);
        if (out.is_open()) {
            out << "# Aplicativos do mesmo Segmento\n";
            out << "# Formato: Nome|ProductId|Descricao curta\n";
            out << "DJMasterDeck|9PD722LB588M|Estação De audio Profissional para DJ\n";
            out << "PlayLive|9NJ8XTJT12F6|Player de audio para apresentacoes ao vivo\n";
            out << "\n";
            out << "# Outros Aplicativos do Desenvolvedor\n";
            out << "publisher=Jantonybit\n";
        }
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        const std::string text = trimCopy(line);
        if (text.empty() || text[0] == '#') continue;

        if (text.rfind("publisher=", 0) == 0) {
            const std::string value = trimCopy(text.substr(10));
            if (!value.empty()) publisherName_ = value;
            continue;
        }

        const size_t sep1 = text.find('|');
        if (sep1 == std::string::npos) continue;
        const size_t sep2 = text.find('|', sep1 + 1);
        const std::string name = trimCopy(text.substr(0, sep1));
        const std::string rawId = trimCopy(text.substr(
            sep1 + 1, sep2 == std::string::npos ? std::string::npos : sep2 - sep1 - 1));
        const std::string description = sep2 == std::string::npos
            ? std::string() : trimCopy(text.substr(sep2 + 1));

        std::string productId;
        for (char c : rawId) {
            if (std::isalnum(static_cast<unsigned char>(c)))
                productId.push_back(c);
        }
        if (name.empty() || productId.empty()) continue;
        promos_.push_back({ name, productId, description });
    }
}

void App::openStorePublisher()
{
    const std::string query = percentEncode(publisherName_);
    const std::wstring primary =
        utf8ToWide("ms-windows-store://publisher/?name=" + query);
    if (launchUri(primary)) return;

    const std::wstring fallback =
        utf8ToWide("ms-windows-store://search/?query=" + query);
    if (launchUri(fallback)) return;

    launchUri(L"https://apps.microsoft.com/");
    status_ = std::string(tr("Nao foi possivel abrir a Store")) + ": " + publisherName_;
}

void App::openStoreProduct(const std::string& productId) const
{
    std::string safeId;
    for (char c : productId) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            safeId.push_back(c);
    }
    if (safeId.empty()) return;

    const std::wstring uri =
        utf8ToWide("ms-windows-store://pdp/?ProductId=" + safeId);
    if (launchUri(uri)) return;
    launchUri(utf8ToWide("https://apps.microsoft.com/detail/" + safeId));
}

void App::setLanguage(AppLanguage language)
{
    language_ = language;
    g_language = language;
    saveUiConfig();
}

void App::setTheme(AppTheme theme)
{
    theme_ = theme;
    g_theme = theme;
    applyTheme();
    saveUiConfig();
}

void App::refreshDrivers()
{
    drivers_ = AsioDevice::enumerateDrivers();
    if (selectedDriver_ >= static_cast<int>(drivers_.size()))
        selectedDriver_ = 0;
    status_ = drivers_.empty()
        ? tr("Nenhum driver ASIO encontrado. Instale o driver da sua interface de audio.",
             "No ASIO driver found. Install your audio interface's driver.")
        : std::string(tr("Drivers ASIO encontrados: ", "ASIO drivers found: ")) +
              std::to_string(drivers_.size());
}

void App::refreshWasapiDevices()
{
    const int oldLoopback = selectedLoopback_;
    const int oldOutput = selectedOutput_;
    wasapiDevices_ = WasapiEngine::enumerateDevices();
    captureDevices_ = WasapiEngine::enumerateCaptureDevices();

    const int count = static_cast<int>(wasapiDevices_.size());
    selectedLoopback_ = oldLoopback >= 0 && oldLoopback < count ? oldLoopback : -1;
    selectedOutput_ = oldOutput >= 0 && oldOutput < count ? oldOutput : -1;

    // O microfone e identificado pelo nome: trocar a ordem dos dispositivos
    // no Windows nao muda a entrada escolhida nas faixas.
    selectedMicrophone_ = -1;
    if (!microphoneName_.empty()) {
        for (size_t i = 0; i < captureDevices_.size(); ++i) {
            if (wideToUtf8Local(captureDevices_[i].name) == microphoneName_) {
                selectedMicrophone_ = static_cast<int>(i);
                break;
            }
        }
    }
}

void App::prepareAudioBuffers()
{
    const size_t frames = static_cast<size_t>(maxAudioFrames_);
    loopbackLeft_.assign(frames, 0.0f);
    loopbackRight_.assign(frames, 0.0f);
    micLeft_.assign(frames, 0.0f);
    micRight_.assign(frames, 0.0f);
    silentChannel_.assign(frames, 0.0f);
    clockOutLeft_.assign(frames, 0.0f);
    clockOutRight_.assign(frames, 0.0f);
    appLeft_.assign(kMaxAppCaptures, std::vector<float>(frames, 0.0f));
    appRight_.assign(kMaxAppCaptures, std::vector<float>(frames, 0.0f));
    // ASIO + loopback (2) + microfone WASAPI (2) + apps (2 por app).
    mixInputs_.assign(static_cast<size_t>(kMaxChannels) + 8 + 4 + 2 * kMaxAppCaptures, nullptr);
    mixOutputs_.assign(8, nullptr);
}

void App::refreshAppCapture()
{
    if (!running_) return;
    std::string error;
    appCapture_.refresh(::GetCurrentProcessId(), sampleRate_, &error);
    if (divertApps_) appCapture_.setAppsMuted(true, &error);
    mixer_.resolveInputNames(inputNames());
    if (!error.empty()) status_ = std::string(tr("Captura por aplicativo: ", "App capture: ")) + error;
    lastAppRefresh_ = std::chrono::steady_clock::now();
}

std::vector<std::string> App::inputNames() const
{
    std::vector<std::string> names;

    if (wasapiClock_) {
        names.push_back("SILENCIO");
    } else if (running_ && device_.isOpen()) {
        const auto& deviceNames = device_.info().inputNames;
        const int count = device_.info().numInputs;
        for (int i = 0; i < count; ++i) {
            if (i < static_cast<int>(deviceNames.size()) && !deviceNames[i].empty())
                names.push_back(deviceNames[i]);
            else
                names.push_back("IN " + std::to_string(i + 1));
        }
    } else {
        for (int i = 0; i < requestedInputs_; ++i)
            names.push_back("IN " + std::to_string(i + 1));
    }

    if (wasapi_.isLoopbackRunning()) {
        names.push_back("AUDIO PC L");
        names.push_back("AUDIO PC R");
    }

    // Microfone WASAPI: occupy duas entradas estereo, na mesma ordem usada
    // pelo audioCallback. O nome e estavel mesmo se o indice mudar.
    if (wasapi_.isMicrophoneRunning()) {
        std::string label = microphoneName_.empty()
                                ? std::string("MIC WASAPI")
                                : "MIC: " + microphoneName_;
        if (label.size() > 34) label = label.substr(0, 34);
        names.push_back(label + " L");
        names.push_back(label + " R");
    }

    // Aplicativos que estao gerando audio no Windows (exceto OBS/streaming e
    // este proprio aplicativo). Cada app ocupa uma entrada estereo L/R.
    const int appCount = std::min(appCapture_.count(), kMaxAppCaptures);
    for (int i = 0; i < appCount; ++i) {
        std::string label = appCapture_.name(i);
        if (label.size() > 30) label = label.substr(0, 30);
        names.push_back(label + " L");
        names.push_back(label + " R");
    }

    if (names.empty()) names.push_back("SILENCIO");
    return names;
}

// Um loopback so e seguro se o dispositivo capturado nao estiver na mesma
// cadeia fisica das entradas monitoradas (monitoramento direto da interface)
// nem na mesma saida usada pelo sink do Master Out.
bool App::loopbackWouldFeedback(int deviceIndex) const
{
    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(wasapiDevices_.size()))
        return false;

    // Mesmo endpoint usado como saida: o sink e capturado de volta.
    if (deviceIndex == selectedOutput_) return true;

    // No modo ASIO, endpoints do mesmo hardware da interface fecham o ciclo
    // acustico (PC -> caixa -> mic -> entrada ASIO). Com "Nao ouvir no PC" o
    // endpoint fica mudo e o loopback captura antes do volume, entao o ciclo
    // e quebrado e a selecao e permitida.
    if (!muteLoopback_ && !wasapiClock_ && !drivers_.empty() && selectedDriver_ >= 0 &&
        selectedDriver_ < static_cast<int>(drivers_.size())) {
        const std::string asioName = drivers_[static_cast<size_t>(selectedDriver_)];
        const std::string wasapiName = wideToUtf8Local(wasapiDevices_[static_cast<size_t>(deviceIndex)].name);
        if (samePhysicalDevice(asioName, wasapiName)) return true;
    }
    return false;
}

// Executado na thread de audio (ASIO ou WASAPI). Nao aloca memoria.
void App::audioCallback(float* const* deviceInputs, float** deviceOutputs,
                        int deviceInputCount, int deviceOutputCount, int frames)
{
    if (frames <= 0 || deviceOutputCount <= 0) return;
    frames = std::min(frames, maxAudioFrames_);

    if (!dspReady_.load(std::memory_order_acquire)) {
        for (int i = 0; i < deviceOutputCount; ++i)
            std::fill(deviceOutputs[i], deviceOutputs[i] + frames, 0.0f);
        return;
    }

    const int baseInputs = std::max(0, deviceInputCount);
    const bool withLoopback = wasapi_.isLoopbackRunning();
    const bool withMic = wasapi_.isMicrophoneRunning();
    const int appCount = std::min(appCapture_.count(), kMaxAppCaptures);
    const int totalInputs = baseInputs + (withLoopback ? 2 : 0) + (withMic ? 2 : 0)
                            + 2 * appCount;

    for (int i = 0; i < baseInputs; ++i)
        mixInputs_[static_cast<size_t>(i)] = deviceInputs ? deviceInputs[i] : silentChannel_.data();
    int inputIndex = baseInputs;
    if (withLoopback) {
        wasapi_.readLoopback(loopbackLeft_.data(), loopbackRight_.data(), frames);
        mixInputs_[static_cast<size_t>(inputIndex++)] = loopbackLeft_.data();
        mixInputs_[static_cast<size_t>(inputIndex++)] = loopbackRight_.data();
    }
    if (withMic) {
        wasapi_.readMicrophone(micLeft_.data(), micRight_.data(), frames);
        mixInputs_[static_cast<size_t>(inputIndex++)] = micLeft_.data();
        mixInputs_[static_cast<size_t>(inputIndex++)] = micRight_.data();
    }
    for (int i = 0; i < appCount; ++i) {
        float* left = appLeft_[static_cast<size_t>(i)].data();
        float* right = appRight_[static_cast<size_t>(i)].data();
        if (!appCapture_.read(i, left, right, frames)) {
            std::fill(left, left + frames, 0.0f);
            std::fill(right, right + frames, 0.0f);
        }
        mixInputs_[static_cast<size_t>(inputIndex++)] = left;
        mixInputs_[static_cast<size_t>(inputIndex++)] = right;
    }

    const int outputCount = std::min(deviceOutputCount, static_cast<int>(mixOutputs_.size()));
    for (int i = 0; i < outputCount; ++i)
        mixOutputs_[static_cast<size_t>(i)] = deviceOutputs[i];

    mixer_.process(mixInputs_.data(), mixOutputs_.data(), totalInputs, outputCount, frames);

    // O sink WASAPI (Master Out) recebe o master produzido pelo mixer.
    if (wasapi_.isOutputRunning()) {
        const float* left = mixOutputs_[0];
        const float* right = outputCount > 1 ? mixOutputs_[1] : mixOutputs_[0];
        wasapi_.writeOutput(left, right, frames);
    }
}

bool App::startEngine()
{
    if (!wasapiClock_ && drivers_.empty()) {
        refreshDrivers();
        if (drivers_.empty()) return false;
    }
    if (wasapiClock_ &&
        (selectedOutput_ < 0 || selectedOutput_ >= static_cast<int>(wasapiDevices_.size()))) {
        status_ = tr("ERRO: no modo WASAPI, escolha uma saida de audio.",
                     "ERROR: in WASAPI mode, choose an audio output.");
        return false;
    }
    if (selectedLoopback_ >= 0 && loopbackWouldFeedback(selectedLoopback_)) {
        status_ = tr("ERRO: 'Audio do PC' neste dispositivo causa realimentacao "
                     "(monitoramento direto da interface ou Master Out). "
                     "Escolha outro dispositivo ou desative o Audio do PC.",
                     "ERROR: 'PC Audio' on this device causes feedback "
                     "(direct interface monitoring or Master Out). "
                     "Choose another device or disable PC Audio.");
        return false;
    }

    std::string error;
    dspReady_.store(false, std::memory_order_release);
    prepareAudioBuffers();
    mixer_.prepare(sampleRate_, maxAudioFrames_);
    mixer_.resetMeters();
    running_ = true;

    // O microfone WASAPI precisa da taxa efetiva do relogio (ASIO ou WASAPI),
    // por isso e iniciado em cada branch abaixo, apos essa taxa ser conhecida.
    auto startMic = [this, &error]() -> bool {
        if (selectedMicrophone_ < 0) return true;
        if (!wasapi_.startMicrophone(selectedMicrophone_, sampleRate_, &error)) {
            status_ = std::string(tr("ERRO no microfone WASAPI: ", "WASAPI microphone error: ")) + error;
            stopEngine(true);
            return false;
        }
        return true;
    };

    // Loopback: captura o audio que o Windows envia para o dispositivo escolhido.
    // Com muteLoopback_ o endpoint e silenciado no Windows, mas o loopback
    // continua capturando em nivel cheio (game/musica entra no mixer sem sair
    // nas caixas).
    if (selectedLoopback_ >= 0 &&
        !wasapi_.startLoopback(selectedLoopback_, sampleRate_, muteLoopback_, &error)) {
        status_ = std::string(tr("ERRO no loopback: ", "Loopback error: ")) + error;
        stopEngine(true);
        return false;
    }

    if (wasapiClock_) {
        // Sem driver ASIO: a thread WASAPI fornece o relogio e chama o mixer.
        if (!wasapi_.startOutputClock(selectedOutput_, sampleRate_,
                [this](float* left, float* right, int frames) {
                    float* outputs[2] = { left, right };
                    audioCallback(nullptr, outputs, 0, 2, frames);
                }, &error)) {
            status_ = std::string(tr("ERRO na saida WASAPI: ", "WASAPI output error: ")) + error;
            stopEngine(true);
            return false;
        }
        // O WASAPI define a taxa real: o mixer e os filtros devem usar essa taxa.
        const int effectiveRate = wasapi_.outputRate() > 0
                                  ? wasapi_.outputRate() : static_cast<int>(sampleRate_);
        sampleRate_ = static_cast<double>(effectiveRate);
        mixer_.prepare(sampleRate_, maxAudioFrames_);
        mixer_.resetMeters();
        if (!startMic()) return false;

        char message[256];
        std::snprintf(message, sizeof(message),
                      tr("WASAPI ativo | %s | %.1f kHz | Master Out pronto",
                         "WASAPI active | %s | %.1f kHz | Master Out ready"),
                      wideToUtf8Local(wasapiDevices_[static_cast<size_t>(selectedOutput_)].name).c_str(),
                      sampleRate_ / 1000.0);
        status_ = message;
        if (wasapi_.isLoopbackRunning()) status_ += tr(" | audio do PC capturado", " | PC audio captured");
        if (wasapi_.isLoopbackMuted()) status_ += tr(" (mudo)", " (muted)");
        if (wasapi_.isMicrophoneRunning())
            status_ += tr(" | microfone WASAPI ativo", " | WASAPI mic active");
        appCapture_.stopAll();
        refreshAppCapture();
        if (appCapture_.count() > 0)
            status_ += " | " + std::to_string(appCapture_.count()) + tr(" app(s) capturado(s)", " app(s) captured");
        dspReady_.store(true, std::memory_order_release);
        return true;
    }

    // Saida sink: o mix do ASIO e empurrado para uma ring buffer e tocado no WASAPI.
    if (selectedOutput_ >= 0 &&
        !wasapi_.startOutputSink(selectedOutput_, sampleRate_, &error)) {
        status_ = std::string(tr("ERRO na saida WASAPI: ", "WASAPI output error: ")) + error;
        stopEngine(true);
        return false;
    }

    if (!device_.open(selectedDriver_, sampleRate_, bufferSize_,
                      requestedInputs_, requestedOutputs_, window_.handle(), &error)) {
        status_ = std::string(tr("ERRO: ", "ERROR: ")) + error;
        stopEngine(true);
        return false;
    }

    // O driver pode ter ajustado a taxa pedida; preparar o DSP com a taxa real.
    if (device_.info().sampleRate > 0.0)
        mixer_.prepare(device_.info().sampleRate, maxAudioFrames_);
    if (!startMic()) return false;

    const int actualInputs = device_.info().numInputs;
    const int actualOutputs = device_.info().numOutputs;
    device_.setProcessCallback([this, actualInputs, actualOutputs](float* const* inputs, float** outputs, int frames) {
        audioCallback(inputs, outputs, actualInputs, actualOutputs, frames);
    });

    if (!device_.start(&error)) {
        status_ = std::string(tr("ERRO: ", "ERROR: ")) + error;
        stopEngine(true);
        return false;
    }

    const double latencyMs = 1000.0 * (device_.info().inputLatency + device_.info().outputLatency)
                             / std::max(1.0, device_.info().sampleRate);
    char message[512];
    std::snprintf(message, sizeof(message),
                  tr("ASIO ativo | %s | %.1f kHz | buffer %ld | E/S %d/%d | latencia %.2f ms",
                     "ASIO active | %s | %.1f kHz | buffer %ld | I/O %d/%d | latency %.2f ms"),
                  drivers_[static_cast<size_t>(selectedDriver_)].c_str(),
                  device_.info().sampleRate / 1000.0, device_.info().bufferSize,
                  actualInputs, actualOutputs, latencyMs);
    status_ = message;
    if (wasapi_.isLoopbackRunning()) status_ += tr(" | audio do PC capturado", " | PC audio captured");
    if (wasapi_.isLoopbackMuted()) status_ += tr(" (mudo)", " (muted)");
    if (wasapi_.isMicrophoneRunning())
        status_ += tr(" | microfone WASAPI ativo", " | WASAPI mic active");
    if (wasapi_.isOutputRunning()) status_ += tr(" | Master Out ativo", " | Master Out active");
    appCapture_.stopAll();
    refreshAppCapture();
    if (appCapture_.count() > 0)
        status_ += " | " + std::to_string(appCapture_.count()) + tr(" app(s) capturado(s)", " app(s) captured");
    dspReady_.store(true, std::memory_order_release);
    return true;
}

void App::stopEngine(bool keepStatus)
{
    dspReady_.store(false, std::memory_order_release);
    device_.stop();
    device_.close();
    wasapi_.stopOutput();
    wasapi_.stopLoopback();
    wasapi_.stopMicrophone();
    appCapture_.stopAll();
    running_ = false;
    mixer_.resetMeters();
    if (!keepStatus) status_ = tr("Motor parado.", "Engine stopped.");
}

void App::render()
{
    midi_.poll();

    // Apps podem comecar/parar de gerar audio a qualquer momento.
    if (running_ && std::chrono::steady_clock::now() - lastAppRefresh_ >
                        std::chrono::milliseconds(1500)) {
        refreshAppCapture();
    }

    if (running_ && device_.consumeResetRequest()) {
        stopEngine();
        startEngine();
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("LiveMixer Streaming", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

    drawBranding();
    ImGui::Separator();
    drawToolbar();
    ImGui::Separator();

    ImGui::BeginChild("Canais", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    const float stripHeight = ImGui::GetContentRegionAvail().y - 10.0f;
    // upper(s) = 236 + 175*s (medido); faderTop = strip - 150*s - 25.
    // Igualando os dois, os controles terminam exatamente onde o fader comeca,
    // sem espaco vazio. O teto evita controles gigantes em telas muito altas.
    controlScale_ = dsp::clamp((stripHeight - 261.0f) / 325.0f, 0.45f, 1.50f);
    faderTopY_ = std::max(0.0f, stripHeight - 150.0f * controlScale_ - 25.0f);

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int channelCount = std::max(1, mixer_.channelCount());
    // Rack de efeitos e Master compartilham exatamente a mesma largura. O
    // objetivo e eliminar o espaco livre no rack e apertura do Master sem
    // estourar a area util em resolucoes menores: primeiro reserva-se uma
    // fatia para as duas colunas laterais e os canais ficam com o restante.
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float gaps = gap * (static_cast<float>(channelCount) + 1.0f);
    const float targetSide = dsp::clamp(availableWidth * 0.095f, 112.0f, 176.0f);
    const float stripWidth = std::clamp(
        (availableWidth - 2.0f * targetSide - gaps) / static_cast<float>(channelCount),
        110.0f, 240.0f);
    const float sideWidth = std::clamp(
        (availableWidth - stripWidth * static_cast<float>(channelCount) - gaps) * 0.5f,
        112.0f, 240.0f);
    const float rackWidth = sideWidth;
    const float masterWidth = sideWidth;

    for (int c = 0; c < mixer_.channelCount(); ++c) {
        ImGui::SameLine();
        ImGui::PushID(c);
        ImGui::BeginChild(("##strip" + std::to_string(c)).c_str(),
                          ImVec2(stripWidth, stripHeight), true, 0);
        drawChannel(mixer_.channel(c), c);
        ImGui::EndChild();
        ImGui::PopID();
    }

    // Rack de efeitos volta a ficar entre os canais e o Master.
    ImGui::SameLine();
    ImGui::BeginChild("##fxrack", ImVec2(rackWidth, stripHeight), true, ImGuiWindowFlags_NoScrollbar);
    drawFxRack();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##master", ImVec2(masterWidth, stripHeight), true, ImGuiWindowFlags_NoScrollbar);
    drawMaster();
    ImGui::EndChild();
    ImGui::EndChild();

    drawStatusBar();
    ImGui::End();
    ImGui::PopStyleVar();

    drawMidiPanel();
    drawScenesPanel();
    drawConflictDialog();
    drawSettingsPanel();
}

bool App::drawGearButton()
{
    const float size = 24.0f;
    ImGui::PushID("##gear");
    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("gear", ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    const ImVec2 center(topLeft.x + size * 0.5f, topLeft.y + size * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 body = ImGui::GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    const ImU32 outline = ImGui::GetColorU32(ImGuiCol_Text);

    for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * (dsp::kPi / 4.0f) + 0.18f;
        const ImVec2 inner(center.x + std::cos(angle) * 8.0f,
                           center.y + std::sin(angle) * 8.0f);
        const ImVec2 outer(center.x + std::cos(angle) * 11.5f,
                           center.y + std::sin(angle) * 11.5f);
        drawList->AddLine(inner, outer, body, 3.0f);
    }
    drawList->AddCircleFilled(center, 8.0f, body, 20);
    drawList->AddCircle(center, 3.5f, outline, 20, 1.5f);

    if (hovered) ImGui::SetTooltip(tr("Configurações"));
    ImGui::PopID();
    return clicked;
}

void App::openManual(AppLanguage language)
{
    const char* name = language == AppLanguage::English ? "manual_en.html" : "manual_pt.html";
    const std::filesystem::path base = executableDirectory();
    std::filesystem::path file = base / "docs" / name;
    if (!std::filesystem::exists(file)) file = base.parent_path() / "docs" / name;
    if (!std::filesystem::exists(file)) {
        status_ = std::string(tr("Manual nao encontrado: ")) + file.string();
        return;
    }
    ShellExecuteW(nullptr, L"open", file.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void App::drawSettingsPanel()
{
    if (!showSettings_) return;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.40f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    const float maxWidth = std::max(220.0f, display.x - 16.0f);
    const float maxHeight = std::max(200.0f, display.y - 16.0f);
    ImGui::SetNextWindowSize(
        ImVec2(std::min(560.0f, maxWidth), std::min(500.0f, maxHeight)),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(std::min(420.0f, maxWidth), std::min(320.0f, maxHeight)),
        ImVec2(maxWidth, maxHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    ImGui::PushFont(sectionFont_); // titulo da janela em negrito
    if (!ImGui::Begin(tr("Configurações"), &showSettings_)) {
        ImGui::End();
        ImGui::PopFont();
        ImGui::PopStyleVar();
        return;
    }

    // Titulos de secao em negrito, na cor da marca.
    auto sectionTitle = [this](const char* text) {
        ImGui::PushFont(sectionFont_);
        coloredText(brandColor(), text);
        ImGui::PopFont();
    };

    ImGui::PushFont(settingsFont_); // todo o conteudo em 14 pt

    sectionTitle(tr("Idioma"));
    ImGui::Spacing();
    if (ImGui::RadioButton(tr("Português"), language_ == AppLanguage::Portuguese))
        setLanguage(AppLanguage::Portuguese);
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("English"), language_ == AppLanguage::English))
        setLanguage(AppLanguage::English);

    ImGui::Separator();
    sectionTitle(tr("Tema"));
    ImGui::Spacing();
    if (ImGui::RadioButton(tr("Meia-noite"), theme_ == AppTheme::Midnight))
        setTheme(AppTheme::Midnight);
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("Dourado"), theme_ == AppTheme::Gold))
        setTheme(AppTheme::Gold);
    if (ImGui::RadioButton(tr("Oceano"), theme_ == AppTheme::Ocean))
        setTheme(AppTheme::Ocean);
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("Claro"), theme_ == AppTheme::Light))
        setTheme(AppTheme::Light);

    ImGui::Separator();
    ImGui::Spacing();
    sectionTitle(tr("Aplicativos do mesmo Segmento"));
    ImGui::Spacing();
    for (size_t i = 0; i < promos_.size(); ++i) {
        const PromoEntry& promo = promos_[i];
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button(promo.name.c_str()))
            openStoreProduct(promo.productId);
        if (!promo.description.empty()) {
            ImGui::SameLine();
            ImGui::TextUnformatted(promo.description.c_str());
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    sectionTitle(tr("Outros Aplicativos do Desenvolvedor"));
    ImGui::Spacing();
    if (ImGui::Button(tr("Ver todos os meus apps")))
        openStorePublisher();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s",
                          tr("Abre a Microsoft Store com todos os apps do autor"));

    ImGui::Separator();
    sectionTitle(tr("Manual do usuario"));
    ImGui::Spacing();
    if (ImGui::Button(tr("Abrir manual (Português)")))
        openManual(AppLanguage::Portuguese);
    ImGui::SameLine();
    if (ImGui::Button(tr("Abrir manual (English)")))
        openManual(AppLanguage::English);

    ImGui::Separator();
    ImGui::TextDisabled("%s 1.0", tr("Versao"));
    if (ImGui::Button(tr("Fechar"))) showSettings_ = false;

    ImGui::PopFont();      // settingsFont_
    ImGui::End();
    ImGui::PopFont();      // sectionFont_ da barra de titulo
    ImGui::PopStyleVar();  // WindowPadding
}

void App::drawBranding()
{
    const ImVec2 cursorOrigin = ImGui::GetCursorPos();
    const float gearX = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - 24.0f;
    ImGui::SetCursorPos(ImVec2(gearX, cursorOrigin.y));
    if (drawGearButton()) showSettings_ = true;
    ImGui::SetCursorPos(cursorOrigin);

    ImGui::PushStyleColor(ImGuiCol_Text, brandColor());
    ImGui::PushFont(titleFont_);
    ImGui::TextUnformatted("LiveMixer Streaming");
    const float titleWidth = ImGui::GetItemRectSize().x;
    ImGui::PopFont();
    ImGui::PopStyleColor();

    // O subtítulo é desenhado caractere a caractere para que a frase inteira
    // ocupe exatamente a mesma largura do nome do aplicativo.
    static const char subtitle[] = "Sound Designer by Jantonybit";
    const size_t length = sizeof(subtitle) - 1;

    ImGui::PushFont(subtitleFont_);
    float naturalWidth = 0.0f;
    float glyphWidth[sizeof(subtitle)] = {};
    for (size_t i = 0; i < length; ++i) {
        glyphWidth[i] = ImGui::CalcTextSize(subtitle + i, subtitle + i + 1).x;
        naturalWidth += glyphWidth[i];
    }
    const float extra = length > 1 ? (titleWidth - naturalWidth) / static_cast<float>(length - 1) : 0.0f;
    const float lineHeight = ImGui::GetTextLineHeight();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImU32 color = ImGui::GetColorU32(brandColor());

    float x = origin.x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < length; ++i) {
        drawList->AddText(ImVec2(x, origin.y), color, subtitle + i, subtitle + i + 1);
        x += glyphWidth[i];
        if (i + 1 < length) x += extra;
    }
    ImGui::Dummy(ImVec2(titleWidth, lineHeight));
    ImGui::PopFont();
}

void App::drawToolbar()
{
    auto stringGetter = [](void* data, int index) -> const char* {
        auto* list = static_cast<std::vector<std::string>*>(data);
        return list->at(static_cast<size_t>(index)).c_str();
    };

    // Em telas estreitas quebra a linha quando nao cabe, em vez de cortar controles.
    auto fitSameLine = [](float width) {
        if (ImGui::GetContentRegionAvail().x < width)
            ImGui::NewLine();
        else
            ImGui::SameLine();
    };

    ImGui::BeginDisabled(running_ || wasapiClock_);

    ImGui::SetNextItemWidth(230.0f);
    if (drivers_.empty())
        ImGui::TextUnformatted(tr("Driver ASIO: nenhum", "No ASIO driver"));
    else
        ImGui::Combo(tr("Driver ASIO", "ASIO Driver"), &selectedDriver_, stringGetter, &drivers_,
                     static_cast<int>(drivers_.size()));

    fitSameLine(120.0f);
    static const double rateValues[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    static const char* rateLabels[] = { "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz", "192 kHz" };
    int rateIndex = 1;
    for (int i = 0; i < 5; ++i)
        if (std::abs(sampleRate_ - rateValues[i]) < 10.0) rateIndex = i;
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo(tr("Taxa", "Rate"), &rateIndex, rateLabels, 5)) sampleRate_ = rateValues[rateIndex];

    fitSameLine(110.0f);
    static const long bufferValues[] = { 32, 64, 128, 256, 512, 1024 };
    static const char* bufferLabels[] = { "32", "64", "128", "256", "512", "1024" };
    int bufferIndex = 3;
    for (int i = 0; i < 6; ++i)
        if (bufferSize_ == bufferValues[i]) bufferIndex = i;
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo(tr("Buffer", "Buffer"), &bufferIndex, bufferLabels, 6)) bufferSize_ = bufferValues[bufferIndex];

    fitSameLine(85.0f);
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputInt(tr("Entradas", "Inputs"), &requestedInputs_);
    requestedInputs_ = dsp::clamp(static_cast<float>(requestedInputs_), 1.0f, 16.0f);
    fitSameLine(85.0f);
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputInt(tr("Saidas", "Outputs"), &requestedOutputs_);
    requestedOutputs_ = dsp::clamp(static_cast<float>(requestedOutputs_), 1.0f, 8.0f);

    fitSameLine(165.0f);
    ImGui::SetNextItemWidth(150.0f);
    int channels = mixer_.channelCount();
    if (ImGui::SliderInt(tr("Canais", "Channels"), &channels, 1, kMaxChannels)) mixer_.setChannelCount(channels);

    ImGui::EndDisabled();

    fitSameLine(95.0f);
    if (!running_) {
        if (widgets::ToggleButton(tr("INICIAR", "START"), false, ImVec4(0.10f, 0.55f, 0.28f, 1.0f), 90.0f))
            startEngine();
    } else {
        if (widgets::ToggleButton(tr("PARAR", "STOP"), false, ImVec4(0.70f, 0.20f, 0.18f, 1.0f), 90.0f))
            stopEngine();
    }

    fitSameLine(150.0f);
    ImGui::BeginDisabled(wasapiClock_);
    if (ImGui::Button(tr("Painel do driver", "Driver panel"))) device_.controlPanel();
    ImGui::EndDisabled();

    fitSameLine(70.0f);
    if (widgets::ToggleButton("MIDI", showMidi_, ImVec4(0.20f, 0.45f, 0.70f, 1.0f), 60.0f))
        showMidi_ = !showMidi_;

    fitSameLine(120.0f);
    if (ImGui::Button(tr("Salvar cena", "Save scene"))) {
        saveScene(currentScene_.empty() ? tr("Cena 1", "Scene 1") : currentScene_);
    }
    fitSameLine(75.0f);
    if (widgets::ToggleButton(tr("Cenas", "Scenes"), showScenes_, ImVec4(0.30f, 0.40f, 0.55f, 1.0f), 65.0f))
        showScenes_ = !showScenes_;

    // Segunda linha: roteamento de audio do computador e saida Master Out.
    ImGui::NewLine();

    ImGui::BeginDisabled(running_);

    const char* modeLabels[] = { "ASIO", "WASAPI" };
    int modeIndex = wasapiClock_ ? 1 : 0;
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::Combo(tr("Modo", "Mode"), &modeIndex, modeLabels, 2)) wasapiClock_ = modeIndex == 1;

    std::vector<std::string> loopItems;
    std::vector<bool> loopBlocked;
    loopItems.reserve(wasapiDevices_.size() + 1);
    loopBlocked.reserve(wasapiDevices_.size() + 1);
    loopItems.push_back(tr("(desativado)", "(disabled)"));
    loopBlocked.push_back(false);
    for (size_t i = 0; i < wasapiDevices_.size(); ++i) {
        const bool blocked = loopbackWouldFeedback(static_cast<int>(i));
        loopBlocked.push_back(blocked);
        loopItems.push_back(wideToUtf8Local(wasapiDevices_[i].name) +
                            (blocked ? tr("  [REALIMENTACAO]", "  [FEEDBACK]") : ""));
    }
    int loopIndex = selectedLoopback_ + 1;
    fitSameLine(230.0f);
    ImGui::SetNextItemWidth(215.0f);
    if (ImGui::Combo(tr("Audio do PC", "PC Audio"), &loopIndex, stringGetter, &loopItems,
                     static_cast<int>(loopItems.size()))) {
        const int chosen = loopIndex - 1;
        if (chosen >= 0 && loopBlocked[static_cast<size_t>(chosen)]) {
            status_ = tr("Audio do PC bloqueado: esse dispositivo fecha o ciclo de realimentacao.",
                         "PC Audio blocked: this device creates a feedback loop.");
        } else {
            selectedLoopback_ = chosen;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Captura TUDO que o Windows envia para o dispositivo escolhido.\n"
                             "Para capturar so um jogo/player, ajuste a saida de audio do app\n"
                             "para esse dispositivo e deixe os outros em outra saida.\n"
                             "Depois selecione AUDIO PC L / AUDIO PC R como Entrada das faixas.",
                             "Captures EVERYTHING Windows sends to the chosen device.\n"
                             "To capture only one game/player, set that app's audio output\n"
                             "to this device and leave the others on another output.\n"
                             "Then select PC AUDIO L / PC AUDIO R as the Input of the channels."));
    }
    if (selectedLoopback_ >= 0 && loopbackWouldFeedback(selectedLoopback_)) {
        fitSameLine(210.0f);
        coloredText(ImVec4(1.0f, 0.35f, 0.30f, 1.0f),
                    tr("RISCO DE REALIMENTACAO", "FEEDBACK RISK"));
    }

    std::vector<std::string> outputItems;
    outputItems.reserve(wasapiDevices_.size() + 1);
    outputItems.push_back(tr("(desativado)", "(disabled)"));
    for (const auto& device : wasapiDevices_) outputItems.push_back(wideToUtf8Local(device.name));
    int outputIndex = selectedOutput_ + 1;
    fitSameLine(230.0f);
    ImGui::SetNextItemWidth(215.0f);
    if (ImGui::Combo("Master Out", &outputIndex, stringGetter, &outputItems,
                     static_cast<int>(outputItems.size()))) {
        const int chosen = outputIndex - 1;
        if (chosen >= 0 && chosen == selectedLoopback_) {
            status_ = tr("Master Out bloqueado: mesmo dispositivo do Audio do PC causa realimentacao.",
                         "Master Out blocked: same device as PC Audio causes feedback.");
        } else {
            selectedOutput_ = chosen;
        }
    }

    std::vector<std::string> micItems;
    micItems.reserve(captureDevices_.size() + 1);
    micItems.push_back(tr("(desativado)", "(disabled)"));
    for (const auto& device : captureDevices_)
        micItems.push_back(wideToUtf8Local(device.name));
    int micIndex = selectedMicrophone_ + 1;
    fitSameLine(230.0f);
    ImGui::SetNextItemWidth(215.0f);
    if (ImGui::Combo(tr("Mic WASAPI", "WASAPI mic"), &micIndex, stringGetter, &micItems,
                     static_cast<int>(micItems.size()))) {
        const int chosen = micIndex - 1;
        selectedMicrophone_ = chosen;
        microphoneName_ = chosen >= 0 ? wideToUtf8Local(captureDevices_[static_cast<size_t>(chosen)].name)
                                      : std::string();
        saveUiConfig();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Usa um microfone/entrada do Windows como entrada fisica,\n"
                             "sem precisar de driver ASIO. O som aparece no combo 'Entrada'\n"
                             "das faixas como 'MIC: <dispositivo> L / R'.",
                             "Uses a Windows microphone/input as a physical source,\n"
                             "with no ASIO driver needed. It appears in each channel's\n"
                             "'Input' combo as 'MIC: <device> L / R'."));
    }

    fitSameLine(160.0f);
    ImGui::Checkbox(tr("Nao ouvir no PC", "Mute PC output"), &muteLoopback_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("O som do PC entra no mixer mas nao sai nas caixas.\n"
                             "O volume do Windows e restaurado ao parar.",
                             "PC audio enters the mixer but does not play on the speakers.\n"
                             "The Windows volume is restored when you stop."));
    }

    fitSameLine(120.0f);
    if (ImGui::Button(tr("Atualizar disp.", "Refresh devices"))) {
        refreshDrivers();
        refreshWasapiDevices();
    }
    ImGui::EndDisabled();

    ImGui::NewLine();
    if (ImGui::Checkbox(tr("Desviar apps do Windows", "Divert Windows apps"), &divertApps_)) {
        std::string error;
        appCapture_.setAppsMuted(divertApps_, &error);
        if (!error.empty()) status_ = error;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("O som dos apps capturados deixa de sair nas caixas e vai\n"
                             "somente para o mixer (Master Out). O nivel capturado continua\n"
                             "cheio. O mudo original do Windows e restaurado ao parar.",
                             "Captured apps stop playing on the speakers and go\n"
                             "only to the mixer (Master Out). The captured level stays\n"
                             "full. The original Windows mute is restored when you stop."));
    }

    fitSameLine(120.0f);
    if (ImGui::Button(tr("Atualizar apps", "Refresh apps"))) refreshAppCapture();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Procura aplicativos que estao gerando audio e os adiciona\n"
                             "ao combo 'Entrada' das faixas. OBS/streaming e este app\n"
                             "sao ignorados automaticamente.",
                             "Finds applications currently producing audio and adds them\n"
                             "to the 'Input' combo of the channels. OBS/streaming and this app\n"
                             "are ignored automatically."));
    }
    if (appCapture_.count() > 0) {
        fitSameLine(75.0f);
        char appsLabel[32];
        std::snprintf(appsLabel, sizeof(appsLabel), "APPS %d", appCapture_.count());
        coloredText(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), appsLabel);
    }
    if (divertApps_ && appCapture_.appsMuted()) {
        fitSameLine(70.0f);
        coloredText(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "DIV ON");
    }

    if (wasapi_.isLoopbackRunning()) {
        fitSameLine(70.0f);
        coloredText(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), "PC ON");
    }
    if (wasapi_.isMicrophoneRunning()) {
        fitSameLine(70.0f);
        coloredText(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), "MIC ON");
    }
    if (wasapi_.isOutputRunning()) {
        fitSameLine(75.0f);
        coloredText(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), "OUT ON");
    }
}

void App::drawFxRack()
{
    // Os modulos permanecem na coluna entre os canais e o Master, com o mesmo
    // tamanho. Como ha mais altura do que largura util, os 3 parametros ficam
    // empilhados na vertical e o controle de volume (Retorno) vai para o meio
    // do bloco. Assim o rack usa a mesma largura de um canal de audio.
    // Mesmo raio dos knobs de equalizacao e mesma margem de 8px em relacao as
    // bordas da janela do modulo (WindowPadding), mantendo a disposicao atual.
    const float knobRadius = 15.0f * controlScale_;
    const float knobSize = knobRadius * 2.0f;
    const float spacing = 6.0f;
    const float moduleHeight = std::max(
        80.0f, (ImGui::GetContentRegionAvail().y - spacing * static_cast<float>(kNumAux - 1)) /
                   static_cast<float>(kNumAux));

    for (int a = 0; a < kNumAux; ++a) {
        FxProcessor& fx = mixer_.aux(a);
        ImGui::PushID(a);
        ImGui::BeginChild(("##fx" + std::to_string(a)).c_str(),
                          ImVec2(0.0f, moduleHeight), true, ImGuiWindowFlags_NoScrollbar);

        const float blockLeft = ImGui::GetCursorPosX();
        const float blockWidth = ImGui::GetContentRegionAvail().x;
        const float blockTop = ImGui::GetCursorPosY();

        // Titulo centralizado no topo do bloco, fora da area dos knobs.
        const char* const title = fxName(a);
        const float titleWidth = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX(blockLeft + std::max(0.0f, (blockWidth - titleWidth) * 0.5f));
        coloredText(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), title);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(tr("A saida deste efeito vai direto para o Master.",
                                 "This effect's output goes straight to the Master."));
        const float titleBottom = ImGui::GetCursorPosY();

        // Medidor na borda direita, acompanhando toda a altura do bloco.
        constexpr float kMeterWidth = 10.0f;
        ImGui::SetCursorPosX(blockLeft + blockWidth - kMeterWidth - 4.0f);
        ImGui::SetCursorPosY(blockTop + 2.0f);
        widgets::VerticalMeter(mixer_.auxMeter(a).left(), mixer_.auxMeter(a).holdLeft(),
                               kMeterWidth, std::max(20.0f, moduleHeight - 12.0f));

        // Volume do efeito exatamente no centro do bloco.
        const float centerX = blockLeft + (blockWidth - knobSize) * 0.5f;
        const float centerY = blockTop + (moduleHeight - knobSize) * 0.5f;
        ImGui::SetCursorPosX(centerX);
        ImGui::SetCursorPosY(centerY);
        const MidiTarget returnTarget{MidiTargetType::AuxReturn, a, 0};
        widgets::KnobParam(tr("Retorno", "Return"), mixer_.auxReturnDb(a), -60.0f, 12.0f, "%.0f dB",
                           knobRadius, -12.0f, learnCallback(returnTarget, -60.0f, 12.0f), kKnobFx);
        markMidi(returnTarget);

        // Tres parametros na vertical: mesma margem dos knobs de EQ (8px da
        // borda esquerda), comecando abaixo do titulo e terminando 8px acima
        // da borda inferior.
        constexpr float kBorderMargin = 8.0f;
        const float paramX = blockLeft;
        const float paramTop = std::max(blockTop + kBorderMargin, titleBottom + 2.0f);
        const float paramStep = std::max(
            0.0f, (blockTop + moduleHeight - kBorderMargin - knobSize - paramTop) / 2.0f);
        auto placeParam = [&](int row) {
            ImGui::SetCursorPosX(paramX);
            ImGui::SetCursorPosY(paramTop + paramStep * static_cast<float>(row));
        };
        placeParam(0);

        // Cada efeito ocupa um bloco de 3 parametros no espaco de MIDI global.
        const int base = a * 3;
        const MidiTarget t0{MidiTargetType::FxParam, a, base};
        const MidiTarget t1{MidiTargetType::FxParam, a, base + 1};
        const MidiTarget t2{MidiTargetType::FxParam, a, base + 2};

        switch (a) {
        case 0: // Delay
            widgets::KnobParam(tr("Tempo", "Time"), fx.delay.timeMs, 20.0f, 1200.0f, "%.0f ms",
                               knobRadius, 250.0f, learnCallback(t0, 20.0f, 1200.0f), kKnobFx);
            markMidi(t0);
            placeParam(1);
            widgets::KnobParam("Feedback", fx.delay.feedback, 0.0f, 0.9f, "%.2f",
                               knobRadius, 0.35f, learnCallback(t1, 0.0f, 0.9f), kKnobFx);
            markMidi(t1);
            placeParam(2);
            widgets::KnobParam("Mix", fx.delay.mix, 0.0f, 1.0f, "%.2f",
                               knobRadius, 0.5f, learnCallback(t2, 0.0f, 1.0f), kKnobFx);
            markMidi(t2);
            break;
        case 1: // Reverb
            widgets::KnobParam(tr("Tamanho", "Size"), fx.reverb.size, 0.0f, 1.0f, "%.2f",
                               knobRadius, 0.65f, learnCallback(t0, 0.0f, 1.0f), kKnobFx);
            markMidi(t0);
            placeParam(1);
            widgets::KnobParam("Damp", fx.reverb.damp, 0.0f, 1.0f, "%.2f",
                               knobRadius, 0.35f, learnCallback(t1, 0.0f, 1.0f), kKnobFx);
            markMidi(t1);
            placeParam(2);
            widgets::KnobParam("Mix", fx.reverb.mix, 0.0f, 1.0f, "%.2f",
                               knobRadius, 0.5f, learnCallback(t2, 0.0f, 1.0f), kKnobFx);
            markMidi(t2);
            break;
        case 2: // Chorus
        default:
            widgets::KnobParam(tr("Taxa", "Rate"), fx.chorus.rate, 0.05f, 8.0f, "%.2f Hz",
                               knobRadius, 0.6f, learnCallback(t0, 0.05f, 8.0f), kKnobFx);
            markMidi(t0);
            placeParam(1);
            widgets::KnobParam(tr("Profund.", "Depth"), fx.chorus.depth, 0.0f, 12.0f, "%.1f ms",
                               knobRadius, 3.5f, learnCallback(t1, 0.0f, 12.0f), kKnobFx);
            markMidi(t1);
            placeParam(2);
            widgets::KnobParam("Mix", fx.chorus.mix, 0.0f, 1.0f, "%.2f",
                               knobRadius, 0.5f, learnCallback(t2, 0.0f, 1.0f), kKnobFx);
            markMidi(t2);
            break;
        }

        ImGui::EndChild();
        ImGui::PopID();
        if (a < kNumAux - 1) ImGui::Spacing();
    }
}

void App::drawChannel(ChannelStrip& strip, int index)
{
    const float s = controlScale_;
    // Em telas muito altas a escala tem teto; o excedente e distribuido entre
    // as secoes para nao sobrar um bloco vazio acima dos faders.
    const float naturalUpper = 236.0f + 175.0f * s;
    const float extraGap = std::max(0.0f, faderTopY_ - naturalUpper) / 4.0f;
    // SameLine() nao volta ao topo da linha quando o item anterior e mais alto
    // (medidor/fader). Sem isso os controles ficam escada abaixo uns dos outros.
    auto sameLineAt = [](float y) {
        ImGui::SameLine();
        ImGui::SetCursorPosY(y);
    };
    ImGui::PushID(index);

    char nameBuffer[32];
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", strip.name.c_str());
    if (ImGui::InputText("##nome", nameBuffer, sizeof(nameBuffer)))
        strip.name = nameBuffer;

    const std::vector<std::string> names = inputNames();
    const int availableInputs = std::max(1, static_cast<int>(names.size()));
    int source = static_cast<int>(strip.inputChannel.get());
    source = dsp::clamp(static_cast<float>(source), 0.0f, static_cast<float>(availableInputs - 1));
    auto inputGetter = [](void* data, int item) -> const char* {
        auto* list = static_cast<std::vector<std::string>*>(data);
        return list->at(static_cast<size_t>(item)).c_str();
    };
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo(tr("Entrada", "Input"), &source, inputGetter,
                     const_cast<std::vector<std::string>*>(&names), availableInputs)) {
        strip.inputChannel.set(static_cast<float>(source));
        strip.inputName = source >= 0 && source < availableInputs
                          ? names[static_cast<size_t>(source)] : std::string();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("IN = entradas fisicas ASIO | MIC: = microfone WASAPI\n"
                             "AUDIO PC = som do Windows | <app> L/R = captura direta do app.",
                             "IN = physical ASIO inputs | MIC: = WASAPI microphone\n"
                             "PC AUDIO = Windows audio | <app> L/R = direct app capture."));
    }

    ImGui::Dummy(ImVec2(0.0f, extraGap));
    ImGui::Separator();

    const MidiTarget gainTarget{MidiTargetType::ChannelGain, index, 0};
    const MidiTarget panTarget{MidiTargetType::ChannelPan, index, 0};
    const MidiTarget hpTarget{MidiTargetType::ChannelHighpass, index, 0};
    const MidiTarget phaseTarget{MidiTargetType::ChannelPhase, index, 0};

    const float row1Y = ImGui::GetCursorPosY();
    widgets::VerticalMeter(strip.inputMeter.left(), strip.inputMeter.holdLeft(), 12.0f * s, 76.0f * s);
    sameLineAt(row1Y);
    widgets::KnobParam(tr("Ganho", "Gain"), strip.gainDb, -60.0f, 18.0f, "%.1f dB", 15.0f * s, 0.0f,
                       learnCallback(gainTarget, -60.0f, 18.0f), kKnobGain);
    markMidi(gainTarget);
    sameLineAt(row1Y);
    widgets::KnobParam("Pan", strip.pan, -1.0f, 1.0f, "%.2f", 15.0f * s, 0.0f,
                       learnCallback(panTarget, -1.0f, 1.0f), kKnobNeutral);
    markMidi(panTarget);
    sameLineAt(row1Y);
    ImGui::BeginGroup();
    if (widgets::ToggleButton("HPF", strip.highpass.get() > 0.5f,
                              ImVec4(0.20f, 0.50f, 0.75f, 1.0f), 42.0f * s,
                              learnCallback(hpTarget, 0.0f, 1.0f)))
        strip.highpass.set(strip.highpass.get() > 0.5f ? 0.0f : 1.0f);
    markMidi(hpTarget);
    if (widgets::ToggleButton("Ø", strip.phase.get() > 0.5f,
                              ImVec4(0.60f, 0.40f, 0.75f, 1.0f), 42.0f * s,
                              learnCallback(phaseTarget, 0.0f, 1.0f)))
        strip.phase.set(strip.phase.get() > 0.5f ? 0.0f : 1.0f);
    markMidi(phaseTarget);
    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0.0f, extraGap));
    ImGui::Separator();
    ImGui::TextUnformatted(tr("Equalizacao", "EQ"));
    struct Band { const char* label; dsp::Param* gain; dsp::Param* freq; float minFreq; float maxFreq; };
    Band bands[4] = {
        { "Low",    &strip.eqLowDb,    &strip.eqLowFreq,    30.0f,  400.0f },
        { "Lo-Mid", &strip.eqLowMidDb, &strip.eqLowMidFreq, 100.0f, 2500.0f },
        { "Hi-Mid", &strip.eqHighMidDb,&strip.eqHighMidFreq,500.0f, 8000.0f },
        { "High",   &strip.eqHighDb,   &strip.eqHighFreq,   2000.0f,18000.0f }
    };
    for (int b = 0; b < 4; ++b) {
        const float eqY = ImGui::GetCursorPosY();
        const MidiTarget eqTarget{MidiTargetType::ChannelEqGain, index, b};
        const ImVec4 eqColor = (b == 1 || b == 2) ? kKnobEqMid : kKnobEqLowHigh;
        widgets::KnobParam(bands[b].label, *bands[b].gain, -18.0f, 18.0f, "%.1f",
                           15.0f * s, 0.0f, learnCallback(eqTarget, -18.0f, 18.0f), eqColor);
        markMidi(eqTarget);
        sameLineAt(eqY);
        float frequency = bands[b].freq->get();
        ImGui::SetNextItemWidth(74.0f * s);
        if (ImGui::SliderFloat(bands[b].label, &frequency, bands[b].minFreq, bands[b].maxFreq, "%.0f Hz"))
            bands[b].freq->set(frequency);
    }

    ImGui::Dummy(ImVec2(0.0f, extraGap));
    ImGui::Separator();
    ImGui::TextUnformatted(tr("Envios p/ efeitos", "FX sends"));
    struct SendFx
    {
        const char* label;
        dsp::Param* value;
        MidiTargetType target;
    };
    const SendFx sends[3] = {
        { fxName(0), &strip.send1Db, MidiTargetType::ChannelSend1 },
        { fxName(1), &strip.send2Db, MidiTargetType::ChannelSend2 },
        { fxName(2), &strip.send3Db, MidiTargetType::ChannelSend3 }
    };
    const float sendsY = ImGui::GetCursorPosY();
    for (int f = 0; f < 3; ++f) {
        if (f > 0) sameLineAt(sendsY);
        ImGui::BeginGroup();
        coloredText(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), sends[f].label);
        const MidiTarget target{sends[f].target, index, 0};
        widgets::KnobParam(sends[f].label, *sends[f].value, -60.0f, 12.0f, "%.0f dB",
                           13.0f * s, -60.0f, learnCallback(target, -60.0f, 12.0f), kKnobSend);
        markMidi(target);
        ImGui::EndGroup();
    }

    // Fader fixo na base: todos os canais e o Master usam o mesmo Y.
    ImGui::Dummy(ImVec2(0.0f, extraGap));
    ImGui::SetCursorPosY(std::max(0.0f, faderTopY_ - 4.0f));
    ImGui::Separator();
    ImGui::SetCursorPosY(faderTopY_);

    const MidiTarget faderTarget{MidiTargetType::ChannelFader, index, 0};
    const MidiTarget muteTarget{MidiTargetType::ChannelMute, index, 0};
    const MidiTarget soloTarget{MidiTargetType::ChannelSolo, index, 0};

    const float faderY = ImGui::GetCursorPosY();
    widgets::VerticalMeter(strip.outputMeter.left(), strip.outputMeter.holdLeft(), 14.0f * s, 150.0f * s);
    sameLineAt(faderY);
    widgets::FaderParam(tr("Fader", "Fader"), strip.faderDb, -60.0f, 12.0f, 150.0f * s,
                        learnCallback(faderTarget, -60.0f, 12.0f));
    markMidi(faderTarget);

    sameLineAt(faderY);
    if (widgets::ToggleButton("M", strip.mute.get() > 0.5f,
                              ImVec4(0.80f, 0.25f, 0.20f, 1.0f), 30.0f * s,
                              learnCallback(muteTarget, 0.0f, 1.0f)))
        strip.mute.set(strip.mute.get() > 0.5f ? 0.0f : 1.0f);
    markMidi(muteTarget);
    sameLineAt(faderY);
    if (widgets::ToggleButton("S", strip.solo.get() > 0.5f,
                              ImVec4(0.90f, 0.75f, 0.15f, 1.0f), 30.0f * s,
                              learnCallback(soloTarget, 0.0f, 1.0f)))
        strip.solo.set(strip.solo.get() > 0.5f ? 0.0f : 1.0f);
    markMidi(soloTarget);

    ImGui::PopID();
}

void App::drawMaster()
{
    const float s = controlScale_;
    ImGui::TextUnformatted("MASTER");
    ImGui::Separator();
    coloredText(kKnobDynamics, tr("Limiter ativo", "Limiter on"));

    // O Master tem poucos controles no topo. Aproveita o espaco livre acima do
    // fader sem alterar a largura: a base continua alinhada a dos canais.
    const float headerBottom = ImGui::GetCursorPosY();
    const float faderBottom = faderTopY_ + 150.0f * s;
    const float masterTop = std::min(headerBottom + 10.0f, faderBottom - 60.0f * s);
    const float masterHeight = std::max(150.0f * s, faderBottom - masterTop);

    ImGui::SetCursorPosY(std::max(0.0f, masterTop - 4.0f));
    ImGui::Separator();
    ImGui::SetCursorPosY(masterTop);

    const float masterY = ImGui::GetCursorPosY();
    auto sameLineAt = [](float y) {
        ImGui::SameLine();
        ImGui::SetCursorPosY(y);
    };
    widgets::VerticalMeter(mixer_.masterMeter().left(), mixer_.masterMeter().holdLeft(), 16.0f * s, masterHeight);
    sameLineAt(masterY);
    widgets::VerticalMeter(mixer_.masterMeter().right(), mixer_.masterMeter().holdRight(), 16.0f * s, masterHeight);
    sameLineAt(masterY);
    const MidiTarget masterTarget{MidiTargetType::MasterFader, 0, 0};
    widgets::FaderParam(tr("Master", "Master"), mixer_.masterFaderDb(), -60.0f, 12.0f, masterHeight,
                        learnCallback(masterTarget, -60.0f, 12.0f));
    markMidi(masterTarget);
}

void App::drawStatusBar()
{
    ImGui::Separator();
    if (running_) {
        char info[256];
        if (wasapiClock_) {
            std::snprintf(info, sizeof(info), tr("WASAPI relogio mestre | %.1f kHz",
                                                 "WASAPI master clock | %.1f kHz"),
                          sampleRate_ / 1000.0);
        } else {
            std::snprintf(info, sizeof(info), tr("DSP %.1f%% | latencia E/S %ld/%ld samples",
                                                 "DSP %.1f%% | I/O latency %ld/%ld samples"),
                          device_.dspLoad(), device_.info().inputLatency, device_.info().outputLatency);
        }
        coloredText(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), info);
        ImGui::SameLine();
    } else {
        coloredText(ImVec4(0.90f, 0.65f, 0.25f, 1.0f), tr("MOTOR PARADO", "ENGINE STOPPED"));
        ImGui::SameLine();
    }
    ImGui::TextUnformatted(status_.c_str());
}

widgets::RightClickCallback App::learnCallback(const MidiTarget& target,
                                               float minimum, float maximum)
{
    return [this, target, minimum, maximum]() {
        midi_.beginLearn(target, minimum, maximum);
        status_ = tr("MIDI: mova o controle desejado para associar ao parametro.",
                     "MIDI: move the desired control to assign it to the parameter.");
    };
}

void App::markMidi(const MidiTarget& target)
{
    const bool learningThis = midi_.isLearning() && midi_.learningTarget() == target;
    const bool bound = midi_.isBound(target);
    if (!learningThis && !bound) return;

    // Desenha um ponto sobre o proprio controle, sem alterar o layout.
    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    const ImVec2 point(maximum.x - 3.0f, minimum.y + 3.0f);
    const ImU32 color = learningThis ? IM_COL32(255, 210, 60, 255) : IM_COL32(70, 215, 240, 255);
    ImGui::GetWindowDrawList()->AddCircleFilled(point, 3.0f, color, 8);
}

void App::drawMidiPanel()
{
    if (!showMidi_) return;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(std::min(500.0f, display.x - 16.0f),
                                    std::min(430.0f, display.y - 16.0f)),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 200.0f),
                                        ImVec2(display.x - 16.0f, display.y - 16.0f));
    if (!ImGui::Begin(tr("MIDI - controladores", "MIDI - controllers"), &showMidi_)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button(tr("Atualizar lista", "Refresh list"))) midi_.refresh();
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancelar learn", "Cancel learn"))) midi_.cancelLearn();
    ImGui::SameLine();
    if (ImGui::Button(tr("Limpar mapa", "Clear map"))) midi_.clearAllBindings();

    ImGui::TextDisabled(tr("Clique com o botao direito sobre um controle da mesa para associar.",
                           "Right-click a mixer control to assign it."));
    if (midi_.isLearning())
        coloredText(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
                    tr("Aprendendo: mova um controle giratorio ou pressione um botao do MIDI.",
                       "Learning: move a knob or press a MIDI button."));
    else
        coloredText(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), tr("Pronto para associar.", "Ready to assign."));

    ImGui::Separator();
    ImGui::TextUnformatted(tr("Controladores detectados:", "Detected controllers:"));
    if (midi_.devices().empty())
        ImGui::TextDisabled(tr("Nenhum controlador MIDI conectado.", "No MIDI controller connected."));
    for (const auto& device : midi_.devices()) {
        ImGui::PushID(device.index);
        bool enabled = device.enabled;
        if (ImGui::Checkbox("##ativo", &enabled)) midi_.setEnabled(device.index, enabled);
        ImGui::SameLine();
        const std::string name = wideToUtf8Local(device.name);
        if (device.opened)
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.50f, 1.0f), "%s %s", name.c_str(),
                               tr("(ativo)", "(active)"));
        else if (!device.enabled)
            ImGui::TextDisabled("%s %s", name.c_str(), tr("(desabilitado)", "(disabled)"));
        else
            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s %s", name.c_str(),
                               tr("(erro)", "(error)"));
        if (!device.error.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", device.error.c_str());
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text("%s (%d):", tr("Associacoes", "Bindings"), static_cast<int>(midi_.bindings().size()));
    ImGui::BeginChild("##midi-bindings", ImVec2(0.0f, 180.0f), true);
    const auto& bindings = midi_.bindings();
    for (size_t i = 0; i < bindings.size(); ++i) {
        const MidiBinding& binding = bindings[i];
        ImGui::PushID(static_cast<int>(i));
        char line[256];
        const char* kind = binding.kind == 0 ? "CC" : (binding.kind == 1 ? "NOTE" : "PITCH");
        std::snprintf(line, sizeof(line), "%s | ch %d %s %d -> %s %d.%d%s",
                      wideToUtf8Local(binding.deviceName).c_str(), binding.channel, kind,
                      binding.data, midiTargetTypeName(binding.target.type),
                      binding.target.index, binding.target.sub,
                      binding.toggle ? " (latch)" : "");
        ImGui::TextUnformatted(line);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) midi_.clearBinding(binding.target);
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

void App::drawConflictDialog()
{
    if (!midi_.hasPendingBinding()) return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(540.0f, display.x - 16.0f),
                                    std::min(300.0f, display.y - 16.0f)),
                             ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f),
                                        ImVec2(display.x - 16.0f, display.y - 16.0f));
    if (!ImGui::Begin(tr("Conflito de controle MIDI", "MIDI control conflict"), nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const MidiBinding& pending = midi_.pendingBinding();
    ImGui::TextWrapped(tr("O controle %s ja esta sendo usado em:",
                          "The control %s is already used for:"),
                       describeMidiEvent(pending).c_str());
    ImGui::Separator();
    ImGui::BeginChild("##conflitos", ImVec2(0.0f, 96.0f), true);
    for (const MidiBinding& conflict : midi_.pendingConflicts()) {
        ImGui::BulletText("%s", describeMidiTarget(conflict.target).c_str());
    }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::Text("%s: %s", tr("Novo comando", "New command"),
                describeMidiTarget(pending.target).c_str());
    ImGui::TextDisabled(tr("Escolha como resolver:", "Choose how to resolve:"));
    ImGui::Spacing();

    if (ImGui::Button(tr("Substituir", "Replace"), ImVec2(160.0f, 0.0f))) {
        midi_.resolvePending(MidiConflictChoice::Replace);
        status_ = tr("Associacao MIDI substituida.", "MIDI binding replaced.");
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Usar em conjunto", "Use together"), ImVec2(160.0f, 0.0f))) {
        midi_.resolvePending(MidiConflictChoice::Together);
        status_ = tr("Controle MIDI agora aciona varios comandos.",
                     "MIDI control now triggers several commands.");
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancelar", "Cancel"), ImVec2(150.0f, 0.0f))) {
        midi_.resolvePending(MidiConflictChoice::Cancel);
        status_ = tr("Associacao MIDI cancelada.", "MIDI binding cancelled.");
    }
    ImGui::End();
}

void App::refreshScenes()
{
    scenes_.clear();
    std::error_code ec;
    if (!std::filesystem::exists(scenesDir_, ec)) return;
    for (const auto& entry : std::filesystem::directory_iterator(scenesDir_, ec)) {
        if (ec) break;
        if (!entry.is_directory(ec)) continue;
        Scene scene;
        scene.name = entry.path().filename().string();
        scene.path = entry.path().string();
        scenes_.push_back(std::move(scene));
    }
    std::sort(scenes_.begin(), scenes_.end(), [](const Scene& a, const Scene& b) {
        return a.name < b.name;
    });
}

bool App::saveScene(const std::string& requestedName)
{
    const std::string name = sanitizeSceneName(requestedName);
    if (name.empty()) {
        status_ = tr("Informe um nome para a cena.", "Enter a name for the scene.");
        return false;
    }

    std::error_code ec;
    const std::string path = scenesDir_ + "\\" + name;
    std::filesystem::create_directories(path, ec);
    const bool mix = mixer_.save(path + "\\mixer.cfg");
    const bool map = midi_.save(path + "\\midi.map");
    if (!mix || !map) {
        status_ = tr("Falha ao salvar a cena.", "Failed to save the scene.");
        return false;
    }

    currentScene_ = name;
    refreshScenes();
    status_ = tr("Cena '", "Scene '") + name + tr("' salva.", "' saved.");
    return true;
}

bool App::loadScene(size_t index)
{
    if (index >= scenes_.size()) return false;
    const Scene& scene = scenes_[index];
    const bool mix = mixer_.load(scene.path + "\\mixer.cfg");
    const bool map = midi_.load(scene.path + "\\midi.map");
    if (!mix || !map) {
        status_ = tr("Falha ao carregar a cena.", "Failed to load the scene.");
        return false;
    }
    currentScene_ = scene.name;
    status_ = tr("Cena '", "Scene '") + scene.name + tr("' carregada.", "' loaded.");
    return true;
}

void App::deleteScene(size_t index)
{
    if (index >= scenes_.size()) return;
    const std::string name = scenes_[index].name;
    std::error_code ec;
    std::filesystem::remove_all(scenes_[index].path, ec);
    if (ec) {
        status_ = tr("Falha ao excluir a cena.", "Failed to delete the scene.");
        return;
    }
    if (currentScene_ == name) currentScene_.clear();
    refreshScenes();
    status_ = tr("Cena '", "Scene '") + name + tr("' excluida.", "' deleted.");
}

void App::drawScenesPanel()
{
    if (!showScenes_) return;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(std::min(480.0f, display.x - 16.0f),
                                    std::min(420.0f, display.y - 16.0f)),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 200.0f),
                                        ImVec2(display.x - 16.0f, display.y - 16.0f));
    if (!ImGui::Begin(tr("Cenas", "Scenes"), &showScenes_)) {
        ImGui::End();
        return;
    }

    ImGui::Text("%s: %s", tr("Cena atual", "Current scene"),
                currentScene_.empty() ? tr("(nenhuma)", "(none)") : currentScene_.c_str());
    if (!currentScene_.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Salvar na atual", "Save to current"))) saveScene(currentScene_);
    }

    ImGui::Separator();
    ImGui::TextUnformatted(tr("Nova cena:", "New scene:"));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##nome-cena", sceneNameBuffer_, sizeof(sceneNameBuffer_));
    if (ImGui::Button(tr("Salvar como nova cena", "Save as new scene"), ImVec2(200.0f, 0.0f))) {
        if (saveScene(sceneNameBuffer_)) sceneNameBuffer_[0] = '\0';
    }

    ImGui::Separator();
    ImGui::Text("%s (%d):", tr("Cenas salvas", "Saved scenes"), static_cast<int>(scenes_.size()));
    ImGui::BeginChild("##cenas", ImVec2(0.0f, 220.0f), true);
    for (size_t i = 0; i < scenes_.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const bool isCurrent = scenes_[i].name == currentScene_;
        if (isCurrent) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.85f, 0.50f, 1.0f));
            ImGui::TextUnformatted("*");
            ImGui::PopStyleColor();
        } else {
            ImGui::TextUnformatted(" ");
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(scenes_[i].name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Carregar", "Load"))) loadScene(i);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Excluir", "Delete"))) deleteScene(i);
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

void App::shutdown()
{
    stopEngine();
}
