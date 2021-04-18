#include <stdint.h>

namespace cyber
{



template<typename allocator_t>
struct StrideVector
{
    using value_type = uint8_t;

    value_type* data;
    uint64_t count : 28;
    uint64_t capacity : 28;
    uint64_t stride : 8;

};


}