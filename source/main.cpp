#include <stdint.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <signal.h>
#include <gpio.h>
#include <functional>
#include <spi.h>

#define SPI_TEST_PATTERN 1
struct        gpiod_chip *chip         = nullptr;
struct        gpiod_line *spi_busy     = nullptr;
struct        gpiod_line *sensor_reset = nullptr;
const uint8_t SPI_BUSY_PIN             = 73; //make runtime configurable - RPI 22
const uint8_t IWRL6432_RESET_PIN       = 75;
const int     IWRL6432_RESET_ACTIVE    = 0;
const int     IWRL6432_RESET_INACTIVE  = 1;
spi_config_t  spi_config               = {};


//callback to get spi busy gpio state, this is important to decouple the spi and gpio implementations
bool gpio_spi_busy()
{
        return gpio_read(spi_busy);
}

void release_resources()
{
    spi_close(spi_config);

    if (nullptr != spi_busy)
    {
       gpio_free(&chip, &spi_busy);
    }

    if (nullptr != sensor_reset)
    {
        gpio_free(&chip, &sensor_reset);
    }
}

void siginthandler(int param)
{
    std::cerr << "user forced program to quit" << std::endl;
    release_resources();
    exit(1);
}


int main(void)
{
	const std::string gpiod_chip_name("gpiochip0");
        signal(SIGINT, siginthandler);

	int          exit_code     = 0;
	spi_config.mode            = 0;
	spi_config.speed           = 10;
	spi_config.bits_per_word   = 8;
	spi_config.device          = "/dev/spidev0.0"; // make runtime configurable
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
	   gpio_init(&chip, &spi_busy, SPI_BUSY_PIN, gpiod_chip_name, true, "spi_busy");
           gpio_init(&chip, &sensor_reset, IWRL6432_RESET_PIN, gpiod_chip_name, false, "sensor_reset");
	} 
	catch(std::exception &e)
	{
            spi_close(spi_config);
            std::cerr << e.what() << std::endl;
            exit_code = -1;
            return(exit_code);
	}

        // reset the IWRL6432
        gpio_write(sensor_reset, IWRL6432_RESET_ACTIVE);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        gpio_write(sensor_reset, IWRL6432_RESET_INACTIVE);

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

        release_resources();
	return exit_code;
}

