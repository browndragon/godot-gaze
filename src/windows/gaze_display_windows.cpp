#include "gaze_display_windows.hpp"

#ifdef _WIN32
#include <windows.h>

namespace Gaze {

GazeDisplayMetrics gaze_windows_get_display_metrics(int screen_index) {
    GazeDisplayMetrics metrics;

    HDC hdc = GetDC(NULL);
    if (hdc) {
        metrics.width_mm = GetDeviceCaps(hdc, HORZSIZE);
        metrics.height_mm = GetDeviceCaps(hdc, VERTSIZE);
        metrics.pixel_width = GetDeviceCaps(hdc, HORZRES);
        metrics.pixel_height = GetDeviceCaps(hdc, VERTRES);
        
        int dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
        if (dpi_x > 0) {
            metrics.scale_factor = (double)dpi_x / 96.0;
        } else {
            metrics.scale_factor = 1.0;
        }
        int raw_w = GetDeviceCaps(hdc, HORZRES);
        int raw_h = GetDeviceCaps(hdc, VERTRES);
        metrics.pixel_width = (metrics.scale_factor > 0.0) ? (int)std::round(raw_w / metrics.scale_factor) : raw_w;
        metrics.pixel_height = (metrics.scale_factor > 0.0) ? (int)std::round(raw_h / metrics.scale_factor) : raw_h;
        ReleaseDC(NULL, hdc);
    }

    if (metrics.width_mm <= 0.0 || metrics.height_mm <= 0.0) {
        metrics.width_mm = metrics.pixel_width * 0.26458; // Standard 96 DPI fallback (0.26458 mm/px)
        metrics.height_mm = metrics.pixel_height * 0.26458;
    }

    return metrics;
}

GazeWindowRect gaze_windows_get_window_rect(int window_index) {
    GazeWindowRect rect;

    HWND hwnd = GetActiveWindow();
    if (!hwnd) {
        hwnd = GetForegroundWindow();
    }

    if (hwnd) {
        RECT rc;
        if (GetClientRect(hwnd, &rc)) {
            POINT pt = { rc.left, rc.top };
            ClientToScreen(hwnd, &pt);
            GazeDisplayMetrics metrics = gaze_windows_get_display_metrics(0);
            double s = (metrics.scale_factor > 0.0) ? metrics.scale_factor : 1.0;
            rect.x = (int)std::round(pt.x / s);
            rect.y = (int)std::round(pt.y / s);
            rect.width = (int)std::round((rc.right - rc.left) / s);
            rect.height = (int)std::round((rc.bottom - rc.top) / s);
        }
    }

    if (rect.width <= 0 || rect.height <= 0) {
        GazeDisplayMetrics metrics = gaze_windows_get_display_metrics(0);
        rect.x = 0;
        rect.y = 0;
        rect.width = metrics.pixel_width;
        rect.height = metrics.pixel_height;
    }

    return rect;
}

GazeMousePoint gaze_windows_get_mouse_position() {
    GazeMousePoint pt;
    POINT p;
    if (GetCursorPos(&p)) {
        GazeDisplayMetrics metrics = gaze_windows_get_display_metrics(0);
        double s = (metrics.scale_factor > 0.0) ? metrics.scale_factor : 1.0;
        pt.x = p.x / s;
        pt.y = p.y / s;
    }
    return pt;
}

int64_t gaze_windows_get_mouse_button_state() {
    int64_t mask = 0;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) mask |= 1;
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) mask |= 2;
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) mask |= 4;
    return mask;
}

} // namespace Gaze
#else
namespace Gaze {
GazeDisplayMetrics gaze_windows_get_display_metrics(int) { return {}; }
GazeWindowRect gaze_windows_get_window_rect(int) { return {}; }
GazeMousePoint gaze_windows_get_mouse_position() { return {}; }
int64_t gaze_windows_get_mouse_button_state() { return 0; }
} // namespace Gaze
#endif


