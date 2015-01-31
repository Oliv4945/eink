#include <c_types.h>
#include "config.h"
#include "spi_flash.h"
#include <osapi.h>
#include "espmissingincludes.h"

MyConfig myConfig;

static char calcChsum() {
	char *p=(char*)&myConfig;
	int x;
	char r=0xa5;
	for (x=1; x<sizeof(MyConfig); x++) r+=p[x];
	return r;
}

#define ESP_PARAM_SEC		0x7A

void configLoad() {
	int c;
	spi_flash_read(ESP_PARAM_SEC*SPI_FLASH_SEC_SIZE, (uint32 *)&myConfig, sizeof(MyConfig));
	c=calcChsum();
	if (c!=myConfig.chsum) {
		os_strcpy(myConfig.url, "http://meuk.spritesserver.nl/espbm.php");
	}
}

void configSave() {
	myConfig.chsum=calcChsum();
	spi_flash_erase_sector(ESP_PARAM_SEC);
	spi_flash_write(ESP_PARAM_SEC*SPI_FLASH_SEC_SIZE, (uint32 *)&myConfig, sizeof(MyConfig));
}
