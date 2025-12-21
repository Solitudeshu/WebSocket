// =============================================================
// MODULE: WEBCAM CAPTURE (SOURCE)
// Sử dụng: Microsoft Media Foundation (MF)
// =============================================================

#include "Webcam.h"

// Thư viện hệ thống & MF
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// Thư viện C++ chuẩn
#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <vector>

// --- LIÊN KẾT THƯ VIỆN (LINKER) ---
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// GUID sửa lỗi xử lý video cho Source Reader
static const GUID MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING_FIX =
{ 0xfb394f3d, 0xccf1, 0x42ee, { 0xbb, 0xb3, 0xf9, 0xb8, 0x02, 0xd2, 0x26, 0x83 } };

// Hàm tiện ích giải phóng đối tượng COM an toàn
template <class T> void SafeRelease(T** ppT) {
    if (*ppT) { (*ppT)->Release(); *ppT = NULL; }
}

// =============================================================
// PHẦN 1: CẤU HÌNH ENCODER (H.264)
// =============================================================

HRESULT ConfigureEncoder(IMFSinkWriter* pSinkWriter, DWORD* streamIndex, UINT32 width, UINT32 height, GUID inputFormat) {
    IMFMediaType* pMediaTypeOut = NULL;
    IMFMediaType* pMediaTypeIn = NULL;
    HRESULT hr;

    // 1. Thiết lập định dạng đầu ra (Output Type): H.264 MP4
    hr = MFCreateMediaType(&pMediaTypeOut);
    if (SUCCEEDED(hr)) hr = pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);

    // Bitrate 1Mbps (Đủ nét cho Web, nhẹ đường truyền)
    if (SUCCEEDED(hr)) hr = pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, 1000000);
    if (SUCCEEDED(hr)) hr = pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (SUCCEEDED(hr)) hr = MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, width, height);
    if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, 30, 1);
    if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    // Thêm stream vào Sink Writer
    if (SUCCEEDED(hr)) hr = pSinkWriter->AddStream(pMediaTypeOut, streamIndex);

    // 2. Thiết lập định dạng đầu vào (Input Type) - Phải khớp với Camera
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&pMediaTypeIn);
    if (SUCCEEDED(hr)) hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, inputFormat);
    if (SUCCEEDED(hr)) hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, width, height);
    if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, 30, 1);
    if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    // Kết nối Input với Output
    if (SUCCEEDED(hr)) hr = pSinkWriter->SetInputMediaType(*streamIndex, pMediaTypeIn, NULL);

    SafeRelease(&pMediaTypeOut);
    SafeRelease(&pMediaTypeIn);
    return hr;
}

// =============================================================
// PHẦN 2: LOGIC QUAY PHIM CHÍNH
// =============================================================

