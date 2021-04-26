#pragma once
#include <stdint.h>
#include <utility>
namespace cyber
{
    // TODO: predicate

    struct QuickSort
    {
        template<typename T>
        static void Sort(T* items, T* begin, T* end)
        {
            if (begin < end)
            {
                T* p = Partition(begin, end);
                Sort(items, begin, p - 1);
                Sort(items, p + 1, end);
            }
        }

        template<typename T, size_t itemsCount>
        static void Sort(T(&items)[itemsCount])
        {
            Sort(items, items, items + itemsCount - 1);
        }

    private:
        template<typename T>
        static T* Partition(T* begin, T* end)
        {
            // choose middle pivot
            T* pivot = begin + (end-begin) / 2u;
            T* swap = begin, *itr = begin, *tmp;
            uint8_t tmpBytes[sizeof(T)];
            tmp = new(tmpBytes)T();

            for (; itr < end; ++itr)
            {
                if (*itr < *pivot)
                {
                    *tmp = *swap;
                    *swap = *itr;
                    *itr = *tmp;
                    ++swap;
                }
            }

            *tmp = *swap;
            *swap = *end;
            *end = *tmp;

            return swap;
        }

    };

}