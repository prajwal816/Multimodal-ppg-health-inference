#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <vector>

namespace ppg::sync {

/// SPSC queue; internal buffer holds `capacity` slots, usable `capacity - 1` elements.
template <typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(std::size_t capacity)
        : cap_(std::max<std::size_t>(2, capacity)), storage_(cap_) {}

    bool try_push(T&& item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        const std::size_t next = (h + 1) % cap_;
        if (next == t) {
            return false;
        }
        storage_[h] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() noexcept(std::is_nothrow_move_assignable_v<T>) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        const std::size_t h = head_.load(std::memory_order_acquire);
        if (t == h) {
            return std::nullopt;
        }
        T out = std::move(storage_[t]);
        tail_.store((t + 1) % cap_, std::memory_order_release);
        return out;
    }

private:
    const std::size_t cap_;
    std::vector<T> storage_;
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace ppg::sync
