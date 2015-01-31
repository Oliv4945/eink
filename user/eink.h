
//Called when the eink pixel buffer is running low
typedef int (*EinkNeedDataCb)();
//Called when eink is done displaying stuff
typedef int (*EinkDoneCb)();

//Push 8 bits of data into the pixel buffer. Returns amount of buffer space left.
int einkPushPixels(char pixels);
//Call to indicate end of data.
void einkDataEnd();

//Start displaying data.
void einkDisplay(EinkNeedDataCb needDataCb, EinkDoneCb doneCb);





