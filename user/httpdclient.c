#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"

#define STATE_HDR 0
#define STATE_BODY 1

static char hdr[1024];
static int state;
static int outpos;
void (*callback)(char *data, int len);

static void ICACHE_FLASH_ATTR httpclientParseChar(struct espconn *conn, char c) {
	static char last[4];
	if (state==STATE_HDR) {
		last[0]=last[1];
		last[1]=last[2];
		last[2]=last[3];
		last[3]=c;
		if (last[0]=='\r' && last[1]=='\n' && last[2]=='\r' && last[3]=='\n') state=STATE_BODY;
	}
}

static void ICACHE_FLASH_ATTR ircRecvCb(void *arg, char *data, unsigned short len) {
	struct espconn *conn=(struct espconn *)arg;
	int x;
	if (state==STATE_HDR) {
		for (x=0; x<len; x++) {
			httpclientParseChar(conn, data[x]);
			if (state==STATE_BODY) break;
		}
		//Do the callback on the remaining data.
		if (x!=len) callback(data+x, len-x);
	} else {
		callback(data, len);
	}
}

static void ICACHE_FLASH_ATTR httpclientConnectedCb(void *arg) {
	struct espconn *conn=(struct espconn *)arg;
	espconn_regist_recvcb(conn, ircRecvCb);
	os_printf("Connected.\n");
	espconn_sent(conn, (unsigned char *)hdr, os_strlen(hdr));
	state=STATE_HDR;
}


static void ICACHE_FLASH_ATTR httpclientDisconCb(void *arg) {
	callback(NULL, 0);
//	os_printf("Discon. Got %s\n", out);
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

	static struct espconn conn;

ICACHE_FLASH_ATTR struct espconn *httpclientGetConn() {
	return &conn;
}

void httpclientFetch(char *hcserver, char *hcloc, int retbufsz, void (*cb)(char*, int)) {
	static ip_addr_t ip;
	callback=cb;
	os_sprintf(hdr, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", hcloc, hcserver);
	os_printf("httpclient: %s", hdr);
	espconn_gethostbyname(&conn, hcserver, &ip, httpServerFoundCb);
}
