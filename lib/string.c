#include <string.h>
#include <debug.h>

/* Copies SIZE bytes from SRC to DST, which must not overlap.
   Returns DST. */
/* 겹치지 않아야 하는 SIZE 바이트를 SRC에서 DST로 복사합니다.
   DST를 반환합니다. */
void *memcpy(void *dst_, const void *src_, size_t size)
{
	unsigned char *dst = dst_;
	const unsigned char *src = src_;

	ASSERT(dst != NULL || size == 0);
	ASSERT(src != NULL || size == 0);

	while (size-- > 0)
		*dst++ = *src++;

	return dst_;
}

/* Copies SIZE bytes from SRC to DST, which are allowed to
   overlap.  Returns DST. */
void *
memmove(void *dst_, const void *src_, size_t size)
{
	unsigned char *dst = dst_;
	const unsigned char *src = src_;

	ASSERT(dst != NULL || size == 0);
	ASSERT(src != NULL || size == 0);

	if (dst < src)
	{
		while (size-- > 0)
			*dst++ = *src++;
	}
	else
	{
		dst += size;
		src += size;
		while (size-- > 0)
			*--dst = *--src;
	}

	return dst;
}

/* Find the first differing byte in the two blocks of SIZE bytes
   at A and B.  Returns a positive value if the byte in A is
   greater, a negative value if the byte in B is greater, or zero
   if blocks A and B are equal. */
int memcmp(const void *a_, const void *b_, size_t size)
{
	const unsigned char *a = a_;
	const unsigned char *b = b_;

	ASSERT(a != NULL || size == 0);
	ASSERT(b != NULL || size == 0);

	for (; size-- > 0; a++, b++)
		if (*a != *b)
			return *a > *b ? +1 : -1;
	return 0;
}

/* Finds the first differing characters in strings A and B.
   Returns a positive value if the character in A (as an unsigned
   char) is greater, a negative value if the character in B (as
   an unsigned char) is greater, or zero if strings A and B are
   equal. */
/* 문자열 A와 B에서 처음 다른 문자를 찾습니다.
   (부호 없는 문자로서) A의 문자가 더 크면 양수 값을,
   (부호 없는 문자로서) B의 문자가 더 크면 음수 값을,
   문자열 A와 B가 같으면 0을 반환합니다. */
int strcmp(const char *a_, const char *b_)
{
	const unsigned char *a = (const unsigned char *)a_;
	const unsigned char *b = (const unsigned char *)b_;

	ASSERT(a != NULL);
	ASSERT(b != NULL);

	while (*a != '\0' && *a == *b)
	{
		a++;
		b++;
	}

	return *a < *b ? -1 : *a > *b;
}

/* Returns a pointer to the first occurrence of CH in the first
   SIZE bytes starting at BLOCK.  Returns a null pointer if CH
   does not occur in BLOCK. */
void *
memchr(const void *block_, int ch_, size_t size)
{
	const unsigned char *block = block_;
	unsigned char ch = ch_;

	ASSERT(block != NULL || size == 0);

	for (; size-- > 0; block++)
		if (*block == ch)
			return (void *)block;

	return NULL;
}

/* Finds and returns the first occurrence of C in STRING, or a
   null pointer if C does not appear in STRING.  If C == '\0'
   then returns a pointer to the null terminator at the end of
   STRING. */
/* 문자열 STRING 안에서 문자 C의 첫 번째 등장을 찾아서 반환합니다.
   만약 C가 STRING 안에 없다면 NULL 포인터를 반환합니다.
   C가 '\0'인 경우는 STRING의 끝에 있는 NULL 종결자에 대한
   포인터를 반환합니다. */
char *strchr(const char *string, int c_)
{
	char c = c_;

	// 입력된 문자열이 유효한지 확인합니다.
	ASSERT(string);

	// 무한 루프를 시작합니다.
	for (;;)
	{
		if (*string == c)
			// 현재 위치의 문자가 찾고자 하는 문자와 일치하면
			// 현재 위치의 포인터를 반환합니다.
			return (char *)string;
		else if (*string == '\0')
			// 문자열의 끝에 도달했으나 찾고자 하는 문자를
			// 찾지 못했으면 NULL을 반환합니다.
			return NULL;
		else
			// 다음 문자로 이동합니다.
			string++;
	}
}

/* Returns the length of the initial substring of STRING that
   consists of characters that are not in STOP. */
