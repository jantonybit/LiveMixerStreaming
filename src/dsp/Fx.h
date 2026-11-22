#pragma once

#include "DspUtils.h"
#include <vector>

// Efeitos de envio (auxiliares). Todos os parametros sao atomicos para poderem
// ser alterados pela interface sem sincronizacao com a thread de audio.
class DelayFx
{
public:
    void prepare(double sampleRate);
    void process(const float* inL, const float* inR, float* outL, float* outR, int frames);

    dsp::Param timeMs{250.0f};
    dsp::Param feedback{0.35f};
    dsp::Param mix{0.5f};

private:
    double sampleRate_ = 48000.0;
    std::vector<float> bufferL_;
    std::vector<float> bufferR_;
    int write_ = 0;
};

class CombFilter
{
public:
    void prepare(double sampleRate, int baseDelay, bool rightChannel);
    float process(float x, float feedback, float damp);
    void reset() { std::fill(buffer_.begin(), buffer_.end(), 0.0f); index_ = 0; filter_ = 0.0f; }

private:
    std::vector<float> buffer_;
    size_t index_ = 0;
    float filter_ = 0.0f;
};

class AllPassFilter
{
public:
    void prepare(double sampleRate, int baseDelay, bool rightChannel);
    float process(float x);
    void reset() { std::fill(buffer_.begin(), buffer_.end(), 0.0f); index_ = 0; }

private:
    std::vector<float> buffer_;
    size_t index_ = 0;
};

class ReverbFx
{
public:
    void prepare(double sampleRate);
    void process(const float* inL, const float* inR, float* outL, float* outR, int frames);

    dsp::Param size{0.65f};
    dsp::Param damp{0.35f};
    dsp::Param mix{0.5f};

private:
    CombFilter combsL_[4];
    CombFilter combsR_[4];
    AllPassFilter allL_[2];
    AllPassFilter allR_[2];
};

class ChorusFx
{
public:
    void prepare(double sampleRate);
    void process(const float* inL, const float* inR, float* outL, float* outR, int frames);

    dsp::Param rate{0.6f};
    dsp::Param depth{3.5f};
    dsp::Param mix{0.5f};

private:
    std::vector<float> bufferL_;
    std::vector<float> bufferR_;
    int write_ = 0;
    double phase_ = 0.0;
    double sampleRate_ = 48000.0;
};

class FxProcessor
{
public:
    enum class Type { Off = 0, Delay = 1, Reverb = 2, Chorus = 3 };

    void prepare(double sampleRate);
    void process(const float* inL, const float* inR, float* outL, float* outR, int frames);

    Type type() const { return static_cast<Type>(static_cast<int>(type_.get())); }
    void setType(Type t) { type_.set(static_cast<float>(static_cast<int>(t))); }

    dsp::Param type_{0.0f};
    DelayFx delay;
    ReverbFx reverb;
    ChorusFx chorus;

private:
    double sampleRate_ = 48000.0;
};
