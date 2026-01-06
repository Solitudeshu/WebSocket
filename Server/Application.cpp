// =============================================================
// MODULE: APPLICATION MANAGER (SOURCE)
// =============================================================

#include "Application.h"
#include <iostream>

using namespace std;

Application::Application() {
    // Constructor mặc định gọi constructor của Process
}

// =============================================================
// PHẦN 1: CÁC HÀM HỖ TRỢ KIỂM TRA TRẠNG THÁI (STATUS CHECKER)
// =============================================================

// Lấy đường dẫn file thực thi (Image Path) từ Process ID
bool Application::GetProcessImagePath(DWORD pid, string& outPath) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;
    char buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameA(hProcess, 0, buffer, &size)) {
        outPath.assign(buffer, size);
        CloseHandle(hProcess);
        return true;
    }
    CloseHandle(hProcess);
    return false;
}

// Tạo danh sách các file .exe đang chạy trong hệ thống
std::unordered_set<std::string> Application::BuildRunningExeSet() {
    unordered_set<string> runningSet;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return runningSet;

    PROCESSENTRY32W pe; // Dùng bản W (Unicode) để đồng bộ với Process.cpp
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == 0) continue;
            string procPath;

            // Ưu tiên lấy đường dẫn đầy đủ để so sánh chính xác
            if (GetProcessImagePath(pe.th32ProcessID, procPath)) {
                // Chuẩn hóa: Chuyển về chữ thường và làm sạch đường dẫn
                runningSet.insert(Process::ToLower(Process::CleanPath(procPath)));
            }
            else {
                // Nếu không lấy được đường dẫn (do quyền hạn), lấy tên file exe
                // Dùng hàm ConvertToString static từ class Process
                runningSet.insert(Process::ToLower(Process::ConvertToString(pe.szExeFile)));
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return runningSet;
}

// =============================================================
// PHẦN 2: CÁC HÀM XỬ LÝ CHÍNH (PUBLIC FUNCTIONS)
// =============================================================

// Liệt kê ứng dụng và kiểm tra trạng thái hoạt động
vector<pair<AppInfo, bool>> Application::ListApplicationsWithStatus() {
    // 1. Đảm bảo danh sách App đã cài được load (tận dụng logic của class Cha)
    if (!m_isListLoaded) {
        LoadInstalledApps();
    }

    vector<pair<AppInfo, bool>> result;
    auto runningSet = BuildRunningExeSet(); // Lấy danh sách các file đang chạy hiện tại

    // 2. Duyệt qua danh sách app đã cài để đối chiếu trạng thái
    for (const auto& app : m_installedApps) {
        string normalizedPath = Process::ToLower(Process::CleanPath(app.path));
        bool running = false;

        // Kiểm tra xem đường dẫn cài đặt có nằm trong tập đang chạy không
        for (const auto& runPath : runningSet) {
            // Logic so sánh linh hoạt: 
            // - runPath chứa normalizedPath (VD: chạy C:\Folder\App.exe so với C:\Folder\App.exe)
            // - hoặc normalizedPath chứa runPath (để bắt các trường hợp đường dẫn tương đối hoặc alias)
            if (runPath.find(normalizedPath) != string::npos || normalizedPath.find(runPath) != string::npos) {
                running = true;
                break;
            }
        }
        result.push_back({ app, running });
    }
    return result;
}

// In danh sách ra console (Helper cho Debug)
void Application::PrintApplicationsWithStatus(size_t maxShow) {
    auto list = ListApplicationsWithStatus();
    size_t count = min(maxShow, list.size());

    cout << "--- Installed Applications Status ---\n";
    for (size_t i = 0; i < count; ++i) {
        cout << "[" << i << "] " << left << setw(30) << list[i].first.name
            << " | Status: " << (list[i].second ? "RUNNING" : "STOPPED") << "\n";
    }
}

// Gọi lại hàm Start của lớp cha (đã tích hợp tìm kiếm đệ quy và Start Menu)
bool Application::StartApplication(const string& pathOrName) {
    // Hàm Process::StartProcess() đã xử lý toàn bộ logic: 
    // (Direct Execution -> Registry Search -> Recursive Start Menu Search)
    return Process::StartProcess(pathOrName);
}

// Gọi lại hàm Stop của lớp cha và xử lý kết quả trả về
bool Application::StopApplication(const string& nameOrPid) {
    // Gọi hàm StopProcess từ class cha
    string res = Process::StopProcess(nameOrPid);

    // In thông báo ra cho người dùng biết kết quả (Server Log)
    cout << "[System]: " << res << endl;

    // Phân tích kết quả trả về để xác định thành công hay thất bại
    // Process trả về chuỗi "Killed..." hoặc "Err...", "Not found"
    if (res.find("Killed") != string::npos) return true;

    return false;
}