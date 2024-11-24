#include <vector>
#include <stdint.h>
#include <cstring>
#include "crc.h"
#include <iostream>

// Function to reverse bits in an integer (similar to the Python version)
uint32_t reverseBits(uint32_t value, uint32_t bitCount) 
{
    uint32_t result = 0;
    for (uint32_t i = 0; i < bitCount; ++i) {
        result = (result << 1) | (value & 1);
        value >>= 1;
    }
    return result;
}

// Function to calculate CRC32 in the same manner as the Python implementation
uint32_t calculateCRC32(const std::vector<uint8_t>& payload, uint32_t poly = 0x04C11DB7, uint32_t initCRC = 0xFFFFFFFF, uint32_t xorOut = 0xFFFFFFFF, bool reverse = false) 
{
    uint32_t crc = initCRC;

    // Process the payload in 4-byte chunks
    for (size_t i = 0; i < payload.size(); i += 4) {
        // Extract a 4-byte chunk
        uint32_t chunk = 0;
        std::vector<uint8_t> chunkBytes(4, 0);
        std::memcpy(&chunkBytes[0], &payload[i], std::min<size_t>(4, payload.size() - i));
        chunk = (chunkBytes[0] << 24) | (chunkBytes[1] << 16) | (chunkBytes[2] << 8) | chunkBytes[3];

        if (reverse) {
            chunk = reverseBits(chunk, 32); // Reverse bits if needed
        }

        // XOR the chunk into the CRC register
        crc ^= chunk;

        // Perform 32 iterations of the CRC algorithm for each bit in the chunk
        for (int j = 0; j < 32; ++j) {
            if (crc & 0x80000000) {
                crc = ((crc << 1) & 0xFFFFFFFF) ^ poly;
            } else {
                crc = (crc << 1) & 0xFFFFFFFF;
            }
        }
    }

    // Reverse bits of the CRC value if needed
    if (reverse) {
        crc = reverseBits(crc, 32);
    }

    // Apply the XOR-out value
    return crc ^ xorOut;
}



