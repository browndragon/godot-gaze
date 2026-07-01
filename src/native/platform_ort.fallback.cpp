#include "platform_ort.hpp"

namespace Gaze {

std::unique_ptr<Ort::Session> platform_create_ort_session(
    Ort::Env& env,
    const std::string& path,
    Ort::SessionOptions* options
) {
    return std::make_unique<Ort::Session>(env, path.c_str(), *options);
}

std::unique_ptr<Ort::Session> platform_create_ort_session(
    Ort::Env& env,
    const std::vector<uint8_t>& buffer,
    Ort::SessionOptions* options
) {
    return std::make_unique<Ort::Session>(env, buffer.data(), buffer.size(), *options);
}

} // namespace Gaze
