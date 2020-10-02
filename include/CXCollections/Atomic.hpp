#ifndef CXATOMIC_H
#define CXATOMIC_H


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
    struct atomic
    {
        static_assert(sizeof(T) <= 4, "true lock-free atomic operations do not work on types larger than 32 bits");
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

    static_assert(sizeof(char)  == 1, "char is not 1 byte for this platform");
    static_assert(sizeof(short) == 2, "short is not 2 bytes for this platform");
    static_assert(sizeof(int)   == 4, "int is not 4 bytes for this platform");

    typedef atomic<bool>           atomic_bool;

    typedef atomic<unsigned char>  atomic_uint8;
    typedef atomic<unsigned short> atomic_uint16;
    typedef atomic<unsigned int>   atomic_uint32;

    typedef atomic<char>  atomic_int8;
    typedef atomic<short> atomic_int16;
    typedef atomic<int>   atomic_int32;

}

#endif // !CXATOMIC_H
