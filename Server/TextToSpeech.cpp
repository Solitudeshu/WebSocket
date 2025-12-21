// =============================================================
// MODULE: TEXT TO SPEECH (SOURCE)
// =============================================================

#include "TextToSpeech.h"

// --- LIÊN KẾT THƯ VIỆN HỆ THỐNG ---
// SAPI: Speech API
// OLE32: Quản lý đối tượng COM (Component Object Model)
#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "ole32.lib")

// Constructor & Destructor
TextToSpeech::TextToSpeech() {}
TextToSpeech::~TextToSpeech() {}

// =============================================================
// PHẦN 1: HÀM HỖ TRỢ CHUYỂN ĐỔI KÝ TỰ
// =============================================================

std::wstring TextToSpeech::Utf8ToWstring(const std::string& str) {
    if (str.empty()) return std::wstring();

    // Bước 1: Tính độ dài cần thiết
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);

    // Bước 2: Cấp phát bộ nhớ và chuyển đổi
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);

    return wstrTo;
}

// =============================================================
// PHẦN 2: LOGIC XỬ LÝ SAPI (CHẠY TRÊN THREAD RIÊNG)
// =============================================================

void TextToSpeech::SpeakLogic(std::string text) {
    // 1. Khởi tạo môi trường COM cho luồng này
    // SAPI là công nghệ dựa trên COM, bắt buộc phải Initialize trước
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return;

    // 2. Tạo đối tượng Voice (ISpVoice)
    ISpVoice* pVoice = NULL;
    hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);

    if (SUCCEEDED(hr)) {
        // 3. Chuyển nội dung sang Unicode (Windows yêu cầu)
        std::wstring wText = Utf8ToWstring(text);

        // 4. Phát âm thanh
        // SPF_DEFAULT: Chế độ đồng bộ (chờ đọc xong mới chạy tiếp lệnh dưới)
        // Vì hàm này đã nằm trong thread riêng, nên việc chờ đợi không làm treo Server chính.
        pVoice->Speak(wText.c_str(), SPF_DEFAULT, NULL);

        // 5. Giải phóng đối tượng Voice
        pVoice->Release();
        pVoice = NULL;
    }

    // 6. Hủy môi trường COM (Dọn dẹp)
    CoUninitialize();
}

// =============================================================
// PHẦN 3: GIAO DIỆN GỌI HÀM (PUBLIC)
// =============================================================

void TextToSpeech::Speak(const std::string& text) {
    // Tạo thread dạng "Fire and Forget" (Bắn và Quên)
    // Server không cần quan tâm khi nào nó đọc xong, cũng không cần quản lý thread này.
    std::thread t(&TextToSpeech::SpeakLogic, this, text);
    t.detach(); // Tách luồng để nó tự chạy ngầm và tự hủy khi xong
}