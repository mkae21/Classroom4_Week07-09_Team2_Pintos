/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */
/* 이 파일은 Nachos 교육용 운영체제의 소스 코드에서 파생되었습니다.
   Nachos 저작권 고지는 아래에 전문을 옮겨 놓았습니다. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */
/* 저작권 (c) 1992-1996 캘리포니아 대학교 리전트. 모든 권리 보유.

   본 소프트웨어의 모든 사본에 위의 저작권 고지 및 다음 두 단락이
   표시되는 경우, 본 소프트웨어 및 해당 설명서를 어떠한 목적으로든
   수수료 없이 서면 계약 없이 사용, 복사, 수정 및 배포할 수 있는
   권한이 부여됩니다.

   어떠한 경우에도 캘리포니아 대학교는 본 소프트웨어 및 해당 설명서의
   사용으로 인해 발생하는 직접, 간접, 특별, 부수적 또는 결과적 손해에
   대해 어떠한 당사자에게도 책임을 지지 않으며, 이는 캘리포니아 대학교가
   그러한 손해의 가능성을 미리 알고 있었다고 하더라도 마찬가지입니다.

   캘리포니아 대학교는 상품성 및 특정 목적에의 적합성에 대한 묵시적
   보증을 포함하되 이에 국한되지 않는 모든 보증을 구체적으로 부인합니다.
   본 계약에 따라 제공되는 소프트웨어는 "있는 그대로" 제공되며,
   캘리포니아 대학교는 유지보수, 지원, 업데이트, 개선 또는 수정을 제공할
   의무가 없습니다.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
/* 세마포어 SEMA를 VALUE로 초기화합니다. 세마포어는 음수가
   아닌 정수와 이를 조작하기 위한 두 개의 원자 연산자입니다:
   - down 또는 "P": 값이 양수가 될 때까지 기다린 다음 값을 감소시킵니다.
   - up 또는 "V": 값을 증가시킵니다(대기 중인 스레드가 하나 있으면 깨웁니다). */
