/**
 * @file test_concurrency.cpp
 * @brief Unit tests for Gaze::Concurrency::AtomicMailbox and Gaze::Concurrency::Pool
 */

#include "doctest.h"
#include "concurrency/atomic_mailbox.hpp"
#include "concurrency/pool.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

using namespace Gaze::Concurrency;

TEST_CASE("AtomicMailbox basic put and take lifecycle") {
    AtomicMailbox<int> mailbox;

    CHECK_FALSE(mailbox.is_pending());

    int val = 0;
    CHECK_FALSE(mailbox.take(val));

    mailbox.put(42);
    CHECK(mailbox.is_pending());

    CHECK(mailbox.take(val));
    CHECK(val == 42);
    CHECK_FALSE(mailbox.is_pending());

    // Second take should return false
    CHECK_FALSE(mailbox.take(val));
}

TEST_CASE("AtomicMailbox latest-value overwrite semantics") {
    AtomicMailbox<int> mailbox;

    mailbox.put(10);
    mailbox.put(20);
    mailbox.put(30);

    CHECK(mailbox.is_pending());

    int val = 0;
    CHECK(mailbox.take(val));
    CHECK(val == 30);
    CHECK_FALSE(mailbox.is_pending());
}

TEST_CASE("AtomicMailbox clear semantics") {
    AtomicMailbox<int> mailbox;

    mailbox.put(99);
    CHECK(mailbox.is_pending());

    mailbox.clear();
    CHECK_FALSE(mailbox.is_pending());

    int val = -1;
    CHECK_FALSE(mailbox.take(val));
    CHECK(val == -1);
}

TEST_CASE("AtomicMailbox multi-threaded producer consumer stress") {
    AtomicMailbox<uint64_t> mailbox;
    std::atomic<bool> stop_flag{false};
    std::atomic<uint64_t> consumed_count{0};
    uint64_t last_consumed = 0;
    bool order_violation = false;

    // Consumer thread
    std::thread consumer([&]() {
        while (!stop_flag.load(std::memory_order_relaxed)) {
            uint64_t item = 0;
            if (mailbox.take(item)) {
                if (item < last_consumed) {
                    order_violation = true;
                }
                last_consumed = item;
                consumed_count.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::yield();
        }
        // Drain any remaining
        uint64_t item = 0;
        if (mailbox.take(item)) {
            if (item < last_consumed) {
                order_violation = true;
            }
            consumed_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Producer thread
    const uint64_t total_items = 2000;
    for (uint64_t i = 1; i <= total_items; ++i) {
        mailbox.put(i);
        if (i % 100 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    stop_flag.store(true, std::memory_order_relaxed);
    consumer.join();

    CHECK_FALSE(order_violation);
    CHECK(consumed_count.load() > 0);
}

TEST_CASE("Pool basic take and release lifecycle") {
    struct TestData {
        int id = 0;
        float value = 0.0f;
    };

    Pool<TestData, 3> pool;
    CHECK(pool.size == 3);

    TestData* item1 = pool.take();
    REQUIRE(item1 != nullptr);
    item1->id = 1;
    item1->value = 1.5f;

    TestData* item2 = pool.take();
    REQUIRE(item2 != nullptr);
    item2->id = 2;

    TestData* item3 = pool.take();
    REQUIRE(item3 != nullptr);
    item3->id = 3;

    // Pool is now exhausted
    TestData* item4 = pool.take();
    CHECK(item4 == nullptr);

    // Release item2 and verify it can be taken again
    pool.release(item2);

    TestData* item_reused = pool.take();
    REQUIRE(item_reused == item2);
    CHECK(item_reused->id == 2);

    // Release all
    pool.release(item1);
    pool.release(item3);
    pool.release(item_reused);

    // Verify all 3 can be checked out again
    TestData* a = pool.take();
    TestData* b = pool.take();
    TestData* c = pool.take();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);
    CHECK(pool.take() == nullptr);

    pool.release(a);
    pool.release(b);
    pool.release(c);
}

TEST_CASE("Pool multi-threaded checkout and release stress") {
    struct FrameBuffer {
        uint64_t sequence = 0;
        char buffer[128];
    };

    Pool<FrameBuffer, 4> pool;
    std::atomic<bool> stop_flag{false};
    std::atomic<uint64_t> successful_passes{0};

    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&pool, &stop_flag, &successful_passes, t]() {
            uint64_t seq = 0;
            while (!stop_flag.load(std::memory_order_relaxed)) {
                FrameBuffer* frame = pool.take();
                if (frame) {
                    frame->sequence = ++seq;
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                    pool.release(frame);
                    successful_passes.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop_flag.store(true, std::memory_order_relaxed);

    for (auto& w : workers) {
        w.join();
    }

    CHECK(successful_passes.load() > 50);
}
