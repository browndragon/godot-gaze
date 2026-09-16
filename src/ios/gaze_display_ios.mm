#include "gaze_display_ios.hpp"

#import <UIKit/UIKit.h>
#include <cmath>

namespace Gaze {


GazeDisplayMetrics gaze_ios_get_display_metrics(int screen_index) {
    GazeDisplayMetrics metrics;

    UIScreen *screen = [UIScreen mainScreen];
    CGRect bounds = [screen bounds];
    CGFloat scale = [screen scale];

    metrics.pixel_width = (int)bounds.size.width;
    metrics.pixel_height = (int)bounds.size.height;
    metrics.scale_factor = (double)scale;

    double ppi = 460.0; // Default iPhone Super Retina OLED PPI
    if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) {
        ppi = 264.0; // iPad Retina PPI
    }

    metrics.width_mm = (metrics.pixel_width * scale * 25.4) / ppi;
    metrics.height_mm = (metrics.pixel_height * scale * 25.4) / ppi;

    if (metrics.width_mm <= 0.0 || metrics.height_mm <= 0.0) {
        metrics.width_mm = metrics.pixel_width * 0.15;
        metrics.height_mm = metrics.pixel_height * 0.15;
    }

    return metrics;
}

GazeWindowRect gaze_ios_get_window_rect(int window_index) {
    GazeWindowRect rect;

    @autoreleasepool {
        UIWindow *window = nil;
        if (@available(iOS 13.0, *)) {
            for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
                if (scene.activationState == UISceneActivationStateForegroundActive && [scene isKindOfClass:[UIWindowScene class]]) {
                    UIWindowScene *windowScene = (UIWindowScene *)scene;
                    for (UIWindow *w in windowScene.windows) {
                        if (w.isKeyWindow) {
                            window = w;
                            break;
                        }
                    }
                }
            }
        }
        if (!window) {
            window = [UIApplication sharedApplication].keyWindow;
        }

        if (window) {
            CGRect bounds = [window bounds];
            rect.x = (int)std::round(bounds.origin.x);
            rect.y = (int)std::round(bounds.origin.y);
            rect.width = (int)std::round(bounds.size.width);
            rect.height = (int)std::round(bounds.size.height);
        } else {
            UIScreen *screen = [UIScreen mainScreen];
            CGRect bounds = [screen bounds];
            rect.x = (int)std::round(bounds.origin.x);
            rect.y = (int)std::round(bounds.origin.y);
            rect.width = (int)std::round(bounds.size.width);
            rect.height = (int)std::round(bounds.size.height);
        }
    }

    return rect;
}

GazeMousePoint gaze_ios_get_mouse_position() {
    return {0.0, 0.0};
}

int64_t gaze_ios_get_mouse_button_state() {
    return 0;
}

} // namespace Gaze


