#pragma once

#include <cstdint>
#include <array>

template <size_t SpecificityDepth>
struct Specificity
{
    std::array<uint8_t, SpecificityDepth> specificity;

    //TODO: maybe cast the whole array to uint64_t to compare?
    bool operator< (const Specificity& other) const
    {
        for(size_t x = 0; x < SpecificityDepth; x++)
        {
            if(specificity[x] < other.specificity[x])
            {
                return true;
            }
            else if(specificity[x] > other.specificity[x])
            {
                return false;
            }
        }

        return false;
    }

    bool operator> (const Specificity& other) const
    {
        for(size_t x = 0; x < SpecificityDepth; x++)
        {
            if(specificity[x] > other.specificity[x])
            {
                return true;
            }
            else if(specificity[x] < other.specificity[x])
            {
                return false;
            }
        }

        return false;
    }

    bool operator==(const Specificity& other) const
    {
        for(size_t x = 0; x < SpecificityDepth; x++)
        {
            if(specificity[x] != other.specificity[x])
            {
                return false;
            }
        }

        return true;
    }
};