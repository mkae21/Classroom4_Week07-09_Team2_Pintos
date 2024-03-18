#include "threads/palloc.h"
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/init.h"
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */
/* 페이지 할당자. 메모리를 페이지 크기(또는 페이지 배수)
   청크로 나눠줍니다. 더 작은 청크를 나눠주는 얼로케이터는
   malloc.h를 참고하세요.

   시스템 메모리는 커널과 사용자 풀이라는 두 개의 "풀"로 나뉩니다.
   사용자 풀은 사용자(가상) 메모리 페이지를 위한 것이고 커널 풀은
   그 외 모든 것을 위한 것입니다. 사용자 프로세스가 미친 듯이
   스왑하는 경우에도 커널은 자체 작업을 위한 메모리가 필요하다는
   것이 이 아이디어의 핵심입니다.

   기본적으로 시스템 RAM의 절반은 커널 풀에, 절반은 사용자 풀에
   할당됩니다. 이는 커널 풀에 비해 지나치게 많은 양이지만
   데모용으로는 괜찮습니다. */

/* A memory pool. */
/* 메모리 풀입니다. */
struct pool
{
	struct lock lock;		 /* Mutual exclusion. */
							 /* 상호 배제. */
	struct bitmap *used_map; /* Bitmap of free pages. */
							 /* 가용 페이지의 비트맵. */
	uint8_t *base;			 /* Base of pool. */
							 /* pool의 최하단. */
};

/* Two pools: one for kernel data, one for user pages. */
/* 두 개의 풀: 하나는 커널 데이터용, 하나는 사용자 페이지용. */
static struct pool kernel_pool, user_pool;

/* Maximum number of pages to put in user pool. */
/* 사용자 풀에 넣을 수 있는 최대 페이지 수입니다. */
size_t user_page_limit = SIZE_MAX;

static void init_pool(struct pool *p, void **bm_base, uint64_t start, uint64_t end);
static bool page_from_pool(const struct pool *, void *page);

/* multiboot info */
/* 멀티 부팅 정보 */
struct multiboot_info
{
	uint32_t flags;
	uint32_t mem_low;
	uint32_t mem_high;
	uint32_t __unused[8];
	uint32_t mmap_len;
	uint32_t mmap_base;
};

/* e820 entry */
/* E820 항목 */
struct e820_entry
{
	uint32_t size;
	uint32_t mem_lo;
	uint32_t mem_hi;
	uint32_t len_lo;
	uint32_t len_hi;
	uint32_t type;
};

/* Represent the range information of the ext_mem/base_mem */
/* ext_mem/base_mem의 범위 정보를 나타냅니다. */
struct area
{
	uint64_t start;
	uint64_t end;
	uint64_t size;
};

#define BASE_MEM_THRESHOLD 0x100000
#define USABLE 1
#define ACPI_RECLAIMABLE 3
#define APPEND_HILO(hi, lo) (((uint64_t)((hi)) << 32) + (lo))

/* Iterate on the e820 entry, parse the range of basemem and extmem. */
/* e820 항목에 대해 반복하고, base_mem과 ext_mem의 범위를 구문 분석합니다. */
static void resolve_area_info(struct area *base_mem, struct area *ext_mem)
{
	struct multiboot_info *mb_info = ptov(MULTIBOOT_INFO);
	struct e820_entry *entries = ptov(mb_info->mmap_base);
	uint32_t i;

	for (i = 0; i < mb_info->mmap_len / sizeof(struct e820_entry); i++)
	{
		struct e820_entry *entry = &entries[i];
		if (entry->type == ACPI_RECLAIMABLE || entry->type == USABLE)
		{
			uint64_t start = APPEND_HILO(entry->mem_hi, entry->mem_lo);
			uint64_t size = APPEND_HILO(entry->len_hi, entry->len_lo);
			uint64_t end = start + size;
			printf("%llx ~ %llx %d\n", start, end, entry->type);

			struct area *area = start < BASE_MEM_THRESHOLD ? base_mem : ext_mem;

			// First entry that belong to this area.
			// 이 영역에 속하는 첫 번째 항목입니다.
			if (area->size == 0)
			{
				*area = (struct area){
					.start = start,
					.end = end,
					.size = size,
				};
			}
			else
			{ // otherwise
				// 그렇지 않으면
				// Extend start
				// 시작 확장
				if (area->start > start)
					area->start = start;
				// Extend end
				// 끝 확장
				if (area->end < end)
					area->end = end;
				// Extend size
				// 크기 확장
				area->size += size;
			}
		}
	}
}

/*
 * Populate the pool.
 * All the pages are manged by this allocator, even include code page.
 * Basically, give half of memory to kernel, half to user.
 * We push base_mem portion to the kernel as much as possible.
 */
/* 풀을 채웁니다.
 * 모든 페이지가 이 할당자에 의해 관리되며 코드 페이지도 포함됩니다.
 * 기본적으로 메모리의 절반은 커널에, 절반은 사용자에게 할당합니다.
 * 가능한 한 base_mem 부분을 커널에 푸시합니다. */
