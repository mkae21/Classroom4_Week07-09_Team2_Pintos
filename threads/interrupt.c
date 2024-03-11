#include "threads/interrupt.h"
#include <debug.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "threads/flags.h"
#include "threads/intr-stubs.h"
#include "threads/io.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/gdt.h"
#endif

/* Number of x86_64 interrupts. */
/* x86_64 인터럽트 수입니다. */
#define INTR_CNT 256

/* Creates an gate that invokes FUNCTION.

   The gate has descriptor privilege level DPL, meaning that it
   can be invoked intentionally when the processor is in the DPL
   or lower-numbered ring.  In practice, DPL==3 allows user mode
   to call into the gate and DPL==0 prevents such calls.  Faults
   and exceptions that occur in user mode still cause gates with
   DPL==0 to be invoked.

   TYPE must be either 14 (for an interrupt gate) or 15 (for a
   trap gate).  The difference is that entering an interrupt gate
   disables interrupts, but entering a trap gate does not.  See
   [IA32-v3a] section 5.12.1.2 "Flag Usage By Exception- or
   Interrupt-Handler Procedure" for discussion. */
/* FUNCTION을 호출하는 게이트를 생성합니다.

   이 게이트는 설명자 권한 수준 DPL을 가지므로 프로세서가 DPL 또는
   더 낮은 번호의 링에 있을 때 의도적으로 호출할 수 있습니다.
   실제로 DPL==3은 사용자 모드가 게이트로 호출할 수 있도록 허용하고
   DPL==0은 이러한 호출을 방지합니다. 사용자 모드에서 발생하는 오류
   및 예외는 여전히 DPL==0인 게이트로 호출됩니다.

   TYPE은 14(인터럽트 게이트의 경우) 또는 15(트랩 게이트의 경우)여야
   합니다. 차이점은 인터럽트 게이트를 입력하면 인터럽트가 비활성화
   되지만 트랩 게이트를 입력하면 비활성화되지 않는다는 것입니다.
   자세한 내용은 [IA32-v3a] 섹션 5.12.1.2 "예외 또는 인터럽트 핸들러
   절차에 의한 플래그 사용"을 참조하십시오. */
struct gate
{
	unsigned off_15_0 : 16;	 // low 16 bits of offset in segment
							 // 세그먼트의 낮은 16비트 오프셋
	unsigned ss : 16;		 // segment selector
							 // 세그먼트 선택기
	unsigned ist : 3;		 // # args, 0 for interrupt/trap gates
							 // # args, 인터럽트/트랩 게이트의 경우 0
	unsigned rsv1 : 5;		 // reserved(should be zero I guess)
							 // 예약됨(0이어야 함)
	unsigned type : 4;		 // type(STS_{TG,IG32,TG32})
	unsigned s : 1;			 // must be 0 (system)
							 // 0이어야 함(시스템)
	unsigned dpl : 2;		 // descriptor(meaning new) privilege level
							 // 설명자(새 의미) 권한 수준
	unsigned p : 1;			 // Present
							 // 현재
	unsigned off_31_16 : 16; // high bits of offset in segment
							 // 세그먼트의 높은 오프셋 비트
	uint32_t off_32_63;
	uint32_t rsv2;
};

/* The Interrupt Descriptor Table (IDT).  The format is fixed by
   the CPU.  See [IA32-v3a] sections 5.10 "Interrupt Descriptor
   Table (IDT)", 5.11 "IDT Descriptors", 5.12.1.2 "Flag Usage By
   Exception- or Interrupt-Handler Procedure". */
/* 인터럽트 설명자 테이블(IDT). 형식은 CPU에 의해 고정됩니다.
   [IA32-v3a] 섹션 5.10 "인터럽트 기술자 테이블(IDT)",
   5.11 "IDT 기술자", 5.12.1.2 "예외 또는 인터럽트 핸들러
   프로시저별 플래그 사용"을 참고하세요. */
static struct gate idt[INTR_CNT];

static struct desc_ptr idt_desc = {
	.size = sizeof(idt) - 1,
	.address = (uint64_t)idt};

