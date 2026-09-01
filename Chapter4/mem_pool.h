#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "macros.h"

namespace Common {
template <typename T>
// final: Prevent inheritance from this class.
class MemPool final {
    // assert check when compiling
    // int&\void are not obj types
    static_assert(std::is_object_v<T>, "MemPool<T> requires T to be an object type.");
    // MemPool<const T> or MemPool<volatile T> is not allowed
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>,
        "MemPool<T> requires a non-cv-qualified T.");
    // MemPool<T> requires a destructible T.
    // ~T must be accessible(public) and not deleted.
    static_assert(std::is_destructible_v<T>, "MemPool<T> requires a destructible T.");

public:
    explicit MemPool(const std::size_t num_elems)
        : store_(num_elems)
        , num_free_blocks_(num_elems)
    {
        ASSERT(num_elems > 0, "Memory Pool must contain at least one ObjectBlock.");
    }

    // for destructor, default is not noexcept, no likely to other functions
    // default is ~MemPool() noexcept(true)
    ~MemPool() noexcept(std::is_nothrow_destructible_v<T>)
    {
        for (auto& object_block : store_) {
            if (!object_block.is_free_) {
                std::destroy_at(object_block.object());
            }
        }
    }

    template <typename... Args>
    T* allocate(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
    {
        ASSERT(num_free_blocks_ > 0, "Memory Pool out of space.");

        auto& object_block = store_[next_free_index_];
        ASSERT(object_block.is_free_,
            "Expected free ObjectBlock at index:" + std::to_string(next_free_index_));

        // byte* -> void*  (static_Cast)
        // void* -> T*  (::new)
        // placement new, ``new (address) T(args...)`` constructs a T object at the specified address.
        // ``::`` is used to call the global new operator, avoiding any overloads in the current scope.
        T* ret = ::new (static_cast<void*>(object_block.storage_))
            T(std::forward<Args>(args)...);
        object_block.is_free_ = false;
        --num_free_blocks_;

        if (num_free_blocks_ > 0) {
            // update next_free_index_
            updateNextFreeIndex();
        }

        return ret;
    }

    void deallocate(const T* elem) noexcept(std::is_nothrow_destructible_v<T>)
    {
        ASSERT(elem != nullptr, "Cannot deallocate a null pointer.");

        const auto elem_index = findElementIndex(elem);
        ASSERT(elem_index < store_.size(),
            "Element being deallocated does not belong to this Memory pool.");

        auto& object_block = store_[elem_index];
        ASSERT(!object_block.is_free_,
            "Expected in-use ObjectBlock at index:" + std::to_string(elem_index));

        const bool was_full = (num_free_blocks_ == 0);
        std::destroy_at(object_block.object());
        object_block.is_free_ = true;
        ++num_free_blocks_;

        if (was_full) {
            next_free_index_ = elem_index;
        }
    }

    MemPool() = delete;
    MemPool(const MemPool&) = delete;
    MemPool(MemPool&&) = delete;
    MemPool& operator=(const MemPool&) = delete;
    MemPool& operator=(MemPool&&) = delete;

private:
    struct ObjectBlock {
        // alignof(ObjectBlock) >= alignof(T)
        alignas(T) std::byte storage_[sizeof(T)];
        bool is_free_ = true;

        ObjectBlock() noexcept = default;
        ObjectBlock(const ObjectBlock&) = delete;
        ObjectBlock(ObjectBlock&&) = delete;
        ObjectBlock& operator=(const ObjectBlock&) = delete;
        ObjectBlock& operator=(ObjectBlock&&) = delete;

        T* object() noexcept
        {
            return std::launder(reinterpret_cast<T*>(storage_));
        }
    };

    void updateNextFreeIndex() noexcept
    {
        do {
            ++next_free_index_;
            if (UNLIKELY(next_free_index_ == store_.size())) {
                next_free_index_ = 0;
            }
        } while (!store_[next_free_index_].is_free_);
    }

    std::size_t findElementIndex(const T* elem) const noexcept
    {
        for (std::size_t index = 0; index < store_.size(); ++index) {
            const auto* block_address = reinterpret_cast<const T*>(store_[index].storage_);
            if (block_address == elem) {
                return index;
            }
        }
        return store_.size();
    }

    std::vector<ObjectBlock> store_;
    // next index to put a new valid element
    std::size_t next_free_index_ = 0;
    // number of free blocks in the pool
    std::size_t num_free_blocks_ = 0;
};
}
