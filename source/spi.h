#pragma once

typedef struct {
	uint8_t mode;
	uint8_t bits_per_word;
	uint32_t speed;
	uint16_t delay;
	std::string device;
	int file_descriptor;
} spi_config_t;

bool spi_init(spi_config_t &spi_config);
void spi_close(spi_config_t &config);
void transfer(uint8_t const *tx, uint8_t const *rx, uint32_t length, spi_config_t config);