#define make_gate(g, function, d, t)                                \
	{                                                               \
		ASSERT((function) != NULL);                                 \
		ASSERT((d) >= 0 && (d) <= 3);                               \
		ASSERT((t) >= 0 && (t) <= 15);                              \
		*(g) = (struct gate){                                       \
			.off_15_0 = (uint64_t)(function) & 0xffff,              \
			.ss = SEL_KCSEG,                                        \
			.ist = 0,                                               \
			.rsv1 = 0,                                              \
			.type = (t),                                            \
			.s = 0,                                                 \
			.dpl = (d),                                             \
			.p = 1,                                                 \
			.off_31_16 = ((uint64_t)(function) >> 16) & 0xffff,     \
			.off_32_63 = ((uint64_t)(function) >> 32) & 0xffffffff, \
			.rsv2 = 0,                                              \
		};                                                          \
	}

/* Creates an interrupt gate that invokes FUNCTION with the given DPL. */
/* 주어진 DPL로 FUNCTION을 호출하는 인터럽트 게이트를 생성합니다. */
#define make_intr_gate(g, function, dpl) make_gate((g), (function), (dpl), 14)

/* Creates a trap gate that invokes FUNCTION with the given DPL. */
/* 주어진 DPL로 FUNCTION을 호출하는 트랩 게이트를 생성합니다. */
#define make_trap_gate(g, function, dpl) make_gate((g), (function), (dpl), 15)

/* Interrupt handler functions for each interrupt. */
/* 각 인터럽트에 대한 인터럽트 핸들러 기능. */
static intr_handler_func *intr_handlers[INTR_CNT];

/* Names for each interrupt, for debugging purposes. */
/* 디버깅을 위한 각 인터럽트의 이름입니다. */
static const char *intr_names[INTR_CNT];

/* External interrupts are those generated by devices outside the
   CPU, such as the timer.  External interrupts run with
   interrupts turned off, so they never nest, nor are they ever
   pre-empted.  Handlers for external interrupts also may not
   sleep, although they may invoke intr_yield_on_return() to
   request that a new process be scheduled just before the
   interrupt returns. */
/* 외부 인터럽트는 타이머와 같이 CPU 외부의 장치에서 생성되는
   인터럽트입니다. 외부 인터럽트는 인터럽트가 꺼진 상태에서
   실행되므로 중첩되거나 선점되지 않습니다. 외부 인터럽트 핸들러도
   잠자기 상태가 아닐 수 있지만, 인터럽트가 반환되기 직전에 새로운
   프로세스를 예약하도록 요청하기 위해 intr_yield_on_return()을
   호출할 수 있습니다. 인터럽트가 반환되기 직전에 새 프로세스를
   예약하도록 요청할 수 있습니다. */
static bool in_external_intr; /* Are we processing an external interrupt? */
							  /* 외부 인터럽트를 처리 중입니까? */
static bool yield_on_return;  /* Should we yield on interrupt return? */
							  /* 인터럽트 리턴 시 양보해야 하나요? */

/* Programmable Interrupt Controller helpers. */
/* 프로그래머블 인터럽트 컨트롤러 도우미. */
static void pic_init(void);
static void pic_end_of_interrupt(int irq);

/* Interrupt handlers. */
/* 인터럽트 핸들러. */
void intr_handler(struct intr_frame *args);

/* Returns the current interrupt status. */
/* 현재 인터럽트 상태를 반환합니다. */
enum intr_level intr_get_level(void)
{
	uint64_t flags;

	/* Push the flags register on the processor stack, then pop the
	   value off the stack into `flags'.  See [IA32-v2b] "PUSHF"
	   and "POP" and [IA32-v3a] 5.8.1 "Masking Maskable Hardware
	   Interrupts". */
	/* 프로세서 스택의 플래그 레지스터를 누른 다음 스택에서 값을
	   '플래그'로 팝합니다. IA32-v2b] "PUSHF" 및 "POP", [IA32-v3a]
	   5.8.1 "마스킹 가능한 하드웨어 인터럽트 마스킹"을 참조하세요. */
	asm volatile("pushfq; popq %0" : "=g"(flags));

	return flags & FLAG_IF ? INTR_ON : INTR_OFF; // 비트 연산자로 flag 확인후 값을 반환
}

/* Enables or disables interrupts as specified by LEVEL and
   returns the previous interrupt status. */
/* LEVEL에 지정된 대로 인터럽트를 활성화 또는 비활성화하고
   이전 인터럽트 상태를 반환합니다. */
enum intr_level intr_set_level(enum intr_level level)
{
	return level == INTR_ON ? intr_enable() : intr_disable();
}

