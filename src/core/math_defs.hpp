/**
 * @file math_defs.hpp
 * @brief Zero-Dependency 3D/2D Vector and Matrix Math structures
 *
 * Implements lightweight vector (GazeVector2, GazeVector3), basis (GazeBasis3D),
 * and transform (GazeTransform3D) structs. Includes utilities for Euler angle
 * decompositions, Rodrigues rotation conversions, and gaze-space forward vector
 * computations.
 */
#pragma once

#include <cmath>
#include <type_traits>
#include <cstdint>

namespace Gaze
{

    // Bilinear resize and BGR-to-RGB conversion helper
    inline void resize_bgr_to_rgb(const uint8_t *src, int src_w, int src_h, uint8_t *dst, int dst_w, int dst_h)
    {
        float x_ratio = ((float)(src_w - 1)) / dst_w;
        float y_ratio = ((float)(src_h - 1)) / dst_h;
        for (int i = 0; i < dst_h; i++)
        {
            for (int j = 0; j < dst_w; j++)
            {
                int x = (int)(x_ratio * j);
                int y = (int)(y_ratio * i);
                float x_diff = (x_ratio * j) - x;
                float y_diff = (y_ratio * i) - y;
                int src_idx = (y * src_w + x) * 3;

                for (int c = 0; c < 3; c++)
                {
                    float a = src[src_idx + c];
                    float b = src[src_idx + 3 + c];
                    float d = src[src_idx + src_w * 3 + c];
                    float e = src[src_idx + src_w * 3 + 3 + c];

                    float val = a * (1.0f - x_diff) * (1.0f - y_diff) +
                                b * (x_diff) * (1.0f - y_diff) +
                                d * (y_diff) * (1.0f - x_diff) +
                                e * (x_diff) * (y_diff);

                    // Write as RGB: destination index c is swapped for R/B (c=0 B -> RGB red; c=2 R -> RGB blue)
                    int dst_c = 2 - c;
                    dst[(i * dst_w + j) * 3 + dst_c] = (uint8_t)val;
                }
            }
        }
    }

    static constexpr double PI = 3.14159265358979323846;
    static constexpr double TAU = 6.28318530717958647692;
    static constexpr double DEG_TO_RAD = PI / 180.0;
    static constexpr double RAD_TO_DEG = 180.0 / PI;

    // Default horizontal field of view in degrees.
    // A ratio of 1000/640 (1.5625) corresponds to a horizontal FOV of ~35.49 degrees:
    // f_x / W = 1 / (2 * tan(FOV/2)) => FOV = 2 * arctan(1 / (2 * 1.5625)) = 35.4885 degrees.
    static constexpr double DEFAULT_CAMERA_FOV_DEGREES = 35.488537576579634;
    static constexpr double DEFAULT_FOCAL_TO_WIDTH_RATIO = 1.5625;

    // Computes focal length in pixels from camera sensor width and horizontal field of view.
    inline double get_focal_length_px(double width_px, double fov_degrees)
    {
        return width_px / (2.0 * std::tan(fov_degrees * DEG_TO_RAD * 0.5));
    }

    // Computes focal length of a scaled image: f' = f_original * (new_dim / original_dim)
    inline double get_focal_length_under_scaling(double f_original, double original_dim, double new_dim)
    {
        if (original_dim <= 0.0) return 0.0;
        return f_original * (new_dim / original_dim);
    }

    // Computes the expected pixel width of a credit card on screen for a given HFOV and distance
    inline double get_card_width_px(double fov_degrees, double card_distance_mm, double frame_width, double card_width_mm = 85.603)
    {
        if (card_distance_mm <= 0.0) return 0.0;
        double fov_rad = fov_degrees * DEG_TO_RAD;
        double denom = 2.0 * card_distance_mm * std::tan(fov_rad * 0.5);
        if (denom <= 0.0) return 0.0;
        return (frame_width * card_width_mm) / denom;
    }

    // Converts Diagonal FOV to Horizontal FOV given current aspect ratio
    inline double diagonal_to_horizontal_fov(double diagonal_fov_degrees, double width, double height)
    {
        if (width <= 0.0 || height <= 0.0 || diagonal_fov_degrees <= 0.0) return 0.0;
        double diag_rad = diagonal_fov_degrees * DEG_TO_RAD;
        double diag_px = std::sqrt(width * width + height * height);
        double h_fov_rad = 2.0 * std::atan((width / diag_px) * std::tan(diag_rad * 0.5));
        return h_fov_rad * RAD_TO_DEG;
    }

