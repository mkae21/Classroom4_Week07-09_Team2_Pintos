#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall-nr.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/gdt.h"
#include "userprog/syscall.h"
#include "userprog/process.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

// static bool is_valid_user_ptr(const void *uaddr);
// static bool is_valid_user_region(const void *uaddr, size_t len);

void check_address(uintptr_t addr);
int add_file_to_fdt(struct file *file);
void remove_file_from_fdt(int fd);
struct file *get_file_from_fd(int fd);

void halt();
void exit(int status);
int fork(const char *thread_name, struct intr_frame *f);
bool exec(const char *cmd_line);
int wait(int pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

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

void syscall_init(void)
{
	// lock_init(&filesys_lock);

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

void syscall_handler(struct intr_frame *f)
{
	// 아래 코드 printf 활성화 하면 테스트 케이스 통과 못 함 ㅎㅎ
	// printf("[syscall] syscall_handler - system call!\n");

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
		f->R.rax = exec(f->R.rdi);
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
		break;
	}
}

// /* Checks if the given user pointer is valid */
// /* 주어진 사용자 포인터가 유효한지 확인합니다. */
// static bool is_valid_user_ptr(const void *uaddr)
// {
// 	return (uaddr != NULL) && is_user_vaddr(uaddr) && pml4_get_page(thread_current()->pml4, uaddr) != NULL;
// }

// /* Checks if the given user memory region is valid */
// /* 지정된 사용자 메모리 영역이 유효한지 확인합니다. */
// static bool is_valid_user_region(const void *uaddr, size_t len)
// {
// 	return is_valid_user_ptr(uaddr) && is_valid_user_ptr(uaddr + len - 1);
// }

/* check_address - 주소가 유효한지 확인한다.
 */
void check_address(uintptr_t addr)
{
	if (addr == NULL)
	{
		exit(-1);
	}

	if (pml4_get_page(thread_current()->pml4, (void *)addr) == NULL)
	{
		exit(-1);
	}

	if (!is_user_vaddr(addr))
	{
		exit(-1);
	}

	if (KERN_BASE < addr || addr < 0)
	{
		exit(-1);
	}

	if (KERN_BASE < addr + 8 || addr + 8 < 0)
	{
		exit(-1);
	}
}

void halt()
{
	power_off();
}

/* Handles the exit system call. */
void exit(int status)
{
	// 현재 스레드의 exit_status를 설정
	struct thread *curr = thread_current();
	curr->exit_status = status;

	/* 프로세스 종료 메시지를 출력
	 * 프로세스 이름과 종료 상태를 출력 */
	printf("%s: exit(%d)\n", curr->name, status);
	// sema_up(&curr->child_wait_sema);

	// 프로세스 종료
	thread_exit();
}

int fork(const char *thread_name, struct intr_frame *f)
{
	return process_fork(thread_name, f);
}

bool exec(const char *cmd_line)
{
	check_address(cmd_line);

	char *cp_name = palloc_get_page(0);

	if (cp_name == NULL)
	{
		exit(-1);
	}

	strlcpy(cp_name, cmd_line, PGSIZE);

	if (process_exec(cp_name) == -1)
	{
		palloc_free_page(cp_name);
		return false;
	}
}

/* wait - 자식 프로세스 pid를 기다렸다가 자식의 종료 상태를 확인한다.
 * 해당 자식 프로세스가 아직 실행 중이면 종료될 때까지 기다린다.
 * 그리고 자식 프로세스가 종료되면, 종료 시에 전달된 상태를 반환한다.
 * 자식 프로세스가 exit()를 호출하지 않았지만 커널에 의해 종료된 경우
 * (예: 예외로 인해 종료된 경우) -1을 반환해야 합니다.
 *
 * 부모 프로세스가 이미 종료된 자식 프로세스를 기다리는 것은 가능하지만,
 * 커널은 여전히 부모가 자식의 종료 상태를 확인하거나 자식이 커널에 의해
 * 종료되었음을 알 수 있도록 해야 한다.
 *
 * wait은 아래의 경우 중 하나라도 해당된다면 즉시 -1을 반환해야 한다.
 *
 * 1. pid가 호출 프로세스의 직접적인 자식이 아닐 때
 * -> 호출 프로세스가 성공적인 fork 호출으로 반환 값 pid를 받았다면,
 *    해당 pid가 호출 프로세스의 직접적인 자식이다.
 * -> 자식은 상속되지 않는다. A가 자식 프로세스 B를 생성하고 B가 자식
 *    프로세스 C를 생성하는 경우, B가 죽었더라도 A는 C를 기다릴 수 없다.
 * -> 프로세스 A의 wait(C) 호출은 실패해야 한다. 마찬가지로 고아
 *    프로세스는 부모 프로세스가 먼저 종료되면 새 부모에게 할당되지 않는다.
 *
 * 2. wait 호출하는 프로세스가 이미 pid에서 wait을 호출했을 때
 * -> 즉, 프로세스는 최대 한 번만 특정 자식을 기다릴 수 있다.
 */
int wait(tid_t child_tid)
{
	// 자식 프로세스의 tid를 인자로 받아 해당 자식 프로세스를 대기
	return process_wait(child_tid);
}

/* create - 처음에 initial_size 바이트 크기의 파일이라는 새 파일을 만듭니다.
 * 성공하면 참을 반환하고, 그렇지 않으면 거짓을 반환합니다.
 * 새 파일을 만든다고 해서 파일이 열리지는 않습니다.
 * 새 파일을 열려면 시스템 호출이 필요한 별도의 작업입니다.
 */
bool create(const char *file, unsigned initial_size)
{
	check_address(file);

	return filesys_create(file, initial_size);
}

/* remove - file이라는 파일을 삭제합니다.
 * 성공하면 참을 반환하고, 그렇지 않으면 거짓을 반환합니다.
 * 파일은 열려 있는지 여부에 관계없이 제거할 수 있으며 열려 있는 파일을 제거해도 닫히지 않습니다.
 * 자세한 내용은 FAQ에서 열려 있는 파일 제거하기를 참조하세요.
 */
bool remove(const char *file)
{
	check_address(file);

	return filesys_remove(file);
}

/* open - file이라는 파일을 연다.
 * fd가 음수가 아닌 정수 핸들을 반환하거나, 파일을 열 수 없는 경우 -1을 반환한다.
 * fd 0/1은 콘솔 용으로 예약되어 있다.
 * fd 0: STDIN_FILENO - 표준 입력
 * fd 1: STDOUT_FILENO - 표준 출력
 *
 * 이 함수는 fd 0/1 중 어느 것도 반환하지 않으며 시스템 콜 인수로만 사용된다.
 *
 * 각각의 프로세스는 독립적인 FDT(File Descriptor Table)을 가지며,
 * fd는 자식 프로세스에 의해 상속된다.
 *
 * 같은 프로세스에서든, 다른 프로세스에서든 단일 파일을 두번 이상 열면
 * 열 때마다 새 fd가 반환된다.
 *
 * 단일 파일에 대한 서로 다른 fd는 별도의 close()를 통해
 * 독립적으로 닫히며 파일 위치를 공유하지 않는다.
 *
 * 추가 작업을 수행하려면 0부터 시작하는 정수를 반환하는 Linux 체계를 따라야 한다.
 */
int open(const char *file)
{
	check_address(file);

	struct file *file_open = filesys_open(file);

	if (file_open == NULL)
	{
		return -1;
	}

	int fd = add_file_to_fdt(file_open);

	if (fd == -1)
	{
		file_close(file_open);
	}

	return fd;
}

/* filesize - fd로 열린 파일의 크기를 바이트 단위로 반환합니다.
 */
int filesize(int fd)
{
	struct file *_file = get_file_from_fd(fd);

	if (_file == NULL)
	{
		return -1;
	}

	return file_length(_file);
}

/* read - fd로 열린 파일에서 buffer로 size 바이트를 읽는다.
 * 실제로 읽은 바이트 수(파일 끝에서 0) 또는
 * 파일을 읽을 수 없는 경우(파일 끝이 아닌 다른 조건으로 인해) -1을 반환한다.
 * fd 0은 input_getc()를 사용하여 키보드에서 읽는다.
 */
int read(int fd, void *buffer, unsigned size)
{
	check_address(buffer);

	struct file *_file = get_file_from_fd(fd);

	if (_file == NULL)
	{
		return -1;
	}

	int byte = 0;
	char *_buffer = buffer;

	if (fd == 0)
	{
		while (byte < size)
		{
			_buffer[byte++] = input_getc();
		}
		return byte;
	}

	return file_read(_file, buffer, size);
}

/* write - fd로 열린 파일에 buffer에서 size 바이트를 쓴다.
 * 실제로 쓰여진 바이트 수를 반환하며, 일부 바이트가 쓰여지지 않은 경우 크기보다 작을 수 있습니다.
 * 파일 끝을 지나서 쓰면 일반적으로 파일이 확장되지만 기본 파일 시스템에서는 파일 확장이 구현되지 않습니다.
 * 예상되는 동작은 파일 끝 부분까지 가능한 한 많은 바이트를 쓰고 실제 쓰여진 바이트 수를 반환하거나 전혀 쓸 수 없는 경우 0을 반환하는 것입니다.
 * 콘솔에 쓰는 코드는 적어도 크기가 수백 바이트보다 크지 않은 한 putbuf() 호출 한 번으로 모든 버퍼를 써야 합니다(큰 버퍼는 분할하는 것이 합리적입니다).
 * 그렇지 않으면 다른 프로세스에서 출력한 텍스트 줄이 콘솔에 인터리빙되어 사람이 읽는 사람과 채점 스크립트 모두를 혼란스럽게 만들 수 있습니다.
 */
int write(int fd, const void *buffer, unsigned size)
{
	check_address(buffer);

	if (fd == 0)
	{
		return -1;
	}

	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}

	struct file *_file = get_file_from_fd(fd);

	if (_file == NULL)
	{
		return -1;
	}

	return file_write(_file, buffer, size);
}

