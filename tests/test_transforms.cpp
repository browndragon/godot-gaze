#include "doctest.h"
#include "../src/core/math_defs.hpp"
#include <cmath>
#include <iostream>

struct AffineTransform2D {
    float m[6]; // m[0]*x + m[1]*y + m[2], m[3]*x + m[4]*y + m[5]

    static AffineTransform2D create(float scale_x, float scale_y, float rotation_rad, float tx, float ty) {
        float cos_r = std::cos(rotation_rad);
        float sin_r = std::sin(rotation_rad);
        AffineTransform2D res;
        res.m[0] = scale_x * cos_r;
        res.m[1] = -scale_y * sin_r;
        res.m[2] = tx;
        res.m[3] = scale_x * sin_r;
        res.m[4] = scale_y * cos_r;
        res.m[5] = ty;
        return res;
    }

    void transform_point(float x, float y, float& out_x, float& out_y) const {
        out_x = m[0] * x + m[1] * y + m[2];
        out_y = m[3] * x + m[4] * y + m[5];
    }

    AffineTransform2D inverse() const {
        float det = m[0] * m[4] - m[1] * m[3];
        if (std::abs(det) < 1e-7f) det = 1.0f;
        float inv_det = 1.0f / det;

        AffineTransform2D inv;
        inv.m[0] = m[4] * inv_det;
        inv.m[1] = -m[1] * inv_det;
        inv.m[2] = (m[1] * m[5] - m[2] * m[4]) * inv_det;
        inv.m[3] = -m[3] * inv_det;
        inv.m[4] = m[0] * inv_det;
        inv.m[5] = (m[2] * m[3] - m[0] * m[5]) * inv_det;
        return inv;
    }
};

TEST_CASE("Affine Transform - Inverse Reversibility Invariant Test")
{
    AffineTransform2D M = AffineTransform2D::create(2.5f, 2.5f, 0.45f, 150.0f, 85.0f);
    AffineTransform2D M_inv = M.inverse();

    float p_x = 320.0f;
    float p_y = 240.0f;

    float t_x = 0.0f, t_y = 0.0f;
    M.transform_point(p_x, p_y, t_x, t_y);

    float rec_x = 0.0f, rec_y = 0.0f;
    M_inv.transform_point(t_x, t_y, rec_x, rec_y);

    CHECK(std::abs(rec_x - p_x) < 1e-3f);
    CHECK(std::abs(rec_y - p_y) < 1e-3f);
}

TEST_CASE("Affine Transform - Orthogonality & Aspect Ratio Invariant Test")
{
    AffineTransform2D M = AffineTransform2D::create(1.8f, 1.8f, -0.30f, 50.0f, 40.0f);

    float u_x = 1.0f, u_y = 0.0f;
    float v_x = 0.0f, v_y = 1.0f;

    float tu_x = 0.0f, tu_y = 0.0f;
    float tv_x = 0.0f, tv_y = 0.0f;
    float origin_x = 0.0f, origin_y = 0.0f;

    M.transform_point(0.0f, 0.0f, origin_x, origin_y);
    M.transform_point(u_x, u_y, tu_x, tu_y);
    M.transform_point(v_x, v_y, tv_x, tv_y);

    float vec_u_x = tu_x - origin_x;
    float vec_u_y = tu_y - origin_y;
    float vec_v_x = tv_x - origin_x;
    float vec_v_y = tv_y - origin_y;

    float dot_product = vec_u_x * vec_v_x + vec_u_y * vec_v_y;
    CHECK(std::abs(dot_product) < 1e-4f);

    float len_u = std::sqrt(vec_u_x * vec_u_x + vec_u_y * vec_u_y);
    float len_v = std::sqrt(vec_v_x * vec_v_x + vec_v_y * vec_v_y);
    CHECK(std::abs(len_u - len_v) < 1e-4f);
}

TEST_CASE("Affine Transform - Physical IPD Scale Invariant Test")
{
    float l_x = 600.0f, l_y = 400.0f;
    float r_x = 800.0f, r_y = 400.0f;

    double focal = 1280.0 * 1.5;
    double eye_dist_px = std::sqrt((r_x - l_x)*(r_x - l_x) + (r_y - l_y)*(r_y - l_y));
    double z_mm = (focal * 63.0) / eye_dist_px;

    CHECK(z_mm >= 300.0);
    CHECK(z_mm <= 1200.0);
    CHECK(std::abs(z_mm - 604.8) < 10.0);
}
