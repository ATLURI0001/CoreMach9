#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace core_mach9 {

#if defined(__cpp_lib_hardware_interference_size)
    using std::hardware_destructive_interference_size;
#else
    // Standard L1 cache line size on x86-64 and ARM64
    constexpr size_t hardware_destructive_interference_size = 64;
#endif

/**
 * Cache-line padded, wait-free Single-Producer Single-Consumer (SPSC) Ring Buffer.
 *
 * Guarantees:
 * - Zero heap allocations during enqueue/dequeue operations.
 * - Cache-line separation between head and tail to eliminate false sharing.
 * - std::memory_order_acquire / std::memory_order_release synchronization.
 * - Power-of-two capacity bitmask wrapping for single-cycle index arithmetic.
 */
template <typename T, size_t Capacity>
class LockFreeRingBuffer {
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

public:
    LockFreeRingBuffer()
        : head_(0)
        , tail_(0)
        , storage_(reinterpret_cast<StorageType*>(raw_buffer_)) {
    }

    ~LockFreeRingBuffer() {
        T item;
        while (pop(item)) {
            // Drain and destroy elements
        }
    }

    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;

    /**
     * Enqueue single item (Producer thread only).
     * Returns true on success, false if the buffer is full.
     */
    template <typename... Args>
    bool emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        if ((current_head - current_tail) >= Capacity) {
            return false; // Buffer full
        }

        const size_t index = current_head & Mask;
        new (storage_[index].data) T(std::forward<Args>(args)...);

        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    bool push(const T& item) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        return emplace(item);
    }

    bool push(T&& item) noexcept(std::is_nothrow_move_constructible_v<T>) {
        return emplace(std::move(item));
    }

    /**
     * Dequeue single item (Consumer thread only).
     * Returns true on success, false if the buffer is empty.
     */
    bool pop(T& item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if (current_tail == current_head) {
            return false; // Buffer empty
        }

        const size_t index = current_tail & Mask;
        T* slot = reinterpret_cast<T*>(storage_[index].data);
        item = std::move(*slot);
        slot->~T();

        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    /**
     * Dequeue a batch of up to max_items into a vector (Consumer thread only).
     * Returns the number of items successfully popped.
     */
    size_t pop_batch(std::vector<T>& destination, size_t max_items) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        const size_t available = current_head - current_tail;
        const size_t to_pop = std::min(available, max_items);

        if (to_pop == 0) return 0;

        destination.reserve(destination.size() + to_pop);
        for (size_t i = 0; i < to_pop; ++i) {
            const size_t index = (current_tail + i) & Mask;
            T* slot = reinterpret_cast<T*>(storage_[index].data);
            destination.push_back(std::move(*slot));
            slot->~T();
        }

        tail_.store(current_tail + to_pop, std::memory_order_release);
        return to_pop;
    }

    [[nodiscard]] size_t size() const noexcept {
        const size_t h = head_.load(std::memory_order_relaxed);
        const size_t t = tail_.load(std::memory_order_relaxed);
        return (h >= t) ? (h - t) : 0;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return Capacity;
    }

private:
    static constexpr size_t Mask = Capacity - 1;

    struct alignas(alignof(T)) StorageType {
        alignas(T) std::byte data[sizeof(T)];
    };

    // Producer cache line
    alignas(hardware_destructive_interference_size) std::atomic<size_t> head_;

    // Consumer cache line
    alignas(hardware_destructive_interference_size) std::atomic<size_t> tail_;

    // Storage buffer
    StorageType* storage_;
    alignas(hardware_destructive_interference_size) std::byte raw_buffer_[sizeof(StorageType) * Capacity];
};

} // namespace core_mach9
