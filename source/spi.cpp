#include <iostream>
#include <stdint.h>
#include <array>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <asm-generic/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "appimagedata.h"
#include <functional>
#include "spi.h"
#include <memory>
#include <thread>
#include <chrono>
#include "crc.h"
#include <cstring>

/* Size of Continuous Image Download Command */
#define continuousImageDownloadCMDMsgSize   (32U)
/* Data Size for calculation of CRC of Continuous Image Download Command */
#define APP_USER_DATA_SIZE                  ((uint32_t) 28U)
/* CRC pattern size i.e 32 bits */
#define APP_CRC_PATTERN_SIZE                ((uint32_t) 4U)
#define APP_CRC_PATTERN_CNT                 (APP_USER_DATA_SIZE/APP_CRC_PATTERN_SIZE)
#define APP_CRC_SECT_CNT                    ((uint32_t) 1U)
#define APP_CRC_WATCHDOG_PRELOAD_VAL        ((uint32_t) 0U)
#define APP_CRC_BLOCK_PRELOAD_VAL           ((uint32_t) 0U)
#define APP_CRC_TYPE                        CRC_TYPE_32bit
#define APP_CRC_DATA_SIZE                   CRC_DW_32bit
#define APP_CRC_BIT_SWAP                    1
#define APP_CRC_BYTE_SWAP                   0

const uint32_t SPIDEV_MAX_BLOCK_SIZE = 4096;
uint8_t dummy_data[1024] = {0};
uint32_t crcValue=0;

const uint32_t CONTINUOUS_IMAGE_DOWNLOAD_CMD[] = {0x00000000, 0x00180010, 0x00000000, 0x00000000, 0x00000000, 0x0000000, 0x000000, 0x0000000};
const uint32_t GET_RBL_STATUS_CMD[]            = {0xFFFFFFF, 0x00100000, 0x00000000, 0x00000000};

uint32_t continuousImageDownloadRESP[8] = {0};
const uint32_t SwitchToApplicationCMD[] = {0x00000000, 0x001A0000, 0x00000000, 0x00000000}; // SWITCH_TO_APPLICATION_CMD
uint32_t SwitchToApplicationRESP[] = {0x0000,0x0000,0x0000,0x0000}; // SWITCH_TO_APPLICATION_RESP

/* Receive buffer after sending Footer commands*/
uint32_t gMcspiRxBuffer1[8]={0};
uint32_t gMcspiRxBuffer2[4]={0};
uint32_t gMcspiRxBuffer3[4]={0};


uint8_t reverse_bits(uint8_t n) 
{
    static constexpr std::array<uint8_t, 256> table{[]() constexpr
    {
            constexpr size_t SIZE = 256;
            std::array<uint8_t, SIZE> result{};

            for (size_t i = 0; i < SIZE; ++i)
            {
                result[i] = (i * 0x0202020202ULL & 0x010884422010ULL) % 0x3ff;
            }
            return result;
    }()};

    return table[n];
}

void spi_transfer(uint8_t *tx, uint8_t *rx, uint32_t length, spi_config_t config)
{
	int result = 0;

	struct spi_ioc_transfer tr = 
	{
		.tx_buf        = (unsigned long)tx,
		.rx_buf        = (unsigned long)rx,
		.len           = length,
		.speed_hz      = config.speed,
		.delay_usecs   = config.delay,
		.bits_per_word = config.bits_per_word,
                .cs_change     = 0, // has no impact on cs toggle between chunks, cs is still active, and cs_off is not available in this kernel version
	};

        // reverse bit order
#ifdef SPI_REVERSE_BIT_ORDER
        for (uint32_t i = 0; i < length; ++i)
        {
            tx[i] = reverse_bits(tx[i]);
        }
#endif // SPI_REVERSE_BIT_ORDER

        result = ioctl(config.file_descriptor, SPI_IOC_MESSAGE(1), &tr); 

        if (result < 1)
        {
	        throw std::runtime_error("ERROR: Cannot send SPI message, error number: "+ std::to_string(result));
        }
        else
        {
        }
}

