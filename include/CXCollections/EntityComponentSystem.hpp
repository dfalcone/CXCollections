#ifndef CYBERDECK_ENTITY_COMPONENT_SYSTEM_H
#define CYBERDECK_ENTITY_COMPONENT_SYSTEM_H

#include <vector>
#include <typeinfo>
#include <tuple>
//#include <cstdlib>
#include <new>
#include <bitset>

#include <any>

// entity
// -- component
// -- component

// entities[]
// components[]
// components[]

#define REGISTER_ARCHETYPE_1(Tc1) struct ARCHETYPE_CAST_FROM_MASK ##Tc1 { using type = Archetype<Tc1>;  }
#define REGISTER_ARCHETYPE_2(Tc1, Tc2) struct ARCHETYPE_CAST_FROM_MASK ##Tc1 ##Tc2 { using type = Archetype<Tc1, Tc2>;  }

//#define REGISTER_ARCHETYPE(maskHash, ...) struct ARCHETYPE_CAST_FROM_MASK ##maskHash { using type = Archetype<__VA_ARGS__>;  }
#define ARCHETYPE_CAST(maskHash) ARCHETYPE_CAST_FROM_MASK ##maskHash::type


struct Entity
{
    uint32_t id;
    uint32_t archetypeId;
    uint32_t componentsId;
    uint32_t userData;
};



//template<typename T=void*>
//struct ComponentGroup
//{
//    std::vector<T> components;
//};

//template<typename T1>
//struct Archetype1 { std::vector<T1> components1; };
//
//template<typename T1, typename T2>
//struct Archetype2 { 
//    std::vector<T1> components1; std::vector<T2> components2;
//};

//template<typename Tc>
//using ComponentGroup = std::vector<Tc>;

template<typename Tc>
struct ComponentGroup
{
    template<typename... Ts>
    friend struct Archetype;

    //Tc& Set(int i, Tc& c) noexcept { components[i] = c; return components[i]; }
    //Tc& Get(int i) noexcept { return components[i]; }
    
    //std::vector<Tc> components;
    Tc* components;

protected:
    //friend struct Archetype;
    inline void Grow(size_t cap) {
        Tc* oldData = components;
        components = (Tc*)operator new(sizeof(Tc) * cap*2, std::align_val_t(alignment));
        memcpy(components, oldData, cap);
        operator delete(oldData, std::align_val_t(alignment));
    }

    static constexpr size_t alignment = 4096; // standard cache line size
};

struct ArchetypeBase
{
    template<typename... Ts>
    friend struct Archetype;

    template<typename... Ts>
    void RegisterUpdateCallback() {

    }

    // generic callback - is casted to bitmask type?
    virtual void UpdateEntities(std::bitset<256> mask, void(*callback)(unsigned, ...)) = 0;

    std::bitset<256> componentMask;
};

template<typename... Ts>
struct Archetype : ArchetypeBase
{


    template<typename Tc>
    inline ComponentGroup<Tc>& GetComponentGroup() { return std::get<ComponentGroup<Tc>>(componentGroups); }

    template<typename Tc>
    inline Tc& GetComponent(unsigned entity) { return (Tc&)GetComponentGroup<Tc>().components[entity]; }

    template<typename Tc>
    inline Tc* GetComponentArray() { return std::get<Tc*>(componentsTuple); }

    //template<typename Tcs...>
    //std::tuple<Tcs&...> GetComponents(unsigned entityIndex) {
    //    return std::make_tuple< GetComponentGroup<Tcs>().components[entityIndex], ... >();
    //}

    unsigned CreateEntity() {
        unsigned entity = entityCount;
        ++entityCount;
        if (entityCount == entityCapacity)
            Grow();
        return entity;
    }

    //typedef void (*UpdateEntitiesCallback)(unsigned entity, auto&...);


    //template<typename Tc1>
    //void UpdateEntities(void(*callback)(unsigned, Tc1&)) {
    //    for (unsigned entity = 0; entity < entityCount; ++entity)
    //        callback(entity, GetComponent<Tc1>(entity));
    //}

    //template<typename Tc1, typename Tc2>
    //void UpdateEntities(void(*callback)(unsigned, Tc1&, Tc2&)) {
    //    for (unsigned entity = 0; entity < entityCount; ++entity)
    //        callback(entity, GetComponent<Tc1>(entity), GetComponent<Tc2>(entity));
    //}

    //template<typename Tc1, typename Tc2, typename Tc3>
    //void UpdateEntities(void(*callback)(unsigned, Tc1&, Tc2&, Tc3&)) {
    //    for (unsigned entity = 0; entity < entityCount; ++entity)
    //        callback(entity, GetComponent<Tc1>(entity), GetComponent<Tc2>(entity), GetComponent<Tc3>(entity));
    //}

