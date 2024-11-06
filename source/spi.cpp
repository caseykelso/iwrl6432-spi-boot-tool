#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdexcept>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "appimagedata.h"
#include <fcntl.h>
#include <asm-generic/ioctl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <asm-generic/ioctl.h>

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

typedef struct {
	uint8_t mode;
	uint8_t bits_per_word;
	uint32_t speed;
	uint16_t delay;
	std::string device;
	int file_descriptor;
} spi_config_t;

uint32_t crcValue=0;

/* CONTINUOUS_IMAGE_DOWNLOAD_CMD command is sent to notify the RBL to setup 
DMA for continuous image download. It has the following format
<MSG_CRC><SPI_CMD_TYPE><LONG_MSG_SIZE><RESERVED><SHORT_MSG><LONG_MSG>*/

static uint32_t continuousImageDownloadCMD[]={0x0000,0x00100018,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};

/* The Image data should be multiple of 16 . dummyData is used to send extra bytes 
so that overall image size should become multiple of 16 */

uint32_t* dummyData = NULL;
uint32_t continuousImageDownloadRESP[8] = {0};
uint32_t SwitchToApplicationCMD[] = {0x9DFAC3F2,0x0000001A,0x0000,0x0000}; // SWITCH_TO_APPLICATION_CMD
uint32_t SwitchToApplicationRESP[] = {0x0000,0x0000,0x0000,0x0000}; // SWITCH_TO_APPLICATION_RESP

/* Receive buffer after sending Footer commands*/
uint32_t gMcspiRxBuffer1[8]={0};
uint32_t gMcspiRxBuffer2[4]={0};
uint32_t gMcspiRxBuffer3[4]={0};

bool spi_setup(spi_config_t spi_config)
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

	std::cout << "spi mode: " << std::hex << spi_config.mode << std::endl;
	std::cout << "spi bits per word: " << std::hex << spi_config.bits_per_word << std::endl;
	std::cout << "spi max speed: " << std::dec << spi_config.speed/1000 << " KHz" << std::endl;

    return result;
}

/* CRC Calculation for Continuous Image Download Command */
void calculatecrc32(void *args)
{
#if 0
    uint32_t                loopCnt, patternCnt, baseAddr;
    CRC_Channel_t           chNumber;
    CRC_Signature           signVal;
    CRC_SignatureRegAddr    psaSignRegAddr;
    
    uint32_t padded_data=16-(Size%16);
    /* Configure CRC parameters */
    baseAddr           = (uint32_t) AddrTranslateP_getLocalAddr(CONFIG_CRC0_BASE_ADDR);
    patternCnt         = APP_CRC_PATTERN_CNT;
    chNumber           = APP_CRC_CHANNEL;

    
    continuousImageDownloadCMD[4]=Size+padded_data;
    continuousImageDownloadCMD[5]=Size;
    /* Get CRC PSA signature register address */
    //CRC_getPSASigRegAddr(baseAddr, chNumber, &psaSignRegAddr);

    /* Initialize and Configure CRC channel */
    //CRC_initialize(baseAddr, chNumber, APP_CRC_WATCHDOG_PRELOAD_VAL, APP_CRC_BLOCK_PRELOAD_VAL);

    //CRC_configure(baseAddr, chNumber, patternCnt, APP_CRC_SECT_CNT, CRC_OPERATION_MODE_FULLCPU, APP_CRC_TYPE, APP_CRC_DATA_SIZE,APP_CRC_BIT_SWAP,APP_CRC_BYTE_SWAP);

    /* Reset the CRC channel*/
    //CRC_channelReset(baseAddr, chNumber);

    /* compute the CRC by writing the data buffer on which CRC computation is needed */
/*
	for (loopCnt = 1; loopCnt < patternCnt+1 ; loopCnt++)
    {
        HW_WR_REG32(psaSignRegAddr.regL, continuousImageDownloadCMD[loopCnt]);
    }
*/
    /* Fetch CRC signature value */
    //CRC_getPSASig(baseAddr, chNumber, &signVal);
    //crcValue=signVal.regL;
#endif 
}


