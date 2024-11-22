//#define CALC32_DUMMY 1
//#define SPI_TEST_PATTERN 1


#include <stdint.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>
#include <signal.h>
#include <gpio.h>
#include <functional>
#include <spi.h>
#include <zlib.h>
#ifdef CALC32_DUMMY
#include "crc32.hpp"
#include "crc32c/crc32c.h"
#endif //CALC32_DUMMY
extern "C" {
#include "crc.h"
}

struct        gpiod_chip *chip         = nullptr;
struct        gpiod_line *spi_busy     = nullptr;
struct        gpiod_line *sensor_reset = nullptr;
const uint8_t SPI_BUSY_PIN             = 73; //make runtime configurable - RPI 22
const uint8_t IWRL6432_RESET_PIN       = 75;
const int     IWRL6432_RESET_ACTIVE    = 0;
const int     IWRL6432_RESET_INACTIVE  = 1;
spi_config_t  spi_config               = {};

const uint8_t DUMMY_SIZE = 12;
uint32_t DUMMY_CRC_VALUE = { 0x28306198 };
uint8_t DUMMY_CRC_MESSAGE[] = { 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00};
uint8_t TEST_GET_RBL_STATUS_CMD[] = {0x0, 0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
#ifdef CALC32_DUMMY
uint32_t google_crc32_calc(uint8_t data[], uint32_t length)
{
        uint32_t result = crc32c::Crc32c(data, length);
        return result;
}

uint32_t zlib_crc32_calc(uint8_t data[], uint32_t length)
{
        uint32_t result = crc32(0L, Z_NULL, 0); // null seed
        result = crc32(result, (Bytef*)data, length);
        return result;
}

uint32_t cpp11_crc32_calc()
{
        std::vector<uint8_t> v(std::begin(DUMMY_CRC_MESSAGE), std::end(DUMMY_CRC_MESSAGE));
        uint32_t result = crc32<IEEE8023_CRC32_POLYNOMIAL>(0xFFFFFFFF, v.begin(), v.end());
        return result;
}

uint32_t my_crc32_calc()
{
    F_CRC_InicializaTabla();
    uint32_t result = F_CRC_CalculaCheckSum(TEST_GET_RBL_STATUS_CMD, DUMMY_SIZE);
    //uint32_t result = F_CRC_CalculaCheckSum(DUMMY_CRC_MESSAGE, DUMMY_SIZE);
    return result;
}
#endif //CALC32_DUMMY

//callback to get spi busy gpio state, this is important to decouple the spi and gpio implementations
bool gpio_spi_busy()
{
        return gpio_read(spi_busy);
}

void release_resources()
{
    // put the IWRL6432 into reset on exit
    gpio_write(sensor_reset, IWRL6432_RESET_ACTIVE);
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
#ifdef CALC32_DUMMY
   uint32_t google_dummy_crc32 = google_crc32_calc(DUMMY_CRC_MESSAGE, DUMMY_SIZE);
   uint32_t zlib_dummy_crc32   = zlib_crc32_calc(DUMMY_CRC_MESSAGE, DUMMY_SIZE);
   uint32_t cpp11_dummy_crc32  = cpp11_crc32_calc();
   uint32_t my_crc32           = my_crc32_calc();
   std::cout << "google dummy crc32: " << std::hex << google_dummy_crc32 << std::endl;
   std::cout << "zlib dummy crc32  : " << std::hex << zlib_dummy_crc32 << std::endl;
   std::cout << "cpp11 dummy crc32 : " << std::hex << cpp11_dummy_crc32 << std::endl;
   std::cout << "my crc32          : " << std::hex << my_crc32 << std::endl;
   std::cout << "expected crc32: 0x28306198" << std::endl;
#else
	const std::string gpiod_chip_name("gpiochip0");
        signal(SIGINT, siginthandler);

	int          exit_code     = 0;
	spi_config.mode            = 0;
	spi_config.speed           = 500000;
	spi_config.bits_per_word   = 16;
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
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

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
#endif // CALC32_DUMMY
}

