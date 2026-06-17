// Structured key-value logger macro.
#pragma once

#include <iostream>
#include <sstream>
#include <string>

namespace Gaze {

inline void log_kv_impl(std::stringstream& ss) {
    ss << "\n";
    std::cout << ss.str() << std::flush;
}

template<typename K, typename V, typename... Args>
void log_kv_impl(std::stringstream& ss, const K& key, const V& val, const Args&... args) {
    ss << " " << key << "=" << val;
    log_kv_impl(ss, args...);
}

template<typename... Args>
void log_info(const std::string& event, const Args&... args) {
    std::stringstream ss;
    ss << "[INFO] event=" << event;
    log_kv_impl(ss, args...);
}

template<typename... Args>
void log_error(const std::string& event, const Args&... args) {
    std::stringstream ss;
    ss << "[ERROR] event=" << event;
    log_kv_impl(ss, args...);
}

} // namespace Gaze
