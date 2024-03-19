#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup(void);
static bool load(const char *file_name, struct intr_frame *if_);
static void initd(void *f_name);
static void __do_fork(void *);

/* General process initializer for initd and other process. */
/* initd 및 기타 프로세스를 위한 일반 프로세스 초기화 프로그램입니다. */
static void process_init(void)
{
	struct thread *current = thread_current();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
/* 파일 이름에서 로드한 "initd"라는 첫 번째 유저랜드 프로그램을 시작합니다.
 * process_create_initd()가 반환되기 전에 새 스레드가 예약될 수 있으며
 * 종료될 수도 있습니다. initd의 스레드 ID를 반환하거나, 스레드를 생성할 수
 * 없는 경우 TID_ERROR를 반환합니다.
 * 이 함수는 한 번만 호출해야 한다는 점에 유의하세요. */
tid_t process_create_initd(const char *file_name)
{
	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	/* FILE_NAME의 복사본을 만듭니다.
	 * 그렇지 않으면 호출자와 load() 사이에 경합이 발생합니다. */
	char *fn_copy = palloc_get_page(0);

	if (fn_copy == NULL)
	{
		return TID_ERROR;
	}

	strlcpy(fn_copy, file_name, PGSIZE);

	char *save_ptr;
	strtok_r(file_name, " ", &save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	/* FILE_NAME을 실행할 새 스레드를 생성합니다. */
	tid_t tid = thread_create(file_name, PRI_DEFAULT, initd, fn_copy);

	if (tid == TID_ERROR)
	{
		palloc_free_page(fn_copy);
	}

	return tid;
}

/* A thread function that launches first user process. */
/* 첫 번째 사용자 프로세스를 실행하는 스레드 함수입니다. */
static void initd(void *f_name)
{
#ifdef VM
	supplemental_page_table_init(&thread_current()->spt);
#endif

	process_init();

	if (process_exec(f_name) < 0)
	{
		PANIC("Fail to launch initd\n");
		PANIC("`initd` 실행 실패\n");
	}

	NOT_REACHED();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
/* 현재 프로세스를 `name`으로 복제합니다. 새 프로세스의 스레드 ID를 반환하거나
 * 스레드를 생성할 수 없는 경우 TID_ERROR를 반환합니다. */
// tid_t process_fork(const char *name, struct intr_frame *if_ UNUSED)
// {
// 	/* Clone current thread to new thread. */
//  /* 현재 스레드를 새 스레드로 복제합니다. */
// 	return thread_create(name,
// 						 PRI_DEFAULT, __do_fork, thread_current());
// }
tid_t process_fork(const char *name, struct intr_frame *if_)
{
	/* Clone current thread to new thread. */
	/* 현재 스레드를 새 스레드로 복제합니다. */
	return thread_create(name, PRI_DEFAULT, __do_fork, if_);
}

#ifndef VM
// /* Duplicate the parent's address space by passing this function to the
//  * pml4_for_each. This is only for the project 2. */
// /* 이 함수를 pml4_for_ach에 전달하여 부모의 주소 공간을 복제합니다.
//  * 이것은 프로젝트 2에만 해당됩니다. */
// static bool duplicate_pte(uint64_t *pte, void *va, void *aux)
// {
// 	struct thread *current = thread_current();
// 	struct thread *parent = (struct thread *)aux;
// 	void *parent_page;
// 	void *newpage;
// 	bool writable;

// 	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
// 	/* 1. TODO: 부모 페이지가 커널 페이지인 경우 즉시 반환합니다. */

// 	/* 2. Resolve VA from the parent's page map level 4. */
// 	/* 2. 상위 페이지 맵 레벨 4에서 VA를 해결합니다. */
// 	parent_page = pml4_get_page(parent->pml4, va);

// 	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
// 	 *    TODO: NEWPAGE. */
// 	/* 3. TODO: 자식에게 새 PAL_USER 페이지를 할당하고
// 	 *    TODO: 결과를 NEWPAGE로 설정합니다. */

// 	/* 4. TODO: Duplicate parent's page to the new page and
// 	 *    TODO: check whether parent's page is writable or not (set WRITABLE
// 	 *    TODO: according to the result). */
// 	/* 4. TODO: 부모 페이지를 새 페이지에 복제하고
// 		  TODO: 부모 페이지가 쓰기 가능한지 여부를 확인합니다.
// 		  TODO: (결과에 따라 쓰기 가능으로 설정). */

// 	/* 5. Add new page to child's page table at address VA with WRITABLE
// 	 *    permission. */
// 	/* 5. 쓰기 권한이 있는 주소 VA의 하위 페이지 테이블에 새 페이지를
// 	 *    추가합니다. */
// 	if (!pml4_set_page(current->pml4, va, newpage, writable))
// 	{
// 		/* 6. TODO: if fail to insert page, do error handling. */
// 		/* 6. TODO: 페이지 삽입에 실패하면 오류 처리를 수행합니다. */
// 	}
// 	return true;
// }

/* 이 함수를 pml4_for_ach에 전달하여 부모의 주소 공간을 복제합니다.
 * 이것은 프로젝트 2에만 해당됩니다. */
static bool duplicate_pte(uint64_t *pte, void *va, void *aux)
{
	struct thread *current = thread_current();
	struct thread *parent = (struct thread *)aux;

	/* 1. TODO: 부모 페이지가 커널 페이지인 경우 즉시 반환합니다. */
	if (is_kernel_vaddr(va))
	{
		return true;
	}

	/* 2. 상위 페이지 맵 레벨 4에서 VA를 해결합니다. */
	void *parent_page = pml4_get_page(parent->pml4, va);

	/* 3. TODO: 자식에게 새 PAL_USER 페이지를 할당하고
	 *    TODO: 결과를 NEWPAGE로 설정합니다. */
	void *newpage = palloc_get_page(PAL_USER);

	/* 4. TODO: 부모 페이지를 새 페이지에 복제하고
		  TODO: 부모 페이지가 쓰기 가능한지 여부를 확인합니다.
		  TODO: (결과에 따라 쓰기 가능으로 설정). */
	memcpy(newpage, parent_page, PGSIZE);
	bool writable = is_writable(pte);

	/* 5. 쓰기 권한이 있는 주소 VA의 하위 페이지 테이블에 새 페이지를
	 *    추가합니다. */
	if (!pml4_set_page(current->pml4, va, newpage, writable))
	{
		/* 6. TODO: 페이지 삽입에 실패하면 오류 처리를 수행합니다. */
		palloc_free_page(newpage);
		return false;
	}

	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
/* 부모 프로세스의 실행 컨텍스트를 복사하는 스레드 함수입니다.
 * 힌트) parent->tf는 프로세스의 유저랜드 컨텍스트를 보유하지 않습니다.
 *       즉, 이 함수에 process_fork의 두 번째 인수를 전달해야 합니다. */
// static void __do_fork(void *aux)
// {
// 	struct intr_frame if_;
// 	struct thread *parent = (struct thread *)aux;
// 	struct thread *current = thread_current();
// 	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
//  /* TODO: 어떻게든 부모_if를 전달해야 합니다. (즉, process_fork() 의 if_) * /
// 	struct intr_frame *parent_if;
// 	bool succ = true;

// 	/* 1. Read the cpu context to local stack. */
// 	memcpy(&if_, parent_if, sizeof(struct intr_frame));

// 	/* 2. Duplicate PT */
// 	current->pml4 = pml4_create();
// 	if (current->pml4 == NULL)
// 		goto error;

// 	process_activate(current);
// #ifdef VM
// 	supplemental_page_table_init(&current->spt);
// 	if (!supplemental_page_table_copy(&current->spt, &parent->spt))
// 		goto error;
// #else
// 	if (!pml4_for_each(parent->pml4, duplicate_pte, parent))
// 		goto error;
// #endif

// 	/* TODO: Your code goes here.
// 	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
// 	 * TODO:       in include/filesys/file.h. Note that parent should not return
// 	 * TODO:       from the fork() until this function successfully duplicates
// 	 * TODO:       the resources of parent.*/

// 	process_init();

// 	/* Finally, switch to the newly created process. */
// 	if (succ)
// 		do_iret(&if_);
// error:
// 	thread_exit();
// }

/* 부모 프로세스의 실행 컨텍스트를 복사하는 스레드 함수입니다.
 * 힌트) parent->tf는 프로세스의 유저랜드 컨텍스트를 보유하지 않습니다.
 *       즉, 이 함수에 process_fork의 두 번째 인수를 전달해야 합니다. */
static void __do_fork(void *aux)
{
	struct intr_frame if_;
	struct thread *parent = (struct thread *)aux;
	struct thread *current = thread_current();
	bool succ = true;

	/* 1. CPU 컨텍스트를 로컬 스택으로 읽습니다. */
	memcpy(&if_, &parent->tf, sizeof(struct intr_frame));

	// 자식 프로세스 반환 값은 0
	if_.R.rax = 0;

	/* 2. PT 복제 */
	current->pml4 = pml4_create();

	if (current->pml4 == NULL)
	{
		goto error;
	}

	process_activate(current);

#ifdef VM
	supplemental_page_table_init(&current->spt);
	if (!supplemental_page_table_copy(&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each(parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: 당신의 코드는 여기에 구현되어야 합니다.
	 * TODO: 힌트) 파일 객체를 복제하려면 `include/filesys/file.h`에서
	 * TODO:       `file_duplicate`를 사용합니다. 이 함수가 부모의 리소스를
	 * TODO:       성공적으로 복제할 때까지 부모는 fork()에서 반환되지 않아야
	 * TODO:       합니다.
	 */
	if (parent->next_fd == FDT_COUNT_LIMIT)
	{
		goto error;
	}

	/* Duplicate the file descriptor table */
	/* 파일 디스크립터 테이블 복제 */
	for (int i = 0; i < FDT_COUNT_LIMIT; i++)
	{
		struct file *file = parent->fdt[i];

		if (file == NULL)
		{
			continue;
		}

		/* Duplicate the file and store it in the child's file descriptor table */
		/* 파일을 복제하여 하위 파일 디스크립터 테이블에 저장합니다. */
		current->fdt[i] = file_duplicate(file);

		if (current->fdt[i] == NULL)
		{
			goto error;
		}
	}

	/* Copy the next available file descriptor index */
	/* 사용 가능한 다음 파일 디스크립터 인덱스 복사 */
	current->next_fd = parent->next_fd;

	/* Duplicate the current working directory */
	/* 현재 작업 디렉터리 복제 */
	if (parent->cwd != NULL)
	{
		current->cwd = dir_reopen(parent->cwd);

		if (current->cwd == NULL)
		{
			goto error;
		}
	}

	process_init();

	/* Finally, switch to the newly created process. */
	/* 마지막으로 새로 생성된 프로세스로 전환합니다. */
	if (succ)
	{
		do_iret(&if_);
	}

error:
	current->exit_status = TID_ERROR;
	thread_exit();
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
/* 현재 실행 컨텍스트를 f_name으로 전환합니다.
 * 실패하면 -1을 반환합니다. */
/* 실행 되어야 하는 명령줄 받을 때 함수명과 매개변수를 분리해 주는 것
 * 현재 실행 context에서 f_name 으로 switch하라 */
int process_exec(void *f_name)
{
	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	/* 스레드 구조에서는 intr_frame을 사용할 수 없습니다.
	 * 현재 스레드가 스케줄을 변경할 때
	 * 실행 정보를 멤버에 저장하기 때문입니다. */
	struct intr_frame _if;

	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	/* 먼저 현재 컨텍스트를 종료합니다. */
	process_cleanup();

	/* And then load the binary */
	/* 그런 다음 바이너리를 로드합니다. */

	/* +1 하는 이유: 마지막 NULL요소 넣기 위해 */
	char *argv[LOADER_ARGS_LEN / 2 + 1];

	/* '\0' == NULL 문자 */
	char *save_ptr;
	int argc = 0;

	for (char *token = strtok_r(f_name, " ", &save_ptr); token != NULL;
		 token = strtok_r(NULL, " ", &save_ptr))
	{
		argv[argc++] = token;
	}

	// Gitbook에 적힌 내용 구현
	_if.R.rdi = argc;

	bool success = load(f_name, &_if);

	/* string 저장 */
	char *addrs[LOADER_ARGS_LEN / 2 + 1];

	for (int i = argc - 1; i >= 0; i--)
	{
		_if.rsp -= strlen(argv[i]) + 1;
		memcpy(_if.rsp, argv[i], strlen(argv[i]) + 1);
		addrs[i] = _if.rsp;
	}

	/* for padding */
	int total = 0;

	for (int k = argc - 1; k >= 0; k--)
	{
		total += strlen(argv[k]) + 1;
	}

	int padding = ROUND_UP(total, sizeof(uint64_t));
	padding -= total;

	_if.rsp -= padding;
	memset(_if.rsp, 0, padding);

	/* blank */
	_if.rsp -= sizeof(_if.rsp);
	memset(_if.rsp, 0, sizeof(_if.rsp));

	/* 주소 값 저장 */
	for (int j = argc - 1; j >= 0; j--)
	{
		_if.rsp -= sizeof(_if.rsp);
		// memcpy(_if.rsp, _if.R.rsi+(strlen(argv[j])+1), sizeof(_if.rsp));
		memcpy(_if.rsp, addrs + j, sizeof(_if.rsp));

		// printf("rsi:%p\n",_if.R.rsi);
		// printf("**************%p\n", _if.R.rsi+(strlen(argv[j])+1));
	}

	// Gitbook에 적힌 내용 구현
	_if.R.rsi = _if.rsp;

	/* 0 반환 값 저장 */
	_if.rsp -= sizeof(void *);
	memset(_if.rsp, 0, sizeof(void *));

	// hex_dump(_if.rsp, _if.rsp, USER_STACK - (uint64_t)_if.rsp, true);

	/* If load failed, quit. */
	palloc_free_page(f_name);

	if (!success)
	{
		return -1;
	}

	/* Start switched process. */
	/* 전환된 프로세스를 시작합니다. */
	do_iret(&_if);
	NOT_REACHED();
}

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
/* 스레드 TID가 죽을 때까지 기다렸다가 종료 상태를 반환합니다.
 * 커널에 의해 종료된 경우 (즉, 예외로 인해 종료된 경우)
 * -1을 반환합니다.
 * TID가 유효하지 않거나 호출 프로세스의 자식이 아닌 경우, 또는
 * 주어진 TID에 대해 process_wait()이 이미 성공적으로 호출된 경우,
 * 기다리지 않고 즉시 -1을 반환합니다.
 *
 * 이 함수는 문제 2-2에서 구현될 예정입니다.
 * 지금은 아무 일도 하지 않습니다. */
// int process_wait(tid_t child_tid UNUSED)
// {
// 	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
// 	 * XXX:       to add infinite loop here before
// 	 * XXX:       implementing the process_wait. */
// 	/* 힌트) process_wait(initd)를 실행하면 핀토스가 종료되므로, process_wait을
// 	 * 구현하기 전에 여기에 무한 루프를 추가하는 것이 좋습니다. */
// 	return -1;
// }

struct thread *get_child_by_tid(struct thread *curr, tid_t child_tid)
{
	struct thread *child = NULL;

	/* 해당 자식 프로세스를 찾기 위해 현재 프로세스의 자식 리스트를 순회 */
	for (struct list_elem *e = list_begin(&curr->children); e != list_end(&curr->children); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, child_elem);

		if (t->tid == child_tid)
		{
			child = t;
			break;
		}
	}

	return child;
}

/* 자식 프로세스가 종료될 때까지 대기하고 종료 상태를 반환하는 함수 */
int process_wait(tid_t child_tid)
{
	struct thread *curr = thread_current();
	struct thread *child = get_child_by_tid(curr, child_tid);

	/* 해당 자식 프로세스가 존재하지 않는 경우 */
	if (child == NULL)
	{
		/* 대기 시간을 주기 위해 일정 시간 동안 대기 */
		timer_sleep(100); // 100 ticks 동안 대기 (적절한 값으로 조정 가능)
		return -1;
	}

	/* 자식 프로세스가 종료될 때까지 대기 */
	sema_down(&child->child_wait_sema);

	/* 자식 프로세스의 종료 상태를 가져옴 */
	int exit_status = child->exit_status;

	/* 자식 프로세스를 부모의 자식 리스트에서 제거 */
	list_remove(&child->child_elem);

	/* 자식 프로세스의 메모리를 해제 */
	palloc_free_page(child);

	/* 자식 프로세스의 종료 상태를 반환 */
	return exit_status;
}

/* Exit the process. This function is called by thread_exit (). */
/* 프로세스를 종료합니다. 이 함수는 thread_exit ()에 의해 호출됩니다. */
// void process_exit(void)
// {
// 	struct thread *curr = thread_current();
// 	/* TODO: Your code goes here.
// 	 * TODO: Implement process termination message (see
// 	 * TODO: project2/process_termination.html).
// 	 * TODO: We recommend you to implement process resource cleanup here. */
// 	/* 할 일: 코드는 여기로 이동합니다. 프로세스 종료 메시지를 구현합니다.
// 	 * (project2/process_termination.html 참조)
// 	 * 여기에서 프로세스 리소스 정리를 구현하는 것이 좋습니다. */
// 	process_cleanup();
// }

/* 프로세스를 종료하는 함수 */
void process_exit(void)
{
	struct thread *curr = thread_current();

	/* 부모 프로세스가 자식 프로세스의 종료를 알 수 있도록 wait_sema를 up */
	// TODO: 아래 코드 필요 있는 지 논의 필요 - Hyeonwoo, 2024.03.18
	// sema_up(&curr->wait_sema);

	/* 파일 디스크립터 테이블 정리
	 * 프로세스가 열어둔 파일들을 모두 닫고 파일 디스크립터 테이블을 초기화 */
	for (int i = 0; i < FDT_COUNT_LIMIT; i++)
	{
		if (curr->fdt[i] != NULL)
		{
			file_close(curr->fdt[i]);
			curr->fdt[i] = NULL;
		}
	}

	/* 프로세스의 리소스를 정리하기 위해 process_cleanup() 함수 호출 */
	process_cleanup();
}

/* Free the current process's resources. */
/* 현재 프로세스의 리소스를 해제합니다. */
// static void process_cleanup(void)
// {
// 	struct thread *curr = thread_current();

// #ifdef VM
// 	supplemental_page_table_kill(&curr->spt);
// #endif

// 	/* Destroy the current process's page directory and switch back
// 	 * to the kernel-only page directory. */
// 	/* 현재 프로세스의 페이지 디렉터리를 삭제하고
// 	 * 커널 전용 페이지 디렉터리로 다시 전환합니다. */
// 	uint64_t *pml4 = curr->pml4;

// 	if (pml4 != NULL)
// 	{
// 		/* Correct ordering here is crucial.  We must set
// 		 * cur->pagedir to NULL before switching page directories,
// 		 * so that a timer interrupt can't switch back to the
// 		 * process page directory.  We must activate the base page
// 		 * directory before destroying the process's page
// 		 * directory, or our active page directory will be one
// 		 * that's been freed (and cleared). */
// 		/* 여기서 올바른 순서가 중요합니다. 타이머 인터럽트가
// 		 * 프로세스 페이지 디렉토리로 다시 전환할 수 없도록
// 		 * 페이지 디렉터리를 전환하기 전에 cur->pagedir을 NULL로
// 		 * 설정해야 합니다. 프로세스의 페이지 디렉터리를 삭제하기
// 		 * 전에 기본 페이지 디렉터리를 활성화해야 하며, 그렇지
// 		 * 않으면 활성 페이지 디렉터리가 해제된(그리고 지워진)
// 		 * 디렉터리가 됩니다. */
// 		curr->pml4 = NULL;

// 		pml4_activate(NULL);
// 		pml4_destroy(pml4);
// 	}
// }

/* 프로세스의 리소스를 정리하는 함수 */
void process_cleanup(void)
{
	struct thread *curr = thread_current();

	/* 현재 프로세스의 페이지 디렉토리를 NULL로 설정 */
	uint64_t *pml4 = curr->pml4;

	if (pml4 != NULL)
	{
		/* 현재 프로세스의 페이지 테이블 활성화 */
		curr->pml4 = NULL;
		pml4_activate(NULL);

		/* 페이지 디렉토리에 할당된 모든 페이지 테이블 삭제 */
		pml4_destroy(pml4);
	}

	/* 파일 디스크립터 테이블 정리 */
	for (int i = 0; i < FDT_COUNT_LIMIT; i++)
	{
		if (curr->fdt[i] != NULL)
		{
			file_close(curr->fdt[i]);
			curr->fdt[i] = NULL;
		}
	}

	/* 현재 프로세스의 모든 자식 프로세스를 찾아 메모리 해제 */
	while (!list_empty(&curr->children))
	{
		struct list_elem *e = list_pop_front(&curr->children);
		struct thread *child = list_entry(e, struct thread, child_elem);
		list_remove(e);
		free(child);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
/* 중첩 스레드에서 사용자 코드를 실행하기 위한 CPU를 설정합니다.
 * 이 함수는 컨텍스트 전환 시마다 호출됩니다. */
void process_activate(struct thread *next)
{
	/* Activate thread's page tables. */
	/* 스레드의 페이지 테이블을 활성화합니다. */
	pml4_activate(next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	/* 인터럽트 처리에 사용할 스레드의 커널 스택을 설정합니다. */
	tss_update(next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */
/* ELF 바이너리를 로드합니다. 다음 정의는 ELF 사양인 [ELF1]에서
 * 거의 그대로 가져온 것입니다. */

/* ELF types.  See [ELF1] 1-2. */
/* ELF 유형. [ELF1] 1-2 참조. */
#define EI_NIDENT 16

#define PT_NULL 0			/* Ignore. */
							/* 무시. */
#define PT_LOAD 1			/* Loadable segment. */
							/* 로드 가능 세그먼트. */
#define PT_DYNAMIC 2		/* Dynamic linking info. */
							/* 동적 링크 정보. */
#define PT_INTERP 3			/* Name of dynamic loader. */
							/* 동적 로더의 이름. */
#define PT_NOTE 4			/* Auxiliary info. */
							/* 보조 정보. */
#define PT_SHLIB 5			/* Reserved. */
							/* 예약됨. */
#define PT_PHDR 6			/* Program header table. */
							/* 프로그램 헤더 테이블. */
#define PT_STACK 0x6474e551 /* Stack segment. */
							/* 스택 세그먼트. */

#define PF_X 1 /* Executable. */
			   /* 실행 가능. */
#define PF_W 2 /* Writable. */
			   /* 쓰기 가능. */
#define PF_R 4 /* Readable. */
			   /* 읽기 가능. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
/* 실행 파일 헤더. [ELF1] 1-4~1-8을 참조하세요.
 * 이것은 ELF 바이너리의 맨 처음에 나타납니다. */
struct ELF64_hdr
{
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR
{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
/* 약어 */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack(struct intr_frame *if_);
static bool validate_segment(const struct Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
						 uint32_t read_bytes, uint32_t zero_bytes,
						 bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
/* FILE_NAME에서 현재 스레드로 ELF 실행 파일을 로드합니다.
 * 실행 파일의 진입점을 *RIP에,
 * 초기 스택 포인터를 *RSP에 저장합니다.
 * 성공하면 참을 반환하고, 그렇지 않으면 거짓을 반환합니다. */
static bool load(const char *file_name, struct intr_frame *if_)
{
	// 실행할 프로그램의 binary 파일을 메모리에 올리는 역할 수행
	bool success = false;
	struct thread *t = thread_current();

	/* Allocate and activate page directory. */
	/* 페이지 디렉토리를 할당하고 활성화합니다. */

	// 각 프로세스가 실행이 될 때, 각 프로세스에 해당하는 VM(virtual memory)이 만들어져야 함
	// 이를 위해 페이지 테이블 엔트리를 생성하는 과정이 우선적으로 필요
	// 그 뒤, 파일을 실제로 VM에 올리는 과정 진행
	t->pml4 = pml4_create();

	if (t->pml4 == NULL)
	{
		goto done;
	}

	/* 프로세스 실행 */
	process_activate(thread_current());
	/* Open executable file. */
	/* 실행 파일을 엽니다. */

	/*---------------------------------*/
	struct file *file = filesys_open(file_name);

	if (file == NULL)
	{
		printf("load: %s: open failed\n", file_name);
		goto done;
	}

	/* Read and verify executable header. */
	/* 실행 파일 헤더를 읽고 확인합니다. */
	/* ELF = 파일의 형식, header + data 구조 */
	struct ELF ehdr;

	if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\2\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 0x3E // amd64
		|| ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Phdr) || ehdr.e_phnum > 1024)
	{
		printf("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	/* 프로그램 헤더를 읽습니다. */
	off_t file_ofs = ehdr.e_phoff;

	for (int i = 0; i < ehdr.e_phnum; ++i)
	{
		if (file_ofs < 0 || file_ofs > file_length(file))
		{
			goto done;
		}

		file_seek(file, file_ofs);

		struct Phdr phdr;

		if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
		{
			goto done;
		}

		file_ofs += sizeof phdr;

		switch (phdr.p_type)
		{
		case PT_NULL:
		case PT_NOTE:
		case PT_PHDR:
		case PT_STACK:
		default:
			/* Ignore this segment. */
			/* 이 세그먼트는 무시. */
			break;
		case PT_DYNAMIC:
		case PT_INTERP:
		case PT_SHLIB:
			goto done;
		case PT_LOAD:
			// 파일이 제대로 된 ELF 인지 검사하는 과정을 동반
			// 세그먼트 단위로 PT_LOAD의 헤더 타입을 가진 부분을 하나씩 메모리로 올림
			if (validate_segment(&phdr, file))
			{
				bool writable = (phdr.p_flags & PF_W) != 0;
				uint64_t file_page = phdr.p_offset & ~PGMASK;
				uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
				uint64_t page_offset = phdr.p_vaddr & PGMASK;
				uint32_t read_bytes, zero_bytes;

				if (phdr.p_filesz > 0)
				{
					/* Normal segment.
					 * Read initial part from disk and zero the rest. */
					/* 일반 세그먼트.
					 * 디스크에서 초기 부분을 읽고 나머지는 제로화합니다. */
					read_bytes = page_offset + phdr.p_filesz;
					zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
				}
				else
				{
					/* Entirely zero.
					 * Don't read anything from disk. */
					/* 완전히 0입니다.
					 * 디스크에서 아무것도 읽지 않습니다. */
					read_bytes = 0;
					zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
				}

				// 파싱 코드를 load 함수 안에 넣으면 이 곳에서 Issue 발생
				// 원인 파악은 못 함
				if (!load_segment(file, file_page, (void *)mem_page,
								  read_bytes, zero_bytes, writable))
				{
					goto done;
				}
			}
			else
			{
				goto done;
			}
			break;
		}
	}

	/* Set up stack. */
	/* 스택을 설정합니다. */
	// 전부 메모리로 올린 뒤에 스택을 만드는 과정이 실행
	/* rsp로 user_stack 저장 */
	if (!setup_stack(if_))
	{
		goto done;
	}

	/* Start address. */
	/* 시작 주소. */
	// 어떤 명령부터 실행되는지를 가리키는, 즉 entry point 역할의 rip를 설정
	if_->rip = ehdr.e_entry;

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */
	/* TODO: 코드가 여기로 이동합니다.
	 * TODO: 인수 전달을 구현합니다. (project2/argument_passing.html 참조) */

	// 들어온 입력을 파싱해 USER_STACK에 채워넣는 루틴 추가
	// 만약 입력으로 들어오는 명령이 argument를 포함하고 있다면 load()의 코드에서 filename이 이제 진정한 파일의 이름이 아닐 수 있음
	// 순수한 실행 파일의 이름을 얻어내기 위해서, 파싱 자체는 load() 함수의 시작 즈음에 이루어져야 함

	// 파싱을 위해서 매뉴얼에서는 strtok_r() 함수를 사용할 것을 권장하고 있기에, 이를 사용
	// 사소하지만 중요한 사항으로, 포인터 변수의 크기가 8 바이트씩이라는 것을 잊지 말기
	// -> 현재 우리 조는 파싱을 load() 함수 외부인 process_exec()에서 진행하고 있음
	success = true;
done:
	/* We arrive here whether the load is successful or not. */
	/* 로드 성공 여부와 상관없이 여기에 도착합니다. */

	// 열었던 실행 파일을 닫음

	file_close(file);
	return success;
}

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
/* 파일에 유효한 로드 가능한 세그먼트가 있는지 확인하고,
 * 유효하면 참을 반환하고 그렇지 않으면 거짓을 반환합니다. */
static bool validate_segment(const struct Phdr *phdr, struct file *file)
{
	/* p_offset and p_vaddr must have the same page offset. */
	/* p_offset과 p_vaddr의 페이지 오프셋이 같아야 합니다. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	/* p_offset은 파일 내를 가리켜야 합니다. */
	if (phdr->p_offset > (uint64_t)file_length(file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	/* p_memsz는 최소한 p_filesz만큼 커야 합니다. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	/* 세그먼트가 비어있지 않아야 합니다. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	/* 가상 메모리 영역은 사용자 주소 공간 범위 내에서
	   시작과 끝을 모두 가져야 합니다. */
	if (!is_user_vaddr((void *)phdr->p_vaddr))
		return false;
	if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	/* 이 영역은 커널 가상 주소 공간을 "감쌀" 수 없습니다. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	/* 0 페이지 매핑을 허용하지 않습니다.
	   0 페이지를 매핑하는 것은 나쁜 생각일 뿐만 아니라, 이를 허용하면
	   시스템 호출에 널 포인터를 전달한 사용자 코드가 memcpy() 등에서
	   널 포인터 어설션을 통해 커널을 패닉 상태로 만들 가능성이 큽니다. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	/* 괜찮아요. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */
/* 이 블록의 코드는 프로젝트 2에서만 사용됩니다.
 * 전체 프로젝트 2에 대해 함수를 구현하려면 #ifndef 매크로 외부에서
 * 구현하세요. */

/* load() helpers. */
/* load() 헬퍼. */
static bool install_page(void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
/* FILE의 오프셋 OFS에서 시작하는 세그먼트를 주소 UPAGE에서
 * 로드합니다. 다음과 같이 총 READ_BYTES + ZERO_BYTES 바이트의
 * 가상 메모리가 초기화됩니다:
 *
 * - UPAGE의 READ_BYTES 바이트는 오프셋 OFS에서 시작하여 FILE에서
 * 읽어야 합니다.
 *
 * - UPAGE + READ_BYTES의 ZERO_BYTES 바이트는 0이 되어야 합니다.
 *
 * 이 함수에 의해 초기화된 페이지는 WRITABLE이 참이면 사용자
 * 프로세스에서 쓰기 가능해야 하고, 그렇지 않으면 읽기 전용이어야 합니다.
 *
 * 성공하면 참을 반환하고 메모리 할당 오류 또는 디스크 읽기 오류가
 * 발생하면 거짓을 반환합니다. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
						 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	file_seek(file, ofs);
	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		/* 이 페이지를 채우는 방법을 계산합니다.
		 * FILE에서 PAGE_READ_BYTES 바이트를 읽고
		 * 최종 PAGE_ZERO_BYTES 바이트를 0으로 만듭니다. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		/* 메모리 페이지를 가져옵니다. */
		uint8_t *kpage = palloc_get_page(PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		/* 이 페이지를 로드합니다. */
		if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)
		{
			palloc_free_page(kpage);
			return false;
		}
		memset(kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		/* 프로세스의 주소 공간에 페이지를 추가합니다. */
		if (!install_page(upage, kpage, writable))
		{
			printf("fail\n");
			palloc_free_page(kpage);
			return false;
		}

		/* Advance. */
		/* 사전. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
/* USER_STACK에서 제로화된 페이지를 매핑하여 최소 스택을 생성합니다. */
static bool setup_stack(struct intr_frame *if_)
{
	bool success = false;
	uint8_t *kpage = palloc_get_page(PAL_USER | PAL_ZERO);

	if (kpage != NULL)
	{
		success = install_page(((uint8_t *)USER_STACK) - PGSIZE, kpage, true);

		if (success)
		{
			if_->rsp = USER_STACK;
		}
		else
		{
			palloc_free_page(kpage);
		}
	}

	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
/* 사용자 가상 주소 UPAGE에서 커널 가상 주소 KPAGE로의 매핑을
 * 페이지 테이블에 추가합니다. WRITABLE이 참이면 사용자 프로세스가
 * 페이지를 수정할 수 있고, 그렇지 않으면 읽기 전용입니다.
 * UPAGE는 이미 매핑되어 있지 않아야 합니다. KPAGE는 palloc_get_page()로
 * 사용자 풀에서 가져온 페이지여야 합니다. 성공하면 true를 반환하고,
 * UPAGE가 이미 매핑되어 있거나 메모리 할당이 실패하면 false를 반환합니다. */
static bool install_page(void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	/* 해당 가상 주소에 이미 페이지가 없는지 확인한 다음
	 * 페이지를 해당 주소에 매핑합니다. */
	return (pml4_get_page(t->pml4, upage) == NULL && pml4_set_page(t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
			 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer(VM_ANON, upage,
											writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack(struct intr_frame *if_)
{
	bool success = false;
	void *stack_bottom = (void *)(((uint8_t *)USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */
