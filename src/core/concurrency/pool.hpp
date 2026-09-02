/**
 * @file pool.hpp
 * @brief Zero-allocation thread-safe fixed-capacity object pool.
 *
 * Pre-allocates a fixed array of items for zero-allocation checkout (`take()`) and $O(1)$ release.
 * Uses compile-time offset validation to safely map checked-out instance pointers back to pool slots.
 */
#pragma once

#include <array>
#include <mutex>
#include <cstddef>
#include <type_traits>

namespace Gaze {
namespace Concurrency {

template <typename T, size_t SZ = 2>
class Pool {
private:
    struct Item {
        T instance; // Must remain the first member for O(1) offset alignment
        bool in_use = false;
    };
    static_assert(offsetof(Item, instance) == 0, "instance must be at offset 0 of Item for pointer casting");

    std::array<Item, SZ> items;
    mutable std::mutex mutex;

public:
    static constexpr size_t size = SZ;

    Pool() = default;
    ~Pool() = default;

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;

    /**
     * @brief Checks out an idle instance from the pool and marks it in_use.
     * @return Pointer to checked-out instance, or nullptr if pool is exhausted.
     */
    T* take() {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& item : items) {
            if (!item.in_use) {
                item.in_use = true;
                return &item.instance;
            }
        }
        return nullptr;
    }

    /**
     * @brief Returns a checked-out instance back to the pool in O(1) time.
     * @param instance Pointer previously returned by take().
     */
    void release(const T* instance) {
        if (!instance) {
            return;
        }
        Item* item = const_cast<Item*>(reinterpret_cast<const Item*>(instance));
        std::lock_guard<std::mutex> lock(mutex);
        item->in_use = false;
    }

    /**
     * @brief Direct index access to underlying instance.
     */
    T* get(size_t idx) {
        if (idx >= SZ) {
            return nullptr;
        }
        return &items[idx].instance;
    }

    /**
     * @brief Direct index access to underlying instance (alias).
     */
    T* get_frame(size_t idx) {
        return get(idx);
    }
};

} // namespace Concurrency

// Backwards-compatible type alias in root Gaze namespace
template <typename T, size_t SZ = 2>
using Pool = Concurrency::Pool<T, SZ>;

} // namespace Gaze
