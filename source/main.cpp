#include <stdint.h>
#include <iostream>
#include <stdexcept>
#include <gpio.h>
#include <functional>
#include <spi.h>

struct        gpiod_chip *chip     = nullptr;
struct        gpiod_line *spi_busy = nullptr;
const uint8_t SPI_BUSY_PIN         = 22;

//callback to get spi busy gpio state, this is important to decouple the spi and gpio implementations
bool gpio_spi_busy()
{
	return gpio_read(&chip, &spi_busy);
}

int main(void)
{
	const std::string gpiod_chip_name("gpiochip0");

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

#ifdef SPI_TEST_PATTERN
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
#else
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

