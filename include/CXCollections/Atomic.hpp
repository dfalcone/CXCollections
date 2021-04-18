#ifndef CXATOMIC_H
#define CXATOMIC_H

#include <stdint.h>

#if !defined(CX_COMPILER_BARRIER)
    #if defined(_WIN32)
        #define VC_EXTRALEAN
        #define WIN32_LEAN_AND_MEAN
        #include <Windows.h>
        #define CX_COMPILER_BARRIER() _ReadWriteBarrier()
    #else
        NOT IMPLEMENTED
    #endif
#endif

namespace CX {
    template<typename T>
    struct alignas(4) atomic
    {
        static_assert(sizeof(T) <= 4, "lock-free atomic operations greater than 32-bits are not gauranteed on all platforms");
    private:
        T m_value;

    public:
        atomic(T v) { m_value = v; }

        inline operator T&() { return reinterpret_cast<T&>(m_value); } // read value

        inline void operator=(const T& rhs) { m_value = rhs; } // write value

        inline void operator+=(const T& rhs) { T v = m_value; CX_COMPILER_BARRIER(); v += rhs; CX_COMPILER_BARRIER(); m_value = v; } // write += value
        inline void operator-=(const T& rhs) { T v = m_value; CX_COMPILER_BARRIER(); v -= rhs; CX_COMPILER_BARRIER(); m_value = v; } // write -= value

        inline void operator++() { T v = m_value; CX_COMPILER_BARRIER(); ++v; CX_COMPILER_BARRIER(); m_value = v; } // write increment
        inline void operator--() { T v = m_value; CX_COMPILER_BARRIER(); --v; CX_COMPILER_BARRIER(); m_value = v; } // write decrement

        inline void operator|=(const T& rhs) { T v = m_value; CX_COMPILER_BARRIER(); v |= rhs; CX_COMPILER_BARRIER(); m_value = v; } // write bitwise |=
        inline void operator&=(const T& rhs) { T v = m_value; CX_COMPILER_BARRIER(); v &= rhs; CX_COMPILER_BARRIER(); m_value = v; } // write bitwise &=
        inline void operator^=(const T& rhs) { T v = m_value; CX_COMPILER_BARRIER(); v ^= rhs; CX_COMPILER_BARRIER(); m_value = v; } // write bitwise ^=
    };

    typedef atomic<bool>            atomic_bool;
    typedef atomic<char>            atomic_char;
    typedef atomic<unsigned char>   atomic_uchar;
    typedef atomic<int>             atomic_int;
    typedef atomic<unsigned int>    atomic_uint;

    typedef atomic<int8_t>          atomic_int8;
    typedef atomic<uint8_t>         atomic_uint8;
    typedef atomic<int16_t>         atomic_int16;
    typedef atomic<uint16_t>        atomic_uint16;
    typedef atomic<int32_t>         atomic_int32;
    typedef atomic<uint32_t>        atomic_uint32;
}

#endif // !CXATOMIC_H
