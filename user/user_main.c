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

//For first run: set ESSID and STA mode, no sleep so Vcomm can be adjusted

//#define FIRST

void user_init(void);

static ETSTimer wdtTimer;

void httpclientCb(char* data, int len) {
	int x;
	int bufLeft;
//	os_printf("Data: %d bytes\n", len);
	if (len==0) {
		tcpConnState=TCPSTATE_CLOSED;
	}
	for (x=0; x<len; x++) {
		if (tcpConnState==TCPSTATE_HEADER) {
			if (data[x]==0) tcpConnState=TCPSTATE_DATA;
		} else if (tcpConnState==TCPSTATE_DATA) {
			bufLeft=einkPushPixels();
		}
	}
}

void tcpEinkNeedData() {
	
}


static void ICACHE_FLASH_ATTR wdtTimerCb(void *arg) {
	os_printf("Wdt. This takes too long. Go to sleep.\n");
	ioEinkEna(0);
	sleepmode();
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

	EinkNeedDataCb *cb

	system_rtc_mem_read(0, &rtcmagic, 4);
	if (rtcmagic!=RTC_MAGIC) {
		rtcmagic=RTC_MAGIC;
		system_rtc_mem_write(0, &rtcmagic, 4);
		//Magic word has fallen out of the RTC. Probably means the battery has been changed or
		//taken out. Go into reconfig mode.
		wifi_set_opmode(2);
		httpdInit(builtInUrls, 80);
	}

#ifdef FIRST
	//Temp store for new ap info.
	static struct station_config stconf;
	os_strncpy((char*)stconf.ssid, "Sprite", 32);
	os_strncpy((char*)stconf.password, "", 64);
	wifi_station_disconnect();
	wifi_station_set_config(&stconf);
	wifi_station_connect();
	wifi_set_opmode(1);
#endif

	tcpConnState=TCPSTATE_HEADER;
	httpclientFetch("meuk.spritesserver.nl", "/espbm.php", 1024, httpclientCb);

	einkDisplay(tcpEinkNeedData, einkDoneCb);

	os_timer_disarm(&wdtTimer);
	os_timer_setfn(&wdtTimer, wdtTimerCb, NULL);
	os_timer_arm(&wdtTimer, 20000, 0); //shut down after 15 seconds
}
