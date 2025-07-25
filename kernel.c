#include "kernel.h"
#include "common.h"
#include "virtio.h"

// LLM 函數前向宣告
void llm_write_file(int file_id, const char *data);
void llm_read_file(int file_id, char *buffer);
void llm_simulate_response(const char *input, char *response);

#define UART_BASE 0x10000000
#define UART_RHR  0x00 // 接收暫存器
#define UART_LSR  0x05 // Line Status Register

extern char __bss[], __bss_end[], __stack_top[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

extern char __free_ram[], __free_ram_end[];

extern paddr_t alloc_pages(uint32_t n);
extern void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);

struct process *current_proc; // 当前运行的进程
struct process *idle_proc;    // 空闲进程

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

long getchar(void) {
    struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
    return ret.error;
}

// 新增：非阻塞讀取UART字元（只在核心模式下使用）
int getchar_raw(void) {
    volatile uint8_t *uart = (volatile uint8_t *)UART_BASE;
    if (uart[UART_LSR] & 0x01) { // Data Ready
        return uart[UART_RHR];
    } else {
        return -1;
    }
}

__attribute__((naked)) void switch_context(uint32_t *prev_sp,
                                           uint32_t *next_sp) {
    __asm__ __volatile__(
        // 保存被调用者保存的寄存器到当前进程的栈上
        "addi sp, sp, -13 * 4\n" // 为13个4字节寄存器分配栈空间
        "sw ra,  0  * 4(sp)\n"
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // 保存当前栈指针到 *prev_sp，并加载新栈指针
        "sw sp, (a0)\n"
        "lw sp, (a1)\n"

        // 从新栈中恢复寄存器
        "lw ra,  0  * 4(sp)\n"
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"
        "ret\n"
    );
}

struct process procs[PROCS_MAX]; // 所有进程控制结构

void yield(void) {
    // 搜索可运行的进程
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    if (next == current_proc)
        return;

    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
          [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

__attribute__((naked)) void user_entry(void) {
    __asm__ __volatile__(
        "csrw sepc, %[sepc]\n"
        "csrw sstatus, %[sstatus]\n"
        "sret\n"
        :
        : [sepc] "r" (USER_BASE),
          [sstatus] "r" (SSTATUS_SPIE | SSTATUS_SUM)
    );
}

extern char __kernel_base[];

struct process *create_process(const void *image, size_t image_size) {
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slots");

    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) user_entry;  // ra

    uint32_t *page_table = (uint32_t *) alloc_pages(1);

    // Kernel pages.
    for (paddr_t paddr = (paddr_t) __kernel_base;
         paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    // virtio-blk
    map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W);

    // UART
    map_page(page_table, UART_BASE, UART_BASE, PAGE_R | PAGE_W);

    // User pages.
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;
        memcpy((void *) page, image + off, copy_size);
        map_page(page_table, USER_BASE + off, page,
                 PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table;
    return proc;
}

void delay(void) {
    for (int i = 0; i < 30000000; i++)
        __asm__ __volatile__("nop");
}

struct process *proc_a;
struct process *proc_b;

void proc_a_entry(void) {
    printf("starting process A\n");
    while (1) {
        putchar('A');
        yield();
    }
}

void proc_b_entry(void) {
    printf("starting process B\n");
    while (1) {
        putchar('B');
        yield();
    }
}

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        "csrrw sp, sscratch, sp\n"
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        "csrr a0, sscratch\n"
        "sw a0,  4 * 30(sp)\n"

        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

void handle_syscall(struct trap_frame *f) {
    switch (f->a3) {
        case SYS_PUTCHAR:
            putchar(f->a0);
            break;
        case SYS_GETCHAR:
            while (1) {
                long ch = getchar();
                if (ch >= 0) {
                    f->a0 = ch;
                    break;
                }
                yield();
            }
            break;
        case SYS_GETCHAR_NONBLOCK:
            f->a0 = getchar_raw();
            break;

        // LLM 相關系統呼叫
        case 200: // SYS_LLM_SEND_REQUEST
            {
                char request[LLM_MAX_MSG_SIZE];
                char *user_request = (char*)f->a0;
                strcpy(request, user_request);
                llm_write_file(LLM_REQUEST_FILE, request);
                llm_write_file(LLM_STATUS_FILE, "request_sent");
                f->a0 = 0; // 成功
            }
            break;

        case 201: // SYS_LLM_GET_RESPONSE
            {
                char response[LLM_MAX_MSG_SIZE];
                char status[64];
                llm_read_file(LLM_STATUS_FILE, status);

                // 清理狀態字串，移除 null bytes
                for (int i = 0; i < 63; i++) {
                    if (status[i] == '\0') {
                        status[i] = '\0';
                        break;
                    }
                }

                if (strcmp(status, "response_ready") == 0) {
                    llm_read_file(LLM_RESPONSE_FILE, response);
                    char *user_buffer = (char*)f->a0;
                    strcpy(user_buffer, response);
                    llm_write_file(LLM_STATUS_FILE, "idle");
                    f->a0 = 1; // 有回應
                } else {
                    f->a0 = 0; // 無回應
                }
            }
            break;

        case 202: // SYS_LLM_SIMULATE
            {
                char request[LLM_MAX_MSG_SIZE];
                char response[LLM_MAX_MSG_SIZE];
                char *user_request = (char*)f->a0;
                char *user_response = (char*)f->a1;

                strcpy(request, user_request);
                llm_simulate_response(request, response);
                strcpy(user_response, response);
                f->a0 = 0; // 成功
            }
            break;
        case SYS_EXIT:
            printf("process %d exited\n", current_proc->pid);
            current_proc->state = PROC_EXITED;
            yield();
            PANIC("unreachable");
        default:
            PANIC("unexpected syscall a3=%x\n", f->a3);
    }
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        user_pc += 4;
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}


void kernel_main(void) {
    memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);

    printf("\n\n");

    WRITE_CSR(stvec, (uint32_t) kernel_entry);
	virtio_blk_init();

	char buf[SECTOR_SIZE];
    read_write_disk(buf, 0, false /* 从磁盘读取 */);
    printf("first sector: %s\n", buf);

    strcpy(buf, "hello from kernel!!!\n");
    read_write_disk(buf, 0, true /* 写入磁盘 */);

    // 初始化 LLM 檔案系統
    llm_write_file(LLM_STATUS_FILE, "idle");
    printf("LLM 檔案系統已初始化\n");

    idle_proc = create_process(NULL, 0);
    idle_proc->pid = -1;
    current_proc = idle_proc;

    create_process(_binary_shell_bin_start, (size_t)_binary_shell_bin_size);

    yield();
    PANIC("switched to idle process");
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // 设置栈指针
        "j kernel_main\n"
        :
        : [stack_top] "r" (__stack_top)
    );
}

