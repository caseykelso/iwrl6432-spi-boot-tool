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

const uint32_t SPIDEV_MAX_BLOCK_SIZE = 2048;
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
//                .cs_change     = 0, // has no impact on cs toggle between chunks, cs is still active, and cs_off is not available in this kernel version
	};

        // reverse bit order
        // TODO: need to revisit, we do not want to mutate the tx buffer, this will need to be a copy
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
//	        std::cout << unsigned(result) << std::endl;
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
#if  0
       uint32_t padded_data=16-(Size%16);
       continuousImageDownloadCMD[4]=Size+padded_data;
       continuousImageDownloadCMD[5]=Size;
       crcValue = crc32(0L, Z_NULL, 0);
       crcValue = crc32(crcValue, (Bytef*)continuousImageDownloadCMD, APP_CRC_PATTERN_CNT+1);
#endif
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
        std::this_thread::sleep_for(std::chrono::milliseconds(config.gpio_sleep_ms));
    }
}

//TODO: disable chip select in spidev
/* Transferring appimage via SPI */
void spiboot(spi_config_t config)
{   
    const uint32_t RX_BUFFER_SIZE     = 1024;
    const uint8_t FIRMWARE_ALIGNMENT  = 16;
    uint32_t number_of_padding_bytes  = FIRMWARE_ALIGNMENT - (Size % FIRMWARE_ALIGNMENT); //Extra bytes to make image size multiple of 16 
    uint32_t number_of_padding_words  = number_of_padding_bytes/4;
    uint32_t crc                      = 0x0;
    uint8_t  rx[RX_BUFFER_SIZE]       = {0};

    //Size = 8192;

    std::cout << "Booting via SPI ..." << std::endl;
    std::cout << "Transferring appimage via SPI ..." << std::endl;
    
    /* calculation of CRC for Continuous Image Download Command */
    calculatecrc32();
#define STATUS_CMD 0
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
    std::cout << "waiting" << std::endl;
    block_until_spi_ready(config);
    spi_transfer((uint8_t*)dummy_data, rx, 32, config);
    block_until_spi_ready(config);
#endif //STATUS_CMD

#define CONTINUOUS_DOWNLOAD 1
#if CONTINUOUS_DOWNLOAD
    // make a copy of the command  so that we can modify it before sending it over the SPI bus
    uint32_t CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[sizeof(CONTINUOUS_IMAGE_DOWNLOAD_CMD)/sizeof(CONTINUOUS_IMAGE_DOWNLOAD_CMD[0])];
    std::copy(std::begin(CONTINUOUS_IMAGE_DOWNLOAD_CMD), std::end(CONTINUOUS_IMAGE_DOWNLOAD_CMD), std::begin(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY));

    // reverse the MSB/LSB on each 16-bit word on the SPI_CMD_TYPE and the LONG_MSG_SIZE members
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[1] = double_reversal(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[1]);
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[4] = double_reversal(Size + number_of_padding_bytes); // SPI_DOWNLOAD_SIZE_IN_BYTES = META_IMAGE_SIZE + padding for 16 byte alignment
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[5] = double_reversal(Size); // FIRMWARE_SIZE without padding

    // pack the message into a vector
    std::vector<uint32_t> v32_DOWNLOAD(std::begin(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY), std::end(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY));

    // calculate the crc32 for the message
    crc = calculate_crc32(v32_DOWNLOAD, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true);

    std::cout  << "crc: " << std::hex << crc << std::endl;
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[0] = double_reversal(crc);

    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[1] = reverse_16bits(CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[1]);
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[4] = reverse_16bits(double_reversal(Size + number_of_padding_bytes)); //reverse_16bits(8192); // SPI_DOWNLOAD_SIZE_IN_BYTES = META_IMAGE_SIZE + padding for 16 byte alignment
    CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY[5] = reverse_16bits(double_reversal(Size)); // FIRMWARE_SIZE without padding
    std::cout << "CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY: " << std::hex << Size << std::endl;
    spi_transfer((uint8_t*)CONTINUOUS_IMAGE_DOWNLOAD_CMD_COPY, NULL, 32, config);
    std::cout << "waiting" << std::endl;
    block_until_spi_ready(config);
//    spi_transfer((uint8_t*)dummy_data, NULL, 32, config);
//    block_until_spi_ready(config);

#if 1
    // Send firmware in SPIDEV_MAX_BLOCK_SIZE chunks
    for (uint32_t i = 0; i < Size; i = i + SPIDEV_MAX_BLOCK_SIZE)
    {
        uint32_t block_size = SPIDEV_MAX_BLOCK_SIZE;

        if (i >= Size)
        {
            std::cout << "firmware programming stop case" << std::endl;
            break;
        }
        else if ((Size - i) < SPIDEV_MAX_BLOCK_SIZE) // remainder case
        {
            block_size = Size - i;
            std::cout << "firmware programming last block of size: " << block_size << std::endl;
            //TODO: padding?
        }
        else // send a full block
        {
            std::cout << "*" << std::endl;
        }

        std::cout << "count: " << std::hex << unsigned(i) << std::endl;
        block_until_spi_ready(config);
        spi_transfer((uint8_t*)(image + i), NULL, block_size, config);
    }

    std::cout << "after for loop" << std::endl;
#if 0
    //TODO: set size here for dummy data
    spi_transfer((uint8_t*)dummy_data, NULL, number_of_padding_words, config);
    std::cout << "transfer dummy data: " << size << std::endl;
    block_until_spi_ready(config);

    size = 16; //TODO: is this right?
    spi_transfer((uint8_t*)continuousImageDownloadRESP, (uint8_t*)gMcspiRxBuffer1, size, config);
    std::cout << "transfer continuous image downoad response: " << size << std::endl;

    block_until_spi_ready(config);

    size = 8; //TODO: is this right?
    spi_transfer((uint8_t*)SwitchToApplicationCMD, (uint8_t*)gMcspiRxBuffer2, size, config);
    std::cout << "transfer switch to application command: " << size << std::endl;

    block_until_spi_ready(config);	

    size = 8; //TODO: is this right?
    spi_transfer((uint8_t*)SwitchToApplicationRESP, (uint8_t*)gMcspiRxBuffer3, size, config);
    std::cout << "transfer switch to application response: " << size << std::endl;

    //TODO: put in check for success
#endif
#endif

#endif

#define APPLICATION_SWITCH 0
#if APPLICATION_SWITCH
    uint32_t SwitchToApplicationCMD_COPY[sizeof(SwitchToApplicationCMD)/sizeof(SwitchToApplicationCMD[0])];
    std::copy(std::begin(SwitchToApplicationCMD), std::end(SwitchToApplicationCMD), std::begin(SwitchToApplicationCMD_COPY));
    SwitchToApplicationCMD_COPY[1] = double_reversal(SwitchToApplicationCMD_COPY[1]);

    std::cout << std::hex << SwitchToApplicationCMD_COPY[1] << std::endl;
    std::vector<uint32_t> v32_SWITCH(std::begin(SwitchToApplicationCMD_COPY), std::end(SwitchToApplicationCMD_COPY));
   
    crc = calculate_crc32(v32_SWITCH, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true);
    std::cout << "SwitchToApplicationCMD CRC32: " << std::hex << crc << std::endl;

    SwitchToApplicationCMD_COPY[0] = double_reversal(crc);
    SwitchToApplicationCMD_COPY[1] = reverse_16bits(SwitchToApplicationCMD_COPY[1]);
 
    spi_transfer((uint8_t*)SwitchToApplicationCMD_COPY, NULL, 16, config);
    std::cout << "waiting" << std::endl;
    block_until_spi_ready(config);
    spi_transfer((uint8_t*)dummy_data, rx, 16, config);
    block_until_spi_ready(config);
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

