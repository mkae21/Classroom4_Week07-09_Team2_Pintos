#ifndef THREADS_VADDR_H
#define THREADS_VADDR_H

#include <debug.h>
#include <stdint.h>
#include <stdbool.h>

#include "threads/loader.h"

/* Functions and macros for working with virtual addresses.
 *
 * See pte.h for functions and macros specifically for x86
 * hardware page tables. */
/* 가상 주소 작업을 위한 함수 및 매크로.
 *
 * x86 하드웨어 페이지 테이블을 위한 함수와 매크로는
 * pte.h를 참조하세요. */

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* Page offset (bits 0:12). */
/* 페이지 오프셋 비트(0:12). */
#define PGSHIFT 0						/* Index of first offset bit. */
										/* 첫 번째 오프셋 비트의 인덱스입니다. */
#define PGBITS 12						/* Number of offset bits. */
										/* 오프셋 비트 수입니다. */
#define PGSIZE (1 << PGBITS)			/* Bytes in a page. */
										/* 페이지의 바이트. */
#define PGMASK BITMASK(PGSHIFT, PGBITS) /* Page offset bits (0:12). */
										/* 페이지 오프셋 비트(0:12). */

/* Offset within a page. */
/* 페이지 내 오프셋. */
#define pg_ofs(va) ((uint64_t)(va) & PGMASK)

#define pg_no(va) ((uint64_t)(va) >> PGBITS)

/* Round up to nearest page boundary. */
/* 가장 가까운 페이지 경계로 반올림(5를 올림)합니다. */
#define pg_round_up(va) ((void *)(((uint64_t)(va) + PGSIZE - 1) & ~PGMASK))

/* Round down to nearest page boundary. */
/* 가장 가까운 페이지 경계로 반내림(5을 내림)합니다. */
#define pg_round_down(va) (void *)((uint64_t)(va) & ~PGMASK)

/* Kernel virtual address start */
/* 커널 가상 주소 시작 */
#define KERN_BASE LOADER_KERN_BASE

/* User stack start */
/* 사용자 스택 시작 */
#define USER_STACK 0x47480000

/* Returns true if VADDR is a user virtual address. */
/* VADDR이 사용자 가상 주소인 경우 true를 반환합니다. */
#define is_user_vaddr(vaddr) (!is_kernel_vaddr((vaddr)))

/* Returns true if VADDR is a kernel virtual address. */
/* VADDR이 커널 가상 주소인 경우 true를 반환합니다. */
#define is_kernel_vaddr(vaddr) ((uint64_t)(vaddr) >= KERN_BASE)

// FIXME: add checking
/* Returns kernel virtual address at which physical address PADDR
 *  is mapped. */
// FIXME: 검사 추가
/* 실제 주소 PADDR이 매핑된 커널 가상 주소를 반환합니다. */
#define ptov(paddr) ((void *)(((uint64_t)paddr) + KERN_BASE))

/* Returns physical address at which kernel virtual address VADDR
 * is mapped. */
/* 커널 가상 주소 VADDR이 매핑된 실제 주소를 반환합니다. */
#define vtop(vaddr)                                \
	({                                             \
		ASSERT(is_kernel_vaddr(vaddr));            \
		((uint64_t)(vaddr) - (uint64_t)KERN_BASE); \
	})

#endif /* threads/vaddr.h */
