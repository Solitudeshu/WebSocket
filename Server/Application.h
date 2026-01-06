// =============================================================
// MODULE: APPLICATION MANAGER (HEADER)
// Nhiệm vụ: Mở rộng từ lớp Process, bổ sung tính năng
//           kiểm tra trạng thái Running/Stopped của ứng dụng.
// =============================================================

#pragma once
#include "Process.h" // Kế thừa từ Process đã có đệ quy
#include <vector>
#include <unordered_set>

class Application : public Process {
private:
    // --- CÁC HÀM HỖ TRỢ NỘI BỘ (PRIVATE HELPERS) ---

    // Xây dựng tập hợp (Set) chứa đường dẫn các file .exe đang chạy
    std::unordered_set<std::string> BuildRunningExeSet();

    // Lấy đường dẫn file thực thi từ PID (Hỗ trợ WinAPI)
    bool GetProcessImagePath(DWORD pid, std::string& outPath);

public:
    Application();

    // Liệt kê danh sách ứng dụng đã cài đặt kèm trạng thái (Running/Stopped)
    std::vector<std::pair<AppInfo, bool>> ListApplicationsWithStatus();

    // In danh sách ra màn hình Console (Debug purpose)
    void PrintApplicationsWithStatus(size_t maxShow = 50);

    // Wrapper: Gọi lại hàm StartProcess của lớp cha (Process)
    // Hỗ trợ khởi chạy từ Tên, Đường dẫn Registry hoặc Start Menu
    bool StartApplication(const std::string& pathOrName);

    // Wrapper: Gọi lại hàm StopProcess của lớp cha
    // Hỗ trợ dừng theo Tên hoặc PID
    bool StopApplication(const std::string& nameOrPid);
};