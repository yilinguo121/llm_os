#!/usr/bin/env python3
"""
Host 端 LLM 服務
監控 lorem.txt 的 sector 變化，與 OS 進行 LLM 對話
"""

import os
import time
import struct
from typing import Optional
import openai

SECTOR_SIZE = 512

class LLMHostService:
    def __init__(self, disk_file="lorem.txt"):
        self.disk_file = disk_file
        self.last_status = ""
        self.openai_api_key = os.getenv("OPENAI_API_KEY")

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
        """只呼叫新版 OpenAI API，失敗就顯示錯誤，不再有模擬回應"""
        if not openai or not self.openai_api_key:
            return "[錯誤] OpenAI API 未正確設定，請確認已安裝 openai 套件並設置 OPENAI_API_KEY。"
        try:
            client = openai.OpenAI(api_key=self.openai_api_key)
            response = client.chat.completions.create(
                model="gpt-3.5-turbo",
                messages=[
                    {"role": "system", "content": "你是 RISC-V OS 的 AI 助手，請用繁體中文回答。"},
                    {"role": "user", "content": request}
                ],
                max_tokens=256,
                temperature=0.7
            )
            reply = response.choices[0].message.content.strip()
            return reply
        except Exception as e:
            return f"[OpenAI API 錯誤] {e}"

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
