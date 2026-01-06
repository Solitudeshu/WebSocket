# WebSocket-based LAN Remote Administration System

![Project Status](https://img.shields.io/badge/Status-Completed-success)
![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B%20%7C%20Visual_Studio-purple)
![Frontend](https://img.shields.io/badge/Frontend-HTML5%20%2F%20JS-orange)

> **Đồ án môn học:** Mạng Máy Tính  
> **Trường:** Đại học Khoa học Tự nhiên, ĐHQG-HCM  
> **Khoa:** Công nghệ Thông tin  
> **Năm học:** 2025 - 2026

## 📖 Giới thiệu 

Dự án là một hệ thống **Remote Administration Tool (RAT)** hoạt động trong mạng nội bộ (LAN), cho phép người quản trị giám sát và điều khiển máy tính trạm thông qua giao diện Web. Hệ thống giải quyết bài toán truyền tải dữ liệu thời gian thực (Real-time) bằng giao thức **WebSocket**, khắc phục độ trễ của phương pháp HTTP Polling truyền thống.

### Cấu trúc dự án

Dự án bao gồm 3 thành phần:
1.  **Agent (Server):** Ứng dụng C++ chạy ngầm trên máy trạm.
2.  **Dashboard (Client):** Giao diện Web HTML/JS để điều khiển.
3.  **Discovery Service (Register):** Server trung gian hỗ trợ tìm kiếm thiết bị tự động.
---

## 👥 Thành viên thực hiện

| STT | MSSV | Họ và Tên |
| :-: | :--- | :--- | 
| 1 | **24120256** | **Hồ Ngọc Lan Anh** | 
| 2 | **24120498** | **Phan Minh Anh** | 
| 3 | **24120501** | **Nguyễn Lê Thanh Huy** | 

**Giáo viên hướng dẫn:** ThS. Đỗ Hoàng Cường

---

## 🛠️ Công nghệ & Kỹ thuật (Technical Stack)

Hệ thống được xây dựng dựa trên các công nghệ và thư viện kỹ thuật cao để đảm bảo hiệu năng và tính tương thích trên Windows.

### Backend (C++ Agent)
| Thành phần | Công nghệ / Thư viện | Mô tả kỹ thuật |
| :--- | :--- | :--- |
| **Network Core** | **Boost.Beast** & **Boost.Asio** | Xử lý kết nối WebSocket bất đồng bộ (Async I/O), đảm bảo hiệu năng cao khi xử lý đa luồng. |
| **Media** | **Microsoft Media Foundation** | Thay thế DirectShow/OpenCV để truy xuất Webcam ở mức thấp (Low-level), encode video sang H.264/MP4. |
| **System API** | **Windows API (Win32)** | Sử dụng `GDI` để chụp màn hình, `Toolhelp32Snapshot` để quản lý tiến trình/ứng dụng. |
| **Input Capture** | **Windows Hooks** (`SetWindowsHookEx`) | Bắt sự kiện bàn phím toàn cục, xử lý logic bộ gõ tiếng Việt (Telex/VNI) và Unicode. |
| **Audio** | **Microsoft SAPI** (`ISpVoice`) | Tích hợp Text-to-Speech để phát âm thanh thông báo trên máy trạm. |
| **Encryption** | **Crypt32** | Mã hóa dữ liệu nhị phân (Ảnh/Video/File) sang Base64 để truyền qua WebSocket Text Frame. |

### Frontend (Dashboard)
* **Core:** HTML5, CSS3 (Glassmorphism UI), Vanilla JavaScript.
* **Rendering:** Sử dụng HTML5 Canvas để vẽ luồng ảnh chụp màn hình và Video tag để phát stream webcam.
* **Connectivity:** WebSocket API chuẩn của trình duyệt.

---

## ✨ Tính năng 

Hệ thống cung cấp các công cụ quản trị mạnh mẽ đã được kiểm thử trong môi trường LAN:

* **🔍 Auto Discovery:** Tự động quét và hiển thị danh sách các máy đang hoạt động (Online) trong mạng.
* **📷 Webcam Streaming:** Xem hình ảnh trực tiếp từ webcam của máy trạm (sử dụng Media Foundation).
* **⌨️ Keylogger:** Ghi lại phím bấm, hỗ trợ đầy đủ tiếng Việt (Telex/VNI) và xử lý Unicode.
* **📸 Screenshot:** Chụp ảnh màn hình máy trạm và gửi về Dashboard theo thời gian thực.
* **📊 System Monitor:** Theo dõi thông tin hệ thống, danh sách tiến trình (Process) và ứng dụng (Application).
* **📁 File Explorer:** Duyệt cây thư mục, ổ đĩa và tải tệp tin (Download) từ máy trạm.
* **📋 Clipboard Manager:** Giám sát và lấy nội dung Clipboard (Text).
* **🗣️ Text-to-Speech:** Gửi văn bản từ Admin và phát âm thanh trên máy trạm.
* **⚙️ Power Control:** Điều khiển tắt máy (Shutdown) hoặc khởi động lại (Restart) từ xa.

---

## 🛠️ Yêu cầu hệ thống

* **IDE:** Visual Studio 2019/2022.
* **Thư viện:** Boost C++ (yêu cầu cấu hình đường dẫn Include/Library trong Project Settings).

---

## 🚀 Hướng dẫn cài đặt & Build

### 1. Build Server (Agent)
Đây là chương trình chạy ngầm trên máy cần điều khiển.

1.  Truy cập thư mục `Server/`.
2.  Mở file giải pháp **`Server.sln`** bằng Visual Studio.
3.  Đảm bảo cấu hình Build là **Release** và **x64**.
4.  Nhấn `Ctrl + Shift + B` để Build Solution.
5.  File thực thi `Server.exe` sẽ nằm trong thư mục `x64/Release/`.

### 2. Build Registry (Discovery Server)
Server trung gian để tìm kiếm IP.

1.  Truy cập thư mục `Register/`.
2.  Mở file giải pháp **`Register.sln`** bằng Visual Studio.
3.  Build tương tự như Server (chế độ Release/x64).
4.  Chạy `Register.exe` trên máy Admin.

### 3. Chạy Client (Dashboard)
Giao diện điều khiển không cần biên dịch.

1.  Truy cập thư mục `Client/`.
2.  Mở file **`index.html`** bằng trình duyệt web hiện đại (Chrome, Edge, Firefox).
3.  Nhấn **Scan Network** và chọn máy Server để kết nối.

---
