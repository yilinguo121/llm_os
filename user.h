#pragma once
#include "common.h"

int getchar(void);
int getchar_nonblock(void);
int syscall(int sysno, int arg0, int arg1, int arg2);

__attribute__((noreturn)) void exit(void);
void putchar(char ch);

