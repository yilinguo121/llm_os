#!/bin/bash
set -xue

# QEMU 執行檔路徑，保持不變
QEMU=qemu-system-riscv32

# 使用 clang 作為交叉編譯器，並設定目標為 riscv32
export CC=clang
# 這裡我們加上 -march 與 -mabi 參數確保使用正確的 RISC-V 32 位架構與 ABI
export CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32 -ffreestanding -nostdlib -march=rv32imac -mabi=ilp32"

# 構建用戶應用程序 (shell.elf)
$CC $CFLAGS -Wl,-Tuser.ld -Wl,-Map=shell.map -o shell.elf shell.c user.c common.c

# 使用 llvm-objcopy（或系統中的 objcopy）將 ELF 轉為純二進制文件
if command -v llvm-objcopy > /dev/null 2>&1; then
    OBJCOPY=llvm-objcopy
elif command -v objcopy > /dev/null 2>&1; then
    OBJCOPY=objcopy
else
    echo "Error: 沒有找到 objcopy 工具，請先安裝一個。" >&2
    exit 1
fi

$OBJCOPY --set-section-flags .bss=alloc,contents -O binary shell.elf shell.bin
$OBJCOPY -I binary -O elf32-littleriscv shell.bin shell.bin.o

# 構建內核，並將用戶程序 (shell.bin.o) 嵌入內核映像中
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf kernel.c mm.c virtio.c common.c shell.bin.o

# 啟動 QEMU，運行內核映像
$QEMU -machine virt \
      -bios default \
      -nographic \
      -serial mon:stdio \
      --no-reboot \
      -d unimp,guest_errors,int,cpu_reset -D qemu.log \
      -drive id=drive0,file=lorem.txt,format=raw,if=none \
      -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
      -kernel kernel.elf

