#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif


#define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)
#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
	((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
		- offsetof (STRUCT, MEMBER.next)))
struct value 
  {
    struct list_elem elem;      /* List element. */
    int value;                  /* Item value. */
  };


/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
/* struct thread의 `magic` 멤버에 대한 임의의 값입니다. 스택 오버플로를
   감지하는 데 사용됩니다. 자세한 내용은 thread.h 상단의 큰 주석을 참고하세요. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
/* 기본 스레드의 임의 값
   이 값을 수정하지 마세요. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
/* 스레드_준비 상태의 프로세스, 즉 실행할 준비가 되었지만
   실제로 실행되지 않는 프로세스의 목록입니다. */
static struct list ready_list;

// sleep_list 생성
static struct list sleep_list;

/* Idle thread. */
/* 유휴 스레드. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
/* 초기 스레드, init.c:main()을 실행하는 스레드. */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
/* allocate_tid()에서 사용하는 락입니다. */
static struct lock tid_lock;

/* Thread destruction requests */
/* 스레드 파괴 요청 */
static struct list destruction_req;

/* Statistics. */
/* 통계. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
							   /* 유휴 타이머 틱 수입니다. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
							   /* 커널 스레드의 타이머 틱 수입니다. */
static long long user_ticks;   /* # of timer ticks in user programs. */
							   /* 사용자 프로그램의 타이머 틱 수입니다. */

/* Scheduling. */
/* 스케줄링. */
#define TIME_SLICE 4		  /* # of timer ticks to give each thread. */
							  /* 각 스레드에 부여할 타이머 틱 수입니다. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */
							  /* 마지막 yield 이후 타이머 틱 횟수입니다. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
/* false(기본값)이면 라운드 로빈 스케줄러를 사용합니다.
   true이면 다단계 피드백 대기열 스케줄러를 사용합니다.
   커널 명령줄 옵션 "-o mlfqs"로 제어합니다. */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);

/* Returns true if T appears to point to a valid thread. */
/* T가 유효한 스레드를 가리키는 것처럼 보이면 true를 반환합니다. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
/* 실행 중인 스레드를 반환합니다.
 * CPU의 스택 포인터 'rsp'를 읽은 다음 해당 포인터를 페이지의
   시작 지점으로 내림으로 처리합니다. 'struct thread'가 항상
   페이지의 시작 부분에 있고 스택 포인터는 중간 어딘가에 위치하기
   때문에 이는 현재 스레드를 찾습니다. */
#define running_thread() ((struct thread *)(pg_round_down(rrsp())))

// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
// thread_start에 대한 전역 설명자 테이블입니다.
// gdt는 thread_init 이후에 설정되므로 임시 gdt를 먼저 설정해야 합니다.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */


bool local_tick(const struct list_elem *a,const struct list_elem *b,void *aux){
    struct thread *A = list_entry(a , struct thread , elem);
    struct thread *B = list_entry(b , struct thread , elem);

    if (A->tick <= B->tick){
        return true;
    }
    else return false;

}
bool larger(const struct list_elem *a,const struct list_elem *b,void *aux){
    struct thread *A = list_entry(a , struct thread , elem);
    struct thread *B = list_entry(b , struct thread , elem);

    if (A->priority >= B->priority){
        return true;
    }
    else return false;

}
/* 현재 실행 중인 코드를 스레드로 변환하여 스레딩 시스템을 초기화합니다.
   일반적으로는 작동하지 않으며 이 경우에만 가능한 이유는 loader.S가
   스택의 맨 아래를 페이지 경계에 배치하도록 주의했기 때문입니다.

   또한 실행 대기열과 tid 락을 초기화합니다.

   이 함수를 호출한 후에는 thread_create()로 스레드를 생성하기 전에
   반드시 페이지 할당자를 초기화해야 합니다.

   이 함수가 완료될 때까지 thread_current()를 호출하는 것은
   안전하지 않습니다.*/
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);


	/* Reload the temporal gdt for the kernel, (gdt 는 global descriptor table)
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	/* 커널에 대한 임시 gdt를 다시 로드합니다.
	   이 gdt에는 사용자 컨텍스트가 포함되지 않습니다.
	   커널은 gdt_init()에서 사용자 컨텍스트가 포함된 gdt를 다시 빌드합니다. */
	struct desc_ptr gdt_ds = {
		.size = sizeof(gdt) - 1,   // 1번 부터 시작함
		.address = (uint64_t)gdt}; // 테이블 주소
	lgdt(&gdt_ds);				   // global descriptor table load

	/* Init the globla thread context */
	/* 글로블 스레드 컨텍스트 초기화 */
	// binary semaphore로 초기화 및 기능 구현, 공유 자원 소유권 초기화
	lock_init(&tid_lock);
	list_init(&ready_list);
	list_init(&sleep_list);
	list_init(&destruction_req);

	/* Set up a thread structure for the running thread. */
	/* 실행 중인 스레드에 대한 스레드 구조를 설정합니다. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
/* 인터럽트를 활성화하여 선점형 스레드 스케줄링을 시작합니다.
   또한 Idle 스레드를 생성합니다. */
