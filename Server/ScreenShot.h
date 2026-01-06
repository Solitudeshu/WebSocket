// =============================================================
// MODULE: SCREENSHOT CAPTURE (HEADER)
// Nhiệm vụ: Chụp ảnh màn hình (Screenshot) và trả về
//           dữ liệu pixel thô (Raw Bitmap).
// =============================================================

#pragma once

#include <vector>
#include <cstdint>

// Chụp toàn bộ màn hình chính (Primary Monitor)
// Trả về: Dữ liệu pixel định dạng BGRA 32-bit
// Kích thước buffer = width * height * 4 bytes.
//
// Sử dụng: GetSystemMetrics để lấy độ phân giải.
// Nếu lỗi -> Trả về vector rỗng.
std::vector<std::uint8_t> ScreenShot();