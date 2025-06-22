#!/usr/bin/env python3
"""
LLM 檔案交換流程驗證工具
確認 OS -> Python -> OS 的完整檔案交換流程
"""

import os
import time
import threading

SECTOR_SIZE = 512

class LLMFlowVerifier:
    def __init__(self, disk_file="lorem.txt"):
        self.disk_file = disk_file
        self.monitoring = False

    def read_sector(self, sector_num: int) -> bytes:
        """讀取指定 sector 的內容"""
        with open(self.disk_file, 'rb') as f:
            f.seek(sector_num * SECTOR_SIZE)
            return f.read(SECTOR_SIZE)

    def get_sector_text(self, sector_num: int) -> str:
        """讀取 sector 的文字內容"""
        data = self.read_sector(sector_num)
        null_pos = data.find(b'\x00')
        if null_pos == -1:
            null_pos = len(data)
        return data[:null_pos].decode('utf-8', errors='ignore')

    def monitor_files(self):
        """監控檔案變化"""
        print("=== 開始監控 LLM 檔案交換 ===")
        print("等待 OS 端發送請求...")

        last_request = ""
        last_response = ""
        last_status = ""

        while self.monitoring:
            try:
                # 讀取當前狀態
                current_request = self.get_sector_text(0)
                current_response = self.get_sector_text(1)
                current_status = self.get_sector_text(2)

                # 檢查是否有新請求
                if current_request != last_request and current_request.strip():
                    print(f"\n📤 [OS -> Python] 新請求: {current_request}")
                    last_request = current_request

                # 檢查狀態變化
                if current_status != last_status:
                    print(f"🔄 狀態變化: '{last_status}' -> '{current_status}'")
                    last_status = current_status

                # 檢查是否有新回應
                if current_response != last_response and current_response.strip():
                    print(f"📥 [Python -> OS] 新回應: {current_response}")
                    last_response = current_response

                time.sleep(0.1)

            except KeyboardInterrupt:
                print("\n停止監控")
                break
            except Exception as e:
                print(f"監控錯誤: {e}")
                time.sleep(1)

    def start_monitoring(self):
        """開始監控"""
        self.monitoring = True
        self.monitor_files()

    def stop_monitoring(self):
        """停止監控"""
        self.monitoring = False

    def show_current_state(self):
        """顯示當前狀態"""
        print("\n=== 當前檔案狀態 ===")
        print(f"Sector 0 (請求): '{self.get_sector_text(0)}'")
        print(f"Sector 1 (回應): '{self.get_sector_text(1)}'")
        print(f"Sector 2 (狀態): '{self.get_sector_text(2)}'")

    def test_manual_flow(self):
        """手動測試檔案交換流程"""
        print("\n=== 手動測試檔案交換 ===")

        # 模擬 OS 發送請求
        print("1. 模擬 OS 發送請求...")
        self.write_sector(0, "測試請求：你好".encode("utf-8"))
        self.write_sector(2, b"request_sent")
        print("   ✅ 請求已寫入 Sector 0")
        print("   ✅ 狀態已設為 'request_sent'")

        time.sleep(1)

        # 模擬 Python 處理回應
        print("\n2. 模擬 Python 處理回應...")
        self.write_sector(1, "你好！我是 AI 助手，很高興為你服務！".encode("utf-8"))
        self.write_sector(2, b"response_ready")
        print("   ✅ 回應已寫入 Sector 1")
        print("   ✅ 狀態已設為 'response_ready'")

        time.sleep(1)

        # 模擬 OS 讀取回應
        print("\n3. 模擬 OS 讀取回應...")
        self.write_sector(2, b"idle")
        print("   ✅ 狀態已設為 'idle'")

        print("\n=== 測試完成 ===")

    def write_sector(self, sector_num: int, data: bytes):
        """寫入資料到指定 sector"""
        if len(data) > SECTOR_SIZE:
            data = data[:SECTOR_SIZE]
        elif len(data) < SECTOR_SIZE:
            data = data.ljust(SECTOR_SIZE, b'\x00')

        with open(self.disk_file, 'r+b') as f:
            f.seek(sector_num * SECTOR_SIZE)
            f.write(data)
            f.flush()
            os.fsync(f.fileno())

def main():
    verifier = LLMFlowVerifier()

    print("LLM 檔案交換流程驗證工具")
    print("1. 顯示當前狀態")
    print("2. 開始監控檔案變化")
    print("3. 手動測試檔案交換")
    print("4. 退出")

    while True:
        try:
            choice = input("\n請選擇 (1-4): ").strip()

            if choice == "1":
                verifier.show_current_state()
            elif choice == "2":
                print("開始監控... (按 Ctrl+C 停止)")
                verifier.start_monitoring()
            elif choice == "3":
                verifier.test_manual_flow()
            elif choice == "4":
                print("退出")
                break
            else:
                print("無效選擇")

        except KeyboardInterrupt:
            print("\n停止監控")
            verifier.stop_monitoring()
        except Exception as e:
            print(f"錯誤: {e}")

if __name__ == "__main__":
    main()
