#pragma once
#include <mutex>
#include <utility>

namespace Gaze {

/**
 * @class AtomicMailbox
 * @brief Thread-safe single-slot mailbox for latest-value handoffs.
 *
 * Implements a single-element buffer where a new 'put' overwrites the previous
 * value, and 'take' moves the value out and clears the pending state.
 * Useful for decoupling threads where only the latest state is relevant (e.g. camera frames and tracking results).
 */
template <typename T>
class AtomicMailbox {
private:
    mutable std::mutex mutex;
    T slot;
    bool pending = false;

public:
    AtomicMailbox() = default;
    ~AtomicMailbox() = default;

    // Overwrite the slot with a new value and mark it as pending
    void put(T value) {
        std::lock_guard<std::mutex> lock(mutex);
        slot = std::move(value);
        pending = true;
    }

    // Move the slot value out if pending, clearing the pending flag. Returns true if successful.
    bool take(T& out_value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!pending) {
            return false;
        }
        out_value = std::move(slot);
        pending = false;
        return true;
    }

    // Reset the slot and clear the pending flag
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        slot = T();
        pending = false;
    }

    // Check if a new value is waiting in the slot
    bool is_pending() const {
        std::lock_guard<std::mutex> lock(mutex);
        return pending;
    }
};

} // namespace Gaze
