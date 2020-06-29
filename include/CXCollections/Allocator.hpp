#ifndef CX_ALLOCATOR_H
#define CX_ALLOCATOR_H


// MIT License - CXCollections
// Copyright(c) 2020 Dante Falcone (dantefalcone@gmail.com)


#include <stdint.h>
#include <cstddef>
#include <type_traits>
#include <new>
#include <assert.h>


namespace cyber
{

    // operator new allocator with alignment support - compatible with stl
    // warning: when using with containers that allocate in large blocks (std::vector) this only allocates the large block with alignment
    template<typename T, size_t alignment>
    struct Allocator
    {
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;

        static constexpr std::align_val_t align_value = static_cast<std::align_val_t>(alignment);

        constexpr Allocator() noexcept {}
        constexpr Allocator(const Allocator&) noexcept = default;
        template <class Tx> constexpr Allocator(Allocator<Tx, alignment> const&) noexcept {}
        template <typename Tx> struct rebind { typedef Allocator<Tx, alignment> other; };

        inline T* allocate(std::size_t n) noexcept
        {
            return reinterpret_cast<T*>(::operator new(sizeof(T) * n, align_value));
        }

        inline void deallocate(T* p, std::size_t bytes) noexcept
        {
            ::operator delete(p, bytes, align_value);
        }
    };


    // preallocates a large block of memory from which to allocate from
    // operates using the free-list technique, using free object memory to store a link to the next free item
    // automatically allocates new block when capacity reached
    // DO NOT USE WITH std::vector, WARNING: std::list allocates on constructor
    template<typename T, size_t alignment = 64, size_t blockCapacity = 0xFFFF / sizeof(T) /*64kb*/>
    struct PoolAllocator
    {
        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        static constexpr std::align_val_t align_value = static_cast<std::align_val_t>(alignment);
        static constexpr size_t block_capacity = blockCapacity;

        struct Node { Node* next; };
        static_assert(sizeof(T) >= sizeof(Node), "value_type must be at least 8 bytes");
        struct Block { Block* prev; Block* next; };

        //-- data members
        void* data = nullptr;
        Node* next = nullptr;
        //-- 16 bytes

        //-- std
        constexpr PoolAllocator() noexcept { }; // hack - list does 1 allocation on construct
        constexpr PoolAllocator(const PoolAllocator&) noexcept {  };
        template <class Tx> constexpr PoolAllocator(PoolAllocator<Tx, alignment, blockCapacity> const&) noexcept {  }
        template <typename Tx> struct rebind { typedef PoolAllocator<Tx, alignment, blockCapacity> other; };
        //--

        // manually malloc the pool memory with n objects
        void* malloc(size_t n)
        {
            // sneakily store block link data in heap alloc
            size_t bytes = sizeof(Block) + sizeof(T) * n;
            data = ::operator new(bytes, align_value);
            
            Block* block = reinterpret_cast<Block*>(data);
            block->prev = nullptr;
            block->next = nullptr;

            // assign the free nodes of the pool
            T* data_begin = reinterpret_cast<T*>(block + 1);
            T* data_last = data_begin + n - 1;
            next = reinterpret_cast<Node*>(data_begin);
            for (T* itr = data_begin; itr != data_last; ++itr)
            {
                // O(n) to set next pointers, additional data members for HEAD/CAP could skip this but favoring size for now
                reinterpret_cast<Node*>(itr)->next = reinterpret_cast<Node*>(itr + 1);
            }
            reinterpret_cast<Node*>(data_last)->next = nullptr;

            return data;
        }

        // manually free the current pool block memory
        void free(void* block_data)
        {
            assert(block_data);
            Block* block_del = reinterpret_cast<Block*>(block_data);
            Block* block_prev = block_del->prev;
            Block* block_next = block_del->next;

            if (block_prev)
            {
                block_prev->next = block_next;
            }

            ::operator delete(block_data, align_value);
        }

        // free all memory block allocations
        void free()
        {
            Block* block_del = reinterpret_cast<Block*>(data);
            assert(block_del);

            // seek first block
            Block* block_prev = block_del->prev;
            while (block_prev)
            {
                block_del = block_prev;
                block_prev = block_prev->prev;
            }

            // free each block from first to last
            while (block_del)
            {
                Block* block_next = block_del->next;
                this->free(block_del);
                block_del = block_next;
            }
        }

        // std compatible
        // allocate one object from the pool, allocate new pool block if nescesary
        inline T* allocate(size_t n) noexcept
        {
            assert(n == 1 && "can only support one allocation at a time");
                        
            // skip if can't grow TODO (extra template param to disable)
            if (next == nullptr)
            {
                // allocate next block
                Block* block_old = reinterpret_cast<Block*>(data);
                this->malloc(blockCapacity);
                Block* block_new = reinterpret_cast<Block*>(data);
                block_new->prev = block_old;
            }

            Node* cur = next;
            next = cur->next;

            return reinterpret_cast<T*>(cur);
        }

        // std compatible
        // deallocate one object from the pool
        inline void deallocate(T* p, size_t bytes) noexcept
        {
            Node* next_old = reinterpret_cast<Node*>(next);
            Node* next_new = reinterpret_cast<Node*>(p);
            next_new->next = next_old;
        }
    };


}

#endif // !CX_ALLOCATOR_H
