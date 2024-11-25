#pragma once
uint32_t calculateCRC32(const std::vector<uint32_t>& payload, uint32_t poly, uint32_t initCRC, uint32_t xorOut, bool reverse);
uint32_t reverseBits(uint32_t value, uint32_t bitCount);