    // Binary-compatible layout with Godot's Vector2 class (both are sequential doubles).
    // Static layout assertions at the end of the file guarantee standard layout and trivial copyability,
    // permitting direct binary casting (e.g. reinterpret_cast or static_cast via raw data pointers).
    struct GazeVector2_64f
    {
        double x;
        double y;

        GazeVector2_64f() = default;
        constexpr GazeVector2_64f(double px, double py) : x(px), y(py) {}

        constexpr explicit operator bool() const
        {
            return x != 0.0 || y != 0.0;
        }

        GazeVector2_64f operator+(const GazeVector2_64f &other) const
        {
            return GazeVector2_64f(x + other.x, y + other.y);
        }

        GazeVector2_64f operator-(const GazeVector2_64f &other) const
        {
            return GazeVector2_64f(x - other.x, y - other.y);
        }

        GazeVector2_64f operator*(double scalar) const
        {
            return GazeVector2_64f(x * scalar, y * scalar);
        }

        GazeVector2_64f operator/(double scalar) const
        {
            return GazeVector2_64f(x / scalar, y / scalar);
        }

        double length() const
        {
            return std::sqrt(x * x + y * y);
        }
    };

    using GazeVector2 = GazeVector2_64f;

    // Binary-compatible layout with Godot's Vector2 class when compiled with float precision.
    struct GazeVector2_32f
    {
        float x;
        float y;

        GazeVector2_32f() = default;
        constexpr GazeVector2_32f(float px, float py) : x(px), y(py) {}
    };

    using GazePoint = GazeVector2_64f;
    struct GazeVector2i
    {
        int x;
        int y;

        GazeVector2i() = default;
        constexpr GazeVector2i(int px, int py) : x(px), y(py) {}

        constexpr explicit operator bool() const
        {
            return x != 0 || y != 0;
        }
    };

    // Precision-explicit float-based 2D vector. Used primarily for pixel-space coordinates 
    // (such as facial landmarks output by inference models) where 32-bit floating precision is sufficient.


    // Binary-compatible layout with Godot's Rect2 class.
    // Storing sequentially as x, y (position) and width, height (size) is binary identical 
    // to a struct of two Vector2 components, allowing direct memory mapping.
    struct GazeRect
    {
        float x;
        float y;
        float width;
        float height;

        GazeRect() = default;
        constexpr GazeRect(float px, float py, float pw, float ph) : x(px), y(py), width(pw), height(ph) {}

        float area() const
        {
            return width * height;
        }
    };

    struct GazeVector3
    {
        float x;
        float y;
        float z;

        GazeVector3() = default;
        constexpr GazeVector3(float px, float py, float pz) : x(px), y(py), z(pz) {}

        constexpr explicit operator bool() const
        {
            return x != 0.0f || y != 0.0f || z != 0.0f;
        }

        GazeVector3 operator+(const GazeVector3 &other) const
        {
            return GazeVector3(x + other.x, y + other.y, z + other.z);
        }

        GazeVector3 operator-(const GazeVector3 &other) const
        {
            return GazeVector3(x - other.x, y - other.y, z - other.z);
        }

        GazeVector3 operator*(float scalar) const
        {
            return GazeVector3(x * scalar, y * scalar, z * scalar);
        }

        float dot(const GazeVector3 &other) const
        {
            return x * other.x + y * other.y + z * other.z;
        }

        GazeVector3 cross(const GazeVector3 &other) const
        {
            return GazeVector3(
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x);
        }

        float length() const
        {
            return std::sqrt(x * x + y * y + z * z);
        }

        GazeVector3 normalized() const
        {
            float len = length();
            if (len > 0.0f)
            {
                return GazeVector3(x / len, y / len, z / len);
            }
            return GazeVector3(0.0f, 0.0f, 0.0f);
        }

