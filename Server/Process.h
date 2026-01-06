// =============================================================
// MODULE: PROCESS MANAGEMENT (HEADER)
// Nhiệm vụ: Quản lý tiến trình, liệt kê ứng dụng cài đặt,
//           khởi chạy (Start) và dừng (Kill) ứng dụng.
// =============================================================

#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <shlobj.h>  // Cho Start Menu search
#include <shlwapi.h> // Cho thao tác path

// Định nghĩa cấu trúc lưu thông tin Ứng dụng
struct AppInfo {
    std::string name; // Tên hiển thị (Display Name)
    std::string path; // Đường dẫn tệp thực thi (.exe)
};

class Process {
protected:
    // Dữ liệu dùng chung: Danh sách các ứng dụng đã quét được
    std::vector<AppInfo> m_installedApps;
    bool m_isListLoaded = false;

    // --- CÁC HÀM HỖ TRỢ NỘI BỘ (PRIVATE HELPERS) ---
    
    // Quét Registry theo đường dẫn Key cụ thể (Uninstall key)
    void ScanRegistryKey(HKEY hRoot, const char* subKey);
    
    // Lấy giá trị chuỗi từ Registry (Hỗ trợ Unicode)
    std::string GetRegString(HKEY hKey, const char* valueName);
    
    // Lấy dung lượng RAM đang sử dụng của một PID (MB)
    double GetProcessMemory(DWORD pid);
    
    // Quét Registry khu vực "App Paths"
    void ScanAppPaths(HKEY hRoot, const char* subKey);

    // Tìm kiếm file đệ quy trong thư mục (Hỗ trợ tìm Shortcut trong Start Menu)
    std::string FindFileRecursive(std::string directory, std::string fileToFind);
    
    // Tìm đường dẫn ứng dụng trong Start Menu/Desktop
    std::string FindAppInStartMenu(std::string appName);

public:
    Process();

    // Tải/Làm mới danh sách Ứng dụng đã cài đặt vào bộ nhớ
    void LoadInstalledApps();

    // Khởi chạy ứng dụng (Start)
    // Logic: Thử chạy trực tiếp -> Tìm trong Registry -> Tìm trong Start Menu
    bool StartProcess(const std::string& nameOrPath);

    // Dừng tiến trình (Kill)
    // Input: Có thể là Tên (notepad.exe) hoặc PID (1234)
    std::string StopProcess(const std::string& nameOrPid);

    // Liệt kê danh sách các tiến trình đang chạy (PID, RAM, Threads...)
    std::string ListProcesses();

    // --- CÁC HÀM TIỆN ÍCH TĨNH (STATIC UTILS) ---
    static std::string ToLower(std::string str);
    static bool IsNumeric(const std::string& str);
    static std::string CleanPath(const std::string& rawPath);       // Làm sạch đường dẫn (bỏ quote, tham số)
    static std::string ConvertToString(const WCHAR* wstr);          // Chuyển đổi WCHAR sang UTF-8
};