/* Transferring appimage via SPI */
void *spibooting(void *args)
{   
    uint32_t padded_data   = 16-(Size%16); //Extra bytes to make image size multiple of 16 
    uint32_t padding       = padded_data/4;
    int32_t  status        = 0; //SystemP_SUCCESS;
    uint32_t gpioBaseAddr, pinNum;
    volatile uint32_t SPIBusy;
    int32_t  transferOK;
  //  MCSPI_Transaction  spiTransaction;
    
 //  Drivers_open();
 //   Board_driversOpen();
    
    /* Enable 256 kb shared RAM for APPSS */
 //   HW_WR_REG32(CSL_APP_CTRL_U_BASE+CSL_APP_CTRL_APPSS_SHARED_MEM_CLK_GATE,0xA);
//    CSL_REG32_FINS(CSL_TOP_PRCM_U_BASE+CSL_TOP_PRCM_HWA_PD_MEM_SHARE_REG,TOP_PRCM_HWA_PD_MEM_SHARE_REG_HWA_PD_MEM_SHARE_REG_HWA_PD_MEM_SHARE_APPSS_CONFIG, 0x3F);

	std::cout << "Booting via SPI ..." << std::endl;
    std::cout << "Transferring appimage via SPI ..." << std::endl;
    
    /* Get address after translation translate */
//    gpioBaseAddr = (uint32_t) AddrTranslateP_getLocalAddr(CSL_APP_GIO_U_BASE);
    pinNum       = 0; //SPI_BUSY_PIN;
    
    /* Setting the input data direction associated with a GPIO Pin. */
//    GPIO_setDirMode(gpioBaseAddr, pinNum, SPI_BUSY_DIR);

    /* calculation of CRC for Continuous Image Download Command */
    calculatecrc32(NULL);
    
    continuousImageDownloadCMD[0]=crcValue;
#if 0 
    dummyData = malloc(sizeof(uint32_t) * padding);
    for(int i=0;i<padding;i++){
        dummyData[i]=0;
    }
#endif
    /* Initiate transfer for Continuous Image Download Command */
#if 0
	MCSPI_Transaction_init(&spiTransaction);
    spiTransaction.channel   = gConfigMcspi0ChCfg[0].chNum;
    spiTransaction.dataSize  = 16;
    spiTransaction.csDisable = TRUE;
    spiTransaction.count     = continuousImageDownloadCMDMsgSize  / 2*(spiTransaction.dataSize/16);
    spiTransaction.txBuf     = (void *)continuousImageDownloadCMD;
    spiTransaction.rxBuf     = NULL;
    spiTransaction.args      = NULL;
    
    transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);
#endif
    /* Waiting for SPIBusy to go low */
    SPIBusy = 1;