        /**
         * @brief Computes the pitch and yaw (in degrees) of this direction vector.
         *
         * Unlike standard spherical coordinates, this computes the yaw relative to a
         * customizable reference direction (defaulting to the forward/negative-Z direction
         * - GazeVector3(0, 0, -1) in OpenCV space) and normalizes the yaw
         * difference into [-180, 180] degrees.
         */
        GazeVector2 get_pitch_yaw(const GazeVector3 &relative = GazeVector3(0.0f, 0.0f, -1.0f)) const
        {
            GazeVector3 n = normalized();
            double pitch_rad = std::asin(static_cast<double>(n.y));

            double yaw_rad = std::atan2(static_cast<double>(-n.x), static_cast<double>(-n.z));
            double rel_yaw_rad = std::atan2(static_cast<double>(-relative.x), static_cast<double>(-relative.z));
            double yaw_diff = yaw_rad - rel_yaw_rad;
            while (yaw_diff > PI)
                yaw_diff -= TAU;
            while (yaw_diff < -PI)
                yaw_diff += TAU;

            return GazeVector2(
                pitch_rad * RAD_TO_DEG,
                yaw_diff * RAD_TO_DEG);
        }

        GazeVector2 get_pitch_yaw_rad() const;
        GazeVector2 get_pitch_yaw_rad(const GazeVector3 &relative_to) const
        {
            GazeVector3 n = normalized();
            double pitch_rad = std::asin(static_cast<double>(n.y));

            double yaw_rad = std::atan2(static_cast<double>(-n.x), static_cast<double>(-n.z));
            double rel_yaw_rad = std::atan2(static_cast<double>(-relative_to.x), static_cast<double>(-relative_to.z));
            double yaw_diff = yaw_rad - rel_yaw_rad;
            while (yaw_diff > PI)
                yaw_diff -= TAU;
            while (yaw_diff < -PI)
                yaw_diff += TAU;

            return GazeVector2(
                pitch_rad,
                yaw_diff);
        }
    };

    // Common direction constants
    inline constexpr GazeVector3 LEFT = GazeVector3(-1.0f, 0.0f, 0.0f);
    inline constexpr GazeVector3 RIGHT = GazeVector3(1.0f, 0.0f, 0.0f);
    inline constexpr GazeVector3 UP = GazeVector3(0.0f, 1.0f, 0.0f);
    inline constexpr GazeVector3 DOWN = GazeVector3(0.0f, -1.0f, 0.0f);
    inline constexpr GazeVector3 FORWARD = GazeVector3(0.0f, 0.0f, -1.0f);
    inline constexpr GazeVector3 BACK = GazeVector3(0.0f, 0.0f, 1.0f);

    inline GazeVector2 GazeVector3::get_pitch_yaw_rad() const
    {
        return get_pitch_yaw_rad(FORWARD);
    }

    struct GazeBasis3D
    {
        GazeVector3 x;
        GazeVector3 y;
        GazeVector3 z;

        GazeBasis3D() = default;
        constexpr GazeBasis3D(const GazeVector3 &px, const GazeVector3 &py, const GazeVector3 &pz) : x(px), y(py), z(pz) {}

        static constexpr GazeBasis3D identity()
        {
            return GazeBasis3D(GazeVector3(1.0, 0.0, 0.0), GazeVector3(0.0, 1.0, 0.0), GazeVector3(0.0, 0.0, 1.0));
        }

        GazeVector3 multiply_vector(const GazeVector3 &v) const
        {
            return x * v.x + y * v.y + z * v.z;
        }

        GazeBasis3D operator*(const GazeBasis3D &other) const
        {
            return GazeBasis3D(
                multiply_vector(other.x),
                multiply_vector(other.y),
                multiply_vector(other.z));
        }

        static GazeBasis3D from_euler_zyx(double pitch_deg, double yaw_deg, double roll_deg)
        {
            double p = pitch_deg * DEG_TO_RAD;
            double y = yaw_deg * DEG_TO_RAD;
            double r = roll_deg * DEG_TO_RAD;
            double cp = std::cos(p), sp = std::sin(p);
            double cy = std::cos(y), sy = std::sin(y);
            double cr = std::cos(r), sr = std::sin(r);
            return GazeBasis3D(
                GazeVector3(cr * cy, sr * cy, -sy),
                GazeVector3(cr * sy * sp - sr * cp, sr * sy * sp + cr * cp, cy * sp),
                GazeVector3(cr * sy * cp + sr * sp, sr * sy * cp - cr * sp, cy * cp));
        }

