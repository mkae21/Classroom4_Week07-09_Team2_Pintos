#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"
/* See [8254] for hardware details of the 8254 timer chip. */
/* 8254 타이머 칩의 하드웨어 세부 정보는 [8254]를 참조하세요. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
/* OS 부팅 이후 타이머 틱 횟수를 저장한 전역 변수입니다. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
/* 타이머 틱당 루프 수입니다.
   timer_calibrate()에 의해 초기화됩니다. */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops(unsigned loops);
static void busy_wait(int64_t loops);
static void real_time_sleep(int64_t num, int32_t denom);

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
/* 8254 프로그래머블 인터벌 타이머(PIT)를
   초당 PIT_FREQ 횟수만큼 인터럽트를 발생시키고
   해당 인터럽트를 등록합니다. */
void timer_init(void)
{
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest. */
	/* 8254 입력 주파수를 TIMER_FREQ로 나눈 값으로
	   가장 가까운 값으로 반올림합니다. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ; // 인터럽트 주기 생성

	outb(0x43, 0x34); /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb(0x40, count & 0xff);
	outb(0x40, count >> 8);

  // 8254 타이머 인터럽트를 0x20번째 벡터에 등록, 
	// timer_interrupt는 타이머 인터럽트를 처리하는 함수
	intr_register_ext(0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
/* 짧은 지연을 구현하는 데 사용되는 loops_per_tick 타이머를 보정합니다. */
void timer_calibrate(void)
{
	unsigned high_bit, test_bit;

	ASSERT(intr_get_level() == INTR_ON);

	// printf("Calibrating timer...  \n");
	printf("타이머 보정...  \n");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
  /* loops_per_tick을 타이머 틱보다 작으면서 
     2의 최대 거듭제곱 값으로 근사합니다. */

	loops_per_tick = 1u << 10;

	while (!too_many_loops(loops_per_tick << 1))
	{
		loops_per_tick <<= 1;
		ASSERT(loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	/* 다음 8비트의 loops_per_tick을 세분화합니다. */
	high_bit = loops_per_tick;

	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops(high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf("%'" PRIu64 " loops/s.\n", (uint64_t)loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
/* OS 부팅 이후 타이머 틱 횟수를 반환합니다. */
int64_t timer_ticks(void)
{
  // interrupt 불가능하게 만들고, old_level에 이전의 상태 넣기
	enum intr_level old_level = intr_disable();
  // t에 전역변수 tick 값 넣기
	int64_t t = ticks;
  // old_level의 상태에 맞춰서 설정
	intr_set_level(old_level);
	barrier();
  // tick 값 반환
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
/* THEN 이후 경과한 타이머 틱 수를 반환하며,
   이 값은 timer_ticks()가 반환한 값이어야 합니다. */
int64_t timer_elapsed(int64_t then)
{
  // 전역 변수 빼기 - 입력받은 시간
	return timer_ticks() - then;
}

/* Suspends execution for approximately TICKS timer ticks. */
/* 약 "TICKS" 타이머 틱 동안 실행을 일시 중단합니다. */
void timer_sleep(int64_t ticks)
{
  // start에 전역 변수 tick값 저장(시작시간 기록)
  // 인터럽트 disable하게
	int64_t start = timer_ticks();
  
  // 인터럽터 켜져 있으면 alert
  // 인터럽트가 켜져 있으면 핸들링하는 동안 동기화 오류 발생
	ASSERT(intr_get_level() == INTR_ON);
  
  //ticks - start < ticks --> 0 < ticks
	while (timer_elapsed(start) < ticks)
    // running thread를 ready_list 맨 뒤로 보내기
		thread_yield();
}

/* Suspends execution for approximately MS milliseconds. */
/* 약 밀리초 동안 실행을 일시 중단합니다. */
void timer_msleep(int64_t ms)
{
	real_time_sleep(ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
/* 약 US 마이크로초 동안 실행을 일시 중단합니다. */
void timer_usleep(int64_t us)
{
	real_time_sleep(us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
/* 약 NS 나노초 동안 실행을 일시 중단합니다. */
void timer_nsleep(int64_t ns)
{
	real_time_sleep(ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
/* 타이머 통계를 출력합니다. */
void timer_print_stats(void)
{
	printf("Timer: %" PRId64 " ticks\n", timer_ticks());
}

/* Timer interrupt handler. */
/* 타이머 인터럽트 핸들러. */
static void timer_interrupt(struct intr_frame *args UNUSED)
{
	ticks++;
	thread_tick();
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
/* 루프 반복이 타이머 틱을 두 번 이상 기다리면 참을 반환하고,
   그렇지 않으면 거짓을 반환합니다. */
static bool too_many_loops(unsigned loops)
{
	/* Wait for a timer tick. */
	/* 타이머 틱을 기다립니다. */
	int64_t start = ticks;
	while (ticks == start)
		barrier();

	/* Run LOOPS loops. */
	/* LOOPS 루프를 실행합니다. */
	start = ticks;
	busy_wait(loops);

	/* If the tick count changed, we iterated too long. */
	/* 틱 수가 변경되면 너무 오래 반복한 것입니다. */
	barrier();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
/* 짧은 지연을 구현하기 위해 간단한 루프 LOOPS 횟수를 반복합니다.

   코드 정렬이 타이밍에 큰 영향을 미칠 수 있으므로 이 함수의 인라인이
   다른 위치에 다르게 정렬되면 결과를 예측하기 어렵기 때문에
   NO_INLINE으로 표시했습니다. */
static void NO_INLINE busy_wait(int64_t loops)
{
	while (loops-- > 0)
		barrier();
}

/* Sleep for approximately NUM/DENOM seconds. */
/* 약 NUM/DENOM 초 동안 절전합니다. */
static void real_time_sleep(int64_t num, int32_t denom)
{
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	/* NUM/DENOM 초를 타이머 틱으로 변환하며 반내림합니다.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM 틱.
	   1초 / TIMER_FREQ 틱
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT(intr_get_level() == INTR_ON);

	if (ticks > 0)
	{
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		/* 최소 한 번의 전체 타이머 틱을 기다리는 중입니다.
		   사용 timer_sleep()을 사용하면 다른 프로세스에 CPU를
		   양보하게 되므로 프로세스에 양보할 수 있기 때문입니다. */
		timer_sleep(ticks);
	}
	else
	{
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		/* 그렇지 않으면 보다 정확한 하위 틱 타이밍을 위해
		   바쁜 대기 루프를 사용합니다. 오버플로우 가능성을 피하기 위해
		   분자와 분모를 1000으로 축소합니다. */
		ASSERT(denom % 1000 == 0);

		busy_wait(loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}