void httpclientFetch(char *url, void (*cb)(char*, int), void(*hcb)(char*));
struct espconn *httpclientGetConn();
