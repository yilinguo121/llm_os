#include "user.h"

extern char __stack_top[];

/* exit 函數，當程序結束時進入無限循環 */
__attribute__((noreturn)) void exit(void) {
	syscall(SYS_EXIT, 0, 0, 0);
    for (;;);
}

int syscall(int sysno, int arg0, int arg1, int arg2) {
    register int a0 __asm__("a0") = arg0;
    register int a1 __asm__("a1") = arg1;
    register int a2 __asm__("a2") = arg2;
    register int a3 __asm__("a3") = sysno;

    __asm__ __volatile__("ecall"
                         : "=r"(a0)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}

void putchar(char ch) {
    syscall(SYS_PUTCHAR, ch, 0, 0);
}

int getchar(void) {
    return syscall(SYS_GETCHAR, 0, 0, 0);
}

int getchar_nonblock(void) {
    return syscall(SYS_GETCHAR_NONBLOCK, 0, 0, 0);
}

/* 程序入口函數 start，放到 .text.start 段 */
__attribute__((section(".text.start")))
__attribute__((naked))
void start(void) {
    __asm__ volatile (
        "mv sp, %[stack_top] \n"  /* 將棧指針設置為 __stack_top */
        "call main           \n"  /* 調用 main 函數 */
        "call exit           \n"  /* 調用 exit 函數結束程序 */
        :: [stack_top] "r" (__stack_top)
    );
}

