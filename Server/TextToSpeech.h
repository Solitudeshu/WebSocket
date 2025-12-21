// =============================================================
// MODULE: TEXT TO SPEECH (HEADER)
// Nhiệm vụ: Chuyển đổi văn bản thành giọng nói sử dụng MS SAPI
// =============================================================

#pragma once
#include <string>
#include <thread>
#include <windows.h>
#include <sapi.h> // Microsoft Speech API

class TextToSpeech {
private:
    // Chuyển đổi chuỗi UTF-8 (từ Web/Socket) sang Wide String (Windows Unicode)
    std::wstring Utf8ToWstring(const std::string& str);

    // Logic chính: Khởi tạo COM và đọc văn bản (Chạy trên luồng riêng)
    void SpeakLogic(std::string text);

public:
    TextToSpeech();
    ~TextToSpeech();

    // Hàm gọi từ bên ngoài (Non-blocking)
    void Speak(const std::string& text);
};