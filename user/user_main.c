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
#include "eink.h"
#include "httpd.h"
#include "cgiwifi.h"
#include "httpdespfs.h"
#include "espfs.h"
#include "config.h"

void user_init(void);
static ETSTimer wdtTimer;
void sleepmode();

#define TCPSTATE_CLOSED 0
#define TCPSTATE_HEADER 1
#define TCPSTATE_DATA 2
int tcpConnState;


//This is used to make sure we use the espconn_recv_[un]hold calls only
//once. The stack trips up when called multiple times.
char tcpPlugged=0;

struct __attribute__ ((__packed__)) EinkHeader {
	uint8_t ver;			//Header version. Only v1 is supported atm.
	uint16_t sleeptime;		//sleep time, in seconds
	uint8_t bmtype;			//bitmap type. Only 0 (1bpp, full refresh) is supported for now. Ignored.
} einkHeader;
int einkHeaderPos;
char einkHeaderIsValid;

//Watchdog timer thing. Shuts down the device when everything takes
//too long, eg due to network outages.
static void ICACHE_FLASH_ATTR wdtTimerCb(void *arg) {
	os_printf("Wdt. This takes too long. Go to sleep.\n");
	ioEinkEna(0);
	sleepmode();
}


//Called when the http client receives something
void httpclientCb(char* data, int len) {
	int x;
	int bufLeft=65535;
	if (data==NULL) {
		//We're done here.
		os_printf("Http conn closed.\n");
		tcpConnState=TCPSTATE_CLOSED;
		einkDataEnd();
		return;
	}
	for (x=0; x<len; x++) {
		if (tcpConnState==TCPSTATE_HEADER) {
			((char*)&einkHeader)[einkHeaderPos++]=data[x];
			if (einkHeaderPos>=sizeof(einkHeader)) {
				if (einkHeader.ver==1) {
					einkHeaderIsValid=1;
				} else {
					os_printf("Don't understand header ver %d.\n", einkHeader.ver);
				}
				tcpConnState=TCPSTATE_DATA;
			}
		} else if (tcpConnState==TCPSTATE_DATA) {
			bufLeft=einkPushPixels(data[x]);
		}
	}
	if (bufLeft<(1450*5) && !tcpPlugged) {
		espconn_recv_hold(httpclientGetConn());
		tcpPlugged=1;
	}
}

void tcpEinkNeedData() {
	if (tcpPlugged) {
		espconn_recv_unhold(httpclientGetConn());
		tcpPlugged=0;
	}
}

EspFsFile *einkFile;

void fileEinkNeedData() {
	char data[128];
	int x, l;
	l=espFsRead(einkFile, data, sizeof(data));
	for (x=0; x<l; x++) {
		einkPushPixels(data[x]);
	}
	if (l<sizeof(data)) einkDataEnd();
}

void fileEinkDoneCb() {
	espFsClose(einkFile);
}

void sleepmode() {
#ifdef FIRST
	ioEinkEna(1);
#else
	if (einkHeaderIsValid) {
		//Sleep the amount of time indicated.
		system_deep_sleep(einkHeader.sleeptime*1000*1000);
	} else {
		//Default to 60sec sleep
		system_deep_sleep(60*1000*1000);
	}
#endif
}

void einkDoneCb() {
	sleepmode();
}

HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/wifi.tpl"},
	{"/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/connect.cgi", cgiWiFiConnect, NULL},
	{"/diag.cgi", cgiDiag, NULL},
	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

#define RTC_MAGIC 0xCAFED00D

void user_init(void)
{
	int rtcmagic;

	stdoutInit();
	ioInit();
	configLoad();

	system_rtc_mem_read(128, &rtcmagic, 4);
	if (rtcmagic!=RTC_MAGIC) {
		//Make sure we're in STA+AP mode
		if (wifi_get_opmode()!=3) {
			wifi_set_opmode(3);
			system_restart();
			return;
		}
		//For the next time: start in normal mode
		rtcmagic=RTC_MAGIC;
		system_rtc_mem_write(128, &rtcmagic, 4);
		//Magic word has fallen out of the RTC. Probably means the battery has been changed or
		//taken out. Go into reconfig mode.

		einkFile=espFsOpen("apconnect.bm");
		einkDisplay(2048, fileEinkNeedData, fileEinkDoneCb);

		httpdInit(builtInUrls, 80);

		os_timer_disarm(&wdtTimer);
		os_timer_setfn(&wdtTimer, wdtTimerCb, NULL);
		os_timer_arm(&wdtTimer, 120000, 0); //shut down after 120 seconds
		return;
	}
	
	if (wifi_get_opmode()!=1) {
		wifi_set_opmode(1);
		system_restart();
		return;
	}

	os_printf("Datasource %s\n", myConfig.url);
	tcpConnState=TCPSTATE_HEADER;
	einkHeaderPos=0;
	einkHeaderIsValid=0;
	httpclientFetch(myConfig.url, httpclientCb);
	einkDisplay(24*1024, tcpEinkNeedData, einkDoneCb);

	os_timer_disarm(&wdtTimer);
	os_timer_setfn(&wdtTimer, wdtTimerCb, NULL);
	os_timer_arm(&wdtTimer, 20000, 0); //shut down after 15 seconds
}
