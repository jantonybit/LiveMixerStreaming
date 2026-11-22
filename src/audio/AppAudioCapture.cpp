#include "AppAudioCapture.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <endpointvolume.h>

// O MinGW so forward-declara IAudioMeterInformation. Definicao minima local
// (IID + vtable) para consultar o nivel de cada sessao de audio.
struct LmAudioMeterInformation : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float* peak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32 count, float* peaks) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD* mask) = 0;
};

static const GUID kLmAudioMeterIid = {
    0xc02216f6, 0x8c67, 0x4b5b, { 0x9d, 0x00, 0xd0, 0x08, 0xe7, 0x3e, 0x00, 0x64 }
};

static const GUID kLmActivateHandlerIid = {
    0x41d949ab, 0x9862, 0x444a, { 0x80, 0xf6, 0xc2, 0x61, 0x33, 0x4d, 0xa5, 0xeb }
};

// ActivateAudioInterfaceAsync exige um handler agil (IAgileObject + FTM),
// mesmo em Win10+. Sem isso a chamada falha com E_ILLEGAL_METHOD_CALL.
static const GUID kLmAgileObjectIid = {
    0x94ea2b94, 0xe9cc, 0x49e0, { 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90 }
};
static const GUID kLmMarshalIid = {
    0x00000003, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

// O MinGW nao traz audioclientactivationparams.h. Estas estruturas sao
// binariamente identicas as do SDK do Windows e evitam depender desse header.
namespace {

enum LmActivationType
{
    LmActivationDefault = 0,
    LmActivationProcessLoopback = 1
};

enum LmLoopbackMode
{
    LmIncludeTargetProcessTree = 0,
    LmExcludeTargetProcessTree = 1
};

struct LmProcessLoopbackParams
{
    DWORD TargetProcessId;
    LmLoopbackMode ProcessLoopbackMode;
};

struct LmActivationParams
{
    LmActivationType ActivationType;
    LmProcessLoopbackParams ProcessLoopbackParams;
};

const wchar_t kVadProcessLoopback[] = L"VAD\\Process_Loopback";

std::wstring processExecutableName(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return std::wstring();

    wchar_t buffer[MAX_PATH]{};
    DWORD size = MAX_PATH;
    std::wstring result;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
        const std::wstring full(buffer, size);
        const size_t separator = full.find_last_of(L"\\/");
        result = separator == std::wstring::npos ? full : full.substr(separator + 1);
    }
    CloseHandle(process);
    return result;
}

bool isStreamingApp(const std::wstring& executable)
{
    if (executable.empty()) return false;
    static const wchar_t* blocked[] = {
        L"obs64.exe", L"obs32.exe", L"obs.exe", L"obs-studio.exe",
        L"centraldeaudio.exe", L"streamlabs obs.exe", L"streamlabsobs.exe",
        L"xsplit.core.exe", L"vmix.exe", L"wirecast.exe"
    };
    for (const wchar_t* name : blocked)
        if (_wcsicmp(executable.c_str(), name) == 0) return true;
    return false;
}

std::string utf8FromWide(const std::wstring& text)
{
    std::string result;
    result.reserve(text.size());
    for (const wchar_t c : text) {
        if (c < 0x80) {
            result += static_cast<char>(c);
        } else if (c < 0x800) {
            result += static_cast<char>(0xC0 | (c >> 6));
            result += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (c >> 12));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return result;
}

// Callback de ativacao assincrona. Vive na thread MTA que chama
// ActivateAudioInterfaceAsync; apenas sinaliza um evento e guarda o resultado.
class ActivateHandler final : public IActivateAudioInterfaceCompletionHandler
{
public:
    explicit ActivateHandler(std::string* detail) : detail_(detail)
    {
        event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        // Agregacao do free-threaded marshaler, como faz FtmBase no sample da MS.
        IUnknown* aggregator = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
        CoCreateFreeThreadedMarshaler(aggregator, &marshaler_);
    }

    ~ActivateHandler()
    {
        if (marshaler_) marshaler_->Release();
        if (client_) client_->Release();
        if (unknown_) unknown_->Release();
        if (event_) CloseHandle(event_);
    }

    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == kLmActivateHandlerIid) {
            *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == kLmAgileObjectIid) {
            *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == kLmMarshalIid && marshaler_)
            return marshaler_->QueryInterface(riid, ppv);
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() override { return ++references_; }

    STDMETHOD_(ULONG, Release)() override
    {
        const ULONG remaining = --references_;
        if (remaining == 0) delete this;
        return remaining;
    }

    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) override
    {
        HRESULT callbackHr = operation->GetActivateResult(&result_, &unknown_);
        if (FAILED(callbackHr)) {
            describe("GetActivateResult", callbackHr);
            result_ = callbackHr;
        } else if (FAILED(result_)) {
            describe("resultado da ativacao", result_);
        } else if (!unknown_) {
            result_ = E_FAIL;
            describe("interface de ativacao nula", result_);
        } else {
            callbackHr = unknown_->QueryInterface(__uuidof(IAudioClient), (void**)&client_);
            if (FAILED(callbackHr)) {
                result_ = callbackHr;
                describe("QueryInterface IAudioClient", callbackHr);
            }
        }
        SetEvent(event_);
        return S_OK;
    }

    HANDLE event() const { return event_; }
    HRESULT result() const { return result_; }
    IAudioClient* client() const { return client_; }

private:
    void describe(const char* stage, HRESULT hr)
    {
        if (!detail_) return;
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), "%s: 0x%08lX", stage,
                      static_cast<unsigned long>(hr));
        *detail_ = buffer;
    }

    std::atomic<ULONG> references_{1};
    std::string* detail_ = nullptr;
    IUnknown* marshaler_ = nullptr;
    HANDLE event_ = nullptr;
    HRESULT result_ = E_UNEXPECTED;
    IUnknown* unknown_ = nullptr;
    IAudioClient* client_ = nullptr;
};

