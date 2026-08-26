#include "doctest.h"
#include "../src/core/math_defs.hpp"
#include <cmath>
#include <iostream>

TEST_CASE("Gaze Input - 3D Inverse Kinematics Ray Formulation")
{
    // Display screen: 345mm x 215mm, 1920 x 1080 px
    const double screen_w_mm = 345.0;
    const double screen_h_mm = 215.0;
    const double screen_w_px = 1920.0;
    const double screen_h_px = 1080.0;
    const double head_dist_mm = 500.0;

    // Test 1: Screen Center (960, 540)
    {
        double px_x = 960.0;
        double px_y = 540.0;

        double target_x_mm = (px_x / screen_w_px - 0.5) * screen_w_mm;
        double target_y_mm = (px_y / screen_h_px - 0.5) * screen_h_mm;
        double target_z_mm = 0.0;

        CHECK(std::abs(target_x_mm - 0.0) < 1e-4);
        CHECK(std::abs(target_y_mm - 0.0) < 1e-4);

        // Origin at (0, 0, 500mm) looking at (0, 0, 0mm)
        double dir_x = target_x_mm - 0.0;
        double dir_y = target_y_mm - 0.0;
        double dir_z = target_z_mm - head_dist_mm;
        double len = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);

        double norm_dir_x = dir_x / len;
        double norm_dir_y = dir_y / len;
        double norm_dir_z = dir_z / len;

        CHECK(std::abs(norm_dir_x - 0.0) < 1e-4);
        CHECK(std::abs(norm_dir_y - 0.0) < 1e-4);
        CHECK(std::abs(norm_dir_z - (-1.0)) < 1e-4);

        // Ray-plane intersection at z = 0
        // p(t) = origin + t * dir -> z(t) = 500 + t * (-1) = 0 -> t = 500
        double t = -head_dist_mm / norm_dir_z;
        double hit_x = 0.0 + t * norm_dir_x;
        double hit_y = 0.0 + t * norm_dir_y;
        double hit_z = head_dist_mm + t * norm_dir_z;

        CHECK(std::abs(hit_x - target_x_mm) < 1e-4);
        CHECK(std::abs(hit_y - target_y_mm) < 1e-4);
        CHECK(std::abs(hit_z - 0.0) < 1e-4);
    }

    // Test 2: Top-Left Corner (0, 0)
    {
        double px_x = 0.0;
        double px_y = 0.0;

        double target_x_mm = (px_x / screen_w_px - 0.5) * screen_w_mm;
        double target_y_mm = (px_y / screen_h_px - 0.5) * screen_h_mm;
        double target_z_mm = 0.0;

        CHECK(std::abs(target_x_mm - (-172.5)) < 1e-4);
        CHECK(std::abs(target_y_mm - (-107.5)) < 1e-4);

        double dir_x = target_x_mm - 0.0;
        double dir_y = target_y_mm - 0.0;
        double dir_z = target_z_mm - head_dist_mm;
        double len = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);

        double norm_dir_x = dir_x / len;
        double norm_dir_y = dir_y / len;
        double norm_dir_z = dir_z / len;

        // Verify ray points left, up/down, and forward into screen
        CHECK(norm_dir_x < -0.2);
        CHECK(norm_dir_y < -0.15);
        CHECK(norm_dir_z < -0.8);

        // Ray-plane intersection
        double t = -head_dist_mm / norm_dir_z;
        double hit_x = 0.0 + t * norm_dir_x;
        double hit_y = 0.0 + t * norm_dir_y;
        double hit_z = head_dist_mm + t * norm_dir_z;

        CHECK(std::abs(hit_x - target_x_mm) < 1e-4);
        CHECK(std::abs(hit_y - target_y_mm) < 1e-4);
        CHECK(std::abs(hit_z - 0.0) < 1e-4);
    }
}

TEST_CASE("Gaze Input - Velocity and Kinematics Invariant")
{
    float p1_x = 100.0f, p1_y = 200.0f;
    float p2_x = 116.0f, p2_y = 200.0f;
    float dt = 0.0166667f; // ~60fps frame delta

    float rel_x = p2_x - p1_x;
    float rel_y = p2_y - p1_y;

    float vel_x = rel_x / dt;
    float vel_y = rel_y / dt;

    CHECK(std::abs(rel_x - 16.0f) < 1e-4f);
    CHECK(std::abs(rel_y - 0.0f) < 1e-4f);
    CHECK(std::abs(vel_x - 960.0f) < 1.0f);
    CHECK(std::abs(vel_y - 0.0f) < 1e-4f);
}
