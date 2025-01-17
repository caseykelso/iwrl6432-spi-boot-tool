#pragma once

typedef std::function<uint32_t ()> gpio_callback_t;

typedef struct {
	uint8_t mode;
	uint8_t bits_per_word;
	uint32_t speed;
	uint16_t delay;
	std::string device;
	int file_descriptor;
	gpio_callback_t gpio_callback;
	uint8_t gpio_sleep_ns;
} spi_config_t;

bool spi_init(spi_config_t &spi_config);
void spi_close(spi_config_t &config);
void spiboot(spi_config_t config);
void spi_transfer(uint8_t *tx, uint8_t *rx, uint32_t length, spi_config_t config);
uint8_t reverse_bits(uint8_t n);
