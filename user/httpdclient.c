#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"

#define STATE_HDR 0
#define STATE_BODY 1

static char hdr[1024];
static char out[1024];
static int state;
static int outpos;
void (*callback)(char *out);

static void ICACHE_FLASH_ATTR httpclientParseChar(struct espconn *conn, char c) {
	static char last[4];
	if (state==STATE_HDR) {
		last[0]=last[1];
		last[1]=last[2];
		last[2]=last[3];
		last[3]=c;
		if (last[0]=='\r' && last[1]=='\n' && last[2]=='\r' && last[3]=='\n') state=STATE_BODY;
	} else {
		last[3]=0;
		if (outpos<(sizeof(out)-1)) {
			out[outpos++]=c;
			out[outpos]=0;
		}
	}
}

static void ICACHE_FLASH_ATTR ircRecvCb(void *arg, char *data, unsigned short len) {
	struct espconn *conn=(struct espconn *)arg;
	int x;
	for (x=0; x<len; x++) httpclientParseChar(conn, data[x]);
}

static void ICACHE_FLASH_ATTR httpclientConnectedCb(void *arg) {
	struct espconn *conn=(struct espconn *)arg;
	espconn_regist_recvcb(conn, ircRecvCb);
	os_printf("Connected.\n");
	espconn_sent(conn, (unsigned char *)hdr, os_strlen(hdr));
	state=STATE_HDR;
}


static void ICACHE_FLASH_ATTR httpclientDisconCb(void *arg) {
	callback(out);
	os_printf("Discon. Got %s\n", out);
}

static void ICACHE_FLASH_ATTR httpServerFoundCb(const char *name, ip_addr_t *ip, void *arg) {
	static esp_tcp tcp;
	struct espconn *conn=(struct espconn *)arg;
	if (ip==NULL) {
		os_printf("Huh? Nslookup of irc server failed :/ Trying again...\n");
	}
	os_printf("httpclient: found ip\n");

	conn->type=ESPCONN_TCP;
	conn->state=ESPCONN_NONE;
	conn->proto.tcp=&tcp;
	conn->proto.tcp->local_port=espconn_port();
	conn->proto.tcp->remote_port=80;
	os_memcpy(conn->proto.tcp->remote_ip, &ip->addr, 4);

	espconn_regist_connectcb(conn, httpclientConnectedCb);
	espconn_regist_disconcb(conn, httpclientDisconCb);
	state=STATE_HDR;
	outpos=0;
	espconn_connect(conn);
}

void httpclientFetch(char *hcserver, char *hcloc, int retbufsz, void (*cb)(char*)) {
	static struct espconn conn;
	static ip_addr_t ip;
	callback=cb;
	os_sprintf(hdr, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", hcloc, hcserver);
	os_printf("httpclient: %s", hdr);
	espconn_gethostbyname(&conn, hcserver, &ip, httpServerFoundCb);
}