void sema_init(struct semaphore *sema, unsigned value)
{
   ASSERT(sema != NULL);

   sema->value = value;
   list_init(&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
/* 세마포어에서 다운 또는 "P" 연산. 세마 값이 양수가 될 때까지 기다렸다가
   원자적으로 감소시킵니다.

   이 함수는 잠자기 상태가 될 수 있으므로 인터럽트 핸들러 내에서 호출해서는
   안 됩니다. 이 함수는 인터럽트가 비활성화된 상태에서 호출할 수 있지만,
   잠자기 상태가 되면 다음 예약된 스레드가 인터럽트를 다시 켤 수 있습니다.
   이것이 sema_down 함수입니다. */
void sema_down(struct semaphore *sema)
{
   enum intr_level old_level;

   ASSERT(sema != NULL);
   ASSERT(!intr_context());

   old_level = intr_disable();
   while (sema->value == 0)
   {
      list_insert_ordered(&sema->waiters, &thread_current()->elem, (list_less_func *)&larger, NULL);
      thread_block();
   }
   sema->value--;
   intr_set_level(old_level);
}

/* 세마포어의 다운 또는 "P" 연산이지만 세마포어가 아직 0이 아닌 경우에만
   가능합니다. 세마포어가 0이면 참을 반환하고 감소하면 참을, 그렇지 않으면
   거짓을 반환합니다.

   이 함수는 인터럽트 핸들러에서 호출할 수 있습니다. */
bool sema_try_down(struct semaphore *sema)
{
   enum intr_level old_level;
   bool success;

   ASSERT(sema != NULL);

   old_level = intr_disable();
   if (sema->value > 0)
   {
      sema->value--;
      success = true;
   }
   else
      success = false;
   intr_set_level(old_level);

   return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
/* 세마포어에서 업 또는 "V" 연산. 세마 값을 증가시키고 세마를
   기다리는 스레드 중 하나(있는 경우)를 깨웁니다.

   이 함수는 인터럽트 핸들러에서 호출할 수 있습니다. */
void sema_up(struct semaphore *sema)
{
   ASSERT(sema != NULL);

   enum intr_level old_level = intr_disable();

   if (!list_empty(&sema->waiters))
   {
      list_sort(&sema->waiters, (list_less_func *)&larger, NULL);
      struct thread *t = list_entry(list_pop_front(&sema->waiters), struct thread, elem);
      thread_unblock(t);
   }

   ++(sema->value);
   intr_set_level(old_level);

   // ready_list가 비어 있어야 가능합니다
   thread_try_yield();
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
/* 한 쌍의 스레드 사이에서 제어를 "핑퐁"하는 세마포어에 대한 자체 테스트.
   printf() 호출을 삽입하여 무슨 일이 일어나고 있는지 확인합니다. */
void sema_self_test(void)
{
   struct semaphore sema[2];
   int i;

   // printf("Testing semaphores...");
   printf("세마포어 테스트......");
   sema_init(&sema[0], 0);
   sema_init(&sema[1], 0);
   thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
   for (i = 0; i < 10; i++)
   {
      sema_up(&sema[0]);
      sema_down(&sema[1]);
   }
   // printf("done.\n");
   printf("완료.\n");
}

/* Thread function used by sema_self_test(). */
/* sema_self_test()에서 사용하는 스레드 함수입니다. */
static void sema_test_helper(void *sema_)
{
   struct semaphore *sema = sema_;
   int i;

   for (i = 0; i < 10; i++)
   {
      sema_down(&sema[0]);
      sema_up(&sema[1]);
   }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
/* LOCK을 초기화합니다. 잠금은 주어진 시간에 최대 하나의 스레드만 보유할 수
   있습니다. 즉, 현재 잠금을 보유하고 있는 스레드가 해당 잠금을 획득하려고
   시도하는 것은 오류입니다.

   잠금은 초기값이 1인 세마포어의 특수화입니다. 잠금과 이러한 세마포어의
   차이점은 두 가지입니다. 첫째, 세마포어는 1보다 큰 값을 가질 수 있지만,
   잠금은 한 번에 하나의 스레드만 소유할 수 있습니다. 둘째, 세마포어는 소유자가
   없으므로 한 스레드가 세마포어를 '다운'하고 다른 스레드가 '업'할 수 있지만,
   잠금을 사용하면 동일한 스레드가 잠금을 획득하고 해제해야 합니다. 이러한
   제한이 부담스럽다면 잠금 대신 세마포어를 사용해야 한다는 좋은 신호입니다. */
void lock_init(struct lock *lock)
{
   ASSERT(lock != NULL);

   lock->holder = NULL;
   sema_init(&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* 잠금을 획득하고 필요한 경우 잠금을 사용할 수 있을 때까지 대기합니다.
   현재 스레드가 이미 잠금을 잡고 있지 않아야 합니다.

   이 함수는 잠자기 상태일 수 있으므로 인터럽트 핸들러 내에서 호출해서는
   안 됩니다. 이 함수는 인터럽트가 비활성화된 상태에서 호출할 수 있지만,
   절전 모드가 필요한 경우 인터럽트가 다시 켜집니다. */
void lock_acquire(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(!lock_held_by_current_thread(lock));

   struct thread *thread_now = thread_current();

   if (lock->holder != NULL)
   {
      thread_now->wait_on_lock = lock;
      if (thread_get_priority() > lock->holder->priority)
      {
         list_insert_ordered(&thread_now->wait_on_lock->holder->donations, &thread_now->d_elem, (list_less_func *)&larger, NULL);

         while (thread_now->wait_on_lock != NULL)
         {
            if (thread_now->priority > thread_now->wait_on_lock->holder->priority)
            {
               thread_now->wait_on_lock->holder->priority = thread_now->priority;
               thread_now = thread_now->wait_on_lock->holder;
            }
            else
               break;
         }
      }
   }
   sema_down(&lock->semaphore);
   thread_current()->wait_on_lock = NULL;
   lock->holder = thread_current();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
/* LOCK을 획득하려고 시도하고 성공하면 참을, 실패하면 거짓을 반환합니다.
   현재 스레드가 이미 잠금을 보유하고 있지 않아야 합니다.

   이 함수는 잠자기 상태가 아니므로 인터럽트 핸들러 내에서 호출할 수 있습니다. */
bool lock_try_acquire(struct lock *lock)
{
   bool success;

   ASSERT(lock != NULL);
   ASSERT(!lock_held_by_current_thread(lock));

   success = sema_try_down(&lock->semaphore);
   if (success)
      lock->holder = thread_current();
   return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
/* 현재 스레드가 소유하고 있어야 하는 LOCK을 해제합니다. 이것이
   lock_release 함수입니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러 내에서
   잠금을 해제하려고 시도하는 것은 의미가 없습니다. */
void lock_release(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(lock_held_by_current_thread(lock));
   struct thread *cur = lock->holder;

   if (!list_empty(&cur->donations))
   {
      struct list_elem *e = list_begin(&cur->donations);
      while (e != list_end(&cur->donations))
      {
         struct thread *t = list_entry(e, struct thread, d_elem);
         if (t->wait_on_lock == lock)
         {
            list_remove(e);
            t->wait_on_lock = NULL;
         }
         e = e->next;
      }
   }

   if (!list_empty(&cur->donations))
   {
      struct thread *t = list_entry(list_front(&cur->donations), struct thread, d_elem);
      cur->priority = t->priority;
   }
   else
   {
      cur->priority = cur->origin_priority;
   }

   lock->holder = NULL;
   sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
/* 현재 스레드가 LOCK을 보유하고 있으면 참을 반환하고, 그렇지 않으면 거짓을
   반환합니다. (다른 스레드가 잠금을 보유하고 있는지 테스트하는 것은 느릴 수
   있습니다). */
bool lock_held_by_current_thread(const struct lock *lock)
{
   ASSERT(lock != NULL);

   return lock->holder == thread_current();
}

/* One semaphore in a list. */
/* 리스트에 하나의 세마포어가 있습니다.*/
struct semaphore_elem
{
   struct list_elem elem;      /* List element. */
   struct semaphore semaphore; /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
/* 조건 변수 COND를 초기화합니다. 조건 변수를 사용하면 한 코드가 조건에 대한
   신호를 보내고 협력 코드가 신호를 수신하여 그에 따라 동작할 수 있습니다. */
void cond_init(struct condition *cond)
{
   ASSERT(cond != NULL);

   list_init(&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* LOCK을 원자적으로 해제하고 다른 코드가 COND 신호를 보낼 때까지
   기다립니다.  COND가 신호를 받은 후 LOCK을 다시 획득한 후 반환합니다.
   이 함수를 호출하기 전에 LOCK을 유지해야 합니다.

   이 함수가 구현하는 모니터는 "Hoare" 스타일이 아닌 "Mesa" 스타일로,
   신호 송수신이 원자 연산이 아닙니다. 따라서 일반적으로 호출자는 대기가
   완료된 후 조건을 다시 확인하고 필요한 경우 다시 대기해야 합니다.

   주어진 조건 변수는 하나의 잠금에만 연결되지만, 하나의 잠금은 여러 개의
   조건 변수와 연결될 수 있습니다. 즉, 잠금에서 조건 변수로의 일대다
   매핑이 존재합니다.

   이 함수는 잠자기 상태일 수 있으므로 인터럽트 핸들러 내에서 호출해서는
   안 됩니다. 이 함수는 인터럽트를 비활성화한 상태에서 호출할 수 있지만,
   절전이 필요한 경우 인터럽트가 다시 켜집니다. */
void cond_wait(struct condition *cond, struct lock *lock)
{
   struct semaphore_elem waiter;

   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock));

   sema_init(&waiter.semaphore, 0);
   list_insert_ordered(&cond->waiters, &waiter.elem, (list_less_func *)cond_priority, NULL);
   lock_release(lock);
   sema_down(&waiter.semaphore);
   lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/* COND에서 대기 중인 스레드가 있는 경우(LOCK으로 보호됨), 이 함수는
   그 중 하나에 대기 중이던 스레드를 깨우도록 신호를 보냅니다.
   이 함수를 호출하기 전에 LOCK을 유지해야 합니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러
   내에서 조건 변수에 신호를 보내려고 시도하는 것은 의미가 없습니다. */

void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock));

   if (!list_empty(&cond->waiters))
   {
      list_sort(&cond->waiters, (list_less_func *)cond_priority, NULL);
      sema_up(&list_entry(list_pop_front(&cond->waiters), struct semaphore_elem, elem)->semaphore);
   }
}

/* COND에서 대기 중인 모든 스레드(있는 경우)를 깨웁니다(LOCK으로 보호됨).
   이 함수를 호출하기 전에 LOCK을 유지해야 합니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러 내에서
   조건 변수를 시그널링하려고 시도하는 것은 의미가 없습니다. */
void cond_broadcast(struct condition *cond, struct lock *lock)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);

   while (!list_empty(&cond->waiters))
      cond_signal(cond, lock);
}

bool cond_priority(const struct list_elem *a, const struct list_elem *b, void *aux)
{
   struct semaphore_elem *sema_a = list_entry(a, struct semaphore_elem, elem);
   struct semaphore_elem *sema_b = list_entry(b, struct semaphore_elem, elem);
   struct thread *t_a = list_entry(list_begin(&sema_a->semaphore.waiters), struct thread, elem);
   struct thread *t_b = list_entry(list_begin(&sema_b->semaphore.waiters), struct thread, elem);
   return t_a->priority > t_b->priority;
}