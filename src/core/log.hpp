/**
 * @file log.hpp
 * @brief Structured Key-Value Logger Utility
 *
 * Implements thread-safe structured logging with an atomic registry to
 * pipe logging events back to host environments (e.g. Godot engine log console
 * or browser console handlers).
 */
#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <atomic>

namespace Gaze {

// Raw callback signature for atomic registry: void(bool is_error, const char* msg)
using LogHandler = void(*)(bool, const char*);

inline std::atomic<LogHandler>& get_log_handler() {
    static std::atomic<LogHandler> handler{nullptr};
    return handler;
}

inline void register_log_handler(LogHandler handler) {
    get_log_handler().store(handler, std::memory_order_release);
}

inline void log_kv_impl(bool is_error, std::stringstream& ss) {
    std::string msg = ss.str();
    LogHandler handler = get_log_handler().load(std::memory_order_acquire);
    if (handler) {
        handler(is_error, msg.c_str());
    } else {
        std::ostream& os = is_error ? std::cerr : std::cout;
        os << msg << "\n" << std::flush;
    }
}

template<typename K, typename V, typename... Args>
void log_kv_impl(bool is_error, std::stringstream& ss, const K& key, const V& val, const Args&... args) {
    ss << " " << key << "=" << val;
    log_kv_impl(is_error, ss, args...);
}

template<typename... Args>
void log_info(const std::string& event, const Args&... args) {
    std::stringstream ss;
    ss << "[INFO] event=" << event;
    log_kv_impl(false, ss, args...);
}

template<typename... Args>
void log_warning(const std::string& event, const Args&... args) {
    std::stringstream ss;
    ss << "[WARNING] event=" << event;
    log_kv_impl(false, ss, args...);
}

template<typename... Args>
void log_error(const std::string& event, const Args&... args) {
    std::stringstream ss;
    ss << "[ERROR] event=" << event;
    log_kv_impl(true, ss, args...);
}

} // namespace Gaze
