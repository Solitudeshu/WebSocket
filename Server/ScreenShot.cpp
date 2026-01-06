// =============================================================
// MODULE: SCREENSHOT CAPTURE (SOURCE)
// =============================================================

#include "ScreenShot.h"

#include <Windows.h>
#include <vector>
#include <cstdint>

// --- LIÊN KẾT THƯ VIỆN ĐỒ HỌA (GDI & USER32) ---
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")

std::vector<std::uint8_t> ScreenShot()
{
    std::vector<std::uint8_t> buffer;

    // 1. Lấy kích thước màn hình hiện tại (Primary Monitor)
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    if (width <= 0 || height <= 0)
        return buffer;

    // 2. Lấy Handle Device Context (HDC) của màn hình
    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC)
        return buffer;

    // 3. Tạo HDC ảo trong bộ nhớ (Memory DC) để vẽ ảnh vào
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) {
        ReleaseDC(NULL, hScreenDC);
        return buffer;
    }

    // 4. Tạo Bitmap tương thích với độ phân giải màn hình
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    if (!hBitmap) {
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreenDC);
        return buffer;
    }

    // 5. Gán (Select) Bitmap vào Memory DC để chuẩn bị vẽ
    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemDC, hBitmap));

    // 6. Copy dữ liệu pixel từ màn hình thật vào Memory DC (Bit-Block Transfer)
    // SRCCOPY: Copy nguyên bản, CAPTUREBLT: Bao gồm cả các cửa sổ trong suốt (Layered Windows)
    BOOL ok = BitBlt(
        hMemDC,
        0, 0, width, height,
        hScreenDC,
        0, 0,
        SRCCOPY | CAPTUREBLT
    );

    // Giải phóng HDC màn hình (không cần nữa)
    ReleaseDC(NULL, hScreenDC);

    if (!ok) {
        // Nếu lỗi, dọn dẹp và trả về rỗng
        SelectObject(hMemDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemDC);
        buffer.clear();
        return buffer;
    }

    // 7. Chuẩn bị cấu trúc BITMAPINFO để lấy dữ liệu dạng mảng byte
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height; // Đặt âm để ảnh không bị lộn ngược (Top-Down)
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;    // 32-bit BGRA (Blue-Green-Red-Alpha)
    bi.bmiHeader.biCompression = BI_RGB;

    // 8. Cấp phát buffer: width * height * 4 bytes (mỗi pixel 4 byte)
    buffer.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    // 9. Trích xuất dữ liệu pixel từ Bitmap vào buffer
    int scanLines = GetDIBits(
        hMemDC,
        hBitmap,
        0,
        static_cast<UINT>(height),
        buffer.data(),
        &bi,
        DIB_RGB_COLORS
    );

    // 10. Dọn dẹp tài nguyên GDI (tránh Memory Leak)
    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);

    if (scanLines == 0) {
        buffer.clear(); // Lỗi khi GetDIBits
    }

    return buffer;
}