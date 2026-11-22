#pragma once

#include "../dsp/Biquad.h"
#include "../dsp/DspUtils.h"
#include "../dsp/Fx.h"
#include "../dsp/Meter.h"

#include <memory>
#include <string>
#include <vector>

constexpr int kMaxChannels = 16;
// Efeitos fixos: 0=Delay, 1=Reverb, 2=Chorus. Cada um tem retorno proprio ao Master.
constexpr int kNumAux = 3;

struct ChannelStrip
{
    ChannelStrip();

    std::string name;

    // Nome da fonte selecionada no combo "Entrada". E salvo junto com a cena
    // porque a ordem dos aplicativos capturados puede mudar entre execucoes.
    std::string inputName;

    dsp::Param inputChannel{0.0f};
    dsp::Param gainDb{0.0f};
    dsp::Param faderDb{0.0f};
    dsp::Param pan{0.0f};
    dsp::Param highpass{0.0f};
    dsp::Param phase{0.0f};
    dsp::Param mute{0.0f};
    dsp::Param solo{0.0f};

    dsp::Param eqLowDb{0.0f};
    dsp::Param eqLowFreq{120.0f};
    dsp::Param eqLowMidDb{0.0f};
    dsp::Param eqLowMidFreq{450.0f};
    dsp::Param eqHighMidDb{0.0f};
    dsp::Param eqHighMidFreq{2500.0f};
    dsp::Param eqHighDb{0.0f};
    dsp::Param eqHighFreq{8000.0f};

    // Envios por efeito: 0=Delay, 1=Reverb, 2=Chorus. Sempre POST-fader.
    dsp::Param send1Db{-60.0f};
    dsp::Param send2Db{-60.0f};
    dsp::Param send3Db{-60.0f};

    PeakMeter inputMeter;
    PeakMeter outputMeter;

    // Estado exclusivo da thread de audio
    dsp::Biquad hp;
    dsp::Biquad low;
    dsp::Biquad lowMid;
    dsp::Biquad highMid;
    dsp::Biquad high;
    dsp::Smoother gainSmooth;
    dsp::Smoother faderSmooth;
    dsp::Smoother panSmooth;
    dsp::Smoother send1Smooth;
    dsp::Smoother send2Smooth;
    dsp::Smoother send3Smooth;

    float cachedLowDb = 999.0f, cachedLowFreq = -1.0f;
    float cachedLowMidDb = 999.0f, cachedLowMidFreq = -1.0f;
    float cachedHighMidDb = 999.0f, cachedHighMidFreq = -1.0f;
    float cachedHighDb = 999.0f, cachedHighFreq = -1.0f;
    float cachedHp = -1.0f;

    void prepare(double sampleRate);
    void updateFilters(double sampleRate);
};

class Mixer
{
public:
    Mixer();

    int channelCount() const { return channelCount_; }
    void setChannelCount(int count);

    ChannelStrip& channel(int index) { return *channels_[static_cast<size_t>(index)]; }
    const ChannelStrip& channel(int index) const { return *channels_[static_cast<size_t>(index)]; }

    FxProcessor& aux(int index) { return aux_[index]; }
    const FxProcessor& aux(int index) const { return aux_[index]; }
    dsp::Param& auxReturnDb(int index) { return auxReturnDb_[index]; }

    dsp::Param& masterFaderDb() { return masterFaderDb_; }
    PeakMeter& masterMeter() { return masterMeter_; }
    PeakMeter& auxMeter(int index) { return auxMeter_[index]; }

    void prepare(double sampleRate, int maxFrames);
    void resetMeters();

    // Executado na thread de audio do ASIO. Nao aloca memoria.
    void process(float* const* inputs, float** outputs, int inputCount, int outputCount, int frames);

    bool save(const std::string& path) const;
    bool load(const std::string& path);

    // Reatribui "inputChannel" a partir de "inputName" quando a lista de
    // entradas muda (principalmente apps que entram/saem da captura).
    void resolveInputNames(const std::vector<std::string>& names);

private:
    std::vector<std::unique_ptr<ChannelStrip>> channels_;
    int channelCount_ = 8;
    double sampleRate_ = 48000.0;
    int maxFrames_ = 512;

    FxProcessor aux_[kNumAux];
    dsp::Param auxReturnDb_[kNumAux]{{-12.0f}, {-12.0f}, {-12.0f}};

    dsp::Param masterFaderDb_{0.0f};
    PeakMeter masterMeter_;
    PeakMeter auxMeter_[kNumAux];
    dsp::Smoother masterSmooth_;

    std::vector<float> masterL_;
    std::vector<float> masterR_;
    std::vector<float> auxInL_[kNumAux];
    std::vector<float> auxInR_[kNumAux];
    std::vector<float> auxOutL_[kNumAux];
    std::vector<float> auxOutR_[kNumAux];
};