/* Enables interrupts and returns the previous interrupt status. */
/* 인터럽트를 활성화하고 이전 인터럽트 상태를 반환합니다. */
enum intr_level intr_enable(void)
{
  // 예전 인터럽트 상태 반환
	enum intr_level old_level = intr_get_level();
	ASSERT(!intr_context());

	/* Enable interrupts by setting the interrupt flag.

	   See [IA32-v2b] "STI" and [IA32-v3a] 5.8.1 "Masking Maskable
	   Hardware Interrupts". */
	/* 인터럽트 플래그를 설정하여 인터럽트를 활성화합니다.

	   [IA32-v2b] "STI" 및 [IA32-v3a] 5.8.1 "마스킹 가능한
	   하드웨어 인터럽트 마스킹"을 참조하세요. */
	asm volatile("sti");

	return old_level;
}

/* Disables interrupts and returns the previous interrupt status. */
/* 인터럽트를 비활성화하고 이전 인터럽트 상태를 반환합니다. */
enum intr_level intr_disable(void)
{
  // current interrupt status
	enum intr_level old_level = intr_get_level();

	/* Disable interrupts by clearing the interrupt flag.
	   See [IA32-v2b] "CLI" and [IA32-v3a] 5.8.1 "Masking Maskable
	   Hardware Interrupts". */
	/* 인터럽트 플래그를 지워 인터럽트를 비활성화합니다.
	   [IA32-v2b] "CLI" 및 [IA32-v3a] 5.8.1 "마스킹 가능한 하드웨어
	   인터럽트 마스킹"을 참조하세요. */
	asm volatile("cli" : : : "memory");

	return old_level;
}

/* Initializes the interrupt system. */
/* 인터럽트 시스템을 초기화합니다. */
void intr_init(void)
{
	int i;

	/* Initialize interrupt controller. */
	/* 인터럽트 컨트롤러를 초기화합니다. */
	pic_init();

	/* Initialize IDT. */
	/* IDT를 초기화합니다. */
	for (i = 0; i < INTR_CNT; i++)
	{
		make_intr_gate(&idt[i], intr_stubs[i], 0);
		intr_names[i] = "unknown";
	}

#ifdef USERPROG
	/* Load TSS. */
	ltr(SEL_TSS);
#endif

	/* Load IDT register. */
	/* IDT 레지스터를 로드합니다. */
	lidt(&idt_desc);

	/* Initialize intr_names. */
	/* intr_names를 초기화합니다. */
	intr_names[0] = "#DE Divide Error";
	intr_names[1] = "#DB Debug Exception";
	intr_names[2] = "NMI Interrupt";
	intr_names[3] = "#BP Breakpoint Exception";
	intr_names[4] = "#OF Overflow Exception";
	intr_names[5] = "#BR BOUND Range Exceeded Exception";
	intr_names[6] = "#UD Invalid Opcode Exception";
	intr_names[7] = "#NM Device Not Available Exception";
	intr_names[8] = "#DF Double Fault Exception";
	intr_names[9] = "Coprocessor Segment Overrun";
	intr_names[10] = "#TS Invalid TSS Exception";
	intr_names[11] = "#NP Segment Not Present";
	intr_names[12] = "#SS Stack Fault Exception";
	intr_names[13] = "#GP General Protection Exception";
	intr_names[14] = "#PF Page-Fault Exception";
	intr_names[16] = "#MF x87 FPU Floating-Point Error";
	intr_names[17] = "#AC Alignment Check Exception";
	intr_names[18] = "#MC Machine-Check Exception";
	intr_names[19] = "#XF SIMD Floating-Point Exception";
}

/* Registers interrupt VEC_NO to invoke HANDLER with descriptor
   privilege level DPL.  Names the interrupt NAME for debugging
   purposes.  The interrupt handler will be invoked with
   interrupt status set to LEVEL. */
/* 설명자 권한 수준 DPL로 HANDLER를 호출하기 위해 인터럽트
   VEC_NO를 등록합니다. 디버깅을 위해 인터럽트 이름을 NAME으로
   지정합니다. 인터럽트 핸들러는 인터럽트 상태가 LEVEL로 설정된
   상태에서 호출됩니다. */
static void register_handler(uint8_t vec_no, int dpl, enum intr_level level,
							 intr_handler_func *handler, const char *name)
{
	ASSERT(intr_handlers[vec_no] == NULL);

	if (level == INTR_ON)
	{
		make_trap_gate(&idt[vec_no], intr_stubs[vec_no], dpl);
	}
	else
	{
		make_intr_gate(&idt[vec_no], intr_stubs[vec_no], dpl);
	}

	intr_handlers[vec_no] = handler;
	intr_names[vec_no] = name;
}