        /**
         * @brief Decomposes the basis into ZYX Euler angles (pitch, yaw, roll in degrees).
         *
         * NON-STANDARD BEHAVIOR:
         * Choosing a standard Euler decomposition for a face pointing towards the camera results
         * in yaw being close to 180 degrees. At this boundary, standard ZYX decompositions (which
         * restrict yaw to [-90, 90] degrees) experience severe gimbal-lock/sign-flipping discontinuities.
         * This function is specialized to force the decomposition to select the branch where
         * cos(yaw) < 0 (yaw centered around 180 degrees), ensuring smooth angles during direct-to-camera tracking.
         */
        GazeVector3 get_euler_deg() const
        {
            double sy = std::sqrt(x.x * x.x + x.y * x.y);
            bool singular = sy < 1e-6;
            double pitch = 0.0, yaw = 0.0, roll = 0.0;
            if (!singular)
            {
                // Since the face points towards the camera, yaw is around 180 degrees.
                // This means cos(yaw) < 0. We decompose choosing the branch where cos(yaw) is negative.
                pitch = std::atan2(-y.z, -z.z) * RAD_TO_DEG;
                yaw = std::atan2(-x.z, -sy) * RAD_TO_DEG;
                roll = std::atan2(-x.y, -x.x) * RAD_TO_DEG;
            }
            else
            {
                pitch = std::atan2(-y.y, y.x) * RAD_TO_DEG;
                yaw = std::atan2(-x.z, -sy) * RAD_TO_DEG;
                roll = 0.0;
            }
            return GazeVector3(pitch, yaw, roll);
        }

        /**
         * @brief Decomposes the basis into standard ZYX Euler angles (pitch, yaw, roll in degrees).
         *
         * Standard ZYX decomposition that restricts the yaw to [-90, 90] degrees (choosing the
         * branch where cos(yaw) > 0). Used specifically for feeding inputs to the ONNX gaze model
         * which expects a standard coordinate convention (yaw centered around 0 degrees).
         */
        GazeVector3 get_euler_gaze_model_deg() const
        {
            double sy = std::sqrt(x.x * x.x + x.y * x.y);
            bool singular = sy < 1e-6;
            double pitch = 0.0, yaw = 0.0, roll = 0.0;
            if (!singular)
            {
                pitch = std::atan2(y.z, z.z) * RAD_TO_DEG;
                yaw = std::atan2(-x.z, sy) * RAD_TO_DEG;
                roll = std::atan2(x.y, x.x) * RAD_TO_DEG;
            }
            else
            {
                pitch = std::atan2(-y.y, y.x) * RAD_TO_DEG;
                yaw = std::atan2(-x.z, sy) * RAD_TO_DEG;
                roll = 0.0;
            }
            return GazeVector3(pitch, yaw, roll);
        }
    };

    struct GazeTransform3D
    {
        GazeBasis3D basis;
        GazeVector3 origin;

        GazeTransform3D() = default;
        constexpr GazeTransform3D(const GazeBasis3D &b, const GazeVector3 &o) : basis(b), origin(o) {}

        static constexpr GazeTransform3D identity()
        {
            return GazeTransform3D(GazeBasis3D::identity(), GazeVector3(0.0, 0.0, 0.0));
        }

        GazeTransform3D operator*(const GazeTransform3D &other) const
        {
            return GazeTransform3D(
                basis * other.basis,
                basis.multiply_vector(other.origin) + origin);
        }
    };

    /**
     * @brief Converts a 3D rotation vector (Rodrigues vector) to a 3x3 rotation basis.
     *
     * Commonly known in graphics/game math as the Axis-Angle to Matrix conversion (where the
     * vector's direction is the axis of rotation and its magnitude is the angle in radians).
     * Named "rodrigues" to match the OpenCV cv::Rodrigues convention since it is used to parse
     * head pose rotations returned by OpenCV's estimator.
     */
    inline GazeBasis3D rodrigues_to_basis(const GazeVector3 &r)
    {
        double theta = r.length();
        if (theta < 1e-6)
        {
            return GazeBasis3D::identity();
        }
        double ux = r.x / theta;
        double uy = r.y / theta;
        double uz = r.z / theta;

        double s = std::sin(theta);
        double c = std::cos(theta);
        double oc = 1.0 - c;

        return GazeBasis3D(
            GazeVector3(c + oc * ux * ux, oc * ux * uy + s * uz, oc * ux * uz - s * uy),
            GazeVector3(oc * ux * uy - s * uz, c + oc * uy * uy, oc * uy * uz + s * ux),
            GazeVector3(oc * ux * uz + s * uy, oc * uy * uz - s * ux, c + oc * uz * uz));
    }