/* 열린 파일 fd에서 읽거나 쓸 다음 바이트를 파일 시작부터 바이트 단위로 표시되는
 * 위치로 변경한다. (따라서 위치가 0이면 파일의 시작). 파일의 현재 끝을 지나서
 * 찾는 것은 오류가 아니다. 나중에 읽으면 파일 끝을 나타내는 0 바이트를 얻는다.
 * 나중에 쓰기는 파일을 확장하여 기록되지 않은 간격을 0으로 채웁니다. (단,
 * 핀토스에서는 프로젝트 4가 완료될 때까지 파일 길이가 고정되어 있으므로 파일
 * 끝을 지나서 쓰면 오류가 반환된다.) 이러한 의미는 파일 시스템에서 구현되며
 * 시스템 호출 구현에 특별한 노력이 필요하지 않습니다. */
void seek(int fd, unsigned position)
{
	struct file *_file = get_file_from_fd(fd);

	if (_file == NULL)
	{
		exit(-1);
	}

	file_seek(_file, position);
}

/* 열린 파일 fd에서 읽거나 쓸 다음 바이트의 위치를
 * 파일 시작 부분부터 바이트 단위로 반환합니다. */
unsigned tell(int fd)
{
	struct file *_file = get_file_from_fd(fd);

	if (_file == NULL)
	{
		exit(-1);
	}

	return file_tell(_file);
}