template <typename LockType>
class ScopeGuard
{
public:
    explicit ScopeGuard(LockType& lock) : lock_(lock) { lock_.lock(); }
    ~ScopeGuard() { lock_.unlock(); }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

private:
    LockType& lock_;
};

} // namespace

struct AppAudioCapture::Entry
{
    DWORD pid = 0;
    std::string name;
    std::string error;
    int requestedRate = 48000;

    // Eventos criados na thread de UI, usados pela worker e fechados no join.
    HANDLE audioEvent = nullptr;
    HANDLE startDone = nullptr;
    HANDLE finished = nullptr;
    std::atomic<bool> stop{false};
    std::atomic<HRESULT> startHr{E_FAIL};
    std::atomic<int> rate{0};

    // Os objetos COM pertencem exclusivamente a worker thread e sao liberados
    // por ela antes de sinalizar 'finished'.
    StereoRing ring{8192};
    HANDLE thread = nullptr;

    ~Entry() { release(); }

    bool release()
    {
        if (thread) {
            const DWORD wait = WaitForSingleObject(finished, 8000);
            if (wait != WAIT_OBJECT_0) return false;
            CloseHandle(thread);
            thread = nullptr;
        }
        if (audioEvent) { CloseHandle(audioEvent); audioEvent = nullptr; }
        if (startDone) { CloseHandle(startDone); startDone = nullptr; }
        if (finished) { CloseHandle(finished); finished = nullptr; }
        return true;
    }
};

