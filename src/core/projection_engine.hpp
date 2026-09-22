/**
 * @file projection_engine.hpp
 * @brief Ray-Plane Projection Engine and Calibration Solver (Layer 4)
 *
 * Implements the mathematical models for projecting raw 3D gaze rays onto
 * physical screen planes, incorporating offset, resolution, and tilt parameters.
 * Computes analytical calibration offsets (both 3D angular biases and 2D pixel offsets)
 * and estimates focal depth from eye distance.
 */
#pragma once

#include "camera_placement.hpp"
#include "opencv_space_conversions.hpp"

namespace Gaze
{

    enum class DisplayOrientation {
        ORIENTATION_0 = 0,     // Natural upright landscape
        ORIENTATION_90 = 90,   // Rotated 90 deg clockwise (portrait right)
        ORIENTATION_180 = 180, // Inverted landscape (180 deg)
        ORIENTATION_270 = 270  // Rotated 270 deg clockwise (portrait left)
    };

    class ProjectionEngine
    {
    private:
        CameraPlacement placement;
        GodotDisplayVector2 screen_size_pixels = GodotDisplayVector2(1920.0, 1080.0);
        SpacedVector2<Space::GodotDisplayMm> screen_size_mm = SpacedVector2<Space::GodotDisplayMm>(527.0, 296.0); // e.g. Typical 24-inch 16:9 monitor
        double camera_focal_length_px = 1000.0;                 // For Z depth estimation if needed
        DisplayOrientation display_orientation = DisplayOrientation::ORIENTATION_0;
        GodotCameraVector3 gravity_vector = GodotCameraVector3(0.0, -1.0, 0.0);
        GodotDisplayVector2 window_offset_pixels = GodotDisplayVector2(0.0, 0.0);

    public:
        ProjectionEngine() = default;

        // Setters / Getters
        void set_camera_placement(const CameraPlacement &p) { placement = p; }
        CameraPlacement get_camera_placement() const { return placement; }

        void set_screen_size_pixels(const GodotDisplayVector2 &size) { screen_size_pixels = size; }
        GodotDisplayVector2 get_screen_size_pixels() const { return screen_size_pixels; }

        void set_screen_size_mm(const SpacedVector2<Space::GodotDisplayMm> &size) { screen_size_mm = size; }
        SpacedVector2<Space::GodotDisplayMm> get_screen_size_mm() const { return screen_size_mm; }

        void set_camera_focal_length_px(double f) { camera_focal_length_px = f; }
        double get_camera_focal_length_px() const { return camera_focal_length_px; }

        void set_display_orientation(DisplayOrientation orientation) { display_orientation = orientation; }
        DisplayOrientation get_display_orientation() const { return display_orientation; }

        void set_gravity_vector(const GodotCameraVector3 &g) { gravity_vector = g; }
        GodotCameraVector3 get_gravity_vector() const { return gravity_vector; }

        void set_window_offset_pixels(const GodotDisplayVector2 &offset) { window_offset_pixels = offset; }
        GodotDisplayVector2 get_window_offset_pixels() const { return window_offset_pixels; }

        /**
         * @brief Estimate user Z depth from camera in millimeters using pinhole camera IPD triangulation.
         * @param eye_distance_px 2D pixel distance between eye centers.
         * @param ipd_mm Interpupillary distance in millimeters (default: 63.0mm).
         * @return Estimated depth Z in millimeters.
         */
        double estimate_depth_z(double eye_distance_px, double ipd_mm = 63.0) const;

        /**
         * @brief Convert screen pixel coordinates to screen millimeter coordinates relative to center (Y-down positive).
         * @param pixel Screen coordinate in pixels.
         * @return Screen coordinate in millimeters.
         */
        SpacedVector2<Space::GodotDisplayMm> pixel_to_millimeter(const GodotDisplayVector2 &pixel) const;

        /**
         * @brief Map screen millimeter coordinates relative to center to 3D position in Camera Space.
         * @param screen_mm Screen coordinate in millimeters.
         * @return 3D position vector in Camera Space.
         */
        GodotCameraVector3 screen_mm_to_camera_space(const SpacedVector2<Space::GodotDisplayMm> &screen_mm) const;

        /**
         * @brief Project 3D camera-space gaze ray to 2D viewport pixel coordinates.
         * @param gaze_origin_cam Gaze origin in 3D camera space.
         * @param raw_gaze_dir_cam Unit gaze direction vector in 3D camera space.
         * @param out_pixel Output projected 2D viewport coordinates in pixels.
         * @return True if ray intersects display plane in front of user, false otherwise.
         */
        bool project_gaze(const GodotCameraVector3 &gaze_origin_cam,
                          const GodotCameraVector3 &raw_gaze_dir_cam,
                          GodotDisplayVector2 &out_pixel) const;

        /**
         * @brief Map 3D gravity vector to nearest orthogonal DisplayOrientation.
         * @param gravity Gravity vector in camera space.
         * @return Derived DisplayOrientation.
         */
        static DisplayOrientation gravity_to_orientation(const GodotCameraVector3 &gravity);
    };

    /**
     * @brief Wraps an angle in radians to [-pi, pi].
     */
    static inline double wrap_angle_rad(double a) {
        while (a > M_PI) a -= 2.0 * M_PI;
        while (a < -M_PI) a += 2.0 * M_PI;
        return a;
    }

} // namespace Gaze
