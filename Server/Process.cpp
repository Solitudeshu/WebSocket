// =============================================================
// MODULE: PROCESS MANAGEMENT (SOURCE)
// =============================================================

#include "Process.h"

// --- LIÊN KẾT THƯ VIỆN HỆ THỐNG (LINKER) ---
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
using namespace std;

Process::Process() { m_isListLoaded = false; }

// =============================================================
// PHẦN 1: CÁC HÀM TIỆN ÍCH (HELPER FUNCTIONS)
// =============================================================

// Helper: Chuyển đổi Unicode (WCHAR) sang UTF-8 (std::string)
string Process::ConvertToString(const WCHAR* wstr) {
    if (!wstr) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size, NULL, NULL);
    return str;
}

// Helper: Chuyển chuỗi về chữ thường (Lowercase)
string Process::ToLower(string str) {
    string res = str;
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

// Helper: Kiểm tra chuỗi có phải là số (PID) hay không
bool Process::IsNumeric(const string& str) {
    if (str.empty()) return false;
    return all_of(str.begin(), str.end(), ::isdigit);
}

// Helper: Làm sạch đường dẫn (Xóa dấu ngoặc kép, tham số thừa)
string Process::CleanPath(const string& rawPath) {
    string s = rawPath;
    // 1. Xóa khoảng trắng đầu cuối
    size_t first = s.find_first_not_of(" \t\r\n");
    if (string::npos == first) return "";
    size_t last = s.find_last_not_of(" \t\r\n");
    s = s.substr(first, (last - first + 1));

    // 2. Xóa dấu ngoặc kép nếu có
    s.erase(remove(s.begin(), s.end(), '\"'), s.end());

    // 3. Cắt các tham số sau dấu phẩy (thường gặp trong Registry)
    size_t commaPos = s.find(',');
    if (commaPos != string::npos) s = s.substr(0, commaPos);

    return s;
}

// Helper: Đọc giá trị String từ Registry (Phiên bản hỗ trợ Unicode)
string Process::GetRegString(HKEY hKey, const char* valueName) {
    // 1. Chuyển tên Value (ví dụ "DisplayName") từ char* sang WCHAR*
    int len = MultiByteToWideChar(CP_ACP, 0, valueName, -1, NULL, 0);
    if (len <= 0) return "";
    wstring wValueName(len, 0);
    MultiByteToWideChar(CP_ACP, 0, valueName, -1, &wValueName[0], len);

    // 2. Dùng RegQueryValueExW (API Unicode) thay vì bản A
    WCHAR buffer[4096]; // Buffer lớn để tránh tràn với tên App dài
    DWORD size = sizeof(buffer);
    DWORD type;

    if (RegQueryValueExW(hKey, wValueName.data(), NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
        // Chỉ xử lý dữ liệu dạng chuỗi
        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            // 3. Chuyển từ WCHAR (Unicode) sang UTF-8 để sử dụng trong C++
            return ConvertToString(buffer);
        }
    }
    return "";
}

// Helper: Lấy dung lượng RAM sử dụng của Process (MB)
double Process::GetProcessMemory(DWORD pid) {
    PROCESS_MEMORY_COUNTERS pmc;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) return 0.0;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        CloseHandle(hProcess);
        return (double)pmc.WorkingSetSize / (1024 * 1024);
    }
    CloseHandle(hProcess);
    return 0.0;
}

// =============================================================
// PHẦN 2: QUÉT REGISTRY & ỨNG DỤNG (SCANNING)
// =============================================================

