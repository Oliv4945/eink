#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include "driver/timer.h"


timer_callback_t TimerCallbackFunc;
LOCAL uint16_t time;

//XXX: 0xffffffff/(80000000/16)=35A
#define US_TO_RTC_TIMER_TICKS(t)          \
    ((t) ?                                   \
     (((t) > 0x35A) ?                   \
      (((t)>>2) * ((APB_CLK_FREQ>>4)/250000) + ((t)&0x3) * ((APB_CLK_FREQ>>4)/1000000))  :    \
      (((t) *(APB_CLK_FREQ>>4)) / 1000000)) :    \
     0)

//FRC1
#define FRC1_ENABLE_TIMER  BIT7

typedef enum {
    DIVDED_BY_1 = 0,
    DIVDED_BY_16 = 4,
    DIVDED_BY_256 = 8,
} TIMER_PREDIVED_MODE;

typedef enum {
    TM_LEVEL_INT = 1,
    TM_EDGE_INT   = 0,
} TIMER_INT_MODE;


void ICACHE_FLASH_ATTR timer_start(void)
{
  RTC_REG_WRITE(FRC1_LOAD_ADDRESS, US_TO_RTC_TIMER_TICKS(time));
}


//todo place some sanity checks here , can't arse myself to figure out the limits...
void ICACHE_FLASH_ATTR timer_set_freq(uint16 time)
{
    if (time > 50000) {
        time = 50000;
    } else if (time < 1) {
        time = 1;
    } else {
        time = time;
    }
    //hz or period??
    //time = PWM_1S / time;
}


// the ISR is placed in SRAM
LOCAL void timer_tim1_intr_handler( void )
{

    //first we clear the IRQ, reload the timer and then we call the callback
    RTC_CLR_REG_MASK(FRC1_INT_ADDRESS, FRC1_INT_CLR_MASK);
    RTC_REG_WRITE(FRC1_LOAD_ADDRESS,US_TO_RTC_TIMER_TICKS(time));
    TimerCallbackFunc();
}


void ICACHE_FLASH_ATTR timer_init(uint16_t freq,timer_callback_t pFunc)
{

    ETS_FRC_TIMER1_INTR_ATTACH(timer_tim1_intr_handler, NULL);
    TM1_EDGE_INT_ENABLE();
    ETS_FRC1_INTR_ENABLE();

    RTC_CLR_REG_MASK(FRC1_INT_ADDRESS, FRC1_INT_CLR_MASK);
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS,  //FRC2_AUTO_RELOAD|  //reload done in the ISR
                  DIVDED_BY_16
                  | FRC1_ENABLE_TIMER
                  | TM_EDGE_INT);
    RTC_REG_WRITE(FRC1_LOAD_ADDRESS, 0);

    //register the callback
    TimerCallbackFunc = pFunc;

    time = freq;
    //timer_set_freq(freq);
    timer_start();
}