bool spi_init(spi_config_t &spi_config)
{
        bool          result       = true;
	int           spi_result   = 0;
	const int32_t SPI_ERROR    = -1;

        spi_config.file_descriptor = open(spi_config.device.c_str(), O_RDWR);

        if (0 > spi_config.file_descriptor)
	{
		close(spi_config.file_descriptor);
		throw std::runtime_error("ERROR: Could not open SPI device.");
	}
        
        spi_result = ioctl(spi_config.file_descriptor, SPI_IOC_WR_MODE, &spi_config.mode);

	if (SPI_ERROR == spi_result)
	{
		result = false;
		throw std::runtime_error("ERROR: Cannot set SPI mode.");
	}

	spi_result = ioctl(spi_config.file_descriptor, SPI_IOC_RD_MODE, &spi_config.mode);

	if (SPI_ERROR == spi_result)
	{
		result = false;
		throw std::runtime_error("ERROR: Cannot get SPI mode.");
	}
        
        spi_result = ioctl(spi_config.file_descriptor, SPI_IOC_WR_LSB_FIRST, &spi_config.mode);

	if (SPI_ERROR == spi_result)
	{
		result = false;
		throw std::runtime_error("ERROR: Cannot set SPI bit order.");
	}

	spi_result = ioctl(spi_config.file_descriptor, SPI_IOC_RD_LSB_FIRST, &spi_config.mode);

	if (SPI_ERROR == spi_result)
	{
		result = false;
		throw std::runtime_error("ERROR: Cannot get SPI bit order.");
	}


	spi_result = ioctl(spi_config.file_descriptor, SPI_IOC_WR_BITS_PER_WORD, &spi_config.bits_per_word);

	if (SPI_ERROR == spi_result)
	{
		result = false;
		throw std::runtime_error("ERROR: Cannot set SPI bits per word.");
	}

	spi_result = ioctl(spi_config.file_descriptor, SPI_IOC_RD_BITS_PER_WORD, &spi_config.bits_per_word);

	if (SPI_ERROR == spi_result)
	{
		result = false;
		throw std::runtime_error("ERROR: Cannot get SPI bits per word.");
	}

	spi_result = ioctl(spi_config.file_descriptor, SPI_IOC_WR_MAX_SPEED_HZ, &spi_config.speed);

	if (SPI_ERROR == spi_result)
	{
		result = false;
		throw std::runtime_error("ERROR: Cannot set SPI bus max speed.");
	}

	spi_result = ioctl(spi_config.file_descriptor, SPI_IOC_RD_MAX_SPEED_HZ, &spi_config.speed);

	if (SPI_ERROR == spi_result)
	{
		result = false;
		throw std::runtime_error("ERROR: Cannot get SPI bus max speed.");
	}

	std::cout << "spi mode: " << std::hex << (unsigned)spi_config.mode << std::endl;
	std::cout << "spi bits per word: " << std::hex << (unsigned)spi_config.bits_per_word << std::endl;
	std::cout << "spi max speed: " << std::dec << spi_config.speed/1000 << " KHz" << std::endl;

    return result;
}

/* CRC Calculation for Continuous Image Download Command */
void calculatecrc32()
{
}

bool is_spi_busy(const spi_config_t config)
{
    static const bool SPIBUSY_ACTIVE   = true;
    static const bool SPIBUSY_INACTIVE = false;

    bool result     = false;
    bool gpio_state = SPIBUSY_ACTIVE;

    if (nullptr != config.gpio_callback)
    {
            gpio_state = config.gpio_callback();
    }
    else
    {
            throw std::runtime_error("ERROR: gpio_callback is not set.");
    }

    // this might seem obvious but it is worth spelling it out in case GPIO were to be switched to active low
    if (SPIBUSY_ACTIVE == gpio_state)
    {
            result = true;
    }
    else
    {
            result = false;
    }

    return (result);
}

void block_until_spi_ready(const spi_config_t config)
{
    while(is_spi_busy(config))
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(config.gpio_sleep_ns));
    }
}

