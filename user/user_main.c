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

static ETSTimer wakeTimer;

static void ICACHE_FLASH_ATTR wakeTimerCb(void *arg) {
	user_init();
}

void cb(char*resp) {
	os_printf("Resp: %s\nSleep...\n", resp);
#ifdef USE_DEEP_SLEEP
	system_deep_sleep(5000000);
#endif
	wifi_set_sleep_type(LIGHT_SLEEP_T);
//	wifi_set_sleep_type(MODEM_SLEEP_T);
//	system_deep_sleep(5000000);

	os_timer_disarm(&wakeTimer);
	os_timer_setfn(&wakeTimer, wakeTimerCb, NULL);
	os_timer_arm(&wakeTimer, 10000, 0);


//	user_init();
}

void user_init(void)
{
//	system_timer_reinit();
	stdoutInit();
	ioInit();
	httpclientFetch("meuk.spritesserver.nl", "/esptest.php", 1024, cb);
}
