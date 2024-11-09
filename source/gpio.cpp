#include <stdint.h>
#include <string>
#include <stdexcept>
#include <gpiod.h>
#include "gpio.h"

void gpio_init(struct gpiod_chip **chip, struct gpiod_line **line, uint8_t gpio_pin, std::string chip_name)
{
	int result = 0;

    *chip        = gpiod_chip_open_by_name(chip_name.c_str());

	if (NULL == *chip)
	{
		throw std::runtime_error("ERROR: Could not open GPIOD chip");
	}

    *line        = gpiod_chip_get_line(*chip, gpio_pin);
	result = gpiod_line_request_input(*line, "spibusy");

	if (result < 0)
	{
		throw std::runtime_error("ERROR: Could not set SPI_BUSY GPIO as a input.");
	}
}

void gpio_free(struct gpiod_chip **chip, struct gpiod_line **line)
{
	gpiod_line_release(*line);
	gpiod_chip_close(*chip);
}

bool gpio_read(struct gpiod_chip **chip, struct gpiod_line **line)
{
	bool result = false;
	//TODO: implement libgpiod read
	return result;
}

