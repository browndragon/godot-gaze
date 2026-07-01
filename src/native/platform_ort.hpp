#pragma once
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>

namespace Gaze {
    // Factory function to create an ONNX Runtime Session from a model path,
    // handling platform-specific path conversions (e.g. wstring on Windows) cleanly.
    std::unique_ptr<Ort::Session> platform_create_ort_session(
        Ort::Env& env,
        const std::string& path,
        Ort::SessionOptions* options
    );

    // Factory function to create an ONNX Runtime Session from an in-memory buffer.
    std::unique_ptr<Ort::Session> platform_create_ort_session(
        Ort::Env& env,
        const std::vector<uint8_t>& buffer,
        Ort::SessionOptions* options
    );
}
