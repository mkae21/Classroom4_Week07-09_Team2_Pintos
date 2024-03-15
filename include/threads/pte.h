#ifndef THREADS_PTE_H
#define THREADS_PTE_H

#include "threads/vaddr.h"

/* Functions and macros for working with x86 hardware page tables.
 * See vaddr.h for more generic functions and macros for virtual addresses.
 *
 * Virtual addresses are structured as follows: */
/* x86 하드웨어 페이지 테이블 작업을 위한 함수 및 매크로.
 * 가상 주소에 대한 보다 일반적인 함수와 매크로는 vaddr.h를 참조하세요.
 *
 * 가상 주소의 구조는 다음과 같습니다. */
/*  63          48 47            39 38            30 29            21 20         12 11         0
 * +-------------+----------------+----------------+----------------+-------------+------------+
 * | Sign Extend |    Page-Map    | Page-Directory | Page-directory |  Page-Table |  Physical  |
 * |             | Level-4 Offset |    Pointer     |     Offset     |   Offset    |   Offset   |
 * +-------------+----------------+----------------+----------------+-------------+------------+
 *               |                |                |                |             |            |
 *               +------- 9 ------+------- 9 ------+------- 9 ------+----- 9 -----+---- 12 ----+
 *                                         Virtual Address
 */
#define PML4SHIFT 39UL
#define PDPESHIFT 30UL
#define PDXSHIFT 21UL
#define PTXSHIFT 12UL

#define PML4(la) ((((uint64_t)(la)) >> PML4SHIFT) & 0x1FF)
#define PDPE(la) ((((uint64_t)(la)) >> PDPESHIFT) & 0x1FF)
#define PDX(la) ((((uint64_t)(la)) >> PDXSHIFT) & 0x1FF)
#define PTX(la) ((((uint64_t)(la)) >> PTXSHIFT) & 0x1FF)
#define PTE_ADDR(pte) ((uint64_t)(pte) & ~0xFFF)

/* The important flags are listed below.
   When a PDE or PTE is not "present", the other flags are
   ignored.
   A PDE or PTE that is initialized to 0 will be interpreted as
   "not present", which is just fine. */
/* 중요한 플래그는 다음과 같습니다.
   PDE 또는 PTE가 "존재하지 않음"이면 다른 플래그는 무시됩니다.
   0으로 초기화된 PDE 또는 PTE는 "존재하지 않음"으로 해석되며, 이는 괜찮습니다. */
#define PTE_FLAGS 0x00000000000000fffUL     /* Flag bits. */
#define PTE_ADDR_MASK 0xffffffffffffff000UL /* Address bits. */
#define PTE_AVL 0x00000e00                  /* Bits available for OS use. */
#define PTE_P 0x1                           /* 1=present, 0=not present. */
                                            /* 1=있음, 0=없음. */
#define PTE_W 0x2                           /* 1=read/write, 0=read-only. */
#define PTE_U 0x4                           /* 1=user/kernel, 0=kernel only. */
#define PTE_A 0x20                          /* 1=accessed, 0=not acccessed. */
#define PTE_D 0x40                          /* 1=dirty, 0=not dirty (PTEs only). */

#endif /* threads/pte.h */
