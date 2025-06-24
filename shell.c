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

// LLM 對話模式相關函數
void llm_mode() {
    printf("\n=== LLM 對話模式 ===\n");
    printf("輸入 !exit 退出對話模式\n");
    printf("輸入 !help 查看幫助\n");
    printf("輸入 !status 查看連接狀態\n\n");

    char input[LLM_MAX_MSG_SIZE];
    char response[LLM_MAX_MSG_SIZE];

    while (1) {
        printf("AI> ");

        // 讀取用戶輸入
        int i = 0;
        while (i < LLM_MAX_MSG_SIZE - 1) {
            int ch = getchar();
            if (ch == '\n' || ch == '\r') { // 支援 Enter
                input[i] = '\0';
                break;
            } else if ((ch == '\b' || ch == 127) && i > 0) { // 支援退格
                i--;
                printf("\b \b");
            } else if (ch >= 32 && ch < 127) {
                input[i++] = ch;
                putchar(ch);
            }
        }

        // 檢查退出命令
        if (strcmp(input, "!exit") == 0) {
            printf("\n退出 LLM 對話模式\n");
            break;
        }

        // 檢查幫助命令
        if (strcmp(input, "!help") == 0) {
            printf("\nLLM 對話模式幫助：\n");
            printf("- 直接輸入問題與 AI 對話\n");
            printf("- !exit: 退出對話模式\n");
            printf("- !help: 顯示此幫助\n");
            printf("- !status: 查看連接狀態\n");
            printf("- 支援中文和英文輸入\n");
            printf("- 使用 Host-Guest 檔案交換機制\n\n");
            continue;
        }

        // 檢查狀態命令
        if (strcmp(input, "!status") == 0) {
            printf("\n=== 連接狀態 ===\n");
            printf("使用 VirtIO 磁碟檔案交換\n");
            printf("Sector 0: 請求檔案\n");
            printf("Sector 1: 回應檔案\n");
            printf("Sector 2: 狀態檔案\n");
            printf("請確保 Host 端 Python 服務正在運行\n\n");
            continue;
        }

        // 跳過空輸入
        if (strlen(input) == 0) {
            continue;
        }

        // 發送請求到 Host 端
        printf("\n[發送請求中...]\n");
        int send_result = syscall(200, (int)input, 0, 0); // SYS_LLM_SEND_REQUEST

        if (send_result != 0) {
            printf("錯誤：無法發送請求\n\n");
            continue;
        }

        // 等待回應
        printf("[等待回應中...]\n");
        int retry_count = 0;
        int has_response = 0;

        while (retry_count < 20000) { // 最多等待 20 秒
            has_response = syscall(201, (int)response, 0, 0); // SYS_LLM_GET_RESPONSE
            if (has_response) {
                break;
            }
            // 簡單延遲
            for (volatile int i = 0; i < 1000000; i++);
            retry_count++;
        }

        if (has_response) {
            printf("\nAI: %s\n\n", response);
        } else {
            printf("\nAI: 抱歉，沒有收到回應。請檢查 Host 端服務是否正在運行。\n\n");
        }
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
                printf("\n=== LLM 檔案系統 ===\n");
                printf("LLM 使用 VirtIO 磁碟進行檔案交換：\n");
                printf("- 磁區 0: llm_request.txt (請求檔案)\n");
                printf("- 磁區 1: llm_response.txt (回應檔案)\n");
                printf("- 磁區 2: llm_status.txt (狀態檔案)\n");
            }
            else if (strcmp(cmdline, "llm") == 0) {
                llm_mode();
            }
            else
                printf("unknown command: %s\n", cmdline);
        }
    }
}
