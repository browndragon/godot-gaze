/**
 * @file test_coordinate_calculus.cpp
 * @brief Unit tests for Strongly-Typed Coordinate Frame Calculus (Spaced<T, S>)
 */

#include "doctest.h"
#include "opencv_space_conversions.hpp"
#include "projection_engine.hpp"
#include "face_model_geometry.hpp"
#include "math_defs.hpp"
#include <cmath>

using namespace Gaze;
using namespace Gaze::CoordinateConversions;

TEST_CASE("Coordinate Calculus: Spaced Vector3 Arithmetic and Space Isolation") {
    GodotCameraVector3 v1(10.0, 20.0, -30.0);
    GodotCameraVector3 v2(5.0, -10.0, 15.0);

    // Vector addition within same space
    GodotCameraVector3 sum = v1 + v2;
    CHECK(sum.x == doctest::Approx(15.0));
    CHECK(sum.y == doctest::Approx(10.0));
    CHECK(sum.z == doctest::Approx(-15.0));

    // Vector subtraction within same space
    GodotCameraVector3 diff = v1 - v2;
    CHECK(diff.x == doctest::Approx(5.0));
    CHECK(diff.y == doctest::Approx(30.0));
    CHECK(diff.z == doctest::Approx(-45.0));

    // Scalar multiplication and division
    GodotCameraVector3 scaled = v1 * 2.0;
    CHECK(scaled.x == doctest::Approx(20.0));
    CHECK(scaled.y == doctest::Approx(40.0));
    CHECK(scaled.z == doctest::Approx(-60.0));

    GodotCameraVector3 divided = scaled / 2.0;
    CHECK(divided.x == doctest::Approx(10.0));
    CHECK(divided.y == doctest::Approx(20.0));
    CHECK(divided.z == doctest::Approx(-30.0));

    // Unary negation
    GodotCameraVector3 negated = -v1;
    CHECK(negated.x == doctest::Approx(-10.0));
    CHECK(negated.y == doctest::Approx(-20.0));
    CHECK(negated.z == doctest::Approx(30.0));

    // Dot product
    double d = dot(v1, v2);
    CHECK(d == doctest::Approx(10.0 * 5.0 + 20.0 * -10.0 + -30.0 * 15.0));

    // Length and Normalization
    GodotCameraVector3 v3(0.0, 3.0, 4.0);
    CHECK(v3.length() == doctest::Approx(5.0));

    GodotCameraVector3 norm = v3.normalized();
    CHECK(norm.length() == doctest::Approx(1.0));
    CHECK(norm.y == doctest::Approx(0.6));
    CHECK(norm.z == doctest::Approx(0.8));
}

TEST_CASE("Coordinate Calculus: Spaced Vector2 Arithmetic") {
    GodotDisplayVector2 p1(100.0, 200.0);
    GodotDisplayVector2 p2(30.0, 40.0);

    GodotDisplayVector2 sum = p1 + p2;
    CHECK(sum.x == doctest::Approx(130.0));
    CHECK(sum.y == doctest::Approx(240.0));

    GodotDisplayVector2 diff = p1 - p2;
    CHECK(diff.x == doctest::Approx(70.0));
    CHECK(diff.y == doctest::Approx(160.0));
}

TEST_CASE("Coordinate Calculus: Spaced Basis and Transform Properties") {
    GodotFaceTransform3D xform = GodotFaceTransform3D::identity();
    CHECK(xform.basis.x.x == doctest::Approx(1.0));
    CHECK(xform.basis.y.y == doctest::Approx(1.0));
    CHECK(xform.basis.z.z == doctest::Approx(1.0));
    CHECK(xform.origin.x == doctest::Approx(0.0));
    CHECK(xform.origin.y == doctest::Approx(0.0));
    CHECK(xform.origin.z == doctest::Approx(0.0));

    // get_euler_rad on identity
    auto euler = xform.basis.get_euler_rad();
    CHECK(euler.x == doctest::Approx(0.0));
    CHECK(euler.y == doctest::Approx(0.0));
    CHECK(euler.z == doctest::Approx(0.0));
}

TEST_CASE("Coordinate Calculus: Spoke-to-Hub OpenCV Camera to Godot Camera Transformation") {
    // OpenCV Camera Space: +X Right, +Y Down, +Z Away from camera into scene
    // Godot Camera Space: +X Right (display left), +Y Up, -Z Forward into room / towards user
    OpenCVCameraVector3 cv_pt(15.0, 25.0, 600.0);

    GodotCameraVector3 godot_pt = to_godot_camera(cv_pt);
    // X is preserved (+X right), Y is inverted (+Y up), Z is inverted (-Z towards user)
    CHECK(godot_pt.x == doctest::Approx(15.0));
    CHECK(godot_pt.y == doctest::Approx(-25.0));
    CHECK(godot_pt.z == doctest::Approx(-600.0));
}

TEST_CASE("Coordinate Calculus: Spoke-to-Hub OpenVINO ADAS Gaze Conversion") {
    // OpenVINO Gaze: +X user right / display left, +Y up, -Z forward towards camera
    // Godot Camera Space: +X camera right / display left, +Y up, +Z towards screen
    OpenVINOGazeVector3 onnx_gaze = OpenVINOGazeVector3(0.1, 0.2, -0.97).normalized();

    GodotCameraVector3 godot_gaze = to_godot_camera(onnx_gaze);
    CHECK(godot_gaze.length() == doctest::Approx(1.0).epsilon(1e-4));
    // Z is inverted (-Z forward in OpenVINO -> +Z towards screen in GodotCamera)
    CHECK(godot_gaze.z > 0.0);
}

TEST_CASE("Coordinate Calculus: Godot Face Local to OpenCV Face Model Round-Trip") {
    auto model_35 = FaceModelGeometry::get_canonical_35pt_model_points();
    auto godot_35 = FaceModelGeometry::get_canonical_godot_model_points();

    for (size_t i = 0; i < 35; ++i) {
        GodotFaceVector3 godot_pt = godot_35[i];
        OpenCVFaceVector3 cv_pt = to_opencv_face(godot_pt);
        CHECK(cv_pt.x == doctest::Approx(model_35[i].x).epsilon(1e-3));
        CHECK(cv_pt.y == doctest::Approx(model_35[i].y).epsilon(1e-3));
        CHECK(cv_pt.z == doctest::Approx(model_35[i].z).epsilon(1e-3));
    }
}

TEST_CASE("Coordinate Calculus: Strongly-Typed Ray-to-Display Pixel Projection") {
    ProjectionEngine proj;
    proj.set_screen_size_pixels(GodotDisplayVector2(1440.0, 900.0));
    proj.set_screen_size_mm(SpacedVector2<Space::GodotDisplayMm>(304.1, 212.4));
    proj.set_camera_placement(CameraPlacement(GodotCameraVector3(0, 106.2, 0), 0.0));

    // Ray origin at eye center (-350mm in front of camera)
    GodotCameraVector3 origin_cam(0.0, -20.0, -350.0);
    // Ray pointing directly forward towards screen center (+Z in Godot Camera space)
    GodotCameraVector3 dir_cam(0.0, 0.0, 1.0);

    GodotDisplayVector2 pixel_out;
    bool proj_ok = proj.project_gaze(origin_cam, dir_cam, pixel_out);
    REQUIRE(proj_ok);

    CHECK(pixel_out.x == doctest::Approx(720.0).epsilon(5.0));
    CHECK(pixel_out.y > 0.0);
    CHECK(pixel_out.y < 900.0);
}
