#pragma once
struct gpiod_chip;
struct gpiod_line;

void gpio_init(struct gpiod_chip **chip, struct gpiod_line **line, uint8_t gpio_pin, std::string chip_name);
void gpio_free(struct gpiod_chip **chip, struct gpiod_line **line);

