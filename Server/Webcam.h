// =============================================================
// MODULE: WEBCAM CAPTURE (HEADER)
// Nhiệm vụ: Quay video từ Camera và trả về dữ liệu file MP4
// =============================================================

#pragma once
#include <vector>

// Hàm quay video (Blocking function - Chạy trong thread riêng)
// durationSeconds: Thời lượng quay (giây)
// Return: Vector chứa bytes của file .mp4
std::vector<char> CaptureWebcam(int durationSeconds);