std::vector<AppAudioCapture::AppInfo> AppAudioCapture::enumerateApps(DWORD excludePid)
{
    std::vector<AppInfo> result;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumerator)))
        return result;

    // Varre todos os endpoints de reproducao ativos: um app pode estar mandando
    // audio para uma saida que nao e a padrao do Windows.
    IMMDeviceCollection* devices = nullptr;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
        UINT deviceCount = 0;
        devices->GetCount(&deviceCount);
        for (UINT d = 0; d < deviceCount; ++d) {
            IMMDevice* device = nullptr;
            if (FAILED(devices->Item(d, &device))) continue;

            IAudioSessionManager2* manager = nullptr;
            if (SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                           nullptr, (void**)&manager))) {
                IAudioSessionEnumerator* sessions = nullptr;
                if (SUCCEEDED(manager->GetSessionEnumerator(&sessions))) {
                    int count = 0;
                    sessions->GetCount(&count);
                    for (int i = 0; i < count; ++i) {
                        IAudioSessionControl* control = nullptr;
                        if (FAILED(sessions->GetSession(i, &control))) continue;

                        IAudioSessionControl2* control2 = nullptr;
                        LmAudioMeterInformation* meter = nullptr;
                        if (SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2),
                                                               (void**)&control2))) {
                            DWORD pid = 0;
                            AudioSessionState state = AudioSessionStateInactive;
                            float peak = 0.0f;
                            if (SUCCEEDED(control2->GetProcessId(&pid)) &&
                                SUCCEEDED(control->GetState(&state)) && pid && pid != excludePid) {
                                const std::wstring executable = processExecutableName(pid);
                                if (!isStreamingApp(executable) && state == AudioSessionStateActive) {
                                    bool duplicate = false;
                                    for (const auto& other : result)
                                        if (other.pid == pid) { duplicate = true; break; }
                                    if (!duplicate) {
                                        if (SUCCEEDED(control->QueryInterface(
                                                kLmAudioMeterIid, (void**)&meter)))
                                            meter->GetPeakValue(&peak);

                                        AppInfo info;
                                        info.pid = pid;
                                        info.peak = peak;
                                        info.name = executable.empty()
                                                    ? (L"Aplicativo " + std::to_wstring(pid))
                                                    : executable;
                                        LPWSTR display = nullptr;
                                        if (SUCCEEDED(control->GetDisplayName(&display)) && display) {
                                            if (*display) info.name = display;
                                            CoTaskMemFree(display);
                                        }
                                        result.push_back(std::move(info));
                                    }
                                }
                            }
                        }
                        if (meter) meter->Release();
                        if (control2) control2->Release();
                        control->Release();
                    }
                    sessions->Release();
                }
                manager->Release();
            }
            device->Release();
        }
        devices->Release();
    }
    enumerator->Release();
    return result;
}

bool AppAudioCapture::setSessionMute(DWORD pid, bool mute, bool* previous)
{
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumerator)))
        return false;

    bool found = false;
    IMMDeviceCollection* devices = nullptr;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
        UINT deviceCount = 0;
        devices->GetCount(&deviceCount);
        for (UINT d = 0; d < deviceCount; ++d) {
            IMMDevice* device = nullptr;
            if (FAILED(devices->Item(d, &device))) continue;
            IAudioSessionManager2* manager = nullptr;
            if (SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                           nullptr, (void**)&manager))) {
                IAudioSessionEnumerator* sessions = nullptr;
                if (SUCCEEDED(manager->GetSessionEnumerator(&sessions))) {
                    int count = 0;
                    sessions->GetCount(&count);
                    for (int i = 0; i < count; ++i) {
                        IAudioSessionControl* control = nullptr;
                        if (FAILED(sessions->GetSession(i, &control))) continue;
                        IAudioSessionControl2* control2 = nullptr;
                        if (SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2),
                                                               (void**)&control2))) {
                            DWORD sessionPid = 0;
                            if (SUCCEEDED(control2->GetProcessId(&sessionPid)) &&
                                sessionPid == pid) {
                                ISimpleAudioVolume* volume = nullptr;
                                if (SUCCEEDED(control->QueryInterface(
                                        __uuidof(ISimpleAudioVolume), (void**)&volume))) {
                                    BOOL wasMuted = FALSE;
                                    if (SUCCEEDED(volume->GetMute(&wasMuted)) && previous)
                                        *previous = wasMuted != FALSE;
                                    if (SUCCEEDED(volume->SetMute(mute ? TRUE : FALSE, nullptr)))
                                        found = true;
                                    volume->Release();
                                }
                            }
                            control2->Release();
                        }
                        control->Release();
                    }
                    sessions->Release();
                }
                manager->Release();
            }
            device->Release();
        }
        devices->Release();
    }
    enumerator->Release();
    return found;
}

