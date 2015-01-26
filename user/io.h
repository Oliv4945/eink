void ICACHE_FLASH_ATTR ioEinkEna(int ena);
void ICACHE_FLASH_ATTR ioInit(void);
void ioSpiSend(uint16_t data);

void ioEinkVscanStart();
void ioEinkHscanStart();
void ioEinkHscanStop();
void ioEinkWrite(uint8_t data);
void ioEinkVscanWrite(int len);
void ioEinkVscanStart();
void ioEinkVscanStop();
void ioEinkClk(int i);
void ioEinkVscanSkip();