void thread_start(void)
{
	/* Create the idle thread. */
	/* 유휴 스레드를 생성합니다. */
	struct semaphore idle_started;
	// idle thread semaphore를 0으로 초기화
	sema_init(&idle_started, 0);
	// ready 상태 thread 생성
	thread_create("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	/* 선제적 스레드 스케줄링을 시작합니다. */
	intr_enable();

	/* Wait for the idle thread to initialize idle_thread. */
	/* 유휴 스레드가 idle_thread를 초기화할 때까지 기다립니다. */
	sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
/* 각 타이머 틱마다 타이머 인터럽트 핸들러가 호출합니다.
   따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행됩니다. */
void thread_tick(void)
{
	struct thread *t = thread_current();

	/* Update statistics. */
	/* 통계 업데이트. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	/* 선점 적용. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return();
}




void thread_wakeup(int64_t ticks)
{
    if (list_empty(&sleep_list))
        return;
    enum intr_level old_level;
    struct thread *to_wakeup = list_entry(list_front(&sleep_list), struct thread, elem);
    old_level = intr_disable();
    while (to_wakeup->tick <= ticks)
    {

		
        list_pop_front(&sleep_list);
		// list_push_back (&ready_list, &to_wakeup->elem);
        list_insert_ordered(&ready_list, &to_wakeup->elem, (list_less_func *)larger, NULL);
        to_wakeup->status = THREAD_READY;
        if (list_empty(&sleep_list))
            return;
        to_wakeup = list_entry(list_front(&sleep_list), struct thread, elem);
    }
    // printf("%lld \n", to_wakeup->wakeup_tick);
    intr_set_level(old_level);
}





/* Prints thread statistics. */
/* 스레드 통계를 출력합니다. */
void thread_print_stats(void)
{
	printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
		   idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
/* 주어진 초기 우선순위를 가진 NAME이라는 이름의 새 커널 스레드를
   생성하고, AUX를 인수로 전달하는 FUNCTION을 실행한 후 준비 큐에
   추가합니다. 새 스레드의 스레드 식별자를 반환하거나 생성에
   실패하면 TID_ERROR를 반환합니다.

   thread_start()가 호출된 경우 thread_create()가 반환되기 전에
   새 스레드가 스케줄링될 수 있습니다. 심지어 thread_create()가
   반환되기 전에 종료될 수도 있습니다. 반대로 새 스레드가 스케줄되기
   전에 원래 스레드가 얼마든지 실행될 수 있습니다. 순서를 보장해야
   하는 경우 세마포어 또는 다른 형태의 동기화를 사용하세요.

   제공된 코드는 새 스레드의 `priority' 멤버를 PRIORITY로 설정하지만
   실제 우선순위 스케줄링은 구현되지 않습니다. 우선순위 스케줄링은
   문제 1-3의 목표입니다. */
tid_t thread_create(const char *name, int priority,
					thread_func *function, void *aux)
{
	struct thread *t;
	tid_t tid;

	ASSERT(function != NULL);

	/* Allocate thread. */
	/* 스레드를 할당합니다. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	/* 스레드를 초기화합니다. */
	init_thread(t, name, priority);
	tid = t->tid = allocate_tid();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	/* 예약된 경우 kernel_thread를 호출합니다.
	 * 참고) rdi는 첫 번째 인자, rsi는 두 번째 인자입니다. */
	t->tf.rip = (uintptr_t)kernel_thread;
	t->tf.R.rdi = (uint64_t)function;
	t->tf.R.rsi = (uint64_t)aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	/* 실행 대기열에 추가합니다. */
	thread_unblock(t);

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
/* 현재 스레드를 절전 모드로 전환합니다. thread_unblock()에 의해
   깨어날 때까지 다시 스케줄링되지 않습니다.

   이 함수는 인터럽트가 꺼진 상태에서 호출해야 합니다. 일반적으로
   synch.h의 동기화 프리미티브 중 하나를 사용하는 것이 좋습니다. */
void thread_block(void)
{
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);
	thread_current()->status = THREAD_BLOCKED;
	schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
/* 차단된 스레드 T를 실행 준비 상태로 전환합니다. T가 차단되지
   않은 경우 오류입니다. (실행 중인 스레드를 준비 상태로 만들려면
   thread_yield()를 사용합니다.)

   이 함수는 실행 중인 스레드를 선점하지 않습니다. 호출자가 인터럽트를
   스스로 비활성화했다면 스레드를 원자적으로 차단 해제하고 다른
   데이터를 업데이트할 수 있습니다. */
void thread_unblock(struct thread *t)
{
	enum intr_level old_level;

	ASSERT(is_thread(t));


	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_push_back (&ready_list, &t->elem);
	// list_insert_ordered(&ready_list , &t->elem , (list_less_func *)priority , NULL);

	t->status = THREAD_READY;
	intr_set_level(old_level);
}

/* Returns the name of the running thread. */
/* 실행 중인 스레드의 이름을 반환합니다. */
const char *thread_name(void)
{
	return thread_current()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
/* 실행 중인 스레드를 반환합니다. 이것은 running_thread()에
   몇 가지 건전성 검사를 더한 것입니다. 자세한 내용은 thread.h의
   상단에 있는 큰 주석을 참조하세요. */
struct thread *thread_current(void)
{
	// running 상태인 thread를 지역 변수 t에 할당
	struct thread *t = running_thread();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	/* T가 실제로 스레드인지 확인하세요. 이러한 어설션 중 하나라도
	   발생하면 해당 스레드의 스택이 오버플로되었을 수 있습니다. 각 스레드의
	   스택은 4KB 미만이므로 몇 개의 큰 자동 배열이나 중간 정도의 재귀가
	   스택 오버플로를 일으킬 수 있습니다. */
	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_RUNNING);


	return t;
}

/* Returns the running thread's tid. */
/* 실행 중인 스레드의 tid를 반환합니다. */
tid_t thread_tid(void)
{
	return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
/* 현재 스레드의 스케줄을 취소하고 파괴합니다.
   호출자에게 반환하지 않습니다. */
void thread_exit(void)
{
	ASSERT(!intr_context());

#ifdef USERPROG
	process_exit();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	/* 상태를 dying으로 설정하고 다른 프로세스를 예약하세요.
	   schedule_tail()을 호출하는 동안 소멸됩니다. */
	intr_disable();
	do_schedule(THREAD_DYING);
	NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
/* CPU를 반환합니다. 현재 스레드는 절전 상태가 되지 않으며
   스케줄러의 변덕에 따라 즉시 다시 스케줄링될 수 있습니다. */
void thread_yield(void)
{
	// 외부 인터럽트가 처리 중이면 alert
	ASSERT(!intr_context());

	// interrupt 불가능하게 만들고 직전 인터럽트 status 반환
	enum intr_level old_level = intr_disable();
	// running 상태인 thread를 지역 변수 curr에 할당
	struct thread *curr = thread_current();

	if (curr != idle_thread)
  {
    // ready_list의 제일 뒤에 보냄
		// list_push_back (&ready_list, &curr->elem);
		list_insert_ordered(&ready_list , &curr->elem , (list_less_func *)priority , NULL);
  }
    
    // ready 상태로 바꿔줌
	  do_schedule (THREAD_READY);
  
    // old_level 값에 맞춰서 enable or disable
	  intr_set_level (old_level);
}

// 추후 Project1 완료 후 성능 Test를 위해 백업 - Hyeonwoo, 2024.03.06
// void thread_yield(void)
// {
// 	// 외부 인터럽트가 처리 중이면 alert
// 	ASSERT(!intr_context());

// 	// interrupt 불가능하게 만들고 직전 인터럽트 status 반환
// 	enum intr_level old_level = intr_disable();
// 	// running 상태인 thread를 지역 변수 curr에 할당
// 	struct thread *curr = thread_current();

// 	if (curr != idle_thread)
// 	{
// 		// ready_list의 제일 뒤에 보냄
// 		list_push_back(&ready_list, &curr->elem);
// 	}

// 	// ready 상태로 바꿔줌
// 	do_schedule(THREAD_READY);

// 	// old_level 값에 맞춰서 enable or disable
// 	intr_set_level(old_level);
// }

// /* 스레드의 wake_up_tick을 비교하여 빠른 순서대로 정렬하는 함수 */
// bool compare_wakeup_tick(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
// {
// 	struct thread *thread_a = list_entry(a, struct thread, elem);
// 	struct thread *thread_b = list_entry(b, struct thread, elem);
// 	return thread_a->wakeup_tick < thread_b->wakeup_tick;
// }

// // 스레드 상태를 blocked로 설정하고 sleep queue에 삽입한 후 대기하는 함수
// void thread_sleep(int64_t tick)
// {
// 	/* If the current thread is not idle thread,
// 	   change the state of the caller thread to BLOCKED,
// 	   store the local tick to wake up, update the global
// 	   tick if necessary, and call schedule() */
// 	/* When you manipulate thread list, disable interrupt! */
// 	/* 현재 스레드가 Idle 스레드가 아닌 경우 호출자 스레드의
// 	   상태를 BLOCKED로 변경하고 로컬 틱을 저장하여 깨우고 필요한
// 	   경우 전역 틱을 업데이트한 다음 schedule()을 호출합니다. */
// 	/* 스레드 목록을 조작할 때는 인터럽트를 비활성화하세요! */
// 	// running 상태인 thread를 지역 변수 curr에 할당
// 	// 외부 인터럽트가 처리 중이면 alert
// 	ASSERT(!intr_context());

// 	// interrupt 불가능하게 만들고 직전 인터럽트 status 반환
// 	enum intr_level old_level = intr_disable();
// 	struct thread *curr = thread_current();

// 	if (curr != idle_thread)
// 	{
// 		// sleep_list의 제일 뒤에 보냄
// 		// list_push_back(&sleep_list, &curr->elem);

// 		// 깨어날 시간 설정
// 		curr->wakeup_tick = tick;

// 		// sleep_list에 현재 스레드를 정렬하여 삽입
// 		list_insert_ordered(&sleep_list, &curr->elem, compare_wakeup_tick, NULL);

// 		// blocked 상태로 바꿔줌
// 		thread_block();
// 	}

// 	// old_level 값에 맞춰서 enable or disable
// 	intr_set_level(old_level);
// }

// // sleep queue에서 깨울 스레드를 찾아서 깨우는 함수
// void thread_wakeup(void)
// {
// 	for (struct list_elem *e = list_begin(&sleep_list); e != list_end(&sleep_list);)
// 	{
// 		struct thread *t = list_entry(e, struct thread, elem);

// 		if (t->wakeup_tick > timer_ticks())
// 		{
// 			break;
// 		}

// 		// 깨어날 시간이 현재 시간보다 작거나 같으면
// 		// sleep_list에서 제거 후 ready_list에 추가
// 		e = list_remove(e);
// 		thread_unblock(t);
// 	}
// }

void thread_sleep(int64_t ticks)
{
    struct thread *curr = thread_current();
    enum intr_level old_level;
    old_level = intr_disable();
    if (curr != idle_thread)
    {
        curr->status = THREAD_BLOCKED;
        curr->tick = ticks;
		// list_push_back (&sleep_list, &curr->elem);
        list_insert_ordered(&sleep_list, &curr->elem, (list_less_func *)local_tick, NULL);
    }
    schedule();
    intr_set_level(old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
/* 현재 스레드의 우선순위를 NEW_PRIORITY로 설정합니다. */
void thread_set_priority(int new_priority)
{
	thread_current()->priority = new_priority;
}

/* Returns the current thread's priority. */
/* 현재 스레드의 우선순위를 반환합니다. */
int thread_get_priority(void)
{
	return thread_current()->priority;
}

/* Sets the current thread's nice value to NICE. */
/* 현재 스레드의 nice 값을 NICE로 설정합니다. */
void thread_set_nice(int nice UNUSED)
{
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
/* 현재 스레드의 nice 값을 반환합니다. */
int thread_get_nice(void)
{
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
/* 시스템 부하 평균의 100배를 반환합니다. */
int thread_get_load_avg(void)
{
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
/* 현재 스레드의 recent_cpu 값의 100배를 반환합니다. */
int thread_get_recent_cpu(void)
{
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
/* Idle 스레드. 다른 어떤 스레드도 실행할 준비가 되어 있지 않을 때 실행됩니다.

   Idle 스레드는 처음에 thread_start()에 의해 ready_list에 넣어집니다.
   초기에 한 번 예약되며, 이때 idle_thread를 초기화하고, 전달된 세마포어를
   "올려"(up) thread_start()가 계속되도록 허용하며, 즉시 블록됩니다.
   그 이후로는 Idle 스레드가 다시 ready_list에 나타나지 않습니다.
   Idle 스레드는 ready_list가 비어 있을 때 next_thread_to_run()에 의해
   특별한 경우로 반환됩니다. */
static void idle(void *idle_started_ UNUSED)
{
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current();
	sema_up(idle_started);

	for (;;)
	{
		/* Let someone else run. */
		/* 다른 사람에게 맡기세요. */
		intr_disable();
		thread_block();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		/* 인터럽트를 다시 활성화하고 다음 인터럽트를 기다립니다.

		   'sti' 명령은 다음 명령이 완료될 때까지 인터럽트를
		   비활성화하므로 이 두 명령은 원자적으로 실행됩니다.
		   이 원자성이 중요하며, 그렇지 않으면 인터럽트를 다시
		   활성화하고 다음 인터럽트를 기다리는 사이에 인터럽트가
		   처리되어 1 클럭만큼의 시간이 낭비될 수 있습니다.

		   [IA32-v2a] "HLT", [IA32-v2b] "STI", [IA32-v3a] 7.11.1
		   "HLT 명령어"를 참조하세요. */
		asm volatile("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
/* 커널 스레드의 기초로 사용되는 함수. */
static void kernel_thread(thread_func *function, void *aux)
{
	ASSERT(function != NULL);

	intr_enable(); /* The scheduler runs with interrupts off. */
				   /* 스케줄러는 인터럽트를 끈 상태로 실행됩니다. */
	function(aux); /* Execute the thread function. */
				   /* 스레드 함수를 실행합니다. */
	thread_exit(); /* If function() returns, kill the thread. */
				   /* function()이 반환되면 스레드를 종료합니다. */
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
/* T의 기본 초기화를 NAME이라는 차단된 스레드로 수행합니다. */
static void init_thread(struct thread *t, const char *name, int priority)
{
	ASSERT(t != NULL);
	ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT(name != NULL);

	// thread를 0으로 초기화
	memset(t, 0, sizeof *t);
	// t의 status를 blocked로 변경
	t->status = THREAD_BLOCKED;
	// t->name의 길이만큼 name에서 t->name으로 문자열 복사
	strlcpy(t->name, name, sizeof t->name);
	// for switching
	t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
	t->priority = priority;
	// for checking stackover flow
	t->magic = THREAD_MAGIC;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
/* 다음으로 스케줄될 스레드를 선택하고 반환합니다. 실행 대기열이
   비어 있지 않으면 실행 대기열에서 스레드를 반환해야 합니다.
   (현재 실행 중인 스레드가 계속 실행될 수 있다면, 그 스레드는
   실행 대기열에 있을 것입니다.)
   실행 대기열이 비어 있으면 idle_thread를 반환합니다. */
static struct thread *next_thread_to_run(void)
{
	if (list_empty(&ready_list))
		// ready_list가 비어있을 때 반환
		return idle_thread;
	else
		// ready_list의 첫 요소 반환
		return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
/* iretq를 사용하여 스레드를 시작합니다. */
void do_iret(struct intr_frame *tf)
{
	__asm __volatile(
		"movq %0, %%rsp\n"
		"movq 0(%%rsp),%%r15\n"
		"movq 8(%%rsp),%%r14\n"
		"movq 16(%%rsp),%%r13\n"
		"movq 24(%%rsp),%%r12\n"
		"movq 32(%%rsp),%%r11\n"
		"movq 40(%%rsp),%%r10\n"
		"movq 48(%%rsp),%%r9\n"
		"movq 56(%%rsp),%%r8\n"
		"movq 64(%%rsp),%%rsi\n"
		"movq 72(%%rsp),%%rdi\n"
		"movq 80(%%rsp),%%rbp\n"
		"movq 88(%%rsp),%%rdx\n"
		"movq 96(%%rsp),%%rcx\n"
		"movq 104(%%rsp),%%rbx\n"
		"movq 112(%%rsp),%%rax\n"
		"addq $120,%%rsp\n"
		"movw 8(%%rsp),%%ds\n"
		"movw (%%rsp),%%es\n"
		"addq $32, %%rsp\n"
		"iretq"
		: : "g"((uint64_t)tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
/* 새로운 스레드의 페이지 테이블을 활성화하여 스레드 전환을 수행하고,
   이전 스레드가 종료 중인 경우 파괴합니다.

   이 함수를 호출할 때 방금 이전 스레드에서 새 스레드가 이미 실행
   중이고 인터럽트는 여전히 비활성화된 상태입니다.

   스레드 전환이 완료되기 전에 printf()를 호출하는 것은 안전하지 않습니다.
   실제로, 이 함수의 끝에 printf()를 추가해야 합니다. */
static void thread_launch(struct thread *th)
{
	uint64_t tf_cur = (uint64_t)&running_thread()->tf;
	uint64_t tf = (uint64_t)&th->tf;
	ASSERT(intr_get_level() == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	/* 주요 스위칭 로직입니다. 먼저 전체 실행 컨텍스트를 intr_frame으로
	   복원하고 do_iret을 호출하여 다음 스레드로 전환합니다. 여기서부터
	   전환이 완료될 때까지 스택을 사용해서는 안 됩니다. */
	__asm __volatile(
		/* 사용할 레지스터를 저장합니다. */
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		/* 입력 한 번 불러오기 */
		"movq %0, %%rax\n"
		"movq %1, %%rcx\n"
		"movq %%r15, 0(%%rax)\n"
		"movq %%r14, 8(%%rax)\n"
		"movq %%r13, 16(%%rax)\n"
		"movq %%r12, 24(%%rax)\n"
		"movq %%r11, 32(%%rax)\n"
		"movq %%r10, 40(%%rax)\n"
		"movq %%r9, 48(%%rax)\n"
		"movq %%r8, 56(%%rax)\n"
		"movq %%rsi, 64(%%rax)\n"
		"movq %%rdi, 72(%%rax)\n"
		"movq %%rbp, 80(%%rax)\n"
		"movq %%rdx, 88(%%rax)\n"
		"pop %%rbx\n" // Saved rcx	// 저장된 rcx
		"movq %%rbx, 96(%%rax)\n"
		"pop %%rbx\n" // Saved rbx	// 저장된 rbx
		"movq %%rbx, 104(%%rax)\n"
		"pop %%rbx\n" // Saved rax	// 저장된 rax
		"movq %%rbx, 112(%%rax)\n"
		"addq $120, %%rax\n"
		"movw %%es, (%%rax)\n"
		"movw %%ds, 8(%%rax)\n"
		"addq $32, %%rax\n"
		"call __next\n" // read the current rip.
						// 현재 립을 읽습니다.
		"__next:\n"
		"pop %%rbx\n"
		"addq $(out_iret -  __next), %%rbx\n"
		"movq %%rbx, 0(%%rax)\n" // rip
		"movw %%cs, 8(%%rax)\n"	 // cs
		"pushfq\n"
		"popq %%rbx\n"
		"mov %%rbx, 16(%%rax)\n" // eflags
		"mov %%rsp, 24(%%rax)\n" // rsp
		"movw %%ss, 32(%%rax)\n"
		"mov %%rcx, %%rdi\n"
		"call do_iret\n"
		"out_iret:\n"
		: : "g"(tf_cur), "g"(tf) : "memory");
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
/* 새로운 프로세스를 스케줄합니다. 진입 시 인터럽트가 꺼져 있어야 합니다.
 * (프로세스 스케줄이 ready -> running으로 변경)
 * 이 함수는 현재 스레드의 상태를 status로 변경한 다음 다른 스레드를 찾아
 * 실행하고 전환합니다. schedule()에서 printf()를 호출하는 것은 안전하지
 * 않습니다. */
static void do_schedule(int status)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(thread_current()->status == THREAD_RUNNING);
	while (!list_empty(&destruction_req))
	{
		struct thread *victim =
			list_entry(list_pop_front(&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current()->status = status;
	schedule();
}

// context switching
/* OS에서 context는 CPU가 해당 프로세스를 실행하기 위한 해당 프로세스의
 * 정보들입니다. 이 Context는 프로세스의 PCB에 저장됩니다. Context Swithcing
 * 때 해당 CPU는 아무런 일을 하지 못합니다. 이 때, 오버헤드가 발생해
 * 효율이 떨어집니다. */

// 스케줄링
static void schedule(void)
{
	struct thread *curr = running_thread();
	// next thread pointing
	struct thread *next = next_thread_to_run();

	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(curr->status != THREAD_RUNNING);
	ASSERT(is_thread(next));
	/* Mark us as running. */
	/* next를 running으로 표시합니다. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	/* 새 타임슬라이스 시작. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	/* 새 주소 공간을 활성화합니다. */
	process_activate(next);
#endif
	// 다음 스레드가 있을 때 (ready_list에서 running으로 바뀐 thread)
	if (curr != next)
	{
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		/* 전환한 스레드가 dying 상태라면, 그 struct thread를 파괴합니다.
		   이 작업은 thread_exit()가 자체적으로 깔개를 꺼내지 않도록 늦게
		   수행해야 합니다. 페이지가 현재 스택에서 사용 중이므로 여기서는
		   페이지 해제 요청을 대기열에 넣습니다. 실제 소멸 로직은 schedule()의
		   시작 부분에 호출됩니다. */
		// Thread가 dying 일때 destruction_req 뒤에 push
		if (curr && curr->status == THREAD_DYING && curr != initial_thread)
		{
			// 예외처리
			ASSERT(curr != next);
			list_push_back(&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		/* 스레드를 전환하기 전에 먼저 현재 실행 중인 정보를 저장합니다. */
		// context switching to 실행할 스레드
		thread_launch(next);
	}
}

/* Returns a tid to use for a new thread. */
/* 새 스레드에 사용할 tid를 반환합니다. */
static tid_t allocate_tid(void)
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire(&tid_lock);
	tid = next_tid++;
	lock_release(&tid_lock);

	return tid;
}