void AppAudioCapture::restoreMute(DWORD pid)
{
    MuteState state;
    {
        ScopeGuard<Lock> guard(lock_);
        auto it = std::find_if(muteStates_.begin(), muteStates_.end(),
                               [pid](const MuteState& s) { return s.pid == pid; });
        if (it == muteStates_.end()) return;
        state = *it;
        muteStates_.erase(it);
    }
    bool ignored = false;
    setSessionMute(pid, state.original, &ignored);
}

void AppAudioCapture::setAppsMuted(bool mute, std::string* error)
{
    std::vector<DWORD> pids;
    {
        ScopeGuard<Lock> guard(lock_);
        for (const auto& entry : entries_)
            if (entry) pids.push_back(entry->pid);
    }

    int applied = 0;
    for (DWORD pid : pids) {
        if (!mute) {
            restoreMute(pid);
            continue;
        }

        bool alreadyTracked = false;
        {
            ScopeGuard<Lock> guard(lock_);
            for (const auto& state : muteStates_)
                if (state.pid == pid) { alreadyTracked = true; break; }
        }

        bool original = false;
        if (!setSessionMute(pid, true, &original)) continue;
        if (!alreadyTracked) {
            ScopeGuard<Lock> guard(lock_);
            muteStates_.push_back(MuteState{pid, original});
        }
        ++applied;
    }

    appsMuted_.store(mute, std::memory_order_release);
    if (error && mute && applied == 0 && !pids.empty())
        *error = "Nao foi possivel silenciar as sessoes dos aplicativos capturados.";
}

int AppAudioCapture::count() const
{
    ScopeGuard<Lock> guard(lock_);
    return static_cast<int>(entries_.size());
}

AppAudioCapture::AppAudioCapture() = default;

AppAudioCapture::~AppAudioCapture()
{
    stopAll();
}

void AppAudioCapture::stopAll()
{
    std::vector<std::unique_ptr<Entry>> oldEntries;
    std::vector<MuteState> states;
    {
        ScopeGuard<Lock> guard(lock_);
        oldEntries.swap(entries_);
        states.swap(muteStates_);
    }
    for (auto& entry : oldEntries) {
        if (!entry) continue;
        entry->stop.store(true, std::memory_order_release);
        if (entry->audioEvent) SetEvent(entry->audioEvent);
        entry->release();
    }
    for (const auto& state : states) {
        bool ignored = false;
        setSessionMute(state.pid, state.original, &ignored);
    }
    appsMuted_.store(false, std::memory_order_release);
    // oldEntries e destruido fora do lock: a worker thread ja foi sinalizada.
}

std::string AppAudioCapture::name(int index) const
{
    ScopeGuard<Lock> guard(lock_);
    if (index < 0 || index >= static_cast<int>(entries_.size())) return std::string();
    return entries_[static_cast<size_t>(index)]->name;
}

DWORD AppAudioCapture::pid(int index) const
{
    ScopeGuard<Lock> guard(lock_);
    if (index < 0 || index >= static_cast<int>(entries_.size())) return 0;
    return entries_[static_cast<size_t>(index)]->pid;
}

bool AppAudioCapture::read(int index, float* left, float* right, int frames)
{
    ScopeGuard<Lock> guard(lock_);
    if (index < 0 || index >= static_cast<int>(entries_.size()) || frames <= 0)
        return false;
    return entries_[static_cast<size_t>(index)]->ring.read(
               left, right, static_cast<size_t>(frames)) > 0;
}