    /**
     * @brief Converts a 3x3 rotation basis to a 3D rotation vector (Rodrigues vector).
     *
     * Inverse of rodrigues_to_basis, matching OpenCV's cv::Rodrigues convention.
     */
    inline GazeVector3 basis_to_rodrigues(const GazeBasis3D &basis)
    {
        double trace = basis.x.x + basis.y.y + basis.z.z;
        double cos_theta = 0.5 * (trace - 1.0);
        if (cos_theta > 1.0)
            cos_theta = 1.0;
        else if (cos_theta < -1.0)
            cos_theta = -1.0;
        double theta = std::acos(cos_theta);

        if (theta < 1e-6)
        {
            double scale = 0.5;
            return GazeVector3(
                scale * (basis.y.z - basis.z.y),
                scale * (basis.z.x - basis.x.z),
                scale * (basis.x.y - basis.y.x));
        }
        else if (std::abs(theta - PI) < 1e-4)
        {
            double xx = (basis.x.x + 1.0) * 0.5;
            double yy = (basis.y.y + 1.0) * 0.5;
            double zz = (basis.z.z + 1.0) * 0.5;
            double xy = (basis.y.x + basis.x.y) * 0.25;
            double xz = (basis.z.x + basis.x.z) * 0.25;
            double yz = (basis.z.y + basis.y.z) * 0.25;

            double x = 0.0, y = 0.0, z = 0.0;
            if (xx > yy && xx > zz)
            {
                if (xx < 0.0)
                    xx = 0.0;
                x = std::sqrt(xx);
                y = xy / x;
                z = xz / x;
            }
            else if (yy > zz)
            {
                if (yy < 0.0)
                    yy = 0.0;
                y = std::sqrt(yy);
                x = xy / y;
                z = yz / y;
            }
            else
            {
                if (zz < 0.0)
                    zz = 0.0;
                z = std::sqrt(zz);
                x = xz / z;
                y = yz / z;
            }
            double len = std::sqrt(x * x + y * y + z * z);
            if (len > 1e-6)
            {
                x /= len;
                y /= len;
                z /= len;
            }
            return GazeVector3(x, y, z) * theta;
        }
        else
        {
            double scale = 0.5 * theta / std::sin(theta);
            return GazeVector3(
                scale * (basis.y.z - basis.z.y),
                scale * (basis.z.x - basis.x.z),
                scale * (basis.x.y - basis.y.x));
        }
    }

    inline GazeVector3 get_head_forward_in_camera_space(const GazeVector3 &rotation_deg)
    {
        double pitch_rad = rotation_deg.x * DEG_TO_RAD;
        double yaw_rad = rotation_deg.y * DEG_TO_RAD;
        double roll_rad = rotation_deg.z * DEG_TO_RAD;

        double cp = std::cos(pitch_rad), sp = std::sin(pitch_rad);
        double cy = std::cos(yaw_rad), sy = std::sin(yaw_rad);
        double cr = std::cos(roll_rad), sr = std::sin(roll_rad);

        double r02 = cr * sy * cp + sr * sp;
        double r12 = sr * sy * cp - cr * sp;
        double r22 = cy * cp;

        return GazeVector3(-r02, r12, r22);
    }

    /**
     * @brief Intersects a 3D ray with a plane in 3D space.
     *
     * Solve for parameter t: plane_normal.dot(ray_origin + t * ray_dir) = plane_d
     * => t = (plane_d - plane_normal.dot(ray_origin)) / plane_normal.dot(ray_dir)
     */
    inline bool intersect_ray_plane(
        const GazeVector3 &ray_origin,
        const GazeVector3 &ray_dir,
        const GazeVector3 &plane_normal,
        double plane_d,
        double &out_t)
    {
        double denom = plane_normal.dot(ray_dir);
        if (std::abs(denom) < 1e-6)
        {
            return false;
        }
        out_t = (plane_d - plane_normal.dot(ray_origin)) / denom;
        return out_t >= 0.0;
    }

