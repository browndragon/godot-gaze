#include "../native/platform_ort.hpp"
#include <windows.h>

namespace Gaze {

std::unique_ptr<Ort::Session> platform_create_ort_session(
    Ort::Env& env,
    const std::string& path,
    Ort::SessionOptions* options
) {
    if (path.empty()) {
        return nullptr;
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(), NULL, 0);
    std::wstring wpath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(), &wpath[0], size_needed);
    return std::make_unique<Ort::Session>(env, wpath.c_str(), *options);
}

std::unique_ptr<Ort::Session> platform_create_ort_session(
    Ort::Env& env,
    const std::vector<uint8_t>& buffer,
    Ort::SessionOptions* options
) {
    return std::make_unique<Ort::Session>(env, buffer.data(), buffer.size(), *options);
}

} // namespace Gaze
