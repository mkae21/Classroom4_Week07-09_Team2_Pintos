#include "devices/input.h"
#include <debug.h>
#include "devices/intq.h"
#include "devices/serial.h"

/* Stores keys from the keyboard and serial port. */
/* 키보드와 직렬 포트의 키를 저장합니다. */
static struct intq buffer;

/* Initializes the input buffer. */
/* 입력 버퍼를 초기화합니다. */
void input_init(void)
{
	intq_init(&buffer);
}

/* Adds a key to the input buffer.
   Interrupts must be off and the buffer must not be full. */
/* 입력 버퍼에 키를 추가합니다.
   인터럽트가 꺼져 있어야 하고 버퍼가 가득 차 있지 않아야 합니다. */
void input_putc(uint8_t key)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(!intq_full(&buffer));

	intq_putc(&buffer, key);
	serial_notify();
}

/* Retrieves a key from the input buffer.
   If the buffer is empty, waits for a key to be pressed. */
/* 입력 버퍼에서 키를 검색합니다.
   버퍼가 비어 있으면 키를 누를 때까지 기다립니다. */
uint8_t input_getc(void)
{
	enum intr_level old_level = intr_disable();
	uint8_t key = intq_getc(&buffer);
	serial_notify();
	intr_set_level(old_level);

	return key;
}

/* Returns true if the input buffer is full,
   false otherwise.
   Interrupts must be off. */
/* 입력 버퍼가 가득 차면 참을 반환하고,
   그렇지 않으면 거짓을 반환합니다.
   인터럽트가 꺼져 있어야 합니다. */
bool input_full(void)
{
	ASSERT(intr_get_level() == INTR_OFF);
	return intq_full(&buffer);
}
