
typedef struct {
	char chsum;
	char pad[3];
	char url[1024];
} MyConfig;

extern MyConfig myConfig;
void configLoad();
void configSave();