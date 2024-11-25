#pragma once
uint32_t calculate_crc32(const std::vector<uint32_t>& payload, uint32_t poly, uint32_t initCRC, uint32_t xorOut, bool reverse);
uint32_t reverse_bits(uint32_t value, const uint32_t bitCount);
uint32_t reverse_bytes(const uint32_t bytes);

