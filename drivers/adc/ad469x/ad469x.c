/***************************************************************************//**
 *   @file   ad469x.c
 *   @brief  Implementation of ad69x Driver.
 *   @author Cristian Pop (cristian.pop@analog.com)
********************************************************************************
 * Copyright 2020(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "ad469x.h"
#include "spi_engine.h"
#include "error.h"

/******************************************************************************/
/************************** Functions Implementation **************************/
/******************************************************************************/

/**
 * Read from device.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param reg_data - The register data.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad469x_spi_reg_read(struct ad469x_dev *dev,
			     uint16_t reg_addr,
			     uint8_t *reg_data)
{
	int32_t ret;
	uint8_t buf[3];

	buf[0] = AD469x_REG_READ(reg_addr >> 8);
	buf[1] = 0xFF & reg_addr;
	buf[2] = 0xFF;

	// register access runs at a lower clock rate (~2MHz)
//	spi_engine_set_speed(dev->spi_desc, dev->reg_access_speed);

	ret = spi_write_and_read(dev->spi_desc, buf, 3);
	*reg_data = buf[0];

//	spi_engine_set_speed(dev->spi_desc, dev->spi_desc->max_speed_hz);

	return ret;
}

/**
 * Write to device.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param reg_data - The register data.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad469x_spi_reg_write(struct ad469x_dev *dev,
			      uint16_t reg_addr,
			      uint8_t reg_data)
{
	int32_t ret;
	uint8_t buf[2];

	// register access runs at a lower clock rate (~2MHz)
	spi_engine_set_speed(dev->spi_desc, dev->reg_access_speed);

	buf[0] = AD469x_REG_READ(reg_addr >> 8);
	buf[1] = 0xFF & reg_addr;
	buf[2] = reg_data;

	ret = spi_write_and_read(dev->spi_desc, buf, 3);

	spi_engine_set_speed(dev->spi_desc, dev->spi_desc->max_speed_hz);

	return ret;
}

/**
 * Read conversion result from device.
 * @param dev - The device structure.
 * @param adc_data - The conversion result data
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad469x_spi_single_conversion(struct ad469x_dev *dev,
				     uint32_t *adc_data)
{
	uint32_t buf = 0;
	int32_t ret;

	ret = spi_write_and_read(dev->spi_desc, (uint8_t *)&buf, 4);

	*adc_data = buf & 0xFFFFF;

	return ret;
}

/**
 * SPI read from device using a mask.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param mask - The mask.
 * @param data - The register data.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad469x_spi_read_mask(struct ad469x_dev *dev,
			      uint16_t reg_addr,
			      uint8_t mask,
			      uint8_t *data)
{
	uint8_t reg_data[3];
	int32_t ret;

	ret = ad469x_spi_reg_read(dev, reg_addr, reg_data);
	*data = (reg_data[1] & mask);

	return ret;
}

/**
 * SPI write to device using a mask.
 * @param dev - The device structure.
 * @param reg_addr - The register address.
 * @param mask - The mask.
 * @param data - The register data.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad469x_spi_write_mask(struct ad469x_dev *dev,
			       uint16_t reg_addr,
			       uint8_t mask,
			       uint8_t data)
{
	uint8_t reg_data;
	int32_t ret;

	ret = ad469x_spi_reg_read(dev, reg_addr, &reg_data);
	reg_data &= ~mask;
	reg_data |= data;
	ret |= ad469x_spi_reg_write(dev, reg_addr, reg_data);

	return ret;
}

/**
 * Initialize the device.
 * @param device - The device structure.
 * @param init_param - The structure that contains the device initial
 *                     parameters.
 * @return 0 in case of success, negative error code otherwise.
 */
int32_t ad469x_init(struct ad469x_dev **device,
		    struct ad469x_init_param *init_param)
{
	struct ad469x_dev *dev;
	int32_t ret;
	uint8_t data = 0;
	int32_t status;
	uint32_t rate;
	struct spi_engine_init_param *spi_eng_init_param = init_param->spi_init.extra;

	dev = (struct ad469x_dev *)malloc(sizeof(*dev));
	if (!dev)
		return -1;

	status = axi_clkgen_init(&dev->clkgen, &init_param->clkgen_init);
	if (status != SUCCESS) {
		printf("error: %s: axi_clkgen_init() failed\n", init_param->clkgen_init.name);
		goto error;
	}

	status = axi_clkgen_set_rate(dev->clkgen, 160000000);
	if (status != SUCCESS) {
		printf("error: %s: axi_clkgen_set_rate() failed\n", init_param->clkgen_init.name);
		goto error;
	}
	status = axi_clkgen_get_rate(dev->clkgen, &rate);

	printf("clock rate %ld", rate);

	ret = spi_init(&dev->spi_desc, &init_param->spi_init);
	if (ret < 0)
		goto error;

	dev->reg_access_speed = init_param->reg_access_speed;
	dev->dev_id = init_param->dev_id;

//	spi_engine_set_transfer_width(dev->spi_desc, 16);
	ad469x_spi_reg_read(dev, AD469x_REG_SCRATCH_PAD, &data);

//	spi_engine_set_transfer_width(dev->spi_desc, spi_eng_init_param->data_width);
//	data |= AD400X_TURBO_MODE(init_param->turbo_mode) |
//	       AD400X_HIGH_Z_MODE(init_param->high_z_mode) |
//	       AD400X_SPAN_COMPRESSION(init_param->span_compression) |
//	       AD400X_EN_STATUS_BITS(init_param->en_status_bits);
	data = 0xEA;
	ret = ad469x_spi_reg_write(dev, AD469x_REG_SCRATCH_PAD, data);
	if (ret < 0)
		goto error;

//	spi_engine_set_transfer_width(dev->spi_desc, 16);
	ad469x_spi_reg_read(dev, AD469x_REG_SCRATCH_PAD, &data);

//	spi_engine_set_transfer_width(dev->spi_desc, spi_eng_init_param->data_width);

	*device = dev;

	return ret;

error:
	ad469x_remove(dev);
	return ret;
}

/***************************************************************************//**
 * @brief Free the resources allocated by ad400x_init().
 * @param dev - The device structure.
 * @return SUCCESS in case of success, negative error code otherwise.
*******************************************************************************/
int32_t ad469x_remove(struct ad469x_dev *dev)
{
	int32_t ret;

	ret = spi_remove(dev->spi_desc);

	free(dev);

	return ret;
}
