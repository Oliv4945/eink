#ifndef __TIMER_H__
#define __TIMER_H__


#define PWM_1S 1000000


typedef void (*timer_callback_t) (void);


void timer_init(uint16 freq,timer_callback_t pFunc);
void timer_start(void);
void timer_stop(void);

#endif

