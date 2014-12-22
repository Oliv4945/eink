#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "gpio.h"

//Pinout:
// GPIO14/MTMS/HSPICLK - clk
// GPIO13/MTCK/HSPID - data
// gpio5 - boostdis
// gpio2 - cl
// gpio12 - strobe ctl shift reg

//Pinout ctl shift reg
// sh0 - OE
// sh1 - Vneg_ena
// sh2 - SPH
// sh3 - CKV
// sh4 - Vpos_ena
// sh5 - LE
// sh6 - SPV
// sh7 - gmode


void ICACHE_FLASH_ATTR ioLed(int ena) {
	if (ena) {
		gpio_output_set(BIT2, 0, BIT2, 0);
	} else {
		gpio_output_set(0, BIT2, BIT2, 0);
	}
}

void ioInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	//Enable SPI
	WRITE_PERI_REG(PERIPHS_IO_MUX, 0x105);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);
	SET_PERI_REG_MASK(SPI_USER(1), SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_COMMAND | SPI_USR_MOSI);
	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_FLASH_MODE);
	//clear Dual or Quad lines transmission mode
	CLEAR_PERI_REG_MASK(SPI_CTRL(1), SPI_QIO_MODE |SPI_DIO_MODE | SPI_DOUT_MODE | SPI_QOUT_MODE);
	
	// set clk divider
	WRITE_PERI_REG(SPI_CLOCK(1),
		((3 & SPI_CLKDIV_PRE) << SPI_CLKDIV_PRE_S) |	// sets the divider 0 = 20MHz , 1 = 10MHz , 2 = 6.667MHz , 3 = 5MHz , 4 = 4M$
		((3 & SPI_CLKCNT_N) << SPI_CLKCNT_N_S) |		// sets the SCL cycle length in SPI_CLKDIV_PRE cycles
		((1 & SPI_CLKCNT_H) << SPI_CLKCNT_H_S) |		// sets the SCL high time in SPI_CLKDIV_PRE cycles
		((3 & SPI_CLKCNT_L) << SPI_CLKCNT_L_S));		// sets the SCL low time in SPI_CLKDIV_PRE cycles
														// N == 3, H == 1, L == 3 : 50% Duty Cycle
														// clear bit 31,set SPI clock div

	//set 8bit output buffer length, the buffer is the low 8bit of register"SPI_FLASH_C0"
	WRITE_PERI_REG(SPI_USER1(1),
		((15 & SPI_USR_MOSI_BITLEN) << SPI_USR_MOSI_BITLEN_S)|
		((15 & SPI_USR_MISO_BITLEN) << SPI_USR_MISO_BITLEN_S));
}


//we keep this function in SRAM for speed reasons
void hspi_spi_send(uint16 data) {
	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_USR_MOSI | SPI_USR_MISO);
	//SPI_FLASH_USER2 bit28-31 is cmd length,cmd bit length is value(0-15)+1,
	// bit15-0 is cmd value.
	WRITE_PERI_REG(SPI_USER2(1), ((15 & SPI_USR_COMMAND_BITLEN) << SPI_USR_COMMAND_BITLEN_S) | data);
	SET_PERI_REG_MASK(SPI_CMD(1), SPI_USR);
	while(READ_PERI_REG(SPI_CMD(1)) & SPI_USR);
}

