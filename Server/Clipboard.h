// =============================================================
// MODULE: CLIPBOARD MONITOR (HEADER)
// Nhiệm vụ: Theo dõi và ghi lại nội dung văn bản được Copy
// =============================================================

#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <windows.h>

class Clipboard {
private:
    std::string _logBuffer;       // Bộ đệm lưu trữ nội dung đã copy
    std::mutex _mutex;            // Khóa bảo vệ luồng (Thread-safe)
    std::thread _monitorThread;   // Luồng chạy ngầm giám sát
    bool _isRunning;              // Cờ kiểm soát trạng thái chạy

    // Windows đánh số mỗi lần nội dung Clipboard thay đổi.
    // Ta lưu số này để biết khi nào có dữ liệu mới.
    DWORD _lastSequenceNumber;

    void RunMonitorLoop();          // Vòng lặp chính của thread
    std::string GetClipboardText(); // Hàm giao tiếp WinAPI để lấy text

public:
    Clipboard();
    ~Clipboard();

    void StartMonitoring();       // Bắt đầu theo dõi
    void StopMonitoring();        // Dừng theo dõi
    std::string GetLogs();        // Lấy dữ liệu log và xóa bộ đệm
};