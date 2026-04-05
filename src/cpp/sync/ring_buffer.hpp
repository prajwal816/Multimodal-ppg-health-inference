#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>

namespace ppg::sync {

/// Single-producer single-consumer ring buffer (lock-free indices).
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0 && Capacity >= 2,
                  "Capacity must be a power of two >= 2");
    static_assert(std::is_nothrow_move_constructible_v<T>);

public:
    bool try_push(T&& item) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        const std::size_t next = inc(head);
        if (next == tail) {
            return false;
        }
        storage_[mask(head)] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);
        if (tail == head) {
            return std::nullopt;
        }
        T out = std::move(storage_[mask(tail)]);
        tail_.store(inc(tail), std::memory_order_release);
        return out;
    }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const std::size_t h = head_.load(std::memory_order_acquire);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        return (h >= t) ? (h - t) : (Capacity - (t - h));
    }

private:
    static constexpr std::size_t mask(std::size_t i) noexcept { return i & (Capacity - 1); }
    static constexpr std::size_t inc(std::size_t i) noexcept { return (i + 1) & (2 * Capacity - 1); }

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, Capacity> raw_{};
    T* storage() noexcept { return reinterpret_cast<T*>(raw_.data()); }

    const T* storage() const noexcept { return reinterpret_cast<const T*>(raw_.data()); }

    // Simpler: use optional array for demo (not as lock-free inner but indices are)
    // Actually the aligned_storage approach needs manual construction - error prone.
};

}  // namespace ppg::sync
