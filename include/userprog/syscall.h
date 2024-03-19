#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);

// 파일 접근 동기화 lock
struct lock filesys_lock;

#endif /* userprog/syscall.h */