// LLM 檔案操作函數
void llm_write_file(int file_id, const char *data) {
    char filename[32];
    if (file_id == LLM_REQUEST_FILE) {
        strcpy(filename, "llm_request.txt");
    } else if (file_id == LLM_RESPONSE_FILE) {
        strcpy(filename, "llm_response.txt");
    } else if (file_id == LLM_STATUS_FILE) {
        strcpy(filename, "llm_status.txt");
    } else {
        return;
    }

    // 寫入到磁碟的第一個磁區（模擬檔案系統）
    char buffer[SECTOR_SIZE];
    memset(buffer, 0, SECTOR_SIZE);
    strcpy(buffer, data);
    read_write_disk(buffer, file_id, true); // 寫入
}

void llm_read_file(int file_id, char *buffer) {
    char filename[32];
    if (file_id == LLM_REQUEST_FILE) {
        strcpy(filename, "llm_request.txt");
    } else if (file_id == LLM_RESPONSE_FILE) {
        strcpy(filename, "llm_response.txt");
    } else if (file_id == LLM_STATUS_FILE) {
        strcpy(filename, "llm_status.txt");
    } else {
        return;
    }

    // 從磁碟讀取（模擬檔案系統）
    char disk_buffer[SECTOR_SIZE];
    read_write_disk(disk_buffer, file_id, false); // 讀取
    strcpy(buffer, disk_buffer);
}

// 模擬 LLM 回應（暫時使用）
void llm_simulate_response(const char *input, char *response) {
    if (strstr(input, "你好") || strstr(input, "hello")) {
        strcpy(response, "你好！我是基於 RISC-V OS 的 AI 助手。");
    } else if (strstr(input, "幫助") || strstr(input, "help")) {
        strcpy(response, "我可以幫助你了解這個作業系統，或者回答一些基本問題。");
    } else if (strstr(input, "系統") || strstr(input, "system")) {
        strcpy(response, "這是一個 RISC-V 32位元作業系統，支援多工處理、虛擬記憶體和檔案系統。");
    } else if (strstr(input, "謝謝") || strstr(input, "thank")) {
        strcpy(response, "不客氣！還有什麼我可以幫助你的嗎？");
    } else {
        strcpy(response, "我理解你的輸入，這很有趣！請告訴我更多。");
    }
}

