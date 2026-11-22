#include "Fx.h"

#include <algorithm>
#include <cmath>

namespace {

inline float readLinear(const std::vector<float>& buffer, int writeIndex, float delaySamples) noexcept
{
    const size_t size = buffer.size();
    if (size == 0) return 0.0f;
    float pos = static_cast<float>(writeIndex) - delaySamples;
    while (pos < 0.0f) pos += static_cast<float>(size);
    while (pos >= static_cast<float>(size)) pos -= static_cast<float>(size);
    const size_t i0 = static_cast<size_t>(pos);
    const size_t i1 = (i0 + 1) % size;
    const float frac = pos - static_cast<float>(i0);
    return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
}

inline float scaleDelay(int baseAt44100, double sampleRate) noexcept
{
    return static_cast<float>(baseAt44100 * sampleRate / 44100.0);
}

} // namespace

void DelayFx::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    const size_t n = static_cast<size_t>(sampleRate * 2.0) + 4;
    if (bufferL_.size() != n) {
        bufferL_.assign(n, 0.0f);
        bufferR_.assign(n, 0.0f);
        write_ = 0;
    }
}

void DelayFx::process(const float* inL, const float* inR, float* outL, float* outR, int frames)
{
    if (bufferL_.empty()) { std::fill(outL, outL + frames, 0.0f); std::fill(outR, outR + frames, 0.0f); return; }

    const float delay = dsp::clamp(timeMs.get(), 1.0f, 1900.0f) * 0.001f * static_cast<float>(sampleRate_);
    const float fb = dsp::clamp(feedback.get(), 0.0f, 0.92f);
    const float mixAmount = dsp::clamp(mix.get(), 0.0f, 1.0f);
    const int size = static_cast<int>(bufferL_.size());

    for (int i = 0; i < frames; ++i) {
        const float dl = readLinear(bufferL_, write_, delay);
        const float dr = readLinear(bufferR_, write_, delay * 1.07f);
        bufferL_[static_cast<size_t>(write_)] = inL[i] + dl * fb;
        bufferR_[static_cast<size_t>(write_)] = inR[i] + dr * fb;
        outL[i] = inL[i] * (1.0f - mixAmount) + dl * mixAmount;
        outR[i] = inR[i] * (1.0f - mixAmount) + dr * mixAmount;
        if (++write_ >= size) write_ = 0;
    }
}

void CombFilter::prepare(double sampleRate, int baseDelay, bool rightChannel)
{
    int length = static_cast<int>(scaleDelay(baseDelay + (rightChannel ? 23 : 0), sampleRate));
    length = std::max(16, length);
    buffer_.assign(static_cast<size_t>(length), 0.0f);
    index_ = 0;
    filter_ = 0.0f;
}

float CombFilter::process(float x, float feedback, float damp)
{
    if (buffer_.empty()) return x;
    const float y = buffer_[index_];
    filter_ = y * (1.0f - damp) + filter_ * damp;
    buffer_[index_] = x + filter_ * feedback;
    if (++index_ >= buffer_.size()) index_ = 0;
    return y;
}

void AllPassFilter::prepare(double sampleRate, int baseDelay, bool rightChannel)
{
    int length = static_cast<int>(scaleDelay(baseDelay + (rightChannel ? 11 : 0), sampleRate));
    length = std::max(16, length);
    buffer_.assign(static_cast<size_t>(length), 0.0f);
    index_ = 0;
}

float AllPassFilter::process(float x)
{
    if (buffer_.empty()) return x;
    const float y = buffer_[index_];
    buffer_[index_] = x + y * 0.5f;
    if (++index_ >= buffer_.size()) index_ = 0;
    return y - x;
}

void ReverbFx::prepare(double sampleRate)
{
    const int combs[4] = { 1116, 1188, 1277, 1356 };
    const int alls[2] = { 556, 441 };
    for (int i = 0; i < 4; ++i) {
        combsL_[i].prepare(sampleRate, combs[i], false);
        combsR_[i].prepare(sampleRate, combs[i], true);
    }
    for (int i = 0; i < 2; ++i) {
        allL_[i].prepare(sampleRate, alls[i], false);
        allR_[i].prepare(sampleRate, alls[i], true);
    }
}