size_t
strcspn(const char *string, const char *stop)
{
	size_t length;

	for (length = 0; string[length] != '\0'; length++)
		if (strchr(stop, string[length]) != NULL)
			break;
	return length;
}

/* Returns a pointer to the first character in STRING that is
   also in STOP.  If no character in STRING is in STOP, returns a
   null pointer. */
char *
strpbrk(const char *string, const char *stop)
{
	for (; *string != '\0'; string++)
		if (strchr(stop, *string) != NULL)
			return (char *)string;
	return NULL;
}

/* Returns a pointer to the last occurrence of C in STRING.
   Returns a null pointer if C does not occur in STRING. */
char *
strrchr(const char *string, int c_)
{
	char c = c_;
	const char *p = NULL;

	for (; *string != '\0'; string++)
		if (*string == c)
			p = string;
	return (char *)p;
}

/* Returns the length of the initial substring of STRING that
   consists of characters in SKIP. */
size_t
strspn(const char *string, const char *skip)
{
	size_t length;

	for (length = 0; string[length] != '\0'; length++)
		if (strchr(skip, string[length]) == NULL)
			break;
	return length;
}

/* Returns a pointer to the first occurrence of NEEDLE within
   HAYSTACK.  Returns a null pointer if NEEDLE does not exist
   within HAYSTACK. */
char *
strstr(const char *haystack, const char *needle)
{
	size_t haystack_len = strlen(haystack);
	size_t needle_len = strlen(needle);

	if (haystack_len >= needle_len)
	{
		size_t i;

		for (i = 0; i <= haystack_len - needle_len; i++)
			if (!memcmp(haystack + i, needle, needle_len))
				return (char *)haystack + i;
	}

	return NULL;
}

/* Breaks a string into tokens separated by DELIMITERS.  The
   first time this function is called, S should be the string to
   tokenize, and in subsequent calls it must be a null pointer.
   SAVE_PTR is the address of a `char *' variable used to keep
   track of the tokenizer's position.  The return value each time
   is the next token in the string, or a null pointer if no
   tokens remain.

   This function treats multiple adjacent delimiters as a single
   delimiter.  The returned tokens will never be length 0.
   DELIMITERS may change from one call to the next within a
   single string.

   strtok_r() modifies the string S, changing delimiters to null
   bytes.  Thus, S must be a modifiable string.  String literals,
   in particular, are *not* modifiable in C, even though for
   backward compatibility they are not `const'.

   Example usage:

   char s[] = "  String to  tokenize. ";
   char *token, *save_ptr;

   for (token = strtok_r (s, " ", &save_ptr); token != NULL;
   token = strtok_r (NULL, " ", &save_ptr))
   printf ("'%s'\n", token);

outputs:

'String'
'to'
'tokenize.'
*/
/* 이 함수는 `DELIMITERS`로 구분된 토큰으로 문자열을 나눕니다.
   이 함수가 처음 호출될 때, `S`는 토큰화할 문자열이어야 하며,
   이후 호출에서는 `null` 포인터가 되어야 합니다. `SAVE_PTR`은
   토큰화기의 위치를 추적하는 데 사용되는 `char *` 변수의 주소
   입니다. 매 번의 반환 값은 문자열에서 다음 토큰이거나, 더 이상
   토큰이 남아 있지 않으면 `null` 포인터입니다.

   이 함수는 연속된 구분자를 하나의 구분자로 취급합니다. 반환
   된 토큰은 길이가 0이 되지 않습니다. `DELIMITERS`는 하나의
   문자열 내에서 다음 호출로 변경될 수 있습니다.

   `strtok_r()` 함수는 `S` 문자열을 수정하여 구분자를 `null`
   바이트로 변경합니다. 따라서, `S`는 수정 가능한 문자열이어야
   합니다. 특히, C에서 문자열 리터럴은 수정할 수 없음에도 불구
   하고, 역호환성을 위해 `const`가 아닙니다.

   사용 예:

	```c
	char s[] = "  문자열을  토큰화합니다. ";
	char *token, *save_ptr;

	for (token = strtok_r (s, " ", &save_ptr); token != NULL;
	token = strtok_r (NULL, " ", &save_ptr))
	printf ("'%s'\n", token);
	```

	출력:

	```
	'문자열을'
	'토큰화합니다.'
	```*/
