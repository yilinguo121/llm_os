#pragma once

#define true  1
#define false 0
#define NULL  ((void *) 0)
#define align_up(value, align)   __builtin_align_up(value, align)
#define is_aligned(value, align) __builtin_is_aligned(value, align)
#define offsetof(type, member)   __builtin_offsetof(type, member)
#define va_list  __builtin_va_list
#define va_start __builtin_va_start
#define va_end   __builtin_va_end
#define va_arg   __builtin_va_arg
#define PAGE_SIZE 4096
#define SYS_PUTCHAR 1
#define SYS_GETCHAR 2
#define SYS_EXIT    3
#define SYS_GETCHAR_NONBLOCK 100

// LLM 相關常數
#define LLM_REQUEST_FILE  0
#define LLM_RESPONSE_FILE 1
#define LLM_STATUS_FILE   2
#define LLM_MAX_MSG_SIZE  512

// LLM 系統呼叫號碼
#define SYS_LLM_SEND_REQUEST  200
#define SYS_LLM_GET_RESPONSE  201
#define SYS_LLM_SIMULATE      202

typedef int bool;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef uint32_t size_t;
typedef uint32_t paddr_t;
typedef uint32_t vaddr_t;

void *memset(void *buf, char c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
char *strstr(const char *haystack, const char *needle);
void printf(const char *fmt, ...);
