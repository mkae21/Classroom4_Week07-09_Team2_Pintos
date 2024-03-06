#include "list.h"
#include "../debug.h"

/* Our doubly linked lists have two header elements: the "head"
   just before the first element and the "tail" just after the
   last element.  The `prev' link of the front header is null, as
   is the `next' link of the back header.  Their other two links
   point toward each other via the interior elements of the list.

   An empty list looks like this:

	   +------+     +------+
   <---| head |<--->| tail |--->
	   +------+     +------+

   A list with two elements in it looks like this:

	   +------+     +-------+     +-------+     +------+
   <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
	   +------+     +-------+     +-------+     +------+

   The symmetry of this arrangement eliminates lots of special
   cases in list processing.  For example, take a look at
   list_remove(): it takes only two pointer assignments and no
   conditionals.  That's a lot simpler than the code would be
   without header elements.

   (Because only one of the pointers in each header element is used,
   we could in fact combine them into a single header element
   without sacrificing this simplicity.  But using two separate
   elements allows us to do a little bit of checking on some
   operations, which can be valuable.) */

/* 이중 연결 리스트에는 두 개의 헤더 요소, 즉 첫 번째 요소 바로 앞의
   '머리'와 마지막 요소 바로 뒤의 '꼬리'가 있습니다. 앞쪽 헤더의
   '이전' 링크는 null이고, 뒤쪽 헤더의 '다음' 링크도 마찬가지입니다.
   다른 두 링크는 목록의 내부 요소를 통해 서로를 가리킵니다.

   빈 리스트는 다음과 같습니다:

	   +------+     +------+
   <---| head |<--->| tail |--->
	   +------+     +------+

   두 개의 요소가 포함된 리스트는 다음과 같습니다:

	   +------+     +-------+     +-------+     +------+
   <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
	   +------+     +-------+     +-------+     +------+

   이 배열의 대칭성은 리스트 처리에서 많은 특수한 경우를 제거합니다.
   예를 들어 list_remove()를 살펴보면 포인터 할당이 두 개만 필요하고
   조건문은 필요하지 않습니다. 헤더 요소가 없는 코드보다 훨씬 간단합니다.

   (각 헤더 요소의 포인터 중 하나만 사용되기 때문에 실제로는 이러한
   단순성을 희생하지 않고도 하나의 헤더 요소로 결합할 수 있습니다.
   하지만 두 개의 개별 요소를 사용하면 일부 연산을 약간 더 검사할 수 있어
   유용할 수 있습니다.) */

static bool is_sorted(struct list_elem *a, struct list_elem *b,
					  list_less_func *less, void *aux) UNUSED;

