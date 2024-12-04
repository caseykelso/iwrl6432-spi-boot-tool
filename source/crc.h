#pragma once
uint32_t calculate_crc32(const std::vector<uint32_t>& payload, uint32_t poly, uint32_t initCRC, uint32_t xorOut, bool reverse);
//uint32_t calculate_crc32_uint8_t_array(const uint8_t payload[], const uint32_t payload_size, uint32_t poly, uint32_t initCRC, uint32_t xorOut, bool reverse);
uint32_t reverse_bits(uint32_t value, const uint32_t bitCount);
uint32_t reverse_bytes(const uint32_t bytes);
uint32_t reverse_bytes_upper_16bits(const uint32_t bytes);
uint32_t reverse_bytes_lower_16bits(const uint32_t bytes);
uint32_t double_reversal(const uint32_t bytes);
uint32_t reverse_16bits(const uint32_t bytes);
uint16_t reverse_bytes(const uint16_t bytes);
