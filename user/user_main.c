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

static ETSTimer tstTimer;


#define INK_STARTUP 0
#define INK_VSTART 1
#define INK_HSEND 2
#define INK_VSTOP 3
static int einkState=0;
static int einkYpos;

static int einkPat=0;

static void ICACHE_FLASH_ATTR tstTimerCb(void *arg) {
	int x;
	if (einkState==INK_STARTUP) {
		ioEinkEna(1);
		os_timer_arm(&tstTimer, 100, 0);
		einkState=INK_VSTART;
	} else if (einkState==INK_VSTART) {
		ioEinkVscanStart();
		einkYpos=0;
		einkState=INK_HSEND;
		os_timer_arm(&tstTimer, 20, 0);


	} else if (einkState==INK_HSEND) {
		os_timer_arm(&tstTimer, 0, 0);
		ioEinkHscanStart();

		for (x=0; x<800; x+=4) {
			if (einkPat==0) ioEinkWrite(0x55);
			if (einkPat==1) ioEinkWrite(0xaa);
			if (einkPat==2) ioEinkWrite(((x^(einkYpos*4))&0x40)?0xff:0x55);
			if (einkPat==3) ioEinkWrite(((x^(einkYpos*4))&0x80)?0xff:0x55);
		}
		ioEinkHscanStop();
		ioEinkVscanWrite((einkPat==3)?0:15);
		einkYpos++;
		if (einkYpos==650) {
			einkState=INK_VSTOP;
		}
	} else if (einkState==INK_VSTOP) {
		ioEinkVscanStop();
		ioEinkEna(0);
		einkPat++;
		einkPat&=3;
		einkState=INK_STARTUP;
		os_timer_arm(&tstTimer, 1000, 0);
	}

}


//void cb(char*resp) {
//	os_printf("Resp: %s\nSleep...\n", resp);
//	system_deep_sleep(10000000);
//}

void user_init(void)
{
//	system_timer_reinit();
	stdoutInit();
	ioInit();
//	httpclientFetch("meuk.spritesserver.nl", "/esptest.php", 1024, cb);

	os_timer_disarm(&tstTimer);
	os_timer_setfn(&tstTimer, tstTimerCb, NULL);
	os_timer_arm(&tstTimer, 2000, 0);

}
