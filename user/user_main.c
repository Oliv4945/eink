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

void user_init(void);
static ETSTimer wdtTimer;
void sleepmode();

#define TCPSTATE_CLOSED 0
#define TCPSTATE_HEADER 1
#define TCPSTATE_DATA 2
int tcpConnState;

static void ICACHE_FLASH_ATTR wdtTimerCb(void *arg) {
	os_printf("Wdt. This takes too long. Go to sleep.\n");
	ioEinkEna(0);
	sleepmode();
}


void httpclientCb(char* data, int len) {
	int x;
	int bufLeft=65535;
	if (len==0) {
		tcpConnState=TCPSTATE_CLOSED;
	}
	for (x=0; x<len; x++) {
		if (tcpConnState==TCPSTATE_HEADER) {
			if (data[x]==0) tcpConnState=TCPSTATE_DATA;
		} else if (tcpConnState==TCPSTATE_DATA) {
			bufLeft=einkPushPixels(data[x]);
		}
	}
	if (bufLeft<(1450*5)) espconn_recv_hold(httpclientGetConn());
}

void tcpEinkNeedData() {
	espconn_recv_unhold(httpclientGetConn());
}


static struct station_config stconf;

void sleepmode() {
#ifdef FIRST
	ioEinkEna(1);
#else
//	wifi_set_sleep_type(MODEM_SLEEP_T);
	system_deep_sleep(60*1000*1000);
#endif
}

void einkDoneCb() {
	sleepmode();
}


HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/wifi/"},
	{"/index.tpl", cgiEspFsTemplate, tplCounter},
	{"/led.cgi", cgiLed, NULL},
	{"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/setmode.cgi", cgiWifiSetMode, NULL},
	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

#define RTC_MAGIC 0xCAFED00D

void user_init(void)
{
	int rtcmagic;
	stdoutInit();
	ioInit();

/*
	system_rtc_mem_read(0, &rtcmagic, 4);
	if (rtcmagic!=RTC_MAGIC) {
		rtcmagic=RTC_MAGIC;
		system_rtc_mem_write(0, &rtcmagic, 4);
		//Magic word has fallen out of the RTC. Probably means the battery has been changed or
		//taken out. Go into reconfig mode.
		wifi_set_opmode(2);
		httpdInit(builtInUrls, 80);
	}

*/
//	wifi_set_opmode(1);

	tcpConnState=TCPSTATE_HEADER;
	httpclientFetch("meuk.spritesserver.nl", "/espbm.php", 1024, httpclientCb);

	einkDisplay(tcpEinkNeedData, einkDoneCb);

	os_timer_disarm(&wdtTimer);
	os_timer_setfn(&wdtTimer, wdtTimerCb, NULL);
	os_timer_arm(&wdtTimer, 20000, 0); //shut down after 15 seconds
}
