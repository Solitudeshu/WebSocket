// =============================================================
// MODULE: CLIPBOARD MONITOR (SOURCE)
// =============================================================

#include "Clipboard.h"
#include <vector>

// Constructor
Clipboard::Clipboard() : _isRunning(false), _lastSequenceNumber(0) {}

// Destructor: Đảm bảo dừng luồng khi hủy đối tượng
Clipboard::~Clipboard() {
    StopMonitoring();
}

// =============================================================
// PHẦN 1: TƯƠNG TÁC HỆ THỐNG (WINAPI)
// =============================================================

std::string Clipboard::GetClipboardText() {
    // 1. Kiểm tra xem Clipboard có chứa văn bản Unicode không?
    // Nếu là ảnh hoặc file thì bỏ qua.
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return "";

    // 2. Mở Clipboard
    if (!OpenClipboard(NULL)) return "";

    std::string result = "";

    // 3. Lấy handle dữ liệu
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData != NULL) {
        // 4. Khóa bộ nhớ toàn cục để đọc dữ liệu an toàn
        WCHAR* pszText = static_cast<WCHAR*>(GlobalLock(hData));
        if (pszText != NULL) {
            // 5. Chuyển đổi từ UTF-16 (Windows) sang UTF-8 (Web/Socket)
            // Lấy kích thước buffer cần thiết
            int bufferSize = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, NULL, 0, NULL, NULL);
            if (bufferSize > 0) {
                std::vector<char> buffer(bufferSize);
                // Thực hiện chuyển đổi
                WideCharToMultiByte(CP_UTF8, 0, pszText, -1, &buffer[0], bufferSize, NULL, NULL);
                // Gán vào kết quả (trừ ký tự null kết thúc)
                result = std::string(buffer.begin(), buffer.end() - 1);
            }
            // Mở khóa bộ nhớ
            GlobalUnlock(hData);
        }
    }

    // 6. Đóng Clipboard (Bắt buộc, nếu không các app khác sẽ không copy được)
    CloseClipboard();
    return result;
}

// =============================================================
// PHẦN 2: VÒNG LẶP GIÁM SÁT (THREAD LOOP)
// =============================================================

void Clipboard::RunMonitorLoop() {
    // Lấy số thứ tự hiện tại làm mốc ban đầu
    _lastSequenceNumber = GetClipboardSequenceNumber();

    while (_isRunning) {
        // Lấy số thứ tự hiện tại của hệ thống
        DWORD currentSeq = GetClipboardSequenceNumber();

        // Nếu số thay đổi -> Có nội dung mới được Copy
        if (currentSeq != _lastSequenceNumber) {
            _lastSequenceNumber = currentSeq;

            std::string text = GetClipboardText();
            if (!text.empty()) {
                // Khóa mutex để ghi log an toàn
                std::lock_guard<std::mutex> lock(_mutex);
                _logBuffer += "[CLIPBOARD]: " + text + "\n";
            }
        }

        // Nghỉ 500ms để tiết kiệm CPU (kiểm tra 2 lần/giây)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// =============================================================
// PHẦN 3: ĐIỀU KHIỂN & TRÍCH XUẤT DỮ LIỆU
// =============================================================

void Clipboard::StartMonitoring() {
    if (_isRunning) return; // Đang chạy rồi thì thôi
    _isRunning = true;
    _logBuffer = "";

    // Khởi tạo luồng mới
    _monitorThread = std::thread(&Clipboard::RunMonitorLoop, this);
}

void Clipboard::StopMonitoring() {
    if (!_isRunning) return;
    _isRunning = false;

    // Chờ luồng kết thúc công việc rồi mới thoát
    if (_monitorThread.joinable()) {
        _monitorThread.join();
    }
}

std::string Clipboard::GetLogs() {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_logBuffer.empty()) return "";

    // Copy dữ liệu ra và xóa bộ đệm cũ
    std::string data = _logBuffer;
    _logBuffer.clear();

    return data;
}