void ReverbFx::process(const float* inL, const float* inR, float* outL, float* outR, int frames)
{
    const float room = dsp::clamp(size.get(), 0.0f, 1.0f);
    const float feedback = 0.72f + 0.26f * room;
    const float dampAmount = 0.10f + 0.80f * dsp::clamp(damp.get(), 0.0f, 1.0f);
    const float mixAmount = dsp::clamp(mix.get(), 0.0f, 1.0f);

    for (int i = 0; i < frames; ++i) {
        float wetL = 0.0f, wetR = 0.0f;
        for (int c = 0; c < 4; ++c) {
            wetL += combsL_[c].process(inL[i] * 0.25f, feedback, dampAmount);
            wetR += combsR_[c].process(inR[i] * 0.25f, feedback, dampAmount);
        }
        for (int c = 0; c < 2; ++c) {
            wetL = allL_[c].process(wetL);
            wetR = allR_[c].process(wetR);
        }
        outL[i] = inL[i] * (1.0f - mixAmount) + wetL * mixAmount * 0.7f;
        outR[i] = inR[i] * (1.0f - mixAmount) + wetR * mixAmount * 0.7f;
    }
}

void ChorusFx::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    const size_t n = static_cast<size_t>(sampleRate * 0.15) + 4;
    if (bufferL_.size() != n) {
        bufferL_.assign(n, 0.0f);
        bufferR_.assign(n, 0.0f);
        write_ = 0;
        phase_ = 0.0;
    }
}

void ChorusFx::process(const float* inL, const float* inR, float* outL, float* outR, int frames)
{
    if (bufferL_.empty()) { std::fill(outL, outL + frames, 0.0f); std::fill(outR, outR + frames, 0.0f); return; }

    const int size = static_cast<int>(bufferL_.size());
    const float rateHz = dsp::clamp(rate.get(), 0.05f, 8.0f);
    const float depthAmount = dsp::clamp(depth.get(), 0.0f, 12.0f) * 0.001f * static_cast<float>(sampleRate_);
    const float base = 0.020f * static_cast<float>(sampleRate_);
    const float mixAmount = dsp::clamp(mix.get(), 0.0f, 1.0f);
    const double increment = 2.0 * 3.14159265358979323846 * rateHz / std::max(1.0, sampleRate_);

    for (int i = 0; i < frames; ++i) {
        const float modL = std::sin(static_cast<float>(phase_)) * depthAmount;
        const float modR = std::sin(static_cast<float>(phase_ + 1.57079633)) * depthAmount;
        const float dl = readLinear(bufferL_, write_, base + modL);
        const float dr = readLinear(bufferR_, write_, base + modR);
        bufferL_[static_cast<size_t>(write_)] = inL[i];
        bufferR_[static_cast<size_t>(write_)] = inR[i];
        outL[i] = inL[i] * (1.0f - mixAmount) + dl * mixAmount;
        outR[i] = inR[i] * (1.0f - mixAmount) + dr * mixAmount;
        if (++write_ >= size) write_ = 0;
        phase_ += increment;
        if (phase_ > 2.0 * 3.14159265358979323846 * 1024.0) phase_ -= 2.0 * 3.14159265358979323846 * 1024.0;
    }
}

void FxProcessor::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    delay.prepare(sampleRate);
    reverb.prepare(sampleRate);
    chorus.prepare(sampleRate);
}

void FxProcessor::process(const float* inL, const float* inR, float* outL, float* outR, int frames)
{
    switch (type()) {
    case Type::Delay:  delay.process(inL, inR, outL, outR, frames); break;
    case Type::Reverb: reverb.process(inL, inR, outL, outR, frames); break;
    case Type::Chorus: chorus.process(inL, inR, outL, outR, frames); break;
    case Type::Off:
    default:
        std::fill(outL, outL + frames, 0.0f);
        std::fill(outR, outR + frames, 0.0f);
        break;
    }
}
