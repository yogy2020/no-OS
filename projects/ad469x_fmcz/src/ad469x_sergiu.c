#include <stdio.h>
#include <sleep.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <xil_cache.h>
#include <xparameters.h>
#include "xil_printf.h"
#include "spi_engine.h"
//#include "ad400x.h"
#include "xgpiops.h"

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/

#define AD4696_RESETN				86

#define SPI_TRIGGER_BASEADDR      		XPAR_SPI_AD4696_TRIGGER_GEN_BASEADDR

#define SPI_ENG_ADDR_VERSION       		0x00
#define SPI_ENG_ADDR_ID            		0x04
#define SPI_ENG_ADDR_SCRATCH       		0x08
#define SPI_ENG_ADDR_ENABLE       		0x40
#define SPI_ENG_ADDR_IRQMASK       		0x80
#define SPI_ENG_ADDR_IRQPEND       		0x84
#define SPI_ENG_ADDR_IRQSRC        		0x88
#define SPI_ENG_ADDR_SYNCID        		0xC0
#define SPI_ENG_ADDR_CMDFIFO_ROOM  		0xD0
#define SPI_ENG_ADDR_SDOFIFO_ROOM  		0xD4
#define SPI_ENG_ADDR_SDIFIFO_LEVEL 		0xD8
#define SPI_ENG_ADDR_CMDFIFO       		0xE0
#define SPI_ENG_ADDR_SDOFIFO       		0xE4
#define SPI_ENG_ADDR_SDIFIFO       		0xE8
#define SPI_ENG_ADDR_SDIFIFO_PEEK  		0xF0
#define SPI_ENG_ADDR_OFFLOAD_EN    		0x100
#define SPI_ENG_ADDR_OFFLOAD_RESET 		0x108
#define SPI_ENG_ADDR_OFFLOAD_CMD   		0x110
#define SPI_ENG_ADDR_OFFLOAD_SDO   		0x114

#define SPI_EN_INST_CS_ON 		0x10FE
#define SPI_EN_INST_CS_OFF		0x10FF
#define SPI_EN_INST_SYNC		0x3000
#define NUM_OF_WORDS 			1
#define SPI_EN_INST_WRD 		(0x0300 | (NUM_OF_WORDS-1))
#define DATA_WIDTH 			 	32
#define DATA_DLENGTH 	 		24
#define NUM_OF_SDI		 		1
#define NUM_OF_CS 		 		1
#define THREE_WIRE		 		0
#define CPOL 			 		1
#define CPHA 			 		1
#define CLOCK_DIVIDER 	 		2
#define INST_CFG 				(0x2100 | (THREE_WIRE << 2) | (CPOL << 1) | CPHA)
#define INST_PRESCALE 			(0x2000 | CLOCK_DIVIDER)
#define INST_DLENGTH 			(0x2200 | DATA_DLENGTH)
#define INST_DLENGTH32 			(0x2200 | DATA_WIDTH)
#define INST_DLENGTH16 			(0x2200 | 16)

#define AD469X_BASEADDR      			XPAR_SPI_AD4696_AXI_REGMAP_BASEADDR
#define AD469X_SCRATCH_OFFSET			0x00a
#define AD469X_CONFIG_IN0_OFFSET		0x030
#define AD469X_STD_SEQ_CONFIG_OFFSET	0x024
#define AD469X_SEQ_CTRL_OFFSET			0x022
#define AD469X_SETUP_OFFSET				0x020
#define AD469X_REF_CTRL_OFFSET			0x021
#define AD469X_WRITE_BIT 				0
#define AD469X_READ_BIT 				1

#define AXI_DMAC_BADDR 				XPAR_AXI_AD4696_DMA_BASEADDR
#define AXI_DMAC_REG_IRQ_MASK		0x80
#define AXI_DMAC_REG_IRQ_PENDING	0x84
#define AXI_DMAC_IRQ_SOT			BIT(0)
#define AXI_DMAC_IRQ_EOT			BIT(1)
#define AXI_DMAC_REG_CTRL			0x400
#define AXI_DMAC_CTRL_ENABLE		BIT(0)
#define AXI_DMAC_CTRL_PAUSE			BIT(1)
#define AXI_DMAC_REG_TRANSFER_ID	0x404
#define AXI_DMAC_REG_START_TRANSFER	0x408
#define AXI_DMAC_REG_FLAGS			0x40c
#define AXI_DMAC_REG_DEST_ADDRESS	0x410
#define AXI_DMAC_REG_SRC_ADDRESS	0x414
#define AXI_DMAC_REG_X_LENGTH		0x418
#define AXI_DMAC_REG_Y_LENGTH		0x41c
#define AXI_DMAC_REG_DEST_STRIDE	0x420
#define AXI_DMAC_REG_SRC_STRIDE		0x424
#define AXI_DMAC_REG_TRANSFER_DONE	0x428

#define CLKGEN_BADDR				XPAR_SPI_CLKGEN_BASEADDR

///**
// * @brief Generate miliseconds delay.
// * @param msecs - Delay in miliseconds.
// * @return None.
// */
//void mdelay(uint32_t msecs)
//{
//	usleep(msecs * 1000);
//}

void generate_transfer (u8 syncid)
{
	// assert CSN
	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, SPI_EN_INST_CS_ON);
	// transfer
	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, SPI_EN_INST_WRD);
	// de-assert CSN
	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, SPI_EN_INST_CS_OFF);
	// SYNC command to generate interrupt
	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, (SPI_EN_INST_SYNC | syncid));
}

