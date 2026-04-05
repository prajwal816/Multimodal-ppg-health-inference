#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <vector>

namespace ppg::sync {

/// Single-producer single-consumer queue with atomic indices; one slot reserved to disambiguate full/empty.
template <typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(std::size_t capacity_power_of_two)
        : cap_(capacity_power_of_two), mask_(cap_ - 1), storage_(cap_) {
        if ((cap_ & mask_) != 0 || cap_ < 2) {
            cap_ = 2;
            mask_ = 1;
            storage_.resize(cap_);
        }
    }

    bool try_push(T&& item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        const std::size_t next = (h + 1) & mask_;
        if (next == (t & mask_)) {
            return false;
        }
        storage_[h & mask_] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() noexcept(std::is_nothrow_move_assignable_v<T>) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        const std::size_t h = head_.load(std::memory_order_acquire);
        if ((t & mask_) == (h & mask_) && t == h) {
            return std::nullopt;
        }
        T out = std::move(storage_[t & mask_]);
        tail_.store((t + 1) & mask_, std::memory_order_release);
        return out;
    }

private:
    std::size_t cap_;
    std::size_t mask_;
    std::vector<T> storage_;
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace ppg::sync
