# AuraLink - Remote Administration Tool (RAT)

![Project Status](https://img.shields.io/badge/Status-Completed-success)
![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B%20%7C%20Visual_Studio-purple)
![Frontend](https://img.shields.io/badge/Frontend-HTML5%20%2F%20JS-orange)

> **Đồ án môn học:** Mạng Máy Tính  
> **Trường:** Đại học Khoa học Tự nhiên, ĐHQG-HCM  
> **Khoa:** Công nghệ Thông tin  
> **Năm học:** 2025 - 2026

## 📖 Giới thiệu (Overview)

**AuraLink** là hệ thống điều khiển và giám sát máy tính từ xa trong mạng nội bộ (LAN), được xây dựng dựa trên giao thức **WebSocket** để đảm bảo tốc độ truyền tải thời gian thực (Real-time). Hệ thống sử dụng mô hình Client-Server kết hợp với cơ chế tự động phát hiện thiết bị (Service Discovery) qua UDP.

### Cấu trúc dự án
Dựa trên mã nguồn hiện tại, dự án được chia thành 3 thành phần chính:

1.  **`Server/` (Agent):** Chương trình chạy trên máy nạn nhân (Target). Được viết bằng C++ sử dụng **Visual Studio**, chịu trách nhiệm thực thi lệnh, quay màn hình, keylog và gửi dữ liệu về Dashboard.
2.  **`Client/` (Dashboard):** Giao diện điều khiển chạy trên trình duyệt Web (HTML/CSS/JS). Kết nối trực tiếp tới Agent hoặc thông qua Registry Server.
3.  **`Register/` (Discovery Server):** Server trung gian viết bằng C++, giúp Admin tự động tìm kiếm IP của các máy đang chạy Agent trong mạng LAN.

---

## ✨ Tính năng (Features)

Hệ thống cung cấp các công cụ quản trị mạnh mẽ đã được kiểm thử trong môi trường LAN:

* **🔍 Auto Discovery:** Tự động quét và hiển thị danh sách các máy đang hoạt động (Online) trong mạng.
* **📷 Webcam Streaming:** Xem hình ảnh trực tiếp từ webcam của máy trạm (sử dụng Media Foundation).
* **⌨️ Keylogger:** Ghi lại phím bấm, hỗ trợ đầy đủ tiếng Việt (Telex/VNI) và xử lý Unicode.
* **📸 Screenshot:** Chụp ảnh màn hình máy trạm và gửi về Dashboard theo thời gian thực.
* **📊 System Monitor:** Theo dõi thông tin hệ thống, danh sách tiến trình (Process) và ứng dụng (Application).
* **📁 File Explorer:** Duyệt cây thư mục, ổ đĩa và tải tệp tin (Download) từ máy trạm.
* **📋 Clipboard Manager:** Giám sát và lấy nội dung Clipboard (Text).
* **🗣️ Text-to-Speech:** Gửi văn bản từ Admin và phát âm thanh (chị Google) trên máy trạm.
* **⚙️ Power Control:** Điều khiển tắt máy (Shutdown) hoặc khởi động lại (Restart) từ xa.

---

## 🛠️ Yêu cầu hệ thống (Prerequisites)

Để biên dịch và chạy dự án, bạn cần chuẩn bị:

* **Hệ điều hành:** Windows 10 hoặc Windows 11.
* **IDE:** Visual Studio 2019 hoặc 2022 (có cài đặt workload "Desktop development with C++").
* **Thư viện:** [Boost C++ Libraries](https://www.boost.org/) (Phiên bản mới nhất).
    * *Lưu ý:* Cần cấu hình đường dẫn `Include Directories` và `Library Directories` tới thư mục Boost trong Project Properties của Visual Studio.

---

## 🚀 Hướng dẫn cài đặt & Build (Installation)

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
4.  Chạy `Register.exe` trên máy Admin (hoặc một máy chủ trong mạng).

### 3. Chạy Client (Dashboard)
Giao diện điều khiển không cần biên dịch.

1.  Truy cập thư mục `Client/`.
2.  Mở file **`index.html`** bằng trình duyệt web hiện đại (Chrome, Edge, Firefox).
3.  Nhập IP của máy Agent (hoặc dùng tính năng **Scan Network** nếu đã chạy Register) để kết nối.

---

## 👥 Thành viên thực hiện (Contributors)

| STT | MSSV | Họ và Tên | Vai trò chính |
| :-: | :--- | :--- | :--- |
| 1 | **24120256** | **Hồ Ngọc Lan Anh** | Frontend Dev, UI/UX, Báo cáo |
| 2 | **24120498** | **Phan Minh Anh** | Backend Dev (Core, Keylogger), Network |
| 3 | **24120501** | **Nguyễn Lê Thanh Huy** | Backend Dev (System, Media), Discovery |

**Giáo viên hướng dẫn:** ThS. Đỗ Hoàng Cường

---

## ⚠️ Khước từ trách nhiệm (Disclaimer)

> Dự án này được phát triển **dành riêng cho mục đích học tập và nghiên cứu** trong khuôn khổ môn học Mạng Máy Tính tại trường ĐH Khoa học Tự nhiên. Nhóm tác giả không chịu trách nhiệm cho bất kỳ hành vi sử dụng mã nguồn này vào mục đích xâm phạm quyền riêng tư hoặc vi phạm pháp luật. Vui lòng chỉ thử nghiệm trên các thiết bị mà bạn sở hữu hoặc được sự cho phép.

---
© 2026 AuraLink Project.
