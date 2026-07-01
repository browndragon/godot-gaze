#include "wmf_camera.hpp"
#include "../core/log.hpp"
#include <chrono>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <combaseapi.h>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

namespace Gaze {

WMFCamera::WMFCamera(int device) : device_id(device), start_time(0.0), pReader(nullptr), m_initialized(false) {}

WMFCamera::~WMFCamera() {
    release();
}

void WMFCamera::set_resolution(int w, int h) {
    target_width = w;
    target_height = h;
}

bool WMFCamera::initialize() {
    log_info("WMFCamera_InitAttempt", "device_id", device_id);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        log_error("WMFCamera_CoInitializeFailed", "hr", hr);
        return false;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        log_error("WMFCamera_MFStartupFailed", "hr", hr);
        return false;
    }

    IMFAttributes* pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
        log_error("WMFCamera_CreateAttributesFailed", "hr", hr);
        return false;
    }

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        pAttributes->Release();
        log_error("WMFCamera_SetGUIDFailed", "hr", hr);
        return false;
    }

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    pAttributes->Release();
    if (FAILED(hr)) {
        log_error("WMFCamera_EnumDevicesFailed", "hr", hr);
        return false;
    }

    if (count == 0 || device_id >= (int)count) {
        for (UINT32 i = 0; i < count; ++i) {
            ppDevices[i]->Release();
        }
        CoTaskMemFree(ppDevices);
        log_error("WMFCamera_DeviceNotFound", "device_id", device_id, "count", count);
        return false;
    }

    IMFMediaSource* pSource = nullptr;
    hr = ppDevices[device_id]->ActivateObject(IID_PPV_ARGS(&pSource));
    for (UINT32 i = 0; i < count; ++i) {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);

    if (FAILED(hr)) {
        log_error("WMFCamera_ActivateObjectFailed", "hr", hr);
        return false;
    }

    IMFSourceReader* reader = nullptr;
    hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &reader);
    pSource->Release();
    if (FAILED(hr)) {
        log_error("WMFCamera_CreateReaderFailed", "hr", hr);
        return false;
    }

    IMFMediaType* pType = nullptr;
    hr = MFCreateMediaType(&pType);
    if (FAILED(hr)) {
        reader->Release();
        log_error("WMFCamera_CreateMediaTypeFailed", "hr", hr);
        return false;
    }

    hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) {
        hr = pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
    }
    if (SUCCEEDED(hr)) {
        hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);
    }
    pType->Release();

    if (FAILED(hr)) {
        reader->Release();
        log_error("WMFCamera_SetMediaTypeFailed", "hr", hr);
        return false;
    }

    pReader = reader;
    m_initialized = true;

    auto now = std::chrono::steady_clock::now();
    start_time = std::chrono::duration<double>(now.time_since_epoch()).count();

    log_info("WMFCamera_InitSuccess", "device_id", device_id);
    return true;
}

bool WMFCamera::grab_frame(Frame& out_frame) {
    if (!m_initialized || !pReader) {
        return false;
    }

    IMFSourceReader* reader = static_cast<IMFSourceReader*>(pReader);
    DWORD streamIndex = 0;
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    IMFSample* pSample = nullptr;

    HRESULT hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);
    if (FAILED(hr) || !pSample) {
        return false;
    }

    IMFMediaBuffer* pBuffer = nullptr;
    hr = pSample->GetBufferByIndex(0, &pBuffer);
    if (SUCCEEDED(hr) && pBuffer) {
        BYTE* pData = nullptr;
        DWORD currentLen = 0;
        hr = pBuffer->Lock(&pData, nullptr, &currentLen);
        if (SUCCEEDED(hr) && pData) {
            IMFMediaType* pCurrentType = nullptr;
            hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
            UINT32 w = 0, h = 0;
            if (SUCCEEDED(hr) && pCurrentType) {
                MFGetAttributeSize(pCurrentType, MF_MT_FRAME_SIZE, &w, &h);
                pCurrentType->Release();
            }
            if (w == 0 || h == 0) {
                w = target_width;
                h = target_height;
            }

            int width = (int)w;
            int height = (int)h;
            int row_pitch = width * 3;
            int size = width * height * 3;

            frame_buffer.resize(size);

            for (int y = 0; y < height; ++y) {
                std::memcpy(frame_buffer.data() + (height - 1 - y) * row_pitch, pData + y * row_pitch, row_pitch);
            }

            pBuffer->Unlock();

            auto now = std::chrono::steady_clock::now();
            double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();

            out_frame.width = width;
            out_frame.height = height;
            out_frame.data = frame_buffer.data();
            out_frame.timestamp = current_time - start_time;

            pBuffer->Release();
            pSample->Release();
            return true;
        }
        if (pBuffer) pBuffer->Release();
    }
    pSample->Release();
    return false;
}

void WMFCamera::release() {
    if (m_initialized) {
        if (pReader) {
            IMFSourceReader* reader = static_cast<IMFSourceReader*>(pReader);
            reader->Release();
            pReader = nullptr;
        }
        MFShutdown();
        CoUninitialize();
        m_initialized = false;
        log_info("WMFCamera_Released", "device_id", device_id);
    }
}

} // namespace Gaze
