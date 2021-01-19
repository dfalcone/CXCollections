#include "Allocator.hpp"
#include "EntityComponentSystem.hpp"

#include <list>
#include <vector>
#include <stdint.h>



void poolalloctest()
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

    constexpr int s1 = sizeof(cyber_vec_s16);
    constexpr int s2 = sizeof(cyber_list_s16);
    constexpr int s3 = sizeof(std::list<int>);
}

struct C1 { int x; };
struct C2 { int x, y; };
//REGISTER_ARCHETYPE(0, C1, C2);

void UpdateEntityCallback(unsigned entity, C1& c1, C2& c2)
{
    
}

void ecstest()
{

    //Archetype t;
    //t.AddComponentType<int>();
    //t.componentGroups[0].components;

    //typeid(int).name();
    //typeid(int).hash_code();
    //typeid(int).operator==;
    //const type_info intTypeInfo(int);
    //if (typeid(int).operator==(intTypeInfo)) {}

    //Archetype<int> t1;
    //t1.components1;
    //Archetype<int, int> t2;


    Archetype<C1, C2> ts;
    ts.GetComponentGroup<C1>();
    ts.GetComponentGroup<C2>();
    //ts.name;
    
    ts.GetComponentArray<C2>();

    unsigned ent = ts.CreateEntity();

    ts.GetComponent<C1>(ent);
    ts.GetComponent<C2>(ent);

    ts.UpdateEntities(UpdateEntityCallback);

    typedef void(*Callback)(unsigned, C1&, C2&);
    ts.UpdateEntities((Callback)[](unsigned e, C1& c1, C2& c2) { c1.x = c2.x; });


    Database db;
    auto idc1 = db.GetComponentId<C1>();
    auto mask = db.ComponentMask<C1, C2>();

    std::vector<ArchetypeBase*> filter;
    db.FilterArchetypes<C1, C2>(&filter);

    ArchetypeBase* pbase = &ts;
    //reinterpret_cast<ARCHETYPE_CAST(0)*>(pbase)->UpdateEntities(UpdateEntityCallback);
}


int main()
{
    ecstest();

    return 0;
}