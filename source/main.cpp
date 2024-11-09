#include <stdint.h>
#include <iostream>
#include <stdexcept>
#include <gpio.h>
#include <functional>
#include <spi.h>

struct gpiod_chip *chip;
struct gpiod_line *spi_busy;
const uint8_t SPI_BUSY_PIN = 22;

//callback to get spi busy gpio state
bool gpio_spi_busy()
{
	return gpio_read(&chip, &spi_busy);
}

int main(void)
{
	int          exit_code     = 0;
	spi_config_t spi_config    = {};
	spi_config.mode            = 0;
	spi_config.speed           = 500000;
	spi_config.bits_per_word   = 8;
	spi_config.device          = "/dev/spidev0.1";
	spi_config.file_descriptor = 0;
	spi_config.delay           = 10; //10 microseconds
	spi_config.gpio_callback   = gpio_spi_busy;
	spi_config.gpio_sleep_ms   = 10;
	const std::string gpiod_chip_name("gpiochip0");

	try 
	{
	   spi_init(spi_config);
	}
	catch(std::exception &e)
	{
		exit_code = -1;
		std::cerr << e.what() << std::endl;
		return(exit_code);
	}

	try 
	{
	   gpio_init(&chip, &spi_busy, SPI_BUSY_PIN, gpiod_chip_name);
	} 
	catch(std::exception &e)
	{
        spi_close(spi_config);
		std::cerr << e.what() << std::endl;
		exit_code = -1;
		return(exit_code);
	}

#if 0
	uint8_t tx[100];

	for (uint8_t i = 0; i < 100; ++i)
	{
		tx[i] = (uint8_t)0xab;
	}

	uint8_t rx[100];
	while(true)
	{
	    spi_transfer(tx, rx, 8, spi_config);
	}
#endif

#if 1
	try 
	{
	    spiboot(spi_config);
	}
	catch(std::exception &e)
	{
		exit_code = -1;
		std::cerr << e.what() << std::endl;
	}
#endif

	spi_close(spi_config);
	gpio_free(&chip, &spi_busy);
	return exit_code;

}

