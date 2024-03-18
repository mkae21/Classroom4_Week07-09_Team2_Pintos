#ifndef THREADS_LOADER_H
#define THREADS_LOADER_H

/* Constants fixed by the PC BIOS. */
/* PC BIOS에 의해 고정된 상수입니다. */
#define LOADER_BASE 0x7c00 /* Physical address of loader's base. */
                           /* 로더 베이스의 실제 주소입니다. */
#define LOADER_END 0x7e00  /* Physical address of end of loader. */
                           /* 로더 끝의 실제 주소입니다. */

/* Physical address of kernel base. */
/* 커널 베이스의 실제 주소입니다. */
#define LOADER_KERN_BASE 0x8004000000

/* Kernel virtual address at which all physical memory is mapped. */
/* 모든 물리적 메모리가 매핑되는 커널 가상 주소입니다. */
#define LOADER_PHYS_BASE 0x200000

/* Multiboot infos */
/* 멀티 부팅 정보 */
#define MULTIBOOT_INFO 0x7000
#define MULTIBOOT_FLAG MULTIBOOT_INFO
#define MULTIBOOT_MMAP_LEN MULTIBOOT_INFO + 44
#define MULTIBOOT_MMAP_ADDR MULTIBOOT_INFO + 48

#define E820_MAP MULTIBOOT_INFO + 52
#define E820_MAP4 MULTIBOOT_INFO + 56

/* Important loader physical addresses. */
/* 중요 로더 물리적 주소. */
#define LOADER_SIG (LOADER_END - LOADER_SIG_LEN)          /* 0xaa55 BIOS signature. */
                                                          /* 0xaa55 바이오스 서명. */
#define LOADER_ARGS (LOADER_SIG - LOADER_ARGS_LEN)        /* Command-line args. */
                                                          /* 명령줄 인수. */
#define LOADER_ARG_CNT (LOADER_ARGS - LOADER_ARG_CNT_LEN) /* Number of args. */
                                                          /* 인수 개수. */

/* Sizes of loader data structures. */
/* 로더 데이터 구조의 크기. */
#define LOADER_SIG_LEN 2
#define LOADER_ARGS_LEN 128
#define LOADER_ARG_CNT_LEN 4

/* GDT selectors defined by loader.
   More selectors are defined by userprog/gdt.h. */
/* 로더에 의해 정의된 GDT 선택기.
   더 많은 선택기는 userprog/gdt.h에 정의되어 있습니다. */
#define SEL_NULL 0x00  /* Null selector. */
                       /* 널 선택기. */
#define SEL_KCSEG 0x08 /* Kernel code selector. */
                       /* 커널 코드 선택기. */
#define SEL_KDSEG 0x10 /* Kernel data selector. */
                       /* 커널 데이터 선택기. */
#define SEL_UDSEG 0x1B /* User data selector. */
                       /* 사용자 데이터 선택기. */
#define SEL_UCSEG 0x23 /* User code selector. */
                       /* 사용자 코드 선택기. */
#define SEL_TSS 0x28   /* Task-state segment. */
                       /* 작업 상태 세그먼트. */
#define SEL_CNT 8      /* Number of segments. */
                       /* 세그먼트 수입니다. */

#endif /* threads/loader.h */
