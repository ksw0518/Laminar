#include "Bit.h"
#include <cstdint>
uint64_t set_occupancy(int index, int bits_in_mask, uint64_t attack_mask)
{
    uint64_t occupancy = 0;

    for (int count = 0; count < bits_in_mask; count++)
    {
        int square = get_ls1b(attack_mask);

        Pop_bit(attack_mask, square);
        if ((index & (1 << count)) != 0)
        {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}
