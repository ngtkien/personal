#include "RA8875.h"
#include "_settings/RA8875Registers.h"
#include "_settings/RA8875UserSettings.h"
// #include "../../platform_config.h"
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>

#include <driver/spi_common.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#define DT_DRV_COMPAT ra8875
#if DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 0
#error"ra8875 series coder is not defined in DTS"
#endif
extern const struct device *display;
static const struct spi_config spi_cfg = {
	.operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
	.frequency = 4000000,
};

uint8_t DisplayTransmitReceive(uint8_t *in, uint8_t *out){
	
	//cs to low
	
	//send data
	
	//cs to high 
    return 1;
}
uint8_t DisplayTransmitReceive3(uint8_t *in, uint8_t *out){
	
    return 1;
}
uint8_t RA8875_readStatus(void);

/**
 * @brief writes data to the RA8875
 *
 * @param ra8875 - Pointer to the display struct
 * @param data - the data to be written (1 byte)
 */
void RA8875_writeData(uint8_t data) {
	uint8_t b[2];
	uint8_t ret[2];
	b[0] = RA8875_DATAWRITE;
	b[1] = data;

	DisplayTransmitReceive(b, ret);
}

/**
 * @brief writes command to the RA8875
 *
 * @param ra8875 - Pointer to the display struct
 * @param data - the command to be written (1 byte)
 */
void RA8875_writeCommand(uint8_t data) {
	uint8_t b[2];
	uint8_t ret[2];
	b[0] = RA8875_CMDWRITE;
	b[1] = data;

	DisplayTransmitReceive(b, ret);
}

/**
 * @brief Setup PWM engine
 *
 * @param pw - pwm selection (1,2)
 * @param on - turn on/off
 * @param clock- the clock setting
 */
void RA8875_PWMsetup(uint8_t pw, uint8_t on, uint8_t clock) {
	uint8_t reg;
	uint8_t set;
	if (pw > 1) {
		reg = RA8875_P2CR;
		if (on)
			set = RA8875_PxCR_ENABLE;
		else
			set = RA8875_PxCR_DISABLE;
	} else {
		reg = RA8875_P1CR;
		if (on)
			set = RA8875_PxCR_ENABLE;
		else
			set = RA8875_PxCR_DISABLE;
	}
	RA8875_writeRegister(reg, (set | (clock & 0xF)));
}

/**
 * @brief  PWM out
 *
 * @param pw - pwm selection (1,2)
 * @param p - rate - 0..255
 */
void RA8875_PWMout(uint8_t pw, uint8_t p) {
	uint8_t reg;
	if (pw > 1) {
		reg = RA8875_P2DCR;
	} else {
		reg = RA8875_P1DCR;
	}
	RA8875_writeRegister(reg, p);
}

/**
 * @brief Read data
 *
 * @param stat - data or parameter
 */
uint8_t RA8875_readData(uint8_t stat) {
	///////////////Read data or  parameter
	uint8_t b[2];
	uint8_t ret[2];
	if (stat) {
		b[0] = RA8875_CMDREAD;
	} else {
		b[0] = RA8875_DATAREAD;
	}
	b[1] = 0x00;

	DisplayTransmitReceive(b, ret);

	return ret[1];
}

/**
 * @brief Read the status
 */
uint8_t RA8875_readStatus(void) {
	return RA8875_readData(1);
}

/**
 * @brief Write in a register
 *
 * @param reg - the register
 * @param val - the data
 */
void RA8875_writeRegister(uint8_t reg, uint8_t val) {
	RA8875_writeCommand(reg);
	RA8875_writeData(val);
}

/**
 * @brief Returns the value inside register
 *
 * @param reg - the register
 * @return the register value
 */
uint8_t RA8875_readRegister(const uint8_t reg) {
	RA8875_writeCommand(reg);
	return RA8875_readData(0);
}

/**
 * @brief Write 16 bit data
 *
 * @param data - the data
 */
void RA8875_writeData16(uint16_t data) {

	uint8_t b[3];
	uint8_t ret[3];
	b[0] = RA8875_DATAWRITE;
	b[1] = (data >> 8) & 0xff;
	b[2] = data & 0xff;

	DisplayTransmitReceive3(b, ret);
}

/**
 * @brief Reads a register and wait till the flag is set. Wait for 20 ms
 *
 * @param reg - the register
 * @param waitFlag - the flag to wait on
 * @return 1 - register has set the flag, 0 the register did not set the flag
 */
uint8_t RA8875_waitPoll(uint8_t regname, uint8_t waitflag) {
	uint8_t temp;
	unsigned long timeout = k_cycle_get_64();

	while (1) {
		temp = RA8875_readRegister(regname);
		if (!(temp & waitflag))
			return 1;
		if (( k_cycle_get_64()- timeout) > 20)
			break;
	}
	return 0;
}

/**
 * @brief Just another specified wait routine until job it's done
 *
 * The routine will wait for the following flags:
 * 0x80(for most operations),
 * 0x40(BTE wait),
 * 0x01(DMA wait)
 *
 * @param re - wait for..
 */
void RA8875_waitBusy(uint8_t res) {
	uint8_t temp;
	unsigned long start = k_cycle_get_64();	//M.Sandercock
	do {
		if (res == 0x01)
			RA8875_writeCommand(RA8875_DMACR);	//dma
		temp = RA8875_readStatus();
		if ((k_cycle_get_64() - start) > 10)
			return;
	} while ((temp & res) == res);
}