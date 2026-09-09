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
        metrics.pixel_width = (int)CGDisplayModeGetWidth(mode);
        metrics.pixel_height = (int)CGDisplayModeGetHeight(mode);
        size_t pixel_w = CGDisplayModeGetPixelWidth(mode);
        if (metrics.pixel_width > 0 && pixel_w > 0) {
            metrics.scale_factor = (double)pixel_w / (double)metrics.pixel_width;
        } else {
            metrics.scale_factor = 1.0;
        }
        CGDisplayModeRelease(mode);
    } else {
        metrics.pixel_width = (int)CGDisplayPixelsWide(target_display);
        metrics.pixel_height = (int)CGDisplayPixelsHigh(target_display);
        metrics.scale_factor = 1.0;
    }

    // Fallback if physical millimeters is 0 (e.g. headless/virtual displays)
    if (metrics.width_mm <= 0.0 || metrics.height_mm <= 0.0) {
        // Default to typical MacBook Retina pitch (~0.20 mm/pt)
        metrics.width_mm = metrics.pixel_width * 0.19946;
        metrics.height_mm = metrics.pixel_height * 0.19194;
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

            // Client content rectangle in Cocoa coordinates (bottom-left origin)
            NSRect content_rect = [window contentRectForFrameRect:[window frame]];
            NSRect screen_rect = screen ? [screen frame] : NSMakeRect(0, 0, 1512, 982);

            // Convert Cocoa bottom-left origin to Top-Left display origin (in Cocoa points / logical pixels)
            rect.x = (int)std::round(content_rect.origin.x - screen_rect.origin.x);
            rect.y = (int)std::round(screen_rect.origin.y + screen_rect.size.height - (content_rect.origin.y + content_rect.size.height));
            rect.width = (int)std::round(content_rect.size.width);
            rect.height = (int)std::round(content_rect.size.height);
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

} // namespace Gaze

