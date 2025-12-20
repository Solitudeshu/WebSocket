#pragma once
#include <vector>
#include <string>

// Hàm quay video native dùng Media Foundation
// Trả về buffer chứa dữ liệu file MP4
std::vector<char> CaptureWebcam(int durationSeconds);