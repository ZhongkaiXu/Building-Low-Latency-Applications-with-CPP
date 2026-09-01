// lock free queue, single producer, single consumer
// be useful for passing data between threads, e.g. producer-consumer pattern
#pragma once

#include <atomic>
#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>
#include <vector>

#include "macros.h"

namespace Common {
// Fixed-capacity, single-producer/single-consumer queue.
// A producer writes a slot before updateWriteIndex().
// A consumer finishes reading a slot before updateReadIndex().
template <typename T>
class LFQueue final {
    // 默认构造 因为初始化的时候一次性创建所有对象
    static_assert(std::is_default_constructible_v<T>,
        "LFQueue<T> requires a default-constructible T.");
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
        "LFQueue requires lock-free atomic<size_t> on this platform.");

public:
    explicit LFQueue(const std::size_t num_elems)
        : store_(storageSize(num_elems))
    {
    }

    // for producer
    [[nodiscard]] T* getNextToWriteTo() noexcept
    {
        // 只有producer自己会改，不需要同步，只要保证原子就行
        const auto write_index = next_write_index_.load(std::memory_order_relaxed);
        // 下次可写入的位置
        const auto next_write_index = nextIndex(write_index);
        // acquire load 保证读到这个值时，对方的写入操作都完成了
        const auto read_index = next_read_index_.load(std::memory_order_acquire);

        // 和read对比，看是否满了，满了就返回nullptr
        // 假设2个元素，实际上产生3个元素的数组，写完2个之后，当前write-index=2，next_write_index=0，read_index=0，满了
        // 不允许写入，每次都要保持一个空的slot，避免read_index和write_index相等时无法区分是空还是满
        return next_write_index == read_index ? nullptr : &store_[write_index];
    }

    void updateWriteIndex() noexcept
    {
        const auto write_index = next_write_index_.load(std::memory_order_relaxed);
        const auto next_write_index = nextIndex(write_index);

        ASSERT(next_write_index != next_read_index_.load(std::memory_order_acquire),
            "Cannot publish an element to a full LFQueue.");
        // release store 保证之前的写入操作对其他线程可见
        // 也就是说之前的操作必须在这个store之前完成，保证了数据的可见性
        next_write_index_.store(next_write_index, std::memory_order_release);
    }

    [[nodiscard]] const T* getNextToRead() const noexcept
    {
        const auto read_index = next_read_index_.load(std::memory_order_relaxed);
        const auto write_index = next_write_index_.load(std::memory_order_acquire);

        // 队列空了，返回nullptr
        return read_index == write_index ? nullptr : &store_[read_index];
    }

    void updateReadIndex() noexcept
    {
        const auto read_index = next_read_index_.load(std::memory_order_relaxed);

        ASSERT(read_index != next_write_index_.load(std::memory_order_acquire),
            "Cannot consume an element from an empty LFQueue.");
        next_read_index_.store(nextIndex(read_index), std::memory_order_release);
    }

    // 当前队列中元素的数量，注意是当前可读的元素数量
    [[nodiscard]] std::size_t size() const noexcept
    {
        const auto read_index = next_read_index_.load(std::memory_order_acquire);
        const auto write_index = next_write_index_.load(std::memory_order_acquire);

        return write_index >= read_index ? write_index - read_index : store_.size() - read_index + write_index;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return next_read_index_.load(std::memory_order_acquire) == next_write_index_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return store_.size() - 1;
    }

    LFQueue() = delete;
    LFQueue(const LFQueue&) = delete;
    LFQueue(LFQueue&&) = delete;
    LFQueue& operator=(const LFQueue&) = delete;
    LFQueue& operator=(LFQueue&&) = delete;

private:
#if defined(__cpp_lib_hardware_interference_size)
    static constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    static constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

    static std::size_t storageSize(const std::size_t capacity) noexcept
    {
        ASSERT(capacity > 0, "LFQueue capacity must be greater than zero.");
        ASSERT(capacity < std::numeric_limits<std::size_t>::max(),
            "LFQueue capacity is too large.");
        return capacity + 1;
    }

    [[nodiscard]] std::size_t nextIndex(const std::size_t index) const noexcept
    {
        const auto next_index = index + 1;
        return next_index == store_.size() ? 0 : next_index;
    }

    std::vector<T> store_;

    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> next_write_index_ { 0 };
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> next_read_index_ { 0 };
};
}