    //template<typename Tc1, typename Tc2, typename Tc3, typename Tc4>
    //void UpdateEntities(void(*callback)(unsigned, Tc1&, Tc2&, Tc3&, Tc4&)) {
    //    for (unsigned entity = 0; entity < entityCount; ++entity)
    //        callback(entity, GetComponent<Tc1>(entity), GetComponent<Tc2>(entity), GetComponent<Tc3>(entity), GetComponent<Tc4>(entity));
    //}

    template<typename... Tcs>
    void UpdateEntities(void(*callback)(unsigned,Tcs&...)) {
        for (unsigned entity = 0; entity < entityCount; ++entity)
            callback(entity, (GetComponent<Tcs>(entity),...));
            //[](...) {}((callback(entity, GetComponent<Tcs>(entity)), 0)...);
    }

    // generic callback - is casted to bitmask type?
    virtual void UpdateEntities(std::bitset<256> mask, void(*callback)(unsigned, ...)) final {
        if ((this->componentMask & mask) != mask)
            return;
        std::any* cs[2];
        std::any** citr = cs;
    
        for (unsigned i = 0; i < 256; ++i) {
            if (!mask.test(i)) continue;
            //for (unsigned j = 0; j < std::tuple_size<Ts* ...>::value; ++j) {
            //    
            //}
        }
    
    }

    //virtual void UpdateEntities(std::bitset<256> mask, void(*callback)(unsigned, ...)) final {};



private:
    //void SetCapacity(unsigned count) {
    //    // C++17 fold expression
    //    ( GetComponentGroup<Ts>().components.resize(count), ... );

    //    // C++11 expander
    //    //using expander = int[];
    //    //(void)expander { 0, (void(GetComponentGroup<Ts>().components.resize(count)), 0)... };

    //    entityCapacity = count;
    //}

    //void Grow() {
    //    // C++17 fold expression
    //    (GetComponentGroup<Ts>().Grow(entityCapacity), ...);
    //    entityCapacity *= 2;
    //}

    template<typename Tc>
    inline void GrowComponent(size_t capOld) {
        Tc*& dataNew = std::get<Tc*>(componentsTuple);
        Tc* dataOld = dataNew;
        dataNew = (Tc*)operator new(sizeof(Tc) * capOld * 2, std::align_val_t(4096));
        memcpy(dataNew, dataOld, sizeof(Tc) * capOld);
        operator delete(dataOld, std::align_val_t(4096));
    }

    void Grow() {
        unsigned capOld = entityCapacity;
        (GrowComponent<Ts>(capOld), ...);
        entityCapacity = capOld * 2;
    }

private:
    unsigned entityCount = 0;
    unsigned entityCapacity = 64;

    std::tuple< ComponentGroup<Ts>... > componentGroups;

    unsigned* entityId_to_componentsIdx;
    std::tuple< Ts* ... > componentsTuple;
};



struct Database
{
    template<typename Tc>
    unsigned GetComponentId() {
        static const unsigned id = next_component_id++;
        return id;
    }

    template<typename... Tcs>
    std::bitset<256> ComponentMask() {
        std::bitset<256> mask;
        (mask.set(GetComponentId<Tcs>(), true), ...);
        return mask;
    }

    template<typename... Tcs>
    void FilterArchetypes(std::vector<ArchetypeBase*>* dst) {
        auto mask = ComponentMask<Tcs...>();
        dst->clear();
        for (ArchetypeBase* arch : archetypes) {
            if ((arch->componentMask & mask) == mask)
                dst->push_back(arch);
        }
    }


private:
    std::vector<ArchetypeBase*> archetypes;
    unsigned next_component_id = 0;
};



//#define REGISTER_ARCHETYPE(Tc1, Tc2) ARCHETYPE_CAST_FROM_MASK ##Tc1 ##Tc2 
//#define ARCHETYPE_CAST_FROM_MASK_010101010100101 Archetype<int, int>*


struct CyberEntity
{
    unsigned id;

    static std::vector<unsigned> freeIds;
    static std::vector<unsigned> parentIds;
    static std::vector<unsigned> childIds; // only store first child
    static std::vector<unsigned> siblingIds; // only store first sibling


};

struct CyberWindow
{
    struct Rect { int x, y, w, h; };

    int id;
    
    static std::vector<Rect> rectComponents;
    static std::vector<void*> osHandleComponents;
    static std::vector<unsigned> flagsComponents;

};


#endif // !CYBERDECK_ENTITY_COMPONENT_SYSTEM_H
