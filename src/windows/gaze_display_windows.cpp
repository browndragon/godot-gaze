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
            rect.x = pt.x;
            rect.y = pt.y;
            rect.width = rc.right - rc.left;
            rect.height = rc.bottom - rc.top;
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

} // namespace Gaze
#endif

