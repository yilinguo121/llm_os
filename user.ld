ENTRY(start)

SECTIONS {
    . = 0x1000000;  /* 設定應用程序的起始地址，避免與內核重疊 */

    /* 機器碼段 */
    .text : {
        KEEP(*(.text.start));  /* 保留入口函數 */
        *(.text .text.*);
    }

    /* 只讀數據段 */
    .rodata : ALIGN(4) {
        *(.rodata .rodata.*);
    }

    /* 已初始化數據段 */
    .data : ALIGN(4) {
        *(.data .data.*);
    }

    /* bss 段：啟動時需被清零 */
    .bss : ALIGN(4) {
        *(.bss .bss.* .sbss .sbss.*);

        . = ALIGN(16);
        . += 64 * 1024;  /* 預留 64KB 的空間給棧 */
        __stack_top = .;

        ASSERT(. < 0x1800000, "too large executable");
    }
}