std::vector<char> CaptureWebcam(int durationSeconds) {
    std::vector<char> fileData;
    const wchar_t* filename = L"temp_webcam_rec.mp4"; // File tạm

    // Các biến giao diện COM
    IMFAttributes* pConfig = NULL;
    IMFActivate** ppDevices = NULL;
    IMFMediaSource* pSource = NULL;
    IMFSourceReader* pReader = NULL;
    IMFSinkWriter* pWriter = NULL;
    IMFMediaType* pType = NULL;
    IMFMediaType* pActualType = NULL;
    IMFSample* pSample = NULL;

    UINT32 count = 0;
    DWORD streamIndex = 0;
    HRESULT hr = S_OK;
    bool recordingSuccess = false;

    // Cấu hình định dạng ưu tiên (NV12 hoặc YUY2 phổ biến trên Webcam)
    bool formatSet = false;
    GUID tryFormats[] = { MFVideoFormat_NV12, MFVideoFormat_YUY2 };
    GUID actualFormat = MFVideoFormat_NV12;
    UINT32 actualWidth = 640;
    UINT32 actualHeight = 480;

    // Logic "Smart Start" (Chỉ tính giờ khi có hình)
    LONGLONG startTick = 0;
    LONGLONG lastStamp = -1;

    // Giới hạn tối đa 60s để tránh file quá lớn gây lỗi RAM/Socket
    if (durationSeconds > 60) durationSeconds = 60;

    // Khởi tạo Media Foundation
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = MFStartup(MF_VERSION);

    std::cout << "[WEBCAM] Scanning hardware...\n";

    // --- BƯỚC 1: TÌM THIẾT BỊ CAMERA ---
    hr = MFCreateAttributes(&pConfig, 1);
    if (SUCCEEDED(hr)) hr = pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr)) hr = MFEnumDeviceSources(pConfig, &ppDevices, &count);

    if (FAILED(hr) || count == 0) {
        std::cerr << "[WEBCAM] Error: No camera found.\n";
        goto CleanUp;
    }

    // --- BƯỚC 2: KÍCH HOẠT CAMERA & TẠO READER ---
    hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
    SafeRelease(&pConfig);

    hr = MFCreateAttributes(&pConfig, 1);
    if (SUCCEEDED(hr)) hr = pConfig->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING_FIX, TRUE);

    hr = MFCreateSourceReaderFromMediaSource(pSource, pConfig, &pReader);
    if (FAILED(hr)) { std::cerr << "[WEBCAM] Error creating SourceReader.\n"; goto CleanUp; }

    // --- BƯỚC 3: THỎA HIỆP ĐỊNH DẠNG (FORMAT NEGOTIATION) ---
    for (const auto& fmt : tryFormats) {
        SafeRelease(&pType);
        MFCreateMediaType(&pType);
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pType->SetGUID(MF_MT_SUBTYPE, fmt);

        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
        if (SUCCEEDED(hr)) {
            actualFormat = fmt;
            formatSet = true;
            std::cout << "[WEBCAM] Camera accepted format: " << (fmt == MFVideoFormat_NV12 ? "NV12" : "YUY2") << "\n";
            break;
        }
    }

    // Nếu không set được, dùng định dạng mặc định của Camera (Fallback)
    if (!formatSet) {
        std::cout << "[WEBCAM] Using camera default format (Fallback).\n";
        if (SUCCEEDED(pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pActualType))) {
            pActualType->GetGUID(MF_MT_SUBTYPE, &actualFormat);
            SafeRelease(&pActualType);
        }
    }
    SafeRelease(&pType);

    // Lấy độ phân giải thực tế
    hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pActualType);
    if (SUCCEEDED(hr)) {
        MFGetAttributeSize(pActualType, MF_MT_FRAME_SIZE, &actualWidth, &actualHeight);
        SafeRelease(&pActualType);
        std::cout << "[WEBCAM] Resolution locked: " << actualWidth << "x" << actualHeight << "\n";
    }

    // --- BƯỚC 4: KHỞI TẠO FILE GHI (SINK WRITER) ---
    DeleteFile(filename); // Xóa file cũ nếu có
    hr = MFCreateSinkWriterFromURL(filename, NULL, NULL, &pWriter);
    if (SUCCEEDED(hr)) {
        hr = ConfigureEncoder(pWriter, &streamIndex, actualWidth, actualHeight, actualFormat);
    }
    if (SUCCEEDED(hr)) hr = pWriter->BeginWriting();

    if (FAILED(hr)) {
        std::cerr << "[WEBCAM] Error: Failed to init MP4 Writer. HR=" << std::hex << hr << "\n";
        goto CleanUp;
    }

    std::cout << "[WEBCAM] Recording " << durationSeconds << "s (Smart Start Logic)...\n";

    // --- BƯỚC 5: VÒNG LẶP GHI HÌNH (RECORDING LOOP) ---
    {
        int framesCaptured = 0;
        int maxRetries = 2000; // Timeout chờ camera khởi động (khoảng 10s)

        while (true) {
            // Kiểm tra thời gian dừng (Chỉ tính khi đã bắt đầu nhận frame)
            if (startTick != 0) {
                LONGLONG currentTick = GetTickCount64();
                // Thêm 500ms buffer để chắc chắn video đủ dài
                if ((currentTick - startTick) > (durationSeconds * 1000 + 500)) {
                    break;
                }
            }

            DWORD streamIndexReader, flags;
            LONGLONG llTimeStamp;

            hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, NULL, &pSample);

            if (FAILED(hr)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;

            if (pSample) {
                // [SMART START]: Frame đầu tiên đến -> Bắt đầu tính giờ
                if (startTick == 0) {
                    startTick = GetTickCount64();
                    std::cout << "[WEBCAM] First frame received. Timer started.\n";
                }

                // Tính timestamp thủ công để video mượt
                LONGLONG realTimeStamp = (GetTickCount64() - startTick) * 10000;
                if (realTimeStamp <= lastStamp) realTimeStamp = lastStamp + 1; // Monotonic check
                lastStamp = realTimeStamp;

                pSample->SetSampleTime(realTimeStamp);
                pSample->SetSampleDuration(333333); // ~30 FPS

                hr = pWriter->WriteSample(streamIndex, pSample);
                SafeRelease(&pSample);

                if (FAILED(hr)) {
                    std::cerr << "[!] Write Error: " << std::hex << hr << "\n";
                    break;
                }
                framesCaptured++;
                if (framesCaptured % 30 == 0) std::cout << "."; // Visual progress
            }
            else {
                // Camera chưa sẵn sàng
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                if (startTick == 0) {
                    maxRetries--;
                    if (maxRetries <= 0) {
                        std::cerr << "[WEBCAM] Timeout waiting for camera startup.\n";
                        break;
                    }
                }
            }
        }
        std::cout << "\n[WEBCAM] Finished. Captured: " << framesCaptured << " frames.\n";

        if (framesCaptured > 10) recordingSuccess = true;
    }

    if (pWriter) pWriter->Finalize();

    // --- BƯỚC 6: ĐỌC FILE VÀO RAM VÀ DỌN DẸP ---
    if (recordingSuccess) {
        // Chờ file nhả khóa
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::ifstream file("temp_webcam_rec.mp4", std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            if (size > 0) {
                // Giới hạn 50MB để tránh tràn bộ nhớ
                if (size > 50 * 1024 * 1024) {
                    std::cerr << "[WEBCAM] File too large (" << size / 1024 / 1024 << " MB). Skip sending.\n";
                }
                else {
                    file.seekg(0, std::ios::beg);
                    fileData.resize((size_t)size);
                    file.read(fileData.data(), size);
                }
            }
            file.close();
            // Xóa file tạm sau khi đọc xong
            DeleteFile(filename);
        }
    }

CleanUp:
    // Giải phóng tài nguyên COM
    SafeRelease(&pWriter);
    SafeRelease(&pReader);
    SafeRelease(&pSource);
    SafeRelease(&pConfig);
    SafeRelease(&pType);
    SafeRelease(&pActualType);
    SafeRelease(&pSample);

    for (UINT32 i = 0; i < count; i++) SafeRelease(&ppDevices[i]);
    CoTaskMemFree(ppDevices);

    MFShutdown();
    CoUninitialize();

    return fileData;
}