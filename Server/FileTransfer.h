// =============================================================
// MODULE: FILE TRANSFER (HEADER)
// Nhiệm vụ: Xử lý các yêu cầu liên quan đến tệp tin:
//           - Tải file (Download) về máy Client
//           - Liệt kê danh sách file/thư mục (Directory Listing)
// =============================================================

#pragma once
#include <string>

class FileTransfer {
public:
    // Xử lý yêu cầu tải file: 
    // Đọc file nhị phân -> Mã hóa Base64 -> Trả về chuỗi protocol để gửi qua Socket
    static std::string HandleDownloadRequest(const std::string& filepath);

    // Liệt kê danh sách file và thư mục:
    // Trả về chuỗi định dạng đặc biệt để Client phân tích và hiển thị cây thư mục
    static std::string ListDirectory(const std::string& path);
};