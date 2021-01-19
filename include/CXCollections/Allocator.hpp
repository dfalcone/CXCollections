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


    // preallocates a large block of memory from which to allocate from
    // operates using the free-list technique, using free object memory to store a link to the next free item
    // allocates new aligned block when capacity reached, blocks are kept as a linked list
    // deallocates blocks when they become empty
    // DO NOT USE WITH std::vector, WARNING: std::list allocates on constructor
    template<typename T, size_t alignment = 64, size_t capacity = 0x10000 /*64kb*/ / sizeof(T) >
    struct PoolAllocator
    {
        static_assert(capacity != 0, "type too large for default block capacity"); // division by sizeof(T) is fraction

        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        struct Node { Node* next; };
        //static_assert(sizeof(T) >= sizeof(Node), "value_type must be at least 8 bytes");
        struct Block { Block* prev; Block* next; };
        
        static constexpr std::align_val_t align_value = static_cast<std::align_val_t>(alignment);
        static constexpr size_t block_capacity = capacity;
        static constexpr size_t data_size = sizeof(value_type) < sizeof(Node) ? sizeof(Node) : sizeof(value_type);

        //-- data members
        void* data = nullptr;
        Node* next = nullptr;
        //-- 16 bytes

        //-- std
        constexpr PoolAllocator() noexcept { }; // hack - list does 1 allocation on construct
        constexpr PoolAllocator(const PoolAllocator&) noexcept = default;
        ~PoolAllocator() noexcept { };
        template <class Tx> constexpr PoolAllocator(PoolAllocator<Tx, alignment, capacity> const&) noexcept {  }
        template <typename Tx> struct rebind { typedef PoolAllocator<Tx, alignment, capacity> other; };
        //--

        // manually malloc the pool memory with n objects
        void* malloc(size_t n) noexcept
        {
            // sneakily store block link data in heap alloc
            size_t bytes = sizeof(Block) + data_size * n;
            data = ::operator new(bytes, align_value);
            
            Block* block = reinterpret_cast<Block*>(data);
            block->prev = nullptr;
            block->next = nullptr;

            // assign the free nodes of the pool
            value_type* data_begin = reinterpret_cast<value_type*>(block + 1);
            value_type* data_last = data_begin + n - 1;
            next = reinterpret_cast<Node*>(data_begin);
            for (value_type* itr = data_begin; itr != data_last; ++itr)
            {
                // O(n) to set next pointers, additional data members for HEAD/CAP could skip this but favoring size for now
                reinterpret_cast<Node*>(itr)->next = reinterpret_cast<Node*>(itr + 1);
            }
            reinterpret_cast<Node*>(data_last)->next = nullptr;

            return data;
        }

        // manually free the current pool block memory
        void free(void* block_data) noexcept
        {
            assert(block_data);
            Block* block_del = reinterpret_cast<Block*>(block_data);
            Block* block_prev = block_del->prev;
            Block* block_next = block_del->next;

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
        inline value_type* allocate(size_t n) noexcept
        {
            assert(n == 1 && "can only support one allocation at a time");
                        
            // skip if can't grow TODO (extra template param to disable)
            if (next == nullptr)
            {
                // allocate next block
                Block* block_old = reinterpret_cast<Block*>(data);
                this->malloc(capacity);
                Block* block_new = reinterpret_cast<Block*>(data);
                block_new->prev = block_old;
                if (block_old)
                {
                    block_old->next = block_new;
                }
            }

            Node* cur = next;
            next = cur->next;

            return reinterpret_cast<value_type*>(cur);
        }

        // std compatible
        // deallocate one object from the pool
        inline void deallocate(value_type* p, size_t n) noexcept
        {
            Node* next_old = reinterpret_cast<Node*>(next);
            Node* next_new = reinterpret_cast<Node*>(p);

            next_new->next = next_old;
            next = next_new;
        }

        // TODO: iterator / for loop

        // find block from data
        Block* find_block(value_type* p)
        {
            uintptr_t bgn_val, end_val;
            uintptr_t p_val = uintptr_t(p);

            Block* block = reinterpret_cast<Block*>(data);
            assert(block);

            while (block->prev != nullptr)
            {
                bgn_val = uintptr_t(block);
                end_val = uintptr_t( reinterpret_cast<uint8_t*>(block) + (sizeof(Block) * data_size * capacity) );

                if (bgn_val < p_val && p_val < end_val)
                    return block;

                block = block->prev;
            }
            
            return nullptr;
        }
    };

    struct FixedPoolAllocatorWrapper
    {
        struct Node { Node* next; };

        //-- data members
        void* data = nullptr;
        Node* next = nullptr;
        //-- 

        // std compatible
        // allocate one object from the pool, allocate new pool block if nescesary
        inline void* allocate(size_t n) noexcept
        {
            assert(n == 1 && "can only support one allocation at a time");
            assert(next && "fixed pool out of memory - most likely will crash");

            Node* cur = next;
            next = cur->next;

            return reinterpret_cast<void*>(cur);
        }

        // std compatible
        // deallocate one object from the pool
        inline void deallocate(void* p, size_t n) noexcept
        {
            Node* next_old = reinterpret_cast<Node*>(next);
            Node* next_new = reinterpret_cast<Node*>(p);

            next_new->next = next_old;
            next = next_new;
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

        struct Node { Node* next; };

        static constexpr std::align_val_t align_value = static_cast<std::align_val_t>(alignment);
        static constexpr size_t block_capacity = capacity;
        static constexpr size_t data_size = sizeof(value_type) < sizeof(Node) ? sizeof(Node) : sizeof(value_type);

        //-- data members
        void* data = nullptr;
        Node* next = nullptr;
        //-- 

        //-- std
        constexpr FixedPoolAllocator() noexcept { this->malloc(capacity); }; // list does 1 allocation on construct
        constexpr FixedPoolAllocator(const FixedPoolAllocator& rhs) noexcept { data = rhs.data; next = rhs.next; };
        ~FixedPoolAllocator() noexcept { /*free(); data = nullptr; next = nullptr;*/ }
        template <class Tx> constexpr FixedPoolAllocator(FixedPoolAllocator<Tx, alignment, capacity> const& rhs) noexcept { static_assert(typeid(Tx) == typeid(T), "");  data = rhs.data; next = rhs.next; }
        template <typename Tx> struct rebind { typedef FixedPoolAllocator<Tx, alignment, capacity> other; };
        //--

        // manually malloc the pool memory with n objects
        void* malloc(size_t n) noexcept
        {
            size_t bytes = data_size * n;
            data = ::operator new(bytes, align_value);

            // assign the free nodes of the pool
            value_type* data_begin = reinterpret_cast<value_type*>(data);
            value_type* data_last = data_begin + n - 1;
            next = reinterpret_cast<Node*>(data_begin);
            for (value_type* itr = data_begin; itr != data_last; ++itr)
            {
                // O(n) to set next pointers, additional data members for HEAD/CAP could skip this but favoring size for now
                reinterpret_cast<Node*>(itr)->next = reinterpret_cast<Node*>(itr + 1);
            }
            reinterpret_cast<Node*>(data_last)->next = nullptr;

            return data;
        }

        // free all memory block allocations
        void free() noexcept
        {
            ::operator delete(data, align_value);
        }

        // std compatible
        // allocate one object from the pool, allocate new pool block if nescesary
        inline value_type* allocate(size_t n) noexcept
        {
            assert(n == 1 && "can only support one allocation at a time");
            assert(next && "fixed pool out of memory - most likely will crash");

            Node* cur = next;
            next = cur->next;

            return reinterpret_cast<value_type*>(cur);
        }

        // std compatible
        // deallocate one object from the pool
        inline void deallocate(value_type* p, size_t n) noexcept
        {
            Node* next_old = reinterpret_cast<Node*>(next);
            Node* next_new = reinterpret_cast<Node*>(p);

            next_new->next = next_old;
            next = next_new;
        }
    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    /* Fixed block allocation in multiple block sizes */

    template<typename T, size_t blockSize>
    struct BlockAllocator
    {
        //-- std
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        //--

        static constexpr size_t block_size = blockSize;
        static constexpr size_t data_size = sizeof(value_type);

        static constexpr size_t subblock_capacities[20]{
            0x000010,
            0x000020,
            0x000040,
            0x000080,
            0x000100,
            0x000200,
            0x000400,
            0x000800,
            0x001000,
            0x002000,
            0x004000,
            0x008000,
            0x010000,
            0x020000,
            0x040000,
            0x080000,
            0x100000,
            0x200000,
            0x400000,
            0x800000,
        };

        static constexpr size_t subblock_sizes[20] = {
            sizeof(T) * subblock_capacities[0 ],
            sizeof(T) * subblock_capacities[1 ],
            sizeof(T) * subblock_capacities[2 ],
            sizeof(T) * subblock_capacities[3 ],
            sizeof(T) * subblock_capacities[4 ],
            sizeof(T) * subblock_capacities[5 ],
            sizeof(T) * subblock_capacities[6 ],
            sizeof(T) * subblock_capacities[7 ],
            sizeof(T) * subblock_capacities[8 ],
            sizeof(T) * subblock_capacities[9 ],
            sizeof(T) * subblock_capacities[10],
            sizeof(T) * subblock_capacities[11],
            sizeof(T) * subblock_capacities[12],
            sizeof(T) * subblock_capacities[13],
            sizeof(T) * subblock_capacities[14],
            sizeof(T) * subblock_capacities[15],
            sizeof(T) * subblock_capacities[16],
            sizeof(T) * subblock_capacities[17],
            sizeof(T) * subblock_capacities[18],
            sizeof(T) * subblock_capacities[19],
        };

        static constexpr size_t block_capacities[20] = {
            block_size / subblock_sizes[0 ],
            block_size / subblock_sizes[1 ],
            block_size / subblock_sizes[2 ],
            block_size / subblock_sizes[3 ],
            block_size / subblock_sizes[4 ],
            block_size / subblock_sizes[5 ],
            block_size / subblock_sizes[6 ],
            block_size / subblock_sizes[7 ],
            block_size / subblock_sizes[8 ],
            block_size / subblock_sizes[9 ],
            block_size / subblock_sizes[10],
            block_size / subblock_sizes[11],
            block_size / subblock_sizes[12],
            block_size / subblock_sizes[13],
            block_size / subblock_sizes[14],
            block_size / subblock_sizes[15],
            block_size / subblock_sizes[16],
            block_size / subblock_sizes[17],
            block_size / subblock_sizes[18],
            block_size / subblock_sizes[19],
        };

        //-- static pools of fixed size blocks
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

        static PoolAllocator<SubBlock0 , 4096, block_capacities[0 ]> pool_0 ;
        static PoolAllocator<SubBlock1 , 4096, block_capacities[1 ]> pool_1 ;
        static PoolAllocator<SubBlock2 , 4096, block_capacities[2 ]> pool_2 ;
        static PoolAllocator<SubBlock3 , 4096, block_capacities[3 ]> pool_3 ;
        static PoolAllocator<SubBlock4 , 4096, block_capacities[4 ]> pool_4 ;
        static PoolAllocator<SubBlock5 , 4096, block_capacities[5 ]> pool_5 ;
        static PoolAllocator<SubBlock6 , 4096, block_capacities[6 ]> pool_6 ;
        static PoolAllocator<SubBlock7 , 4096, block_capacities[7 ]> pool_7 ;
        static PoolAllocator<SubBlock8 , 4096, block_capacities[8 ]> pool_8 ;
        static PoolAllocator<SubBlock9 , 4096, block_capacities[9 ]> pool_9 ;
        static PoolAllocator<SubBlock10, 4096, block_capacities[10]> pool_10;
        static PoolAllocator<SubBlock11, 4096, block_capacities[11]> pool_11;
        static PoolAllocator<SubBlock12, 4096, block_capacities[12]> pool_12;
        static PoolAllocator<SubBlock13, 4096, block_capacities[13]> pool_13;
        static PoolAllocator<SubBlock14, 4096, block_capacities[14]> pool_14;
        static PoolAllocator<SubBlock15, 4096, block_capacities[15]> pool_15;
        static PoolAllocator<SubBlock15, 4096, block_capacities[16]> pool_16;
        static PoolAllocator<SubBlock15, 4096, block_capacities[17]> pool_17;
        static PoolAllocator<SubBlock15, 4096, block_capacities[18]> pool_18;
        static PoolAllocator<SubBlock15, 4096, block_capacities[19]> pool_19;
        //--

        //-- std
        constexpr BlockAllocator() noexcept { } // list does 1 allocation on construct
        constexpr BlockAllocator(const BlockAllocator& rhs) noexcept = default;
        ~BlockAllocator() noexcept { }
        template <class Tx> constexpr BlockAllocator(BlockAllocator<Tx, blockSize> const& rhs) noexcept { }
        template <typename Tx> struct rebind { typedef BlockAllocator<Tx, blockSize> other; };
        //--

        // std compatible
        value_type* allocate(size_t n) noexcept
        {
            value_type* ret;
            
            // find the subblock with smallest capacity for n
            size_t subblock_i = 0;
            while (n > subblock_capacities[subblock_i]) ++subblock_i;

            // return allocation from pool associated with subblock
            switch (subblock_i)
            {
            case 0 : ret = pool_0 .allocate(1)->data; break;
            case 1 : ret = pool_1 .allocate(1)->data; break;
            case 2 : ret = pool_2 .allocate(1)->data; break;
            case 3 : ret = pool_3 .allocate(1)->data; break;
            case 4 : ret = pool_4 .allocate(1)->data; break;
            case 5 : ret = pool_5 .allocate(1)->data; break;
            case 6 : ret = pool_6 .allocate(1)->data; break;
            case 7 : ret = pool_7 .allocate(1)->data; break;
            case 8 : ret = pool_8 .allocate(1)->data; break;
            case 9 : ret = pool_9 .allocate(1)->data; break;
            case 10: ret = pool_10.allocate(1)->data; break;
            case 11: ret = pool_11.allocate(1)->data; break;
            case 12: ret = pool_12.allocate(1)->data; break;
            case 13: ret = pool_13.allocate(1)->data; break;
            case 14: ret = pool_14.allocate(1)->data; break;
            case 15: ret = pool_15.allocate(1)->data; break;
            case 16: ret = pool_16.allocate(1)->data; break;
            case 17: ret = pool_17.allocate(1)->data; break;
            case 18: ret = pool_18.allocate(1)->data; break;
            case 19: ret = pool_19.allocate(1)->data; break;
            default: ret = nullptr; assert(0);        break;
            }

            return ret;
        }

        // std compatible
        // deallocate subblock object from its pool
        void deallocate(value_type* p, size_t n) noexcept
        {
            // find the subblock with smallest capacity for n
            size_t subblock_i = 0;
            while (n > subblock_capacities[subblock_i]) ++subblock_i;

            switch (subblock_i)
            {
            case 0 : pool_0 .deallocate((SubBlock0 *)p, 1); break;
            case 1 : pool_1 .deallocate((SubBlock1 *)p, 1); break;
            case 2 : pool_2 .deallocate((SubBlock2 *)p, 1); break;
            case 3 : pool_3 .deallocate((SubBlock3 *)p, 1); break;
            case 4 : pool_4 .deallocate((SubBlock4 *)p, 1); break;
            case 5 : pool_5 .deallocate((SubBlock5 *)p, 1); break;
            case 6 : pool_6 .deallocate((SubBlock6 *)p, 1); break;
            case 7 : pool_7 .deallocate((SubBlock7 *)p, 1); break;
            case 8 : pool_8 .deallocate((SubBlock8 *)p, 1); break;
            case 9 : pool_9 .deallocate((SubBlock9 *)p, 1); break;
            case 10: pool_10.deallocate((SubBlock10*)p, 1); break;
            case 11: pool_11.deallocate((SubBlock11*)p, 1); break;
            case 12: pool_12.deallocate((SubBlock12*)p, 1); break;
            case 13: pool_13.deallocate((SubBlock13*)p, 1); break;
            case 14: pool_14.deallocate((SubBlock14*)p, 1); break;
            case 15: pool_15.deallocate((SubBlock15*)p, 1); break;
            case 16: pool_16.deallocate((SubBlock16*)p, 1); break;
            case 17: pool_17.deallocate((SubBlock17*)p, 1); break;
            case 18: pool_18.deallocate((SubBlock18*)p, 1); break;
            case 19: pool_19.deallocate((SubBlock19*)p, 1); break;
            default: assert(0);                             break;
            }

        }

        // get pool block pointer from subblock - this is the allocation memory pointer
        void* get_subblock_block(value_type* subblockPtr, size_t subblockCapacity)
        {
            void* ret;
            size_t subblock_i = 0;
            while (subblockCapacity > subblock_capacities[subblock_i]) ++subblock_i;

            switch (subblock_i)
            {
            case 0 : ret = pool_0 .find_block(subblockPtr); break;
            case 1 : ret = pool_1 .find_block(subblockPtr); break;
            case 2 : ret = pool_2 .find_block(subblockPtr); break;
            case 3 : ret = pool_3 .find_block(subblockPtr); break;
            case 4 : ret = pool_4 .find_block(subblockPtr); break;
            case 5 : ret = pool_5 .find_block(subblockPtr); break;
            case 6 : ret = pool_6 .find_block(subblockPtr); break;
            case 7 : ret = pool_7 .find_block(subblockPtr); break;
            case 8 : ret = pool_8 .find_block(subblockPtr); break;
            case 9 : ret = pool_9 .find_block(subblockPtr); break;
            case 10: ret = pool_10.find_block(subblockPtr); break;
            case 11: ret = pool_11.find_block(subblockPtr); break;
            case 12: ret = pool_12.find_block(subblockPtr); break;
            case 13: ret = pool_13.find_block(subblockPtr); break;
            case 14: ret = pool_14.find_block(subblockPtr); break;
            case 15: ret = pool_15.find_block(subblockPtr); break;
            case 16: ret = pool_16.find_block(subblockPtr); break;
            case 17: ret = pool_17.find_block(subblockPtr); break;
            case 18: ret = pool_18.find_block(subblockPtr); break;
            case 19: ret = pool_19.find_block(subblockPtr); break;
            default: ret = nullptr; assert(0); break;
            }

            return ret;
        }

    };

    // creates separate allocations/instances per define - not global
    template<typename T, size_t blockSize>
    struct UnqiueBlockAllocator
    {

    };

}

#endif // !CX_ALLOCATOR_H
