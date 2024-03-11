#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>

/* Number of timer interrupts per second. */
/* 초당 타이머 인터럽트 횟수. */
#define TIMER_FREQ 100

void timer_init(void);
void timer_calibrate(void);

int64_t timer_ticks(void);
int64_t timer_elapsed(int64_t);

void timer_sleep(int64_t ticks);
void timer_msleep(int64_t milliseconds);
void timer_usleep(int64_t microseconds);
void timer_nsleep(int64_t nanoseconds);

void timer_print_stats(void);

// Design tip for modularization

// Functions to add
// 1. The function that sets thread state to blocked and wait after insert it to sleep queue.
// 2. The function that find the thread to wake up from sleep queue and wake up it.
// 3. The function that save the minimum value of tick that threads have.
// 4. The function that return the minimum value of tick.

// 모듈화를 위한 설계 팁

// 추가할 함수
// 1. 스레드 상태를 blocked로 설정하고 sleep queue에 삽입한 후 대기하는 함수
// 2. sleep queue에서 깨울 스레드를 찾아서 깨우는 함수
// 3. 스레드가 가지고 있는 틱의 최소값을 저장하는 함수
// 4. 틱의 최소값을 반환하는 함수

#endif /* devices/timer.h */