    /**
     * @brief Applies pitch and yaw bias corrections (and scaling) to a raw 3D gaze vector.
     */
    inline GazeVector3 apply_3d_bias_vector(
        const GazeVector3 &raw_gaze_dir,
        const GazeVector2 &bias_pitch_yaw,
        const GazeVector2 &scale_pitch_yaw = GazeVector2(1.0, 1.0))
    {
        GazeVector2 py = raw_gaze_dir.get_pitch_yaw_rad();
        double calib_yaw = py.y * scale_pitch_yaw.y + bias_pitch_yaw.y;
        double calib_pitch = py.x * scale_pitch_yaw.x + bias_pitch_yaw.x;

        double cos_pitch = std::cos(calib_pitch);
        return GazeVector3(
                   -std::sin(calib_yaw) * cos_pitch,
                   std::sin(calib_pitch),
                   -std::cos(calib_yaw) * cos_pitch)
            .normalized();
    }

    /**
     * @brief Projects a camera-space gaze ray onto a tilted screen plane in millimeters.
     */
    inline bool project_ray_to_screen_mm(
        const GazeVector3 &origin_cam,
        const GazeVector3 &dir_cam,
        const GazeVector3 &camera_offset,
        double camera_tilt_deg,
        const GazeVector2 &screen_size_mm,
        GazeVector2 &out_pos_mm)
    {
        double theta_rad = camera_tilt_deg * DEG_TO_RAD;
        double cos_t = std::cos(theta_rad);
        double sin_t = std::sin(theta_rad);

        double O_disp_z = sin_t * origin_cam.y - cos_t * origin_cam.z + camera_offset.z;
        double v_disp_z = sin_t * dir_cam.y - cos_t * dir_cam.z;

        if (std::abs(v_disp_z) < 1e-6)
        {
            return false;
        }

        double t = -O_disp_z / v_disp_z;
        if (t < 0.0)
        {
            return false;
        }

        double W_half = screen_size_mm.x * 0.5;
        double H_half = screen_size_mm.y * 0.5;

        double O_disp_x = -origin_cam.x + camera_offset.x + W_half;
        double O_disp_y = -(cos_t * origin_cam.y + sin_t * origin_cam.z + camera_offset.y) + H_half;

        double v_disp_x = -dir_cam.x;
        double v_disp_y = -(cos_t * dir_cam.y + sin_t * dir_cam.z);

        out_pos_mm.x = O_disp_x + v_disp_x * t;
        out_pos_mm.y = O_disp_y + v_disp_y * t;
        return true;
    }

    // type_traits included at top
    static_assert(std::is_standard_layout<GazeVector2_64f>::value, "GazeVector2_64f must be standard-layout");
    static_assert(std::is_trivial<GazeVector2_64f>::value, "GazeVector2_64f must be trivial");

    static_assert(std::is_standard_layout<GazeVector2i>::value, "GazeVector2i must be standard-layout");
    static_assert(std::is_trivial<GazeVector2i>::value, "GazeVector2i must be trivial");

    static_assert(std::is_standard_layout<GazeVector2_32f>::value, "GazeVector2_32f must be standard-layout");
    static_assert(std::is_trivial<GazeVector2_32f>::value, "GazeVector2_32f must be trivial");

    static_assert(std::is_standard_layout<GazeRect>::value, "GazeRect must be standard-layout");
    static_assert(std::is_trivial<GazeRect>::value, "GazeRect must be trivial");

    static_assert(std::is_standard_layout<GazeVector3>::value, "GazeVector3 must be standard-layout");
    static_assert(std::is_trivial<GazeVector3>::value, "GazeVector3 must be trivial");

    static_assert(std::is_standard_layout<GazeBasis3D>::value, "GazeBasis3D must be standard-layout");
    static_assert(std::is_trivial<GazeBasis3D>::value, "GazeBasis3D must be trivial");

    static_assert(std::is_standard_layout<GazeTransform3D>::value, "GazeTransform3D must be standard-layout");
    static_assert(std::is_trivial<GazeTransform3D>::value, "GazeTransform3D must be trivial");

} // namespace Gaze
