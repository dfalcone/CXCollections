#include "Allocator.hpp"

#include <list>
#include <vector>
#include <stdint.h>

int main()
{

    struct S16
    {
        float m[8];
    };

    std::vector<S16> std_vec_s16;
    std::vector<S16, cyber::Allocator<S16, 128>> cyber_vec_s16;

    auto* a = &std_vec_s16.emplace_back();
    auto* b = &cyber_vec_s16.emplace_back();

    size_t a_ptr = (uintptr_t)a;
    size_t b_ptr = (uintptr_t)b;

    bool is_a_aligned = (a_ptr & 127ull) == 0;
    bool is_b_aligned = (b_ptr & 127ull) == 0;

    printf("a_ptr: %zu isAligned: %i\n", a_ptr, is_a_aligned);
    printf("b_ptr: %zu isAligned: %i\n", b_ptr, is_b_aligned);
    
    std::list<S16, cyber::PoolAllocator<S16>> cyber_list_s16;
    //auto* c = &cyber_list_s16.emplace_back();

    int s1 = sizeof(cyber_vec_s16);
    //int s2 = sizeof(cyber_list_s16);
    int s3 = sizeof(std::list<int>);

    return 0;
}