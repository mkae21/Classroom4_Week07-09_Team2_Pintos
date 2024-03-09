#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
/* 카운팅 세마포어입니다. */
struct semaphore
{
	unsigned value;		 /* Current value. */
						 /* 현재 값입니다. */
	struct list waiters; /* List of waiting threads. */
						 /* 대기 스레드 목록. */
};

void sema_init(struct semaphore *, unsigned value);
void sema_down(struct semaphore *);
bool sema_try_down(struct semaphore *);
void sema_up(struct semaphore *);
void sema_self_test(void);

/* Lock. */
struct lock
{
	struct thread *holder;		/* Thread holding lock (for debugging). */
								/* 스레드 홀딩 락 (디버깅용). */
	struct semaphore semaphore; /* Binary semaphore controlling access. */
								/* 바이너리 세마포어로 액세스를 제어합니다. */
};

void lock_init(struct lock *);
void lock_acquire(struct lock *);
bool lock_try_acquire(struct lock *);
void lock_release(struct lock *);
bool lock_held_by_current_thread(const struct lock *);

/* Condition variable. */
/* 조건 변수. */
struct condition
{
	struct list waiters; /* List of waiting threads. */
						 /* 대기 스레드 목록. */
};
bool cond_priority(const struct list_elem *a, const struct list_elem  *b, void *aux); 
void cond_init(struct condition *);
void cond_wait(struct condition *, struct lock *);
void cond_signal(struct condition *, struct lock *);
void cond_broadcast(struct condition *, struct lock *);

/* Optimization barrier.
 *
 * The compiler will not reorder operations across an
 * optimization barrier.  See "Optimization Barriers" in the
 * reference guide for more information.*/
/* 최적화 배리어.
 *
 * 컴파일러는 최적화 배리어를 가로질러 연산의 순서를 바꾸지 않습니다.
 * 자세한 내용은 참조 가이드의 "최적화 배리어"을 참조하세요.*/
#define barrier() asm volatile("" : : : "memory")

#endif /* threads/synch.h */
