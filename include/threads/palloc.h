#ifndef THREADS_PALLOC_H
#define THREADS_PALLOC_H

#include <stdint.h>
#include <stddef.h>

/* How to allocate pages. */
/* 페이지 할당 방법. */
enum palloc_flags
{
	PAL_ASSERT = 001, /* Panic on failure. */
					  /* 실패 시 패닉. */
	PAL_ZERO = 002,	  /* Zero page contents. */
					  /* 페이지 내용을 0으로 설정합니다. */
	PAL_USER = 004	  /* User page. */
					  /* 사용자 페이지. */
};

/* Maximum number of pages to put in user pool. */
/* 사용자 풀에 넣을 수 있는 최대 페이지 수입니다. */
extern size_t user_page_limit;

uint64_t palloc_init(void);
void *palloc_get_page(enum palloc_flags);
void *palloc_get_multiple(enum palloc_flags, size_t page_cnt);
void palloc_free_page(void *);
void palloc_free_multiple(void *, size_t page_cnt);

#endif /* threads/palloc.h */
