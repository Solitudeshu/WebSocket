#include "Webcam.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <shlwapi.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <string>

// Link thư viện hệ thống
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")

static const GUID MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING_FIX =
{ 0xfb394f3d, 0xccf1, 0x42ee, { 0xbb, 0xb3, 0xf9, 0xb8, 0x02, 0xd2, 0x26, 0x83 } };

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) { (*ppT)->Release(); *ppT = NULL; }
}

HRESULT ConfigureEncoder(IMFSinkWriter* pSinkWriter, DWORD* streamIndex, UINT32 width, UINT32 height, GUID inputFormat) {
    IMFMediaType* pMediaTypeOut = NULL;
    IMFMediaType* pMediaTypeIn = NULL;
    HRESULT hr;

    // 1. Output Type (H.264 MP4)
    hr = MFCreateMediaType(&pMediaTypeOut);
    if (SUCCEEDED(hr)) hr = pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);

    // Bitrate 1Mbps là đủ nét cho Webcam
    if (SUCCEEDED(hr)) hr = pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, 1000000);

    if (SUCCEEDED(hr)) hr = pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (SUCCEEDED(hr)) hr = MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, width, height);
    if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, 30, 1);
    if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (SUCCEEDED(hr)) hr = pSinkWriter->AddStream(pMediaTypeOut, streamIndex);

    // 2. Input Type
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&pMediaTypeIn);
    if (SUCCEEDED(hr)) hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, inputFormat);
    if (SUCCEEDED(hr)) hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, width, height);
    if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, 30, 1);
    if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (SUCCEEDED(hr)) hr = pSinkWriter->SetInputMediaType(*streamIndex, pMediaTypeIn, NULL);

    SafeRelease(&pMediaTypeOut);
    SafeRelease(&pMediaTypeIn);
    return hr;
}

std::vector<char> CaptureWebcam(int durationSeconds) {
    std::vector<char> fileData;
    const wchar_t* filename = L"temp_webcam_rec.mp4";

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

    // Biến định dạng
    bool formatSet = false;
    GUID tryFormats[] = { MFVideoFormat_NV12, MFVideoFormat_YUY2 };
    GUID actualFormat = MFVideoFormat_NV12;
    UINT32 actualWidth = 640;
    UINT32 actualHeight = 480;

    // --- LOGIC THỜI GIAN "SMART START" ---
    LONGLONG startTick = 0; // Chưa bắt đầu tính giờ
    LONGLONG lastStamp = -1;

    // Giới hạn 60s để an toàn bộ nhớ (Có thể tăng nếu muốn)
    if (durationSeconds > 60) durationSeconds = 60;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = MFStartup(MF_VERSION);

    std::cout << "[WEBCAM] Scanning hardware...\n";

    // 1. TÌM CAMERA
    hr = MFCreateAttributes(&pConfig, 1);
    if (SUCCEEDED(hr)) hr = pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr)) hr = MFEnumDeviceSources(pConfig, &ppDevices, &count);

    if (FAILED(hr) || count == 0) {
        std::cerr << "[WEBCAM] Error: No camera found.\n";
        goto CleanUp;
    }

    // 2. KÍCH HOẠT CAMERA
    hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
    SafeRelease(&pConfig);

    // 3. TẠO READER
    hr = MFCreateAttributes(&pConfig, 1);
    if (SUCCEEDED(hr)) hr = pConfig->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING_FIX, TRUE);
    hr = MFCreateSourceReaderFromMediaSource(pSource, pConfig, &pReader);
    if (FAILED(hr)) { std::cerr << "[WEBCAM] Error creating SourceReader.\n"; goto CleanUp; }

    // 4. THỎA HIỆP ĐỊNH DẠNG
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
    if (!formatSet) {
        std::cout << "[WEBCAM] Using camera default format (Fallback).\n";
        if (SUCCEEDED(pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pActualType))) {
            pActualType->GetGUID(MF_MT_SUBTYPE, &actualFormat);
            SafeRelease(&pActualType);
        }
    }
    SafeRelease(&pType);

    // 5. LẤY ĐỘ PHÂN GIẢI THỰC
    hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pActualType);
    if (SUCCEEDED(hr)) {
        MFGetAttributeSize(pActualType, MF_MT_FRAME_SIZE, &actualWidth, &actualHeight);
        SafeRelease(&pActualType);
        std::cout << "[WEBCAM] Resolution locked: " << actualWidth << "x" << actualHeight << "\n";
    }

    // 6. KHỞI TẠO BỘ GHI
    DeleteFile(filename);
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

    // 7. VÒNG LẶP GHI HÌNH
    {
        int framesCaptured = 0;
        int maxRetries = 2000; // Chờ tối đa 10s cho camera bật

        while (true) {
            // Kiểm tra thời gian dừng (Chỉ kiểm tra nếu đã bắt đầu quay)
            if (startTick != 0) {
                LONGLONG currentTick = GetTickCount64();
                // [FIX] Thêm 500ms buffer để chắc chắn video đủ dài (10.5s -> Player hiển thị 10s)
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
                // --- [SMART START] ---
                // Chỉ bắt đầu tính giờ khi nhận được Frame đầu tiên
                if (startTick == 0) {
                    startTick = GetTickCount64();
                    std::cout << "[WEBCAM] First frame received. Timer started.\n";
                }

                // Tính timestamp dựa trên thời gian thực
                LONGLONG realTimeStamp = (GetTickCount64() - startTick) * 10000;

                // Đảm bảo timestamp luôn tăng
                if (realTimeStamp <= lastStamp) realTimeStamp = lastStamp + 1;
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
                if (framesCaptured % 30 == 0) std::cout << ".";
            }
            else {
                // Camera đang khởi động
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                // Chỉ trừ retry nếu chưa nhận được frame nào
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

    // 8. ĐỌC FILE VÀO RAM
    if (recordingSuccess) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::ifstream file("temp_webcam_rec.mp4", std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            if (size > 0) {
                // Giới hạn 50MB (an toàn cho WebSocket)
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
            DeleteFile(filename);
        }
    }

CleanUp:
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