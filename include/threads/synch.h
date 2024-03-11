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

// TODO: Modify to insert thread at waiters list in order of priority.
// TODO: 우선순위에 따라 `waiters` 리스트에 스레드를 삽입하도록 수정합니다.
void sema_down(struct semaphore *);

bool sema_try_down(struct semaphore *);

// TODO: Sort the waiters list in order of priority
// -> It is consider the case of changing priority of threads in waiters list.
// TODO: 우선순위에 따라 `waiters` 리스트를 정렬합니다.
// -> 대기열에 있는 스레드의 우선순위를 변경하는 경우를 생각해봅시다.
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

// TODO: If the lock is not available, store address of the lock.
// Store the current priority and maintain donated threads on list(multiple donation).
// Donate priority.

// TODO: 락을 사용할 수 없는 경우, 락의 주소를 저장합니다.
// 현재 우선순위를 저장하고 기부된 스레드를 리스트에 유지합니다(multiple donation).
// 우선순위를 기부합니다.
void lock_acquire(struct lock *);

bool lock_try_acquire(struct lock *);

// TODO: When the lock is released, remove the thread that holds the lock on donation list and set priority properly.
// TODO: 잠금이 해제되면 donation list에서 락이 걸려있는 스레드를 제거하고 우선순위를 올바르게 설정합니다.
void lock_release(struct lock *);

bool lock_held_by_current_thread(const struct lock *);

/* Condition variable. */
/* 조건 변수. */
struct condition
{
	struct list waiters; /* List of waiting threads. */
						 /* 대기 스레드 목록. */
};

// Initialize the condition variable data structure.
// 조건 변수 자료 구조를 초기화합니다.
void cond_init(struct condition *);

// Wait for signal by the condition variable.
// TODO: Modify to insert thread at waiters list in order of priority.
// 조건 변수의 신호를 기다립니다.
// TODO: 우선순위에 따라 `waiters` 리스트에 스레드를 삽입하도록 수정합니다.
void cond_wait(struct condition *, struct lock *);

// Send a signal to thread of the highest priority waiting in the condition variable.
// TODO: Sort the waiters list in order of priority.
// -> It is consider the case of changing priority of threads in waiters list.
// 조건 변수에서 대기 중인 우선순위가 가장 높은 스레드에 신호를 보냅니다.
// TODO: 우선순위에 따라 `waiters` 리스트를 정렬합니다.
// -> 대기열에 있는 스레드의 우선순위를 변경하는 경우를 생각해봅시다.
void cond_signal(struct condition *, struct lock *);

// Send a signal to all threads waiting in the condition variable.
// 조건 변수에 대기 중인 모든 스레드에 신호를 보냅니다.
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
