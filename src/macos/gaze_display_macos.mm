#include "gaze_display_macos.hpp"

#import <CoreGraphics/CoreGraphics.h>
#import <AppKit/AppKit.h>
#include <vector>
#include <cmath>

namespace Gaze {


GazeDisplayMetrics gaze_macos_get_display_metrics(int screen_index) {
    GazeDisplayMetrics metrics;

    uint32_t max_displays = 16;
    CGDirectDisplayID active_displays[16];
    uint32_t display_count = 0;
    CGGetActiveDisplayList(max_displays, active_displays, &display_count);

    CGDirectDisplayID target_display = CGMainDisplayID();
    if (display_count > 0 && screen_index >= 0 && screen_index < (int)display_count) {
        target_display = active_displays[screen_index];
    }

    CGSize physical_size = CGDisplayScreenSize(target_display);
    metrics.width_mm = physical_size.width;
    metrics.height_mm = physical_size.height;

    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(target_display);
    if (mode) {
        size_t pixel_w = CGDisplayModeGetPixelWidth(mode);
        size_t pixel_h = CGDisplayModeGetPixelHeight(mode);
        size_t point_w = CGDisplayModeGetWidth(mode);
        if (point_w > 0 && pixel_w > 0) {
            metrics.scale_factor = (double)pixel_w / (double)point_w;
        } else {
            metrics.scale_factor = 1.0;
        }
        metrics.pixel_width = (int)pixel_w;
        metrics.pixel_height = (int)pixel_h;
        CGDisplayModeRelease(mode);
    } else {
        metrics.pixel_width = (int)CGDisplayPixelsWide(target_display);
        metrics.pixel_height = (int)CGDisplayPixelsHigh(target_display);
        metrics.scale_factor = 1.0;
    }

    // Fallback if physical millimeters is 0 (e.g. headless/virtual displays)
    if (metrics.width_mm <= 0.0 || metrics.height_mm <= 0.0) {
        // Default to typical MacBook Retina pitch (~0.20 mm/pt)
        double pt_pitch = 0.20;
        metrics.width_mm = (metrics.pixel_width / metrics.scale_factor) * pt_pitch;
        metrics.height_mm = (metrics.pixel_height / metrics.scale_factor) * pt_pitch;
    }

    return metrics;
}

GazeWindowRect gaze_macos_get_window_rect(int window_index) {
    GazeWindowRect rect;

    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        NSArray<NSWindow *> *windows = [app windows];
        NSWindow *window = nil;

        if (window_index >= 0 && window_index < (int)[windows count]) {
            NSWindow *w = [windows objectAtIndex:window_index];
            if ([w isVisible]) {
                window = w;
            }
        }
        if (!window && [app keyWindow] && [[app keyWindow] isVisible]) {
            window = [app keyWindow];
        }
        if (!window && [app mainWindow] && [[app mainWindow] isVisible]) {
            window = [app mainWindow];
        }
        if (!window) {
            for (NSWindow *w in windows) {
                if ([w isVisible] && ([w styleMask] & NSWindowStyleMaskTitled)) {
                    window = w;
                    break;
                }
            }
        }


        if (window) {
            NSScreen *screen = [window screen];
            if (!screen) {
                screen = [NSScreen mainScreen];
            }
            double scale = screen ? [screen backingScaleFactor] : 1.0;
            if (scale <= 0.0) scale = 1.0;

            // Client content rectangle in Cocoa coordinates (bottom-left origin)
            NSRect content_rect = [window contentRectForFrameRect:[window frame]];
            NSRect screen_rect = screen ? [screen frame] : NSMakeRect(0, 0, 1512, 982);

            // Convert Cocoa bottom-left origin to Top-Left display origin, scaled to match Godot window pixels
            rect.x = (int)std::round((content_rect.origin.x - screen_rect.origin.x) * scale);
            rect.y = (int)std::round((screen_rect.origin.y + screen_rect.size.height - (content_rect.origin.y + content_rect.size.height)) * scale);
            rect.width = (int)std::round(content_rect.size.width * scale);
            rect.height = (int)std::round(content_rect.size.height * scale);
        } else {
            // Fallback to primary screen metrics if no window is active yet
            GazeDisplayMetrics metrics = gaze_macos_get_display_metrics(0);
            rect.x = 0;
            rect.y = 0;
            rect.width = metrics.pixel_width;
            rect.height = metrics.pixel_height;
        }
    }

    return rect;
}

GazeMousePoint gaze_macos_get_mouse_position() {
    GazeMousePoint pt;
    @autoreleasepool {
        NSPoint mouse_loc = [NSEvent mouseLocation];
        NSScreen *primary = [[NSScreen screens] firstObject];
        NSRect screen_frame = primary ? [primary frame] : NSMakeRect(0, 0, 1512, 982);
        double scale = primary ? [primary backingScaleFactor] : 1.0;
        if (scale <= 0.0) scale = 1.0;
        pt.x = (mouse_loc.x - screen_frame.origin.x) * scale;
        pt.y = (screen_frame.origin.y + screen_frame.size.height - mouse_loc.y) * scale;
    }
    return pt;
}

int64_t gaze_macos_get_mouse_button_state() {
    @autoreleasepool {
        NSUInteger buttons = [NSEvent pressedMouseButtons];
        int64_t state = 0;
        if (buttons & (1 << 0)) state |= 1; // Left button
        if (buttons & (1 << 1)) state |= 2; // Right button
        if (buttons & (1 << 2)) state |= 4; // Middle button
        return state;
    }
}

} // namespace Gaze

