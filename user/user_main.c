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

#define BMFIFOLEN (1024*32)
static char bmBuff[BMFIFOLEN];
static char *bmRpos, *bmWpos;

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
		if (x<800 && einkPat>=2) {
			os_timer_arm(&tstTimer, 2, 0);
		} else {
			os_timer_arm(&tstTimer, 0, 0);
			ioEinkHscanStart();
	
			if (einkPat==0) for (x=0; x<800; x+=4) ioEinkWrite(0x55);
			if (einkPat==1) for (x=0; x<800; x+=4) ioEinkWrite(0xaa);
			if (einkPat==2) {
				for (x=0; x<800; x+=4) {
					ioEinkWrite(*bmRpos++);
					if (bmRpos>&bmBuff[BMFIFOLEN-1]) bmRpos=bmBuff;
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
		} else {
			einkPat++;
			einkState=INK_VSTART;
			os_timer_arm(&tstTimer, 100, 0);
		}
	}
}



void cb(char* data, int len) {
	int x;
	os_printf("Data: %d bytes\n", len);
	for (x=0; x<len; x++) {
		*bmWpos++=data[x];
		if (bmWpos>&bmBuff[BMFIFOLEN-1]) bmWpos=bmBuff;
	}
}

void user_init(void)
{
//	system_timer_reinit();
	stdoutInit();
	ioInit();
	
	bmRpos=bmBuff;
	bmWpos=bmBuff;
	httpclientFetch("meuk.spritesserver.nl", "/espbm.php", 1024, cb);

	os_timer_disarm(&tstTimer);
	os_timer_setfn(&tstTimer, tstTimerCb, NULL);
	os_timer_arm(&tstTimer, 3000, 0);
}