DWORD WINAPI AppAudioCapture::captureThreadMain(LPVOID param)
{
    auto* entry = static_cast<Entry*>(param);
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comOk = SUCCEEDED(comHr);
    HRESULT hr = comOk ? S_OK : comHr;

    auto fail = [&](const char* stage, HRESULT result) -> HRESULT {
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), "%s: 0x%08lX", stage,
                      static_cast<unsigned long>(result));
        entry->error = buffer;
        return result;
    };

    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;

    // Ativacao do dispositivo virtual de captura por processo.
    if (SUCCEEDED(hr)) {
        LmActivationParams params{};
        params.ActivationType = LmActivationProcessLoopback;
        params.ProcessLoopbackParams.TargetProcessId = entry->pid;
        params.ProcessLoopbackParams.ProcessLoopbackMode = LmIncludeTargetProcessTree;

        PROPVARIANT activateParams{};
        activateParams.vt = VT_BLOB;
        activateParams.blob.cbSize = sizeof(params);
        activateParams.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

        auto* handler = new ActivateHandler(&entry->error);
        IActivateAudioInterfaceAsyncOperation* operation = nullptr;
        hr = ActivateAudioInterfaceAsync(kVadProcessLoopback, __uuidof(IAudioClient),
                                         &activateParams, handler, &operation);
        if (SUCCEEDED(hr)) {
            const DWORD wait = WaitForSingleObject(handler->event(), 6000);
            hr = wait == WAIT_OBJECT_0 ? handler->result()
                                       : fail("ativacao expirou", HRESULT_FROM_WIN32(WAIT_TIMEOUT));
            if (SUCCEEDED(hr)) {
                if (!SUCCEEDED(hr) || !handler->client()) {
                    hr = fail("cliente de ativacao ausente", E_FAIL);
                } else {
                    client = handler->client();
                    client->AddRef();
                }
            }
            if (operation) operation->Release();
        } else {
            hr = fail("ActivateAudioInterfaceAsync", hr);
        }
        handler->Release();
    }

    if (SUCCEEDED(hr)) {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = static_cast<DWORD>(entry->requestedRate);
        format.wBitsPerSample = 16;
        format.nBlockAlign = 4;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_LOOPBACK |
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
                                0, 0, &format, nullptr);
        if (FAILED(hr)) hr = fail("IAudioClient::Initialize", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);
        if (FAILED(hr)) hr = fail("IAudioClient::GetService", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = client->SetEventHandle(entry->audioEvent);
        if (FAILED(hr)) hr = fail("IAudioClient::SetEventHandle", hr);
    }
    if (SUCCEEDED(hr)) {
        hr = client->Start();
        if (FAILED(hr)) hr = fail("IAudioClient::Start", hr);
    }

    entry->startHr.store(hr, std::memory_order_release);
    SetEvent(entry->startDone);

    if (SUCCEEDED(hr)) {
        entry->ring.reset();
        std::vector<float> left(16384), right(16384);
        while (!entry->stop.load(std::memory_order_acquire)) {
            const DWORD wait = WaitForSingleObject(entry->audioEvent, 250);
            if (wait == WAIT_TIMEOUT) continue;
            if (wait != WAIT_OBJECT_0) break;

            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            UINT64 position = 0, timestamp = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, &position, &timestamp)))
                continue;
            if (frames > 0) {
                const size_t count = std::min<size_t>(frames, left.size());
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::fill(left.begin(), left.begin() + count, 0.0f);
                    std::fill(right.begin(), right.begin() + count, 0.0f);
                } else {
                    // Formato pedido: PCM 16 bits estereo (AUTOCONVERTPCM garante).
                    const auto* src = reinterpret_cast<const int16_t*>(data);
                    for (size_t i = 0; i < count; ++i) {
                        left[i] = static_cast<float>(src[i * 2]) / 32768.0f;
                        right[i] = static_cast<float>(src[i * 2 + 1]) / 32768.0f;
                    }
                }
                entry->ring.write(left.data(), right.data(), count);
            }
            capture->ReleaseBuffer(frames);
        }
        client->Stop();
    }

    if (capture) capture->Release();
    if (client) client->Release();
    if (comOk) CoUninitialize();
    SetEvent(entry->finished);
    return 0;
}

