#pragma once

#include <algorithm>
#include <atomic>
#include <vector>

// Ring buffer estereo de produtor unico / consumidor unico, sem alocacao na thread de audio.
// Usado para cruzar entre as threads do WASAPI e do ASIO (ambas a mesma taxa nominal).
class StereoRing
{
public:
    explicit StereoRing(size_t requestedCapacity = 4096)
        : capacity_(nextPowerOfTwo(std::max<size_t>(64, requestedCapacity))),
          mask_(capacity_ - 1),
          left_(capacity_, 0.0f),
          right_(capacity_, 0.0f)
    {
    }

    void reset()
    {
        write_.store(0, std::memory_order_relaxed);
        read_.store(0, std::memory_order_relaxed);
    }

    // Produtor: escreve ate 'frames'. Retorna a quantidade efetivamente escrita.
    size_t write(const float* left, const float* right, size_t frames)
    {
        const size_t w = write_.load(std::memory_order_relaxed);
        const size_t r = read_.load(std::memory_order_acquire);
        const size_t used = w - r;
        const size_t room = capacity_ - used;
        const size_t count = std::min(frames, room);
        for (size_t i = 0; i < count; ++i) {
            const size_t index = (w + i) & mask_;
            left_[index] = left[i];
            right_[index] = right[i];
        }
        write_.store(w + count, std::memory_order_release);
        return count;
    }

    // Consumidor: le ate 'frames', preenchendo o restante com silencio.
    // Retorna a quantidade efetivamente lida.
    size_t read(float* left, float* right, size_t frames)
    {
        const size_t r = read_.load(std::memory_order_relaxed);
        const size_t w = write_.load(std::memory_order_acquire);
        const size_t used = w - r;
        const size_t count = std::min(frames, used);
        for (size_t i = 0; i < count; ++i) {
            const size_t index = (r + i) & mask_;
            left[i] = left_[index];
            right[i] = right_[index];
        }
        std::fill(left + count, left + frames, 0.0f);
        std::fill(right + count, right + frames, 0.0f);
        read_.store(r + count, std::memory_order_release);
        return count;
    }

    size_t available() const
    {
        return write_.load(std::memory_order_acquire) - read_.load(std::memory_order_acquire);
    }

    size_t capacity() const { return capacity_; }

private:
    static size_t nextPowerOfTwo(size_t value)
    {
        size_t result = 1;
        while (result < value) result <<= 1;
        return result;
    }

    size_t capacity_;
    size_t mask_;
    std::vector<float> left_;
    std::vector<float> right_;
    std::atomic<size_t> write_{0};
    std::atomic<size_t> read_{0};
};
