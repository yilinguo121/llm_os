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

// 簡單的 LLM 回應函數
void llm_response(const char *input) {
    printf("[LLM] ");

    // 簡單的關鍵字匹配回應
    if (strstr(input, "你好") || strstr(input, "hello")) {
        printf("你好！很高興見到你。我是基於 RISC-V OS 的 AI 助手。\n");
    } else if (strstr(input, "幫助") || strstr(input, "help")) {
        printf("我可以幫助你了解這個作業系統，或者回答一些基本問題。\n");
    } else if (strstr(input, "系統") || strstr(input, "system")) {
        printf("這是一個 RISC-V 32位元作業系統，支援多工處理、虛擬記憶體和檔案系統。\n");
    } else if (strstr(input, "謝謝") || strstr(input, "thank")) {
        printf("不客氣！還有什麼我可以幫助你的嗎？\n");
    } else if (strlen(input) == 0) {
        printf("請輸入你的問題或想法。\n");
    } else {
        printf("我理解你說的是：\"%s\"。這很有趣！請告訴我更多。\n", input);
    }
}

void main(void) {
    int in_llm_mode = 0;  // LLM 模式狀態

    while (1) {
prompt:
        if (in_llm_mode) {
            printf("[LLM] > ");
        } else {
            printf("> ");
        }

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

        // 處理命令
        if (in_llm_mode) {
            // LLM 模式下的命令處理
            if (strcmp(cmdline, "!exit") == 0) {
                in_llm_mode = 0;
                printf("退出 LLM 模式\n");
            } else {
                llm_response(cmdline);
            }
        } else {
            // 一般 shell 模式下的命令處理
            if (strcmp(cmdline, "hello") == 0)
                printf("Hello world from shell!\n");
            else if (strcmp(cmdline, "exit") == 0)
                exit();
            else if (strcmp(cmdline, "help") == 0) {
                printf("\n=== RISC-V OS Shell 命令說明 ===\n");
                printf("hello  - 顯示歡迎訊息\n");
                printf("exit   - 結束程式\n");
                printf("help   - 顯示此說明\n");
                printf("llm    - 進入 AI 對話模式\n");
            }
            else if (strcmp(cmdline, "llm") == 0) {
                in_llm_mode = 1;
                printf("[LLM] 歡迎！我是你的 AI 助手，有什麼可以幫助你的嗎？\n");
                printf("[LLM] 輸入 !exit 可以退出對話模式。\n");
            }
            else
                printf("unknown command: %s\n", cmdline);
        }
    }
}
