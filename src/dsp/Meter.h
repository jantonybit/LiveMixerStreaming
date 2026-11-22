#pragma once

#include "DspUtils.h"
#include <atomic>
#include <cmath>

// Pico medidor com retencao de pico. Atualizado apenas na thread de audio.
class PeakMeter
{
public:
    void reset() noexcept
    {
        curL_ = curR_ = holdL_ = holdR_ = 0.0f;
        holdFrames_ = 0;
        pubL_.store(0.0f);
        pubR_.store(0.0f);
        pubHoldL_.store(0.0f);
        pubHoldR_.store(0.0f);
    }

    void update(float left, float right) noexcept
    {
        const float l = std::fabs(left);
        const float r = std::fabs(right);
        if (l > curL_) curL_ = l;
        if (r > curR_) curR_ = r;
        if (l > holdL_) { holdL_ = l; holdFrames_ = 0; }
        if (r > holdR_) { holdR_ = r; holdFrames_ = 0; }
    }

    void commit(int frames, double sampleRate) noexcept
    {
        pubL_.store(curL_, std::memory_order_relaxed);
        pubR_.store(curR_, std::memory_order_relaxed);
        curL_ = curR_ = 0.0f;

        holdFrames_ += frames;
        if (holdFrames_ > static_cast<int>(sampleRate * 1.1)) {
            holdL_ *= 0.86f;
            holdR_ *= 0.86f;
        }
        pubHoldL_.store(holdL_, std::memory_order_relaxed);
        pubHoldR_.store(holdR_, std::memory_order_relaxed);
    }

    float left() const noexcept { return pubL_.load(std::memory_order_relaxed); }
    float right() const noexcept { return pubR_.load(std::memory_order_relaxed); }
    float holdLeft() const noexcept { return pubHoldL_.load(std::memory_order_relaxed); }
    float holdRight() const noexcept { return pubHoldR_.load(std::memory_order_relaxed); }

private:
    std::atomic<float> pubL_{0.0f};
    std::atomic<float> pubR_{0.0f};
    std::atomic<float> pubHoldL_{0.0f};
    std::atomic<float> pubHoldR_{0.0f};
    float curL_ = 0.0f;
    float curR_ = 0.0f;
    float holdL_ = 0.0f;
    float holdR_ = 0.0f;
    int holdFrames_ = 0;
};