/* Returns true if ELEM is a head, false otherwise. */
/* ELEM이 head이면 true를 반환하고, 그렇지 않으면 false를 반환합니다. */
static inline bool is_head(struct list_elem *elem)
{
	return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* Returns true if ELEM is an interior element,
   false otherwise. */
/* ELEM이 내부 요소이면 true를 반환하고, 그렇지 않으면 false를 반환합니다. */
static inline bool is_interior(struct list_elem *elem)
{
	return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* Returns true if ELEM is a tail, false otherwise. */
/* ELEM이 꼬리이면 참을 반환하고, 그렇지 않으면 거짓을 반환합니다. */
static inline bool is_tail(struct list_elem *elem)
{
	return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

/* Initializes LIST as an empty list. */
/* LIST를 빈 목록으로 초기화합니다. */
void list_init(struct list *list)
{
	ASSERT(list != NULL);
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

/* Returns the beginning of LIST. */
/* 리스트의 시작 부분을 반환합니다. */
struct list_elem *list_begin(struct list *list)
{
	ASSERT(list != NULL);
	return list->head.next;
}

/* Returns the element after ELEM in its list.  If ELEM is the
   last element in its list, returns the list tail.  Results are
   undefined if ELEM is itself a list tail. */
/* 리스트에서 ELEM 뒤에 오는 요소를 반환합니다. ELEM이 리스트의
   마지막 요소인 경우 리스트 꼬리를 반환합니다. ELEM 자체가 리스트
   꼬리인 경우 결과는 정의되지 않습니다. */
struct list_elem *list_next(struct list_elem *elem)
{
	ASSERT(is_head(elem) || is_interior(elem));
	return elem->next;
}

/* Returns LIST's tail.

   list_end() is often used in iterating through a list from
   front to back.  See the big comment at the top of list.h for
   an example. */
/* 리스트의 꼬리를 반환합니다.

   list_end()는 리스트를 앞뒤로 반복할 때 자주 사용됩니다.
   예제는 list.h 상단의 큰 주석을 참조하세요. */
struct list_elem *list_end(struct list *list)
{
	ASSERT(list != NULL);
	return &list->tail;
}

/* Returns the LIST's reverse beginning, for iterating through
   LIST in reverse order, from back to front. */
/* LIST를 뒤에서 앞으로 역순으로 반복하기 위해
   LIST의 역방향 시작을 반환합니다. */
struct list_elem *list_rbegin(struct list *list)
{
	ASSERT(list != NULL);
	return list->tail.prev;
}

/* Returns the element before ELEM in its list.  If ELEM is the
   first element in its list, returns the list head.  Results are
   undefined if ELEM is itself a list head. */
/* 목록에서 ELEM 앞의 요소를 반환합니다. ELEM이 목록의 첫 번째
   요소인 경우 목록 헤드를 반환합니다. ELEM 자체가 리스트 헤드인
   경우 결과는 정의되지 않습니다. */
struct list_elem *list_prev(struct list_elem *elem)
{
	ASSERT(is_interior(elem) || is_tail(elem));
	return elem->prev;
}

/* Returns LIST's head.

   list_rend() is often used in iterating through a list in
   reverse order, from back to front.  Here's typical usage,
   following the example from the top of list.h:

   for (e = list_rbegin (&foo_list); e != list_rend (&foo_list);
   e = list_prev (e))
   {
   struct foo *f = list_entry (e, struct foo, elem);
   ...do something with f...
   } */
/* 리스트의 헤드를 반환합니다.

   list_rend()는 종종 리스트를 뒤에서 앞쪽으로, 역순으로 목록을
   반복할 때 자주 사용됩니다. 다음은 일반적인 사용 예시입니다,
   목록의 맨 위에 있는 예시를 따르세요:

   for (e = list_rbegin (&foo_list); e != list_rend (&foo_list); e = list_prev (e))
   {
	   struct foo *f = list_entry (e, 구조체 foo, elem);
	   // ...f로 무언가를 수행...
   } */
struct list_elem *list_rend(struct list *list)
{
	ASSERT(list != NULL);
	return &list->head;
}

/* Return's LIST's head.

   list_head() can be used for an alternate style of iterating
   through a list, e.g.:

   ```
   e = list_head (&list);

   while ((e = list_next (e)) != list_end (&list))
   {
   ...
   }
   ```*/
/* 리스트의 헤드를 반환합니다.

   list_head()는 목록을 반복하는 다른 스타일에 사용할 수 있습니다:

   ```
   e = list_head (&list);

   while ((e = list_next (e)) != list_end (&list))
   {
	   // ...
   }
   ```*/
struct list_elem *list_head(struct list *list)
{
	ASSERT(list != NULL);
	return &list->head;
}

/* Return's LIST's tail. */
/* 리스트의 꼬리를 반환합니다. */
struct list_elem *list_tail(struct list *list)
{
	ASSERT(list != NULL);
	return &list->tail;
}

/* Inserts ELEM just before BEFORE, which may be either an
   interior element or a tail.  The latter case is equivalent to
   list_push_back(). */
/* 내부 요소 또는 꼬리일 수 있는 BEFORE 바로 앞에 ELEM을 삽입합니다.
   후자의 경우는 list_push_back()과 동일합니다. */
void list_insert(struct list_elem *before, struct list_elem *elem)
{
	ASSERT(is_interior(before) || is_tail(before));
	ASSERT(elem != NULL);

	elem->prev = before->prev;
	elem->next = before;
	before->prev->next = elem;
	before->prev = elem;
}

/* Removes elements FIRST though LAST (exclusive) from their
   current list, then inserts them just before BEFORE, which may
   be either an interior element or a tail. */
/* 현재 목록에서 첫 번째부터 마지막(exclusive)까지의 요소를 제거한 다음
   내부 요소 또는 꼬리일 수 있는 BEFORE 바로 앞에 삽입합니다. */
void list_splice(struct list_elem *before,
				 struct list_elem *first, struct list_elem *last)
{
	ASSERT(is_interior(before) || is_tail(before));
	if (first == last)
		return;
	last = list_prev(last);

	ASSERT(is_interior(first));
	ASSERT(is_interior(last));

	/* Cleanly remove FIRST...LAST from its current list. */
	/* 현재 목록에서 첫 번째...마지막을 깨끗하게 제거합니다. */
	first->prev->next = last->next;
	last->next->prev = first->prev;

	/* Splice FIRST...LAST into new list. */
	/* FIRST...LAST를 새 목록으로 연결합니다. */
	first->prev = before->prev;
	last->next = before;
	before->prev->next = first;
	before->prev = last;
}

/* Inserts ELEM at the beginning of LIST, so that it becomes the
   front in LIST. */
/* LIST의 앞부분에 ELEM을 삽입하여 LIST의 앞부분이 되도록 합니다. */
void list_push_front(struct list *list, struct list_elem *elem)
{
	list_insert(list_begin(list), elem);
}

/* Inserts ELEM at the end of LIST, so that it becomes the
   back in LIST. */
/* 리스트의 끝에 ELEM을 삽입하여 리스트의 뒷부분이 되도록 합니다. */
void list_push_back(struct list *list, struct list_elem *elem)
{
	list_insert(list_end(list), elem);
}

/* Removes ELEM from its list and returns the element that
   followed it.  Undefined behavior if ELEM is not in a list.

   It's not safe to treat ELEM as an element in a list after
   removing it.  In particular, using list_next() or list_prev()
   on ELEM after removal yields undefined behavior.  This means
   that a naive loop to remove the elements in a list will fail:

 ** DON'T DO THIS **
 for (e = list_begin (&list); e != list_end (&list); e = list_next (e))
 {
 ...do something with e...
 list_remove (e);
 }
 ** DON'T DO THIS **

 Here is one correct way to iterate and remove elements from a
list:

for (e = list_begin (&list); e != list_end (&list); e = list_remove (e))
{
...do something with e...
}

If you need to free() elements of the list then you need to be
more conservative.  Here's an alternate strategy that works
even in that case:

while (!list_empty (&list))
{
struct list_elem *e = list_pop_front (&list);
...do something with e...
}
*/
/* 목록에서 ELEM을 제거하고 그 뒤에 오는 요소를 반환합니다.
   ELEM이 목록에 없는 경우 정의되지 않은 동작입니다.

   ELEM을 제거한 후 목록의 요소로 취급하는 것은 안전하지 않습니다.
   특히 제거 후 ELEM에 list_next() 또는 list_prev()를 사용하면
   정의되지 않은 동작이 발생합니다. 즉, 목록에서 요소를 제거하는
   순진한 루프는 실패합니다:

   ** 이러지 마세요 **
   for (e = list_begin (&list); e != list_end (&list); e = list_next (e))
   {
	  // ...e로 무언가를 수행...
	  list_remove (e);
   }
   ** 이렇게 하지 마세요 **

   다음은 반복해서 요소를 제거하는 올바른 방법 중 하나입니다.
   list:

   for (e = list_begin (&list); e != list_end (&list); e = list_remove (e))
   {
	   // ...e로 무언가를 수행...
   }

   목록의 요소를 free() 해야 하는 경우에는 보다 보수적으로 사용해야 합니다.
   다음은 효과가 있는 대체 전략입니다. 이 경우에도 작동합니다:

   while (!list_empty (&list))
   {
	   // struct list_elem *e = list_pop_front(&list);
	   // ...e로 뭔가를 하세요...
	}*/
struct list_elem *list_remove(struct list_elem *elem)
{
	ASSERT(is_interior(elem));
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	return elem->next;
}

/* Removes the front element from LIST and returns it.
   Undefined behavior if LIST is empty before removal. */
/* 리스트에서 앞쪽 요소를 제거하고 반환합니다.
   제거하기 전에 LIST가 비어 있으면 정의되지 않은 동작입니다. */
struct list_elem *list_pop_front(struct list *list)
{
	struct list_elem *front = list_front(list);
	list_remove(front);
	return front;
}

/* Removes the back element from LIST and returns it.
   Undefined behavior if LIST is empty before removal. */
/* LIST에서 뒤쪽 요소를 제거하고 반환합니다.
   제거하기 전에 LIST가 비어 있으면 정의되지 않은 동작입니다. */
struct list_elem *list_pop_back(struct list *list)
{
	struct list_elem *back = list_back(list);
	list_remove(back);
	return back;
}

/* Returns the front element in LIST.
   Undefined behavior if LIST is empty. */
/* 리스트의 앞쪽 요소를 반환합니다.
   LIST가 비어 있으면 정의되지 않은 동작입니다. */
struct list_elem *list_front(struct list *list)
{
	ASSERT(!list_empty(list));
	return list->head.next;
}

/* Returns the back element in LIST.
   Undefined behavior if LIST is empty. */
/* LIST의 뒤쪽 요소를 반환합니다.
   LIST가 비어 있으면 정의되지 않은 동작입니다. */
struct list_elem *list_back(struct list *list)
{
	ASSERT(!list_empty(list));
	return list->tail.prev;
}

/* Returns the number of elements in LIST.
   Runs in O(n) in the number of elements. */
/* 리스트에 있는 요소의 수를 반환합니다.
   요소 수에서 O(n) 단위로 실행됩니다. */
size_t list_size(struct list *list)
{
	struct list_elem *e;
	size_t cnt = 0;

	for (e = list_begin(list); e != list_end(list); e = list_next(e))
		cnt++;
	return cnt;
}

/* Returns true if LIST is empty, false otherwise. */
/* LIST가 비어 있으면 참을 반환하고, 그렇지 않으면 거짓을 반환합니다. */
bool list_empty(struct list *list)
{
	return list_begin(list) == list_end(list);
}

/* Swaps the `struct list_elem *'s that A and B point to. */
/* A와 B가 가리키는 '구조체 list_elem *'을 바꿉니다. */
static void swap(struct list_elem **a, struct list_elem **b)
{
	struct list_elem *t = *a;
	*a = *b;
	*b = t;
}

/* Reverses the order of LIST. */
/* LIST의 순서를 반대로 합니다. */
void list_reverse(struct list *list)
{
	if (!list_empty(list))
	{
		struct list_elem *e;

		for (e = list_begin(list); e != list_end(list); e = e->prev)
			swap(&e->prev, &e->next);
		swap(&list->head.next, &list->tail.prev);
		swap(&list->head.next->prev, &list->tail.prev->next);
	}
}

/* Returns true only if the list elements A through B (exclusive)
   are in order according to LESS given auxiliary data AUX. */
/* 주어진 보조 데이터 AUX에 따라 목록 요소 A~B(배타적)가 LESS에
   따라 순서대로 정렬된 경우에만 true를 반환합니다. */
static bool is_sorted(struct list_elem *a, struct list_elem *b,
					  list_less_func *less, void *aux)
{
	if (a != b)
		while ((a = list_next(a)) != b)
			if (less(a, list_prev(a), aux))
				return false;
	return true;
}

/* Finds a run, starting at A and ending not after B, of list
   elements that are in nondecreasing order according to LESS
   given auxiliary data AUX.  Returns the (exclusive) end of the
   run.
   A through B (exclusive) must form a non-empty range. */
/* 주어진 보조 데이터 AUX에 따라 LESS에 따라 감소하지 않는 순서인
   리스트 요소의 A에서 시작하여 B 뒤가 아닌 끝인 실행을 찾습니다.
   실행의 (배타적인) 끝을 반환합니다. A부터 B(배타적)까지는
   비어 있지 않은 범위를 형성해야 합니다. */
static struct list_elem *find_end_of_run(struct list_elem *a, struct list_elem *b,
										 list_less_func *less, void *aux)
{
	ASSERT(a != NULL);
	ASSERT(b != NULL);
	ASSERT(less != NULL);
	ASSERT(a != b);

	do
	{
		a = list_next(a);
	} while (a != b && !less(a, list_prev(a), aux));
	return a;
}

/* Merges A0 through A1B0 (exclusive) with A1B0 through B1
   (exclusive) to form a combined range also ending at B1
   (exclusive).  Both input ranges must be nonempty and sorted in
   nondecreasing order according to LESS given auxiliary data
   AUX. The output range will be sorted the same way. */
/* A0 ~ A1B0(exclusive)을 A1B0 ~ B1(exclusive)과 병합하여
   B1(exclusive)으로 끝나는 합산 범위를 형성합니다. 두 입력
   범위는 모두 비어 있지 않아야 하며 보조 데이터 AUX가 주어지면
   LESS에 따라 비증가 순서로 정렬됩니다. 출력 범위도 같은 방식으로
   정렬됩니다. */
static void inplace_merge(struct list_elem *a0, struct list_elem *a1b0,
						  struct list_elem *b1,
						  list_less_func *less, void *aux)
{
	ASSERT(a0 != NULL);
	ASSERT(a1b0 != NULL);
	ASSERT(b1 != NULL);
	ASSERT(less != NULL);
	ASSERT(is_sorted(a0, a1b0, less, aux));
	ASSERT(is_sorted(a1b0, b1, less, aux));

	while (a0 != a1b0 && a1b0 != b1)
		if (!less(a1b0, a0, aux))
			a0 = list_next(a0);
		else
		{
			a1b0 = list_next(a1b0);
			list_splice(a0, list_prev(a1b0), a1b0);
		}
}

/* Sorts LIST according to LESS given auxiliary data AUX, using a
   natural iterative merge sort that runs in O(n lg n) time and
   O(1) space in the number of elements in LIST. */
/* LIST의 요소 수에서 O(n lg n) 시간과 O(1) 공간에서 실행되는 자연
   반복 병합 정렬을 사용하여 주어진 보조 데이터 AUX에 따라 LIST를
   LESS에 따라 정렬합니다. */
void list_sort(struct list *list, list_less_func *less, void *aux)
{
	size_t output_run_cnt; /* Number of runs output in current pass. */
						   /* 현재 패스에서 출력되는 실행 횟수입니다. */

	ASSERT(list != NULL);
	ASSERT(less != NULL);

	/* Pass over the list repeatedly, merging adjacent runs of
	   nondecreasing elements, until only one run is left. */
	/* 감소하지 않는 요소의 인접한 실행을 병합하여 하나의 실행만
	   남을 때까지 목록을 반복해서 넘깁니다. */
	do
	{
		struct list_elem *a0;	/* Start of first run. */
								/* 첫 실행 시작. */
		struct list_elem *a1b0; /* End of first run, start of second. */
								/* 첫 번째 실행이 끝나고 두 번째 실행이 시작됩니다. */
		struct list_elem *b1;	/* End of second run. */
								/* 두 번째 실행이 끝났습니다. */

		output_run_cnt = 0;
		for (a0 = list_begin(list); a0 != list_end(list); a0 = b1)
		{
			/* Each iteration produces one output run. */
			/* 각 반복은 하나의 출력 실행을 생성합니다. */
			output_run_cnt++;

			/* Locate two adjacent runs of nondecreasing elements
			   A0...A1B0 and A1B0...B1. */
			/* 감소하지 않는 요소 A0...A1B0 및 A1B0...B1의 인접한
			   두 개의 실행을 찾습니다. */
			a1b0 = find_end_of_run(a0, list_end(list), less, aux);
			if (a1b0 == list_end(list))
				break;
			b1 = find_end_of_run(a1b0, list_end(list), less, aux);

			/* Merge the runs. */
			/* 실행을 병합합니다. */
			inplace_merge(a0, a1b0, b1, less, aux);
		}
	} while (output_run_cnt > 1);

	ASSERT(is_sorted(list_begin(list), list_end(list), less, aux));
}

/* Inserts ELEM in the proper position in LIST, which must be
   sorted according to LESS given auxiliary data AUX.
   Runs in O(n) average case in the number of elements in LIST. */
/* LIST의 적절한 위치에 ELEM을 삽입하며, 보조 데이터 AUX가 주어지면
   LESS에 따라 정렬해야 합니다. LIST의 엘리먼트 수에서 O(n) 평균
   대소문자로 실행됩니다. */
void list_insert_ordered(struct list *list, struct list_elem *elem,
						 list_less_func *less, void *aux)
{
	struct list_elem *e;

	ASSERT(list != NULL);
	ASSERT(elem != NULL);
	ASSERT(less != NULL);

	for (e = list_begin(list); e != list_end(list); e = list_next(e))
		if (less(elem, e, aux))
			break;
	return list_insert(e, elem);
}

/* Iterates through LIST and removes all but the first in each
   set of adjacent elements that are equal according to LESS
   given auxiliary data AUX.  If DUPLICATES is non-null, then the
   elements from LIST are appended to DUPLICATES. */
/* LIST를 반복하고 주어진 보조 데이터 AUX에 따라 LESS에 따라
   동일한 인접 요소의 각 집합에서 첫 번째를 제외한 모든 요소를
   제거합니다. DUPLICATES가 null이 아닌 경우, LIST의 요소가
   DUPLICATES에 추가됩니다. */
void list_unique(struct list *list, struct list *duplicates,
				 list_less_func *less, void *aux)
{
	struct list_elem *elem, *next;

	ASSERT(list != NULL);
	ASSERT(less != NULL);
	if (list_empty(list))
		return;

	elem = list_begin(list);
	while ((next = list_next(elem)) != list_end(list))
		if (!less(elem, next, aux) && !less(next, elem, aux))
		{
			list_remove(next);
			if (duplicates != NULL)
				list_push_back(duplicates, next);
		}
		else
			elem = next;
}

/* Returns the element in LIST with the largest value according
   to LESS given auxiliary data AUX.  If there is more than one
   maximum, returns the one that appears earlier in the list.  If
   the list is empty, returns its tail. */
/* 주어진 보조 데이터 AUX에 따라 LESS에 따라 가장 큰 값을 가진
   LIST의 요소를 반환합니다. 최대값이 두 개 이상이면 목록의 맨 앞에
   나타나는 값을 반환합니다. 목록이 비어 있으면 꼬리를 반환합니다. */
struct list_elem *list_max(struct list *list, list_less_func *less, void *aux)
{
	struct list_elem *max = list_begin(list);
	if (max != list_end(list))
	{
		struct list_elem *e;

		for (e = list_next(max); e != list_end(list); e = list_next(e))
			if (less(max, e, aux))
				max = e;
	}
	return max;
}

/* Returns the element in LIST with the smallest value according
   to LESS given auxiliary data AUX.  If there is more than one
   minimum, returns the one that appears earlier in the list.  If
   the list is empty, returns its tail. */
/* 주어진 보조 데이터 AUX에 따라 LESS에 따라 가장 작은 값을 가진
   LIST의 요소를 반환합니다. 최소값이 두 개 이상이면 목록의 맨 앞에
   나타나는 것을 반환합니다. 목록이 비어 있으면 꼬리를 반환합니다. */
struct list_elem *list_min(struct list *list, list_less_func *less, void *aux)
{
	struct list_elem *min = list_begin(list);

	if (min != list_end(list))
	{
		struct list_elem *e;

		for (e = list_next(min); e != list_end(list); e = list_next(e))
			if (less(e, min, aux))
				min = e;
	}

	return min;
}
