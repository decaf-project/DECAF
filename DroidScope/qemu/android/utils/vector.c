#include <android/utils/vector.h>
#include <limits.h>

extern void
_avector_ensure( void**  items, size_t  itemSize, unsigned*  pMaxItems, unsigned  newCount )
{
    unsigned  oldMax = *pMaxItems;

    if (newCount > oldMax) {
        unsigned  newMax = oldMax;
        unsigned  bigMax = UINT_MAX / itemSize;

        if (itemSize == 0) {
            AASSERT_FAIL("trying to reallocate array of 0-size items (count=%d)\n", newCount);
        }

        if (newCount > bigMax) {
            AASSERT_FAIL("trying to reallocate over-sized array of %d-bytes items (%d > %d)\n",
                         itemSize, newCount, bigMax);
        }

        while (newMax < newCount) {
            unsigned newMax2 = newMax + (newMax >> 1) + 4;
            if (newMax2 < newMax || newMax2 > bigMax)
                newMax2 = bigMax;
            newMax = newMax2;
        }

        *items     = _android_array_realloc( *items, itemSize, newMax );
        *pMaxItems = newMax;
    }
}

