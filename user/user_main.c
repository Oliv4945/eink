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
static ETSTimer wdtTimer;


#define INK_STARTUP 0
#define INK_VSTART 1
#define INK_HSEND 2
#define INK_VSTOP 3
static int einkState=0;
static int einkYpos;
static int einkPat=0;

#define BMFIFOLEN (1024*24)
static char bmBuff[BMFIFOLEN];
static char *bmRpos, *bmWpos;
static char tcpConnOpen;


void sleepmode();

static void ICACHE_FLASH_ATTR tstTimerCb(void *arg) {
	int x;
	if (einkState==INK_STARTUP) {
		einkPat=0;
		ioEinkEna(1);
		os_timer_arm(&tstTimer, 100, 0);
		einkState=INK_VSTART;
	} else if (einkState==INK_VSTART) {
		ioEinkVscanStart();
		einkYpos=0;
		einkState=INK_HSEND;
		os_timer_arm(&tstTimer, 20, 0);
	} else if (einkState==INK_HSEND) {
		//Calculate length of data in fifo
		x=bmWpos-bmRpos;
		if (x<0) x+=BMFIFOLEN;
		if (x<800 && einkPat>=2 && tcpConnOpen) {
			os_timer_arm(&tstTimer, 100, 0);
		} else {
			os_timer_arm(&tstTimer, 0, 0);
			ioEinkHscanStart();
	
			if (einkPat==0) for (x=0; x<800; x+=4) ioEinkWrite(0x55);
			if (einkPat==1) for (x=0; x<800; x+=4) ioEinkWrite(0xaa);
			if (einkPat==2) {
				for (x=0; x<800; x+=4) {
					ioEinkWrite(*bmRpos++);
					if (bmRpos>&bmBuff[BMFIFOLEN-1]) {
//						os_printf("fifo: r wraparound\n");
						bmRpos=bmBuff;
					}
				}
			}
			ioEinkHscanStop();
			ioEinkVscanWrite((einkPat==3)?0:15);
			einkYpos++;
			if (einkYpos==600) {
				einkState=INK_VSTOP;
			}
		}
	} else if (einkState==INK_VSTOP) {
		ioEinkVscanStop();
		if (einkPat==2) {
			ioEinkEna(0);
			einkState=INK_STARTUP;
			os_printf("Done displaying image. Sleeping.\n");
			sleepmode();
		} else {
			einkPat++;
			einkState=INK_VSTART;
			os_timer_arm(&tstTimer, 100, 0);
		}
	}
}



void cb(char* data, int len) {
	int x;
//	os_printf("Data: %d bytes\n", len);
	if (len==0) {
		tcpConnOpen=0;
	}
	for (x=0; x<len; x++) {
		*bmWpos++=data[x];
		if (bmWpos>&bmBuff[BMFIFOLEN-1]) {
//			os_printf("fifo: w wraparound\n");
			bmWpos=bmBuff;
		}
		if (bmWpos==bmRpos) {
			os_printf("fifo: OVERFLOW!\n");
		}
	}
}


static void ICACHE_FLASH_ATTR wdtTimerCb(void *arg) {
	os_printf("Wdt. This takes too long. Go to sleep.\n");
	ioEinkEna(0);
	sleepmode();
}

void sleepmode() {
	system_deep_sleep(20*60*1000*1000);
//	system_deep_sleep(10*1000*1000);
	os_printf("WtF, after system_deep_sleep()?\n");
//	while(1);
}


void user_init(void)
{
//	system_timer_reinit();
	stdoutInit();
	ioInit();
	
	bmRpos=bmBuff;
	bmWpos=bmBuff;
	tcpConnOpen=1;
	httpclientFetch("meuk.spritesserver.nl", "/espbm.php", 1024, cb);

	os_timer_disarm(&tstTimer);
	os_timer_setfn(&tstTimer, tstTimerCb, NULL);
	os_timer_arm(&tstTimer, 3000, 0);

	os_timer_disarm(&wdtTimer);
	os_timer_setfn(&wdtTimer, wdtTimerCb, NULL);
	os_timer_arm(&wdtTimer, 20000, 0); //shut down after 15 seconds
}