/* Registers external interrupt VEC_NO to invoke HANDLER, which
   is named NAME for debugging purposes.  The handler will
   execute with interrupts disabled. */
/* 외부 인터럽트 VEC_NO를 등록하여 디버깅 목적으로 NAME이라는
   이름의 HANDLER를 호출합니다. 핸들러는 인터럽트를 비활성화한
   상태로 실행됩니다. */
void intr_register_ext(uint8_t vec_no, intr_handler_func *handler,
					   const char *name)
{
	ASSERT(vec_no >= 0x20 && vec_no <= 0x2f);
	register_handler(vec_no, 0, INTR_OFF, handler, name);
}

/* Registers internal interrupt VEC_NO to invoke HANDLER, which
   is named NAME for debugging purposes.  The interrupt handler
   will be invoked with interrupt status LEVEL.

   The handler will have descriptor privilege level DPL, meaning
   that it can be invoked intentionally when the processor is in
   the DPL or lower-numbered ring.  In practice, DPL==3 allows
   user mode to invoke the interrupts and DPL==0 prevents such
   invocation.  Faults and exceptions that occur in user mode
   still cause interrupts with DPL==0 to be invoked.  See
   [IA32-v3a] sections 4.5 "Privilege Levels" and 4.8.1.1
   "Accessing Nonconforming Code Segments" for further
   discussion. */
/* 내부 인터럽트 VEC_NO를 등록하여 디버깅 목적으로 NAME이라는
   이름의 HANDLER를 호출합니다. 인터럽트 핸들러는 인터럽트 상태
   LEVEL로 호출됩니다.

   핸들러는 설명자 권한 수준 DPL을 가지므로 프로세서가 DPL 또는
   낮은 번호의 링에 있을 때 의도적으로 호출될 수 있습니다. 실제로
   DPL==3은 사용자 모드에서 인터럽트를 호출할 수 있도록 허용하고
   DPL==0은 이러한 호출을 방지합니다. 사용자 모드에서 발생하는
   결함 및 예외는 여전히 DPL==0으로 인터럽트를 호출합니다. 자세한
   내용은 [IA32-v3a] 섹션 4.5 "권한 수준" 및 4.8.1.1 "부적합 코드
   세그먼트에 액세스하기"를 참조하세요. */
void intr_register_int(uint8_t vec_no, int dpl, enum intr_level level,
					   intr_handler_func *handler, const char *name)
{
	ASSERT(vec_no < 0x20 || vec_no > 0x2f);
	register_handler(vec_no, dpl, level, handler, name);
}

/* Returns true during processing of an external interrupt
   and false at all other times. */
/* 외부 인터럽트 처리 중에는 참을 반환하고
   그 외의 모든 경우에는 거짓을 반환합니다. */
bool intr_context(void)
{
	return in_external_intr;
}

/* During processing of an external interrupt, directs the
   interrupt handler to yield to a new process just before
   returning from the interrupt.  May not be called at any other
   time. */
/* 외부 인터럽트를 처리하는 동안 인터럽트 핸들러가 인터럽트에서
   돌아오기 직전에 새 프로세스로 양보하도록 지시합니다.
   다른 시간에는 호출할 수 없습니다. */
void intr_yield_on_return(void)
{
	ASSERT(intr_context());
	yield_on_return = true;
}

/* 8259A Programmable Interrupt Controller. */
/* 8259A 프로그래머블 인터럽트 컨트롤러. */

/* Every PC has two 8259A Programmable Interrupt Controller (PIC)
   chips.  One is a "master" accessible at ports 0x20 and 0x21.
   The other is a "slave" cascaded onto the master's IRQ 2 line
   and accessible at ports 0xa0 and 0xa1.  Accesses to port 0x20
   set the A0 line to 0 and accesses to 0x21 set the A1 line to
   1.  The situation is similar for the slave PIC.

   By default, interrupts 0...15 delivered by the PICs will go to
   interrupt vectors 0...15.  Unfortunately, those vectors are
   also used for CPU traps and exceptions.  We reprogram the PICs
   so that interrupts 0...15 are delivered to interrupt vectors
   32...47 (0x20...0x2f) instead. */
