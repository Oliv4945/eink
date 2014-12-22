//#define USE_US_TIMER
#include "espmissingincludes.h"
#include "ets_sys.h"
#include "stdout.h"
#include "c_types.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "io.h"
#include "httpdclient.h"

extern uint8_t at_wifiMode;
void user_init(void);

void cb(char*resp) {
	os_printf("Resp: %s\nSleep...\n", resp);
	system_deep_sleep(10000000);
}

void user_init(void)
{
//	system_timer_reinit();
	stdoutInit();
	ioInit();
	httpclientFetch("meuk.spritesserver.nl", "/esptest.php", 1024, cb);
}
