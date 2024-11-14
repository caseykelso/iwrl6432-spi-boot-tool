#include <iostream>
#include <stdint.h>
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
#include <zlib.h>

/* Size of Continuous Image Download Command */
#define continuousImageDownloadCMDMsgSize   (32U)
#define APP_CRC_CHANNEL                     (CRC_CHANNEL_1)

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

uint32_t crcValue=0;

/* CONTINUOUS_IMAGE_DOWNLOAD_CMD command is sent to notify the RBL to setup 
DMA for continuous image download. It has the following format
<MSG_CRC><SPI_CMD_TYPE><LONG_MSG_SIZE><RESERVED><SHORT_MSG><LONG_MSG>*/

static uint32_t continuousImageDownloadCMD[]={0x0000,0x00100018,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};
static uint32_t GET_RBL_STATUS_CMD[]={0x0000,0x00100011,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};

/* The Image data should be multiple of 16 . dummy_data is used to send extra bytes 
so that overall image size should become multiple of 16 */

const uint32_t SPIDEV_MAX_BLOCK_SIZE = 4096;
uint32_t* dummy_data = NULL;
uint32_t continuousImageDownloadRESP[8] = {0};
uint32_t SwitchToApplicationCMD[] = {0x9DFAC3F2,0x0000001A,0x0000,0x0000}; // SWITCH_TO_APPLICATION_CMD
uint32_t SwitchToApplicationRESP[] = {0x0000,0x0000,0x0000,0x0000}; // SWITCH_TO_APPLICATION_RESP

/* Receive buffer after sending Footer commands*/
uint32_t gMcspiRxBuffer1[8]={0};
uint32_t gMcspiRxBuffer2[4]={0};
uint32_t gMcspiRxBuffer3[4]={0};

void spi_transfer(uint8_t const *tx, uint8_t const *rx, uint32_t length, spi_config_t config)
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

        result = ioctl(config.file_descriptor, SPI_IOC_MESSAGE(1), &tr); 

        if (result < 1)
        {
	        throw std::runtime_error("ERROR: Cannot send SPI message, error number: "+ std::to_string(result));
        }
        else
        {
	        std::cout << unsigned(result) << std::endl;
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
        uint32_t padded_data=16-(Size%16);
        continuousImageDownloadCMD[4]=Size+padded_data;
        continuousImageDownloadCMD[5]=Size;
	crcValue = crc32(0L, Z_NULL, 0);
	crcValue = crc32(crcValue, (Bytef*)continuousImageDownloadCMD, APP_CRC_PATTERN_CNT+1);
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
    uint32_t padded_data   = 16-(Size%16); //Extra bytes to make image size multiple of 16 
    uint32_t padding       = padded_data/4;

    std::cout << "Booting via SPI ..." << std::endl;
    std::cout << "Transferring appimage via SPI ..." << std::endl;
    
    /* calculation of CRC for Continuous Image Download Command */
    calculatecrc32();
    
//    continuousImageDownloadCMD[0]=crcValue;

    dummy_data = (uint32_t*)malloc(sizeof(uint32_t) * padding);

    for(uint32_t i=0; i < padding; i++)
    {
        dummy_data[i]=0;
    }

    if (NULL == dummy_data)
    {
        throw std::runtime_error("ERROR: failed to allocate SPI dummy data.");
    }

    uint8_t tx[100];

    for (uint8_t i = 0; i < 100; ++i)
    {
        tx[i] = (uint8_t)0xab;
    }

    uint8_t rx[100];

    /* Initiate transfer for Continuous Image Download Command */
    uint32_t size = continuousImageDownloadCMDMsgSize; ///config.bits_per_word; // / 2*(config.length/config.bits_per_word;

    spi_transfer((uint8_t*)continuousImageDownloadCMD, NULL, size, config); 
    std::cout << "transfer download command: " << size << std::endl;
   
    std::cout << "1" << std::endl; 
    block_until_spi_ready(config);
    std::cout << "2" << std::endl; 

    spi_transfer((uint8_t*)GET_RBL_STATUS_CMD , NULL, size, config); 
    std::cout << "transfer download command: " << size << std::endl;
   
    std::cout << "3" << std::endl; 
    block_until_spi_ready(config);

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

        spi_transfer((uint8_t*)image + i, NULL, block_size, config);
    }

    //TODO: set size here for dummy data
    spi_transfer((uint8_t*)dummy_data, NULL, size, config);
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
    std::cout << "Booting via SPI is completed." << std::endl;
}

void spi_close(spi_config_t &config)
{
    if (-1 != config.file_descriptor)
    {
       close(config.file_descriptor); //TODO: consider if this can be called twice in an error state, and if that is safe
    }
}