/* Transferring appimage via SPI */
void spiboot(spi_config_t config)
{   
    const uint32_t RX_BUFFER_SIZE     = 1024;
    const uint8_t FIRMWARE_ALIGNMENT  = 16;
    uint32_t number_of_padding_bytes  = 0; 
    uint32_t number_of_padding_words  = 0; 
    uint32_t crc                      = 0x0;
    uint8_t  rx[RX_BUFFER_SIZE]       = {0};
    uint32_t total_bytes_sent         = 0;

    uint8_t image_copy[sizeof(image)/sizeof(image[0])];
    std::copy(std::begin(image), std::end(image), std::begin(image_copy));

    uint16_t *image_copy_uint16 = (uint16_t*)image_copy;

    for (uint32_t i = 0; i < (Size/2); ++i)
    {
        image_copy_uint16[i] = reverse_bytes(image_copy_uint16[i]);
    }

    if (0 != (Size % FIRMWARE_ALIGNMENT))
    {
        number_of_padding_bytes = FIRMWARE_ALIGNMENT - (Size % FIRMWARE_ALIGNMENT); //Extra bytes to make image size multiple of 16
        number_of_padding_words = number_of_padding_bytes/4;
    }
    
#define STATUS_CMD 1
#if STATUS_CMD
    // make a copy of the command  so that we can modify it before sending it over the SPI bus
    uint32_t GET_RBL_STATUS_CMD_COPY[sizeof(GET_RBL_STATUS_CMD)/sizeof(GET_RBL_STATUS_CMD[0])];
    std::copy(std::begin(GET_RBL_STATUS_CMD), std::end(GET_RBL_STATUS_CMD), std::begin(GET_RBL_STATUS_CMD_COPY));

    // reverse the MSB/LSB on each 16-bit word on the SPI_CMD_TYPE and the LONG_MSG_SIZE members
    GET_RBL_STATUS_CMD_COPY[1] = double_reversal(GET_RBL_STATUS_CMD_COPY[1]);

    // pack the message into a vector
    std::vector<uint32_t> v32_STATUS(std::begin(GET_RBL_STATUS_CMD_COPY), std::end(GET_RBL_STATUS_CMD_COPY));

    // calculate the crc32 for the message
    crc = calculate_crc32(v32_STATUS, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true);

    GET_RBL_STATUS_CMD_COPY[0] = double_reversal(crc);
    
    GET_RBL_STATUS_CMD_COPY[1] = reverse_16bits(GET_RBL_STATUS_CMD_COPY[1]);
    spi_transfer((uint8_t*)GET_RBL_STATUS_CMD_COPY, NULL, 16, config);
    block_until_spi_ready(config);
    spi_transfer((uint8_t*)dummy_data, rx, 32, config);
    block_until_spi_ready(config);
#endif //STATUS_CMD

#define CONTINUOUS_DOWNLOAD 1
#if CONTINUOUS_DOWNLOAD
    // make a copy of the command  so that we can modify it before sending it over the SPI bus
    uint32_t CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[sizeof(CONTINUOUS_IMAGE_DOWNLOAD_CMD)/sizeof(CONTINUOUS_IMAGE_DOWNLOAD_CMD[0])];
    std::copy(std::begin(CONTINUOUS_IMAGE_DOWNLOAD_CMD), std::end(CONTINUOUS_IMAGE_DOWNLOAD_CMD), std::begin(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY));


    /* calculation of CRC for Continuous Image Download Command */
    uint32_t padded_data=16-(Size%16);

    // reverse the MSB/LSB on each 16-bit word on the SPI_CMD_TYPE and the LONG_MSG_SIZE members
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[1] = double_reversal(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[1]);
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[4] = reverse_16bits(double_reversal(Size + number_of_padding_bytes)); // SPI_DOWNLOAD_SIZE_IN_BYTES = META_IMAGE_SIZE + padding for 16 byte alignment
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[5] = reverse_16bits(double_reversal(Size)); // FIRMWARE_SIZE without padding

    // pack the message into a vector
    std::vector<uint32_t> v32_DOWNLOAD(std::begin(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY), std::end(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY));

    // calculate the crc32 for the message
    crc = calculate_crc32(v32_DOWNLOAD, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true);

    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[0] = double_reversal(crc);
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[1] = reverse_16bits(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[1]);
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[4] = reverse_16bits(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[4]);
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[5] = reverse_16bits(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[5]);

    spi_transfer((uint8_t*)CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY, NULL, 32, config);
    block_until_spi_ready(config);

    uint32_t i = 0;
    const uint32_t block_size = SPIDEV_MAX_BLOCK_SIZE;

    // Send firmware in SPIDEV_MAX_BLOCK_SIZE chunks
    for (i = 0; i < (Size); i = i + SPIDEV_MAX_BLOCK_SIZE)
    {

        // stop case
        if (Size < (i + SPIDEV_MAX_BLOCK_SIZE))
        {
            break;
        }

        {
            config.bits_per_word = 8;
            spi_transfer((uint8_t*)(image_copy + i), NULL, block_size, config);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            total_bytes_sent = total_bytes_sent + block_size;
        }
    }

    uint32_t remainder = Size % SPIDEV_MAX_BLOCK_SIZE;

    // send remaining data that doesn't align with the SPI max block size
    if (0 != remainder) 
    {
        config.bits_per_word = 8;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        spi_transfer((uint8_t*)(image_copy + (Size-remainder)), NULL, remainder, config);
        total_bytes_sent = total_bytes_sent + remainder;
    }

    std::cout << "total_bytes_sent: " << std::dec << total_bytes_sent << std::endl;

    spi_transfer((uint8_t*)(dummy_data), NULL,12, config);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    block_until_spi_ready(config);
    spi_transfer((uint8_t*)dummy_data, NULL, 32, config);
    block_until_spi_ready(config);


#endif

#define APPLICATION_SWITCH 1
#if APPLICATION_SWITCH
    uint32_t SwitchToApplicationCMD_COPY[sizeof(SwitchToApplicationCMD)/sizeof(SwitchToApplicationCMD[0])];
    std::copy(std::begin(SwitchToApplicationCMD), std::end(SwitchToApplicationCMD), std::begin(SwitchToApplicationCMD_COPY));
    SwitchToApplicationCMD_COPY[1] = double_reversal(SwitchToApplicationCMD_COPY[1]);

    std::vector<uint32_t> v32_SWITCH(std::begin(SwitchToApplicationCMD_COPY), std::end(SwitchToApplicationCMD_COPY));
   
    crc = calculate_crc32(v32_SWITCH, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true);

    SwitchToApplicationCMD_COPY[0] = double_reversal(crc);
    SwitchToApplicationCMD_COPY[1] = reverse_16bits(SwitchToApplicationCMD_COPY[1]);
 
    config.bits_per_word = 8;
    spi_transfer((uint8_t*)SwitchToApplicationCMD_COPY, NULL, 16, config);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    block_until_spi_ready(config);
    spi_transfer((uint8_t*)dummy_data, rx, 16, config);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
#endif
    std::cout << "Booting via SPI is completed." << std::endl;
}

void spi_close(spi_config_t &config)
{
    if (-1 != config.file_descriptor)
    {
       close(config.file_descriptor); //TODO: consider if this can be called twice in an error state, and if that is safe
    }
}