/* fd를 닫는다. 프로세스를 exit하거나 terminate하면
 * 열려 있는 모든 파일 기술자가 닫혀야 한다. */
void close(int fd)
{
	struct file *_file = get_file_from_fd(fd);

	if (_file == NULL)
	{
		exit(-1);
	}

	file_close(_file);
	remove_file_from_fdt(fd);
}

// file을 fdt에 추가하고 fd를 반환한다.
int add_file_to_fdt(struct file *file)
{
	struct thread *t = thread_current();
	int fd = 2;

	while (t->fdt[fd] != NULL && fd < FDT_SIZE)
	{
		fd++;
	}

	if (fd >= FDT_SIZE)
	{
		return -1;
	}

	t->fdt[fd] = file;

	return fd;
}

// fd에 해당하는 file을 fdt에서 제거한다.
void remove_file_from_fdt(int fd)
{
	struct thread *t = thread_current();

	t->fdt[fd] = NULL;
}

// fd에 해당하는 file을 반환한다.
struct file *get_file_from_fd(int fd)
{
	if (fd < 2 || fd >= FDT_SIZE)
	{
		return NULL;
	}

	struct thread *t = thread_current();
	struct file *_file = t->fdt[fd];

	if (_file == NULL)
	{
		return NULL;
	}

	return _file;
}