/* 모든 PC에는 두 개의 8259A 프로그래머블 인터럽트 컨트롤러(PIC)
   칩이 있습니다. 하나는 포트 0x20 및 0x21에서 액세스할 수 있는
   "마스터"입니다. 다른 하나는 마스터의 IRQ 2 라인에 캐스케이드되어
   포트 0xa0 및 0xa1에서 액세스할 수 있는 "슬레이브"입니다. 포트
   0x20에 액세스하면 A0 라인이 0으로 설정되고 0x21에 액세스하면
   A1 라인이 1로 설정됩니다. 슬레이브 PIC도 상황은 비슷합니다.

   기본적으로 PIC가 제공하는 인터럽트 0...15는 인터럽트 벡터 0...15로
   이동합니다. 안타깝게도 이러한 벡터는 CPU 트랩과 예외에도 사용됩니다.
   인터럽트 0...15가 대신 인터럽트 벡터 32...47(0x20...0x2f)로
   전달되도록 PIC를 다시 프로그래밍합니다. */

/* Initializes the PICs.  Refer to [8259A] for details. */
/* PIC를 초기화합니다. 자세한 내용은 [8259A]를 참조하십시오. */
static void pic_init(void)
{
	/* Mask all interrupts on both PICs. */
	/* 양쪽 PIC의 모든 인터럽트를 마스킹합니다. */
	outb(0x21, 0xff);
	outb(0xa1, 0xff);

	/* Initialize master. */
	/* 마스터 초기화. */
	outb(0x20, 0x11); /* ICW1: single mode, edge triggered, expect ICW4. */
					  /* ICW1: 단일 모드, 에지 트리거, ICW4 예상. */
	outb(0x21, 0x20); /* ICW2: line IR0...7 -> irq 0x20...0x27. */
					  /* ICW2: 라인 IR0...7 -> irq 0x20...0x27. */
	outb(0x21, 0x04); /* ICW3: slave PIC on line IR2. */
					  /* ICW3: IR2 라인의 슬레이브 PIC. */
	outb(0x21, 0x01); /* ICW4: 8086 mode, normal EOI, non-buffered. */
					  /* ICW4: 8086 모드, 일반 EOI, 비버퍼링. */

	/* Initialize slave. */
	/* 슬레이브를 초기화합니다. */
	outb(0xa0, 0x11); /* ICW1: single mode, edge triggered, expect ICW4. */
					  /* ICW1: 단일 모드, 에지 트리거, ICW4 예상. */
	outb(0xa1, 0x28); /* ICW2: line IR0...7 -> irq 0x28...0x2f. */
					  /* ICW2: 라인 IR0...7 -> irq 0x28...0x2f. */
	outb(0xa1, 0x02); /* ICW3: slave ID is 2. */
					  /* ICW3: 슬레이브 ID는 2입니다. */
	outb(0xa1, 0x01); /* ICW4: 8086 mode, normal EOI, non-buffered. */
					  /* ICW4: 8086 모드, 일반 EOI, 비버퍼링. */

	/* Unmask all interrupts. */
	/* 모든 인터럽트 마스크를 해제합니다. */
	outb(0x21, 0x00);
	outb(0xa1, 0x00);
}

/* Sends an end-of-interrupt signal to the PIC for the given IRQ.
   If we don't acknowledge the IRQ, it will never be delivered to
   us again, so this is important.  */
/* 지정된 IRQ에 대한 인터럽트 종료 신호를 PIC로 보냅니다. IRQ를
   인식하지 못하면 다시는 전달되지 않으므로 매우 중요합니다.  */
static void
pic_end_of_interrupt(int irq)
{
	ASSERT(irq >= 0x20 && irq < 0x30);

	/* Acknowledge master PIC. */
	/* 마스터 PIC를 승인합니다. */
	outb(0x20, 0x20);

	/* Acknowledge slave PIC if this is a slave interrupt. */
	/* 슬레이브 인터럽트인 경우 슬레이브 PIC를 승인합니다. */
	if (irq >= 0x28)
		outb(0xa0, 0x20);
}
/* Interrupt handlers. */
/* 인터럽트 핸들러. */

/* Handler for all interrupts, faults, and exceptions.  This
   function is called by the assembly language interrupt stubs in
   intr-stubs.S.  FRAME describes the interrupt and the
   interrupted thread's registers. */
/* 모든 인터럽트, 오류 및 예외에 대한 핸들러. 이 함수는
   intr-stubs.S의 어셈블리 언어 인터럽트 스텁에 의해 호출됩니다.
   FRAME은 인터럽트와 인터럽트된 스레드의 레지스터를 설명합니다. */
