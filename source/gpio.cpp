#include <stdint.h>
#include <string>
#include <stdexcept>
#include <gpiod.h>
#include "gpio.h"

static bool initialized = false;

void gpio_init(struct gpiod_chip **chip, struct gpiod_line **line, uint8_t gpio_pin, std::string chip_name, bool input, std::string line_name)
{
        int result = 0;
        *chip        = gpiod_chip_open_by_name(chip_name.c_str());

        if (NULL == *chip)
        {
             throw std::runtime_error("ERROR: Could not open GPIOD chip");
        }

        *line        = gpiod_chip_get_line(*chip, gpio_pin);

        if (input)
        {
            result = gpiod_line_request_input(*line, line_name.c_str());
        }
        else
        {
            result = gpiod_line_request_output(*line, line_name.c_str(), 0);
        }

        if (result < 0)
        {
            std::string direction = "input";

            if (!input)
            {
                direction = "output";
            }

            throw std::runtime_error("ERROR: Could not set "+line_name+" GPIO as a "+direction+".");
        }

        initialized = true;
}

void gpio_free(struct gpiod_chip **chip, struct gpiod_line **line)
{
    if (initialized)
    {
        gpiod_line_release(*line);
        gpiod_chip_close(*chip);
        initialized = false;
    }
}

bool gpio_read(struct gpiod_line *line)
{
	bool result = true;
        int value = gpiod_line_get_value(line);

        if (0 == value)
        {
            result = false;
        }

	return result;
}

void gpio_write(struct gpiod_line *line, int value)
{
    gpiod_line_set_value(line, value); 
}

