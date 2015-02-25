#include "eink.h"
#include "espmissingincludes.h"
#include "ets_sys.h"
#include "stdout.h"
#include "c_types.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "io.h"
#include "httpdclient.h"
#include "mem.h"

static ETSTimer einkTimer;
static EinkNeedDataCb *needDataCb=NULL;
static EinkDoneCb *doneCb=NULL;

#define INK_STARTUP 0
#define INK_VSTART 1
#define INK_HSEND 2
#define INK_VSTOP 3
#define INK_PAUSED 4
#define INK_RESUME 5
static int einkState=0;
static int einkYpos;
static int einkPat=0;

static char *bmBuff;
static char *bmRpos, *bmWpos;
static int bmfifolen;

#define WMARK_RESTART (20000)
#define WMARK_UNPLUG (1024)
static int plugged=0;
static char dataEnded;

static int ICACHE_FLASH_ATTR fifoLen() {
	int l=bmWpos-bmRpos;
	if (l<0) l+=bmfifolen;
	return l;
}


static void ICACHE_FLASH_ATTR einkTimerCb(void *arg) {
	int x;
	//Lookup table. In binary: index abcd -> data 0a0b0c0d
	char cmdSet[]={
			0,1,4,1+4,
			16,1+16,4+16,1+4+16,
			64,1+64,4+64,1+4+64,
			16+64,1+16+64,4+16+64,1+4+16+64
		};
	if (fifoLen()<(WMARK_UNPLUG) && !dataEnded && needDataCb!=NULL) needDataCb();

	if (einkState==INK_STARTUP) {
		einkPat=0;
		ioEinkEna(1);
		os_timer_arm(&einkTimer, 100, 0);
		einkState=INK_VSTART;
		REG_SET_BIT(0x3ff00014, BIT(0));
		os_update_cpu_frequency(160);
	} else if (einkState==INK_PAUSED) {
		if (fifoLen()>(bmfifolen-WMARK_RESTART) || dataEnded) {
			//Wake up e-ink display!
			ioEinkEna(1);
			os_timer_arm(&einkTimer, 100, 0);
			REG_SET_BIT(0x3ff00014, BIT(0));
			os_update_cpu_frequency(160);
			einkState=INK_RESUME;
		} else {
			//Keep sleeping
			os_timer_arm(&einkTimer, 100, 0);
		}
	} else if (einkState==INK_RESUME) {
		//Eink has just been powered on to resume writing. Move back to current line.
		ioEinkVscanStart();
		if (einkYpos!=0) {
			ioEinkWrite(0);
			ioEinkClk(800/4);
			ioEinkVscanWrite(15);
			//The code above seems to write _2_ lines, that's why we start x at 2. Strange.
			for (x=2; x<einkYpos; x++) {
				ioEinkHscanStart();
				ioEinkWrite(0xff);
				ioEinkClk(800/4);
				ioEinkHscanStop();
				ioEinkVscanWrite(0);
			}
			//derp
			ioEinkWrite(0);
			ioEinkClk(800/4);
		}
		einkState=INK_HSEND;
		os_timer_arm(&einkTimer, 10, 0);
	} else if (einkState==INK_VSTART) {
		ioEinkVscanStart();
		einkYpos=0;
		einkState=INK_HSEND;
		os_timer_arm(&einkTimer, 20, 0);
	} else if (einkState==INK_HSEND) {
		if (einkPat<4) {
			os_timer_arm(&einkTimer, 0, 0);
			ioEinkHscanStart();
	
			if (einkPat==0 || einkPat==1) {
				ioEinkWrite(0x55);
				ioEinkClk(800/4);
			}
			if (einkPat==2 || einkPat==3) {
				ioEinkWrite(0xaa);
				ioEinkClk(800/4);
			}
			ioEinkHscanStop();
			ioEinkVscanWrite(15);
			einkYpos++;
			if (einkYpos==600) {
				einkState=INK_VSTOP;
			}
		} else {
			if (plugged && fifoLen()<WMARK_UNPLUG) {
				if (!dataEnded && needDataCb!=NULL) needDataCb();
				plugged=0;
			}
			if (fifoLen()<(800/4) && !dataEnded) {
				//Fifo ran dry. Kill eink power and sleep to wait for more data.
				ioEinkWrite(0xaa);
				ioEinkClk(800/4);
//				ioEinkVscanStop();
				ioEinkEna(0);
				REG_CLR_BIT(0x3ff00014, BIT(0));
				os_update_cpu_frequency(80);
				einkState=INK_PAUSED;
				os_timer_arm(&einkTimer, 20, 0);
			} else {
				//We can draw stuf. Reschedule ASAP please.
				os_timer_arm(&einkTimer, 0, 0);
				if (fifoLen()<(WMARK_UNPLUG) && !dataEnded && needDataCb!=NULL) needDataCb();
				ioEinkHscanStart();

				for (x=0; x<800; x+=8) {
					ioEinkWrite(cmdSet[(*bmRpos)>>4]);
					ioEinkWrite(cmdSet[(*bmRpos++)&0xf]);
					if (bmRpos>&bmBuff[bmfifolen-1]) bmRpos=bmBuff;
				}
				ioEinkHscanStop();
				ioEinkVscanWrite(115);
				einkYpos++;
				if (einkYpos==600) {
					einkState=INK_VSTOP;
				}
			}
		}

	} else if (einkState==INK_VSTOP) {
		//Skip any remaining lines
		ioEinkVscanStop();
		if (einkPat==4) {
			einkState=INK_STARTUP;
			ioEinkEna(0);
			os_free(bmBuff);
			if (doneCb!=NULL) doneCb();
		} else {
			einkPat++;
			einkState=INK_VSTART;
			os_timer_arm(&einkTimer, 10, 0);
		}
	}
}

int einkPushPixels(char d) {
	*bmWpos++=d;
	if (bmWpos>&bmBuff[bmfifolen-1]) {
		bmWpos=bmBuff;
	}
	if (bmWpos==bmRpos) {
		os_printf("fifo: OVERFLOW!\n");
	}
	return fifoLen();
}

void einkDataEnd() {
	dataEnded=1;
}

void einkDisplay(int fifosz, EinkNeedDataCb ncb, EinkDoneCb dcb) {
	needDataCb=ncb;
	doneCb=dcb;
	dataEnded=0;
	bmBuff=os_malloc(fifosz);
	bmRpos=bmBuff;
	bmWpos=bmBuff;
	bmfifolen=fifosz;
	os_timer_disarm(&einkTimer);
	os_timer_setfn(&einkTimer, einkTimerCb, NULL);
	os_timer_arm(&einkTimer, 10, 0);
}