static void populate_pools(struct area *base_mem, struct area *ext_mem)
{
	extern char _end;
	void *free_start = pg_round_up(&_end);

	uint64_t total_pages = (base_mem->size + ext_mem->size) / PGSIZE;
	uint64_t user_pages = total_pages / 2 > user_page_limit ? user_page_limit : total_pages / 2;
	uint64_t kern_pages = total_pages - user_pages;

	// Parse E820 map to claim the memory region for each pool.
	// E820 맵을 구문 분석하여 각 풀의 메모리 영역을 요청합니다.
	enum
	{
		KERN_START,
		KERN,
		USER_START,
		USER
	} state = KERN_START;
	uint64_t rem = kern_pages;
	uint64_t region_start = 0, end = 0, start, size, size_in_pg;

	struct multiboot_info *mb_info = ptov(MULTIBOOT_INFO);
	struct e820_entry *entries = ptov(mb_info->mmap_base);

	uint32_t i;
	for (i = 0; i < mb_info->mmap_len / sizeof(struct e820_entry); i++)
	{
		struct e820_entry *entry = &entries[i];
		if (entry->type == ACPI_RECLAIMABLE || entry->type == USABLE)
		{
			start = (uint64_t)ptov(APPEND_HILO(entry->mem_hi, entry->mem_lo));
			size = APPEND_HILO(entry->len_hi, entry->len_lo);
			end = start + size;
			size_in_pg = size / PGSIZE;

			if (state == KERN_START)
			{
				region_start = start;
				state = KERN;
			}

			switch (state)
			{
			case KERN:
				if (rem > size_in_pg)
				{
					rem -= size_in_pg;
					break;
				}
				// generate kernel pool
				// 커널 풀 생성
				init_pool(&kernel_pool,
						  &free_start, region_start, start + rem * PGSIZE);
				// Transition to the next state
				// 다음 상태로 전환
				if (rem == size_in_pg)
				{
					rem = user_pages;
					state = USER_START;
				}
				else
				{
					region_start = start + rem * PGSIZE;
					rem = user_pages - size_in_pg + rem;
					state = USER;
				}
				break;
			case USER_START:
				region_start = start;
				state = USER;
				break;
			case USER:
				if (rem > size_in_pg)
				{
					rem -= size_in_pg;
					break;
				}
				ASSERT(rem == size);
				break;
			default:
				NOT_REACHED();
			}
		}
	}

	// generate the user pool
	// 사용자 풀 생성
	init_pool(&user_pool, &free_start, region_start, end);

	// Iterate over the e820_entry. Setup the usable.
	// e820_entry를 반복합니다. 사용 가능한 것을 설정합니다.
	uint64_t usable_bound = (uint64_t)free_start;
	struct pool *pool;
	void *pool_end;
	size_t page_idx, page_cnt;

	for (i = 0; i < mb_info->mmap_len / sizeof(struct e820_entry); i++)
	{
		struct e820_entry *entry = &entries[i];
		if (entry->type == ACPI_RECLAIMABLE || entry->type == USABLE)
		{
			uint64_t start = (uint64_t)
				ptov(APPEND_HILO(entry->mem_hi, entry->mem_lo));
			uint64_t size = APPEND_HILO(entry->len_hi, entry->len_lo);
			uint64_t end = start + size;

			// TODO: add 0x1000 ~ 0x200000, This is not a matter for now.
			// All the pages are unuable
			// TODO: 0x1000 ~ 0x200000 추가, 지금은 문제가 되지 않습니다.
			// 모든 페이지를 사용할 수 없습니다.
			if (end < usable_bound)
				continue;

			start = (uint64_t)
				pg_round_up(start >= usable_bound ? start : usable_bound);
		split:
			if (page_from_pool(&kernel_pool, (void *)start))
				pool = &kernel_pool;
			else if (page_from_pool(&user_pool, (void *)start))
				pool = &user_pool;
			else
				NOT_REACHED();

			pool_end = pool->base + bitmap_size(pool->used_map) * PGSIZE;
			page_idx = pg_no(start) - pg_no(pool->base);
			if ((uint64_t)pool_end < end)
			{
				page_cnt = ((uint64_t)pool_end - start) / PGSIZE;
				bitmap_set_multiple(pool->used_map, page_idx, page_cnt, false);
				start = (uint64_t)pool_end;
				goto split;
			}
			else
			{
				page_cnt = ((uint64_t)end - start) / PGSIZE;
				bitmap_set_multiple(pool->used_map, page_idx, page_cnt, false);
			}
		}
	}
}

