// TODO: Comments.
#pragma once
#include <array>
#include <mutex>
#include <cstddef>

template <typename T, size_t SZ = 2>
class Pool
{
private:
    struct Item
    {
        T instance; // Must remain the first member for O(1) offset alignment
        bool in_use = false;
    };
    std::array<Item, SZ> items;
    std::mutex mutex;

public:
    static constexpr size_t size = SZ;

    Pool() = default;
    ~Pool() = default;

    // Checks out an idle instance and marks it in_use.
    T *take()
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto &item : items)
        {
            if (!item.in_use)
            {
                item.in_use = true;
                return &item.instance;
            }
        }
        return nullptr;
    }

    // O(1) release back to pool by casting the T* pointer directly to Item*
    void release(const T *instance)
    {
        if (!instance)
            return;
        Item *item = const_cast<Item *>(reinterpret_cast<const Item *>(instance));
        std::lock_guard<std::mutex> lock(mutex);
        item->in_use = false;
    }

    T *get_frame(size_t idx)
    {
        if (idx >= SZ)
            return nullptr;
        return &items[idx].instance;
    }
};
