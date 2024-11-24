#define CALC32_DUMMY 1
//#define SPI_TEST_PATTERN 1

#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <thread>
#include <cstring>
#include <chrono>
#include <vector>
#include <signal.h>
#include <gpio.h>
#include <functional>
#include <spi.h>
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

const uint8_t DUMMY_SIZE = 24;
uint32_t DUMMY_CRC_VALUE = { 0x28306198 };
//uint8_t DUMMY_CRC_MESSAGE[] = { 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00};
//uint8_t DUMMY_CRC_MESSAGE[] = { 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
//uint32_t DUMMY_CRC_MESSAGE[] = {0x9DFAC3F2,0x0000001A,0x0000,0x0000, 0x00, 0x0, 0x0};
uint8_t DUMMY_CRC_MESSAGE[] =  {0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t DUMMY_CRC_MESSAGE_REVERSED[DUMMY_SIZE];
uint8_t TEST_GET_RBL_STATUS_CMD[] = {0x0, 0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
uint8_t TEST_SWITCH_TO_APPLICATION_CMD[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1A, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

// Function to reverse bits in an integer (similar to the Python version)
uint32_t reverseBits(uint32_t value, uint32_t bitCount) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < bitCount; ++i) {
        result = (result << 1) | (value & 1);
        value >>= 1;
    }
    return result;
}



// Function to calculate CRC32 in the same manner as the Python implementation
uint32_t calculateCRC32(const std::vector<uint8_t>& payload, uint32_t poly = 0x04C11DB7,
                        uint32_t initCRC = 0xFFFFFFFF, uint32_t xorOut = 0xFFFFFFFF, bool reverse = false) {
    uint32_t crc = initCRC;

    // Process the payload in 4-byte chunks
    for (size_t i = 0; i < payload.size(); i += 4) {
        // Extract a 4-byte chunk
        uint32_t chunk = 0;
        std::vector<uint8_t> chunkBytes(4, 0);
        std::memcpy(&chunkBytes[0], &payload[i], std::min<size_t>(4, payload.size() - i));
        chunk = (chunkBytes[0] << 24) | (chunkBytes[1] << 16) | (chunkBytes[2] << 8) | chunkBytes[3];

        if (reverse) {
            chunk = reverseBits(chunk, 32); // Reverse bits if needed
        }

        // XOR the chunk into the CRC register
        crc ^= chunk;

        // Perform 32 iterations of the CRC algorithm for each bit in the chunk
        for (int j = 0; j < 32; ++j) {
            if (crc & 0x80000000) {
                crc = ((crc << 1) & 0xFFFFFFFF) ^ poly;
            } else {
                crc = (crc << 1) & 0xFFFFFFFF;
            }
        }
    }

    // Reverse bits of the CRC value if needed
    if (reverse) {
        crc = reverseBits(crc, 32);
    }

    // Apply the XOR-out value
    return crc ^ xorOut;
}

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
   std::vector<uint8_t> v(std::begin(DUMMY_CRC_MESSAGE), std::end(DUMMY_CRC_MESSAGE));
   uint32_t crc = calculateCRC32(v, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true);
   std::cout << "crc32: " << std::hex << crc << std::endl;
#else
	const std::string gpiod_chip_name("gpiochip0");
        signal(SIGINT, siginthandler);

	int          exit_code     = 0;
	spi_config.mode            = 0;
	spi_config.speed           = 500000;
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