char *strtok_r(char *s, const char *delimiters, char **save_ptr)
{
	char *token;

	// 구분자와 저장 포인터가 NULL이 아닌지 확인합니다.
	ASSERT(delimiters != NULL);
	ASSERT(save_ptr != NULL);

	/* s가 NULL이 아니면 s에서 시작합니다.
	   s가 NULL이면 저장된 위치에서 시작합니다. */
	if (s == NULL)
		s = *save_ptr;
	// s가 NULL인지 다시 한 번 확인합니다.
	ASSERT(s != NULL);

	/* 현재 위치에서 구분자를 건너뜁니다. */
	while (strchr(delimiters, *s) != NULL)
	{
		/* strchr()는 우리가 NULL 바이트를 찾는 경우 항상 NULL이 아닌 값을 반환합니다.
		   왜냐하면 모든 문자열은 끝에 NULL 바이트를 포함하기 때문입니다. */
		if (*s == '\0')
		{
			*save_ptr = s;
			return NULL; // 문자열의 끝에 도달했으므로 NULL을 반환합니다.
		}

		s++; // 다음 문자로 이동합니다.
	}

	/* 구분자가 아닌 문자를 건너뛰면서 토큰의 시작 위치를 찾습니다. */
	token = s;
	while (strchr(delimiters, *s) == NULL)
		s++;
	if (*s != '\0')
	{
		*s = '\0';		   // 현재 구분자를 NULL로 바꿔서 토큰을 분리합니다.
		*save_ptr = s + 1; // 다음 토큰의 시작 위치를 저장합니다.
	}
	else
		*save_ptr = s; // 문자열의 끝에 도달했으므로, 다음 시작 위치도 문자열의 끝입니다.
	return token;	   // 찾은 토큰을 반환합니다.
}

/* Sets the SIZE bytes in DST to VALUE. */
/* DST의 SIZE 바이트를 VALUE로 설정합니다. */
void *memset(void *dst_, int value, size_t size)
{
	unsigned char *dst = dst_;

	ASSERT(dst != NULL || size == 0);

	while (size-- > 0)
		*dst++ = value;

	return dst_;
}

/* Returns the length of STRING. */
size_t
strlen(const char *string)
{
	const char *p;

	ASSERT(string);

	for (p = string; *p != '\0'; p++)
		continue;
	return p - string;
}

/* If STRING is less than MAXLEN characters in length, returns
   its actual length.  Otherwise, returns MAXLEN. */
size_t
strnlen(const char *string, size_t maxlen)
{
	size_t length;

	for (length = 0; string[length] != '\0' && length < maxlen; length++)
		continue;
	return length;
}

/* Copies string SRC to DST.  If SRC is longer than SIZE - 1
   characters, only SIZE - 1 characters are copied.  A null
   terminator is always written to DST, unless SIZE is 0.
   Returns the length of SRC, not including the null terminator.

   strlcpy() is not in the standard C library, but it is an
   increasingly popular extension.  See
http://www.courtesan.com/todd/papers/strlcpy.html for
information on strlcpy(). */
size_t
strlcpy(char *dst, const char *src, size_t size)
{
	size_t src_len;

	ASSERT(dst != NULL);
	ASSERT(src != NULL);

	src_len = strlen(src);
	if (size > 0)
	{
		size_t dst_len = size - 1;
		if (src_len < dst_len)
			dst_len = src_len;
		memcpy(dst, src, dst_len);
		dst[dst_len] = '\0';
	}
	return src_len;
}

/* Concatenates string SRC to DST.  The concatenated string is
   limited to SIZE - 1 characters.  A null terminator is always
   written to DST, unless SIZE is 0.  Returns the length that the
   concatenated string would have assuming that there was
   sufficient space, not including a null terminator.

   strlcat() is not in the standard C library, but it is an
   increasingly popular extension.  See
http://www.courtesan.com/todd/papers/strlcpy.html for
information on strlcpy(). */
size_t
strlcat(char *dst, const char *src, size_t size)
{
	size_t src_len, dst_len;

	ASSERT(dst != NULL);
	ASSERT(src != NULL);

	src_len = strlen(src);
	dst_len = strlen(dst);
	if (size > 0 && dst_len < size)
	{
		size_t copy_cnt = size - dst_len - 1;
		if (src_len < copy_cnt)
			copy_cnt = src_len;
		memcpy(dst + dst_len, src, copy_cnt);
		dst[dst_len + copy_cnt] = '\0';
	}
	return src_len + dst_len;
}
