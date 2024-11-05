#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <bitset>

extern "C" {
#include <mpsse.h>
};

#define RCMD	"\x03\x00\x00\x00"	// Standard SPI flash read command (0x03) followed by starting address (0x000000)
#define FOUT	"/tmp/flash.bin"		// Output file
#define RADAR_HOSTINTR1 GPIOL2
const uint8_t SPI_DATA_READY_ACTIVE     = 1U;
const uint8_t SPI_DATA_READY_INACTIVE   = 0U;
#define SPI_CHIPSELECT_IDLE_HIGH 1
#define SPI_CHIPSELECT_IDLE_LOW 0
#define BLOCK_SIZE 8

#include <cstdint>
#include <array>

uint8_t reverse_bits(uint8_t n) {
        static constexpr std::array<uint8_t, 256> table{[]() constexpr{
                constexpr size_t SIZE = 256;
                std::array<uint8_t, SIZE> result{};

                for (size_t i = 0; i < SIZE; ++i)
                    result[i] = (i * 0x0202020202ULL & 0x010884422010ULL) % 0x3ff;
                return result;
        }()};

    return table[n];
}

int main(void)
{
    FILE *fp                    = NULL;
    char *data                  = NULL;
    int retval                  = EXIT_FAILURE;

    struct mpsse_context *spi   = NULL;

    spi  = MPSSE(SPI0, FIFTEEN_MHZ, MSB);
    SetCSIdle(spi, SPI_CHIPSELECT_IDLE_HIGH); // chip select idles high

    if (NULL == spi || !spi->open)
    {
        std::cerr << "Failed to open SPI bus." << std::endl;
        exit(1);
    }

    printf("%s initialized at %dHz (SPI mode 0)\n", GetDescription(spi), GetClock(spi));

    std::cout << "starting spi read" << std::endl;

    // Wait for the peripheral to signal that data is ready and then read that data
    while(1)
    {
        if (SPI_DATA_READY_ACTIVE == PinState(spi, RADAR_HOSTINTR1, -1))
        {
            Start(spi);
            data = Read(spi, BLOCK_SIZE);
 
            Stop(spi);
            if (NULL != data) // did we read data?
            {

                for (uint8_t i = 0; i < BLOCK_SIZE; ++i)
                {
                    std::cout << std::hex << static_cast<unsigned>(reverse_bits(data[i])) << " ";
 
                }
                std::cout << std::endl;
            }
        }
        else
        {
//           std::cout << "#" << std::endl;  
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    }
    std::cout << "exit" << std::endl;
    Close(spi);
    return 0;
}

