#!/usr/bin/env python3
"""
Host 端 LLM 服務
監控 lorem.txt 的 sector 變化，與 OS 進行 LLM 對話
"""

import os
import time
import struct
from typing import Optional

SECTOR_SIZE = 512

class LLMHostService:
    def __init__(self, disk_file="lorem.txt"):
        self.disk_file = disk_file
        self.last_status = ""

    def read_sector(self, sector_num: int) -> bytes:
        """讀取指定 sector 的內容"""
        with open(self.disk_file, 'rb') as f:
            f.seek(sector_num * SECTOR_SIZE)
            return f.read(SECTOR_SIZE)

    def write_sector(self, sector_num: int, data: bytes):
        """寫入資料到指定 sector"""
        # 確保資料長度是 SECTOR_SIZE
        if len(data) > SECTOR_SIZE:
            data = data[:SECTOR_SIZE]
        elif len(data) < SECTOR_SIZE:
            data = data.ljust(SECTOR_SIZE, b'\x00')

        with open(self.disk_file, 'r+b') as f:
            f.seek(sector_num * SECTOR_SIZE)
            f.write(data)
            f.flush()

    def get_status(self) -> str:
        """讀取狀態檔案 (sector 2)"""
        data = self.read_sector(2)
        # 找到第一個 null byte
        null_pos = data.find(b'\x00')
        if null_pos == -1:
            null_pos = len(data)
        return data[:null_pos].decode('utf-8', errors='ignore')

    def get_request(self) -> str:
        """讀取請求檔案 (sector 0)"""
        data = self.read_sector(0)
        # 找到第一個 null byte
        null_pos = data.find(b'\x00')
        if null_pos == -1:
            null_pos = len(data)
        return data[:null_pos].decode('utf-8', errors='ignore')

    def set_response(self, response: str):
        """寫入回應檔案 (sector 1)"""
        self.write_sector(1, response.encode('utf-8'))

    def set_status(self, status: str):
        """寫入狀態檔案 (sector 2)"""
        self.write_sector(2, status.encode('utf-8'))

    def simulate_llm_response(self, request: str) -> str:
        """模擬 LLM 回應（暫時使用）"""
        request_lower = request.lower()

        if "你好" in request or "hello" in request_lower:
            return "你好！我是基於 RISC-V OS 的 AI 助手。很高興與你對話！"
        elif "幫助" in request or "help" in request_lower:
            return "我可以幫助你了解這個作業系統，或者回答一些基本問題。請告訴我你想了解什麼？"
        elif "系統" in request or "system" in request_lower:
            return "這是一個 RISC-V 32位元作業系統，支援多工處理、虛擬記憶體、檔案系統和 LLM 對話功能。"
        elif "謝謝" in request or "thank" in request_lower:
            return "不客氣！還有什麼我可以幫助你的嗎？"
        elif "測試" in request or "test" in request_lower:
            return "LLM 對話模式測試成功！Host-Guest 檔案交換機制運作正常。"
        elif "時間" in request or "time" in request_lower:
            return f"現在時間是：{time.strftime('%Y-%m-%d %H:%M:%S')}"
        else:
            return f"我理解你的問題：「{request}」。這很有趣！請告訴我更多細節，我會盡力幫助你。"

    def process_requests(self):
        """處理請求的主迴圈"""
        print("=== LLM Host 服務啟動 ===")
        print(f"監控檔案：{self.disk_file}")
        print("等待 OS 端請求...")

        while True:
            try:
                # 檢查狀態
                current_status = self.get_status()

                # 如果有新請求
                if current_status == "request_sent":
                    print("\n[收到新請求]")

                    # 讀取請求內容
                    request = self.get_request()
                    print(f"請求內容：{request}")

                    # 模擬 LLM 處理（這裡可以替換為真實的 LLM API）
                    print("正在處理請求...")
                    response = self.simulate_llm_response(request)
                    print(f"回應內容：{response}")

                    # 寫入回應
                    self.set_response(response)

                    # 更新狀態為回應就緒
                    self.set_status("response_ready")
                    print("[回應已寫入，等待 OS 讀取]")

                # 如果狀態是 idle，表示 OS 已經讀取了回應
                elif current_status == "idle" and self.last_status == "response_ready":
                    print("[OS 已讀取回應，等待下一個請求]")

                self.last_status = current_status

                # 短暫休眠避免過度佔用 CPU
                time.sleep(0.1)

            except KeyboardInterrupt:
                print("\n[服務停止]")
                break
            except Exception as e:
                print(f"[錯誤] {e}")
                time.sleep(1)

def main():
    service = LLMHostService()
    service.process_requests()

if __name__ == "__main__":
    main()