// Quét key Uninstall chuẩn của Windows
void Process::ScanRegistryKey(HKEY hRoot, const char* subKey) {
    HKEY hUninstall;
    if (RegOpenKeyExA(hRoot, subKey, 0, KEY_READ, &hUninstall) != ERROR_SUCCESS) return;
    char keyName[256];
    DWORD index = 0;
    DWORD keyNameLen = 256;

    // Duyệt qua tất cả các subkey
    while (RegEnumKeyExA(hUninstall, index, keyName, &keyNameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        HKEY hApp;
        if (RegOpenKeyExA(hUninstall, keyName, 0, KEY_READ, &hApp) == ERROR_SUCCESS) {
            string name = GetRegString(hApp, "DisplayName");
            string iconPath = GetRegString(hApp, "DisplayIcon");
            if (iconPath.empty()) iconPath = GetRegString(hApp, "InstallLocation");

            if (!name.empty() && !iconPath.empty()) {
                string exePath = CleanPath(iconPath);
                // Chỉ lấy nếu là file .exe
                if (ToLower(exePath).find(".exe") != string::npos) {
                    m_installedApps.push_back({ name, exePath });
                }
            }
            RegCloseKey(hApp);
        }
        keyNameLen = 256; index++;
    }
    RegCloseKey(hUninstall);
}

// Quét key "App Paths" (Nơi đăng ký alias lệnh chạy)
void Process::ScanAppPaths(HKEY hRoot, const char* subKey) {
    HKEY hAppPaths;
    if (RegOpenKeyExA(hRoot, subKey, 0, KEY_READ, &hAppPaths) != ERROR_SUCCESS)
        return;

    char keyName[256];
    DWORD index = 0;

    while (true) {
        DWORD nameLen = sizeof(keyName);
        FILETIME ft{};
        LONG res = RegEnumKeyExA(
            hAppPaths,
            index,
            keyName,
            &nameLen,
            nullptr,
            nullptr,
            nullptr,
            &ft
        );

        if (res == ERROR_NO_MORE_ITEMS) break; // Hết danh sách
        if (res != ERROR_SUCCESS) {
            ++index; continue; // Skip lỗi
        }

        // Mở subkey (ví dụ "notepad.exe")
        HKEY hEntry;
        if (RegOpenKeyExA(hAppPaths, keyName, 0, KEY_READ, &hEntry) != ERROR_SUCCESS) {
            ++index; continue;
        }

        // Đọc giá trị Default (đường dẫn full)
        char  buffer[1024];
        DWORD size = sizeof(buffer);
        DWORD type = 0;
        std::string exePath;

        if (RegQueryValueExA(hEntry, nullptr, nullptr, &type,
            (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
            if (type == REG_SZ || type == REG_EXPAND_SZ) {
                exePath.assign(buffer);
            }
        }

        RegCloseKey(hEntry);

        if (exePath.empty()) { ++index; continue; }

        // Chuẩn hóa đường dẫn
        exePath = CleanPath(exePath);
        std::string lowerPath = ToLower(exePath);
        if (lowerPath.find(".exe") == std::string::npos) {
            ++index; continue;
        }

        // Lấy tên hiển thị từ tên Key (vd: "notepad.exe" -> "notepad")
        std::string displayName = keyName;
        std::string lowerKey = ToLower(displayName);
        if (lowerKey.size() > 4 &&
            lowerKey.substr(lowerKey.size() - 4) == ".exe") {
            displayName = displayName.substr(0, displayName.size() - 4);
        }

        // Kiểm tra trùng lặp với danh sách Uninstall
        bool exists = false;
        std::string normalizedNew = ToLower(CleanPath(exePath));
        for (const auto& app : m_installedApps) {
            std::string normalizedOld = ToLower(CleanPath(app.path));
            if (normalizedOld == normalizedNew) {
                exists = true; break;
            }
        }

        if (!exists) {
            m_installedApps.push_back({ displayName, exePath });
        }

        ++index;
    }

    RegCloseKey(hAppPaths);
}

// Hàm Wrapper: Tải toàn bộ danh sách App từ các nguồn
void Process::LoadInstalledApps() {
    m_installedApps.clear();

    // 1. App cài đặt tiêu chuẩn (Uninstall Keys)
    ScanRegistryKey(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    ScanRegistryKey(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    ScanRegistryKey(HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    // 2. App có đăng ký đường dẫn tắt (App Paths)
    ScanAppPaths(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths");
    ScanAppPaths(HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths");

    m_isListLoaded = true;
}

// =============================================================
// PHẦN 3: TÌM KIẾM ĐỆ QUY & START MENU (RECURSIVE SEARCH)
// =============================================================

// Tìm file đệ quy trong thư mục
string Process::FindFileRecursive(string directory, string fileToFind) {
    string searchPath = directory + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return "";

    string resultPath = "";
    string lowerFind = ToLower(fileToFind);

    do {
        string currentName = findData.cFileName;
        if (currentName == "." || currentName == "..") continue;

        string fullPath = directory + "\\" + currentName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Đệ quy vào thư mục con
            string found = FindFileRecursive(fullPath, fileToFind);
            if (!found.empty()) {
                resultPath = found;
                break;
            }
        }
        else {
            string lowerName = ToLower(currentName);
            // Logic: Tên file chứa từ khóa VÀ phải là .exe hoặc .lnk (shortcut)
            if (lowerName.find(lowerFind) != string::npos) {
                if (lowerName.find(".lnk") != string::npos || lowerName.find(".exe") != string::npos) {
                    resultPath = fullPath;
                    break;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return resultPath;
}

// Tìm ứng dụng trong các thư mục Start Menu/Desktop
string Process::FindAppInStartMenu(string appName) {
    char path[MAX_PATH];
    string foundPath = "";

    // Các vị trí phổ biến của Shortcut
    vector<int> foldersToCheck = {
        CSIDL_COMMON_PROGRAMS, // Start Menu (All Users)
        CSIDL_PROGRAMS,        // Start Menu (Current User)
        CSIDL_DESKTOPDIRECTORY // Desktop
    };

    for (int folderId : foldersToCheck) {
        if (SHGetFolderPathA(NULL, folderId, NULL, 0, path) == S_OK) {
            foundPath = FindFileRecursive(string(path), appName);
            if (!foundPath.empty()) return foundPath;
        }
    }
    return "";
}

// =============================================================
// PHẦN 4: LOGIC KHỞI CHẠY (CORE START LOGIC)
// =============================================================

bool Process::StartProcess(const string& nameOrPath) {
    if (nameOrPath.empty()) return false;
    string input = ToLower(nameOrPath);

    // Xử lý cắt đuôi .exe nếu có (ví dụ "discord.exe" -> "discord")
    string searchName = input;
    if (searchName.length() > 4 && searchName.substr(searchName.length() - 4) == ".exe") {
        searchName = searchName.substr(0, searchName.length() - 4);
    }

    // --- TRƯỜNG HỢP 1: Chạy trực tiếp (Direct Execution) ---
    // Ví dụ: "calc", "notepad", "C:\\Windows\\System32\\cmd.exe"
    HINSTANCE res = ShellExecuteA(NULL, "open", nameOrPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)res > 32) return true;

    // --- TRƯỜNG HỢP 2: Tìm trong Registry (Installed Apps) ---
    if (!m_isListLoaded) LoadInstalledApps();

    for (const auto& app : m_installedApps) {
        // So sánh tên hiển thị hoặc đường dẫn cài đặt
        if (ToLower(app.name).find(searchName) != string::npos || ToLower(app.path).find(searchName) != string::npos) {
            HINSTANCE res = ShellExecuteA(NULL, "open", app.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if ((intptr_t)res > 32) return true;
        }
    }

    // --- TRƯỜNG HỢP 3: Tìm đệ quy trong Start Menu (Fallback) ---
    // Các app User Mode (như Discord) thường nằm ở đây dưới dạng shortcut
    string shortcutPath = FindAppInStartMenu(searchName);
    if (!shortcutPath.empty()) {
        // Chạy file .lnk hoặc .exe tìm được
        HINSTANCE res = ShellExecuteA(NULL, "open", shortcutPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return ((intptr_t)res > 32);
    }

    return false;
}

// =============================================================
// PHẦN 5: DỪNG & LIỆT KÊ TIẾN TRÌNH (STOP & LIST)
// =============================================================

string Process::StopProcess(const string& nameOrPid) {
    // 1. Nếu input là số -> Dừng theo PID
    if (IsNumeric(nameOrPid)) {
        DWORD pid = stoul(nameOrPid);
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess && TerminateProcess(hProcess, 1)) {
            CloseHandle(hProcess);
            return "Killed PID " + nameOrPid;
        }
        return "Err: Cannot kill PID " + nameOrPid;
    }

    // 2. Nếu input là chữ -> Dừng theo tên (Kill tất cả process cùng tên)
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "Err Snapshot";

    PROCESSENTRY32W pe32; pe32.dwSize = sizeof(PROCESSENTRY32W);
    int count = 0;
    string target = ToLower(nameOrPid);

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            string exeName = ConvertToString(pe32.szExeFile);
            string lowerName = ToLower(exeName);
            if (lowerName == target || lowerName == target + ".exe") {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProc) { TerminateProcess(hProc, 1); CloseHandle(hProc); count++; }
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return count > 0 ? "Killed " + to_string(count) : "Not found";
}

string Process::ListProcesses() {
    stringstream ss;
    ss << "PID\tRAM(MB)\tThreads\tName\n";

    // Tạo Snapshot hệ thống
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return "Err Snapshot";

    PROCESSENTRY32W pe32; pe32.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(hSnap, &pe32)) {
        do {
            string exeName = ConvertToString(pe32.szExeFile);
            double ram = GetProcessMemory(pe32.th32ProcessID);
            ss << pe32.th32ProcessID << "\t" << fixed << setprecision(1) << ram << "\t" << pe32.cntThreads << "\t" << exeName << "\n";
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return ss.str();
}