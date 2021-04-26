// MIT License - CXCollections
// Copyright(c) 2020 Dante Falcone (dantefalcone@gmail.com)

#ifndef CX_ALLOCATOR_H
#define CX_ALLOCATOR_H

#include <stdint.h>
#include <cstddef>
#include <type_traits>
#include <new>
#include <assert.h>

#define MIN_SUBBLOCK_CAPACITY 0x00000010
#define MAX_SUBBLOCK_CAPACITY 0x08000000

namespace cyber
{
    namespace _ {
        struct Node { Node* next; };
        struct Block { Block* prev; Block* next; };
    }

    // operator new allocator with alignment support - compatible with stl
    // warning: when using with containers that allocate in large blocks (std::vector) this only allocates the large block with alignment
    template<typename T, size_t alignment = 64>
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

        inline value_type* allocate(std::size_t n) noexcept
        {
            return reinterpret_cast<value_type*>(::operator new(sizeof(value_type) * n, align_value));
        }

        inline void deallocate(value_type* p, std::size_t n) noexcept
        {
            ::operator delete(p, align_value);
        }
    };

    template<typename T>
    struct _PoolAllocatorStorage_Unique
    {
        void* data = nullptr;
        _::Node* next = nullptr;
    };

    template<typename T>
    struct _PoolAllocatorStorage_Static
    {
        static void* data;
        static _::Node* next;
    };

    template<typename T>
    void* _PoolAllocatorStorage_Static<T>::data = nullptr;

    template<typename T>
    _::Node* _PoolAllocatorStorage_Static<T>::next = nullptr;

    template<typename T, size_t alignment, size_t capacity, typename Storage_T>
    struct _PoolAllocatorImplementation : Storage_T
    {
        static_assert(capacity != 0, "type too large for default block capacity"); // division by sizeof(T) is fraction

        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        static constexpr std::align_val_t align_value = static_cast<std::align_val_t>(alignment);
        static constexpr size_t block_capacity = capacity;
        static constexpr size_t data_size = sizeof(value_type) < sizeof(_::Node) ? sizeof(_::Node) : sizeof(value_type);

        // manually malloc the pool memory with n objects
        void* malloc(size_t n) noexcept
        {
            // sneakily store block link data in heap alloc
            size_t bytes = sizeof(_::Block) + data_size * n;
            this->data = ::operator new(bytes, align_value);

            _::Block* block = reinterpret_cast<_::Block*>(this->data);
            block->prev = nullptr;
            block->next = nullptr;

            // assign the free nodes of the pool
            value_type* data_begin = reinterpret_cast<value_type*>(block + 1);
            value_type* data_last = data_begin + n - 1;
            this->next = reinterpret_cast<_::Node*>(data_begin);
            for (value_type* itr = data_begin; itr != data_last; ++itr)
            {
                // O(n) to set next pointers, additional data members for HEAD/CAP could skip this but favoring size for now
                reinterpret_cast<_::Node*>(itr)->next = reinterpret_cast<_::Node*>(itr + 1);
            }
            reinterpret_cast<_::Node*>(data_last)->next = nullptr;

            return this->data;
        }

        // manually free the current pool block memory
        void free(void* block_data) noexcept
        {
            assert(block_data);
            _::Block* block_del = reinterpret_cast<_::Block*>(block_data);
            _::Block* block_prev = block_del->prev;
            _::Block* block_next = block_del->next;

            if (block_prev)
            {
                block_prev->next = block_next;
            }

            if (block_next)
            {
                block_next->prev = block_prev;
            }

            ::operator delete(block_data, align_value);
        }

        // free all memory block allocations
        void free() noexcept
        {
            _::Block* block_del = reinterpret_cast<_::Block*>(this->data);
            assert(block_del);

            // seek first block
            _::Block* block_prev = block_del->prev;
            while (block_prev)
            {
                block_del = block_prev;
                block_prev = block_prev->prev;
            }

            // free each block from first to last
            while (block_del)
            {
                _::Block* block_next = block_del->next;
                this->free(block_del);
                block_del = block_next;
            }
        }

        // std compatible
        // allocate one object from the pool, allocate new pool block if nescesary
        value_type* allocate(size_t n) noexcept
        {
            assert(n == 1 && "can only support one allocation at a time");

            // skip if can't grow TODO (extra template param to disable)
            if (this->next == nullptr)
            {
                // allocate next block
                _::Block* block_old = reinterpret_cast<_::Block*>(this->data);
                this->malloc(capacity);
                _::Block* block_new = reinterpret_cast<_::Block*>(this->data);
                block_new->prev = block_old;
                if (block_old)
                {
                    block_old->next = block_new;
                }
            }

            _::Node* cur = this->next;
            this->next = cur->next;

            return reinterpret_cast<value_type*>(cur);
        }

        // std compatible
        // deallocate one object from the pool
        void deallocate(value_type * p, size_t n) noexcept
        {
            _::Node* next_old = reinterpret_cast<_::Node*>(this->next);
            _::Node* next_new = reinterpret_cast<_::Node*>(p);

            next_new->next = next_old;
            this->next = next_new;
        }

        // find block from data
        _::Block* find_block(value_type * p)
        {
            uintptr_t bgn_val, end_val;
            uintptr_t p_val = uintptr_t(p);

            _::Block* block = reinterpret_cast<_::Block*>(this->data);
            assert(block);

            while (block->prev != nullptr)
            {
                bgn_val = uintptr_t(block);
                end_val = uintptr_t(reinterpret_cast<uint8_t*>(block) + (sizeof(_::Block) * data_size * capacity));

                if (bgn_val < p_val && p_val < end_val)
                    return block;

                block = block->prev;
            }

            return nullptr;
        }
    };

    // preallocates a large block of memory from which to allocate from
    // storage is static per type
    // operates using the free-list technique, using free object memory to store a link to the next free item
    // allocates new aligned block when capacity reached, blocks are kept as a linked list
    // deallocates blocks when they become empty
    // DO NOT USE WITH std::vector, WARNING: std::list allocates on constructor
    template<typename T, size_t alignment = 64, size_t capacity = 0x10000 /*64kb*/ / sizeof(T) >
    struct StaticPoolAllocator : _PoolAllocatorImplementation<T, alignment, capacity, _PoolAllocatorStorage_Static<T>>
    {
        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        //-- std
        constexpr StaticPoolAllocator() noexcept { }; // hack - list does 1 allocation on construct
        constexpr StaticPoolAllocator(const StaticPoolAllocator&) noexcept = default;
        ~StaticPoolAllocator() noexcept { };
        template <class Tx> constexpr StaticPoolAllocator(StaticPoolAllocator<Tx, alignment, capacity> const&) noexcept {  }
        template <typename Tx> struct rebind { typedef StaticPoolAllocator<Tx, alignment, capacity> other; };
        //--
    };

    // preallocates a large block of memory from which to allocate from
    // storage lives with allocator object - will increase size of type that uses this by size of 2 pointers
    // operates using the free-list technique, using free object memory to store a link to the next free item
    // allocates new aligned block when capacity reached, blocks are kept as a linked list
    // deallocates blocks when they become empty
    // DO NOT USE WITH std::vector, WARNING: std::list allocates on constructor
    template<typename T, size_t alignment = 64, size_t capacity = 0x10000 /*64kb*/ / sizeof(T) >
    struct UniquePoolAllocator : _PoolAllocatorImplementation<T, alignment, capacity, _PoolAllocatorStorage_Unique<T>>
    {
        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        //-- std
        constexpr UniquePoolAllocator() noexcept { }; // hack - list does 1 allocation on construct
        constexpr UniquePoolAllocator(const UniquePoolAllocator&) noexcept = default;
        ~UniquePoolAllocator() noexcept { };
        template <class Tx> constexpr UniquePoolAllocator(UniquePoolAllocator<Tx, alignment, capacity> const&) noexcept {  }
        template <typename Tx> struct rebind { typedef UniquePoolAllocator<Tx, alignment, capacity> other; };
        //--
    };

    struct FixedPoolAllocatorWrapper
    {
        //-- data members
        void* data = nullptr;
        _::Node* next = nullptr;
        //-- 

        // std compatible
        // allocate one object from the pool, allocate new pool block if nescesary
        inline void* allocate(size_t n) noexcept
        {
            assert(n == 1 && "can only support one allocation at a time");
            assert(this->next && "fixed pool out of memory - most likely will crash");

            _::Node* cur = this->next;
            this->next = cur->next;

            return reinterpret_cast<void*>(cur);
        }

        // std compatible
        // deallocate one object from the pool
        inline void deallocate(void* p, size_t n) noexcept
        {
            _::Node* next_old = reinterpret_cast<_::Node*>(this->next);
            _::Node* next_new = reinterpret_cast<_::Node*>(p);

            next_new->next = next_old;
            this->next = next_new;
        }
    };

    // heap
    template<typename T, size_t alignment = 64, size_t capacity = 0xFFFF / sizeof(T) >
    struct FixedPoolAllocator
    {
        static_assert(capacity != 0, "type too large for default block capacity"); // division by sizeof(T) is fraction

        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        static constexpr std::align_val_t align_value = static_cast<std::align_val_t>(alignment);
        static constexpr size_t block_capacity = capacity;
        static constexpr size_t data_size = sizeof(value_type) < sizeof(_::Node) ? sizeof(_::Node) : sizeof(value_type);

        //-- data members
        void* data = nullptr;
        _::Node* next = nullptr;
        //-- 

        //-- std
        constexpr FixedPoolAllocator() noexcept { this->malloc(capacity); }; // list does 1 allocation on construct
        constexpr FixedPoolAllocator(const FixedPoolAllocator& rhs) noexcept { data = rhs.data; next = rhs.next; };
        ~FixedPoolAllocator() noexcept { /*free(); data = nullptr; next = nullptr;*/ }
        template <class Tx> constexpr FixedPoolAllocator(FixedPoolAllocator<Tx, alignment, capacity> const& rhs) noexcept {  data = rhs.data; next = rhs.next; }
        template <typename Tx> struct rebind { typedef FixedPoolAllocator<Tx, alignment, capacity> other; };
        //--

        // manually malloc the pool memory with n objects
        void* malloc(size_t n) noexcept
        {
            size_t bytes = data_size * n;
            this->data = ::operator new(bytes, align_value);

            // assign the free nodes of the pool
            value_type* data_begin = reinterpret_cast<value_type*>(this->data);
            value_type* data_last = data_begin + n - 1;
            next = reinterpret_cast<_::Node*>(data_begin);
            for (value_type* itr = data_begin; itr != data_last; ++itr)
            {
                // O(n) to set next pointers, additional data members for HEAD/CAP could skip this but favoring size for now
                reinterpret_cast<_::Node*>(itr)->next = reinterpret_cast<_::Node*>(itr + 1);
            }
            reinterpret_cast<_::Node*>(data_last)->next = nullptr;

            return this->data;
        }

        // free all memory block allocations
        void free() noexcept
        {
            ::operator delete(this->data, align_value);
        }

        // std compatible
        // allocate one object from the pool, allocate new pool block if nescesary
        inline value_type* allocate(size_t n) noexcept
        {
            assert(n == 1 && "can only support one allocation at a time");
            assert(next && "fixed pool out of memory - most likely will crash");

            _::Node* cur = this->next;
            this->next = cur->next;

            return reinterpret_cast<value_type*>(cur);
        }

        // std compatible
        // deallocate one object from the pool
        inline void deallocate(value_type* p, size_t n) noexcept
        {
            _::Node* next_old = reinterpret_cast<_::Node*>(this->next);
            _::Node* next_new = reinterpret_cast<_::Node*>(p);

            next_new->next = next_old;
            this->next = next_new;
        }
    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    /* Fixed block allocation from separate pool allocators of different subdivisions of block size */
    template<typename T, size_t blockSize>
    struct _BlockAllocatorStorage_Base
    {
        static constexpr size_t block_size = blockSize;
        static constexpr size_t data_size = sizeof(T);

        static constexpr size_t subblock_capacities[22]{
            0x00000010,
            0x00000020,
            0x00000040,
            0x00000080,
            0x00000100,
            0x00000200,
            0x00000400,
            0x00000800,
            0x00001000,
            0x00002000,
            0x00004000,
            0x00008000,
            0x00010000,
            0x00020000,
            0x00040000,
            0x00080000,
            0x00100000,
            0x00200000,
            0x00400000,
            0x00800000,
            0x01000000,
            0x02000000,
        };

        static constexpr size_t subblock_sizes[22] = {
            sizeof(T) * subblock_capacities[0 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[0 ],
            sizeof(T) * subblock_capacities[1 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[1 ],
            sizeof(T) * subblock_capacities[2 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[2 ],
            sizeof(T) * subblock_capacities[3 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[3 ],
            sizeof(T) * subblock_capacities[4 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[4 ],
            sizeof(T) * subblock_capacities[5 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[5 ],
            sizeof(T) * subblock_capacities[6 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[6 ],
            sizeof(T) * subblock_capacities[7 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[7 ],
            sizeof(T) * subblock_capacities[8 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[8 ],
            sizeof(T) * subblock_capacities[9 ] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[9 ],
            sizeof(T) * subblock_capacities[10] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[10],
            sizeof(T) * subblock_capacities[11] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[11],
            sizeof(T) * subblock_capacities[12] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[12],
            sizeof(T) * subblock_capacities[13] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[13],
            sizeof(T) * subblock_capacities[14] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[14],
            sizeof(T) * subblock_capacities[15] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[15],
            sizeof(T) * subblock_capacities[16] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[16],
            sizeof(T) * subblock_capacities[17] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[17],
            sizeof(T) * subblock_capacities[18] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[18],
            sizeof(T) * subblock_capacities[19] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[19],
            sizeof(T) * subblock_capacities[20] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[20],
            sizeof(T) * subblock_capacities[21] > 0x7FFFFFFFF ? 0 : sizeof(T) * subblock_capacities[21],
        };

        static constexpr size_t block_capacities[22] = {
            block_size / subblock_sizes[0 ] ? block_size / subblock_sizes[0 ] : 1,
            block_size / subblock_sizes[1 ] ? block_size / subblock_sizes[1 ] : 1,
            block_size / subblock_sizes[2 ] ? block_size / subblock_sizes[2 ] : 1,
            block_size / subblock_sizes[3 ] ? block_size / subblock_sizes[3 ] : 1,
            block_size / subblock_sizes[4 ] ? block_size / subblock_sizes[4 ] : 1,
            block_size / subblock_sizes[5 ] ? block_size / subblock_sizes[5 ] : 1,
            block_size / subblock_sizes[6 ] ? block_size / subblock_sizes[6 ] : 1,
            block_size / subblock_sizes[7 ] ? block_size / subblock_sizes[7 ] : 1,
            block_size / subblock_sizes[8 ] ? block_size / subblock_sizes[8 ] : 1,
            block_size / subblock_sizes[9 ] ? block_size / subblock_sizes[9 ] : 1,
            block_size / subblock_sizes[10] ? block_size / subblock_sizes[10] : 1,
            block_size / subblock_sizes[11] ? block_size / subblock_sizes[11] : 1,
            block_size / subblock_sizes[12] ? block_size / subblock_sizes[12] : 1,
            block_size / subblock_sizes[13] ? block_size / subblock_sizes[13] : 1,
            block_size / subblock_sizes[14] ? block_size / subblock_sizes[14] : 1,
            block_size / subblock_sizes[15] ? block_size / subblock_sizes[15] : 1,
            block_size / subblock_sizes[16] ? block_size / subblock_sizes[16] : 1,
            block_size / subblock_sizes[17] ? block_size / subblock_sizes[17] : 1,
            block_size / subblock_sizes[18] ? block_size / subblock_sizes[18] : 1,
            block_size / subblock_sizes[19] ? block_size / subblock_sizes[19] : 1,
            block_size / subblock_sizes[20] ? block_size / subblock_sizes[20] : 1,
            block_size / subblock_sizes[21] ? block_size / subblock_sizes[21] : 1,
        };

        struct SubBlock0  { T data[subblock_capacities[0 ]]; };
        struct SubBlock1  { T data[subblock_capacities[1 ]]; };
        struct SubBlock2  { T data[subblock_capacities[2 ]]; };
        struct SubBlock3  { T data[subblock_capacities[3 ]]; };
        struct SubBlock4  { T data[subblock_capacities[4 ]]; };
        struct SubBlock5  { T data[subblock_capacities[5 ]]; };
        struct SubBlock6  { T data[subblock_capacities[6 ]]; };
        struct SubBlock7  { T data[subblock_capacities[7 ]]; };
        struct SubBlock8  { T data[subblock_capacities[8 ]]; };
        struct SubBlock9  { T data[subblock_capacities[9 ]]; };
        struct SubBlock10 { T data[subblock_capacities[10]]; };
        struct SubBlock11 { T data[subblock_capacities[11]]; };
        struct SubBlock12 { T data[subblock_capacities[12]]; };
        struct SubBlock13 { T data[subblock_capacities[13]]; };
        struct SubBlock14 { T data[subblock_capacities[14]]; };
        struct SubBlock15 { T data[subblock_capacities[15]]; };
        struct SubBlock16 { T data[subblock_capacities[16]]; };
        struct SubBlock17 { T data[subblock_capacities[17]]; };
        struct SubBlock18 { T data[subblock_capacities[18]]; };
        struct SubBlock19 { T data[subblock_capacities[19]]; };
        struct SubBlock20 { T data[subblock_capacities[20]]; };
        struct SubBlock21 { T data[subblock_capacities[21]]; };
    };



    template<typename T, size_t blockSize>
    struct _BlockAllocatorStorage_Static : _BlockAllocatorStorage_Base<T, blockSize>
    {
        using Storage_T = _BlockAllocatorStorage_Base<T, blockSize>;

        static StaticPoolAllocator<Storage_T::SubBlock0 , 4096, Storage_T::block_capacities[0 ]> pool_0;
        static StaticPoolAllocator<Storage_T::SubBlock1 , 4096, Storage_T::block_capacities[1 ]> pool_1;
        static StaticPoolAllocator<Storage_T::SubBlock2 , 4096, Storage_T::block_capacities[2 ]> pool_2;
        static StaticPoolAllocator<Storage_T::SubBlock3 , 4096, Storage_T::block_capacities[3 ]> pool_3;
        static StaticPoolAllocator<Storage_T::SubBlock4 , 4096, Storage_T::block_capacities[4 ]> pool_4;
        static StaticPoolAllocator<Storage_T::SubBlock5 , 4096, Storage_T::block_capacities[5 ]> pool_5;
        static StaticPoolAllocator<Storage_T::SubBlock6 , 4096, Storage_T::block_capacities[6 ]> pool_6;
        static StaticPoolAllocator<Storage_T::SubBlock7 , 4096, Storage_T::block_capacities[7 ]> pool_7;
        static StaticPoolAllocator<Storage_T::SubBlock8 , 4096, Storage_T::block_capacities[8 ]> pool_8;
        static StaticPoolAllocator<Storage_T::SubBlock9 , 4096, Storage_T::block_capacities[9 ]> pool_9;
        static StaticPoolAllocator<Storage_T::SubBlock10, 4096, Storage_T::block_capacities[10]> pool_10;
        static StaticPoolAllocator<Storage_T::SubBlock11, 4096, Storage_T::block_capacities[11]> pool_11;
        static StaticPoolAllocator<Storage_T::SubBlock12, 4096, Storage_T::block_capacities[12]> pool_12;
        static StaticPoolAllocator<Storage_T::SubBlock13, 4096, Storage_T::block_capacities[13]> pool_13;
        static StaticPoolAllocator<Storage_T::SubBlock14, 4096, Storage_T::block_capacities[14]> pool_14;
        static StaticPoolAllocator<Storage_T::SubBlock15, 4096, Storage_T::block_capacities[15]> pool_15;
        static StaticPoolAllocator<Storage_T::SubBlock16, 4096, Storage_T::block_capacities[16]> pool_16;
        static StaticPoolAllocator<Storage_T::SubBlock17, 4096, Storage_T::block_capacities[17]> pool_17;
        static StaticPoolAllocator<Storage_T::SubBlock18, 4096, Storage_T::block_capacities[18]> pool_18;
        static StaticPoolAllocator<Storage_T::SubBlock19, 4096, Storage_T::block_capacities[19]> pool_19;
        static StaticPoolAllocator<Storage_T::SubBlock20, 4096, Storage_T::block_capacities[20]> pool_20;
        static StaticPoolAllocator<Storage_T::SubBlock21, 4096, Storage_T::block_capacities[21]> pool_21;
    };

    template<typename T, size_t blockSize>
    struct _BlockAllocatorStorage_Unique : _BlockAllocatorStorage_Base<T, blockSize>
    {
        using Storage_T = _BlockAllocatorStorage_Base<T, blockSize>;

        UniquePoolAllocator<Storage_T::SubBlock0 , 4096, Storage_T::block_capacities[0 ]> pool_0;
        UniquePoolAllocator<Storage_T::SubBlock1 , 4096, Storage_T::block_capacities[1 ]> pool_1;
        UniquePoolAllocator<Storage_T::SubBlock2 , 4096, Storage_T::block_capacities[2 ]> pool_2;
        UniquePoolAllocator<Storage_T::SubBlock3 , 4096, Storage_T::block_capacities[3 ]> pool_3;
        UniquePoolAllocator<Storage_T::SubBlock4 , 4096, Storage_T::block_capacities[4 ]> pool_4;
        UniquePoolAllocator<Storage_T::SubBlock5 , 4096, Storage_T::block_capacities[5 ]> pool_5;
        UniquePoolAllocator<Storage_T::SubBlock6 , 4096, Storage_T::block_capacities[6 ]> pool_6;
        UniquePoolAllocator<Storage_T::SubBlock7 , 4096, Storage_T::block_capacities[7 ]> pool_7;
        UniquePoolAllocator<Storage_T::SubBlock8 , 4096, Storage_T::block_capacities[8 ]> pool_8;
        UniquePoolAllocator<Storage_T::SubBlock9 , 4096, Storage_T::block_capacities[9 ]> pool_9;
        UniquePoolAllocator<Storage_T::SubBlock10, 4096, Storage_T::block_capacities[10]> pool_10;
        UniquePoolAllocator<Storage_T::SubBlock11, 4096, Storage_T::block_capacities[11]> pool_11;
        UniquePoolAllocator<Storage_T::SubBlock12, 4096, Storage_T::block_capacities[12]> pool_12;
        UniquePoolAllocator<Storage_T::SubBlock13, 4096, Storage_T::block_capacities[13]> pool_13;
        UniquePoolAllocator<Storage_T::SubBlock14, 4096, Storage_T::block_capacities[14]> pool_14;
        UniquePoolAllocator<Storage_T::SubBlock15, 4096, Storage_T::block_capacities[15]> pool_15;
        UniquePoolAllocator<Storage_T::SubBlock16, 4096, Storage_T::block_capacities[16]> pool_16;
        UniquePoolAllocator<Storage_T::SubBlock17, 4096, Storage_T::block_capacities[17]> pool_17;
        UniquePoolAllocator<Storage_T::SubBlock18, 4096, Storage_T::block_capacities[18]> pool_18;
        UniquePoolAllocator<Storage_T::SubBlock19, 4096, Storage_T::block_capacities[19]> pool_19;
        UniquePoolAllocator<Storage_T::SubBlock20, 4096, Storage_T::block_capacities[20]> pool_20;
        UniquePoolAllocator<Storage_T::SubBlock21, 4096, Storage_T::block_capacities[21]> pool_21;
    };

    // implementation for block allocator, self handling buffer/pool creation by block size
    template<typename T, size_t blockSize, typename Storage_T>
    struct _BlockAllocatorImplementation : Storage_T
    {
        // std compatible
        T* allocate(size_t n) noexcept
        {
            T* ret;
            
            // find the subblock with smallest capacity for n
            size_t subblock_i = 0;
            while (n > Storage_T::subblock_capacities[subblock_i]) ++subblock_i;

            // return allocation from pool associated with subblock
            switch (subblock_i)
            {
            case 0 : ret = this->pool_0 .allocate(1)->data; break;
            case 1 : ret = this->pool_1 .allocate(1)->data; break;
            case 2 : ret = this->pool_2 .allocate(1)->data; break;
            case 3 : ret = this->pool_3 .allocate(1)->data; break;
            case 4 : ret = this->pool_4 .allocate(1)->data; break;
            case 5 : ret = this->pool_5 .allocate(1)->data; break;
            case 6 : ret = this->pool_6 .allocate(1)->data; break;
            case 7 : ret = this->pool_7 .allocate(1)->data; break;
            case 8 : ret = this->pool_8 .allocate(1)->data; break;
            case 9 : ret = this->pool_9 .allocate(1)->data; break;
            case 10: ret = this->pool_10.allocate(1)->data; break;
            case 11: ret = this->pool_11.allocate(1)->data; break;
            case 12: ret = this->pool_12.allocate(1)->data; break;
            case 13: ret = this->pool_13.allocate(1)->data; break;
            case 14: ret = this->pool_14.allocate(1)->data; break;
            case 15: ret = this->pool_15.allocate(1)->data; break;
            case 16: ret = this->pool_16.allocate(1)->data; break;
            case 17: ret = this->pool_17.allocate(1)->data; break;
            case 18: ret = this->pool_18.allocate(1)->data; break;
            case 19: ret = this->pool_19.allocate(1)->data; break;
            case 20: ret = this->pool_20.allocate(1)->data; break;
            case 21: ret = this->pool_21.allocate(1)->data; break;
            default: ret = nullptr; assert(0);        break;
            }

            return ret;
        }

        // std compatible
        // deallocate subblock object from its pool
        void deallocate(T* p, size_t n) noexcept
        {
            // find the subblock with smallest capacity for n
            size_t subblock_i = 0;
            while (n > Storage_T::subblock_capacities[subblock_i]) ++subblock_i;

            switch (subblock_i)
            {
            case 0 : this->pool_0 .deallocate(reinterpret_cast<typename Storage_T::SubBlock0* >(p), 1); break;
            case 1 : this->pool_1 .deallocate(reinterpret_cast<typename Storage_T::SubBlock1* >(p), 1); break;
            case 2 : this->pool_2 .deallocate(reinterpret_cast<typename Storage_T::SubBlock2* >(p), 1); break;
            case 3 : this->pool_3 .deallocate(reinterpret_cast<typename Storage_T::SubBlock3* >(p), 1); break;
            case 4 : this->pool_4 .deallocate(reinterpret_cast<typename Storage_T::SubBlock4* >(p), 1); break;
            case 5 : this->pool_5 .deallocate(reinterpret_cast<typename Storage_T::SubBlock5* >(p), 1); break;
            case 6 : this->pool_6 .deallocate(reinterpret_cast<typename Storage_T::SubBlock6* >(p), 1); break;
            case 7 : this->pool_7 .deallocate(reinterpret_cast<typename Storage_T::SubBlock7* >(p), 1); break;
            case 8 : this->pool_8 .deallocate(reinterpret_cast<typename Storage_T::SubBlock8* >(p), 1); break;
            case 9 : this->pool_9 .deallocate(reinterpret_cast<typename Storage_T::SubBlock9* >(p), 1); break;
            case 10: this->pool_10.deallocate(reinterpret_cast<typename Storage_T::SubBlock10*>(p), 1); break;
            case 11: this->pool_11.deallocate(reinterpret_cast<typename Storage_T::SubBlock11*>(p), 1); break;
            case 12: this->pool_12.deallocate(reinterpret_cast<typename Storage_T::SubBlock12*>(p), 1); break;
            case 13: this->pool_13.deallocate(reinterpret_cast<typename Storage_T::SubBlock13*>(p), 1); break;
            case 14: this->pool_14.deallocate(reinterpret_cast<typename Storage_T::SubBlock14*>(p), 1); break;
            case 15: this->pool_15.deallocate(reinterpret_cast<typename Storage_T::SubBlock15*>(p), 1); break;
            case 16: this->pool_16.deallocate(reinterpret_cast<typename Storage_T::SubBlock16*>(p), 1); break;
            case 17: this->pool_17.deallocate(reinterpret_cast<typename Storage_T::SubBlock17*>(p), 1); break;
            case 18: this->pool_18.deallocate(reinterpret_cast<typename Storage_T::SubBlock18*>(p), 1); break;
            case 19: this->pool_19.deallocate(reinterpret_cast<typename Storage_T::SubBlock19*>(p), 1); break;
            case 20: this->pool_20.deallocate(reinterpret_cast<typename Storage_T::SubBlock20*>(p), 1); break;
            case 21: this->pool_21.deallocate(reinterpret_cast<typename Storage_T::SubBlock21*>(p), 1); break;
            default: assert(0);                             break;
            }

        }

        // get pool block pointer from subblock - this is the allocation memory pointer
        void* get_subblock_block(T* subblockPtr, size_t subblockCapacity)
        {
            void* ret;
            size_t subblock_i = 0;
            while (subblockCapacity > Storage_T::subblock_capacities[subblock_i]) ++subblock_i;

            switch (subblock_i)
            {
            case 0 : ret = this->pool_0 .find_block(subblockPtr); break;
            case 1 : ret = this->pool_1 .find_block(subblockPtr); break;
            case 2 : ret = this->pool_2 .find_block(subblockPtr); break;
            case 3 : ret = this->pool_3 .find_block(subblockPtr); break;
            case 4 : ret = this->pool_4 .find_block(subblockPtr); break;
            case 5 : ret = this->pool_5 .find_block(subblockPtr); break;
            case 6 : ret = this->pool_6 .find_block(subblockPtr); break;
            case 7 : ret = this->pool_7 .find_block(subblockPtr); break;
            case 8 : ret = this->pool_8 .find_block(subblockPtr); break;
            case 9 : ret = this->pool_9 .find_block(subblockPtr); break;
            case 10: ret = this->pool_10.find_block(subblockPtr); break;
            case 11: ret = this->pool_11.find_block(subblockPtr); break;
            case 12: ret = this->pool_12.find_block(subblockPtr); break;
            case 13: ret = this->pool_13.find_block(subblockPtr); break;
            case 14: ret = this->pool_14.find_block(subblockPtr); break;
            case 15: ret = this->pool_15.find_block(subblockPtr); break;
            case 16: ret = this->pool_16.find_block(subblockPtr); break;
            case 17: ret = this->pool_17.find_block(subblockPtr); break;
            case 18: ret = this->pool_18.find_block(subblockPtr); break;
            case 19: ret = this->pool_19.find_block(subblockPtr); break;
            case 20: ret = this->pool_20.find_block(subblockPtr); break;
            case 21: ret = this->pool_21.find_block(subblockPtr); break;
            default: ret = nullptr; assert(0); break;
            }

            return ret;
        }

    };

    // creates a block allocator, buffers by type, all buffers are global static so alloc object does not need to be stored
    template<typename T, size_t blockSize>
    struct StaticBlockAllocator : _BlockAllocatorImplementation<T, blockSize, _BlockAllocatorStorage_Static<T, blockSize>>
    {
        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        //-- std
        constexpr StaticBlockAllocator() noexcept { }
        constexpr StaticBlockAllocator(const StaticBlockAllocator& rhs) noexcept = default;
        ~StaticBlockAllocator() noexcept { }
        template <class Tx> constexpr StaticBlockAllocator(StaticBlockAllocator<Tx, blockSize> const& rhs) noexcept { }
        template <typename Tx> struct rebind { typedef StaticBlockAllocator<Tx, blockSize> other; };
        //--
    };

    // creates a block allocator, buffers by type, each allocator is a unique object with its own buffers
    template<typename T, size_t blockSize>
    struct UniqueBlockAllocator : _BlockAllocatorImplementation<T, blockSize, _BlockAllocatorStorage_Unique<T, blockSize>>
    {
        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        //-- std
        constexpr UniqueBlockAllocator() noexcept { }
        constexpr UniqueBlockAllocator(const UniqueBlockAllocator& rhs) noexcept = default;
        ~UniqueBlockAllocator() noexcept { }
        template <class Tx> constexpr UniqueBlockAllocator(UniqueBlockAllocator<Tx, blockSize> const& rhs) noexcept { }
        template <typename Tx> struct rebind { typedef UniqueBlockAllocator<Tx, blockSize> other; };
        //--
    };

}

#endif // !CX_ALLOCATOR_H
