/**
 * @file atomic_mailbox.hpp
 * @brief Thread-safe single-slot mailbox for latest-value handoffs.
 *
 * Implements a thread-safe single-element buffer where a new 'put' overwrites the previous
 * value, and 'take' moves the value out and clears the pending state.
 * Useful for decoupling producer/consumer threads where only the latest state is relevant
 * (e.g. camera frame ingestion and tracking result dispatch).
 */
#pragma once

#include <mutex>
#include <utility>

namespace Gaze {
namespace Concurrency {

template <typename T>
class AtomicMailbox {
private:
    mutable std::mutex mutex;
    T slot;
    bool pending = false;

public:
    AtomicMailbox() = default;
    ~AtomicMailbox() = default;

    AtomicMailbox(const AtomicMailbox&) = delete;
    AtomicMailbox& operator=(const AtomicMailbox&) = delete;

    AtomicMailbox(AtomicMailbox&&) = delete;
    AtomicMailbox& operator=(AtomicMailbox&&) = delete;

    /**
     * @brief Overwrite the slot with a new value and mark it as pending.
     */
    void put(T value) {
        std::lock_guard<std::mutex> lock(mutex);
        slot = std::move(value);
        pending = true;
    }

    /**
     * @brief Move the slot value out if pending, clearing the pending flag.
     * @param out_value Output reference to receive the moved value.
     * @return true if a pending value was retrieved, false if mailbox was empty.
     */
    bool take(T& out_value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!pending) {
            return false;
        }
        out_value = std::move(slot);
        pending = false;
        return true;
    }

    /**
     * @brief Reset the slot to default and clear the pending flag.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        slot = T();
        pending = false;
    }

    /**
     * @brief Check if a new value is waiting in the slot.
     */
    bool is_pending() const {
        std::lock_guard<std::mutex> lock(mutex);
        return pending;
    }
};

} // namespace Concurrency

// Backwards-compatible type alias in root Gaze namespace
template <typename T>
using AtomicMailbox = Concurrency::AtomicMailbox<T>;

} // namespace Gaze
