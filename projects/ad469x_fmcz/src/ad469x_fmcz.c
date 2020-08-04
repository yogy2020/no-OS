#include <stdio.h>
#include <sleep.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <xil_cache.h>
#include <xparameters.h>
#include "xil_printf.h"
#include "spi_engine.h"
#include "ad469x.h"
#include "error.h"
#include "delay.h"

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/
#define AD469x_EVB_SAMPLE_NO			10000
#define AD469x_DMA_BASEADDR             XPAR_AXI_AD4696_DMA_BASEADDR
#define AD469x_SPI_ENGINE_BASEADDR      XPAR_SPI_AD4696_AXI_REGMAP_BASEADDR
#define AD469x_SPI_CS                   0
#define AD469x_SPI_ENG_REF_CLK_FREQ_HZ	XPAR_PS7_SPI_0_SPI_CLK_FREQ_HZ

#define SPI_ENGINE_OFFLOAD_EXAMPLE	0

struct spi_engine_init_param spi_eng_init_param  = {
	.ref_clk_hz = AD469x_SPI_ENG_REF_CLK_FREQ_HZ,
	.type = SPI_ENGINE,
	.spi_engine_baseaddr = AD469x_SPI_ENGINE_BASEADDR,
	.cs_delay = 2,
	.data_width = 24,
};

struct ad469x_init_param ad469x_init_param = {
	.spi_init = {
		.chip_select = AD469x_SPI_CS,
		.max_speed_hz = 40000000,
		.mode = SPI_MODE_0,
		.platform_ops = &spi_eng_platform_ops,
		.extra = (void*)&spi_eng_init_param,
	},
	1000000,
	ID_AD4003, /* dev_id */
};

int main()
{
	struct ad469x_dev *dev;
	uint32_t *offload_data;
	uint32_t adc_data;
	struct spi_engine_offload_init_param spi_engine_offload_init_param = {
		.offload_config = OFFLOAD_RX_EN,
		.rx_dma_baseaddr = AD469x_DMA_BASEADDR,
	};
	struct spi_engine_offload_message msg;

	uint8_t commands_data[2] = {0xFF, 0xFF};
	int32_t ret, data;
	uint32_t i;

	print("Test\n\r");

	uint32_t spi_eng_msg_cmds[3] = {
		CS_LOW,
		READ(2),
		CS_HIGH
	};

	Xil_ICacheEnable();
	Xil_DCacheEnable();

	ret = ad469x_init(&dev, &ad469x_init_param);
	if (ret < 0)
		return ret;

	if (SPI_ENGINE_OFFLOAD_EXAMPLE == 0) {
		while(1) {
			ad469x_spi_single_conversion(dev, &adc_data);
			xil_printf("ADC: %d\n\r", adc_data);
		}
	}
	/* Offload example */
	else {
		ret = spi_engine_offload_init(dev->spi_desc, &spi_engine_offload_init_param);
		if (ret != SUCCESS)
			return FAILURE;

		msg.commands = spi_eng_msg_cmds;
		msg.no_commands = ARRAY_SIZE(spi_eng_msg_cmds);
		msg.rx_addr = 0x800000;
		msg.tx_addr = 0xA000000;
		msg.commands_data = commands_data;

		ret = spi_engine_offload_transfer(dev->spi_desc, msg, AD469x_EVB_SAMPLE_NO);
		if (ret != SUCCESS)
			return ret;

		mdelay(2000);
		Xil_DCacheInvalidateRange(0x800000, AD469x_EVB_SAMPLE_NO * 4);
		offload_data = (uint32_t *)msg.rx_addr;

		for(i = 0; i < AD469x_EVB_SAMPLE_NO / 2; i++) {
			data = *offload_data & 0xFFFFF;
			if (data > 524287)
				data = data - 1048576;
			printf("ADC%d: %d \n", i, data);
			offload_data += 1;
		}
	}

	print("Success\n\r");

	Xil_DCacheDisable();
	Xil_ICacheDisable();
}
