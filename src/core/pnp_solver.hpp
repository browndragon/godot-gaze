/**
 * @file pnp_solver.hpp
 * @brief Perspective-n-Point Pose Estimation Interface
 *
 * Provides high-level PnP solver dispatch over 3D model points and 2D image coordinates.
 */
#pragma once

#include "sqpnp_solver.hpp"
#include <vector>

namespace Gaze
{

/**
 * @brief Solves the Perspective-n-Point pose using the globally optimal SQPnP algorithm.
 * @param model_points 3D points in model space.
 * @param image_points 2D points in pixel coordinates.
 * @param fx Focal length X.
 * @param fy Focal length Y.
 * @param cx Principal point X.
 * @param cy Principal point Y.
 * @param rvec Output Rodrigues rotation vector in camera space.
 * @param tvec Output Translation vector in camera space.
 * @return true if convergence succeeded.
 */
inline bool solve_pnp_rvec(
    const std::vector<OpenCVFaceVector3> &model_points,
    const std::vector<SpacedVector2<Space::GodotCameraWorkingImagePixels>> &image_points,
    double fx, double fy, double cx, double cy,
    OpenCVCameraVector3 &rvec, OpenCVCameraVector3 &tvec)
{
    return SQPnPSolver::solve_rvec(model_points, image_points, fx, fy, cx, cy, rvec, tvec);
}

inline bool solve_pnp(
    const std::vector<OpenCVFaceVector3> &model_points,
    const std::vector<SpacedVector2<Space::GodotCameraWorkingImagePixels>> &image_points,
    double fx, double fy, double cx, double cy,
    SpacedBasis<Space::OpenCVFaceModel, Space::OpenCVCamera> &rotation, OpenCVCameraVector3 &translation)
{
    return SQPnPSolver::solve(model_points, image_points, fx, fy, cx, cy, rotation, translation);
}

} // namespace Gaze
