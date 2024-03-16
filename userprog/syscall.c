#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "userprog/process.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

static bool is_valid_user_ptr(const void *uaddr);
static bool is_valid_user_region(const void *uaddr, size_t len);

static void halt();
static void exit(int status);
static int fork(const char *thread_name, struct intr_frame *f);
static bool exec(const char *cmd_line);
static int wait(int pid);
static bool create(const char *file, unsigned initial_size);
static bool remove(const char *file);
static int open(const char *file);
static int filesize(int fd);
static int read(int fd, void *buffer, unsigned size);
static int write(int fd, const void *buffer, unsigned size);
static void seek(int fd, unsigned position);
static unsigned tell(int fd);
static void close(int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */
/* 시스템 호출. 이전에는 시스템 호출 서비스가 인터럽트 핸들러
 * (예: Linux의 경우 int 0x80)에 의해 처리되었습니다. 그러나 x86-64에서는
 * 제조업체가 시스템 호출을 요청하는 효율적인 경로인 `syscall` 명령어를
 * 제공합니다. syscall 명령어는 모델별 레지스터(MSR)에서 값을 읽는 방식으로
 * 작동합니다. 자세한 내용은 매뉴얼을 참조하세요. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
									/* 세그먼트 선택기 msr*/
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
									/* 긴 모드 SYSCALL 대상 */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */
									/* 이플래그를 위한 마스크 */

void debug_msg(const char *format, ...)
{
#ifdef DEBUG_THREADS
	va_list args;

	printf("[DEBUG] thread_name: %s, ", thread_current()->name);
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	putchar('\n');
#endif
}

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	/* 인터럽트 서비스 루틴은 syscall_entry가 사용자 영역 스택을 커널
	 * 모드 스택으로 교체할 때까지 인터럽트를 제공하지 않아야 합니다.
	 * 따라서 FLAG_FL을 마스킹했습니다. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
/* 메인 시스템 호출 인터페이스 */
// void syscall_handler(struct intr_frame *f UNUSED)
// {
// 	// TODO: Your implementation goes here.
// 	printf("system call!\n");
// 	thread_exit();
// }

/*interrupt 받아옴*/
void syscall_handler(struct intr_frame *f)
{
	printf("[syscall] syscall_handler - system call!\n");

	thread_current()->tf = *f;

	switch (f->R.rax) // 시스템 콜 번호
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_EXEC:
		if (exec(f->R.rdi) == -1)
			exit(-1);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	default:
		thread_exit();
	}
}

/* Checks if the given user pointer is valid */
/* 주어진 사용자 포인터가 유효한지 확인합니다. */
static bool is_valid_user_ptr(const void *uaddr)
{
	return (uaddr != NULL) && is_user_vaddr(uaddr);
}

/* Checks if the given user memory region is valid */
/* 지정된 사용자 메모리 영역이 유효한지 확인합니다. */
static bool is_valid_user_region(const void *uaddr, size_t len)
{
	return is_valid_user_ptr(uaddr) && is_valid_user_ptr(uaddr + len - 1);
}

static void halt()
{
	
	power_off();
}

/* Handles the exit system call. */
static void exit(int status)
{
#ifdef USERPROG
	// 현재 스레드의 exit_status를 설정
	thread_current()->exit_status = status;
#endif

	// 프로세스 종료
	thread_exit();
}

static int fork(const char *thread_name, struct intr_frame *f)
{
	return process_fork(thread_name, f);
}

static bool exec(const char *cmd_line)
{
	return process_exec(cmd_line);
}

/* Handles the wait system call. */
static int wait(tid_t child_tid)
{
	// 자식 프로세스의 tid를 인자로 받아 해당 자식 프로세스를 대기
	return process_wait(child_tid);
}

static bool create(const char *file, unsigned initial_size)
{
}

static bool remove(const char *file)
{
}

static int open(const char *file)
{
}

static int filesize(int fd)
{
}

struct file *get_file_by_fd(int fd)
{
	struct thread *curr = thread_current();

	// 파일 디스크립터 유효성 검사
	if (fd < 0 || fd >= FDT_COUNT_LIMIT)
	{
		return NULL;
	}

	// 해당 파일 디스크립터에 연결된 파일 객체 반환
	return curr->fdt[fd];
}

static int read(int fd, void *buffer, unsigned size)
{
	if (!is_valid_user_region(buffer, size))
	{
		thread_exit();
	}

	if (fd == 0)
	{
		for (unsigned i = 0; i < size; i++)
		{
			((char *)buffer)[i] = input_getc();
		}

		return size;
	}

	struct file *file = get_file_by_fd(fd);

	if (file == NULL)
	{
		return -1;
	}

	return file_read(file, buffer, size);
}

static int write(int fd, const void *buffer, unsigned size)
{
}

static void seek(int fd, unsigned position)
{
}

static unsigned tell(int fd)
{
}

static void close(int fd)
{
}