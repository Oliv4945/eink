//Shitty http client thing

#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "httpdclient.h"

#define STATE_HDR 0
#define STATE_BODY 1
#define STATE_REDIR 2
static char hdr[1024];
static int state;
static int outpos;
static char host[128];
static int port;
static char path[512];
void (*callback)(char *data, int len);

static struct espconn conn;

static void ICACHE_FLASH_ATTR httpServerFoundCb(const char *name, ip_addr_t *ip, void *arg);

static void ICACHE_FLASH_ATTR parseIntoFields(char *url) {
	int x, i=0;

	port=80;
	//Parse http://host[:port]/bla into host, port, path
	for (x=7; url[x]!=0 && url[x]!='/' && url[x]!=':'; x++) host[i++]=url[x];
	host[i]=0;
	if (url[x]==':') {
		x++;
		port=atoi(&url[x]);
		while (url[x]!='/' && url[x]!=0) x++;
	}
	strcpy(path, &url[x]);
}

static void ICACHE_FLASH_ATTR httpclientParseChar(struct espconn *conn, char c) {
	static char last[4];
	int x;
	if (state==STATE_HDR) {
		last[0]=last[1];
		last[1]=last[2];
		last[2]=last[3];
		last[3]=c;
		if (last[0]=='\r' && last[1]=='\n' && last[2]=='\r' && last[3]=='\n') {
			state=STATE_BODY;
			return;
		}
		if (c=='\r' || c=='\n') {
			os_printf("Header %s\n", hdr);
			if (os_strncmp(hdr, "Location:", 9)==0) {
				x=9;
				while (hdr[x]!='h' && hdr[x]!=0) x++;
				os_printf("Following redirect to %s\n", &hdr[x]);
				espconn_disconnect(conn);
				parseIntoFields(&hdr[x]);
				state=STATE_REDIR;
				return;
			}
			hdr[0]=0;
		} else {
			x=os_strlen(hdr);
			hdr[x]=c;
			hdr[x+1]=0;
		}
	}
}

static void ICACHE_FLASH_ATTR ircRecvCb(void *arg, char *data, unsigned short len) {
	struct espconn *conn=(struct espconn *)arg;
	int x;
	if (len==0) return;
	if (state==STATE_HDR) {
		for (x=0; x<len; x++) {
			httpclientParseChar(conn, data[x]);
			if (state==STATE_BODY) break;
			if (state==STATE_REDIR) {
				return;
			}
		}
		//Do the callback on the remaining data.
		if (x!=len) {
			x++;
			callback(data+x, len-x);
		}
	} else {
		callback(data, len);
	}
}

static void ICACHE_FLASH_ATTR httpclientConnectedCb(void *arg) {
	struct espconn *conn=(struct espconn *)arg;
	char buff[1024];
	espconn_regist_recvcb(conn, ircRecvCb);
	os_printf("Connected.\n");
	os_sprintf(buff, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
	os_printf("httpclient: %s", buff);
	espconn_sent(conn, (unsigned char *)buff, os_strlen(buff));
	state=STATE_HDR;
}


static void ICACHE_FLASH_ATTR httpclientDisconCb(void *arg) {
	static ip_addr_t ip;
	if (state==STATE_REDIR) {
		espconn_gethostbyname(&conn, host, &ip, httpServerFoundCb);
		return;
	}
	callback(NULL, 0);
//	os_printf("Discon. Got %s\n", out);
}


static void ICACHE_FLASH_ATTR httpServerFoundCb(const char *name, ip_addr_t *ip, void *arg) {
	static esp_tcp tcp;
	struct espconn *conn=(struct espconn *)arg;
	if (ip==NULL) {
		os_printf("Huh? Nslookup of server failed :/\n");
	}
	os_printf("httpclient: found ip. Connecting to host %s port %d path %s\n", host, port, path);

	conn->type=ESPCONN_TCP;
	conn->state=ESPCONN_NONE;
	conn->proto.tcp=&tcp;
	conn->proto.tcp->local_port=espconn_port();
	conn->proto.tcp->remote_port=port;
	os_memcpy(conn->proto.tcp->remote_ip, &ip->addr, 4);

	espconn_regist_connectcb(conn, httpclientConnectedCb);
	espconn_regist_disconcb(conn, httpclientDisconCb);
	state=STATE_HDR;
	outpos=0;
	hdr[0]=0;
	espconn_connect(conn);
}


struct espconn ICACHE_FLASH_ATTR *httpclientGetConn() {
	return &conn;
}


void ICACHE_FLASH_ATTR httpclientFetch(char *url, void (*cb)(char*, int)) {
	static ip_addr_t ip;
	parseIntoFields(url);
	callback=cb;
	espconn_gethostbyname(&conn, host, &ip, httpServerFoundCb);
}
