#include <vector>
#include <stdint.h>
#include <cstring>
#include "crc.h"
#include <iostream>

uint32_t reverse_bits(uint32_t value, const uint32_t bitCount) 
{
    uint32_t result = 0;

    for (uint32_t i = 0; i < bitCount; ++i) 
    {
        result = (result << 1) | (value & 1);
        value >>= 1;
    }

    return result;
}

uint32_t reverse_bytes(const uint32_t bytes)
{
    uint32_t result = 0x0;
    result = (((0xFF000000 & bytes) >> 24)  | ((0x00FF0000 & bytes) >> 8) | ((0x0000FF00 & bytes) << 8) | ((0x000000FF & bytes) << 24));
    return result;
}


uint32_t calculate_crc32(const std::vector<uint32_t>& payload, uint32_t poly = 0x04C11DB7, uint32_t initCRC = 0xFFFFFFFF, uint32_t xorOut = 0xFFFFFFFF, bool reverse = false) 
{
    uint32_t crc                   = initCRC;
    const uint8_t SKIP_CRC_FIELD   = 1;
    const uint8_t INCREMENT_32BITS = 1;

    // Process the payload in 4-byte chunks
    for (size_t i = SKIP_CRC_FIELD; i < payload.size(); i += INCREMENT_32BITS) 
    {
        uint32_t chunk = reverse_bytes(payload[i]);

        if (reverse) 
        {
            chunk = reverse_bits(chunk, 32); // Reverse bits if needed
        }

        // XOR the chunk into the CRC register
        crc ^= chunk;

        // Perform 32 iterations of the CRC algorithm for each bit in the chunk
        for (int j = 0; j < 32; ++j) 
        {
            if (crc & 0x80000000) 
            {
                crc = ((crc << 1) & 0xFFFFFFFF) ^ poly;
            } 
            else 
            {
                crc = (crc << 1) & 0xFFFFFFFF;
            }
        }
    }

    // Reverse bits of the CRC value if needed
    if (reverse) 
    {
        crc = reverse_bits(crc, 32);
    }

    // Apply the XOR-out value
    return (crc ^ xorOut);
}