/* Initializes the page allocator and get the memory size */
/* 페이지 할당자를 초기화하고 메모리 크기를 가져옵니다. */
uint64_t palloc_init(void)
{
	/* End of the kernel as recorded by the linker.
	   See kernel.lds.S. */
	/* 링커에 의해 기록된 커널의 끝입니다.
	   kernel.lds.S를 참조하십시오. */
	extern char _end;
	struct area base_mem = {.size = 0};
	struct area ext_mem = {.size = 0};

	resolve_area_info(&base_mem, &ext_mem);
	printf("Pintos booting with: \n");
	printf("\tbase_mem: 0x%llx ~ 0x%llx (Usable: %'llu kB)\n",
		   base_mem.start, base_mem.end, base_mem.size / 1024);
	printf("\text_mem: 0x%llx ~ 0x%llx (Usable: %'llu kB)\n",
		   ext_mem.start, ext_mem.end, ext_mem.size / 1024);
	populate_pools(&base_mem, &ext_mem);
	return ext_mem.end;
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
/* PAGE_CNT만큼 연속된 가용 페이지 그룹을 가져와 반환합니다.
   PAL_USER가 설정되어 있으면 사용자 풀에서, 그렇지 않으면 커널
   풀에서 페이지를 가져옵니다. FLAGS에 PAL_ZERO가 설정되어 있으면,
   페이지가 0으로 채워집니다. 사용 가능한 페이지가 너무 적으면
   널 포인터를 반환하지만, PAL_ASSERT가 FLAGS에 설정되어 있지 않으면
   커널이 패닉에 빠집니다. */
void *palloc_get_multiple(enum palloc_flags flags, size_t page_cnt)
{
	struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;

	lock_acquire(&pool->lock);
	size_t page_idx = bitmap_scan_and_flip(pool->used_map, 0, page_cnt, false);
	lock_release(&pool->lock);

	void *pages = page_idx != BITMAP_ERROR ? pool->base + PGSIZE * page_idx : NULL;

	if (pages)
	{
		if (flags & PAL_ZERO)
		{
			memset(pages, 0, PGSIZE * page_cnt);
		}
	}
	else
	{
		if (flags & PAL_ASSERT)
		{
			PANIC("palloc_get: out of pages");
		}
	}

	return pages;
}

/* Obtains a single free page and returns its kernel virtual
   address.
   If PAL_USER is set, the page is obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the page is filled with zeros.  If no pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
/* 하나의 가용 페이지를 가져와 커널 가상 주소를 반환합니다.
   PAL_USER가 설정되어 있으면 사용자 풀에서, 그렇지 않으면 커널 풀에서
   페이지를 가져옵니다. FLAGS에 PAL_ZERO가 설정되어 있으면 페이지가
   0으로 채워집니다. 가용 페이지가 없으면 널 포인터를 반환하지만,
   PAL_ASSERT가 FLAGS에 설정되어 있지 않으면 커널이 패닉에 빠집니다. */
void *palloc_get_page(enum palloc_flags flags)
{
	return palloc_get_multiple(flags, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
/* PAGES부터 시작하는 PAGE_CNT 페이지를 해제합니다. */
void palloc_free_multiple(void *pages, size_t page_cnt)
{
	struct pool *pool;

	ASSERT(pg_ofs(pages) == 0);
	if (pages == NULL || page_cnt == 0)
		return;

	if (page_from_pool(&kernel_pool, pages))
		pool = &kernel_pool;
	else if (page_from_pool(&user_pool, pages))
		pool = &user_pool;
	else
		NOT_REACHED();

	size_t page_idx = pg_no(pages) - pg_no(pool->base);

#ifndef NDEBUG
	memset(pages, 0xcc, PGSIZE * page_cnt);
#endif
	ASSERT(bitmap_all(pool->used_map, page_idx, page_cnt));
	bitmap_set_multiple(pool->used_map, page_idx, page_cnt, false);
}

/* Frees the page at PAGE. */
/* PAGE에서 페이지를 해제합니다. */
void palloc_free_page(void *page)
{
	palloc_free_multiple(page, 1);
}

/* Initializes pool P as starting at START and ending at END */
/* pool P를 START에서 시작하여 END에서 끝나는 것으로 초기화합니다. */
static void init_pool(struct pool *p, void **bm_base, uint64_t start, uint64_t end)
{
	/* We'll put the pool's used_map at its base.
	   Calculate the space needed for the bitmap
	   and subtract it from the pool's size. */
	/* pool의 used_map을 베이스에 넣겠습니다.
	   비트맵에 필요한 공간을 계산하여 pool의 크기에서 뺍니다. */
	uint64_t pgcnt = (end - start) / PGSIZE;
	size_t bm_pages = DIV_ROUND_UP(bitmap_buf_size(pgcnt), PGSIZE) * PGSIZE;

	lock_init(&p->lock);
	p->used_map = bitmap_create_in_buf(pgcnt, *bm_base, bm_pages);
	p->base = (void *)start;

	// Mark all to unusable.
	// 모두 사용 불가능으로 표시합니다.
	bitmap_set_all(p->used_map, true);

	*bm_base += bm_pages;
}

/* Returns true if PAGE was allocated from POOL,
   false otherwise. */
/* PAGE가 POOL에서 할당된 경우 true를 반환하고,
   그렇지 않으면 false를 반환합니다. */
static bool page_from_pool(const struct pool *pool, void *page)
{
	size_t page_no = pg_no(page);
	size_t start_page = pg_no(pool->base);
	size_t end_page = start_page + bitmap_size(pool->used_map);
	return page_no >= start_page && page_no < end_page;
}