u32 adc_transfer_24 (u8 rw_bit, u8 byte, u16 adc_reg)
{
	u32 adc_sendbuff = 0;
	adc_sendbuff = (rw_bit << 23) | (adc_reg << 8) | (byte);
	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_SDOFIFO, adc_sendbuff);
	generate_transfer (1);
	mdelay (100);
	return Xil_In32(AD469X_BASEADDR + SPI_ENG_ADDR_SDIFIFO);
}

int main_sergiu()
{
	XGpioPs_Config *ConfigPtr;
	XGpioPs Gpio;
	int Status;
	uint32_t i;
	u32 dma_adc_data [1000];
	u32 start_addr = 0;
	u32 readbuff = 0;

	Xil_Out32(CLKGEN_BADDR + 0x40, 0x3); //clkgen out of reset
	mdelay (100);

	ConfigPtr = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_DEVICE_ID);

	Status = XGpioPs_CfgInitialize(&Gpio, ConfigPtr, ConfigPtr->BaseAddr);
	XGpioPs_SetDirectionPin(&Gpio, AD4696_RESETN, 1);
	XGpioPs_SetOutputEnablePin(&Gpio, AD4696_RESETN, 1);
	XGpioPs_WritePin(&Gpio, AD4696_RESETN, 0x0);
	mdelay (100);
	XGpioPs_WritePin(&Gpio, AD4696_RESETN, 0x1);
	mdelay (100);

	start_addr = &dma_adc_data;

	for (i=0;i<1000;i++)
		dma_adc_data [i] = 0;

	xil_printf("Start addr: 0x%x\n\r", start_addr);

	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_SCRATCH, 0xdeadbeef);
	readbuff = Xil_In32(AD469X_BASEADDR + SPI_ENG_ADDR_SCRATCH);

	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_SCRATCH, 0x5a5a5a5a);
	readbuff = Xil_In32(AD469X_BASEADDR + SPI_ENG_ADDR_SCRATCH);

	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_ENABLE, 0);
	Xil_Out32(SPI_TRIGGER_BASEADDR + 0x10, 0x0);

//
	Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, INST_CFG);
    Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, INST_PRESCALE);
    Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, INST_DLENGTH);

    // check adc scratch
    readbuff = adc_transfer_24 (AD469X_READ_BIT, 0, AD469X_SCRATCH_OFFSET);
    xil_printf("scratch: 0x%x\n\r", readbuff);

    adc_transfer_24 (AD469X_WRITE_BIT, 0x5a, AD469X_SCRATCH_OFFSET);
    readbuff = adc_transfer_24 (AD469X_READ_BIT, 0, AD469X_SCRATCH_OFFSET);
    xil_printf("scratch: 0x%x\n\r", readbuff);

    adc_transfer_24 (AD469X_WRITE_BIT, 0xf0, AD469X_SCRATCH_OFFSET);
    readbuff = adc_transfer_24 (AD469X_READ_BIT, 0, AD469X_SCRATCH_OFFSET);
    xil_printf("scratch: 0x%x\n\r", readbuff);

    // std_seq_config
    readbuff = adc_transfer_24 (AD469X_READ_BIT, 0, AD469X_SEQ_CTRL_OFFSET);
    xil_printf("scratch: 0x%x\n\r", readbuff);

    adc_transfer_24 (AD469X_WRITE_BIT, readbuff & 0xff, AD469X_SEQ_CTRL_OFFSET);
    readbuff = adc_transfer_24 (AD469X_READ_BIT, 0, AD469X_SEQ_CTRL_OFFSET);
    xil_printf("scratch: 0x%x\n\r", readbuff);

    // adc setup
    readbuff = adc_transfer_24 (AD469X_READ_BIT, 0, AD469X_SETUP_OFFSET);
    xil_printf("setup: 0x%x\n\r", readbuff);

    adc_transfer_24 (AD469X_WRITE_BIT, (readbuff | 0x4), AD469X_SETUP_OFFSET); //last reg transfer

    Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, 0x2000); // clockdiv = 0
    Xil_Out32(AD469X_BASEADDR + SPI_ENG_ADDR_CMDFIFO, INST_DLENGTH16);

while(1)
{
	 readbuff = adc_transfer_24 (0, 0, 0);
     xil_printf("dac_data: 0x%x\n\r", readbuff);
     mdelay (100);
}

    //start dma transfer
	Xil_Out32(AXI_DMAC_BADDR + AXI_DMAC_REG_FLAGS, 0x2); // with tlast
	Xil_Out32(AXI_DMAC_BADDR + AXI_DMAC_REG_DEST_ADDRESS, start_addr);
	Xil_Out32(AXI_DMAC_BADDR + AXI_DMAC_REG_X_LENGTH, 4000);
	Xil_Out32(AXI_DMAC_BADDR + AXI_DMAC_REG_START_TRANSFER, 0x1);

    //start offload mode
    Xil_Out32 (AD469X_BASEADDR + SPI_ENG_ADDR_OFFLOAD_CMD, SPI_EN_INST_CS_ON);
    Xil_Out32 (AD469X_BASEADDR + SPI_ENG_ADDR_OFFLOAD_CMD, SPI_EN_INST_WRD);
	Xil_Out32 (AD469X_BASEADDR + SPI_ENG_ADDR_OFFLOAD_CMD, SPI_EN_INST_CS_OFF);
    Xil_Out32 (AD469X_BASEADDR + SPI_ENG_ADDR_OFFLOAD_CMD, SPI_EN_INST_SYNC);

    Xil_Out32 (AD469X_BASEADDR + SPI_ENG_ADDR_OFFLOAD_EN, 1);

    xil_printf("some data: 0x%x\n\r", dma_adc_data[0]);
	xil_printf("Success\n\r");

	Xil_DCacheDisable();
	Xil_ICacheDisable();

}
