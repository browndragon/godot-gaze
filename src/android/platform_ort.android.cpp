#include "../native/platform_ort.hpp"
#include <onnxruntime_cxx_api.h>
#include "../core/log.hpp"

// Forward declaration of the NNAPI C-API function from ONNX Runtime.
// This is to avoid header search path issues across platform toolchains.
#ifdef __cplusplus
extern "C" {
#endif
ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_Nnapi, _In_ OrtSessionOptions* options, uint32_t nnapi_flags);
#ifdef __cplusplus
}
#endif

namespace Gaze {

std::unique_ptr<Ort::Session> platform_create_ort_session(
    Ort::Env& env,
    const std::string& path,
    Ort::SessionOptions* options
) {
    if (options) {
        uint32_t nnapi_flags = 0;
        OrtStatus* status = OrtSessionOptionsAppendExecutionProvider_Nnapi(*options, nnapi_flags);
        if (status != nullptr) {
            log_warning("AndroidNNAPIFailed", "reason", "Falling back to CPU");
            OrtGetApiBase()->GetApi(ORT_API_VERSION)->ReleaseStatus(status);
        }
    }
    return std::make_unique<Ort::Session>(env, path.c_str(), *options);
}

std::unique_ptr<Ort::Session> platform_create_ort_session(
    Ort::Env& env,
    const std::vector<uint8_t>& buffer,
    Ort::SessionOptions* options
) {
    if (options) {
        uint32_t nnapi_flags = 0;
        OrtStatus* status = OrtSessionOptionsAppendExecutionProvider_Nnapi(*options, nnapi_flags);
        if (status != nullptr) {
            log_warning("AndroidNNAPIFailed", "reason", "Falling back to CPU");
            OrtGetApiBase()->GetApi(ORT_API_VERSION)->ReleaseStatus(status);
        }
    }
    return std::make_unique<Ort::Session>(env, buffer.data(), buffer.size(), *options);
}

} // namespace Gaze