#if 0 
    while(SPIBusy==1)
    {
        SPIBusy=GPIO_pinRead(gpioBaseAddr, pinNum);
    }

    /* Initiate transfer for Image Data in two chunks and than send Dummy Data */
    MCSPI_Transaction_init(&spiTransaction);
    spiTransaction.channel  = gConfigMcspi0ChCfg[0].chNum;
    spiTransaction.dataSize = 16;
    spiTransaction.csDisable = TRUE;
    spiTransaction.count    = Size / 4*(spiTransaction.dataSize/16);
    spiTransaction.txBuf    = (void *)image;
    spiTransaction.rxBuf    = NULL;
    spiTransaction.args     = NULL;
    transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);

    MCSPI_Transaction_init(&spiTransaction);
    spiTransaction.channel  = gConfigMcspi0ChCfg[0].chNum;
    spiTransaction.dataSize = 16;
    spiTransaction.csDisable = TRUE;
    spiTransaction.count    = Size / 4*(spiTransaction.dataSize/16);
    spiTransaction.txBuf    = (void *)(image+(Size/2));
    spiTransaction.rxBuf    = NULL;
    spiTransaction.args     = NULL;
    transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);
    
    MCSPI_Transaction_init(&spiTransaction);
    spiTransaction.channel  = gConfigMcspi0ChCfg[0].chNum;
    spiTransaction.dataSize = 16;
    spiTransaction.csDisable = TRUE;
    spiTransaction.count    = 6/ (spiTransaction.dataSize/16);
    spiTransaction.txBuf    = (void *)dummyData;
    spiTransaction.rxBuf    = NULL;
    spiTransaction.args     = NULL;
    
    transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);
    
    /* Waiting for SPIBusy to go low */
    SPIBusy = 1;
    
    while(SPIBusy==1)
    {
        SPIBusy=GPIO_pinRead(gpioBaseAddr, pinNum);
    }

    /* Receiving Continuous Image Download RESP */
    MCSPI_Transaction_init(&spiTransaction);
    spiTransaction.channel  = gConfigMcspi0ChCfg[0].chNum;
    spiTransaction.dataSize = 16;
    spiTransaction.csDisable = TRUE;
    spiTransaction.count    = 16/ (spiTransaction.dataSize/16);
    spiTransaction.txBuf    = (void *)continuousImageDownloadRESP;
    spiTransaction.rxBuf    = (void *)gMcspiRxBuffer1;
    spiTransaction.args     = NULL;
    transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);
    
    /* Waiting for SPIBusy to go low */
    SPIBusy = 1;
    
    while(SPIBusy==1)
    {
        SPIBusy=GPIO_pinRead(gpioBaseAddr, pinNum);
    }


    /* Initiate transfer for SWITCH_TO_APPLICATION_CMD */
    MCSPI_Transaction_init(&spiTransaction);
    spiTransaction.channel  = gConfigMcspi0ChCfg[0].chNum;
    spiTransaction.dataSize = 16;
    spiTransaction.csDisable = TRUE;
    spiTransaction.count    = 8/ (spiTransaction.dataSize/16);
    spiTransaction.txBuf    = (void *)SwitchToApplicationCMD;
    spiTransaction.rxBuf    = (void *)gMcspiRxBuffer2;
    spiTransaction.args     = NULL;
    transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);
    
    /* Waiting for SPIBusy to go low */
    SPIBusy = 1;
    
    while(SPIBusy==1)
    {
        SPIBusy=GPIO_pinRead(gpioBaseAddr, pinNum);
    }

  
    /* Initiate transfer for SWITCH_TO_APPLICATION_RESP */
    MCSPI_Transaction_init(&spiTransaction);
    spiTransaction.channel  = gConfigMcspi0ChCfg[0].chNum;
    spiTransaction.dataSize = 16;
    spiTransaction.csDisable = TRUE;
    spiTransaction.count    = 8/ (spiTransaction.dataSize/16);
    spiTransaction.txBuf    = (void *)SwitchToApplicationRESP;
    spiTransaction.rxBuf    = (void *)gMcspiRxBuffer3;
    spiTransaction.args     = NULL;
    transferOK = MCSPI_transfer(gMcspiHandle[CONFIG_MCSPI0], &spiTransaction);

    if((status != transferOK) ||
       (MCSPI_TRANSFER_COMPLETED != spiTransaction.status))
    {
        DebugP_assert(FALSE); 
    }
    
    DebugP_log("Booting via SPI is completed \r\n");

    Board_driversClose();
    Drivers_close();
#endif
    return NULL;
}

int main(void)
{
    int      exit_code         = 0;
	spi_config_t spi_config    = {};
	spi_config.mode            = 0;
	spi_config.speed           = 500000;
	spi_config.bits_per_word   = 8;
	spi_config.device          = "/dev/spidev0.1";
	spi_config.file_descriptor = 0;

	try 
	{
	   spi_setup(spi_config);
	} 
	catch(std::exception &e)
	{
		exit_code = -1;
		std::cerr << e.what() << std::endl;
	}

    close(spi_config.file_descriptor); //TODO: consider if this can be called twice in an error state, and if that is safe
    return exit_code;

}
