#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "gpio.h"
#include "spi_register.h"


//GPIO pinout:
// GPIO14/MTMS/HSPICLK - clk
// GPIO13/MTCK/HSPID - data
#define BOOSTDIS 4
#define E_CL 2
#define STROBE 12

// WARNING: The labels of the GPIO4 and GPIO5 pins seem switched on the ESP-12 module.

//Control shift register pinout:
#define E_OE		(1<<0)
#define VNEG_ENA	(1<<1)
#define E_SPH		(1<<2)
#define E_CKV		(1<<3)
#define VPOS_ENA	(1<<4)
#define E_LE		(1<<5)
#define E_SPV		(1<<6)
#define E_GMODE		(1<<7)

static uint8_t sregVal;

//we keep this function in SRAM for speed reasons
void ioSpiSend(uint16_t data) {
	while(READ_PERI_REG(SPI_CMD(1)) & SPI_USR);
	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_USR_MOSI | SPI_USR_MISO);
	//SPI_FLASH_USER2 bit28-31 is cmd length,cmd bit length is value(0-15)+1,
	// bit15-0 is cmd value.
	WRITE_PERI_REG(SPI_USER2(1), ((7 & SPI_USR_COMMAND_BITLEN) << SPI_USR_COMMAND_BITLEN_S) | data);
	SET_PERI_REG_MASK(SPI_CMD(1), SPI_USR);
}

void ioShiftCtl() {
	ioSpiSend(sregVal);
	while(READ_PERI_REG(SPI_CMD(1)) & SPI_USR);
	gpio_output_set((1<<STROBE), 0, (1<<STROBE), 0);
	os_delay_us(1);
	gpio_output_set(0, (1<<STROBE), (1<<STROBE), 0);
}

void ioEinkHclk() {
	gpio_output_set((1<<E_CL), 0, (1<<E_CL), 0);
	gpio_output_set(0, (1<<E_CL), (1<<E_CL), 0);
}

void ioEinkVclk() {
	sregVal&=~(E_CKV); ioShiftCtl();
	os_delay_us(1);
	sregVal|=(E_CKV); ioShiftCtl();
}

void ioEinkVscanStart() {
	sregVal|=(E_GMODE|E_OE|E_CKV); 
	sregVal&=~(E_LE);
	ioShiftCtl();
	os_delay_us(1000);
	sregVal|=(E_SPV); ioShiftCtl();
	os_delay_us(500);
	sregVal&=~(E_SPV); ioShiftCtl();
	os_delay_us(1);
	sregVal&=~(E_CKV); ioShiftCtl();
	os_delay_us(25);
	sregVal|=(E_CKV); ioShiftCtl();
	os_delay_us(1);
	sregVal|=(E_SPV); ioShiftCtl();

	os_delay_us(25);
	ioEinkVclk();
	os_delay_us(25);
	ioEinkVclk();
	os_delay_us(25);
	ioEinkVclk();
}

void ioEinkVscanStop() {
	ioEinkVclk();
	ioEinkVclk();
	ioEinkVclk();
	ioEinkVclk();
	ioEinkVclk();
	ioEinkVclk();
	ioEinkVclk();
	ioEinkVclk();
	sregVal&=~(E_CKV); ioShiftCtl();
	os_delay_us(3000);
	sregVal|=(E_CKV); ioShiftCtl();
	os_delay_us(430);
}

void ioEinkHscanStart() {
	ioEinkHclk();
	ioEinkHclk();
	sregVal&=~(E_SPH); ioShiftCtl();
}

void ioEinkHscanStop() {
	sregVal|=(E_SPH); ioShiftCtl();
}

void ioEinkWrite(uint8_t data) {
	ioSpiSend(data);
	while(READ_PERI_REG(SPI_CMD(1)) & SPI_USR);
	gpio_output_set((1<<E_CL), 0, (1<<E_CL), 0);
	gpio_output_set(0, (1<<E_CL), (1<<E_CL), 0);
}

void ioEinkVscanWrite(int len) {
	gpio_output_set((1<<E_CL), 0, (1<<E_CL), 0);
	sregVal&=~(E_CKV); ioShiftCtl();
	gpio_output_set(0, (1<<E_CL), (1<<E_CL), 0);
	os_delay_us(len);
	sregVal|=(E_CKV); ioShiftCtl();
	sregVal|=(E_LE); ioShiftCtl();
	sregVal&=~(E_LE); ioShiftCtl();
	os_delay_us(1);
}

void ICACHE_FLASH_ATTR ioEinkEna(int ena) {
	if (ena) {
		gpio_output_set(0, (1<<BOOSTDIS), (1<<BOOSTDIS), 0);
		os_delay_us(1000);
		sregVal=VNEG_ENA; ioShiftCtl();
		os_delay_us(1000);
		sregVal=VNEG_ENA|VPOS_ENA; ioShiftCtl();
	} else {
		sregVal=VNEG_ENA; ioShiftCtl();
		os_delay_us(1000);
		sregVal=0; ioShiftCtl();
		os_delay_us(1000);
		gpio_output_set((1<<BOOSTDIS), 0, (1<<BOOSTDIS), 0);
	}
}

void ICACHE_FLASH_ATTR ioInit() {
	gpio_init();
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	gpio_output_set((1<<BOOSTDIS), (1<<E_CL)|(1<<STROBE), (1<<BOOSTDIS)|(1<<E_CL)|(1<<STROBE), 0);

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
		((8 & SPI_USR_MOSI_BITLEN) << SPI_USR_MOSI_BITLEN_S)|
		((8 & SPI_USR_MISO_BITLEN) << SPI_USR_MISO_BITLEN_S));
	ioShiftCtl(0);
}



