#include "user.h"

void delay() {
    for (volatile int i = 0; i < 100000; ++i);
}

// 顯示游標
void show_cursor(void) {
    putchar('_');
}

// 隱藏游標
void hide_cursor(void) {
    putchar('\b');  // 退格
    putchar(' ');   // 空格覆蓋
    putchar('\b');  // 再退格
}

void main(void) {
    while (1) {
prompt:
        printf("> ");
        char cmdline[128];
        int i = 0;
        int cursor_visible = 0;
        int blink = 0;
        show_cursor();
        cursor_visible = 1;
        while (1) {
            int ch = getchar_nonblock();
            if (ch >= 0) {
                if (cursor_visible) {
                    hide_cursor();
                    cursor_visible = 0;
                }
                if (ch == '\r') {
                    printf("\n");
                    cmdline[i] = '\0';
                    break;
                } else if (ch == '\b' || ch == 127) { // 退格鍵
                    if (i > 0) {
                        i--;
                        putchar('\b');
                        putchar(' ');
                        putchar('\b');
                    }
                } else if (i == sizeof(cmdline) - 1) {
                    printf("command line too long\n");
                    goto prompt;
                } else {
                    putchar(ch);
                    cmdline[i++] = ch;
                }
                show_cursor();
                cursor_visible = 1;
            } else {
                delay();
                blink++;
                if (blink > 100) { // 1秒左右閃一次
                    if (cursor_visible) {
                        hide_cursor();
                        cursor_visible = 0;
                    } else {
                        show_cursor();
                        cursor_visible = 1;
                    }
                    blink = 0;
                }
            }
        }
        if (strcmp(cmdline, "hello") == 0)
            printf("Hello world from shell!\n");
        else if (strcmp(cmdline, "exit") == 0)
            exit();
        else if (strcmp(cmdline, "help") == 0) {
            printf("\n=== RISC-V OS Shell 命令說明 ===\n");
            printf("hello  - 顯示歡迎訊息\n");
            printf("exit   - 結束程式\n");
            printf("help   - 顯示此說明\n");
        }
        else
            printf("unknown command: %s\n", cmdline);
    }
}
