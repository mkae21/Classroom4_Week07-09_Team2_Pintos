#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
/* 스레드 수명 주기의 상태. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. (실행 중인 스레드.) */
	THREAD_READY,	/* Not running but ready to run. (실행 중이 아니지만 실행할 준비가 되었습니다.) */
	THREAD_BLOCKED, /* Waiting for an event to trigger. (이벤트가 트리거되기를 기다리는 중입니다.) */
	THREAD_DYING	/* About to be destroyed. (곧 파괴됩니다.) */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
/* 스레드 식별자 유형.
   원하는 유형으로 재정의할 수 있습니다.*/
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */
							  /* tid_t의 오류 값입니다. */

/* 스레드 우선순위. */
#define PRI_MIN 0	   /* 우선순위가 가장 낮습니다. */
#define PRI_DEFAULT 31 /* 기본 우선순위. */
#define PRI_MAX 63	   /* 최우선 순위. */

/* 커널 스레드 또는 사용자 프로세스입니다.
 *
 * 각 스레드 구조는 자체 4KB 페이지에 저장됩니다. 스레드 구조 자체는
 * 페이지 맨 아래(오프셋 0)에 위치합니다. 페이지의 나머지 부분은 스레드의
 * 커널 스택을 위해 예약되어 있으며, 이 스택은 페이지 상단(오프셋 4kB)에서
 * 아래쪽으로 늘어납니다. 아래 그림을 참조하세요.
 *
 *      4 kB +---------------------------------+
 *           |            커널 스택            |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         아래쪽으로 성장         |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * 결론은 두 가지입니다:
 *
 *    1. 첫째, `struct thread`가 너무 커지지 않도록 해야 합니다.
 *       그렇게 되면 커널 스택을 위한 공간이 충분하지 않게 됩니다.
 *       우리의 기본 `struct thread'는 크기가 몇 바이트에 불과합니다.
 *       아마도 1KB 미만으로 유지되어야 할 것입니다.
 *
 *    2. 둘째, 커널 스택이 너무 커지지 않도록 해야 합니다. 스택이
 *       오버플로되면 스레드 상태가 손상됩니다. 따라서 커널 함수는
 *       큰 구조체나 배열을 정적이 아닌 지역 변수로 할당해서는 안
 *       됩니다. 대신 malloc() 또는 palloc_get_page()와 함께 동적
 *       할당을 사용하세요.
 *
 * 이러한 문제의 첫 번째 증상은 실행 중인 스레드의 `struct thread`의
 * `magic` 멤버가 THREAD_MAGIC으로 설정되어 있는지 확인하는
 * thread_current()의 어설션 실패일 수 있습니다. 스택 오버플로는
 * 일반적으로 이 값을 변경하여 어설션을 트리거합니다. */
/* `elem' 멤버는 두 가지 용도로 사용됩니다. 실행 대기열(thread.c)의
   엘리먼트이거나 세마포어 대기 목록(synch.c)의 엘리먼트가 될 수 있습니다.
   준비 상태의 스레드만 실행 대기열에 있는 반면, 차단 상태의 스레드만
   세마포어 대기 목록에 있기 때문에 이 두 가지 방법으로만 사용할 수 있습니다. */

/* Project 2: System Call FDT */
#define FDT_PAGES 3 // 왜 값이 3인지 THE Jungle에게 문의 필요
#define FDT_SIZE (FDT_PAGES * (1 << 9))
#define FDT_COUNT_LIMIT 128 // 파일 디스크립터 테이블의 최대 크기

struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
							   /* 스레드 식별자. */
	enum thread_status status; /* Thread state. */
							   /* 스레드 상태. */
	char name[16];			   /* Name (for debugging purposes). */
							   /* 이름 (디버깅 목적). */
	int priority;			   /* Priority. */
							   /* 우선순위. */
	int origin_priority;	   /* Initial priority. */
							   /* 초기 우선순위. */
	int64_t tick;
	int64_t wakeup_tick;

	/* Shared between thread.c and synch.c. */
	/* thread.c와 synch.c가 공유합니다. */
	struct list_elem elem;	   /* List element. */
							   /* 리스트 요소. */
	struct lock *wait_on_lock; /* Lock the thread is waiting for. */
							   /* 스레드가 기다리는 락. */
	struct list donations;	   /* List of threads that donated priority. */
							   /* 우선순위를 기부한 스레드 목록. */
	struct list_elem d_elem;   /* Donation list element. */
							   /* 기부 리스트 요소. */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	/* 소유: userprog/process.c. */
	uint64_t *pml4;					  /* Page map level 4 */
									  /* 페이지 맵 레벨 4 */
	struct list children;			  /* List of children */
									  /* 자식 목록 */
	struct list_elem child_elem;	  /* List element for child list */
									  /* 자식 목록의 리스트 요소 */
	struct semaphore duplicate_sema;  // 복제 완료를 알리기 위한 세마포어
	struct semaphore child_wait_sema; // 자식 프로세스가 종료될 때까지 대기하기 위한 세마포어
	// struct semaphore exit_sema;		   // 종료 완료를 알리기 위한 세마포어
	int exit_status;		   // 프로세스의 종료 상태
	int next_fd;			   // 다음 할당할 파일 디스크립터
	struct dir *cwd;		   // 현재 작업 디렉토리
	struct file *loading_file; // 현재 로딩 중인 파일

	// 아래 두 개의 멤버 변수는 부모 프로세스와 자식 프로세스 값이 같아야 함
	// 부모를 찾는 용도가 아닌, 데이터가 변형되지 않은 원본을 보관하기 위한 용도
	// fork가 끝나면 의미가 없어지므로, fork가 끝나면 NULL로 초기화
	struct intr_frame parent_if; // 부모 프로세스의 intr_frame
	struct file **fdt;			 // 파일 디스크립터 테이블
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	/* 스레드가 소유한 전체 가상 메모리에 대한 표입니다. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	/* 소유: thread.c. */
	struct intr_frame tf; /* Information for switching */
						  /* switching 정보 */
	unsigned magic;		  /* Detects stack overflow. */
						  /* 스택 오버플로를 감지. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
/* false(기본값)이면 라운드 로빈 스케줄러를 사용합니다.
   true이면 다단계 피드백 대기열 스케줄러를 사용합니다.
   커널 명령줄 옵션 "-o mlfqs"로 제어합니다. */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);

// Point of updates
// 1. Insert thread in ready_list in the order of priority. (note that it is not scalable)
// 2. When the thread is added to the ready_list, compare priority of new thread and priority of the current thread.
// 3. If the priority of the new thread is higher, call schedule() (the current thread yields CPU).

// 업데이트 지점
// 1. 우선순위에 따라 ready_list에 스레드를 삽입합니다. (확장 가능하지 않음에 유의하세요.)
// -> "확장 가능하지 않음"은 스레드 개수가 많아지면 list로 관리하는 것이 비효율적임을 의미
// -> list_insert_ordered가 있지만 스레드 개수가 많아지면 O(n^2)의 시간복잡도를 가짐
// 2. 스레드가 ready_list에 추가되면 새 스레드의 우선순위와 현재 스레드의 우선순위를 비교합니다.
// 3. 새 스레드의 우선순위가 더 높으면 schedule()을 호출합니다(현재 스레드에서 CPU를 산출합니다).
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);

/* 스레드의 우선순위를 비교하여 높은 순서대로 정렬하는 함수 */
bool compare_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

// When the thread is unblocked, it is inserted to `ready_list` in the priority order.
// 스레드가 차단 해제되면 우선순위에 따라 `ready_list`에 삽입됩니다.
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;

// The current thread yields CPU and it is inserted to `ready_list` in priority order.
// 현재 스레드의 CPU 사용량을 산출하여 우선순위에 따라 `ready_list`에 삽입합니다.
void thread_yield(void);

void thread_sleep(int64_t tick);

/* project 1 종료 이후 시간 여유가 되면 아래 함수로 성능 테스트 - Hyeonwoo, 2024.03.07 */
// 스레드의 wakeup_tick을 비교하여 빠른 순서대로 정렬하는 함수
// bool compare_wakeup_tick(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

// 스레드 상태를 blocked로 설정하고 sleep queue에 삽입한 후 대기하는 함수
// void thread_sleep(int64_t tick);

// sleep queue에서 깨울 스레드를 찾아서 깨우는 함수
void thread_wakeup(int64_t ticks);

int thread_get_priority(void);

// Set priority of the current thread.
// Reorder the `ready_list`.
// TODO: Set priority considering the donation.
// 현재 스레드의 우선순위를 설정합니다.
// `ready_list`의 순서를 변경합니다.
// TODO: donation을 고려하여 우선순위를 설정합니다.
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);
bool larger(const struct list_elem *a, const struct list_elem *b, void *aux);

void thread_try_yield(void);

#endif /* threads/thread.h */