void AppAudioCapture::refresh(DWORD excludePid, double sampleRate, std::string* error)
{
    const std::vector<AppInfo> apps = enumerateApps(excludePid);

    // Tira um snapshot rapido; a ativacao (lenta) acontece fora do lock para
    // nao travar a thread de audio do mixer.
    std::vector<std::unique_ptr<Entry>> existing;
    {
        ScopeGuard<Lock> guard(lock_);
        existing.swap(entries_);
    }

    std::vector<std::unique_ptr<Entry>> started;
    for (const auto& app : apps) {
        bool exists = false;
        for (const auto& entry : existing)
            if (entry && entry->pid == app.pid) { exists = true; break; }
        for (const auto& entry : started)
            if (entry && entry->pid == app.pid) { exists = true; break; }
        if (exists) continue;

        auto entry = std::make_unique<Entry>();
        entry->pid = app.pid;
        entry->name = utf8FromWide(app.name);
        entry->requestedRate = static_cast<int>(sampleRate);
        entry->audioEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        entry->startDone = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        entry->finished = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!entry->audioEvent || !entry->startDone || !entry->finished) {
            if (error) *error = "Falha ao criar os eventos da captura de aplicativo.";
            continue;
        }

        Entry* raw = entry.get();
        raw->thread = CreateThread(nullptr, 0, &AppAudioCapture::captureThreadMain,
                                   raw, 0, nullptr);
        if (!raw->thread) {
            if (error) *error = "Falha ao criar a thread da captura de aplicativo.";
            raw->release();
            continue;
        }

        const DWORD wait = WaitForSingleObject(raw->startDone, 6000);
        const HRESULT hr = raw->startHr.load(std::memory_order_acquire);
        if (wait != WAIT_OBJECT_0 || FAILED(hr)) {
            char message[320];
            std::snprintf(message, sizeof(message),
                          "Nao foi possivel capturar '%s': %s",
                          raw->name.c_str(),
                          raw->error.empty()
                              ? "falha desconhecida na ativacao do Windows."
                              : raw->error.c_str());
            if (error) *error = message;
            raw->stop.store(true, std::memory_order_release);
            SetEvent(raw->audioEvent);
            raw->release();
            continue;
        }
        started.push_back(std::move(entry));

        // Se o desvio do Windows estiver ativo, o app recem-capturado tambem
        // precisa ser silenciado na sua sessao do Windows.
        if (appsMuted_.load(std::memory_order_acquire)) {
            bool original = false;
            if (setSessionMute(app.pid, true, &original)) {
                ScopeGuard<Lock> guard(lock_);
                muteStates_.push_back(MuteState{app.pid, original});
            }
        }
    }

    // Troca atomica da lista visivel para a thread de audio.
    std::vector<std::unique_ptr<Entry>> retired;
    {
        ScopeGuard<Lock> guard(lock_);
        entries_.reserve(existing.size() + started.size());
        for (auto& entry : existing) {
            if (!entry) continue;
            bool active = false;
            for (const auto& app : apps)
                if (app.pid == entry->pid) { active = true; break; }
            if (active) entries_.push_back(std::move(entry));
            else retired.push_back(std::move(entry));
        }
        for (auto& entry : started) entries_.push_back(std::move(entry));
    }

    for (auto& entry : retired) {
        if (!entry) continue;
        restoreMute(entry->pid);
        entry->stop.store(true, std::memory_order_release);
        if (entry->audioEvent) SetEvent(entry->audioEvent);
        entry->release();
    }
}