void intr_handler(struct intr_frame *frame)
{
	bool external;
	intr_handler_func *handler;

	/* External interrupts are special.
	   We only handle one at a time (so interrupts must be off)
	   and they need to be acknowledged on the PIC (see below).
	   An external interrupt handler cannot sleep. */
	/* 외부 인터럽트는 특별합니다. 한 번에 하나만 처리하며 (따라서
	   인터럽트는 꺼져 있어야 함) PIC에서 승인해야 합니다 (아래 참조).
	   외부 인터럽트 핸들러는 절전 모드로 전환할 수 없습니다. */
	external = frame->vec_no >= 0x20 && frame->vec_no < 0x30;
	if (external)
	{
		ASSERT(intr_get_level() == INTR_OFF);
		ASSERT(!intr_context());

		in_external_intr = true;
		yield_on_return = false;
	}

	/* Invoke the interrupt's handler. */
	/* 인터럽트의 핸들러를 호출합니다. */
	handler = intr_handlers[frame->vec_no];
	if (handler != NULL)
		handler(frame);
	else if (frame->vec_no == 0x27 || frame->vec_no == 0x2f)
	{
		/* There is no handler, but this interrupt can trigger
		   spuriously due to a hardware fault or hardware race
		   condition.  Ignore it. */
		/* 핸들러는 없지만 이 인터럽트는 하드웨어 오류 또는
		   하드웨어 경합 조건으로 인해 가짜로 트리거될 수 있습니다.
		   무시하세요. */
	}
	else
	{
		/* No handler and not spurious.  Invoke the unexpected
		   interrupt handler. */
		/* 핸들러가 없고 가짜가 아닙니다.
		   예기치 않은 인터럽트 핸들러를 호출합니다. */
		intr_dump_frame(frame);
		PANIC("Unexpected interrupt");
	}

	/* Complete the processing of an external interrupt. */
	/* 외부 인터럽트 처리를 완료합니다. */
	if (external)
	{
		ASSERT(intr_get_level() == INTR_OFF);
		ASSERT(intr_context());

		in_external_intr = false;
		pic_end_of_interrupt(frame->vec_no);

		if (yield_on_return)
			thread_yield();
	}
}

/* Dumps interrupt frame F to the console, for debugging. */
/* 디버깅을 위해 인터럽트 프레임 F를 콘솔에 덤프합니다. */
void intr_dump_frame(const struct intr_frame *f)
{
	/* CR2 is the linear address of the last page fault.
	   See [IA32-v2a] "MOV--Move to/from Control Registers" and
	   [IA32-v3a] 5.14 "Interrupt 14--Page Fault Exception
	   (#PF)". */
	/* CR2는 마지막 페이지 오류의 선형 주소입니다.

	   [IA32-v2a] "MOV--제어 레지스터로/로부터 이동" 및 [IA32-v3a]
	   5.14 "인터럽트 14--페이지 오류 예외(#PF)"를 참조하세요. */
	uint64_t cr2 = rcr2();
	printf("Interrupt %#04llx (%s) at rip=%llx\n",
		   f->vec_no, intr_names[f->vec_no], f->rip);
	printf(" cr2=%016llx error=%16llx\n", cr2, f->error_code);
	printf("rax %016llx rbx %016llx rcx %016llx rdx %016llx\n",
		   f->R.rax, f->R.rbx, f->R.rcx, f->R.rdx);
	printf("rsp %016llx rbp %016llx rsi %016llx rdi %016llx\n",
		   f->rsp, f->R.rbp, f->R.rsi, f->R.rdi);
	printf("rip %016llx r8 %016llx  r9 %016llx r10 %016llx\n",
		   f->rip, f->R.r8, f->R.r9, f->R.r10);
	printf("r11 %016llx r12 %016llx r13 %016llx r14 %016llx\n",
		   f->R.r11, f->R.r12, f->R.r13, f->R.r14);
	printf("r15 %016llx rflags %08llx\n", f->R.r15, f->eflags);
	printf("es: %04x ds: %04x cs: %04x ss: %04x\n",
		   f->es, f->ds, f->cs, f->ss);
}

/* Returns the name of interrupt VEC. */
/* 인터럽트 VEC의 이름을 반환합니다. */
const char *intr_name(uint8_t vec)
{
	return intr_names[vec];
}